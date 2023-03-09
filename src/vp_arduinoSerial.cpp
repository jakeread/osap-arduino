/*
arduino-ports/ardu-vport.h

turns serial objects into competent link layers

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2022

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the squidworks and ponyo
projects. Copyright is retained and must be preserved. The work is provided as
is; no warranty is provided, and users accept all liability.
*/

#include "vp_arduinoSerial.h"
#include "utils/cobs.h"
#include "osap.h"

VPort_ArduinoSerial::VPort_ArduinoSerial( Vertex* _parent, const char* _name, Uart* _uart
) : VPort ( _parent, _name ){
  stream = _uart; // should convert Uart* to Stream*, as Uart inherits stream
  uart = _uart;
  // additionally, give ourselves more stack allocation:
  maxPacketHold = 4;
}

VPort_ArduinoSerial::VPort_ArduinoSerial( Vertex* _parent, const char* _name, Serial_* _usbcdc
) : VPort ( _parent, _name ){
  stream = _usbcdc;
  usbcdc = _usbcdc;
  // additionally, give ourselves more stack allocation:
  maxPacketHold = 4;
}

void VPort_ArduinoSerial::begin(uint32_t baudRate){
  if(uart != nullptr){
    uart->begin(baudRate);
  } else if (usbcdc != nullptr){
    usbcdc->begin(baudRate);
  }
}

void VPort_ArduinoSerial::begin(void){
  if(uart != nullptr){
    uart->begin(1000000);
  } else if (usbcdc != nullptr){
    usbcdc->begin(9600);  // baud ignored on cdc begin
  }
}

// link packets are max 256 bytes in length, including the 0 delimiter
// structured like:
// checksum | pck/ack key | pck id | cobs encoded data | 0

void VPort_ArduinoSerial::loop(void){
  // byte injestion: think of this like the rx interrupt stage,
  while(stream->available() && rxBufferLen == 0){
    // read byte into the current stub,
    rxBuffer[rxBufferWp ++] = stream->read();
    if(rxBuffer[rxBufferWp - 1] == 0){
      // always reset keepalive last-rx time,
      lastRxTime = millis();
      // 1st, we checksum:
      if(rxBuffer[0] != rxBufferWp){
        OSAP_ERROR("serLink bad checksum, cs: " + String(rxBuffer[0]) + " wp: " + String(rxBufferWp));
      } else {
        // acks, packs, or broken things
        switch(rxBuffer[1]){
          case SERLINK_KEY_PCK:
            // dirty guard for retransmitted packets,
            if(rxBuffer[2] != lastIdRxd){
              rxBufferId = rxBuffer[2]; // stash ID
              rxBufferLen = rxBufferWp;
            } else {
              OSAP_ERROR("serLink double rx");
            }
            break;
          case SERLINK_KEY_ACK:
            if(rxBuffer[2] == outAwaitingId){
              outAwaitingLen = 0;
            }
            break;
          case SERLINK_KEY_KEEPALIVE:
            // noop,
            break;
          default:
            // makes no sense,
            break;
        }
      }
      // always reset on delimiter,
      rxBufferWp = 0;
    }
  } // end while-receive

  // now check if we can get an osap packet to write into...
  // would be nice to cobs-in-place, non? https://github.com/charlesnicholson/nanocobs
  // performance improvement also might mean... different algos for USB-serial and for Serial-Serial,
  // though sharing them would mean compatibility across i.e. usb-to-uart devices, IDK man
  if(rxBufferLen){//} && !ackIsAwaiting){
    // OSAP_DEBUG("VP: rx'd");
    VPacket* pck = stackRequest(this);
    if(pck != nullptr){
      // OSAP_DEBUG("VP: stack acquired");
      // we can decode COBS straight in... i.e. if we have smaller vertex sizes
      // than the default serial-packet length (255, bc uint8), we could be doing some
      // bigly unsafe-unpacks here (!)
      #warning no length guard here !!
      pck->len = cobsDecode(&(rxBuffer[3]), rxBufferLen - 2, pck->data); // fill that packet up,
      pck->arrivalTime = millis();
      // and can set the ack,
      ackIsAwaiting = true;
      ackAwaiting[0] = 4;                 // checksum still, innit
      ackAwaiting[1] = SERLINK_KEY_ACK;   // it's an ack bruv
      ackAwaiting[2] = rxBufferId;      // which pck r we akkin m8
      ackAwaiting[3] = 0;                 // delimiter
      // we've cleared it now, can resume-rx'ing
      rxBufferLen = 0;
    } // if-no-packet for us, we are awaiting,
  }

  // check & execute actual tx
  checkOutputStates();
}

