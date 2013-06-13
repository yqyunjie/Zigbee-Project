/*
 * File: ember-static-struct.h
 * Description: Type definitions for customer configurable memory allocations
 * Author(s): Richard Kelsey, kelsey@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#ifndef __EMBER_STATIC_STRUCT_H__ 
#define __EMBER_STATIC_STRUCT_H__

// This file is used in conjunction with ember-static-memory.h
//  to provide typedefs for structures allocated there.

// #########################################################
// # Application developers should not modify any portion  #
// #  of this file.  Doing so may lead to mysterious bugs. #
// #########################################################

// Neighbors

typedef struct {
  int16u data0[2];
  int8u  data1[10];
} EmNeighborTableEntry;

// Routing

typedef struct {
  int16u data0[2];
  int8u data1[2];
} EmRouteTableEntry;

typedef struct {
  int16u source;
  int16u sender;
  int8u  id;
  int8u  forwardRoutingCost;
  int8u  quarterSecondsToLive;
  int8u  routeTableIndex;
} EmDiscoveryTableEntry;

typedef struct {
  int16u source; 
  int8u sequence;    
  int16u neighborBitmask;
} EmBroadcastTableEntry;

// APS

typedef struct {
  int16u data0;
  int8u data1[8];
  int8u data2;
} EmAddressTableEntry;

typedef struct {
  int16u data0;
  int8u data1[4];
} EmApsUnicastMessageData;

typedef struct {
  int8u identifier[8];
  int8u key[16];
  int8u info;
} EmKeyTableEntry;

// Network general info

typedef struct {
  int16u parentId;
  int8u parentEui64[8];
  int8u nodeType;
  int8u zigbeeState;
  int8u radioChannel;
  int8s radioPower;
  int16u localNodeId;
  int16u localPanId;

  int32u securityStateBitmask;
  int8u macDataSequenceNumber;
  int8u zigbeeSequenceNumber;
  int8u apsSequenceNumber;
  int8u zigbeeNetworkSecurityLevel;

  // Network security stuff
  int32u nextNwkFrameCounter;
  int8u securityKeySequenceNumber;

  // APS security stuff
  int32u incomingTcLinkKeyFrameCounter;

  // Neighbor table
  EmNeighborTableEntry* neighborTable;
  int8u neighborTableSize;
  int8u neighborCount;

  // Incoming frame counters table
  int32u* frameCounters;

  // Child aging stuff
  //----------------------------------------------------------------
  // The last time we updated the child timers for each unit.
  // The milliseconds needs to be larger because we use it on children, who
  // may go a long time between calls to emberTick().
  int32u lastChildAgeTimeMs;
  int16u lastChildAgeTimeSeconds;
  int16u lastChildAgeTimeMinutes;// Not really minutes.  The units are set
                                 // by emberEndDevicePollTimeoutShift.

  // The number of ticks since our last successful poll.  Ticks are seconds
  // for mobile devices and (seconds << emberEndDevicePollTimeoutShift) for
  // other end devices.
  int8u ticksSinceLastPoll; // for timing out our parent
  int32u msSinceLastPoll;   // for APS retry timeout adjustment

  // Transmission statistics that are reported in NWK_UPDATE_RESPONSE ZDO
  // messages.
  int16u unicastTxAttempts;
  int16u unicastTxFailures;
}EmberNetworkInfo;

#endif // #ifndef __EMBER_STATIC_STRUCT_H__
