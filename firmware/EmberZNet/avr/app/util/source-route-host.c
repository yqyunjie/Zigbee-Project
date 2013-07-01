// File: source-route-host.c
//
// Description: Example code for managing source routes on a host-based gateway.
// For node-based gateways refer to source-route.c.
//
// New source routes are added to the table in the
// ezspIncomingRouteRecordHandler() callback. Adding a route results in routes
// to all intermediate relays as well.
//
// Before sending a unicast message, the application must obtain the source
// route (using emberFindSourceRoute() provided in this file) and then call
// ezspSetSourceRoute().
//
// In this implementation, the maximum table size is 255 entries since a
// one-byte index is used and the index 0xFF is reserved.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/ezsp/ezsp-host-configuration-defaults.h"
#include "source-route-common.h"

// AppBuilder includes this file and uses the define below to turn off source
// routing. This doesnt affect non-AppBuilder applications.
#ifndef ZA_NO_SOURCE_ROUTING

static SourceRouteTableEntry table[EZSP_HOST_SOURCE_ROUTE_TABLE_SIZE];
int8u sourceRouteTableSize = EZSP_HOST_SOURCE_ROUTE_TABLE_SIZE;
SourceRouteTableEntry *sourceRouteTable = table;

void ezspIncomingRouteRecordHandler(EmberNodeId source,
                                    EmberEUI64 sourceEui,
                                    int8u lastHopLqi,
                                    int8s lastHopRssi,
                                    int8u relayCount,
                                    int16u *relayList)
{
  int8u previous;
  int8u i;

  if (sourceRouteTableSize == 0) {
    return;
  }

  // The source of the route record is furthest from the gateway. We start there
  // and work closer.
  previous = sourceRouteAddEntry(source, NULL_INDEX);

  // Go through the relay list and add them to the source route table.
  for (i = 0; i < relayCount; i++) {
    EmberNodeId id = relayList[i];
    // We pass the index of the previous entry to link the route together.
    previous = sourceRouteAddEntry(id, previous);
  }
}

// Note: We assume that the given relayList location is big enough to handle the
// longest source route.
boolean emberFindSourceRoute(EmberNodeId destination,
                             int8u *relayCount,
                             int16u *relayList)
{
  int8u index = sourceRouteFindIndex(destination);

  if (index == NULL_INDEX) {
    return FALSE;
  }

  // Fill in the relay list. The first relay in the list is the closest to the
  // destination (furthest from the gateway).
  *relayCount = 0;
  while (sourceRouteTable[index].closerIndex != NULL_INDEX) {
    index = sourceRouteTable[index].closerIndex;
    relayList[*relayCount] = sourceRouteTable[index].destination;
    *relayCount += 1;
  }
  return TRUE;
}

#endif //ZA_NO_SOURCE_ROUTING
