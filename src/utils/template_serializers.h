#ifndef TEMPLATE_SERIALIZERS_H_
#define TEMPLATE_SERIALIZERS_H_

#include <Arduino.h>
#include "../osap.h"

// --------------------------  We declare a unit type, for void-passers

struct Unit{};
constexpr Unit unit{};

// --------------------------  Key Codes 

#define TYPEKEY_VOID 0 
#define TYPEKEY_INT 1
#define TYPEKEY_BOOL 2
#define TYPEKEY_FLOAT 3
#define TYPEKEY_STRING 4 

template<typename T>
uint8_t getTypeKey(void){
  return 0;
}
template<> inline
uint8_t getTypeKey<Unit>(void){
  return TYPEKEY_VOID;
}
template<> inline 
uint8_t getTypeKey<int>(void){
  return TYPEKEY_INT;
}
template<> inline                     // boolean
uint8_t getTypeKey<boolean>(void){
  return TYPEKEY_BOOL;
}
template<> inline                   // floats
uint8_t getTypeKey<float>(void){
  return TYPEKEY_FLOAT;
}
template<> inline 
uint8_t getTypeKey<char*>(void){
  return TYPEKEY_STRING;
}
template<> inline
uint8_t getTypeKey<String>(void){
  return TYPEKEY_STRING;
}


// --------------------------  Unions (serdes utes) 

union chunk_float32 {
  uint8_t bytes[4];
  float f;
};

// --------------------------  Serializing 
// TODO: do they write keys, or not ? we could have serialize_tight() and serialize_safe()
// TODO: serializers should be length-guarded: serialize(var, buffer, wptr, maxsize)
// ... they could simply stop writing in those cases 

template<typename T>
void serialize(T var, uint8_t* buffer, size_t* wptr){}

template<>inline
void serialize<int>(int var, uint8_t* buffer, size_t* wptr){
  buffer[(*wptr) ++] = var & 255;
  buffer[(*wptr) ++] = (var >> 8) & 255;
  buffer[(*wptr) ++] = (var >> 16) & 255;
  buffer[(*wptr) ++] = (var >> 24) & 255;
}

template<>inline 
void serialize<bool>(bool var, uint8_t* buffer, size_t* wptr){
  buffer[(*wptr) ++] = var ? 1 : 0;
}

template<>inline
void serialize<float>(float var, uint8_t* buffer, size_t* wptr){
  chunk_float32 chunk;
  chunk.f = var;
  buffer[(*wptr) ++] = chunk.bytes[0]; 
  buffer[(*wptr) ++] = chunk.bytes[1]; 
  buffer[(*wptr) ++] = chunk.bytes[2]; 
  buffer[(*wptr) ++] = chunk.bytes[3]; 
}

template<>inline 
void serialize<char*>(char* var, uint8_t* buffer, size_t* wptr){
  buffer[(*wptr) ++] = TYPEKEY_STRING;
  size_t len = strlen(var);
  buffer[(*wptr) ++] = len;
  // this should copy the string but not its trailing zero, 
  memcpy(&(buffer[*wptr]), var, len);
  (*wptr) += len;
}

// --------------------------  Deserializing 

template<typename T>
T deserialize(uint8_t* buffer, size_t* rptr){}

template<>inline 
int deserialize<int>(uint8_t* buffer, size_t* rptr){
  int val = 0;
  val |= buffer[(*rptr) ++];
  val |= buffer[(*rptr) ++] << 8;
  val |= buffer[(*rptr) ++] << 16;
  val |= buffer[(*rptr) ++] << 24;
  return val; 
}

template<>inline 
bool deserialize<bool>(uint8_t* buffer, size_t* rptr){
  return buffer[(*rptr) ++];
}

template<>inline 
float deserialize<float>(uint8_t* buffer, size_t* rptr){
  chunk_float32 chunk = {
    .bytes = {
      buffer[(*rptr) ++],
      buffer[(*rptr) ++],
      buffer[(*rptr) ++],
      buffer[(*rptr) ++],
    }
  };
  return chunk.f;
}

// fk it, we can expose our serialized 'string' as an arduino String 
// for convenience / familiarity... nothing in-system uses this, so 
// flash shouldn't be affected until it's invoked ~ 
char string_stash[256] = { 'h', 'e', 'l', 'l', 'o' };
template<>inline 
String deserialize<String>(uint8_t* buffer, size_t* rptr){
  // ok, we can check for our bytecode:
  if(buffer[(*rptr)] != TYPEKEY_STRING){
    return "str_err";
  }
  // otherwise we have this len to unpack:
  size_t len = buffer[(*rptr) + 1];
  // copy those into our stash and stuff a null terminator: 
  memcpy(string_stash, &(buffer[(*rptr) + 2]), len);
  string_stash[len] = '\0';
  // don't forget to increment the rptr accordingly
  *rptr += len + 2;
  // a new String, on the stack, to return: 
  return string_stash;
  // return String(string_stash);
}

#endif 