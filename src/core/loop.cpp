/*
osap/osapLoop.cpp

main osap op: whips data vertex-to-vertex

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "loop.h"
#include "packets.h"
#include "osap.h"

#define TEMP_MAX_PCK_PER_LOOP 16
VPacket* packetsToHandle[TEMP_MAX_PCK_PER_LOOP];

void vertexLoopRecursor(Vertex* vt){
  // run yer loop code, 
  if(vt->type != VT_TYPE_ROOT) vt->loop();
  // run yer childrens' 
  for(uint8_t c = 0; c < vt->numChildren; c ++){
    vertexLoopRecursor(vt->children[c]);
  }
}

void osapLoop(Vertex* root){
  // run their loops, broh, 
  vertexLoopRecursor(root);
  // we're going to try to serve a bunch of packets, to start... maximum 16 per turn, 
  // but this should be revised to use sorted-queues, RAM is valuable ! 
  // we have a list already (!) that we are allegedly maintaining in order 
  // so this bit of implementation should get an update when we start really cramming data thru graphs 
  uint16_t pckListLength = stackGetPacketsToHandle(packetsToHandle, TEMP_MAX_PCK_PER_LOOP);
  // stash high-water mark,
  if(pckListLength > OSAP::loopItemsHighWaterMark) OSAP::loopItemsHighWaterMark = pckListLength;
  // log 'em 
  // if(pckListLength > 0) digitalWrite(2, HIGH);
  // OSAP_DEBUG("list has " + String(itemListLen) + " elements", LOOP);
  // then we can handle 'em one by one, also rm'ing deadies, 
  uint32_t now = millis();
  for(uint16_t i = 0; i < pckListLength; i ++){
    // rm deadies... this whole block is uggo, innit ? 
    packetsToHandle[i]->deadline = ts_readUint16(packetsToHandle[i]->data, 0) - (now - packetsToHandle[i]->arrivalTime);
    if(packetsToHandle[i]->deadline < 0){
      OSAP_DEBUG(  "item at " + String(packetsToHandle[i]->vt->name) + 
                  " times out w/ " + String(packetsToHandle[i]->deadline) + 
                  " ms to live, of " + String(ts_readUint16(packetsToHandle[i]->data, 0)) + " ttl");
      stackRelease(packetsToHandle[i]);
    }
    // run the handler, 
    osapPacketHandler(packetsToHandle[i]);
  }
}

// 344 bytes 
void osapPacketHandler(VPacket* pck){
  // get a ptr for the item, 
  uint16_t ptr = 0;
  if(!findPtr(pck->data, &ptr)){
    OSAP_ERROR("item at " + String(pck->vt->name) + " unable to find ptr, deleting...");
    stackRelease(pck);
    return;
  }
  // now the handle-switch, pck->data[ptr] = PK_PTR, we switch on instruction which is behind that, 
  switch(PK_READKEY(pck->data[ptr + 1])){
    // ------------------------------------------ Terminal / Destination Switches 
    case PK_DEST:
      pck->vt->destHandler(pck, ptr);
      break;
    case PK_PINGREQ:
      pck->vt->pingRequestHandler(pck, ptr);
      break;
    case PK_SCOPEREQ:
      pck->vt->scopeRequestHandler(pck, ptr);
      break;
    case PK_PINGRES:
    case PK_SCOPERES:
      OSAP_ERROR("ping or scope request issued to " + String(pck->vt->name) + " not handling those in embedded");
      stackRelease(pck);
      break;
    // ------------------------------------------ Internal Transport 
    // this handler *returns true* if the pckt is broken & should be wiped, else we hang-10 and wait, 
    case PK_SIB:
    case PK_PARENT:
    case PK_CHILD: 
      if(internalTransport(pck, ptr)) stackRelease(pck);
      break;
    // ------------------------------------------ Network Transport 
    case PK_PFWD:
      // port forward...
      if(pck->vt->vport == nullptr){
        OSAP_ERROR("pfwd to non-vport " + String(pck->vt->name));
        stackRelease(pck);
      } else {
        if(pck->vt->vport->cts()){
          // walk it & transmit, 
          if(walkPtr(pck->data, pck->vt, 1, ptr)){
            pck->vt->vport->send(pck->data, pck->len);
          } else {
            OSAP_ERROR("pfwd fails for bad ptr walk");
          }
          stackRelease(pck);
        } else {
          // failed to send this turn (flow controlled), will return here next round 
        }
      }
      break;
    case PK_BFWD:
    case PK_BBRD:
      // bus forward / bus broadcast: 
      if(pck->vt->vbus == nullptr){
        OSAP_ERROR("bfwd to non-vbus " + String(pck->vt->name));
        stackRelease(pck);
      } else {
        // arg is rxAddr for bus-forwards, is broadcastChannel for bus-broadcast, 
        uint16_t arg = readArg(pck->data, ptr + 1);
        if(pck->data[ptr + 1] == PK_BFWD){
          if(pck->vt->vbus->cts(arg)){
            // walk ptr and tx, 
            if(walkPtr(pck->data, pck->vt, 1, ptr)){
              pck->vt->vbus->send(pck->data, pck->len, arg);
            } else {
              OSAP_ERROR("bfwd fails for bad ptr walk");
            }
            // we sent it, clear it: 
            stackRelease(pck);
          } else {
            // failed to bfwd (flow controlled), returning here next round... 
          }
        } else if (pck->data[ptr + 1] == PK_BBRD){
          if(pck->vt->vbus->ctb(arg)){
            if(walkPtr(pck->data, pck->vt, 1, ptr)){
              // OSAP_DEBUG("broadcasting on ch " + String(arg));
              pck->vt->vbus->broadcast(pck->data, pck->len, arg);
            } else {
              OSAP_ERROR("bbrd fails for bad ptr walk");
            }
            stackRelease(pck);
          } else {
            // failed to bbrd, returning next... 
          }
        }
      }
      break;
    case PK_LLESCAPE:
      OSAP_ERROR("lldebug to embedded, dumping");
      stackRelease(pck);
      break;
    default:
      OSAP_ERROR("unrecognized ptr to " + String(pck->vt->name) + " " + String(PK_READKEY(pck->data[ptr + 1])));
      stackRelease(pck);
      break;
  } // end the-big-switch, 
}

// this handles internal transport... checking for errors along paths, and running flowcontrol 
// returns true to wipe current item, false to leave-in-wait, 
boolean internalTransport(VPacket* pck, uint16_t ptr){
  // we walk thru our little internal tree here, 
  Vertex* vt = pck->vt;
  // ptr for the walk, use pck->data[ptr] == PK_INSTRUCTION, not PK_PTR, 
  uint16_t fwdPtr = ptr + 1;
  // count # of ops, 
  uint8_t opCount = 0;
  // for a max. of 16 fwd steps, 
  for(uint8_t s = 0; s < 16; s ++){
    uint16_t arg = readArg(pck->data, fwdPtr);
    switch(PK_READKEY(pck->data[fwdPtr])){
      // ---------------------------------------- Internal Dir Cases 
      case PK_SIB:
        // check validity of route & shift our reference vt,
        if(vt->parent == nullptr){
          OSAP_ERROR("no parent at " + String(vt->name) + " during sib transport"); return true;
        } else if (arg >= vt->parent->numChildren){
          OSAP_ERROR("no sibling " + String(arg) + " at " + String(vt->name) + " during sib transport"); return true;
        } else {
          // this is it: we go fwds to this vt & end-of-switch statements increment ptrs
          vt = vt->parent->children[arg];
        }
        break;
      case PK_PARENT:
        if(vt->parent == nullptr){
          OSAP_ERROR("no parent at " + String(vt->name) + " during parent transport"); return true;
        } else {
          // likewise... 
          vt = vt->parent;
        }
        break;
      case PK_CHILD:
        if(arg >= vt->numChildren){
          OSAP_ERROR("no child " + String(arg) + " at " + String(vt->name) + " during child transport"); return true;
        } else {
          // again, just walk fwds... 
          vt = vt->children[arg];
        }
        break;
      // ---------------------------------------- Terminal / Exit Cases 
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD: 
      case PK_DEST:
      case PK_PINGREQ:
      case PK_SCOPEREQ:
      case PK_LLESCAPE:
        // check / transport...
        if(vt->currentPacketHold < vt->maxPacketHold){
          // walk the ptr fwds, 
          walkPtr(pck->data, pck->vt, opCount, ptr);
          // current holder now has one less, 
          pck->vt->currentPacketHold --;
          // pass the packet, i.e. pass the vertex to the packet 
          pck->vt = vt;
          // next holder now has one more 
          vt->currentPacketHold ++;
          #warning we would also sort this bad-boy back into place here, non ? 
          pck->arrivalTime = millis();
        }
        // in either case (fwd'd or not) packet is not broken, don't delete: 
        return false;
      default:
        OSAP_ERROR("internal transport failure, ptr walk ends at unknown key");
        return true;
    } // end switch 
    fwdPtr += 2;
    opCount ++;
  } // end max-16-steps, 
  // if we're past all 16 and didn't hit a terminal, pckt is eggregiously long, rm it 
  OSAP_ERROR("internal transport failure, very long walk along the internal transport treadmill");
  return true;
}

