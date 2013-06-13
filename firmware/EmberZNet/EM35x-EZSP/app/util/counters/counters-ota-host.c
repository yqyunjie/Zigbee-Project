// File: counters-ota-host.c
//
// Description: An EZSP host application library for retrieving Ember stack
// counters over the air.  See counters-ota.h for documentation.
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/counters/counters-ota.h"

static EmberStatus sendCluster(EmberNodeId destination, 
                               int16u clusterId,
                               int8u length,
                               int8u *contents)
{
  EmberApsFrame apsFrame;
  int8u sequence;
  apsFrame.profileId = EMBER_PRIVATE_PROFILE_ID;
  apsFrame.clusterId = clusterId;
  apsFrame.sourceEndpoint = 0;
  apsFrame.destinationEndpoint = 0;
  apsFrame.options = (EMBER_APS_OPTION_RETRY 
                      | EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY);
  return ezspSendUnicast(EMBER_OUTGOING_DIRECT,
                         destination,
                         &apsFrame,
                         0,  // message tag
                         length,  // mesage length
                         contents,
                         &sequence);
}

EmberStatus emberSendCountersRequest(EmberNodeId destination, 
                                     boolean clearCounters)
{
  return sendCluster(destination,
                     (clearCounters 
                      ? EMBER_REPORT_AND_CLEAR_COUNTERS_REQUEST
                      : EMBER_REPORT_COUNTERS_REQUEST),
                     0,
                     NULL);
}

boolean emberIsIncomingCountersRequest(EmberApsFrame *apsFrame, EmberNodeId sender)
{
  int8u i;
  int8u length = 0;
  int16u counters[EMBER_COUNTER_TYPE_COUNT];
  int8u payload[MAX_PAYLOAD_LENGTH];

  if (apsFrame->profileId != EMBER_PRIVATE_PROFILE_ID
      || (apsFrame->clusterId != EMBER_REPORT_COUNTERS_REQUEST
          && apsFrame->clusterId != EMBER_REPORT_AND_CLEAR_COUNTERS_REQUEST)) {
    return FALSE;
  }
  
  ezspReadAndClearCounters(counters);

  for (i = 0; i < EMBER_COUNTER_TYPE_COUNT; i++) {
    int16u val = counters[i];
    if (val != 0) {
      payload[length] = i;
      payload[length + 1] = LOW_BYTE(val);
      payload[length + 2] = HIGH_BYTE(val);
      length += 3;
    }
    if (length + 3 > MAX_PAYLOAD_LENGTH
        || (length 
            && (i + 1 == EMBER_COUNTER_TYPE_COUNT))) {
      // The response cluster is the request id with the high bit set.
      sendCluster(sender, apsFrame->clusterId | 0x8000, length, payload);      
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
  return emberIsIncomingCountersResponse(apsFrame);
}
