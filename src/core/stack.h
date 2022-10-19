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
#include "./osap_config.h" 

#define VT_STACK_ORIGIN 0 
#define VT_STACK_DESTINATION 1 

class Vertex;

// core routing layer chunk-of-stuff, 
// https://stackoverflow.com/questions/1813991/c-structure-with-pointer-to-self
typedef struct stackItem {
  uint8_t data[VT_SLOTSIZE];          // data bytes
  uint16_t len = 0;                   // data bytes count 
  uint32_t arrivalTime = 0;           // ms-since-system-alive, time at last ingest
  int32_t timeToDeath = 0;            // ms of time until pckt vanishes on this hop
  Vertex* vt;                         // vertex to whomst we belong, 
  uint8_t od;                         // origin / destination to which we belong, 
  uint8_t indice;                     // actual physical position in the stack 
  uint16_t ptr = 0;                   // current data[ptr] == 88 
  stackItem* next = nullptr;          // linked ringbuffer next 
  stackItem* previous = nullptr;      // linked ringbuffer previous 
} stackItem;

// stack setup / reset 
void stackReset(Vertex* vt);

// stack origin side 
boolean stackEmptySlot(Vertex* vt, uint8_t od);
void stackLoadSlot(Vertex* vt, uint8_t od, uint8_t* data, uint16_t len);

// stack exit side 
uint8_t stackGetItems(Vertex* vt, uint8_t od, stackItem** items, uint8_t maxItems);
void stackClearSlot(Vertex* vt, uint8_t od, stackItem* item);
void stackClearSlot(stackItem* item);


#endif 