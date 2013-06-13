// File: zigbee-device-host.c
//
// Description: ZigBee Device Object (ZDO) functions not provided by the stack.
//
//
// Copyright 2007 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "zigbee-device-common.h"
#include "zigbee-device-host.h"

static int8u zigDevRequestBuffer[EZSP_MAX_FRAME_LENGTH];

static EmberStatus sendZigDevRequestBuffer(EmberNodeId target,
                                           int16u clusterId,
                                           EmberApsOption options,
                                           int8u length);

EmberStatus emberNetworkAddressRequest(EmberEUI64 target,
                                       boolean reportKids,
                                       int8u childStartIndex)
{
  int8u *payload = zigDevRequestBuffer + ZDO_MESSAGE_OVERHEAD;
  MEMCOPY(payload, target, EUI64_SIZE);
  payload[8] = reportKids ? 1 : 0;
  payload[9] = childStartIndex;
  return sendZigDevRequestBuffer(EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS,
                                 NETWORK_ADDRESS_REQUEST,
                                 EMBER_APS_OPTION_SOURCE_EUI64,
                                 ZDO_MESSAGE_OVERHEAD + 10);
}

EmberStatus emberIeeeAddressRequest(EmberNodeId target,
                                    boolean reportKids,
                                    int8u childStartIndex,
                                    EmberApsOption options)
{
  int8u *payload = zigDevRequestBuffer + ZDO_MESSAGE_OVERHEAD;
  payload[0] = LOW_BYTE(target);
  payload[1] = HIGH_BYTE(target);
  payload[2] = reportKids ? 1 : 0;
  payload[3] = childStartIndex;
  return sendZigDevRequestBuffer(target,
                                 IEEE_ADDRESS_REQUEST,
                                 options,
                                 ZDO_MESSAGE_OVERHEAD + 4);
}

EmberStatus ezspMatchDescriptorsRequest(EmberNodeId target,
                                        int16u profile,
                                        int8u inCount,
                                        int8u outCount,
                                        int16u *inClusters,
                                        int16u *outClusters,
                                        EmberApsOption options)
{
  int8u i;
  int8u *payload = zigDevRequestBuffer + ZDO_MESSAGE_OVERHEAD;
  int8u offset = ZDO_MESSAGE_OVERHEAD + 5;   // Add 2 bytes for NWK Address 
                                             // Add 2 bytes for Profile Id
                                             // Add 1 byte for in Cluster Count
  int8u length = (offset + 
                  (inCount * 2) + // Times 2 for 2 byte Clusters
                  1 +             // Out Cluster Count
                  (outCount * 2)); // Times 2 for 2 byte Clusters
  
  if (length > EZSP_MAX_FRAME_LENGTH)
    return EMBER_NO_BUFFERS;

  payload[0] = LOW_BYTE(target);
  payload[1] = HIGH_BYTE(target);
  payload[2] = LOW_BYTE(profile);
  payload[3] = HIGH_BYTE(profile);
  payload[4] = inCount;

  for (i = 0; i < inCount; i++) {
    zigDevRequestBuffer[(i * 2) + offset] = LOW_BYTE(inClusters[i]);
    zigDevRequestBuffer[(i * 2) + offset + 1] = HIGH_BYTE(inClusters[i]);
  }
  offset += (inCount * 2);
  zigDevRequestBuffer[offset] = outCount;
  offset++;
  for (i = 0; i < outCount; i++) {
    zigDevRequestBuffer[(i * 2) + offset] = LOW_BYTE(outClusters[i]);
    zigDevRequestBuffer[(i * 2) + offset + 1] = HIGH_BYTE(outClusters[i]);
  }

  return sendZigDevRequestBuffer(target,
                                 MATCH_DESCRIPTORS_REQUEST,
                                 options,
                                 length);
}

