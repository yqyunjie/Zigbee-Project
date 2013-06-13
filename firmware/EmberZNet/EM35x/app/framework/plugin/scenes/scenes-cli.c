// *******************************************************************
// * scenes-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "scenes.h"

EmberCommandEntry emberAfPluginScenesCommands[] = {
  emberCommandEntryAction("print", emAfPluginScenesServerPrintInfo, "", "Print the scenes table."),
  emberCommandEntryTerminator(),
};
