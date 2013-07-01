// File: counters-ota.c
//
// Description: A library for retrieving Ember stack counters over the air.
// See counters-ota.h for documentation.
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "app/util/counters/counters.h"
#include "app/util/counters/counters-ota.h"

static EmberStatus sendCluster(EmberNodeId destination, 
                               int16u clusterId,
                               EmberMessageBuffer payload)
{
  EmberApsFrame apsFrame;
  apsFrame.profileId = EMBER_PRIVATE_PROFILE_ID;
  apsFrame.clusterId = clusterId;
  apsFrame.sourceEndpoint = 0;
  apsFrame.destinationEndpoint = 0;
  apsFrame.options = (EMBER_APS_OPTION_RETRY 
                      | EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY);
  return emberSendUnicast(EMBER_OUTGOING_DIRECT,
                          destination,
                          &apsFrame,
                          payload);
}

EmberStatus emberSendCountersRequest(EmberNodeId destination, 
                                     boolean clearCounters)
{
  return sendCluster(destination,
                     (clearCounters 
                      ? EMBER_REPORT_AND_CLEAR_COUNTERS_REQUEST
                      : EMBER_REPORT_COUNTERS_REQUEST),
                     EMBER_NULL_MESSAGE_BUFFER);
}

boolean emberIsIncomingCountersRequest(EmberApsFrame *apsFrame, EmberNodeId sender)
{
  EmberMessageBuffer reply = EMBER_NULL_MESSAGE_BUFFER;
  int8u length = 0;
  int8u i;
  int8u temp[3];

  if (apsFrame->profileId != EMBER_PRIVATE_PROFILE_ID
      || (apsFrame->clusterId != EMBER_REPORT_COUNTERS_REQUEST
          && apsFrame->clusterId != EMBER_REPORT_AND_CLEAR_COUNTERS_REQUEST)) {
    return FALSE;
  }
  
  for (i = 0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
    int16u val = emberCounters[i];
    if (val != 0) {
      temp[0] = i;
      temp[1] = LOW_BYTE(val);
      temp[2] = HIGH_BYTE(val);
      if (reply == EMBER_NULL_MESSAGE_BUFFER) {
        reply = emberAllocateStackBuffer();
        if (reply == EMBER_NULL_MESSAGE_BUFFER) {
          return TRUE;
        }
      }
      if (emberAppendToLinkedBuffers(reply, temp, 3) != EMBER_SUCCESS) {
        emberReleaseMessageBuffer(reply);
        return TRUE;
      }
      length += 3;
    }
    if (length + 3 > MAX_PAYLOAD_LENGTH
        || (length && 
            (i + 1 == EMBER_COUNTER_TYPE_COUNT))) {
      // The response cluster is the request id with the high bit set.
      sendCluster(sender, apsFrame->clusterId | 0x8000, reply);      
      emberReleaseMessageBuffer(reply);
      reply = EMBER_NULL_MESSAGE_BUFFER;
      length = 0;
    }
  }

  return TRUE;
}

boolean emberIsIncomingCountersResponse(EmberApsFrame *apsFrame)
{
  return (apsFrame->profileId == EMBER_PRIVATE_PROFILE_ID
          && (apsFrame->clusterId == EMBER_REPORT_AND_CLEAR_COUNTERS_RESPONSE
              || apsFrame->clusterId == EMBER_REPORT_COUNTERS_RESPONSE));
}

boolean emberIsOutgoingCountersResponse(EmberApsFrame *apsFrame,
                                       EmberStatus status)
{
  boolean isResponse = emberIsIncomingCountersResponse(apsFrame);
  if (isResponse
      && apsFrame->clusterId == EMBER_REPORT_AND_CLEAR_COUNTERS_RESPONSE
      && status == EMBER_SUCCESS) {
    emberClearCounters();
  }
  return isResponse;  
}

