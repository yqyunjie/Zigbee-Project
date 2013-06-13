// *******************************************************************
// * messaging-client-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/messaging-client/messaging-client.h"

static void confirm(void);
static void print(void);

EmberCommandEntry emberAfPluginMessagingClientCommands[] = {
  emberCommandEntryAction("confirm",  confirm, "u", ""),
  emberCommandEntryAction("print", print, "u", ""),
  emberCommandEntryTerminator(),
};

// plugin messaging-client confirm <endpoint:1>
static void confirm(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfStatus status = emberAfPluginMessagingClientConfirmMessage(endpoint);
  emberAfMessagingClusterPrintln("%p 0x%x", "confirm", status);
}

// plugin messaging-client print <endpoint:1>
static void print(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emAfPluginMessagingClientPrintInfo(endpoint);
}
