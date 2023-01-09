/*
osap/vt_rpc.h

remote procedure call for osap devices

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VT_RPC_H_
#define VT_RPC_H_

#include "core/vertex.h"
#include "core/packets.h"
#include "osap.h"
#include "core/ts.h"

// argtype and return type, 
template<typename AT, typename RT>
class RPC : public Vertex {
  public:
    // we for sure need to handle our own paquiats, 
    void destHandler(VPacket* pck, uint16_t ptr) override {
      // pck->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == EP_KEY, ptr + 3 = ID (if ack req.) 
      switch(pck->data[ptr + 2]){
        case MVC_INFO_REQ:
          {
            // write our reply header: it's info-response w/ matching ID
            uint8_t id = pck->data[ptr + 3];
            uint16_t wptr = 0;
            payload[wptr ++] = PK_DEST;
            payload[wptr ++] = MVC_INFO_RES;
            payload[wptr ++] = id;
            // let's just write in sizeof our types?
            ts_writeInt16(sizeof(AT), payload, &wptr);
            ts_writeInt16(sizeof(RT), payload, &wptr);
            uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
            stackLoadPacket(pck, datagram, len);
          }
          break;
        default:
          OSAP::error("rpc keyless dest ?");
          stackRelease(pck);
      } // end switch 
    };
    // let's stash one of each, mostly just checking I get templates, 
    AT lastArgs; 
    RT lastReturn;
    // a constructor...
    RPC(
      Vertex* _parent,
      const char* _name,
      AT _argLike,
      RT _retLike
    ) : Vertex(_parent){
      // appending... 
      strcpy(name, "rpc_");
      strncat(name, _name, VT_NAME_MAX_LEN - 5);
      // type self,
      type = VT_TYPE_RPC;
      // done for now, 
    }
};

#endif 