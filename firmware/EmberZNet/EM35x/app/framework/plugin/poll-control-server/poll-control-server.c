// *******************************************************************
// * poll-control-server.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"

typedef struct {
  boolean inUse;
  EmberEUI64 bindingId;
  boolean fastPolling;
  int16u fastPollingTimeout;
} EmberAfPollControlState;

static EmberAfPollControlState stateTable[EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS];
static boolean fastPolling = FALSE;
static boolean fastPollWait = FALSE;
static int16u maxFastPollingTimeout = 0;
static int16u postFastPollTimeRemaining = 0;

static EmberAfPollControlState *stateFromBindingIndex(int8u index)
{
  int8u i;
  EmberEUI64 longId;
  EmberBindingTableEntry entry;

  EmberStatus status; 
  if (index == EMBER_NULL_BINDING) {
    return NULL;
  }

  status = emberGetBinding(index, &entry);
  if (status != EMBER_SUCCESS) {
    return NULL;
  }

  MEMCOPY(longId, entry.identifier, EUI64_SIZE);
  for (i = 0; i < EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS; i++) {
    if (stateTable[i].inUse && 
        0 == MEMCOMPARE(longId, stateTable[i].bindingId, EUI64_SIZE)) {
      return &stateTable[i];
    }
  }

  return NULL;
}

static boolean mustStopFastPolling(void)
{
  boolean fastPoll = FALSE;
  int i;

  for (i = 0; i < EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS; i++) {
    fastPoll = fastPoll || stateTable[i].fastPolling;
  }

  return fastPoll ? FALSE : TRUE;
}

static EmberAfPollControlState *allocateState(int8u index)
{
  int8u i;
  EmberBindingTableEntry entry;

  EmberStatus status;
  if (index == EMBER_NULL_BINDING) {
    return NULL;
  }

  status = emberGetBinding(index, &entry);
  if (status != EMBER_SUCCESS) {
    return NULL;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS; i++) {
    if (!stateTable[i].inUse) {
      MEMCOPY(stateTable[i].bindingId, entry.identifier, EUI64_SIZE);
      stateTable[i].inUse = TRUE;
      return &stateTable[i];
    }
  }

  return NULL;
}

static void stateTableInit(void)
{
  int8u i;

  for (i = 0; i < EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS; i++) {
    stateTable[i].inUse = FALSE;
    stateTable[i].fastPolling = FALSE;
    stateTable[i].fastPollingTimeout = 0;
  }
}
  
static EmberAfStatus readServerAttribute(int8u endpoint,
                                         EmberAfAttributeId attributeId,
                                         PGM_P name,
                                         int8u *data,
                                         int8u size)
{
  EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                                    ZCL_POLL_CONTROL_CLUSTER_ID,
                                                    attributeId,
                                                    data,
                                                    size);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPollControlClusterPrintln("ERR: %ping %p 0x%x", "read", name, status);
  }
  return status;
}

static EmberAfStatus writeServerAttribute(int8u endpoint,
                                          EmberAfAttributeId attributeId,
                                          PGM_P name,
                                          int8u *data,
                                          EmberAfAttributeType type)
{
  EmberAfStatus status = emberAfWriteServerAttribute(endpoint,
                                                     ZCL_POLL_CONTROL_CLUSTER_ID,
                                                     attributeId,
                                                     data,
                                                     type);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPollControlClusterPrintln("ERR: %ping %p 0x%x", "writ", name, status);
  }
  return status;
}

static void establishFastPollParameters(int8u endpoint) 
{
  EmberAfStatus status;
  int8u i;
  int32u checkInInterval;

  // None of the clients have requested fast polling
  if (mustStopFastPolling()) {
    fastPolling = FALSE;
    return;
  }

  status = readServerAttribute(endpoint,
                               ZCL_CHECK_IN_INTERVAL_ATTRIBUTE_ID,
                               "check in interval",
                               (int8u *)&checkInInterval,
                               sizeof(checkInInterval));

  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return;
  }

  status = readServerAttribute(endpoint,
                               ZCL_FAST_POLL_TIMEOUT_ATTRIBUTE_ID,
                               "fast poll timeout",
                               (int8u *)&maxFastPollingTimeout,
                               sizeof(maxFastPollingTimeout));

  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return;
  }

  fastPolling = TRUE;

  // Need to calculate the maximum fast poll timeout among reported clients
  for (i = 0; i < EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_MAX_CLIENTS; i++) {
    if (stateTable[i].fastPolling && 
        stateTable[i].fastPollingTimeout > maxFastPollingTimeout) {
      maxFastPollingTimeout = stateTable[i].fastPollingTimeout;
    }
  }

  // Need to see if there's a gap between the end of the time requested for fast 
  // polling and the next check in.
  if (maxFastPollingTimeout >= 
      checkInInterval - EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_CHECK_IN_RESPONSE_TIMEOUT) {
    maxFastPollingTimeout = 
      checkInInterval - EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_CHECK_IN_RESPONSE_TIMEOUT;
  } else {
    postFastPollTimeRemaining = 
      checkInInterval - 
      (maxFastPollingTimeout + EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_CHECK_IN_RESPONSE_TIMEOUT);
  }
}

