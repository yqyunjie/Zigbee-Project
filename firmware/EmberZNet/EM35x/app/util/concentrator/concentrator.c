// File: concentrator.c
//
// The concentrator library is deprecated and will be removed in a future
// release.  Similar functionality is available in the Concentrator Support
// plugin in Application Framework.
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"              // for halCommonGetInt16uQuarterSecondTick()
#include "concentrator.h"

// This function has the same signature for host applications (in ezsp.h)
// as for node applications (in ember.h).  We use an extern to make the
// library portable between host and node.
extern EmberStatus emberSendManyToOneRouteRequest(int16u concentratorType,
                                                  int8u radius);

static int8u routeErrorCount;
static int8u deliveryFailureCount;

#define MIN_QS (EMBER_CONCENTRATOR_MIN_SECONDS_BETWEEN_DISCOVERIES << 2)
#define MAX_QS (EMBER_CONCENTRATOR_MAX_SECONDS_BETWEEN_DISCOVERIES << 2)

// Event control stuff.
static boolean eventActive = TRUE;
static int16u timeToExecuteQS = 0;
static int16u lastExecutionQS;

#define elapsedTimeInt16u(oldTime, newTime)      \
  ((int16u) ((int16u)(newTime) - (int16u)(oldTime)))

static int16u elapsedQS(void)
{
  int16u now = halCommonGetInt16uQuarterSecondTick();
  return elapsedTimeInt16u(lastExecutionQS, now);
}


//---------------------------------------------------------------------------
// Public functions

void emberConcentratorStartDiscovery(void)
{
  eventActive = TRUE;
}

void emberConcentratorStopDiscovery(void)
{
  eventActive = FALSE;
}

int16u emberConcentratorQueueRouteDiscovery(void)
{
  timeToExecuteQS = MIN_QS;
  return timeToExecuteQS;
}

void emberConcentratorNoteRouteError(EmberStatus status)
{
  if (status == EMBER_SOURCE_ROUTE_FAILURE 
      || status == EMBER_MANY_TO_ONE_ROUTE_FAILURE) {
    routeErrorCount += 1;
    if (routeErrorCount >= EMBER_CONCENTRATOR_ROUTE_ERROR_THRESHOLD) {
      emberConcentratorQueueRouteDiscovery();
    }
  }
}

void emberConcentratorNoteDeliveryFailure(EmberOutgoingMessageType type,
                                          EmberStatus status)
{
  if ((type == EMBER_OUTGOING_DIRECT
       || type == EMBER_OUTGOING_VIA_ADDRESS_TABLE
       || type == EMBER_OUTGOING_VIA_BINDING)
      && status == EMBER_DELIVERY_FAILED) {
    deliveryFailureCount += 1;
    if (deliveryFailureCount >= EMBER_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD) {
      emberConcentratorQueueRouteDiscovery();
    }
  }
}

void emberConcentratorTick(void)
{
  if (eventActive) {
    int16u elapsed = elapsedQS();
    if (elapsed >= timeToExecuteQS) {
      routeErrorCount = 0;
      deliveryFailureCount = 0;
      lastExecutionQS = halCommonGetInt16uQuarterSecondTick();
      if(emberSendManyToOneRouteRequest(
        EMBER_CONCENTRATOR_TYPE, EMBER_CONCENTRATOR_RADIUS) == EMBER_SUCCESS) {
        emberConcentratorRouteDiscoverySentHandler();
      }
      timeToExecuteQS = MAX_QS;
    } 
  }
}
