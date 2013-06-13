// * standalone-bootloader-client-cli.c
// *
// * This file defines the standalone bootloader client CLI.
// * 
// * Copyright 2012 by Silicon Labs. All rights reserved.                   *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/standalone-bootloader-common/bootloader-protocol.h"
#include "standalone-bootloader-client.h"

#include "app/util/serial/command-interpreter2.h"

//------------------------------------------------------------------------------
// Forward declarations



//------------------------------------------------------------------------------
// Globals

EmberCommandEntry emberAfPluginStandaloneBootloaderClientCommands[] = {
  emberCommandEntryAction("status", emAfStandaloneBootloaderClientPrintStatus, "",
                          "Prints the current status of the client"),
  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Functions

