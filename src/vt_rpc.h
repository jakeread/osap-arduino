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

// argtype and return type, 
template<typename AT, typename RT>
class RPC : public Vertex {
  public:
    // we for sure need to handle our own paquiats, 
    void destHandler(VPacket* pck, uint16_t ptr) override {
      stackRelease(pck);
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