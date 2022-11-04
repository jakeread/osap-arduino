/*
osape/vertices/endpoint.cpp

network : software interface

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vt_endpoint.h"
#include "osap.h"

// -------------------------------------------------------- Constructors 

// route constructor 
EndpointRoute::EndpointRoute(Route* _route, uint8_t _mode, uint32_t _timeoutLength){
  if(_mode != EP_ROUTEMODE_ACKED && _mode != EP_ROUTEMODE_ACKLESS){
    _mode = EP_ROUTEMODE_ACKLESS;
  }
  route = _route;
  ackMode = _mode;
  timeoutLength = _timeoutLength;
}

EndpointRoute::~EndpointRoute(void){
  delete route;
}

// base constructor, 
Endpoint::Endpoint(
  Vertex* _parent, const char* _name, 
  EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len),
  boolean (*_beforeQuery)(void)
) : Vertex(_parent) {
  // see vertex.cpp -> vport constructor for notes on this 
  strcpy(name, "ep_");
  strncat(name, _name, VT_NAME_MAX_LEN - 4);
  name[VT_NAME_MAX_LEN - 1] = '\0';
  // type, 
	type = VT_TYPE_ENDPOINT;
  // set callbacks,
  if(_onData) onData_cb = _onData;
  if(_beforeQuery) beforeQuery_cb = _beforeQuery;
}

// -------------------------------------------------------- Dummies / Defaults 

EP_ONDATA_RESPONSES onDataDefault(uint8_t* data, uint16_t len){
  return EP_ONDATA_ACCEPT;
}

boolean beforeQueryDefault(void){
  return true;
}

// -------------------------------------------------------- Endpoint Route / Write API 

void Endpoint::write(uint8_t* _data, uint16_t len){
  // copy data in,
  if(len > ENDPOINT_MAX_DATA_SIZE){
    OSAP_ERROR("attempt to write too-large data to endpoint");
    return; // no lol 
  }
  memcpy(data, _data, len);
  dataLen = len;
  // set route freshness 
  for(uint8_t r = 0; r < numRoutes; r ++){
    #ifdef OSAP_IS_MINI
    routes[r]->state = EP_TX_FRESH;
    #else 
    if(routes[r]->state == EP_TX_AWAITING_ACK){
      routes[r]->state = EP_TX_AWAITING_AND_FRESH;
    } else {
      routes[r]->state = EP_TX_FRESH;
    }
    #endif 
  }
}

// for floats, booleans... if we do quick-
uint8_t _quickStash[4];

void Endpoint::write(boolean _data){
  _quickStash[0] = _data;
  write(_quickStash, 1);
}

// add a route to an endpoint, returns indice where it's dropped, 
uint8_t Endpoint::addRoute(Route* _route, uint8_t _mode, uint32_t _timeoutLength){
	// guard against more-than-allowed routes 
	if(numRoutes >= ENDPOINT_MAX_ROUTES) {
    OSAP_ERROR("route add is oob"); 
    return 0;
	}
  // no acked msgs for tiny friends 
  #ifdef OSAP_IS_MINI
  _mode = EP_ROUTEMODE_ACKLESS;
  #endif 
  // build, stash, increment 
  uint8_t indice = numRoutes;
  routes[numRoutes ++] = new EndpointRoute(_route, _mode, _timeoutLength);
  return indice; 
}

boolean Endpoint::clearToWrite(void){
  for(uint8_t r = 0; r < numRoutes; r ++){
    if(routes[r]->state != EP_TX_IDLE){
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------- Loop 

void Endpoint::loop(void){
  // ok we are doing a time-based dispatch... 
  unsigned long now = millis();
  EndpointRoute* routeTxList[ENDPOINT_MAX_ROUTES];
  uint8_t routeTxListLen = 0;
  // stack fresh routes, and also transition timeouts / etc, 
  // we make & sort this list, but set it up round-robin, since many 
  // cases will see the same TTL & same write-to time, meaning routes that 
  // happen to be in low indices would chance on "higher priority" 
  uint8_t r = lastRouteServiced;
  for(uint8_t i = 0; i < numRoutes; i ++){
    r ++; if(r >= numRoutes) r = 0;
    switch(routes[r]->state){
      case EP_TX_FRESH:
        routeTxList[routeTxListLen ++] = routes[r];
        break;
      // don't compile ack-related kit if we are trying to be tiny 
      #ifndef OSAP_IS_MINI
      case EP_TX_AWAITING_ACK:
				// check timeout & transition to idle state on ack-timeout, 
        if(routes[r]->lastTxTime + routes[r]->timeoutLength > now){
          routes[r]->state = EP_TX_IDLE;
        }
				break;
      case EP_TX_AWAITING_AND_FRESH:
        // check timeout & transition to fresh state 
        if(routes[r]->lastTxTime + routes[r]->timeoutLength > now){
          routes[r]->state = EP_TX_FRESH;
        }
      #endif 
      default:
        // noop for IDLE / otherwise...
        break;
    }
  }
  // now, would do a sort... they're all fresh at the same time, so lowest TTL would win,
  // this one we would want to be stable, meaning original order is preserved in 
  // otherwise identical cases, since we round-robin fairness as well as TTL / TTD  
  #warning no sort algo yet, for endpoints-outgoing, but should be earliest-deadline 
  // serve 'em... these are all EP_TX_FRESH state, 
  for(r = 0; r < routeTxListLen; r ++){
    // can we pull a memory chunk to occupy ?
    VPacket* pck = stackRequest(this);
    if(pck != nullptr){
      // make sure we'll have enough space...
      if(dataLen + routeTxList[r]->route->pathLen + 3 >= VT_VPACKET_MAX_SIZE){
        OSAP_ERROR("attempting to write oversized datagram at " + name);
        routeTxList[r]->state = EP_TX_IDLE;
        continue;
      }
      // write dest key, mode key, & id if acked, 
      uint16_t wptr = 0;
      payload[wptr ++] = PK_DEST;
      if(routeTxList[r]->ackMode == EP_ROUTEMODE_ACKLESS){
        payload[wptr ++] = EP_SS_ACKLESS;
      } else {
        #ifndef OSAP_IS_MINI
        payload[wptr ++] = EP_SS_ACKED;
        payload[wptr ++] = nextAckID;
        routeTxList[r]->ackId = nextAckID;
        nextAckID ++;
        #endif 
      } 
      // write data into the payload, 
      memcpy(&(payload[wptr]), data, dataLen);
      wptr += dataLen;
      // write the packet, 
      uint16_t len = writeDatagram(datagram, VT_VPACKET_MAX_SIZE, routeTxList[r]->route, payload, wptr);
      // tx time is now, and state is awaiting ack, 
      routeTxList[r]->lastTxTime = now;
      routeTxList[r]->state = EP_TX_AWAITING_ACK;
      lastRouteServiced = r;
      // ingest it...
      stackLoadPacket(pck, datagram, len);
    } else {
      // stack has no more empty slots, bail from the loop, 
      break;
    }
  } // end fresh-tx-awaiting state checks, 
}

// -------------------------------------------------------- Destination Handler  

// this is a big flash memory offender, at 780 bytes 
void Endpoint::destHandler(VPacket* pck, uint16_t ptr){
  // pck->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == EP_KEY, ptr + 3 = ID (if ack req.) 
  switch(pck->data[ptr + 2]){
    case EP_SS_ACKLESS:
      { // singlesegment transmit-to-us, w/o ack, 
        uint8_t* rxData = &(pck->data[ptr + 3]); uint16_t rxLen = pck->len - (ptr + 4);
        EP_ONDATA_RESPONSES resp = onData_cb(rxData, rxLen);
        switch(resp){
          case EP_ONDATA_WAIT:    // in a wait case, we no-op / escape, it comes back around 
            pck->arrivalTime = millis();
            break;
          case EP_ONDATA_ACCEPT:  // here we copy it in, but carry on to the reject term to delete og gram
            memcpy(data, rxData, rxLen);
            dataLen = rxLen;
          case EP_ONDATA_REJECT:  // this clears for *this* and the above
            stackRelease(pck);
            break;
        } // end resp-handler, 
      }
      break;
    case EP_SS_ACKED:
      { // singlesegment transmit-to-us, w/ ack, 
        uint8_t id = pck->data[ptr + 3];
        uint8_t* rxData = &(pck->data[ptr + 4]); uint16_t rxLen = pck->len - (ptr + 5);
        EP_ONDATA_RESPONSES resp = onData_cb(rxData, rxLen);
          switch(resp){
            case EP_ONDATA_WAIT: // this is a little danger-danger, 
              pck->arrivalTime = millis();
              break;
            case EP_ONDATA_ACCEPT:
              memcpy(data, rxData, rxLen);
              dataLen = rxLen;
            case EP_ONDATA_REJECT:
              // write the ack, ship it, 
              payload[0] = PK_DEST;
              payload[1] = EP_SS_ACK;
              payload[2] = id;
              uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, 3);
              // ack reloads-in-place, 
              stackLoadPacket(pck, datagram, len);
              break;
          }
      }
      break;
    case EP_QUERY:
      {
        // beforeQuery, 
        beforeQuery_cb();
        // request for our data, 
        payload[0] = PK_DEST;
        payload[1] = EP_QUERY_RESP;
        payload[2] = pck->data[ptr + 3];
        memcpy(&(payload[3]), data, dataLen);
        uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, dataLen + 3);
        // query reloads-in-place
        stackLoadPacket(pck, datagram, len);
      }
      break;
    #ifndef OSAP_IS_MINI
    // for example, just the below statement is 68 bytes... seems wild 
    case EP_SS_ACK:
      // acks to us, 
      for(uint8_t r = 0; r < numRoutes; r ++){
        if(pck->data[ptr + 3] == routes[r]->ackId){
          switch(routes[r]->state){
            case EP_TX_AWAITING_ACK:
              routes[r]->state = EP_TX_IDLE;
              goto ackEnd;
            case EP_TX_AWAITING_AND_FRESH:
              routes[r]->state = EP_TX_FRESH;
              goto ackEnd;
            case EP_TX_FRESH:
            case EP_TX_IDLE:
            default:
              // these are nonsense states, likely double-transmits, likely safely ignored,
              goto ackEnd;
          } // end switch 
        }
      } // end for-each route, if we've reached this point, still dump it;
      ackEnd:
      stackRelease(pck);
      break;
    #endif 
    case EP_ROUTE_QUERY_REQ:
      // MVC request for a route of ours, 
      {
        uint8_t id = pck->data[ptr + 3];
        uint16_t r = ts_readUint16(pck->data, ptr + 4);
        uint16_t wptr = 0;
        // dest, key, id... mode, 
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = EP_ROUTE_QUERY_RES;
        payload[wptr ++] = id;
        if(r < numRoutes){
          payload[wptr ++] = routes[r]->ackMode;
          // ttl, segsize, 
          ts_writeUint16(routes[r]->route->ttl, payload, &wptr);
          ts_writeUint16(routes[r]->route->segSize, payload, &wptr);
          // path ! 
          memcpy(&(payload[wptr]), routes[r]->route->path, routes[r]->route->pathLen);
          wptr += routes[r]->route->pathLen;
        } else {
          payload[wptr ++] = 0; // no-route-here, 
        }
        // clear request, write reply in place, 
        uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
        stackLoadPacket(pck, datagram, len);
      }
      break;
    case EP_ROUTE_SET_REQ:
      // MVC request to set a new route, 
      {
        // get an ID, 
        uint8_t id = pck->data[ptr + 3];
        // prep a response, 
        payload[0] = PK_DEST;
        payload[1] = EP_ROUTE_SET_RES;
        payload[2] = id;
        if(numRoutes + 1 <= ENDPOINT_MAX_ROUTES){
          // tell call-er it should work, 
          payload[3] = 1;
          // gather & set route, 
          uint8_t mode = pck->data[ptr + 4];
          uint16_t ttl = ts_readUint16(pck->data, ptr + 5);
          uint16_t segSize = ts_readUint16(pck->data, ptr + 7);
          uint8_t* path = &(pck->data[ptr + 9]);
          uint16_t pathLen = pck->len - (ptr + 10);
          OSAP_DEBUG("adding path... w/ ttl " + String(ttl) + " ss " + String(segSize) + " pathLen " + String(pathLen));
          uint8_t routeIndice = addRoute(new Route(path, pathLen, ttl, segSize), mode);
          payload[4] = routeIndice;
        } else {
          // nope, 
          payload[3] = 0;
          payload[4] = 0;
        }
        // either case, write the reply, 
        uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, 5);
        stackLoadPacket(pck, datagram, len);
      }
      break;
    case EP_ROUTE_RM_REQ:
      // MVC request to rm a route... 
      {
        // msg id, & indice to remove, 
        uint8_t id = pck->data[ptr + 3];
        uint8_t r = pck->data[ptr + 4];
        // prep a response, 
        payload[0] = PK_DEST;
        payload[1] = EP_ROUTE_RM_RES;
        payload[2] = id;
        if(r < numRoutes){
          // RM ok, 
          payload[3] = 1;
          // delete / run destructor 
          delete routes[r];
          // shift...
          for(uint8_t i = r; i < numRoutes - 1; i ++){
            routes[i] = routes[i + 1];
          }
          // last is null, 
          routes[numRoutes] = nullptr;
          numRoutes --;
        } else {
          // rm not-ok
          payload[3] = 0;
        }
        // either case, write reply 
        uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, 4);
        stackLoadPacket(pck, datagram, len);
      }
      break;
    default:
      OSAP_ERROR("endpoint rx msg w/ unrecognized endpoint key " + String(pck->data[ptr + 2]) + " bailing");
      stackRelease(pck);
      break;
  } // end switch... 
}
