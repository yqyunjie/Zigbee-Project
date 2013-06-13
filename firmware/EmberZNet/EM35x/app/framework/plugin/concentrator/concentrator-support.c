// *****************************************************************************
// * concentrator-support.c
// *
// * Code common to SOC and host to handle periodically broadcasting
// * many-to-one route requests (MTORRs).
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-event.h"
#include "concentrator-callback.h"

#include "app/framework/plugin/concentrator/concentrator-support.h"

// *****************************************************************************
// Globals

static int8u routeErrorCount = 0;
static int8u deliveryFailureCount = 0;

#define MIN_QS (EMBER_AF_PLUGIN_CONCENTRATOR_MIN_TIME_BETWEEN_BROADCASTS_SECONDS << 2)
#define MAX_QS (EMBER_AF_PLUGIN_CONCENTRATOR_MAX_TIME_BETWEEN_BROADCASTS_SECONDS << 2)

#if (MIN_QS > MAX_QS)
  #error "Minimum broadcast time must be less than max (EMBER_PLUGIN_CONCENTRATOR_MIN_TIME_BETWEEN_BROADCASTS_SECONDS < EMBER_PLUGIN_CONCENTRATOR_MAX_TIME_BETWEEN_BROADCASTS_SECONDS)"
#endif

// Handy values to make the code more readable.
#define USE_MIN_TIME TRUE
#define USE_MAX_TIME FALSE

EmberEventControl emberAfPluginConcentratorUpdateEventControl;

// Use a shorter name to make the code more readable
#define myEvent emberAfPluginConcentratorUpdateEventControl

#ifndef EMBER_AF_HAS_ROUTER_NETWORK
  #error "Concentrator support only allowed on routers and coordinators."
#endif

// *****************************************************************************
// Functions

static int32u queueRouteDiscovery(boolean useMinTime)
{
  int32u timeLeftQS = (useMinTime ? MIN_QS : MAX_QS); 

  // NOTE:  Since timeToExecute is always in MS we must convert MIN_QS => MIN_MS
  if (useMinTime 
      && (myEvent.timeToExecute > 0)
      && (myEvent.timeToExecute < (MIN_QS * 250))) {

    // Do nothing.  Our queued event will fire shortly.
    // We don't want to reset its time and actually delay
    // when it will fire.
    timeLeftQS = myEvent.timeToExecute >> 2;

  } else {
    emberEventControlSetDelayQS(myEvent,
                                timeLeftQS);
  }
   
  // Tell the caller we have approximately 1 quarter second left
  // even though we actually have less than that.  This lets them plan their
  // for events that are waiting to fire based on the MTORR.
  return (timeLeftQS > 0
          ? timeLeftQS
          : 1);
}

int32u emberAfPluginConcentratorQueueDiscovery(void)
{
  return queueRouteDiscovery(USE_MIN_TIME);
}

void emberAfPluginConcentratorInitCallback(void)
{
#if (!defined(EZSP_HOST) || !defined(EMBER_AF_PLUGIN_CONCENTRATOR_NCP_SUPPORT))
    queueRouteDiscovery(USE_MAX_TIME);
#endif
}

void emberAfPluginConcentratorNcpInitCallback(void)
{
#if defined(EMBER_AF_PLUGIN_CONCENTRATOR_NCP_SUPPORT)
    ezspSetConcentrator(TRUE,
                        EMBER_AF_PLUGIN_CONCENTRATOR_CONCENTRATOR_TYPE,
                        EMBER_AF_PLUGIN_CONCENTRATOR_MIN_TIME_BETWEEN_BROADCASTS_SECONDS,
                        EMBER_AF_PLUGIN_CONCENTRATOR_MAX_TIME_BETWEEN_BROADCASTS_SECONDS,
                        EMBER_AF_PLUGIN_CONCENTRATOR_ROUTE_ERROR_THRESHOLD,
                        EMBER_AF_PLUGIN_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD,
                        EMBER_AF_PLUGIN_CONCENTRATOR_MAX_HOPS);
#endif
}

void emberAfPluginConcentratorStopDiscovery(void)
{
  emberEventControlSetInactive(myEvent);
  myEvent.timeToExecute = 0;
  emberAfCorePrintln("Concentrator advertisements stopped."); 
}

static void routeErrorCallback(EmberStatus status)
{
  if (status == EMBER_SOURCE_ROUTE_FAILURE 
      || status == EMBER_MANY_TO_ONE_ROUTE_FAILURE) {
    routeErrorCount += 1;
    if (routeErrorCount >= EMBER_AF_PLUGIN_CONCENTRATOR_ROUTE_ERROR_THRESHOLD) {
      emberAfPluginConcentratorQueueDiscovery();
    }
  }
}

void emberAfDeliveryStatusCallback(EmberOutgoingMessageType type,
                                  EmberStatus status)
{
  if ((type == EMBER_OUTGOING_DIRECT
       || type == EMBER_OUTGOING_VIA_ADDRESS_TABLE
       || type == EMBER_OUTGOING_VIA_BINDING)
      && status == EMBER_DELIVERY_FAILED) {
    deliveryFailureCount += 1;
    if (deliveryFailureCount >= EMBER_AF_PLUGIN_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD) {
      emberAfPluginConcentratorQueueDiscovery();
    }
  }
}

void emberAfPluginConcentratorUpdateEventHandler(void)
{
  routeErrorCount = 0;
  deliveryFailureCount = 0;
  if (EMBER_SUCCESS
      == emberSendManyToOneRouteRequest(EMBER_AF_PLUGIN_CONCENTRATOR_CONCENTRATOR_TYPE, 
                                        EMBER_AF_PLUGIN_CONCENTRATOR_MAX_HOPS)) {
    emberAfDebugPrintln("send MTORR");
    emberAfPluginConcentratorBroadcastSentCallback();
  }
  queueRouteDiscovery(USE_MAX_TIME);
}

void emberIncomingRouteErrorHandler(EmberStatus status, EmberNodeId target)
{
  routeErrorCallback(status);
}

void ezspIncomingRouteErrorHandler(EmberStatus status, EmberNodeId target)
{
  routeErrorCallback(status);
}


//------------------------------------------------------------------------------
// These functions are defined for legacy support for the software using the old
// app/util/concentrator/ names.
// I would really like to #define the old function names to the new ones,
// but creating a real function with the same name will cause a duplicate symbol 
// error to occur if both this plugin and the old code are included.  Users can then
// remove the app/util/concentrator code from their project to prevent confusion
// and redundancy.
int32u emberConcentratorQueueRouteDiscovery(void)
{
  return emberAfPluginConcentratorQueueDiscovery();
}

void emberConcentratorStartDiscovery(void)
{
  emberAfPluginConcentratorInitCallback();  
}

void emberConcentratorStopDiscovery(void)
{
  emberAfPluginConcentratorStopDiscovery();  
}

//------------------------------------------------------------------------------
