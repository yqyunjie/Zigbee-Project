



#ifdef EMBER_AF_STAY_AWAKE_WHEN_NOT_JOINED
  #error "EMBER_AF_STAY_AWAKE_WHEN_NOT_JOINED is deprecated.  Please use option in the 'End Device Support'  plugin."
#endif

#if !defined(EMBER_AF_NUM_MISSED_POLLS_TO_TRIGGER_MOVE)
  #define EMBER_AF_NUM_MISSED_POLLS_TO_TRIGGER_MOVE EMBER_AF_PLUGIN_END_DEVICE_SUPPORT_MAX_MISSED_POLLS
#endif

extern boolean emAfStayAwakeWhenNotJoined;
extern boolean emAfForceEndDeviceToStayAwake;

void emberAfForceEndDeviceToStayAwake(boolean stayAwake);
void emAfPrintSleepDuration(int32u sleepDurationMS, int8u eventIndex);
void emAfPrintForceAwakeStatus(void);

#define emAfOkToSleep() (emberAfGetCurrentSleepControl() < EMBER_AF_STAY_AWAKE)
