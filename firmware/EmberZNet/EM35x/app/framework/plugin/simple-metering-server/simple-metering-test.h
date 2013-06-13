// *******************************************************************
// * simple-metering-test.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

void emAfTestMeterTick(int8u endpoint);
void emAfTestMeterInit(int8u endpoint);

// The Test meter's profile interval period timeframe enum value
// is 3 which according to the SE spec is 15 minutes
#define PROFILE_INTERVAL_PERIOD_TIMEFRAME 3
#define PROFILE_INTERVAL_PERIOD_IN_MINUTES 15
#define PROFILE_INTERVAL_PERIOD_IN_SECONDS (PROFILE_INTERVAL_PERIOD_IN_MINUTES * 60)
#define PROFILE_INTERVAL_PERIOD_IN_MILLISECONDS ((PROFILE_INTERVAL_PERIOD_IN_MINUTES * 60) * 1000)
#define MAX_PROFILE_INDEX (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_PROFILES - 1)
#define TOTAL_PROFILE_TIME_SPAN_IN_SECONDS (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_PROFILES * (PROFILE_INTERVAL_PERIOD_IN_MINUTES * 60))

void afTestMeterPrint(void);
void afTestMeterSetConsumptionRate(int16u rate);
void afTestMeterSetConsumptionVariance(int16u variance);
void afTestMeterAdjust(int8u endpoint);

// 0 off, 1 if electric, 2 if gas
void afTestMeterMode(int8u endpoint, int8u electric);
 
void afTestMeterSetError(int8u endpoint, int8u error);
// Sets the random error occurence:
//   data = 0: disable
//   otherwise: 
void afTestMeterRandomError(int8u changeIn256);

void afTestMeterEnableProfiles(int8u enable);

boolean emAfTestMeterGetProfiles(int8u intervalChannel,
                                 int32u endTime,
                                 int8u numberOfPeriods);


