// *******************************************************************
// * identify-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

static void print(void);

EmberCommandEntry emberAfPluginIdentifyCommands[] = {
  emberCommandEntryAction("print",  print, "", "Print the identify state of each endpoint"),
  emberCommandEntryTerminator(),
};

// plugin identify print
static void print(void) 
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
  int8u i;
  for (i = 0; i < emberAfEndpointCount(); ++i) {
    int8u endpoint = emberAfEndpointFromIndex(i);
    emberAfIdentifyClusterPrintln("Endpoint 0x%x is identifying: %p",
                                  endpoint,
                                  (emberAfIsDeviceIdentifying(endpoint)
                                   ? "TRUE"
                                   : "FALSE"));
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_IDENTIFY_CLUSTER)
}
