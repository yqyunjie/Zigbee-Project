// File: source-route.c
//
// Description: Example code for managing source routes on a node-based gateway.
// For host-based gateways refer to source-route-host.c.
//
// New source routes are added to the table in the
// emberIncomingRouteRecordHandler() callback. Adding a route results in routes
// to all intermediate relays as well.
//
// For every outgoing packet, the stack calls emberAppendSourceRouteHandler().
// If a source route to the destination is found, it is added to the packet.
//
// In this implementation, the maximum table size is 255 entries since a
// one-byte index is used and the index 0xFF is reserved.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"
#include "app/util/security/security.h"
#include "source-route-common.h"

// AppBuilder includes this file and uses the define below to turn off source
// routing. This doesnt affect non-AppBuilder applications.
#ifndef ZA_NO_SOURCE_ROUTING

// ZigBee protocol limitations effectively constrain us to a 11-hop source route
// in the worst case scenario (for a 12-hop delivery in total), so we enforce
// a threshold value on the relayCount and ignore source route operations beyond
// this threshold.  See explanatory comment in emberAppendSourceRouteHandler()
// for more details about this.
// This value could be reduced at the designer's discretion, but not increased
// beyond 11 without causing routing problems.
#define MAX_RELAY_COUNT    11

#ifndef EXTERNAL_TABLE
static SourceRouteTableEntry table[EMBER_SOURCE_ROUTE_TABLE_SIZE];
int8u sourceRouteTableSize = EMBER_SOURCE_ROUTE_TABLE_SIZE;
SourceRouteTableEntry *sourceRouteTable = table;
#endif


void emberIncomingRouteRecordHandler(EmberNodeId source,
                                     EmberEUI64 sourceEui,
                                     int8u relayCount,
                                     EmberMessageBuffer header,
                                     int8u relayListIndex)
{
  int8u previous;
  int8u i;

  if (sourceRouteTableSize == 0) {
    return;
  }

  // Ignore over-sized source routes, since we can't reuse them reliably anyway.
  if (relayCount > MAX_RELAY_COUNT) {
    return;
  }

  // The source of the route record is furthest from the gateway. We start there
  // and work closer.
  previous = sourceRouteAddEntry(source, NULL_INDEX);

  // Go through the relay list and add them to the source route table.
  for (i = 0; i < relayCount; i++) {
    int8u index = relayListIndex + (i << 1);
    EmberNodeId id = emberGetLinkedBuffersLowHighInt16u(header, index);
    // We pass the index of the previous entry to link the route together.
    previous = sourceRouteAddEntry(id, previous);
  }
  
  // If the following message has APS Encryption, our node will need to know
  // the IEEE Address of the source in order to properly decrypt.
  securityAddToAddressCache(source, sourceEui);
}

int8u emberAppendSourceRouteHandler(EmberNodeId destination,
                                    EmberMessageBuffer header)
{
  int8u foundIndex = sourceRouteFindIndex(destination);
  int8u relayCount = 0;
  int8u addedBytes;
  int8u bufferLength = emberMessageBufferLength(header);
  int8u i;

  if (foundIndex == NULL_INDEX) {
    return 0;
  }

  // Find out what the relay count is.
  i = foundIndex;
  while (sourceRouteTable[i].closerIndex != NULL_INDEX) {
    i = sourceRouteTable[i].closerIndex;
    relayCount++;
  }

  // Per Ember Case 10096, we need to protect against oversized source
  // routes that will overflow the PHY packet length (127).  Since the
  // worst-case packet overhead is the APS-encrypted, NWK-encrypted,
  // tunneled key delivery sent by the Trust Center during
  // authentication of new devices, and this leaves enough only room
  // for an 11-hop source route frame, we abort our route-appending
  // operation if the resulting source route contains more than 11
  // relays.  Unless a route already exists to the destination, this
  // will likely result in the stack not sending the packet at all,
  // but this is better than forcing too much data into the packet
  // (which, as of this writing, will trigger an assert in mac.c and
  // cause a reset).

  if (MAX_RELAY_COUNT < relayCount) {
    return 0;
  }

  addedBytes = 2 + (relayCount << 1);

  if (header != EMBER_NULL_MESSAGE_BUFFER) {
    // Two bytes for the relay count and relay index.
    int8u bufferIndex = bufferLength;

    if (emberSetLinkedBuffersLength(header, bufferLength + addedBytes)
        != EMBER_SUCCESS) {
      return 0;
    }
    
    // Set the relay count and relay index.
    emberSetLinkedBuffersByte(header, bufferIndex++, relayCount);
    emberSetLinkedBuffersByte(header,
                              bufferIndex++,
                              (0 < relayCount
                               ? relayCount - 1
                               : 0));

    // Fill in the relay list. The first relay in the list is the closest to the
    // destination (furthest from the gateway).
    i = foundIndex;
    while (sourceRouteTable[i].closerIndex != NULL_INDEX) {
      i = sourceRouteTable[i].closerIndex;
      emberSetLinkedBuffersLowHighInt16u(header, 
                                         bufferIndex,
                                         sourceRouteTable[i].destination);
      bufferIndex += 2;
    }
  }
  
  return addedBytes;
}
#endif //ZA_NO_SOURCE_ROUTING



