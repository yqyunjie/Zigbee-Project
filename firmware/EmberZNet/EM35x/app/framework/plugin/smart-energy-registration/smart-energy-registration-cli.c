// *******************************************************************
// * smart-energy-registration-cli.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "smart-energy-registration.h"

#ifdef EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD
static void setRegistrationDelayPeriod(void);
#endif //EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD

EmberCommandEntry emberAfPluginSmartEnergyRegistrationCommands[] = {
#ifdef EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD
  // Allows to set the discovery period (in seconds).
  emberCommandEntryAction("set-period",      setRegistrationDelayPeriod, "w", "Sets the discovery period (in seconds)"),
#endif //EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD

  emberCommandEntryTerminator(),
};

#ifdef EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD
static void setRegistrationDelayPeriod(void)
{
  emAfPluginSmartEnergyRegistrationDiscoveryPeriod =
      (int32u)emberUnsignedCommandArgument(0) * MILLISECOND_TICKS_PER_SECOND;

  emberAfAppPrintln("Smart energy registration discovery period set to 0x%4x",
        emAfPluginSmartEnergyRegistrationDiscoveryPeriod);
}
#endif //EMBER_AF_PLUGIN_SMART_ENERGY_REGISTRATION_DELAY_PERIOD
