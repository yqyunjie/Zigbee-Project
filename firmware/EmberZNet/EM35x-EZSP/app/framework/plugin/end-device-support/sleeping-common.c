
#include "app/framework/util/af-event.h"
#include "app/framework/plugin/end-device-support/end-device-support.h"
#include "app/framework/util/af-main.h"

// *****************************************************************************
// Globals

#if defined(EZSP_HOST)
  #define MAX_SLEEP_VALUE_MS MAX_TIMER_MILLISECONDS_HOST
#else
  #define MAX_SLEEP_VALUE_MS 0xFFFFFFFFUL
#endif

// *****************************************************************************

void emAfPrintSleepDuration(int32u sleepDurationMS, int8u eventIndex)
{
  emberAfDebugPrint("sleep ");
  if (sleepDurationMS == MAX_SLEEP_VALUE_MS) {
    emberAfDebugPrintln("forever");
  } else {
    emberAfDebugPrintln("%l ms (until %p Event)", 
                        sleepDurationMS,
                        (eventIndex == 0xFF
                         ? "Stack"
                         : emAfEventStrings[eventIndex]));
  }

  // IMPORTANT:  At this point App Framework apps do blocking serial IO.
  // This means that flush macros are no-ops and serial data is printed
  // at the uart's rate.  
  // If there is serial data in the output queue prior to sleeping then we
  // may inadvertently wake up unless we force the flush of the data here.
  emberSerialWaitSend(APP_SERIAL);
}

void emAfPrintForceAwakeStatus(void)
{
  if (emAfForceEndDeviceToStayAwake) {
    emberAfDebugPrintln("Force to Stay awake: yes");
  }
}
