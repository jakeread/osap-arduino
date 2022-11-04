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
}

VPort_ArduinoSerial::VPort_ArduinoSerial( Vertex* _parent, const char* _name, Serial_* _usbcdc
) : VPort ( _parent, _name ){
  stream = _usbcdc;
  usbcdc = _usbcdc;
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

#warning TODO here 
/*
OK I think this is the plan:
- we can rm the rxBuffer & inAwaiting double-up by guarding the while(stream->available()) w/ while(stream->available() && rxBufferLen == 0)
  - then cobs-decode it straight into an OSAP packet, ok 
- then we can rm the txBuffer and outAwaiting double-up by guarding cts() with, also, (txBufferLen == 0), meaning we won't ever 
  - have OSAP write into it w/o our consent... then this is easy, and it saves... two memcpy, on every packet that goes thru, 
  - should count for something, non ? 
*/
void VPort_ArduinoSerial::loop(void){
  // byte injestion: think of this like the rx interrupt stage, 
  while(stream->available()){
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
              inAwaitingId = rxBuffer[2]; // stash ID 
              inAwaitingLen = cobsDecode(&(rxBuffer[3]), rxBufferWp - 2, inAwaiting); // fill inAwaiting 
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

  // check insertion & genny the ack if we can 
  if(inAwaitingLen && !ackIsAwaiting){
    // can we get a packet to write into?
    VPacket* pck = stackRequest(this);
    // with this new thing... we could get the packet *ahead of time* 
    // to write-into in this code, also saving memory... 
    // for now I'll just try it with a kind of replacement, more copying... 
    if(pck != nullptr){
      // digitalWrite(2, HIGH);
      stackLoadPacket(pck, inAwaiting, inAwaitingLen);
      // we've loaded it, can ack that: 
      ackIsAwaiting = true;
      ackAwaiting[0] = 4;                 // checksum still, innit 
      ackAwaiting[1] = SERLINK_KEY_ACK;   // it's an ack bruv 
      ackAwaiting[2] = inAwaitingId;      // which pck r we akkin m8 
      ackAwaiting[3] = 0;                 // delimiter 
      inAwaitingLen = 0;
    } // else we are awaiting & will check back 
  }
  // check & execute actual tx 
  checkOutputStates();
}

void VPort_ArduinoSerial::send(uint8_t* data, uint16_t len){
  // cts() == true means that our outAwaiting has been tx'd, is drained, etc 
  if(!cts()) return;
  // setup, 
  outAwaiting[0] = len + 5;               // pck[0] is checksum = len + checksum + cobs start + cobs delimit + ack/pack + id 
  outAwaiting[1] = SERLINK_KEY_PCK;       // this ones a packet m8 
  outAwaitingId ++; if(outAwaitingId == 0) outAwaitingId = 1;
  outAwaiting[2] = outAwaitingId;         // an id     
  cobsEncode(data, len, &(outAwaiting[3]));  // encode 
  outAwaiting[len + 4] = 0;               // stuff delimiter, 
  outAwaitingLen = outAwaiting[0];        // track... 
  // transmit attempts etc 
  outAwaitingNTA = 0;
  outAwaitingLTAT = 0;
  // try it 
  checkOutputStates();                    // try / start write 
}

// we are CTS if outPck is not occupied, 
boolean VPort_ArduinoSerial::cts(void){
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
    } else if (outAwaitingLen && (outAwaitingLTAT == 0 || outAwaitingLTAT + SERLINK_RETRY_TIME < micros())){
      // then packets... the above says: if we have an out packet and (1) we haven't yet tx'd it *or* (2) we should retry it, 
      if(outAwaitingNTA > SERLINK_RETRY_MACOUNT){
        // bail on it, this was meant to be our last attempt, still no ack, 
        outAwaitingLen = 0;
      } else {
        // setup to retransmit 
        txState = SERLINK_TX_PCK;
        lastTxTime = millis();
        outAwaitingLTAT = micros();
        outTxRp = 0;
        outAwaitingNTA ++;
      }
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
  // --------------------------
  /*
  if(ackIsAwaiting && txBufferLen == 0){   // can we ack? 
    memcpy(txBuffer, ackAwaiting, 4);
    txBufferLen = 4;
    lastTxTime = millis();
    txBufferRp = 0;
    ackIsAwaiting = false;
  } else if(outAwaitingLen > 0 && txBufferLen == 0){   // would we be clear to tx ? 
    // check retransmit cases, 
    if(outAwaitingLTAT == 0 || outAwaitingLTAT + SERLINK_RETRY_TIME < micros()){
      memcpy(txBuffer, outAwaiting, outAwaitingLen);
      outAwaitingLTAT = micros();
      txBufferLen = outAwaitingLen;
      lastTxTime = millis();
      txBufferRp = 0;
      outAwaitingNTA ++;
    } 
    // check if last attempt, 
    if(outAwaitingNTA >= SERLINK_RETRY_MACOUNT){
      outAwaitingLen = 0;
    }
  } else if (millis() - lastTxTime > SERLINK_KEEPALIVE_TX_TIME && txBufferLen == 0){
    //OSAP_DEBUG("keepalive-ing " + name + " " + String(isOpen()));
    memcpy(txBuffer, keepAlivePacket, 3);
    txBufferLen = 3;
    lastTxTime = millis();
  }
  // finally, we write out so long as we can: 
  // we aren't guaranteed to get whole pckts out in each fn call 
  while(stream->availableForWrite() && txBufferLen != 0){
    // output next byte, 
    stream->write(txBuffer[txBufferRp ++]);
    // check for end of buffer; reset transmit states if so 
    if(txBufferRp >= txBufferLen) {
      txBufferLen = 0; 
      txBufferRp = 0;
    }
  }
  */
}