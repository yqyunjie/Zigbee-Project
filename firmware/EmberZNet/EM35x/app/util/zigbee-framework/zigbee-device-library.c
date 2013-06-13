// File: zigbee-device-library.c
//
// Description: ZigBee Device Object (ZDO) functions not provided by the stack.
//
//
// Copyright 2007 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "zigbee-device-common.h"
#include "zigbee-device-library.h"

static EmberStatus sendZigDevRequestBuffer(EmberNodeId destination,
                                           int16u clusterId,
                                           EmberApsOption options,
                                           EmberMessageBuffer request);

EmberStatus emberMatchDescriptorsRequest(EmberNodeId target,
                                         int16u profile,
                                         EmberMessageBuffer inClusters,
                                         EmberMessageBuffer outClusters,
                                         EmberApsOption options)
{
  int8u inCount = (inClusters == EMBER_NULL_MESSAGE_BUFFER
                   ? 0
                   : emberMessageBufferLength(inClusters) 
                   / 2); // Clusters are 2 bytes long
  int8u outCount = (outClusters == EMBER_NULL_MESSAGE_BUFFER
                    ? 0
                    : emberMessageBufferLength(outClusters)
                    / 2); // Clusters are 2 bytes long
  EmberMessageBuffer request;
  int8u offset = ZDO_MESSAGE_OVERHEAD + 5;
  int8u contents[ZDO_MESSAGE_OVERHEAD + 5];  // Add 2 bytes for NWK Address 
                                             // Add 2 bytes for Profile Id
                                             // Add 1 byte for in Cluster Count

  EmberStatus result;
  int8u i;
  
  contents[0] = emberNextZigDevRequestSequence();
  contents[1] = LOW_BYTE(target);
  contents[2] = HIGH_BYTE(target);
  contents[3] = LOW_BYTE(profile);
  contents[4] = HIGH_BYTE(profile);
  contents[5] = inCount;

  request = emberFillLinkedBuffers(contents, offset);
  if (request == EMBER_NULL_MESSAGE_BUFFER)
    return EMBER_NO_BUFFERS;

  if (emberSetLinkedBuffersLength(request,
                                  offset + 
                                  (inCount * 2) + // Times 2 for 2 byte Clusters
                                  1 +             // Out Cluster Count
                                  (outCount * 2)) // Times 2 for 2 byte Clusters
      != EMBER_SUCCESS)
    return EMBER_NO_BUFFERS;
  for (i = 0; i < inCount; i++) {
    emberSetLinkedBuffersByte(request,
                              (i * 2) + offset,
                              emberGetLinkedBuffersByte(inClusters, (i * 2)));
    emberSetLinkedBuffersByte(request,
                              (i * 2) + offset + 1,
                              emberGetLinkedBuffersByte(inClusters, 
                                                        (i * 2) + 1));
  }
  offset += (inCount * 2);
  emberSetLinkedBuffersByte(request, offset, outCount);
  offset++;
  for (i = 0; i < outCount; i++) {
    emberSetLinkedBuffersByte(request,
                              (i * 2) + offset,
                              emberGetLinkedBuffersByte(outClusters, (i * 2)));
    emberSetLinkedBuffersByte(request,
                              (i * 2) + offset + 1,
                              emberGetLinkedBuffersByte(outClusters,
                                                        (i * 2) + 1));
  }

  result = sendZigDevRequestBuffer(target,
                                   MATCH_DESCRIPTORS_REQUEST,
                                   options,
                                   request);
  emberReleaseMessageBuffer(request);
  return result;
}


