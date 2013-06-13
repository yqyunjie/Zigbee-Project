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
  value->concentratorType = fetchInt8u();
  value->routeRecordState = fetchInt8u();
}

void appendEmberInitialSecurityState(EmberInitialSecurityState *value)
{
  appendInt16u(value->bitmask);
  appendEmberKeyData(&(value->preconfiguredKey));
  appendEmberKeyData(&(value->networkKey));
  appendInt8u(value->networkKeySequenceNumber);
  appendInt8uArray(EUI64_SIZE, value->preconfiguredTrustCenterEui64);
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

void appendEmberZigbeeNetwork(EmberZigbeeNetwork *value)
{
  appendInt8u(value->channel);
  appendInt16u(value->panId);
  appendInt8uArray(8, value->extendedPanId);
  appendInt8u(value->allowingJoin);
  appendInt8u(value->stackProfile);
  appendInt8u(value->nwkUpdateId);
}

void appendEmberAesMmoHashContext(EmberAesMmoHashContext* context)
{
  appendInt8uArray(EMBER_AES_HASH_BLOCK_SIZE, context->result);
  appendInt32u(context->length);
}

void fetchEmberAesMmoHashContext(EmberAesMmoHashContext* context)
{
  fetchInt8uArray(EMBER_AES_HASH_BLOCK_SIZE, context->result);
  context->length = fetchInt32u();
}

void appendEmberNetworkInitStruct(const EmberNetworkInitStruct* networkInitStruct)
{
  appendInt16u(networkInitStruct->bitmask);
}

void fetchEmberNetworkInitStruct(EmberNetworkInitStruct* networkInitStruct)
{
  networkInitStruct->bitmask = fetchInt16u();
}

void appendEmberVersionStruct(const EmberVersion* versionStruct)
{
  appendInt16u(versionStruct->build);
  appendInt8u(versionStruct->major);
  appendInt8u(versionStruct->minor);
  appendInt8u(versionStruct->patch);
  appendInt8u(versionStruct->special);
  appendInt8u(versionStruct->type);
}

void fetchEmberVersionStruct(EmberVersion* versionStruct)
{
  versionStruct->build = fetchInt16u();
  versionStruct->major = fetchInt8u();
  versionStruct->minor = fetchInt8u();
  versionStruct->special = fetchInt8u();
  versionStruct->type  = fetchInt8u();
}

#ifndef XAP2B
void appendEmberZllNetwork(EmberZllNetwork *network)
{
  appendEmberZigbeeNetwork(&(network->zigbeeNetwork));
  appendEmberZllSecurityAlgorithmData(&(network->securityAlgorithm));
  appendInt8uArray(EUI64_SIZE, network->eui64);
  appendInt16u(network->nodeId);
  appendInt16u(network->state);
  appendInt8u(network->nodeType);
  appendInt8u(network->numberSubDevices);
  appendInt8u(network->totalGroupIdentifiers);
  appendInt8u(network->rssiCorrection);
}

void fetchEmberZllNetwork(EmberZllNetwork* network)
{
  fetchEmberZigbeeNetwork(&(network->zigbeeNetwork));
  fetchEmberZllSecurityAlgorithmData(&(network->securityAlgorithm));
  fetchInt8uArray(EUI64_SIZE, network->eui64);
  network->nodeId = fetchInt16u();
  network->state = fetchInt16u();
  network->nodeType = fetchInt8u();
  network->numberSubDevices = fetchInt8u();
  network->totalGroupIdentifiers = fetchInt8u();
  network->rssiCorrection = fetchInt8u();
}

void appendEmberZllSecurityAlgorithmData(EmberZllSecurityAlgorithmData* data)
{
  appendInt32u(data->transactionId);
  appendInt32u(data->responseId);
  appendInt16u(data->bitmask);
}

void fetchEmberZllSecurityAlgorithmData(EmberZllSecurityAlgorithmData* data)
{
  data->transactionId = fetchInt32u();         
  data->responseId    = fetchInt32u();         
  data->bitmask       = fetchInt16u();         
}

void appendEmberZllInitialSecurityState(EmberZllInitialSecurityState* state)
{
  appendInt32u(state->bitmask);              
  appendInt8u(state->keyIndex);              
  appendEmberKeyData(&((state)->encryptionKey));     
  appendEmberKeyData(&((state)->preconfiguredKey));  
}

void appendEmberTokTypeStackZllData(EmberTokTypeStackZllData *data)
{
  appendInt32u(data->bitmask);
  appendInt16u(data->freeNodeIdMin);
  appendInt16u(data->freeNodeIdMax);
  appendInt16u(data->myGroupIdMin);
  appendInt16u(data->freeGroupIdMin);
  appendInt16u(data->freeGroupIdMax);
  appendInt8u(data->rssiCorrection);
}

void fetchEmberTokTypeStackZllData(EmberTokTypeStackZllData *data)
{
  data->bitmask = fetchInt32u();
  data->freeNodeIdMin = fetchInt16u();
  data->freeNodeIdMax = fetchInt16u();
  data->myGroupIdMin = fetchInt16u();
  data->freeGroupIdMin = fetchInt16u();
  data->freeGroupIdMax = fetchInt16u();
  data->rssiCorrection = fetchInt8u();
}

void appendEmberTokTypeStackZllSecurity(EmberTokTypeStackZllSecurity *security)
{
  appendInt32u(security->bitmask);
  appendInt8u(security->keyIndex);
  appendInt8uArray(16, security->encryptionKey);
  appendInt8uArray(16, security->preconfiguredKey);
}

void fetchEmberTokTypeStackZllSecurity(EmberTokTypeStackZllSecurity *security)
{
  security->bitmask = fetchInt32u();
  security->keyIndex = fetchInt8u();
  fetchInt8uArray(16, security->encryptionKey);
  fetchInt8uArray(16, security->preconfiguredKey);
}
#endif // XAP2B
