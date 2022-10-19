/*
osap/osapLoop.cpp

main osap op: whips data vertex-to-vertex

Jake Read at the Center for Bits and Atoms
(c) Massachusetts Institute of Technology 2021

This work may be reproduced, modified, distributed, performed, and
displayed for any purpose, but must acknowledge the osap project.
Copyright is retained and must be preserved. The work is provided as is;
no warranty is provided, and users accept all liability.
*/

#include "loop.h"
#include "packets.h"
#include "osap.h"

#define MAX_ITEMS_PER_LOOP 32
//#define LOOP_DEBUG

// we'll stack up to 64 messages to handle per loop, 
// more items would cause issues: will throw errors and design circular looping at that point 
stackItem* itemList[MAX_ITEMS_PER_LOOP];
uint16_t itemListLen = 0;

void listSetupRecursor(Vertex* vt){
  // run the vertex' loop... but not if it's the root, yar 
  if(vt->type != VT_TYPE_ROOT) vt->loop();
  // for each input / output stack, try to collect all items... 
  // alright I'm doing this collect... but want a kind of pickup-where-you-left-off thing, 
  // so that we can have a fixed-length loop, i.e. 64 items per, but still do fairness... 
  // otherwise our itemList has to be large enough to carry potentially every single item ? 
  for(uint8_t od = 0; od < 2; od ++){
    uint8_t count = stackGetItems(vt, od, &(itemList[itemListLen]), MAX_ITEMS_PER_LOOP - itemListLen);
    itemListLen += count;
  }
  // recurse children...
  for(uint8_t c = 0; c < vt->numChildren; c ++){
    listSetupRecursor(vt->children[c]);
  }
}

// sort-in-place based on time-to-death, 
void listSort(stackItem** list, uint16_t listLen){
  // write each item's time-to-death, 
  uint32_t now = millis();
  for(uint16_t i = 0; i < listLen; i ++){
    list[i]->timeToDeath = ts_readUint16(list[i]->data, 0) - (now - list[i]->arrivalTime);
  }
  // also... vertex arrivalTime should be uint32_t milliseconds of arrival... 
  #warning not-yet sorted... 
}

// this handles internal transport... checking for errors along paths, and running flowcontrol 
// returns true to wipe current item, false to leave-in-wait, 
boolean internalTransport(stackItem* item, uint16_t ptr){
  // we walk thru our little internal tree here, 
  Vertex* vt = item->vt;
  // ptr for the walk, use item->data[ptr] == PK_INSTRUCTION, not PK_PTR, 
  uint16_t fwdPtr = ptr + 1;
  // count # of ops, 
  uint8_t opCount = 0;
  // for a max. of 16 fwd steps, 
  for(uint8_t s = 0; s < 16; s ++){
    uint16_t arg = readArg(item->data, fwdPtr);
    switch(PK_READKEY(item->data[fwdPtr])){
      // ---------------------------------------- Internal Dir Cases 
      case PK_SIB:
        // check validity of route & shift our reference vt,
        if(vt->parent == nullptr){
          OSAP::error("no parent at " + vt->name + " during sib transport"); return true;
        } else if (arg >= vt->parent->numChildren){
          OSAP::error("no sibling " + String(arg) + " at " + vt->name + " during sib transport"); return true;
        } else {
          // this is it: we go fwds to this vt & end-of-switch statements increment ptrs
          vt = vt->parent->children[arg];
        }
        break;
      case PK_PARENT:
        if(vt->parent == nullptr){
          OSAP::error("no parent at " + vt->name + " during parent transport"); return true;
        } else {
          // likewise... 
          vt = vt->parent;
        }
        break;
      case PK_CHILD:
        if(arg >= vt->numChildren){
          OSAP::error("no child " + String(arg) + " at " + vt->name + " during child transport"); return true;
        } else {
          // again, just walk fwds... 
          vt = vt->children[arg];
        }
        break;
      // ---------------------------------------- Terminal / Exit Cases 
      case PK_PFWD:
      case PK_BFWD:
      case PK_BBRD: 
      case PK_DEST:
      case PK_PINGREQ:
      case PK_SCOPEREQ:
      case PK_LLESCAPE:
        // check / transport...
        if(stackEmptySlot(vt, VT_STACK_DESTINATION)){
          // walk the ptr fwds, 
          walkPtr(item->data, item->vt, opCount, ptr);
          // ingest at the new place, 
          stackLoadSlot(vt, VT_STACK_DESTINATION, item->data, item->len);
          // return true to clear it out, 
          return true;
        } else {
          return false; 
        }
      default:
        OSAP::error("internal transport failure, ptr walk ends at unknown key");
        return true;
    } // end switch 
    fwdPtr += 2;
    opCount ++;
  } // end max-16-steps, 
  // if we're past all 16 and didn't hit a terminal, pckt is eggregiously long, rm it 
  return true;
}

// -------------------------------------------------------- LOOP Begins Here 

