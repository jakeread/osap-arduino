
#include "serializers.h"

// ptr-is-ptr, ... 
void serializers_writeUint16(uint8_t* buf, uint16_t* wptr, uint16_t val){
  buf[(*wptr) ++] = val & 255;
  buf[(*wptr) ++] = (val >> 8) & 255;
}

// w/ direct offset 
void serializers_writeUint16(uint8_t* buf, uint16_t offset, uint16_t val){
  buf[offset] = val & 255;
  buf[offset + 1] = (val >> 8) & 255;
}

// reading
uint16_t serializers_readUint16(uint8_t* buf, uint16_t offset){
  return (buf[offset + 1] << 8) | buf[offset];
}

// ptr is ptr...
void serializers_writeString(uint8_t* buf, uint16_t* wptr, char* val){
  // add one to the len so that we include the trailing zero and 
  // can copy the str out directly:
  size_t len = strlen(val) + 1;
  // debug / test w/ this...
  // use memcpy instead of strcpy to avoid uint8_t vs. char pain:
  memcpy(&(buf[*wptr]), val, len);
  // strcpy(&(buf[*wptr]), val);
  // resolve the length 
  *wptr += len;
}

void serializers_readString(uint8_t* buf, uint16_t offset, char* dest, size_t maxLen){
  // erp, let's find the location of the trailing zero:
  uint16_t zero = offset;
  boolean found = false;
  for(uint16_t z = offset; z < offset + maxLen; z ++){
    if(buf[z] == '\0'){
      zero = z;
      found = true;
      break;
    }
  }
  // if it wasn't there, suppose we stick it in at maxlen:
  if(!found) zero = offset + maxLen;
  // ok ok, we can use memcpy then, 
  memcpy(dest, &(buf[offset]), (zero - offset));
  // re-write the trailing zero, just-in-case, 
  buf[offset + zero] = '\0';
}