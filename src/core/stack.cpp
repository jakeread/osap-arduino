/*
osap/stack.cpp

graph vertex data chonk 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "stack.h"
#include "vertex.h"
#include "osap.h"

// local state !

VPacket* queueStart;
VPacket* firstFree;
uint16_t nowServing = 0;
uint16_t nextTicket = 0;

// -------------------------------------------------------- Init! 

void stackReset(VPacket* stack, uint16_t stackLen){
  // per indice, 
  for(uint16_t p = 0; p < stackLen; p ++){
    stack[p].len = 0;
    stack[p].vt = nullptr;
    stack[p].arrivalTime = 0;
    stack[p].deadline = 0;
    stack[p].indice = p; // not sure if we'll use this ??
  }
  // set next ptrs,
  for(uint16_t p = 0; p < stackLen - 1; p ++){
    stack[p].next = &(stack[p+1]);
  }
  stack[stackLen - 1].next = &(stack[0]);
  // set previous ptrs, 
  for(uint16_t p = 1; p < stackLen; p ++){
    stack[p].previous = &(stack[p-1]);
  }
  stack[0].previous = &(stack[stackLen - 1]);  
  // queueStart element is [0], as is the firstFree, at startup, 
  queueStart = &(stack[0]);
  firstFree = &(stack[0]);
}

// -------------------------------------------------------- Origin / Injest 

// if we have free space, hand it over:
// this is ~ polling, so items re-request each time. 
VPacket* stackRequest(Vertex* vt){
  // null to start, 
  VPacket* res = nullptr;
  // if we have stack avail & vt isn't maxxed on packets, 
  if(firstFree->vt == nullptr && vt->currentPacketHold < vt->maxPacketHold){
    OSAP::debug(String(vt->currentPacketHold) + " : " + String(vt->maxPacketHold));
    // digitalWrite(2, HIGH);
    // this is available, hand it over:
    res = firstFree;
    res->vt = vt;
    vt->currentPacketHold ++;
    // increment first-free before doing so, 
    // if that's full, it will be obvious on next check to firstFree... 
    firstFree = firstFree->next;
    return res;
  } else {
    #warning this does not do anything yet, right? ... need arbitration above, or sth
    vt->ticket = nextTicket;
    nextTicket ++;
    // return that nullptr, 
    return res;
  }
}

// this... kind of, should be depricated, in exchange for direct-loading, non?
void stackLoadPacket(VPacket* packet, uint8_t* data, uint16_t dataLen){
  if(dataLen > VT_VPACKET_MAX_SIZE){
    OSAP_ERROR("attempt to load oversized packet");
  } else {
    memcpy(packet->data, data, dataLen);
    packet->len = dataLen;
    packet->arrivalTime = millis();
  }
}

// -------------------------------------------------------- Exit / Release 

void stackRelease(VPacket* packet){
  // decriment this
  packet->vt->currentPacketHold --;
  // OSAP::debug("release at " + String(packet->vt->name) + " has " + String(packet->vt->currentPacketHold));
  // reset stats... the last two are maybe not necessary to reset, but we're tidy out here 
  packet->len = 0;
  packet->vt = nullptr;
  packet->arrivalTime = 0;
  packet->deadline = 0;
  // it's actual location in the stacko is: 
  uint16_t indice = packet->indice;
  // if was queueStart, queueStart now at next,
  if(queueStart == packet){
    queueStart = packet->next;
    // and we wouldn't have to do any of the below (?)
  } else {
    // pull from chain, now is free of associations, 
    packet->previous->next = packet->next;
    packet->next->previous = packet->previous;
    // now, insert this where old firstFree was 
    firstFree->previous->next = packet;
    packet->previous = firstFree->previous;    
    packet->next = firstFree;
    firstFree->previous = packet;
    // and the item is the new firstFree element, 
    firstFree = packet;
  }
}

// -------------------------------------------------------- Collect 

// well this is a little awkward but we should be able to eliminate it 
// when we are doing the insert-sort. 
uint16_t stackGetPacketsToHandle(VPacket** packets, uint16_t maxPackets){
  // this is the zero-packets case;
  if(firstFree == queueStart) return 0;
  // this is how many max we can possibly list, 
  uint16_t iters = min(maxPackets, OSAP::stackLen);
  // otherwise do... 
  VPacket* pck = queueStart;
  uint16_t count = 0;
  for(uint16_t p = 0; p < iters; p ++){
    // stash it, 
    packets[p] = pck; 
    count ++;
    if(pck->next->vt == nullptr){
      // if next is empty, this is final count:
      return count;
    } else {
      // if it ain't, collect next and continue stuffing 
      pck = pck->next;
    }
  }
  // end-of-loop thru all possible, none free, so:
  return count;
}