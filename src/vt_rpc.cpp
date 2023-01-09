/*
osap/vt_rpc.cpp

remote procedure call for osap devices

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "vt_rpc.h"
#include "osap.h"

// -------------------------------------------------------- Constructor

RPC::RPC(
  Vertex* _parent, 
  const char* _name
) : Vertex(_parent) {
  // appending... 
  strcpy(name, "rpc_");
  strncat(name, _name, VT_NAME_MAX_LEN - 5);
  // type self,
  type = VT_TYPE_RPC;
  // done for now, 
}

void RPC::destHandler(VPacket* pck, uint16_t ptr){
  stackRelease(pck);
}