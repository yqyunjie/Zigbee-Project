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


#endif // #ifndef __EMBER_STATIC_STRUCT_H__
