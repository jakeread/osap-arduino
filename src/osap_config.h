/*
osap_config.h

config options for an osap-embedded build 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef OSAP_CONFIG_H_
#define OSAP_CONFIG_H_

// size of vertex stacks, lenght, then count,
#define VT_SLOTSIZE 128
#define VT_STACKSIZE 2  // must be >= 2 for ringbuffer operation 
#define VT_MAXCHILDREN 16
#define VT_MAXITEMSPERTURN 8
#define VT_MAXNAMELEN 32

// max # of endpoints that could be spawned here,
#define MAX_CONTEXT_ENDPOINTS 16

// count of routes each endpoint can have, 
#define ENDPOINT_MAX_ROUTES 2
#define ENDPOINT_ROUTE_MAX_LEN 64

// count of broadcast channels width, 
#define VBUS_MAX_BROADCAST_CHANNELS 64 

// squish it ?
#define OSAP_IS_MINI 

#endif 