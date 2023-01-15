/*
osap/vertex.h

graph vertex

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2020

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VERTEX_H_
#define VERTEX_H_

#include <Arduino.h>
#include "ts.h"
#include "routes.h"
#include "stack.h"
// vertex config is build dependent, define in <folder-containing-osape>/osapConfig.h
#include "../osap_config.h"

// we have the vertex type,
// since it contains ptrs to others of its type, we fwd declare the type...
class Vertex;
// ...
typedef struct VPacket VPacket;
typedef struct VPort VPort;
typedef struct VBus VBus;

// addressable node in the graph !
class Vertex {
  public:
    // just temporary stashes, used all over the place to prep messages, etc,
    // *static* - so one per system, not per vertex
    static uint8_t payload[VT_VPACKET_MAX_SIZE];
    static uint8_t datagram[VT_VPACKET_MAX_SIZE];
    // -------------------------------- Methods
    virtual void loop(void);
    virtual void destHandler(VPacket* pck, uint16_t ptr);
    void pingRequestHandler(VPacket* pck, uint16_t ptr);
    void scopeRequestHandler(VPacket* pck, uint16_t ptr);
    // -------------------------------- Graph Neighbourhood and Addressing
    // a type, a position, a name
    uint8_t type = VT_TYPE_CODE; // (default)
    uint16_t indice = 0;
    char name[VT_NAME_MAX_LEN];  // 32 char max reserved for names,
    // parent & children (other vertices)
    Vertex* parent = nullptr;
    Vertex* children[VT_MAX_CHILDREN]; // I think this is OK on storage: just pointers
    uint16_t numChildren = 0;
    // -------------------------------- Stack Access
    // we have a ticket number for slot-taking,
    uint16_t ticket = 0;
    // and a max-fullness, which should be vairable,
    // i.e. we still want to keep some slots open if one is being greedy...
    // so that rare-participants can be served immediately
    uint16_t currentPacketHold = 0;
    uint16_t maxPacketHold = 1;
    // -------------------------------- Loop-Accessible Extensions
    // sometimes a vertex is a vport, sometimes it is a vbus,
    // we use these ptrs to reach thru to that during the transport loop
    VPort* vport;
    VBus* vbus;
    // -------------------------------- Graph Traversal Helper Data
    // a time tag, for when we were last scoped (need for graph traversals, final implementation tbd)
    uint32_t scopeTimeTag = 0;
    // -------------------------------- CONSTRUCTORS
    // base constructor,
    Vertex(Vertex* _parent, const char* _name);
    // parent only (used by VPort and VBus)
    Vertex(Vertex* _parent) : Vertex(_parent, "unnammed"){};
    // no args (used by root)
    Vertex(void) : Vertex(nullptr, "unnammed"){};
};

// ---------------------------------------------- VPort

class VPort : public Vertex {
  public:
    // -------------------------------- OK these bbs are methods,
    virtual void send(uint8_t* data, uint16_t len) = 0;
    virtual boolean cts(void) = 0;
    virtual boolean isOpen(void) = 0;
    // base constructor,
    VPort(Vertex* _parent, const char* _name);
};

// ---------------------------------------------- VBus

class VBus : public Vertex{
  public:
    // -------------------------------- Methods: these are purely virtual...
    virtual void send(uint8_t* data, uint16_t len, uint8_t rxAddr) = 0;
    virtual void broadcast(uint8_t* data, uint16_t len, uint8_t broadcastChannel) = 0;
    // clear to send, clear to broadcast,
    virtual boolean cts(uint8_t rxAddr) = 0;
    virtual boolean ctb(uint8_t broadcastChannel) = 0;
    // link state per rx-addr,
    virtual boolean isOpen(uint8_t rxAddr) = 0;
    // handle things aimed at us, for mvc etc
    void destHandler(VPacket* pck, uint16_t ptr) override;
    // busses can read-in to broadcasts, which is either successful absorption (returning true) or flow-controlled (ret. false)
    boolean injestBroadcastPacket(uint8_t* data, uint16_t len, uint8_t broadcastChannel);
    // we have also... broadcast channels... these are little route stubs & channel pairs, which we just straight up index,
    Route* broadcastChannels[VBUS_MAX_BROADCAST_CHANNELS];
    // have to update those...
    void setBroadcastChannel(uint8_t channel, Route* route);
    // has an rx addr,
    uint16_t ownRxAddr = 0;
    // has a width-of-addr-space,
    uint16_t addrSpaceSize = 0;
    // base constructor, children inherit...
    VBus(Vertex* _parent, const char* _name);
};

#endif
