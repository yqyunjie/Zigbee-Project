// *****************************************************************************
// * concentrator-support-cli.c
// *
// * CLI interface to manage the concentrator's periodic MTORR broadcasts.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/concentrator/concentrator-support.h"

// *****************************************************************************
// Forward Declarations

static void concentratorStatus(void);
static void concentratorStartDiscovery(void);
static void optionAggregationCommand(void);

// *****************************************************************************
// Globalsonon

EmberCommandEntry emberAfPluginConcentratorCommands[] = {
  emberCommandEntryAction("status", concentratorStatus, "",
                          "Prints current status and configured parameters of the concentrator"),
  emberCommandEntryAction("start",  concentratorStartDiscovery, "",
                          "Starts the periodic broadcast of MTORRs"),
  emberCommandEntryAction("stop",   emberAfPluginConcentratorStopDiscovery, "",
                          "Stops the periodic broadcast of MTORRs"),
  emberCommandEntryAction("agg", optionAggregationCommand, "", ""),
  
  emberCommandEntryTerminator(),
};

// *****************************************************************************
// Functions

void concentratorStatus(void)
{
  boolean active = (emberAfPluginConcentratorUpdateEventControl.status
                    != EMBER_EVENT_INACTIVE);
  int32u nowMS32 = halCommonGetInt32uMillisecondTick();

  emberAfAppPrintln("Active: %p", 
                    (active
                     ? "yes"
                     : "no"));
  emberAfAppPrintln("Type:  %p RAM",
                    ((EMBER_AF_PLUGIN_CONCENTRATOR_CONCENTRATOR_TYPE
                      == EMBER_LOW_RAM_CONCENTRATOR)
                     ? "Low"
                     : "High"));
  emberAfAppPrintln("Time before next broadcast (ms):   %l",
                    emberAfPluginConcentratorUpdateEventControl.timeToExecute - nowMS32);
  emberAfAppPrintln("Min Time Between Broadcasts (sec): %d", 
                    EMBER_AF_PLUGIN_CONCENTRATOR_MIN_TIME_BETWEEN_BROADCASTS_SECONDS);
  emberAfAppPrintln("Max Time Between Broadcasts (sec): %d", 
                    EMBER_AF_PLUGIN_CONCENTRATOR_MAX_TIME_BETWEEN_BROADCASTS_SECONDS);
  emberAfAppPrintln("Max Hops: %d",
                    (EMBER_AF_PLUGIN_CONCENTRATOR_MAX_HOPS == 0
                     ? EMBER_MAX_HOPS
                     : EMBER_AF_PLUGIN_CONCENTRATOR_MAX_HOPS));
  emberAfAppPrintln("Route Error Threshold:      %d",
                    EMBER_AF_PLUGIN_CONCENTRATOR_ROUTE_ERROR_THRESHOLD);
  emberAfAppPrintln("Delivery Failure Threshold: %d",
                    EMBER_AF_PLUGIN_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD);
}

void concentratorStartDiscovery(void)
{
  int32u qsLeft = emberAfPluginConcentratorQueueDiscovery();
  emberAfAppPrintln("%d sec until next MTORR broadcast", (qsLeft >> 2));
}

static void optionAggregationCommand(void)
{
  emberAfPluginConcentratorQueueDiscovery();
}