EmberStatus emberEndDeviceBindRequest(int8u endpoint,
                                      EmberApsOption options)
{
  EmberMessageBuffer request;
  EmberStatus result;
  int8u contents[ZDO_MESSAGE_OVERHEAD + 14];
  EmberEndpointDescription descriptor;
  int8u i;
  
  if (!emberGetEndpointDescription(endpoint, &descriptor)) {
    return EMBER_INVALID_ENDPOINT;
  }
  
  contents[0]  = emberNextZigDevRequestSequence();
  contents[1]  = LOW_BYTE(emberGetNodeId());
  contents[2]  = HIGH_BYTE(emberGetNodeId());
  MEMCOPY(contents + 3, emberGetEui64(), 8);
  contents[11] = endpoint;
  contents[12] = LOW_BYTE(descriptor.profileId);
  contents[13] = HIGH_BYTE(descriptor.profileId);
  contents[14] = descriptor.inputClusterCount;

  request = emberFillLinkedBuffers(contents, sizeof(contents));
  if (request == EMBER_NULL_MESSAGE_BUFFER)
    return EMBER_NO_BUFFERS;
  for (i = 0; i < descriptor.inputClusterCount; i++) {
    int16u clusterId = emberGetEndpointCluster(endpoint,
                                               EMBER_INPUT_CLUSTER_LIST,
                                               i);
    int8u temp = LOW_BYTE(clusterId);
    emberAppendToLinkedBuffers(request,
                               &temp,
                               1);
    temp = HIGH_BYTE(clusterId);
    emberAppendToLinkedBuffers(request,
                               &temp,
                               1);
  }
  emberAppendToLinkedBuffers(request,
                             &descriptor.outputClusterCount,
                             1);
  for (i = 0; i < descriptor.outputClusterCount; i++) {
    int16u clusterId = emberGetEndpointCluster(endpoint,
                                               EMBER_OUTPUT_CLUSTER_LIST,
                                               i);
    int8u temp = LOW_BYTE(clusterId);
    emberAppendToLinkedBuffers(request,
                               &temp,
                               1);
    temp = HIGH_BYTE(clusterId);
    emberAppendToLinkedBuffers(request,
                               &temp,
                               1);
  }
  result = sendZigDevRequestBuffer(EMBER_ZIGBEE_COORDINATOR_ADDRESS,
                                   END_DEVICE_BIND_REQUEST,
                                   options,
                                   request);

  if (request != EMBER_NULL_MESSAGE_BUFFER)
    emberReleaseMessageBuffer(request);
  return result;
}

EmberStatus emberSendZigDevRequest(EmberNodeId destination,
                                   int16u clusterId,
                                   EmberApsOption options,
                                   int8u *contents,
                                   int8u length)
{
  EmberMessageBuffer message;
  EmberStatus result;

  contents[0] = emberNextZigDevRequestSequence();

  message = emberFillLinkedBuffers(contents, length);

  if (message == EMBER_NULL_MESSAGE_BUFFER)
    return EMBER_NO_BUFFERS;
  else {
    result = sendZigDevRequestBuffer(destination,
                                     clusterId,
                                     options,
                                     message);
    emberReleaseMessageBuffer(message);
    return result;
  }
}

static EmberStatus sendZigDevRequestBuffer(EmberNodeId destination,
                                           int16u clusterId,
                                           EmberApsOption options,
                                           EmberMessageBuffer request)
{
  EmberApsFrame apsFrame;

  apsFrame.sourceEndpoint = EMBER_ZDO_ENDPOINT;
  apsFrame.destinationEndpoint = EMBER_ZDO_ENDPOINT;
  apsFrame.clusterId = clusterId;
  apsFrame.profileId = EMBER_ZDO_PROFILE_ID;
  apsFrame.options = options;

  if (destination == EMBER_BROADCAST_ADDRESS
      || destination == EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS
      || destination == EMBER_SLEEPY_BROADCAST_ADDRESS) 
    return emberSendBroadcast(destination, 
		              &apsFrame, 
			      emberGetZigDevRequestRadius(), 
			      request);
  else
    return emberSendUnicast(EMBER_OUTGOING_DIRECT,
                            destination,
                            &apsFrame,
                            request);
}

EmberNodeId emberDecodeAddressResponse(EmberMessageBuffer response,
                                       EmberEUI64 eui64Return)
{
  int8u contents[11];
  if (emberMessageBufferLength(response) < ZDO_MESSAGE_OVERHEAD + 11)
    return EMBER_NULL_NODE_ID;
  emberCopyFromLinkedBuffers(response, ZDO_MESSAGE_OVERHEAD, contents, 11);
  if (contents[0] == EMBER_ZDP_SUCCESS) {
    MEMCOPY(eui64Return, contents + 1, EUI64_SIZE);
    return HIGH_LOW_TO_INT(contents[10], contents[9]);
  } else
    return EMBER_NULL_NODE_ID;
}
