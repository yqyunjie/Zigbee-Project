// *****************************************************************************
// * end-device-support.c
// *
// * Code common to SOC and host to handle managing sleeping and polling
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/util/util.h"
#include "app/framework/util/common.h"
#include "app/framework/util/af-event.h"

#include "app/framework/plugin/end-device-support/end-device-support.h"

#if defined(EMBER_SCRIPTED_TEST)
  #include "app/framework/util/af-event-test.h"

  #define EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_LONG_POLL_INTERVAL_SECONDS 15
  #define EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_SHORT_POLL_INTERVAL_SECONDS 1
#endif

// *****************************************************************************
// Globals

typedef struct {
  EmberAfApplicationTask currentAppTasks;
  EmberAfApplicationTask wakeTimeoutBitmask;
  int32u longPollIntervalQs;
  int16u shortPollIntervalQs;
  int16u wakeTimeoutQs;
  int16u lastAppTaskScheduleTime;
  EmberAfEventSleepControl defaultSleepControl;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

#if defined(EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_STAY_AWAKE_WHEN_NOT_JOINED)
  #define STAY_AWAKE_WHEN_NOT_JOINED_DEFAULT TRUE
#else
  #define STAY_AWAKE_WHEN_NOT_JOINED_DEFAULT FALSE
#endif
boolean emAfStayAwakeWhenNotJoined = STAY_AWAKE_WHEN_NOT_JOINED_DEFAULT;

boolean emAfForceEndDeviceToStayAwake = FALSE;

#ifndef EMBER_AF_HAS_END_DEVICE_NETWORK
  #error "End device support only allowed on end devices."
#endif

// *****************************************************************************
// Functions

void emberAfPluginEndDeviceSupportInitCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    states[i].longPollIntervalQs = (EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_LONG_POLL_INTERVAL_SECONDS
                                    * 4);
    states[i].shortPollIntervalQs = (EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_SHORT_POLL_INTERVAL_SECONDS
                                     * 4);
    states[i].wakeTimeoutQs = (EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_WAKE_TIMEOUT_SECONDS
                               * 4);
    states[i].wakeTimeoutBitmask = EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_WAKE_TIMEOUT_BITMASK;
  }
}

void emberAfAddToCurrentAppTasksCallback(EmberAfApplicationTask tasks)
{
  if (EMBER_SLEEPY_END_DEVICE <= emAfCurrentNetwork->nodeType) {
    State *state = &states[emberGetCurrentNetwork()];
    state->currentAppTasks |= tasks;
    if (tasks & state->wakeTimeoutBitmask) {
      state->lastAppTaskScheduleTime = halCommonGetInt16uMillisecondTick();
    }
  }
}

void emberAfRemoveFromCurrentAppTasksCallback(EmberAfApplicationTask tasks)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->currentAppTasks &= (~tasks);
}

int32u emberAfGetCurrentAppTasksCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->currentAppTasks;
}

// NO PRINTFS.  This may be called in ISR context.
void emberAfForceEndDeviceToStayAwake(boolean stayAwake)
{
  emAfForceEndDeviceToStayAwake = stayAwake;
}

int32u emberAfGetLongPollIntervalQsCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->longPollIntervalQs;
}

void emberAfSetLongPollIntervalQsCallback(int32u longPollIntervalQs)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->longPollIntervalQs = longPollIntervalQs;
}

int16u emberAfGetShortPollIntervalQsCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->shortPollIntervalQs;
}

void emberAfSetShortPollIntervalQsCallback(int16u shortPollIntervalQs)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->shortPollIntervalQs = shortPollIntervalQs;
}

#ifdef EZSP_HOST
  #define EMBER_OK_TO_LONG_POLL() TRUE
#else
  #define EMBER_OK_TO_LONG_POLL() emberOkToLongPoll()
#endif

int32u emberAfGetCurrentPollIntervalQsCallback(void)
{
  if (EMBER_SLEEPY_END_DEVICE <= emAfCurrentNetwork->nodeType) {
    State *state = &states[emberGetCurrentNetwork()];
    if (elapsedTimeInt16u(state->lastAppTaskScheduleTime,
                          halCommonGetInt16uMillisecondTick())
        > state->wakeTimeoutQs * MILLISECOND_TICKS_PER_QUARTERSECOND) {
      state->currentAppTasks &= ~state->wakeTimeoutBitmask;
    }
    if (!EMBER_OK_TO_LONG_POLL()
        || state->currentAppTasks != 0
        || emberAfGetCurrentSleepControlCallback() != EMBER_AF_OK_TO_HIBERNATE) {
      return emberAfGetShortPollIntervalQsCallback();
    }
  }
  return emberAfGetLongPollIntervalQsCallback();
}

int16u emberAfGetWakeTimeoutQsCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->wakeTimeoutQs;
}

void emberAfSetWakeTimeoutQsCallback(int16u wakeTimeoutQs)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->wakeTimeoutQs = wakeTimeoutQs;
}

EmberAfApplicationTask emberAfGetWakeTimeoutBitmaskCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->wakeTimeoutBitmask;
}

void emberAfSetWakeTimeoutBitmaskCallback(EmberAfApplicationTask tasks)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->wakeTimeoutBitmask = tasks;
}

void emberAfSetDefaultSleepControlCallback(EmberAfEventSleepControl control)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->defaultSleepControl = control;
}

EmberAfEventSleepControl emberAfGetDefaultSleepControlCallback(void)
{
  State *state = &states[emberGetCurrentNetwork()];
  return state->defaultSleepControl;
}

EmberAfEventSleepControl emberAfGetCurrentSleepControlCallback(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  State *state = &states[networkIndex];
  EmberAfEventSleepControl sleepControl = state->defaultSleepControl;
#if defined(EMBER_AF_GENERATED_EVENT_CONTEXT)
  int8u i;
  for (i = 0; i < emAfAppEventContextLength; i++) {
    EmberAfEventContext *context = &(emAfAppEventContext[i]);
    if (networkIndex == emberAfNetworkIndexFromEndpoint(context->endpoint)
        && emberEventControlGetActive(*context->eventControl)
        && sleepControl < context->sleepControl) {
      sleepControl = context->sleepControl;
    }
  }
#endif //EMBER_AF_GENERATED_EVENT_CONTEXT
  return sleepControl;
}


#if defined(EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_USE_BUTTONS)

void emberAfHalButtonIsrCallback(int8u button, int8u state)
{
  if (state != BUTTON_PRESSED) {
    return;
  }

  emberAfForceEndDeviceToStayAwake(button == BUTTON0);
}

#endif