void VPort_ArduinoSerial::send(uint8_t* data, uint16_t len){
  // cts() == true means that our outAwaiting has been tx'd, is drained, etc
  if(!cts()) return;
  // setup,
  // OSAP_DEBUG("sendy");
  outAwaiting[0] = len + 5;               // pck[0] is checksum = len + checksum + cobs start + cobs delimit + ack/pack + id
  outAwaiting[1] = SERLINK_KEY_PCK;       // this ones a packet m8
  outAwaitingId ++; if(outAwaitingId == 0) outAwaitingId = 1;
  outAwaiting[2] = outAwaitingId;         // an id
  cobsEncode(data, len, &(outAwaiting[3]));  // encode
  outAwaiting[len + 4] = 0;               // stuff delimiter,
  outAwaitingLen = outAwaiting[0];        // track...
  // transmit attempts etc
  outAwaitingTransmitted = false;
  // try it
  checkOutputStates();                    // try / start write
}

// we are CTS if outPck is not occupied,
boolean VPort_ArduinoSerial::cts(void){
  // OSAP_DEBUG("VP cts: " + String(outAwaitingLen));
  return (outAwaitingLen == 0);
}

// we are open if we've heard back lately,
boolean VPort_ArduinoSerial::isOpen(void){
  return (millis() - lastRxTime < SERLINK_KEEPALIVE_RX_TIME && lastRxTime != 0);
}

// I figure this... should get a little smarter, also, at the protocol level? maybe we'll learn something useful w/ CRC on the bus...
void VPort_ArduinoSerial::checkOutputStates(void){
  // if we're not tx'ing anything, check if we could be:
  if(txState == SERLINK_TX_NONE){
    // in priority,
    if(ackIsAwaiting){
      // acks get out first,
      txState = SERLINK_TX_ACK;
      lastTxTime = millis();
    } else if (outAwaitingLen && !outAwaitingTransmitted){
      // setup to retransmit
      txState = SERLINK_TX_PCK;
      lastTxTime = millis();
      outAwaitingTransmitted = true;
      outTxRp = 0;
    } else if (millis() - lastTxTime > SERLINK_KEEPALIVE_TX_TIME){
      // then keepalives,
      txState = SERLINK_TX_KPA;
      lastTxTime = millis();
    }
  }
  // then operate on this?
  switch(txState){
    case SERLINK_TX_NONE:
      break;
    case SERLINK_TX_ACK:
      while(stream->availableForWrite()){
        stream->write(ackAwaiting[ackTxRp ++]);
        if(ackTxRp >= 4){
          ackTxRp = 0;
          ackIsAwaiting = false;
          txState = SERLINK_TX_NONE;
          return;
        }
      }
      break;
    case SERLINK_TX_PCK:
      while(stream->availableForWrite()){
        stream->write(outAwaiting[outTxRp ++]);
        if(outTxRp >= outAwaitingLen){
          outTxRp = 0;
          outAwaitingLen = 0;
          txState = SERLINK_TX_NONE;
          return;
        }
      }
      break;
    case SERLINK_TX_KPA:
      while(stream->availableForWrite()){
        stream->write(keepAlivePacket[keepAliveTxRp ++]);
        if(keepAliveTxRp >= 3){
          keepAliveTxRp = 0;
          txState = SERLINK_TX_NONE;
          return;
        }
      }
      break;
    default:
      break;
  }
}
