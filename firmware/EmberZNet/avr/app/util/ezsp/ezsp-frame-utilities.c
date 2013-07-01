// File: ezsp-frame-utilities.h
// 
// Description: Functions for reading and writing command and response frames.
// 
// Copyright 2006 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"
#include "ezsp-protocol.h"
#include "ezsp-frame-utilities.h"

int8u* ezspReadPointer;
int8u* ezspWritePointer;

int8u fetchInt8u(void)
{
  return *ezspReadPointer++;
}

void appendInt8u(int8u value)
{
  if (ezspWritePointer - ezspFrameContents < EZSP_MAX_FRAME_LENGTH) {
    *ezspWritePointer = value;
  }
  ezspWritePointer++;
}

void appendInt16u(int16u value)
{
  appendInt8u(LOW_BYTE(value));
  appendInt8u(HIGH_BYTE(value));
}

int16u fetchInt16u(void)
{
  int8u low = fetchInt8u();
  int8u high = fetchInt8u();
  return HIGH_LOW_TO_INT(high, low);
}

void appendInt32u(int32u value)
{
  appendInt16u((int16u)(value & 0xFFFF));
  appendInt16u((int16u)(value >> 16 & 0xFFFF));
}

int32u fetchInt32u(void)
{
  int16u low = fetchInt16u();
  return (((int32u) fetchInt16u()) << 16) + low;
}

void appendInt8uArray(int8u length, int8u *contents)
{
  if (ezspWritePointer - ezspFrameContents + length <= EZSP_MAX_FRAME_LENGTH) {
    MEMCOPY(ezspWritePointer, contents, length);
  }
  ezspWritePointer += length;
}

void appendInt16uArray(int8u length, int16u *contents)
{
  int8u i;
  for (i = 0; i < length; i++) {
    appendInt16u(contents[i]);
  }
}

void fetchInt8uArray(int8u length, int8u *contents)
{
  MEMCOPY(contents, ezspReadPointer, length);
  ezspReadPointer += length;
}

void fetchInt16uArray(int8u length, int16u *contents)
{
  int8u i;
  for (i = 0; i < length; i++) {
    contents[i] = fetchInt16u();
  }
}

void fetchEmberNetworkParameters(EmberNetworkParameters *value)
{
  fetchInt8uArray(EXTENDED_PAN_ID_SIZE, value->extendedPanId);
  value->panId = fetchInt16u();
  value->radioTxPower = fetchInt8u();
  value->radioChannel = fetchInt8u();
  value->joinMethod = fetchInt8u();
  value->nwkManagerId = fetchInt16u();
  value->nwkUpdateId = fetchInt8u();
  value->channels = fetchInt32u();
}

void appendEmberApsFrame(EmberApsFrame *value)
{
  appendInt16u(value->profileId);
  appendInt16u(value->clusterId);
  appendInt8u(value->sourceEndpoint);
  appendInt8u(value->destinationEndpoint);
  appendInt16u(value->options);
  appendInt16u(value->groupId);
  appendInt8u(value->sequence);
}

void fetchEmberApsFrame(EmberApsFrame *value)
{
  value->profileId = fetchInt16u();
  value->clusterId = fetchInt16u();
  value->sourceEndpoint = fetchInt8u();
  value->destinationEndpoint = fetchInt8u();
  value->options = fetchInt16u();
  value->groupId = fetchInt16u();
  value->sequence = fetchInt8u();
}

void appendEmberMulticastTableEntry(EmberMulticastTableEntry *value)
{
  appendInt16u(value->multicastId);
  appendInt8u(value->endpoint);
}

void fetchEmberMulticastTableEntry(EmberMulticastTableEntry *value)
{
  value->multicastId = fetchInt16u();
  value->endpoint    = fetchInt8u();
}

void fetchEmberNeighborTableEntry(EmberNeighborTableEntry *value)
{
  value->shortId = fetchInt16u();
  value->averageLqi = fetchInt8u();
  value->inCost = fetchInt8u();
  value->outCost = fetchInt8u();
  value->age = fetchInt8u();
  fetchInt8uArray(EUI64_SIZE, value->longId);
}

void fetchEmberRouteTableEntry(EmberRouteTableEntry *value)
{
  value->destination = fetchInt16u();
  value->nextHop = fetchInt16u();
  value->status = fetchInt8u();
  value->age = fetchInt8u();
}

void appendEmberInitialSecurityState(EmberInitialSecurityState *value)
{
  appendInt16u(value->bitmask);
  appendEmberKeyData(&(value->preconfiguredKey));
  appendEmberKeyData(&(value->networkKey));
  appendInt8u(value->networkKeySequenceNumber);
}

void fetchEmberCurrentSecurityState(EmberCurrentSecurityState *value)
{
  value->bitmask = fetchInt16u();
  fetchInt8uArray(EUI64_SIZE, value->trustCenterLongAddress);
}

void fetchEmberKeyStruct(EmberKeyStruct *value)
{
  value->bitmask = fetchInt16u();
  value->type = fetchInt8u();
  fetchEmberKeyData(&(value->key));
  value->outgoingFrameCounter = fetchInt32u();
  value->incomingFrameCounter = fetchInt32u();
  value->sequenceNumber = fetchInt8u();
  fetchInt8uArray(EUI64_SIZE, value->partnerEUI64);
}

void fetchEmberZigbeeNetwork(EmberZigbeeNetwork *value)
{
  value->channel = fetchInt8u();
  value->panId = fetchInt16u();
  fetchInt8uArray(8, value->extendedPanId);
  value->allowingJoin = fetchInt8u();
  value->stackProfile = fetchInt8u();
  value->nwkUpdateId = fetchInt8u();
}

