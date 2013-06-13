// *******************************************************************
// * identify.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "identify-callback.h"

typedef struct {
  boolean identifying;
  int16u identifyTime;
} EmAfIdentifyState;

static EmAfIdentifyState stateTable[EMBER_AF_IDENTIFY_CLUSTER_SERVER_ENDPOINT_COUNT];

static EmberAfStatus readIdentifyTime(int8u endpoint, int16u *identifyTime);
static EmberAfStatus writeIdentifyTime(int8u endpoint, int16u identifyTime);
static EmberStatus scheduleIdentifyTick(int8u endpoint);

static EmAfIdentifyState *getIdentifyState(int8u endpoint);

static EmAfIdentifyState *getIdentifyState(int8u endpoint)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_IDENTIFY_CLUSTER_ID);
  return (ep == 0xFF ? NULL : &stateTable[ep]);
}

void emberAfIdentifyClusterServerInitCallback(int8u endpoint)
{
  scheduleIdentifyTick(endpoint);
}

void emberAfIdentifyClusterServerTickCallback(int8u endpoint)
{
  int16u identifyTime;
  if (readIdentifyTime(endpoint, &identifyTime) == EMBER_ZCL_STATUS_SUCCESS
      && identifyTime > 0) {
    // This tick writes the new attribute, which will trigger the Attribute
    // Changed callback below, which in turn will schedule or cancel the tick.
    // Because of this, the tick does not have to be scheduled here.
    writeIdentifyTime(endpoint, identifyTime - 1);
  }
}

void emberAfIdentifyClusterServerAttributeChangedCallback(int8u endpoint,
                                                          EmberAfAttributeId attributeId)
{
  if (attributeId == ZCL_IDENTIFY_TIME_ATTRIBUTE_ID) {
    scheduleIdentifyTick(endpoint);
  }
}

boolean emberAfIdentifyClusterIdentifyCallback(int16u time)
{
  // This Identify callback writes the new attribute, which will trigger the
  // Attribute Changed callback above, which in turn will schedule or cancel the
  // tick.  Because of this, the tick does not have to be scheduled here.
  emberAfIdentifyClusterPrintln("RX identify:IDENTIFY 0x%2x", time);
  emberAfSendImmediateDefaultResponse(writeIdentifyTime(emberAfCurrentEndpoint(), time));
  return TRUE;
}

boolean emberAfIdentifyClusterIdentifyQueryCallback(void)
{
  EmberAfStatus status;
  int16u identifyTime;

  emberAfIdentifyClusterPrintln("RX identify:QUERY");

  // According to the 075123r02ZB, a device shall not send an Identify Query
  // Response if it is not currently identifying.  Instead, or if reading the
  // Identify Time attribute fails, send a Default Response.
  status = readIdentifyTime(emberAfCurrentEndpoint(), &identifyTime);
  if (status != EMBER_ZCL_STATUS_SUCCESS || identifyTime == 0) {
    emberAfSendImmediateDefaultResponse(status);
    return TRUE;
  }

  emberAfFillCommandIdentifyClusterIdentifyQueryResponse(identifyTime);
  emberAfSendResponse();
  return TRUE;
}

EmberAfStatus readIdentifyTime(int8u endpoint, 
                               int16u *identifyTime)
{
  EmberAfStatus status = emberAfReadAttribute(endpoint,
                                              ZCL_IDENTIFY_CLUSTER_ID,
                                              ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                              CLUSTER_MASK_SERVER,
                                              (int8u *)identifyTime,
                                              sizeof(*identifyTime),
                                              NULL); // data type
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfIdentifyClusterPrintln("ERR: reading identify time %x", status);
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
  return status;
}

static EmberAfStatus writeIdentifyTime(int8u endpoint, int16u identifyTime)
{
  EmberAfStatus status = emberAfWriteAttribute(endpoint,
                                               ZCL_IDENTIFY_CLUSTER_ID,
                                               ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                               CLUSTER_MASK_SERVER,
                                               (int8u *)&identifyTime,
                                               ZCL_INT16U_ATTRIBUTE_TYPE);
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfIdentifyClusterPrintln("ERR: writing identify time %x", status);
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
  return status;
}

static EmberStatus scheduleIdentifyTick(int8u endpoint)
{
  EmberAfStatus status;
  EmAfIdentifyState *state = getIdentifyState(endpoint);
  int16u identifyTime;

  if(state == NULL) {
    return EMBER_BAD_ARGUMENT;
  }

  status = readIdentifyTime(endpoint, &identifyTime);
  if(status == EMBER_ZCL_STATUS_SUCCESS) {
    if (!state->identifying) {
      state->identifying = TRUE;
      state->identifyTime = identifyTime;
      emberAfPluginIdentifyStartFeedbackCallback(endpoint, 
                                                 identifyTime);
    }
    if (identifyTime > 0) {
      return emberAfScheduleClusterTick(endpoint,
                                        ZCL_IDENTIFY_CLUSTER_ID,
                                        EMBER_AF_SERVER_CLUSTER_TICK,
                                        MILLISECOND_TICKS_PER_SECOND,
                                        EMBER_AF_OK_TO_HIBERNATE);
    }
  }

  state->identifying = FALSE;
  emberAfPluginIdentifyStopFeedbackCallback(endpoint);
  return emberAfDeactivateClusterTick(endpoint,
                                      ZCL_IDENTIFY_CLUSTER_ID,
                                      EMBER_AF_SERVER_CLUSTER_TICK);
}
