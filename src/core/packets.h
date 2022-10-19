/*
osap/packets.h

reading / writing from osap packets / datagrams 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_PACKETS_H_
#define OSAP_PACKETS_H_

#include <Arduino.h>
#include "vertex.h"

// -------------------------------------------------------- Routing (Packet) Keys

#define PK_PTR 240
#define PK_DEST 224
#define PK_PINGREQ 192 
#define PK_PINGRES 176 
#define PK_SCOPEREQ 160 
#define PK_SCOPERES 144 
#define PK_SIB 16 
#define PK_PARENT 32 
#define PK_CHILD 48 
#define PK_PFWD 64 
#define PK_BFWD 80
#define PK_BBRD 96 
#define PK_LLESCAPE 112 

// to read *just the key* from key, arg pair
#define PK_READKEY(data) (data & 0b11110000)

// packet utes, 
void writeKeyArgPair(unsigned char* buf, uint16_t ptr, uint8_t key, uint16_t arg);
uint16_t readArg(uint8_t* buf, uint16_t ptr);
boolean findPtr(uint8_t* pck, uint16_t* ptr);
boolean walkPtr(uint8_t* pck, Vertex* vt, uint8_t steps, uint16_t ptr = 4);
uint16_t writeDatagram(uint8_t* gram, uint16_t maxGramLength, Route* route, uint8_t* payload, uint16_t payloadLen);
uint16_t writeReply(uint8_t* ogGram, uint8_t* gram, uint16_t maxGramLength, uint8_t* payload, uint16_t payloadLen);

#endif 