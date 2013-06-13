// *****************************************************************************
// * af-event.c
// *
// * Application event code that is common to both the SOC and EZSP platforms.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros
#include "../include/af.h"
#include "callback.h"
#include "af-event.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "../security/crypto-state.h"
#include "app/framework/util/service-discovery.h"
#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
#include "app/framework/plugin/fragmentation/fragmentation.h"
#endif 

#include "app/framework/plugin/test-harness/test-harness.h"

#include "app/framework/util/attribute-storage.h"

// *****************************************************************************
// Globals

// Task ids used to run events through idling
EmberTaskId emAfTaskId;

#ifdef EMBER_AF_GENERATED_EVENT_CODE
  EMBER_AF_GENERATED_EVENT_CODE
#endif //EMBER_AF_GENERATED_EVENT_CODE

#if defined(EMBER_AF_GENERATED_EVENT_CONTEXT)
int16u emAfAppEventContextLength = EMBER_AF_EVENT_CONTEXT_LENGTH;
EmberAfEventContext emAfAppEventContext[] = {
  EMBER_AF_GENERATED_EVENT_CONTEXT
};
#endif //EMBER_AF_GENERATED_EVENT_CONTEXT

PGM_P emAfEventStrings[] = {
  EM_AF_SERVICE_DISCOVERY_EVENT_STRINGS

#ifdef EMBER_AF_GENERATED_EVENTS
  EMBER_AF_GENERATED_EVENT_STRINGS
#endif

#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
  EMBER_AF_FRAGMENTATION_EVENT_STRINGS
#endif

  EMBER_AF_TEST_HARNESS_EVENT_STRINGS
};

EmberEventData emAfEvents[] = {
  EM_AF_SERVICE_DISCOVERY_EVENTS

#ifdef EMBER_AF_GENERATED_EVENTS
  EMBER_AF_GENERATED_EVENTS
#endif

#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
  EMBER_AF_FRAGMENTATION_EVENTS
#endif

  EMBER_KEY_ESTABLISHMENT_TEST_HARNESS_EVENT

  {NULL, NULL}
};

// *****************************************************************************
// Functions

// A function used to initialize events for idling
void emAfInitEvents(void) 
{
  emberTaskEnableIdling(TRUE);
  emAfTaskId = emberTaskInit(emAfEvents);
}

void emberAfRunEvents(void) 
{
  // Don't run events while crypto operation is in progress
  // (BUGZID: 12127)
  if (emAfIsCryptoOperationInProgress()) {
    // DEBUG Bugzid: 11944
    emberAfCoreFlush();
    return;
  }
  emberRunTask(emAfTaskId);
}

static EmberAfEventContext *findEventContext(int8u endpoint,
                                             int16u clusterId,
                                             boolean isClient) 
{
#if defined(EMBER_AF_GENERATED_EVENT_CONTEXT)
  int8u i;
  for (i = 0; i < emAfAppEventContextLength; i++) {
    EmberAfEventContext *context = &(emAfAppEventContext[i]);
    if (context->endpoint == endpoint 
        && context->clusterId == clusterId
        && context->isClient == isClient) {
      return context;
    }
  }
#endif //EMBER_AF_GENERATED_EVENT_CONTEXT
  return NULL;
}

EmberStatus emberAfEventControlSetDelay(EmberEventControl *eventControl, int32u timeMs)
{
  if (timeMs < MAX_SLEEP_UNITS) {
    emberEventControlSetDelayMS(*eventControl, (int16u)timeMs);
  } else if (timeMs / MILLISECOND_TICKS_PER_QUARTERSECOND < MAX_SLEEP_UNITS) {
    emberEventControlSetDelayQS(*eventControl,
                                (int16u)(timeMs / MILLISECOND_TICKS_PER_QUARTERSECOND));
  } else if (timeMs / MILLISECOND_TICKS_PER_MINUTE < MAX_SLEEP_UNITS) {
    emberEventControlSetDelayMinutes(*eventControl,
                                     (int16u)(timeMs / MILLISECOND_TICKS_PER_MINUTE));
  } else {
    return EMBER_BAD_ARGUMENT;
  }
  return EMBER_SUCCESS;
}

EmberStatus emberAfScheduleClusterTick( int8u endpoint, 
                                        int16u clusterId, 
                                        boolean isClient, 
                                        int32u timeMs, 
                                        EmberAfEventSleepControl sleepControl) 
{
  EmberAfEventContext *context = 
    findEventContext(endpoint, clusterId, isClient);

  // Disabled endpoints cannot schedule events.  This will catch the problem in
  // simulation.
  EMBER_TEST_ASSERT(emberAfEndpointIsEnabled(endpoint));

  if (context != NULL
      && emberAfEndpointIsEnabled(endpoint)
      && (EMBER_SUCCESS
          == emberAfEventControlSetDelay(context->eventControl, timeMs))) {
    context->sleepControl = sleepControl;
    return EMBER_SUCCESS;
  }
  return EMBER_BAD_ARGUMENT;
}

int32u emberAfMsToNextEventExtended(int32u maxMs, int8u* returnIndex)
{
  return emberMsToNextEventExtended(emAfEvents, maxMs, returnIndex);
}

int32u emberAfMsToNextEvent(int32u maxMs)
{
  return emberAfMsToNextEventExtended(maxMs, NULL);
}

EmberStatus emberAfDeactivateClusterTick(int8u endpoint,
                                         int16u clusterId,
                                         boolean isClient) 
{
  EmberAfEventContext *context = 
    findEventContext(endpoint, clusterId, isClient);
  if (context != NULL) {
    emberEventControlSetInactive((*(context->eventControl)));
    return EMBER_SUCCESS;
  }
  return EMBER_BAD_ARGUMENT;
}


// Used to calculate the duration and unit used by the host to set the sleep timer
void emAfGetTimerDurationAndUnitFromMS(int32u ms, int16u *duration, int8u* unit)
{
  if (ms <= MAX_TIMER_UNITS_HOST) {
    *unit = EMBER_EVENT_MS_TIME;
    *duration = ms;
  } else if (ms / MILLISECOND_TICKS_PER_QUARTERSECOND <= MAX_TIMER_UNITS_HOST) {
    *unit = EMBER_EVENT_QS_TIME;
    *duration = ms / MILLISECOND_TICKS_PER_QUARTERSECOND;
  } else {
    *unit = EMBER_EVENT_MINUTE_TIME;
    if (ms / MILLISECOND_TICKS_PER_MINUTE <= MAX_TIMER_UNITS_HOST)
      *duration = ms / MILLISECOND_TICKS_PER_MINUTE;
    else
      *duration = MAX_TIMER_UNITS_HOST;
  }
}

int32u emAfGetMSFromTimerDurationAndUnit(int16u duration, int8u unit)
{
  int32u ms;
  if (unit == EMBER_EVENT_MINUTE_TIME) {
    ms = duration * MILLISECOND_TICKS_PER_MINUTE;
  } else if (unit == EMBER_EVENT_QS_TIME) {
    ms = duration * MILLISECOND_TICKS_PER_QUARTERSECOND;
  } else {
    ms = duration;
  }
  return ms;
}
