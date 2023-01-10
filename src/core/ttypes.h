/*
core/typeTemplates.h

type templates

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2023

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "ts.h"

#ifndef TTYPES_H_
#define TTYPES_H_

// ------------------------------------ Key Generators 

// we can support ~ some subset of types to start, 
// and can throw runtime errors (?) by looking for type-key-zeroes 
// to alert users of bad / unsupported types ? or sth 
template<typename T>
uint8_t getTypeKey(void){
  return 0;
}
template<> inline                     // boolean 
uint8_t getTypeKey<boolean>(void){
  return TK_BOOL;
}
template<> inline                     // uints / ints 
uint8_t getTypeKey<uint8_t>(void){
  return TK_UINT8;
}
template<> inline 
uint8_t getTypeKey<int8_t>(void){
  return TK_INT8;
}
template<> inline 
uint8_t getTypeKey<uint16_t>(void){
  return TK_UINT16;
}
template<> inline 
uint8_t getTypeKey<int16_t>(void){
  return TK_INT16;
}
template<> inline 
uint8_t getTypeKey<uint32_t>(void){
  return TK_UINT32;
}
template<> inline 
uint8_t getTypeKey<int32_t>(void){
  return TK_INT32;
}
template<> inline 
uint8_t getTypeKey<uint64_t>(void){
  return TK_UINT64;
}
template<> inline 
uint8_t getTypeKey<int64_t>(void){
  return TK_INT64;
}
template<> inline                   // floats 
uint8_t getTypeKey<float>(void){
  return TK_FLOAT32;
}
template<> inline 
uint8_t getTypeKey<double>(void){
  return TK_FLOAT64;
}

#endif 