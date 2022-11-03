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
    void loop(void) override;
    void destHandler(stackItem* item, uint16_t ptr);
    OSAP(const char* _name);// : Vertex(_name);
    #ifndef OSAP_IS_MINI
    static void error(String msg, OSAPErrorLevels lvl = MINOR );
    static void debug(String msg, OSAPDebugStreams stream = DBG_DFLT );
    #endif 
    static uint32_t loopItemsHighWaterMark;
    // I'm uuuh... going to stuff type stuff in here, as a hack, sorry:
    float readFloat(uint8_t* buf);
};

#define OSAP_DEBUG(msg) 
#define OSAP_ERROR(msg)
#define OSAP_ERROR_HALTING(msg)

#endif 