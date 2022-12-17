/*
osap/osap.h

osap root / vertex factory 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_H_
#define OSAP_H_

#include "core/vertex.h"

// largely semantic class, OSAP represents the root vertex in whichever context 
// and it's where run the main loop from, etc... 
// here is where we coordinate context-level stuff: adding new instances, 
// stashing error messages & counts, etc, 

enum OSAPErrorLevels { HALTING, MEDIUM, MINOR };
enum OSAPDebugStreams { DBG_DFLT, LOOP };

class OSAP : public Vertex {
  public: 
    void init(void);
    void loop(void) override;
    void destHandler(VPacket* pck, uint16_t ptr);
    // das root 
    OSAP(const char* _name, VPacket* _stack, uint16_t _stackLen);// : Vertex(_name);
    // hangs on 2 the stack of msgs, 
    static VPacket* stack;
    static uint16_t stackLen;
    // does some debuggen 
    static void error(String msg, OSAPErrorLevels lvl = MINOR );
    static void debug(String msg, OSAPDebugStreams stream = DBG_DFLT );
    static uint32_t loopItemsHighWaterMark;
    // I'm uuuh... going to stuff type stuff in here, as a hack, sorry:
    float readFloat(uint8_t* buf);
};

// debug w/ this... 
#ifdef OSAP_HAS_DEBUG_MSGS
#define OSAP_DEBUG(msg) OSAP::debug(String(msg))
#else 
#define OSAP_DEBUG(msg) 
#endif 

// genny error msgs w/ this... 
#ifdef OSAP_HAS_ERROR_MSGS
#define OSAP_ERROR(msg) OSAP::error(String(msg)) 
#else
#define OSAP_ERROR(msg) 
#endif 

// this... yeah: we should i.e. stash the message, suspend operation, 
// and try to make some effort to leave comms open, but this is the current situation: 
#define OSAP_ERROR_HALTING(msg) while(1){};

#endif 