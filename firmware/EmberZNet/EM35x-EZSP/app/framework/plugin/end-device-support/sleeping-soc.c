
#include "app/framework/util/af-event.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"

// *****************************************************************************
// Globals

// *****************************************************************************
// Forward Declarations

// *****************************************************************************
// Functions

#ifdef EMBER_AF_HAS_RX_ON_WHEN_IDLE_NETWORK

int32u emberAfCheckForSleepCallback(void)
{
  return 0;
}

#else

static int32u fullSleep(int32u sleepDurationMS, int8u eventIndex)
{
  int32u sleepDurationAttemptedMS = sleepDurationMS;

  if (emberAfPreGoToSleepCallback(sleepDurationMS)) {
    return 0; //User app says don't go to sleep
  }

  emAfPrintSleepDuration(sleepDurationMS, eventIndex);

  emberAfEepromShutdownCallback();
  
  // turn off the radio
  emberStackPowerDown();

  ATOMIC(
    // turn off board and peripherals
    halPowerDown();
    // turn micro to power save mode - wakes on external interrupt
    // or when the time specified expires
    halSleepForMilliseconds(&sleepDurationMS);
    // power up board and peripherals
    halPowerUp();
  );
  // power up radio
  emberStackPowerUp();

  emberAfEepromNoteInitializedStateCallback(FALSE);

  // Allow the stack to time out any of its events and check on its
  // own network state.
  emberTick();

  // Inform the application how long we have slept, sleepDuration has
  // been decremented by the call to halSleepForMilliseconds() by the amount
  // of time that we slept.  To get the actual sleep time we must determine
  // the delta between the amount we asked for and the amount of sleep time
  // LEFT in our request value.
  emberAfPostWakeUpCallback(sleepDurationAttemptedMS - sleepDurationMS);

  // message to know we are awake
  emberAfDebugPrintln("wakeup %l ms", 
                      (sleepDurationAttemptedMS - sleepDurationMS));
  emberAfDebugFlush();

  emAfPrintForceAwakeStatus();

  return (sleepDurationAttemptedMS - sleepDurationMS);
}

enum {
  AWAKE,
  IDLE,
  SLEEP,
};

int32u emberAfCheckForSleepCallback(void)
{
  int8u state = SLEEP;
  int8u nextEventIndex;
  int8u i;

  // If we were told to stay awake, sleeping and idling are not possible.  If
  // the stack says it is not okay to nap, we cannot sleep, but we can try to
  // idle.
  if (emAfForceEndDeviceToStayAwake) {
    return 0;
  } else if (!emberOkToNap()) {
    state = IDLE;
  }

  // Unless we have already established that we cannot sleep at all, we need to
  // look through every network to see whether any of them says it is not okay
  // to sleep.  Also, we may have been told to stay awake when not joined, so
  // watch for unjoined networks too.
  for (i = 0; state != AWAKE && i < EMBER_SUPPORTED_NETWORKS; i++) {
    emberAfPushNetworkIndex(i);
    if (emAfStayAwakeWhenNotJoined
        && emberNetworkState() != EMBER_JOINED_NETWORK) {
      state = AWAKE;
    } else if (!emAfOkToSleep()) {
      state = IDLE;
    }
    emberAfPopNetworkIndex();
  }

  // If anyone says to stay awake, we cannot sleep or idle.  If we do not have
  // to stay awake but also cannot sleep, we can try idling.  Otherwise, we can
  // go to sleep.
  switch (state) {
  case IDLE:
    ATOMIC(
      emberMarkTaskIdle(emAfTaskId);
    )
    emberMarkTaskActive(emAfTaskId);
    // FALLTHROUGH
  case AWAKE:
    return 0;
  default:
    {
      // If the stack says not to hibernate, it is because it has events that
      // need to be serviced and therefore we need to consider those events in
      // our sleep calculation.
      int32u sleepDurationMS = MAX_INT32U_VALUE;
      if (!emberOkToHibernate()) {
        sleepDurationMS = emberMsToNextStackEvent();
      }
      sleepDurationMS = emberAfMsToNextEventExtended(sleepDurationMS,
                                                     &nextEventIndex);
      return (sleepDurationMS == 0
              ? 0
              : fullSleep(sleepDurationMS, nextEventIndex));
    }
  }
}

#endif