// ... would be breadth-first, ideally 
void osapLoop(Vertex* root){
  // we want to build a list of items, recursing through... 
  itemListLen = 0;
  listSetupRecursor(root);
  // check now if items are nearly oversized...
  // see notes in the log from 2022-06-22 if this error occurs, 
  if(itemListLen >= MAX_ITEMS_PER_LOOP - 2){
    OSAP::error("loop items exceeds " + String(MAX_ITEMS_PER_LOOP) + ", breaking per-loop transport properties... pls fix", HALTING);
  }
  // stash high-water mark,
  if(itemListLen > OSAP::loopItemsHighWaterMark) OSAP::loopItemsHighWaterMark = itemListLen;
  // log 'em 
  // OSAP::debug("list has " + String(itemListLen) + " elements", LOOP);
  // otherwise we can carry on... the item should be sorted, global vars, 
  listSort(itemList, itemListLen);
  // then we can handle 'em one by one 
  for(uint16_t i = 0; i < itemListLen; i ++){
    osapItemHandler(itemList[i]);
  }
}

void osapItemHandler(stackItem* item){
  // clear dead items, 
  if(item->timeToDeath < 0){
    OSAP::debug(  "item at " + item->vt->name + " times out w/ " + String(item->timeToDeath) + 
                  " ms to live, of " + String(ts_readUint16(item->data, 0)) + " ttl", LOOP);
    stackClearSlot(item);
    return;
  }
  // get a ptr for the item, 
  uint16_t ptr = 0;
  if(!findPtr(item->data, &ptr)){    
    OSAP::error("item at " + item->vt->name + " unable to find ptr, deleting...");
    stackClearSlot(item);
    return;
  }
  // now the handle-switch, item->data[ptr] = PK_PTR, we switch on instruction which is behind that, 
  switch(PK_READKEY(item->data[ptr + 1])){
    // ------------------------------------------ Terminal / Destination Switches 
    case PK_DEST:
      item->vt->destHandler(item, ptr);
      break;
    case PK_PINGREQ:
      item->vt->pingRequestHandler(item, ptr);
      break;
    case PK_SCOPEREQ:
      item->vt->scopeRequestHandler(item, ptr);
      break;
    case PK_PINGRES:
    case PK_SCOPERES:
      OSAP::error("ping or scope request issued to " + item->vt->name + " not handling those in embedded", MEDIUM);
      stackClearSlot(item);
      break;
    // ------------------------------------------ Internal Transport 
    case PK_SIB:
    case PK_PARENT:
    case PK_CHILD:  // transport handler returns true if msg should be wiped, false if it should be cycled
      if(internalTransport(item, ptr)){
        stackClearSlot(item);
      }
      break;
    // ------------------------------------------ Network Transport 
    case PK_PFWD:
      // port forward...
      if(item->vt->vport == nullptr){
        OSAP::error("pfwd to non-vport " + item->vt->name, MEDIUM);
        stackClearSlot(item);
      } else {
        if(item->vt->vport->cts()){
          // walk one step, but only if fn returns true (having success) 
          if(walkPtr(item->data, item->vt, 1, ptr)) item->vt->vport->send(item->data, item->len);
          stackClearSlot(item);
        } else {
          // failed to send this turn (flow controlled), will return here next round 
        }
      }
      break;
    case PK_BFWD:
    case PK_BBRD:
      // bus forward / bus broadcast: 
      if(item->vt->vbus == nullptr){
        OSAP::error("bfwd to non-vbus " + item->vt->name, MEDIUM);
        stackClearSlot(item);
      } else {
        // arg is rxAddr for bus-forwards, is broadcastChannel for bus-broadcast, 
        uint16_t arg = readArg(item->data, ptr + 1);
        if(item->data[ptr + 1] == PK_BFWD){
          if(item->vt->vbus->cts(arg)){
            if(walkPtr(item->data, item->vt, 1, ptr)){
              item->vt->vbus->send(item->data, item->len, arg);
            } else {
              OSAP::error("bfwd fails for bad ptr walk");
            }
            stackClearSlot(item);
          } else {
            // failed to bfwd (flow controlled), returning here next round... 
          }
        } else if (item->data[ptr + 1] == PK_BBRD){
          if(item->vt->vbus->ctb(arg)){
            if(walkPtr(item->data, item->vt, 1, ptr)){
              // OSAP::debug("broadcasting on ch " + String(arg));
              item->vt->vbus->broadcast(item->data, item->len, arg);
            } else {
              OSAP::error("bbrd fails for bad ptr walk");
            }
            stackClearSlot(item);
          } else {
            // failed to bbrd, returning next... 
          }
        } else {
          // doesn't make any sense, we switched in on these terms... 
          OSAP::error("absolute nonsense", MEDIUM);
          stackClearSlot(item);
        }
      }
      break;
    case PK_LLESCAPE:
      OSAP::error("lldebug to embedded, dumping", MINOR);
      stackClearSlot(item);
      break;
    default:
      OSAP::error("unrecognized ptr to " + item->vt->name + " " + String(PK_READKEY(item->data[ptr + 1])), MINOR);
      stackClearSlot(item);
      // error, delete, 
      break;
  } // end swiiiitch 
}