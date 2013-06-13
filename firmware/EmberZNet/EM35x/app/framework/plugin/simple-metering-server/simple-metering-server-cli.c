// *******************************************************************
// * simple-metering-server-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE
#include "app/framework/plugin/simple-metering-server/simple-metering-test.h"

static int8u getEndpointArgument(int8u index);
static void print(void);
static void rate(void);
static void variance(void);
static void adjust(void);
static void off(void);
#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == ELECTRIC_METER)
static void electric(void);
#endif
#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == GAS_METER)
static void gas(void);
#endif
#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ERRORS
static void randomError(void);
static void setError(void);
#endif //EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ERRORS
#if ( EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_PROFILES != 0 )
static void profiles(void);
#endif
#endif //EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE

EmberCommandEntry emberAfPluginSimpleMeteringServerCommands[] = {
#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE
  emberCommandEntryAction("print", print, "", ""),
  emberCommandEntryAction("rate", rate, "v", ""),
  emberCommandEntryAction("variance", variance, "v", ""),
  emberCommandEntryAction("adjust", adjust, "u", ""),
  emberCommandEntryAction("off", off, "u", ""),
#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == ELECTRIC_METER)
  emberCommandEntryAction("electric", electric, "u", ""),
#endif
#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == GAS_METER)
  emberCommandEntryAction("gas", gas, "u", ""),
#endif
#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ERRORS
  emberCommandEntryAction("rnd_error", randomError, "u", ""),
  emberCommandEntryAction("set_error", setError, "uu", ""),
#endif
#if ( EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_PROFILES != 0 )
  emberCommandEntryAction("profiles", profiles, "u", ""),
#endif
#endif //EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE
  emberCommandEntryTerminator(),
};

#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE
static int8u getEndpointArgument(int8u index)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(index);
  return (endpoint == 0
          ? emberAfPrimaryEndpointForCurrentNetworkIndex()
          : endpoint);
}

// plugin simple-metering-server print
static void print(void) 
{
  afTestMeterPrint();
}

// plugin simple-metering-server rate <int:2>
static void rate(void)
{
  afTestMeterSetConsumptionRate((int16u)emberUnsignedCommandArgument(0));
}

// plugin simple-metering-server variance <int:2>
static void variance(void)
{
  afTestMeterSetConsumptionVariance((int16u)emberUnsignedCommandArgument(0));
}

// plugin simple-metering-server adjust <endpoint:1>
static void adjust(void)
{
  afTestMeterAdjust(getEndpointArgument(0));
}

// plugin simple-metering-server off <endpoint: 1>
static void off(void)
{
  afTestMeterMode(getEndpointArgument(0), 0);
}

#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == ELECTRIC_METER)
// plugin simple-metering-server electric <endpoint:1>
static void electric(void)
{
  afTestMeterMode(getEndpointArgument(0), 1);
}
#endif

#if (EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_TYPE == GAS_METER)
// plugin simple-metering-server gas <endpoint:1>
static void gas(void)
{
  afTestMeterMode(getEndpointArgument(0), 2);
}
#endif

#ifdef EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ERRORS
// plugin simple-metering-server rnd_error <data:1>
static void randomError(void)
{
  // enables random error setting at each tick
  afTestMeterRandomError((int8u)emberUnsignedCommandArgument(0));  
}

// plugin simple-metering-server set_error <data:1> <endpoint:1>
static void setError(void)
{
  // sets error, in the process overriding random_error
  afTestMeterSetError(getEndpointArgument(1), 
                      (int8u)emberUnsignedCommandArgument(0));                           
}
#endif //EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ERRORS

#if ( EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_PROFILES != 0 )
// plugin simple-metering-server profiles <data:1>
static void profiles(void)
{
  afTestMeterEnableProfiles((int8u)emberUnsignedCommandArgument(0));
}
#endif
#endif //EMBER_AF_PLUGIN_SIMPLE_METERING_SERVER_TEST_METER_ENABLE
