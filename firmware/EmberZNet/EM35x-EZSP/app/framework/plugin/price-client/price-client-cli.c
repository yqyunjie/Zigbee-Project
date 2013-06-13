// *******************************************************************
// * price-client-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/price-client/price-client.h"

static void print(void);

EmberCommandEntry emberAfPluginPriceClientCommands[] = {
  emberCommandEntryAction("print",  print, "u", "Print the price info"),
  emberCommandEntryTerminator(),
};

// plugin price-client print <endpoint:1>
static void print(void) 
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emAfPluginPriceClientPrintInfo(endpoint);
}
