// File: source-route-common.h
//
// Description: Common code used for managing source routes on both node-based
// and host-based gateways. See source-route.c for node-based gateways and
// source-route-host.c for host-based gateways.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __SOURCE_ROUTE_COMMON_H__
#define __SOURCE_ROUTE_COMMON_H__

typedef struct {
  EmberNodeId destination;
  int8u closerIndex;          // The entry one hop closer to the gateway.
  int8u olderIndex;           // The entry touched before this one.
} SourceRouteTableEntry;

extern int8u sourceRouteTableSize;
extern SourceRouteTableEntry *sourceRouteTable;

// A special index. For destinations that are neighbors of the gateway,
// closerIndex is set to 0xFF. For the oldest entry, olderIndex is set to
// 0xFF.
#define NULL_INDEX 0xFF

int8u sourceRouteFindIndex(EmberNodeId id);
int8u sourceRouteAddEntry(EmberNodeId id, int8u furtherIndex);
void sourceRouteInit(void);

#endif // __SOURCE_ROUTE_COMMON_H__
