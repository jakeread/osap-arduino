/*
osap/osap.cpp

osap root / vertex factory

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "osap.h"
#include "core/loop.h"
#include "core/packets.h"
#include "utils/cobs.h"

#include <FlashStorage_SAMD.h>

// stash most recents, and counts, and high water mark,
uint32_t OSAP::loopItemsHighWaterMark = 0;
uint32_t errorCount = 0;
uint32_t debugCount = 0;
// strings...
unsigned char latestError[VT_VPACKET_MAX_SIZE];
unsigned char latestDebug[VT_VPACKET_MAX_SIZE];
uint16_t latestErrorLen = 0;
uint16_t latestDebugLen = 0;

VPacket* OSAP::stack;
uint16_t OSAP::stackLen = 0;

// name-stashing kit,
const int WRITTEN_SIGNATURE = 0xBEEFDEED;
char testName[12] = "testy-testy";
char tempStr[100];

OSAP::OSAP(const char* _name, VPacket* _stack, uint16_t _stackLen) : Vertex(){
  // see vertex.cpp -> vport constructor for notes on this
  strcpy(name, "rt_");
  strncat(name, _name, VT_NAME_MAX_LEN - 4);
  name[VT_NAME_MAX_LEN - 1] = '\0';
  // now we'll stash these,
  stack = _stack;
  stackLen = _stackLen;
  // reset / ready the stacko,
  stackReset(stack, stackLen);
};

// get previous name if it was stashed,
void OSAP::init(void){
  // wake up and check for your name...
  int16_t storedAddress = 0;
  int signature;
  EEPROM.get(storedAddress, signature);
  if(signature == WRITTEN_SIGNATURE){
    // EEPROM.get will pull into this temp thing, we think ?
    EEPROM.get(storedAddress + sizeof(signature), tempStr);
    // and stash in our-name, with a minimum-size
    strncpy(name, tempStr, VT_NAME_MAX_LEN - 1);
    // String newName = String(tempStr); // via cstr -> arduino String, whomst we should rm!
    // name = newName; // we are this now...
  }
}

void OSAP::loop(void){
  // this is the root, so we kick all of the internal net operation from here
  osapLoop(this);
}

void OSAP::destHandler(VPacket* pck, uint16_t ptr){
  // classic switch on 'em
  // pck->data[ptr] == PK_PTR, ptr + 1 == PK_DEST, ptr + 2 == ROOT_KEY, ptr + 3 = ID (if ack req.)
  uint16_t wptr = 0;
  uint16_t len = 0;
  switch(pck->data[ptr + 2]){
    case RT_RENAME_REQ:
        {
        // following an example...
        // int signature = 0;
        // // this is zero ~ because we are not actually writing into the flash address,
        // // it's emulated eeprom
        // int16_t storedAddress = 0;
        // char cBuffer[100];
        // EEPROM.get(storedAddress, signature);
        // // check...
        // if(signature == WRITTEN_SIGNATURE){
        //   OSAP_ERROR("sigCheck is deadbeef !");
        //   EEPROM.get(storedAddress + sizeof(signature), tempStr);
        //   OSAP_ERROR("rtName..." + String(tempStr));
        // } else {
        //   OSAP_ERROR("sigCheck is blank, will try writing");
        //   EEPROM.put(storedAddress, WRITTEN_SIGNATURE);
        //   EEPROM.put(storedAddress + sizeof(signature), testName);
        // }
        // start formulating reply ahead of time,
        payload[wptr ++] = PK_DEST;
        payload[wptr ++] = RT_RENAME_RES;
        payload[wptr ++] = pck->data[ptr + 3];
        // get the string... write is str8 to the name ?
        uint16_t rptr = ptr + 4;
        String incoming = ts_readString(pck->data, &rptr);
        // OSAP_DEBUG("str in is: " + incoming);
        // get that as a cstr, since we have not properly flushed arduino strings from sys...
        strcpy(tempStr, incoming.c_str());
        // from *there* copy it to our name for this runtime,
        strncpy(name, tempStr, VT_NAME_MAX_LEN - 1);
        // OSAP_DEBUG("name is now: " + String(name));
        // and stash it 2 our nvm,
        int16_t storedAddress = 0;
        int signature = 0;
        EEPROM.put(storedAddress, WRITTEN_SIGNATURE);
        EEPROM.put(storedAddress + sizeof(signature), tempStr);
        EEPROM.commit();
        // we need to get these, I guess as a char-array anyways,
        // there needs to be a "changeName" function (?) etc,
        payload[wptr ++] = 1; // can return '0' if not-d21 / also should do per-micro compile,
        #warning should not copile flash stuff if we have a non-samd-supported chip (!)
        len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
        stackLoadPacket(pck, datagram, len);
      }
      break;
    case RT_DBG_STAT:
    case RT_DBG_ERRMSG:
    case RT_DBG_DBGMSG:
      // return w/ the res key & same issuing ID
      payload[wptr ++] = PK_DEST;
      payload[wptr ++] = RT_DBG_RES;
      payload[wptr ++] = pck->data[ptr + 3];
      // stash high water mark, errormsg count, debugmsgcount, and our version # -
      ts_writeUint32(OSAP::loopItemsHighWaterMark, payload, &wptr);
      ts_writeUint32(errorCount, payload, &wptr);
      ts_writeUint32(debugCount, payload, &wptr);
      ts_writeUint32(OSAP_VERSION_NUM, payload, &wptr);
      // then one of these strings - latest message - if it was requested...
      // optionally, a string... I know we switch() then if(), it's uggo,
      if(pck->data[ptr + 2] == RT_DBG_ERRMSG){
        ts_writeString(latestError, latestErrorLen, payload, &wptr, VT_VPACKET_MAX_SIZE / 2);
      } else if (pck->data[ptr + 2] == RT_DBG_DBGMSG){
        ts_writeString(latestDebug, latestDebugLen, payload, &wptr, VT_VPACKET_MAX_SIZE / 2);
      }
      // that's the payload, I figure,
      len = writeReply(pck->data, datagram, VT_VPACKET_MAX_SIZE, payload, wptr);
      // and it's a replacement again,
      stackLoadPacket(pck, datagram, len);
      break;
    default:
      OSAP_ERROR("unrecognized key to root node " + String(pck->data[ptr + 2]));
      stackRelease(pck);
      break;
  }
}

// later we can... make these w/ the OSAP_DEBUG and OSAP_ERROR macros,
// to do i.e. simple logging if we are MINI and more verbosity if we are a BIGBOI

uint8_t errBuf[255];
uint8_t errBufEncoded[255];

void debugPrint(String msg){
  // whatever you want,
  uint32_t len = msg.length();
  // max this long, per the serlink bounds
  if(len + 9 > 255) len = 255 - 9;
  // header...
  errBuf[0] = len + 8;  // len, key, cobs start + end, strlen (4)
  errBuf[1] = 172;      // serialLink debug key
  errBuf[2] = len & 255;
  errBuf[3] = (len >> 8) & 255;
  errBuf[4] = (len >> 16) & 255;
  errBuf[5] = (len >> 24) & 255;
  msg.getBytes(&(errBuf[6]), len + 1);
  // encode from 2, leaving the len, key header...
  size_t ecl = cobsEncode(&(errBuf[2]), len + 4, errBufEncoded);
  // what in god blazes ? copy back from encoded -> previous...
  memcpy(&(errBuf[2]), errBufEncoded, ecl);
  // set tail to zero, to delineate,
  errBuf[errBuf[0] - 1] = 0;
  // direct escape
  Serial.write(errBuf, errBuf[0]);
}

void OSAP::error(String msg, OSAPErrorLevels lvl){
  const char* str = msg.c_str();
  msg.getBytes(latestError, VT_VPACKET_MAX_SIZE);
  latestErrorLen = msg.length();
  debugPrint(msg);
  errorCount ++;
}

void OSAP::debug(String msg, OSAPDebugStreams stream){
  msg.getBytes(latestDebug, VT_VPACKET_MAX_SIZE);
  latestDebugLen = msg.length();
  debugPrint(msg);
  debugCount ++;
}

// there's another one of these in ts.h, sorry again:
union chnk_float32 {
  uint8_t bytes[4];
  float f;
};

float OSAP::readFloat(uint8_t* buf){
  chnk_float32 chunk = { .bytes = { buf[0], buf[1], buf[2], buf[3] } };
  return chunk.f;
}
