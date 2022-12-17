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
#define VT_VPACKET_MAX_SIZE 128
#define VT_NAME_MAX_LEN 31
#define VT_MAX_CHILDREN 16

// count of routes each endpoint can have, 
// these, equally, should be allocated in-total, and assigned 
// to individual endpoints, right ? 
#define ENDPOINT_MAX_DATA_SIZE 32 
#define ENDPOINT_MAX_ROUTES 2
#define ENDPOINT_ROUTE_MAX_LEN 48

// count of broadcast channels width, 
#define VBUS_MAX_BROADCAST_CHANNELS 64 

// micro-sized ? should just rm this entirely, one-size osap... 
#define OSAP_IS_MINI

// send debug messages?
#define OSAP_HAS_DEBUG_MSGS

// send error messages?
#define OSAP_HAS_ERROR_MSGS

#endif 