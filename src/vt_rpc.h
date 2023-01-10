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
#include "core/ttypes.h"

// with return type, argument type... 
template <typename RT, typename AT>
class RPCVertex : public Vertex {
  public:
    // we stash a function pointer, to call, 
    RT (*funcPtr)(AT argVal) = nullptr;
    // these collect keys for us, 
    // TypeKey<AT> typeKeyAT;
    // TypeKey<RT> typeKeyRT;
    // we for sure need to handle our own paquiats, 
    void destHandler(VPacket* pck, uint16_t ptr) override {
      // pck->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == EP_KEY, ptr + 3 = ID (if ack req.) 
      switch(pck->data[ptr + 2]){
        case RPC_INFO_REQ:
          {
            // write our reply header: it's info-response w/ matching ID
            uint8_t id = pck->data[ptr + 3];
            uint16_t wptr = 0;
            payload[wptr ++] = PK_DEST;
            payload[wptr ++] = RPC_INFO_RES;
            payload[wptr ++] = id;
            // rpcWrap.serializeInfo(payload);
            // let's just write in sizeof our types?
            payload[wptr ++] = getTypeKey<AT>();//typeKeyAT.get();
            ts_writeInt16(sizeof(AT), payload, &wptr);
            payload[wptr ++] = getTypeKey<RT>(); //typeKeyRT.get();
            ts_writeInt16(sizeof(RT), payload, &wptr);
            uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
            stackLoadPacket(pck, datagram, len);
          }
          break;
        case RPC_CALL_REQ:
          {
            // start writing reply header... 
            uint8_t id = pck->data[ptr + 3];
            uint16_t wptr = 0;
            payload[wptr ++] = PK_DEST;
            payload[wptr ++] = RPC_CALL_RES;
            payload[wptr ++] = id;
            // we should actually be able to do this inline:
            // uuh, memcpy-in and memcpy-out, I suppose (?) ? 
            AT arg; // = reinterpret_cast<AT>(((void*)(payload[wptr]))); // ?
            memcpy((void*)(&arg), &(pck->data[ptr + 4]), sizeof(AT));
            OSAP::debug("got arg as " + String(arg));
            // call it, getting a return type...
            RT ret = funcPtr(arg);
            OSAP::debug("got return as " + String(ret));
            // and let's write that back ? 
            memcpy(&(payload[wptr]), (void*)(&ret), sizeof(RT));
            wptr += sizeof(RT);
            // aaand ship it ?
            uint16_t len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
            stackLoadPacket(pck, datagram, len);
          }
          break;
        default:
          OSAP::error("rpc keyless dest ?");
          stackRelease(pck);
      } // end switch 
    };
    // a constructor...
    RPCVertex(
      Vertex* _parent,
      const char* _name,
      RT (*_funcPtr)(AT _argVal)
    ) : Vertex(_parent){
      // appending... 
      strcpy(name, "rpc_");
      strncat(name, _name, VT_NAME_MAX_LEN - 5);
      // type self,
      type = VT_TYPE_RPC;
      // done for now, 
      funcPtr = _funcPtr;
    }
};

// -------------------------------------------------------- ...

// skeleton of a new typeset ?
// we can have ... type-specific constructors, I believe? 
// see... https://www.cprogramming.com/tutorial/template_specialization.html

// class TypeInterface {
//   public:
//     uint8_t key = 0;
//     virtual uint8_t getKey(void);
//     virtual void serialize(char* dest) = 0;
// };

// template <typename T>
// class TypeThing : public TypeInterface {
// };
// template<>
// class TypeThing<float> : public TypeInterface {
//   public:
//     uint8_t key = TK_FLOAT32;
// };

// // -------------------------------------------------------- RPC Interface ?

// class RPCInterface {
//   public: 
//     size_t writeArgKeys(uint8_t* dest);
//     size_t writeReturnKeys(uint8_t* dest);
//     virtual size_t call(uint8_t* argSrc, uint8_t* retDest);
// }

// class RPCInterfaceTyped : public RPCInterface {
  
// }

// template <>
// class Likeness <int16_t> {
//   public: 
//     uint8_t key = TK_INT16;
// };

// // could just template this,
// // then specialize the serialize & deserialize classes 
// class TypeThing_UInt16 {
//   public:
//     uint8_t key = TK_UINT16;
//     uint16_t stash = 0;
//     void serialize(uint8_t* dest, uint16_t maxLen); // these can both just memcpy 
//     T deserialize(uint8_t* src);                    // ibid 
// };

#endif 