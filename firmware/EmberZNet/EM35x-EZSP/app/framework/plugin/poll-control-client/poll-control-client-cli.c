// *******************************************************************
// * poll-control-client-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/poll-control-client/poll-control-client.h"

static void mode(void);
static void timeout(void);
static void respond(void);
static void print(void);
static void stop(void);
static void setLong(void);
static void setShort(void);

EmberCommandEntry emberAfPluginPollControlClientCommands[] = {
  emberCommandEntryAction("mode",  mode, "u", ""),
  emberCommandEntryAction("timeout", timeout, "v", ""),
  emberCommandEntryAction("respond",  respond, "u", ""),
  emberCommandEntryAction("print", print, "", ""),
  emberCommandEntryAction("stop", stop, "vvv", ""),
  emberCommandEntryAction("set-long", setLong, "vvvw", ""),
  emberCommandEntryAction("set-short", setShort, "vvvv", ""),
  emberCommandEntryTerminator(),
};

// plugin poll-control-client mode <mode:1>
static void mode(void)
{
  int8u mode = (int8u)emberUnsignedCommandArgument(0);
  emAfSetFastPollingMode(mode);
  emberAfPollControlClusterPrintln("%p 0x%x", "mode", mode);
}

// plugin poll-control-client timeout <timeout:2>
static void timeout(void)
{
  int16u timeout = (int16u)emberUnsignedCommandArgument(0);
  emAfSetFastPollingTimeout(timeout);
  emberAfPollControlClusterPrintln("%p 0x%x", "timeout", timeout);
}

// plugin poll-control-client respond <mode:1>
static void respond(void)
{
  int8u mode = (int8u)emberUnsignedCommandArgument(0);
  emAfSetResponseMode(mode);
  emberAfPollControlClusterPrintln("%p 0x%x", "respond", mode);
}

static void print(void)
{
  emAfPollControlClientPrint();
}

// plugin poll-control-client stop <nodeId:2> <srcEndpoint:2> <dstEndpoint:2>
static void stop(void)
{
  EmberStatus status;
  emberAfFillCommandPollControlClusterFastPollStop();
  emberAfSetCommandEndpoints((int16u)emberUnsignedCommandArgument(1),
                             (int16u)emberUnsignedCommandArgument(2));
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                     (EmberNodeId)emberUnsignedCommandArgument(0));
  if(status != EMBER_SUCCESS) {
    emberAfPollControlClusterPrintln("Error in fast poll stop %x", status);
  }
}

// plugin poll-control-client set-long <nodeId:2> <srcEndpoint:2> <dstEndpoint:2> 
// <interval:4>
static void setLong(void)
{
  EmberStatus status;

  emberAfFillCommandPollControlClusterSetLongPollInterval((int32u)emberUnsignedCommandArgument(3));

  emberAfSetCommandEndpoints((int16u)emberUnsignedCommandArgument(1),
                             (int16u)emberUnsignedCommandArgument(2));
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                     (EmberNodeId)emberUnsignedCommandArgument(0));
  if(status != EMBER_SUCCESS) {
    emberAfPollControlClusterPrintln("Error setting long poll interval %x", status);
  }
}

// plugin poll-control-client set-short <nodeId:2> <srcEndpoint:2> <dstEndpoint:2> 
// <interval:2>
static void setShort(void)
{
  EmberStatus status;

  emberAfFillCommandPollControlClusterSetShortPollInterval((int16u)emberUnsignedCommandArgument(3));

  emberAfSetCommandEndpoints((int16u)emberUnsignedCommandArgument(1),
                             (int16u)emberUnsignedCommandArgument(2));
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                     (EmberNodeId)emberUnsignedCommandArgument(0));
  if(status != EMBER_SUCCESS) {
    emberAfPollControlClusterPrintln("Error setting short poll interval %x", status);
  }
}
