/*
osap/stack.h

graph vertex data chonk

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef STACK_H_
#define STACK_H_

#include <Arduino.h>
#include "../osap_config.h" 

// https://stackoverflow.com/questions/1813991/c-structure-with-pointer-to-self
class Vertex;

// the world runs on messages, or "virtual packets"
typedef struct VPacket {
  // it's a data stash;
  uint8_t data[VT_VPACKET_MAX_SIZE];          // data bytes
  uint16_t len = 0;                   // data bytes count
  // it has an arrival time, and a calculated deadline-for-handling
  uint32_t arrivalTime = 0;           // ms-since-system-alive, time at last ingest
  int32_t deadline = 0;               // ms of time until pckt times out on this hop
  // it belongs to someone, or maybe not,
  Vertex* vt = nullptr;               // vertex to whomst we belong,
  // it lies in a real list in memory,
  uint16_t indice;                     // actual physical position in the stack
  // and a virtual list... for sorting,
  VPacket* next = nullptr;          // linked ringbuffer next
  VPacket* previous = nullptr;      // linked ringbuffer previous
} VPacket;

// we can... reset this, stack of messages,
void stackReset(VPacket* stack, uint16_t stackLen);

// and users can request open items, to write into,
VPacket* stackRequest(Vertex* vt);
void stackLoadPacket(VPacket* packet, uint8_t* data, uint16_t dataLen);
void stackLoadPacket(VPacket* packet, uint16_t dataLen);

// and they can relinquish those:
void stackRelease(VPacket* packet);

// and we can collect a list of 'em to handle, in order.
// in the future, should just be able to walk the ring buffer forever
// though will have to guard against re-stashes
uint16_t stackGetPacketsToHandle(VPacket** packets, uint16_t maxPackets);

#endif
