/*
osap/vertex.cpp

graph vertex 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2019

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vertex.h"
#include "stack.h"
#include "osap.h"
#include "packets.h"

// ---------------------------------------------- Temporary Stash 

uint8_t Vertex::payload[VT_SLOTSIZE];
uint8_t Vertex::datagram[VT_SLOTSIZE];

// ---------------------------------------------- Vertex Constructor and Defaults 

Vertex::Vertex( 
  Vertex* _parent, String _name, 
  void (*_loop)(Vertex* vt),
  void (*_onOriginStackClear)(Vertex* vt, uint8_t slot),
  void (*_onDestinationStackClear)(Vertex* vt, uint8_t slot)
){
  // name self, reset stack... 
  name = _name;
  stackReset(this);
  // callback assignments... 
  loop_cb = _loop;
  onOriginStackClear_cb = _onOriginStackClear;
  onDestinationStackClear_cb = _onDestinationStackClear;
  // insert self to osap net,
  if(_parent == nullptr){
    type = VT_TYPE_ROOT;
    indice = 0;
  } else {
    if (_parent->numChildren >= VT_MAXCHILDREN) {
      OSAP::error("trying to nest a vertex under " + _parent->name + " but we have reached VT_MAXCHILDREN limit", HALTING);
    } else {
      this->indice = _parent->numChildren;
      this->parent = _parent;
      _parent->children[_parent->numChildren ++] = this;
    }
  }
}

void Vertex::loop(void){
  if(loop_cb != nullptr) return loop_cb(this);
}

void Vertex::destHandler(stackItem* item, uint16_t ptr){
  // generic handler...
  OSAP::debug("generic destHandler at " + name);
  stackClearSlot(item);
}

void Vertex::pingRequestHandler(stackItem* item, uint16_t ptr){
  // key & id, 
  payload[0] = PK_PINGRES;
  payload[1] = item->data[ptr + 2];
  // write a new gram, 
  uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, 2);
  // clear previous, 
  stackClearSlot(item);
  // load next... there will be one empty, as this has just arrived here... & we just wiped it 
  stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
}

void Vertex::scopeRequestHandler(stackItem* item, uint16_t ptr){
  // key & id, 
  payload[0] = PK_SCOPERES;
  payload[1] = item->data[ptr + 2];
  // next items write starting here, 
  uint16_t wptr = 2;
  // scope time-tag, 
  ts_writeUint32(scopeTimeTag, payload, &wptr);
  // and read in the previous scope (this is traversal state required to delineate loops in the graph) 
  uint16_t rptr = ptr + 3;
  ts_readUint32(&scopeTimeTag, item->data, &rptr);
  // write the vertex type,  
  payload[wptr ++] = type;
  // vport / vbus link states, 
  if(type == VT_TYPE_VPORT){
    payload[wptr ++] = (vport->isOpen() ? 1 : 0);
  } else if (type == VT_TYPE_VBUS){
    uint16_t addrSize = vbus->addrSpaceSize;
    uint16_t addr = 0;
    // ok we write the address size in first, then our own rxaddr, 
    ts_writeUint16(vbus->addrSpaceSize, payload, &wptr);
    ts_writeUint16(vbus->ownRxAddr, payload, &wptr);
    // then *so long a we're not overwriting*, we stuff link-state bytes, 
    while(wptr + 8 + name.length() <= VT_SLOTSIZE){
      payload[wptr] = 0;
      for(uint8_t b = 0; b < 8; b ++){
        payload[wptr] |= (vbus->isOpen(addr) ? 1 : 0) << b;
        addr ++;
        if(addr >= addrSize) goto end;
      }
      wptr ++;
    }
    end:
    wptr ++; // += 1 more, so we write into next, 
  }
  // our own indice, # siblings, and # children, 
  ts_writeUint16(indice, payload, &wptr);
  if(parent != nullptr){
    ts_writeUint16(parent->numChildren, payload, &wptr);
  } else {
    ts_writeUint16(0, payload, &wptr);
  }
  ts_writeUint16(numChildren, payload, &wptr);
  // finally, our string name:
  ts_writeString(name, payload, &wptr);
  // and roll that back up, rm old, and ship it, 
  uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
  stackClearSlot(item);
  stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
}


void Vertex::onOriginStackClear(uint8_t slot){
  if(onOriginStackClear_cb != nullptr) return onOriginStackClear_cb(this, slot);
}

void Vertex::onDestinationStackClear(uint8_t slot){
  if(onDestinationStackClear_cb != nullptr) return onDestinationStackClear_cb(this, slot);
}

// ---------------------------------------------- VPort Constructor and Defaults 

VPort::VPort(
  Vertex* _parent, String _name
) : Vertex(_parent, "vp_" + _name, nullptr, nullptr, nullptr) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VPORT;
  vport = this; 
}

// ---------------------------------------------- VBus Constructor and Defaults 

VBus::VBus(
  Vertex* _parent, String _name
) : Vertex(_parent, "vb_" + _name, nullptr, nullptr, nullptr) {
  // set type, reacharound, & callbacks 
  type = VT_TYPE_VBUS;
  vbus = this;
  // these should all init to nullptr, 
  for(uint8_t ch = 0; ch < VBUS_MAX_BROADCAST_CHANNELS; ch ++){
    broadcastChannels[ch] = nullptr;
  }
}

void VBus::injestBroadcastPacket(uint8_t* data, uint16_t len, uint8_t broadcastChannel){
  // ok so first we want to see if we have anything sub'd to this channel, so
  if(broadcastChannels[broadcastChannel] != nullptr){
    // we have a route, so we want to load this data *as we inject some new path segments* 
    Route* route = broadcastChannels[broadcastChannel];
    // we could definitely do this faster w/o using the stackLoadSlot fn, but we won't do that yet... 
    // will use the vertex-global datagram stash for that 
    uint16_t ptr = 0; 
    if(!findPtr(data, &ptr)){ OSAP::error("can't find ptr during broadcast injest", MEDIUM); return; }
    // packet should look like 
    // ttl, segsize, <prev_instruct>, <bbrd_txAddr>, PTR, <payload>
    // we want to inject the channel's route such that 
    // ttl, segsize, <prev_instruct>, <bbrd_txAddr>, PTR, <ch_route>, <payload>
    // shouldn't actually be too difficult, eh?
    // we do need to guard on lengths, 
    if(len + route->pathLen > VT_SLOTSIZE){ OSAP::error("datagram + channel route is too large", MEDIUM); return; }
    // copy up to PTR: pck[ptr] == PK_PTR, so we want to *include* this byte, having len ptr + 1, 
    memcpy(datagram, data, ptr + 1);
    // copy in route, but recall that as initialized, route->path[0] == PK_PTR, we don't want to double that up, 
    memcpy(&(datagram[ptr + 1]), &(route->path[1]), route->pathLen - 1);
    // then the rest of the gram, from just after-the-ptr, to end, 
    memcpy(&datagram[ptr + 1 + route->pathLen - 1], &(data[ptr + 1]), len - ptr - 1);
    // now we can load this in, 
    stackLoadSlot(this, VT_STACK_ORIGIN, datagram, len + route->pathLen - 1);
    // aye that's it innit? 
  }
}

void VBus::setBroadcastChannel(uint8_t channel, Route* route){
  if(channel >= VBUS_MAX_BROADCAST_CHANNELS) return;
  // seems a little sus, idk 
  broadcastChannels[channel] = route;
}

void VBus::destHandler(stackItem* item, uint16_t ptr){
  // item->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == the key we're switching on...
  switch(item->data[ptr + 2]){
    case VBUS_BROADCAST_MAP_REQ:
      // mvc request a map of our active broadcast channels, this is akin to bus link-state-scope packet
      {
        uint16_t wptr = 0;
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = VBUS_BROADCAST_MAP_RES;
        payload[wptr ++] = item->data[ptr + 3];
        // max length of channels... max 255, same as max endpoint routes (?) 
        // this is maybe an error, consult packet spec (transport layer) for completeness, 
        // time being... rare to have > 255 broadcast channels, 
        payload[wptr ++] = VBUS_MAX_BROADCAST_CHANNELS;
        // then *so long a we're not overwriting*, we stuff link-state bytes, 
        // idk, 32 is arbitrary, we have to account for return-route length properly... 
        uint16_t channel = 0;
        while(wptr + 32 <= VT_SLOTSIZE){
          payload[wptr] = 0;
          for(uint8_t b = 0; b < 8; b ++){
            payload[wptr] |= (broadcastChannels[channel] == nullptr ? 0 : 1) << b;
            channel ++;
            if(channel >= VBUS_MAX_BROADCAST_CHANNELS) goto end;
          }
          wptr ++;
        }
        end:
        wptr ++; // += 1 more, so we write into next, 
        // we're ready to write the reply back, 
        uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
        stackClearSlot(item);
        stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
        break;
      }
    case VBUS_BROADCAST_QUERY_REQ:
      // mvc requests broadcast channel info on a particular channel, 
      {
        uint16_t wptr = 0;
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = VBUS_BROADCAST_QUERY_RES;
        payload[wptr ++] = item->data[ptr + 3];
        // the indice of the channel we're looking at, 
        uint16_t ch = item->data[ptr + 4];
        // if the ch exists, 
        if(ch < VBUS_MAX_BROADCAST_CHANNELS && broadcastChannels[ch] != nullptr){
          payload[wptr ++] = 1;
          // now... these are route objects, but we only use the path part... 
          // but we'll re-use route-object serialization schemes from EP_ROUTE_QUERY_REQ 
          ts_writeUint16(broadcastChannels[ch]->ttl, payload, &wptr);
          ts_writeUint16(broadcastChannels[ch]->segSize, payload, &wptr);
          // path copy 
          memcpy(&(payload[wptr]), broadcastChannels[ch]->path, broadcastChannels[ch]->pathLen);
          wptr += broadcastChannels[ch]->pathLen;
        } else {
          payload[wptr ++] = 0;
        }
        // write reply, 
        uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
        stackClearSlot(item);
        stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
        break;
      }
    case VBUS_BROADCAST_SET_REQ:
      // mvc requests to set a broadcast channel route 
      {
        // get an ID, 
        uint8_t id = item->data[ptr + 3];
        // ch to write into...
        uint8_t ch = item->data[ptr + 4];
        // reply-write-pointer 
        uint16_t wptr = 0;
        // prep a response, 
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = EP_ROUTE_SET_RES;
        payload[wptr ++] = id;
        if(ch >= VBUS_MAX_BROADCAST_CHANNELS){
          // won't go 
          OSAP::error("attempt to write to oob broadcast channel");
          payload[wptr ++] = 0;
        } else {
          // should go 
          payload[wptr ++] = 1;          
          if(broadcastChannels[ch] != nullptr) OSAP::debug("overwriting previous broadcast ch at " + String(ch));
          uint16_t ttl = ts_readUint16(item->data, ptr + 5);
          uint16_t segSize = ts_readUint16(item->data, ptr + 7);
          uint8_t* path = &(item->data[ptr + 9]);
          uint16_t pathLen = item->len - (ptr + 10);
          setBroadcastChannel(ch, new Route(path, pathLen, ttl, segSize));
        }
        // in any case, write the reply, 
        uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
        stackClearSlot(item);
        stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
        break;
      }
    case VBUS_BROADCAST_RM_REQ:
      // mvc requests to rm a broadcast channel, 
      // todo / cleanliness: might be salient to 'write 0' to delete (?) who knows 
      {
        // id & indice to rm 
        uint8_t id = item->data[ptr + 3];
        uint8_t ch = item->data[ptr + 4];
        uint16_t wptr = 0;
        // prep res 
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = VBUS_BROADCAST_RM_RES;
        payload[wptr ++] = id;
        // can we rm ?
        if(ch < VBUS_MAX_BROADCAST_CHANNELS){
          if(broadcastChannels[ch] != nullptr) {
            delete broadcastChannels[ch];
            broadcastChannels[ch] = nullptr;
            payload[wptr ++] = 1;
          } else {
            // didn't exist, so, a bad delete: 
            payload[wptr ++] = 0;
          }
        } else {
          // bad req, should throw errors... 
          payload[wptr ++] = 0;
        }
        // can send now, 
        uint16_t len = writeReply(item->data, datagram, VT_SLOTSIZE, payload, wptr);
        stackClearSlot(item);
        stackLoadSlot(this, VT_STACK_DESTINATION, datagram, len);
        break;
      }
    default:
      OSAP::error("vbus rx msg w/ unrecognized vbus key " + String(item->data[ptr + 2]) + " bailing", MINOR);
      stackClearSlot(item);
      break;
  } 
}