static EmberAfStatus scheduleCheckInTick(int8u endpoint)
{
  // The device is expected to check in after the amount of time indicated in
  // the check in interval attribute.  If the attribute cannot be read, we
  // abort without modifying the event.  If the attribute is set to zero, it
  // means that the poll control server is turned off and will not check in
  // with the client.
  int32u checkInInterval;
  EmberAfStatus status = readServerAttribute(endpoint,
                                             ZCL_CHECK_IN_INTERVAL_ATTRIBUTE_ID,
                                             "check in interval",
                                             (int8u *)&checkInInterval,
                                             sizeof(checkInInterval));
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return status;
  } else if (checkInInterval == 0) {
    emberAfDeactivateClusterTick(endpoint,
                                 ZCL_POLL_CONTROL_CLUSTER_ID,
                                 EMBER_AF_SERVER_CLUSTER_TICK);
  } else {
    emberAfScheduleClusterTick(endpoint,
                               ZCL_POLL_CONTROL_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               (checkInInterval
                                * MILLISECOND_TICKS_PER_QUARTERSECOND),
                               EMBER_AF_OK_TO_HIBERNATE);
  }
  fastPolling = FALSE;
  return EMBER_ZCL_STATUS_SUCCESS;
}

static EmberAfStatus scheduleFastPollTick(int8u endpoint,
                                          int16u fastPollTimeout)
{
  // If the fast poll timeout is zero, the device is expected to continue fast
  // polling until the amount of time indicated in the fast poll timeout
  // attribute.
  if (fastPollTimeout == 0) {
    EmberAfStatus status = readServerAttribute(endpoint,
                                               ZCL_FAST_POLL_TIMEOUT_ATTRIBUTE_ID,
                                               "fast poll timeout",
                                               (int8u *)&fastPollTimeout,
                                               sizeof(fastPollTimeout));
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      return status;
    }
  }

  emberAfScheduleClusterTick(endpoint,
                             ZCL_POLL_CONTROL_CLUSTER_ID,
                             EMBER_AF_SERVER_CLUSTER_TICK,
                             (fastPollTimeout
                              * MILLISECOND_TICKS_PER_QUARTERSECOND),
                             EMBER_AF_OK_TO_NAP);
  fastPolling = TRUE;
  return EMBER_ZCL_STATUS_SUCCESS;
}

void emberAfPollControlClusterServerInitCallback(int8u endpoint)
{
  int32u longPollInterval = emberAfGetLongPollIntervalQsCallback();
  int16u shortPollInterval = emberAfGetShortPollIntervalQsCallback();
  stateTableInit();
  scheduleCheckInTick(endpoint);
  writeServerAttribute(endpoint,
                       ZCL_LONG_POLL_INTERVAL_ATTRIBUTE_ID,
                       "long poll interval",
                       (int8u *)&longPollInterval,
                       ZCL_INT32U_ATTRIBUTE_TYPE);
  writeServerAttribute(endpoint,
                       ZCL_SHORT_POLL_INTERVAL_ATTRIBUTE_ID,
                       "short poll interval",
                       (int8u *)&shortPollInterval,
                       ZCL_INT16U_ATTRIBUTE_TYPE);
}

void emberAfPollControlClusterServerTickCallback(int8u endpoint)
{
  int8u i;

  // If we've not yet sent out the parallel check-in requests (i.e.
  // if we're not in temporary fast poll mode waiting for responses), 
  // we must do that.
  if (!fastPollWait && postFastPollTimeRemaining == 0) {
    for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
      EmberBindingTableEntry binding;
      EmberStatus status = emberGetBinding(i, &binding);
      if (status == EMBER_SUCCESS
          && binding.type == EMBER_UNICAST_BINDING
          && binding.local == endpoint
          && binding.clusterId == ZCL_POLL_CONTROL_CLUSTER_ID) {
        EmberAfPollControlState *state = stateFromBindingIndex(i);
        if (state == NULL) {
          state = allocateState(i);
          if (state == NULL) {
            continue;
          }
        }
        emberAfFillCommandPollControlClusterCheckIn();
        status = emberAfSendCommandUnicast(EMBER_OUTGOING_VIA_BINDING, i);
        if (status == EMBER_SUCCESS) {
          // Reset the fast polllmode of the state
          state->fastPolling = FALSE;
        }
      }
    }
    fastPollWait = TRUE;
    scheduleFastPollTick(endpoint, 
                         EMBER_AF_PLUGIN_POLL_CONTROL_SERVER_CHECK_IN_RESPONSE_TIMEOUT);
  } else {
    if (postFastPollTimeRemaining == 0) {
      // Do we need to fast poll, and if so, for how long?
      establishFastPollParameters(endpoint);
      fastPollWait = FALSE;
    } else {
      fastPolling = FALSE;
      emberAfScheduleClusterTick(endpoint,
                                 ZCL_POLL_CONTROL_CLUSTER_ID,
                                 EMBER_AF_SERVER_CLUSTER_TICK,
                                 (postFastPollTimeRemaining
                                  * MILLISECOND_TICKS_PER_QUARTERSECOND),
                                 EMBER_AF_OK_TO_HIBERNATE);
      postFastPollTimeRemaining = 0;
      return;
    }
    if (fastPolling) {
      scheduleFastPollTick(endpoint, maxFastPollingTimeout);
    } else {
      scheduleCheckInTick(endpoint);
    }
  }
}

