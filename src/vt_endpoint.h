/*
osap/vt_endpoint.h

network : software interface

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#ifndef VT_ENDPOINT_H_
#define VT_ENDPOINT_H_

#include "core/vertex.h"
#include "core/packets.h"

// ---------------------------------------------- Endpoint Routes, extends OSAP Core Routes

enum EP_ROUTE_STATES { EP_TX_IDLE, EP_TX_FRESH, EP_TX_AWAITING_ACK, EP_TX_AWAITING_AND_FRESH };

class EndpointRoute {
  public:
    Route* route;
    uint8_t ackId = 0;
    uint8_t ackMode = EP_ROUTEMODE_ACKLESS;
    EP_ROUTE_STATES state = EP_TX_IDLE;
    uint32_t lastTxTime = 0;
    uint32_t timeoutLength;
    // constructor,
    EndpointRoute(Route* _route, uint8_t _mode, uint32_t _timeoutLength = 1000);
    // destructor...
    ~EndpointRoute(void);
};

// ---------------------------------------------- Endpoints

// endpoint handler responses must be one of these enum -
enum EP_ONDATA_RESPONSES { EP_ONDATA_REJECT, EP_ONDATA_ACCEPT, EP_ONDATA_WAIT };

// default handlers,
EP_ONDATA_RESPONSES onDataDefault(uint8_t* data, uint16_t len);
boolean beforeQueryDefault(void);

class Endpoint : public Vertex {
  public:
    // local data store & length,
    // we *should* have users pass us ptrs to these, and...
    // tell us when they are new ? or something ?
    uint8_t data[ENDPOINT_MAX_DATA_SIZE];
    uint16_t dataLen = 0;
    // callbacks: on new data & before a query is written out
    EP_ONDATA_RESPONSES (*onData_cb)(uint8_t* data, uint16_t len) = onDataDefault;
    boolean (*beforeQuery_cb)(void) = beforeQueryDefault;
    // we override vertex loop,
    void loop(void) override;
    void destHandler(VPacket* pck, uint16_t ptr) override;
    // methods,
    void write(uint8_t* _data, uint16_t len);
    // ... for boolean types, do:
    void write(boolean _data);
    boolean clearToWrite(void);
    uint8_t addRoute(Route* _route, uint8_t _mode = EP_ROUTEMODE_ACKLESS, uint32_t _timeoutLength = 1000);
    // routes, for tx-ing to:
    EndpointRoute* routes[ENDPOINT_MAX_ROUTES];
    uint16_t numRoutes = 0;
    uint16_t lastRouteServiced = 0;
    uint8_t nextAckID = 77;
    // base constructor,
    Endpoint(
      Vertex* _parent,
      const char* _name,
      EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len),
      boolean (*_beforeQuery)(void)
    );
    // these are called "delegating constructors" ... best reference is
    // here: https://en.cppreference.com/w/cpp/language/constructor
    // onData only,
    Endpoint(
      Vertex* _parent,
      const char* _name,
      EP_ONDATA_RESPONSES (*_onData)(uint8_t* data, uint16_t len)
    ) : Endpoint (
      _parent, _name, _onData, nullptr
    ){};
    // beforeQuery only,
    Endpoint(
      Vertex* _parent,
      const char* _name,
      boolean (*_beforeQuery)(void)
    ) : Endpoint (
      _parent, _name, nullptr, _beforeQuery
    ){};
    // name only,
    Endpoint(
      Vertex* _parent,
      const char* _name
    ) : Endpoint (
      _parent, _name, nullptr, nullptr
    ){};
};

#endif
