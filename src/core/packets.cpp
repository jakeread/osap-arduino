/*
osap/packets.cpp

common routines 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "packets.h"
#include "ts.h"
#include "osap.h"

void writeKeyArgPair(unsigned char* buf, uint16_t ptr, uint8_t key, uint16_t arg){
  buf[ptr] = key | (0b00001111 & (arg >> 8));
  buf[ptr + 1] = arg & 0b11111111;
}
// not sure how I want to do this yet... 
uint16_t readArg(uint8_t* buf, uint16_t ptr){
  return ((buf[ptr] & 0b00001111) << 8) | buf[ptr + 1];
}

boolean findPtr(uint8_t* pck, uint16_t* pt){
  // 1st instruction is always at pck[4], pck[0][1] == ttl, pck[2][3] == segSize 
  uint16_t ptr = 4;
  // there's a potential speedup where we assume given *pt is already incremented somewhat, 
  // maybe shaves some ns... but here we just look fresh every time, 
  for(uint8_t i = 0; i < 16; i ++){
    switch(PK_READKEY(pck[ptr])){
      case PK_PTR: // var is here 
        *pt = ptr;
        return true;
      case PK_SIB:
      case PK_PARENT:
      case PK_CHILD:
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD:
        ptr += 2;
        break;
      default:
        return false;
    }
  }
  // case where no ptr after 16 hops, 
  return false;
}

boolean walkPtr(uint8_t* pck, Vertex* source, uint8_t steps, uint16_t ptr){
  // if the ptr we were handed isn't in the right spot, try to find it... 
  if(pck[ptr] != PK_PTR){
    // if that fails, bail... 
    if(!findPtr(pck, &ptr)){
      OSAP::error("before a ptr walk, ptr is out of place...");
      return false;
    }
  }
  // carry on w/ the walking algo, 
  for(uint8_t s = 0; s < steps; s ++){
    switch PK_READKEY(pck[ptr + 1]){
      case PK_SIB:
        {
          // stash indice from-whence it came,
          uint16_t txIndice = source->indice;
          // for loop's next step, this is the source now, 
          source = source->parent->children[readArg(pck, ptr + 1)];
          // where ptr is currently, we stash new key/pair for a reversal, 
          writeKeyArgPair(pck, ptr, PK_SIB, txIndice);
          // increment packet's ptr, and our own... 
          pck[ptr + 2] = PK_PTR; 
          ptr += 2;
        }
        break;
      case PK_PARENT:
        // reversal for a 'parent' instruction is to bounce back down to the child, 
        writeKeyArgPair(pck, ptr, PK_CHILD, source->indice);
        // next source is now...
        source = source->parent;
        // same increment, 
        pck[ptr + 2] = PK_PTR;
        ptr += 2;
        break;
      case PK_CHILD:
        // next source is... 
        source = source->children[readArg(pck, ptr + 1)];
        // reversal for 'child' instruction is to go back up to parent, 
        writeKeyArgPair(pck, ptr, PK_PARENT, 0);
        // same increment, 
        pck[ptr + 2] = PK_PTR;
        ptr += 2; 
        break;
      case PK_PFWD:
        // reversal for pfwd instruction is identical, 
        writeKeyArgPair(pck, ptr, PK_PFWD, 0);
        pck[ptr + 2] = PK_PTR;
        ptr += 2;
        // though this should only ever be called w/ one step, 
        if(steps != 1){
          OSAP::error("likely bad call to walkPtr, we have port fwd w/ more than one step");
          return false;
        }
        break;
      case PK_BFWD:
        // reversal for bfwd instruction is to return *up*... 
        writeKeyArgPair(pck, ptr, PK_BFWD, source->vbus->ownRxAddr);
        pck[ptr + 2] = PK_PTR;
        ptr += 2;
        // this also should only ever be called w/ one step, 
        if(steps != 1){
          OSAP::error("likely bad call to walkPtr, we have bus fwd w/ more than one step");
          return false; 
        }
        break;
      case PK_BBRD:
        // broadcasts are a little strange, we also stuff the ownRxAddr in,
        writeKeyArgPair(pck, ptr, PK_BBRD, source->vbus->ownRxAddr);
        pck[ptr + 2] = PK_PTR;
        ptr += 2;
        break;
      default:
        OSAP::error("have out of place keys in the ptr walk...");
        return false;
    }
  } // end steps, alleged success,  
  return true; 
}

uint16_t writeDatagram(uint8_t* gram, uint16_t maxGramLength, Route* route, uint8_t* payload, uint16_t payloadLen){
  uint16_t wptr = 0;
  ts_writeUint16(route->ttl, gram, &wptr);
  ts_writeUint16(route->segSize, gram, &wptr);
  memcpy(&(gram[wptr]), route->path, route->pathLen);
  wptr += route->pathLen;
  if(wptr + payloadLen > route->segSize){
    OSAP::error("writeDatagram asked to write packet that exceeds segSize, bailing", MEDIUM);
    return 0;
  }
  memcpy(&(gram[wptr]), payload, payloadLen);
  wptr += payloadLen;
  return wptr;
}

// original gram, payload, len, 
uint16_t writeReply(uint8_t* ogGram, uint8_t* gram, uint16_t maxGramLength, uint8_t* payload, uint16_t payloadLen){
  // 1st up, we can straight copy the 1st 4 bytes, 
  memcpy(gram, ogGram, 4);
  // now find a ptr, 
  uint16_t ptr = 0;
  if(!findPtr(ogGram, &ptr)){
    OSAP::error("writeReply can't find the pointer...", MEDIUM);
    return 0;
  }
  // do we have enough space? it's the minimum of the allowed segsize & stated maxGramLength, 
  maxGramLength = min(maxGramLength, ts_readUint16(ogGram, 2));
  if(ptr + 1 + payloadLen > maxGramLength){
    OSAP::error("writeReply asked to write packet that exceeds maxGramLength, bailing", MEDIUM);
    return 0;
  }
  // write the payload in, apres-pointer, 
  memcpy(&(gram[ptr + 1]), payload, payloadLen);
  // now we can do a little reversing... 
  uint16_t wptr = 4;
  uint16_t end = ptr;
  uint16_t rptr = ptr;
  // 1st byte... the ptr, 
  gram[wptr ++] = PK_PTR;
  // now for a max 16 steps, 
  for(uint8_t h = 0; h < 16; h ++){
    if(wptr >= end) break;
    rptr -= 2;
    switch(PK_READKEY(ogGram[rptr])){
      case PK_SIB:
      case PK_PARENT:
      case PK_CHILD:
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD:
        gram[wptr ++] = ogGram[rptr];
        gram[wptr ++] = ogGram[rptr + 1];
        break;
      default:
        OSAP::error("writeReply fails to reverse this packet, bailing", MEDIUM);
        return 0;
    }
  } // end thru-loop, 
  // it's written, return the len  // we had gram[ptr] = PK_PTR, so len was ptr + 1, then added payloadLen, 
  return end + 1 + payloadLen;
}