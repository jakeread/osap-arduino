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

template<typename T>
class TypeKey {
  public: 
    uint8_t get(void){
      return 0;
    }
};
template<>
class TypeKey <float> {
  public: 
    uint8_t get(void){
      return TK_FLOAT32;
    }
};
template<>
class TypeKey <int16_t> {
  public: 
    uint8_t get(void){
      return TK_INT16;
    }
};

#endif 