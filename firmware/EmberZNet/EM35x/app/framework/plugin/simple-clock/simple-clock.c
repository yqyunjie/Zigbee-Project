// *******************************************************************
// * simple-clock.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"

// The variable "time" represents seconds since the ZigBee epoch, which was 0
// hours, 0 minutes, 0 seconds, on the 1st of January, 2000 UTC.  The variable
// "tick" is the millsecond tick at which that time was set.  The variable
// "remainder" is used to track sub-second chunks of time when converting from
// ticks to seconds.
typedef struct {
  int32u time;
  int32u tick;
  int16u remainder;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

// These events are used to periodically force an update to the internal time
// in order to avoid potential rollover problems with the system ticks.  A call
// to GetCurrentTime or SetTime will reschedule the event for the current
// network.
extern EmberEventControl emberAfPluginSimpleClockUpdateNetworkEventControls[];

void emberAfPluginSimpleClockInitCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    emberAfPushNetworkIndex(i);
    emberAfSetTimeCallback(0);
    emberAfPopNetworkIndex();
  }
}

int32u emberAfGetCurrentTimeCallback(void)
{
  // Using system ticks, calculate how many seconds have elapsed since we last
  // got the time.  That amount plus the old time is our new time.  Remember
  // the tick time right now too so we can do the same calculations again when
  // we are next asked for the time.  Also, keep track of the sub-second chunks
  // of time during the conversion from ticks to seconds so the clock does not
  // drift due to rounding.
  State *state = &states[emberGetCurrentNetwork()];
  int32u elapsed, lastTick = state->tick;
  state->tick = halCommonGetInt32uMillisecondTick();
  elapsed = elapsedTimeInt32u(lastTick, state->tick);
  state->time += elapsed / MILLISECOND_TICKS_PER_SECOND;
  state->remainder += elapsed % MILLISECOND_TICKS_PER_SECOND;
  if (MILLISECOND_TICKS_PER_SECOND <= state->remainder) {
    state->time++;
    state->remainder -= MILLISECOND_TICKS_PER_SECOND;
  }

  // Schedule an event to recalculate time to help avoid rollover problems.
  emberAfNetworkEventControlSetDelayMinutes(emberAfPluginSimpleClockUpdateNetworkEventControls,
                                            1440); // one day
  return state->time;
}

void emberAfSetTimeCallback(int32u utcTime)
{
  State *state = &states[emberGetCurrentNetwork()];
  state->tick = halCommonGetInt32uMillisecondTick();
  state->time = utcTime;
  state->remainder = 0;

  // Immediately get the new time in order to reschedule the event.
  emberAfGetCurrentTimeCallback();
}

void emberAfPluginSimpleClockUpdateNetworkEventHandler(void)
{
  // Get the time, which will reschedule the event.
  emberAfGetCurrentTimeCallback();
}
