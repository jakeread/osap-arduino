/*
osap/routes.h

directions

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_ROUTES_H_
#define OSAP_ROUTES_H_

#include <Arduino.h>

// a route type... 
class Route {
  public:
    uint8_t path[64];
    uint16_t pathLen = 0;
    uint16_t ttl = 1000;
    uint16_t segSize = 128;
    // write-direct constructor, 
    Route(uint8_t* _path, uint16_t _pathLen, uint16_t _ttl, uint16_t _segSize);
    // write-along constructor, 
    Route(void);
    // pass-thru initialize constructors, 
    Route* sib(uint16_t indice);
    Route* pfwd(void);
    Route* bfwd(uint16_t rxAddr);
    Route* bbrd(uint16_t channel);
};

#endif 