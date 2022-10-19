/*
arduino-ports/vp_arduinoSerial.h

turns arduino serial objects into competent link layers, for OSAP 

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#ifndef ARDU_SERLINK_H_
#define ARDU_SERLINK_H_

#include <Arduino.h>
#include "core/vertex.h"

// buffer is max 256 long for that sweet sweet uint8_t alignment 
#define SERLINK_BUFSIZE 255
// -1 checksum, -1 packet id, -1 packet type, -2 cobs
#define SERLINK_SEGSIZE SERLINK_BUFSIZE - 5
// packet keys; 
#define SERLINK_KEY_PCK 170  // 0b10101010
#define SERLINK_KEY_ACK 171  // 0b10101011
#define SERLINK_KEY_KEEPALIVE 173 
// retry settings 
#define SERLINK_RETRY_MACOUNT 2
#define SERLINK_RETRY_TIME 100000  // microseconds 
#define SERLINK_KEEPALIVE_TX_TIME 800 // milliseconds 
#define SERLINK_KEEPALIVE_RX_TIME 1200 // ms 

#define SERLINK_LIGHT_ON_TIME 100 // in ms 

// note that we use uint8_t write ptrs / etc: and a size of 255, 
// so we are never dealing w/ wraps etc, god bless 

class VPort_ArduinoSerial : public VPort {
  public:
    // arduino std begin 
    void begin(uint32_t baud);
    void begin(void);
    // -------------------------------- our own gd send & cts & loop fns, 
    void loop(void) override;
    void checkOutputStates(void);
    void send(uint8_t* data, uint16_t len) override;
    boolean cts(void) override;
    boolean isOpen(void) override;
    // -------------------------------- Data 
    // Uart & USB are both Stream classes, 
    Stream* stream;
    // we have an overloaded constructor w/ uart or Serial_, the usb class 
    Uart* uart = nullptr;
    Serial_* usbcdc = nullptr; 
    // incoming, always kept clear to receive: 
    uint8_t rxBuffer[SERLINK_BUFSIZE];
    uint8_t rxBufferWp = 0;
    // keepalive state, 
    uint32_t lastRxTime = 0;
    uint32_t lastTxTime = 0;
    uint8_t keepAlivePacket[3] = {3, SERLINK_KEY_KEEPALIVE, 0};
    // guard on double transmits 
    uint8_t lastIdRxd = 0;
    // incoming stash
    uint8_t inAwaiting[SERLINK_BUFSIZE];
    uint8_t inAwaitingId = 0;
    uint8_t inAwaitingLen = 0;
    // outgoing ack, 
    uint8_t ackAwaiting[4];
    boolean ackIsAwaiting = false;
    // outgoing await,
    uint8_t outAwaiting[SERLINK_BUFSIZE];
    uint8_t outAwaitingId = 1;
    uint8_t outAwaitingLen = 0;
    uint8_t outAwaitingNTA = 0;
    unsigned long outAwaitingLTAT = 0;
    // outgoing buffer,
    uint8_t txBuffer[SERLINK_BUFSIZE];
    uint8_t txBufferLen = 0;
    uint8_t txBufferRp = 0;
    // -------------------------------- Constructors 
    VPort_ArduinoSerial(Vertex* _parent, String _name, Uart* _uart);
    VPort_ArduinoSerial(Vertex* _parent, String _name, Serial_* _usbcdc);
};

#endif 