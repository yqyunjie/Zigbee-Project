#define CBKE_OPERATION_GENERATE_KEYS   0
#define CBKE_OPERATION_GENERATE_SECRET 1

#if defined(EMBER_AF_PLUGIN_TEST_HARNESS)
  extern EmberEventControl emAfKeyEstablishmentTestHarnessEventControl;

  extern int16u emAfKeyEstablishmentTestHarnessGenerateKeyTime;
  extern int16u emAfKeyEstablishmentTestHarnessConfirmKeyTime;

  extern int16u emAfKeyEstablishmentTestHarnessAdvertisedGenerateKeyTime;

  extern boolean emAfTestHarnessAllowRegistration;

  // Allows test harness to change the message or suppress it.
  // Returns TRUE if the message should be sent, false if not.
  boolean emAfKeyEstablishmentTestHarnessMessageSendCallback(int8u message);

  boolean emAfKeyEstablishmentTestHarnessCbkeCallback(int8u cbkeOperation,
                                                      int8u* data1,
                                                      int8u* data2);
  void emAfKeyEstablishementTestHarnessEventHandler(void);

  #define EMBER_AF_TEST_HARNESS_EVENT_STRINGS "Test harness",

  #define EMBER_KEY_ESTABLISHMENT_TEST_HARNESS_EVENT \
    { &emAfKeyEstablishmentTestHarnessEventControl, emAfKeyEstablishementTestHarnessEventHandler },
  #define EMBER_AF_CUSTOM_KE_EPHEMERAL_DATA_GENERATE_TIME_SECONDS \
    emAfKeyEstablishmentTestHarnessGenerateKeyTime
  #define EMBER_AF_CUSTOM_KE_GENERATE_SHARED_SECRET_TIME_SECONDS \
    emAfKeyEstablishmentTestHarnessConfirmKeyTime

  #define EM_AF_ADVERTISED_EPHEMERAL_DATA_GEN_TIME_SECONDS \
    emAfKeyEstablishmentTestHarnessAdvertisedGenerateKeyTime

  extern boolean emKeyEstablishmentPolicyAllowNewKeyEntries;
  extern boolean emAfTestHarnessSupportForNewPriceFields;

  #define sendSE11PublishPriceCommand emAfTestHarnessSupportForNewPriceFields

#else
  #define sendSE11PublishPriceCommand TRUE

  #define EMBER_AF_TEST_HARNESS_EVENT_STRINGS

  #define emAfKeyEstablishmentTestHarnessMessageSendCallback(x)      (TRUE)
  #define emAfKeyEstablishmentTestHarnessCbkeCallback(x, y, z) (FALSE)

  #define EMBER_KEY_ESTABLISHMENT_TEST_HARNESS_EVENT

  #define emAfTestHarnessAllowRegistration (1)
#endif

void emAfTestHarnessResetApsFrameCounter(void);
void emAfTestHarnessAdvanceApsFrameCounter(void);