EmberStatus ezspEndDeviceBindRequest(EmberNodeId localNodeId,
                                     EmberEUI64 localEui64,
                                     int8u endpoint,
                                     int16u profile,
                                     int8u inCount,
                                     int8u outCount,
                                     int16u *inClusters,
                                     int16u *outClusters,
                                     EmberApsOption options)
{
  int8u i;
  int8u *payload = zigDevRequestBuffer + ZDO_MESSAGE_OVERHEAD;
  int8u offset = ZDO_MESSAGE_OVERHEAD + 14;  // Add 2 bytes for our NWK Address 
                                             // Add 8 bytes for our EUI64
                                             // Add 1 byte for endpoint
                                             // Add 2 bytes for Profile Id
                                             // Add 1 byte for in Cluster Count
  int8u length = (offset + 
                  (inCount * 2) +  // Times 2 for 2 byte Clusters
                  1 +              // Out Cluster Count
                  (outCount * 2)); // Times 2 for 2 byte Clusters
  
  if (length > EZSP_MAX_FRAME_LENGTH)
    return EMBER_NO_BUFFERS;

  payload[0] = LOW_BYTE(localNodeId);
  payload[1] = HIGH_BYTE(localNodeId);
  MEMCOPY(payload + 2, localEui64, 8);
  payload[10] = endpoint;
  payload[11] = LOW_BYTE(profile);
  payload[12] = HIGH_BYTE(profile);
  payload[13] = inCount;

  for (i = 0; i < inCount; i++) {
    zigDevRequestBuffer[(i * 2) + offset] = LOW_BYTE(inClusters[i]);
    zigDevRequestBuffer[(i * 2) + offset + 1] = HIGH_BYTE(inClusters[i]);
  }
  offset += (inCount * 2);
  zigDevRequestBuffer[offset] = outCount;
  offset++;
  for (i = 0; i < outCount; i++) {
    zigDevRequestBuffer[(i * 2) + offset] = LOW_BYTE(outClusters[i]);
    zigDevRequestBuffer[(i * 2) + offset + 1] = HIGH_BYTE(outClusters[i]);
  }

  return sendZigDevRequestBuffer(EMBER_ZIGBEE_COORDINATOR_ADDRESS,
                                 END_DEVICE_BIND_REQUEST,
                                 options,
                                 length);
}

EmberStatus emberSendZigDevRequest(EmberNodeId destination,
                                   int16u clusterId,
                                   EmberApsOption options,
                                   int8u *contents,
                                   int8u length)
{
  EmberStatus result;

  if (length > EZSP_MAX_FRAME_LENGTH)
    return EMBER_NO_BUFFERS;

  MEMCOPY(zigDevRequestBuffer, contents, length);

  result = sendZigDevRequestBuffer(destination,
                                   clusterId,
                                   options,
                                   length);
  return result;
}

static EmberStatus sendZigDevRequestBuffer(EmberNodeId destination,
                                           int16u clusterId,
                                           EmberApsOption options,
                                           int8u length)
{
  EmberApsFrame apsFrame;
  int8u sequence = emberNextZigDevRequestSequence();
  zigDevRequestBuffer[0] = sequence;

  apsFrame.sourceEndpoint = EMBER_ZDO_ENDPOINT;
  apsFrame.destinationEndpoint = EMBER_ZDO_ENDPOINT;
  apsFrame.clusterId = clusterId;
  apsFrame.profileId = EMBER_ZDO_PROFILE_ID;
  apsFrame.options = options;

  if (destination == EMBER_BROADCAST_ADDRESS
      || destination == EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS
      || destination == EMBER_SLEEPY_BROADCAST_ADDRESS) 
    return ezspSendBroadcast(destination,
                             &apsFrame,
                             emberGetZigDevRequestRadius(),
                             sequence,
                             length,
                             zigDevRequestBuffer,
                             &apsFrame.sequence);
  else
    return ezspSendUnicast(EMBER_OUTGOING_DIRECT,
                           destination,
                           &apsFrame,
                           sequence,
                           length,
                           zigDevRequestBuffer,
                           &apsFrame.sequence);
}

EmberNodeId ezspDecodeAddressResponse(int8u *response,
                                      EmberEUI64 eui64Return)
{
  int8u *payload = response + ZDO_MESSAGE_OVERHEAD;
  if (payload[0] == EMBER_ZDP_SUCCESS) {
    MEMCOPY(eui64Return, payload + 1, EUI64_SIZE);
    return HIGH_LOW_TO_INT(payload[10], payload[9]);
  } else
    return EMBER_NULL_NODE_ID;
}
