/*
osap/routes.cpp

directions

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "routes.h"
#include "packets.h"

Route::Route(uint8_t* _path, uint16_t _pathLen, uint16_t _ttl, uint16_t _segSize){
  ttl = _ttl;
  segSize = _segSize;
  // nope, 
  if(_pathLen > 64){
    _pathLen = 0;
  }
  memcpy(path, _path, _pathLen);
  pathLen = _pathLen;
}

Route::Route(void){
  path[pathLen ++] = PK_PTR;
}

Route* Route::sib(uint16_t indice){
  writeKeyArgPair(path, pathLen, PK_SIB, indice);
  pathLen += 2;
  return this;
}

Route* Route::pfwd(void){
  writeKeyArgPair(path, pathLen, PK_PFWD, 0);
  pathLen += 2;
  return this;
}

Route* Route::bfwd(uint16_t rxAddr){
  writeKeyArgPair(path, pathLen, PK_BFWD, rxAddr);
  pathLen += 2;
  return this;
}

Route* Route::bbrd(uint16_t channel){
  writeKeyArgPair(path, pathLen, PK_BBRD, channel);
  pathLen += 2;
  return this; 
}