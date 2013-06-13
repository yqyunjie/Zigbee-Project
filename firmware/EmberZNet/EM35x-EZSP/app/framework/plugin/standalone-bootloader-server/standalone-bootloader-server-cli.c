// *****************************************************************************
// * standalone-bootloader-server-cli.c
// *
// * This file defines the standalone bootloader server CLI.
// * 
// * Copyright 2012 by Silicon Labs. All rights reserved.                   *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/include/af-types.h"
#include "app/framework/plugin/standalone-bootloader-common/bootloader-protocol.h"
#include "standalone-bootloader-server.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/ota-common/ota-cli.h"

//------------------------------------------------------------------------------
// Forward declarations

static void sendLaunchRequestToEui(void);
static void sendLaunchRequestToTarget(void);

//------------------------------------------------------------------------------
// Globals

static EmberCommandEntry bootloadCommands[] = {
  emberCommandEntryAction("target", sendLaunchRequestToTarget, "uu",
                          "Sends a launch bootloader request to the currently cached target"),
  emberCommandEntryAction("eui",    sendLaunchRequestToEui, "buu",
                          "Send a launch bootloader request to the specified EUI64"),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emberAfPluginStandaloneBootloaderServerCommands[] = {
  emberCommandEntryAction("status", emAfStandaloneBootloaderServerPrintStatus, "",
                          "Prints the current status of the server"),
  emberCommandEntryAction("query", (CommandAction)emberAfPluginStandaloneBootloaderServerBroadcastQuery, "",
                          "Sends a broadcast standalone bootloader query."),
  emberCommandEntrySubMenu("bootload", bootloadCommands,
                           "Sends a request to launch the standalone bootloader"),
  emberCommandEntryAction("print-target", emAfStandaloneBootloaderServerPrintTargetClientInfoCommand, "",
                          "Print the cached info about the current bootload target"),
  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Functions

static void sendLaunchRequestToTarget(void)
{
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  EmberAfOtaImageId id = emAfOtaFindImageIdByIndex(index);
  int8u tag =  (int8u)emberUnsignedCommandArgument(1);
  
  emberAfPluginStandaloneBootloaderServerStartClientBootloadWithCurrentTarget(&id, tag);
}

static void sendLaunchRequestToEui(void)
{
  EmberEUI64 longId;
  int8u index = (int8u)emberUnsignedCommandArgument(1);
  EmberAfOtaImageId id = emAfOtaFindImageIdByIndex(index);
  int8u tag =  (int8u)emberUnsignedCommandArgument(2);
  emberCopyEui64Argument(0, longId);

  emberAfPluginStandaloneBootloaderServerStartClientBootload(longId, &id, tag);
}
// *****************************************************************************