void emberAfPollControlClusterServerAttributeChangedCallback(int8u endpoint,
                                                             EmberAfAttributeId attributeId)
{
  if (attributeId == ZCL_CHECK_IN_INTERVAL_ATTRIBUTE_ID && !fastPolling) {
    scheduleCheckInTick(endpoint);
  }
}

boolean emberAfPollControlClusterCheckInResponseCallback(int8u startFastPolling,
                                                         int16u fastPollTimeout)
{
  EmberAfStatus status;
  int8u bindingIndex = emberAfGetBindingIndex();
  EmberAfPollControlState *state;
  emberAfPollControlClusterPrintln("RX: CheckInResponse 0x%x, 0x%2x",
                                   startFastPolling,
                                   fastPollTimeout);

  // To account for multiple clients, we must log the "vote" of the responding client.
  // When all clients have responded, we will begin fast polling (if applicable) for the 
  // maximum fast polling timeout duration.
  state = stateFromBindingIndex(bindingIndex);
  if (state != NULL) {
    state->fastPollingTimeout = fastPollTimeout;
    state->fastPolling = startFastPolling;
    status = EMBER_ZCL_STATUS_SUCCESS;
  } else {
    status = EMBER_ZCL_STATUS_FAILURE;
  }

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfPollControlClusterFastPollStopCallback(void)
{
  EmberAfStatus status;
  int8u bindingIndex;
  EmberAfPollControlState *state;
  emberAfPollControlClusterPrintln("RX: FastPollStop");

  bindingIndex = emberAfGetBindingIndex();
  state = stateFromBindingIndex(bindingIndex);
  if (state != NULL) {
    state->fastPolling = FALSE;
    if (mustStopFastPolling()) {
      status = scheduleCheckInTick(emberAfCurrentEndpoint());
      postFastPollTimeRemaining = 0;
    } else {
      status = EMBER_ZCL_STATUS_WAIT_FOR_DATA;
    }
  } else {
    status = EMBER_ZCL_STATUS_FAILURE;
  }

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfPollControlClusterSetLongPollIntervalCallback(int32u newLongPollInterval)
{
  EmberAfStatus status;
  int8u endpoint = emberAfCurrentEndpoint();
  int32u longPollInterval = newLongPollInterval;

#ifdef ZCL_USING_POLL_CONTROL_CLUSTER_LONG_POLL_INTERVAL_MIN_ATTRIBUTE
  int32u longPollIntervalMin;
  status = readServerAttribute(endpoint,
                               ZCL_LONG_POLL_INTERVAL_ATTRIBUTE_ID,
                               "long poll interval min",
                               (int8u *)&longPollIntervalMin,
                               sizeof(checkInInterval));
  if (status != EMBER_SUCCESS) {
    goto write_interval;
  }

  if (longPollInterval < longPollIntervalMin) {
    longPollInterval = longPollIntervalMin;
  }

write_interval:
#endif // ZCL_USING_POLL_CONTROL_CLUSTER_LONG_POLL_INTERVAL_MIN_ATTRIBUTE

  emberAfSetLongPollIntervalQsCallback(longPollInterval);
  status = writeServerAttribute(endpoint,
                                ZCL_LONG_POLL_INTERVAL_ATTRIBUTE_ID,
                                "long poll interval",
                                (int8u *)&longPollInterval,
                                ZCL_INT32U_ATTRIBUTE_TYPE);

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfPollControlClusterSetShortPollIntervalCallback(int16u newShortPollInterval)
{
  EmberAfStatus status;
  int8u endpoint = emberAfCurrentEndpoint();

  emberAfSetShortPollIntervalQsCallback(newShortPollInterval);
  status = writeServerAttribute(endpoint,
                                ZCL_SHORT_POLL_INTERVAL_ATTRIBUTE_ID,
                                "short poll interval",
                                (int8u *)&newShortPollInterval,
                                ZCL_INT16U_ATTRIBUTE_TYPE);

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}
