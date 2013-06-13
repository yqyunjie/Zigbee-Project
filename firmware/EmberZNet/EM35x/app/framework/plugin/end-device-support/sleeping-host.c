
#include "app/framework/util/af-event.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"
#include "app/framework/util/af-main.h"

// *****************************************************************************
// Globals

#ifndef EMBER_AF_HAS_RX_ON_WHEN_IDLE_NETWORK
static volatile boolean ncpIsAwake = FALSE;
#endif

// *****************************************************************************

#ifdef EMBER_AF_HAS_RX_ON_WHEN_IDLE_NETWORK

int32u emberAfCheckForSleepCallback(void)
{
  return 0;
}

// WARNING:  This executes in ISR context.
void emberAfNcpIsAwakeIsrCallback(void)
{
}

#else

static int32u fullSleep(int32u sleepDurationMS, int8u eventIndex)
{
  EmberStatus status;
  int32u time = halCommonGetInt32uMillisecondTick();
  int32u elapsedSleepTimeMS;
  int8u timerUnit;
  int16u timerDuration;
  int32u timerDurationMS;

  // Get the proper timer duration and units we will use to set our
  // wake timer on the NCP. We are basically converting MS to
  // MS, QS or minutes.
  emAfGetTimerDurationAndUnitFromMS(sleepDurationMS, &timerDuration, &timerUnit);

  // Translate back to MS so that we call the PreGoToSleepCallback
  // with the accurate MS (corrected for rounding due to units)
  timerDurationMS = emAfGetMSFromTimerDurationAndUnit(timerDuration, timerUnit);

  // a final check with the app if we can go to sleep
  if (emberAfPreGoToSleepCallback(timerDurationMS)) {
    return 0; // User app says don't go to sleep.
  }

  emAfPrintSleepDuration(sleepDurationMS, eventIndex);

  emberAfEepromShutdownCallback();

  // If we have anything waiting in the queue send it
  // before we sleep
  emberSerialWaitSend(EMBER_AF_PRINT_OUTPUT);

  // set the variable that will instruct the stack to sleep after
  // processing the next EZSP command
  ezspSleepMode = EZSP_FRAME_CONTROL_DEEP_SLEEP;

  // Set timer on NCP so we get woken up, this call will
  // also put the NCP to sleep due to sleep mode setting above.
  status = ezspSetTimer(0, timerDuration, timerUnit, FALSE);

  // check that we were able to set the timer and that an
  // ezsp error was not received in the call.
  if (status == EMBER_SUCCESS && !emberAfNcpNeedsReset()) {

    // setting awake flag to false since NCP is sleeping.
    ncpIsAwake = FALSE;
    // this disables interrupts and reenables them once it completes
    ATOMIC(
      halPowerDown();  // turn off board and peripherals
      // Use maintain timer so that we can accurately
      // measure how long we have actually slept and don't
      // have to manage the system time.
      halSleep(SLEEPMODE_MAINTAINTIMER);
      halPowerUp();   // power up board and peripherals
    );
  } else {
    emberAfDebugPrintln("%p: setTimer fail: 0x%X", "Error", status);
    emberAfDebugFlush();
  }

  // Make sure the NCP is awake since the host could have been waken up via
  // other external interrupt like button press.  If it's not then wake it.
  if (ncpIsAwake == FALSE) {
    ezspWakeUp();
    do {
#ifdef EMBER_TEST
      // Simulation only.
      simulatedTimePasses();
#endif
    } while (ncpIsAwake == FALSE);
  }

  ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;

  // Get the amount of time that has actually ellapsed since we went to sleep
  elapsedSleepTimeMS = elapsedTimeInt32u(time, halCommonGetInt32uMillisecondTick());

  // Inform the application how long we slept
  emberAfPostWakeUpCallback(elapsedSleepTimeMS);

  emberAfDebugPrintln("wakeup %l ms", elapsedSleepTimeMS);
  emberAfDebugFlush();

  emAfPrintForceAwakeStatus();

  return elapsedSleepTimeMS;
}

int32u emberAfCheckForSleepCallback(void)
{
  int32u sleepDurationMS = MAX_INT32U_VALUE;
  int8u nextEventIndex;
  int8u i;

  // If we were told to stay awake, sleeping is not possible.
  if (emAfForceEndDeviceToStayAwake) {
    return 0;
  }

  // Unless we have already established that we cannot sleep at all, we need to
  // look through every network to see whether any of them says it is not okay
  // to sleep.  Also, we may have been told to stay awake when not joined, so
  // watch for unjoined networks too.
  for (i = 0; sleepDurationMS != 0 && i < EMBER_SUPPORTED_NETWORKS; i++) {
    emberAfPushNetworkIndex(i);
    if ((emAfStayAwakeWhenNotJoined
         && emberNetworkState() != EMBER_JOINED_NETWORK)
        || !emAfOkToSleep()) {
      sleepDurationMS = 0;
    }
    emberAfPopNetworkIndex();
  }

  if (sleepDurationMS != 0) {
    sleepDurationMS = emberAfMsToNextEventExtended(MAX_TIMER_MILLISECONDS_HOST,
                                                   &nextEventIndex);
  }
  return (sleepDurationMS == 0
          ? 0
          : fullSleep(sleepDurationMS, nextEventIndex));
}

// WARNING:  This executes in ISR context.
void emberAfNcpIsAwakeIsrCallback(void)
{
  ncpIsAwake = TRUE;
}

#endif
