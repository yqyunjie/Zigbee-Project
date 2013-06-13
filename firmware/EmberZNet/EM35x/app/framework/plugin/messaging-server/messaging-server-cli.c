// *******************************************************************
// * messaging-server-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/messaging-server/messaging-server.h"

static void msg(void);
static void append(void);
static void id(void);
static void time(void);
static void relativeTime(void);
static void transmission(void);
static void importance(void);
static void confirm(void);
static void valid(void);
static void display(void);
static void cancel(void);
static void print(void);

static EmberAfPluginMessagingServerMessage message;

EmberCommandEntry emberAfPluginMessagingServerTransmissionCommands[] = {
  emberCommandEntryAction("normal",  transmission, "", ""),
  emberCommandEntryAction("ipan",  transmission, "", ""),
  emberCommandEntryAction("both",  transmission, "", ""),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emberAfPluginMessagingServerImportanceCommands[] = {
  emberCommandEntryAction("low",  importance, "", ""),
  emberCommandEntryAction("medium",  importance, "", ""),
  emberCommandEntryAction("high",  importance, "", ""),
  emberCommandEntryAction("critical",  importance, "", ""),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emberAfPluginMessagingServerConfirmCommands[] = {
  emberCommandEntryAction("not",  confirm, "", ""),
  emberCommandEntryAction("req",  confirm, "", ""),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emberAfPluginMessagingServerCommands[] = {
  emberCommandEntryAction("message",  msg, "b", ""),
  emberCommandEntryAction("append",  append, "b", ""),
  emberCommandEntryAction("id",  id, "w", ""),
  emberCommandEntryAction("time",  time, "wv", ""),
  emberCommandEntryAction("relative-time",  relativeTime, "sv", ""),
  emberCommandEntryAction("transmission",  NULL, (PGM_P)emberAfPluginMessagingServerTransmissionCommands, ""),
  emberCommandEntryAction("importance",  NULL, (PGM_P)emberAfPluginMessagingServerImportanceCommands, ""),
  emberCommandEntryAction("confirm",  NULL, (PGM_P)emberAfPluginMessagingServerConfirmCommands, ""),
  emberCommandEntryAction("valid",  valid, "u", ""),
  emberCommandEntryAction("invalid",  valid, "u", ""),
  emberCommandEntryAction("display",  display, "vuu", ""),
  emberCommandEntryAction("cancel",  cancel, "vuu", ""),
  emberCommandEntryAction("print", print, "u", ""),
  emberCommandEntryTerminator(),
};

// plugin messaging-server message <message string>
static void msg(void)
{
  int8u length = emberCopyStringArgument(0,
                                         message.message + 1,
                                         EMBER_AF_PLUGIN_MESSAGING_SERVER_MESSAGE_SIZE,
                                         FALSE);
  message.message[0] = length;
}

// plugin messaging-server append <message string>
static void append(void)
{
  int8u oldLength = message.message[0];
  int8u length = emberCopyStringArgument(0,
                                         message.message + oldLength + 1,
                                         (EMBER_AF_PLUGIN_MESSAGING_SERVER_MESSAGE_SIZE
                                          - oldLength),
                                         FALSE);
  message.message[0] = oldLength + length;
}

// plugin messaging-server id <messageId:4>
static void id(void)
{
  message.messageId = emberUnsignedCommandArgument(0);
}

// plugin messaging-server time <start time:4> <duration:2>
static void time(void)
{
  message.startTime = emberUnsignedCommandArgument(0);
  message.durationInMinutes = (int16u)emberUnsignedCommandArgument(1);
}

// plugin messaging-server relative-time <+/-time> <duration>

// Rather than use absolute time, this will set the start-time relative to the current time +/-
// the CLI parameter in MINUTES.
static void relativeTime(void)
{
  message.startTime = (emberAfGetCurrentTime()
                       + (emberSignedCommandArgument(0) * 60));
  message.durationInMinutes = (int16u)emberUnsignedCommandArgument(1);
}

// plugin messaging-server transmission <normal | ipan | both>
static void transmission(void)
{
  int8u commandChar = emberCurrentCommand->name[0];
  message.messageControl &= ~ZCL_MESSAGING_CLUSTER_TRANSMISSION_MASK;
  if (commandChar == 'b') { // both
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_TRANSMISSION_NORMAL_AND_ANONYMOUS;
  } else if (commandChar == 'i') { // inter pan
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_TRANSMISSION_ANONYMOUS;
  }
  // Do nothing for 'normal'.
}

// plugin messaging-server importance <low | medium | high | critical>
static void importance(void)
{
  int8u commandChar = emberCurrentCommand->name[0];
  message.messageControl &= ~ZCL_MESSAGING_CLUSTER_IMPORTANCE_MASK;
  if (commandChar == 'm') { // medium
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_IMPORTANCE_MEDIUM;
  } else if (commandChar == 'h') { // high
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_IMPORTANCE_HIGH;
  } else if (commandChar == 'c') { // critical
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_IMPORTANCE_CRITICAL;
  }
  // Do nothing for 'low' importance.
}

// plugin messaging-server confirm <not | req>
static void confirm(void)
{
  int8u commandChar = emberCurrentCommand->name[0];
  message.messageControl &= ~ZCL_MESSAGING_CLUSTER_CONFIRMATION_MASK;
  if (commandChar == 'r') { // required
    message.messageControl |= EMBER_ZCL_MESSAGING_CONTROL_CONFIRMATION_REQUIRED;
  }
  // Do nothing for 'not' (not required).
}

// plugin messaging-server <valid | invalid>
static void valid(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emberAfPluginMessagingServerSetMessage(endpoint,
                                         emberCurrentCommand->name[0] == 'v'
                                         ? &message
                                         : NULL);
}

// plugin messaging-server display <nodeId:2> <srcEndpoint:1> <dstEndpoint:1>
static void display(void)
{
  EmberNodeId nodeId = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u srcEndpoint = (int8u)emberUnsignedCommandArgument(1);
  int8u dstEndpoint = (int8u)emberUnsignedCommandArgument(2);
  emberAfPluginMessagingServerDisplayMessage(nodeId,
                                             srcEndpoint,
                                             dstEndpoint);
}

// plugin messaging-server cancel <nodeId:2> <srcEndpoint:1> <dstEndpoint:1>
static void cancel(void)
{
  EmberNodeId nodeId = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u srcEndpoint = (int8u)emberUnsignedCommandArgument(1);
  int8u dstEndpoint = (int8u)emberUnsignedCommandArgument(2);
  emberAfPluginMessagingServerCancelMessage(nodeId,
                                            srcEndpoint,
                                            dstEndpoint);
}

// plugin messaging-server print <endpoint:1>
static void print(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emAfPluginMessagingServerPrintInfo(endpoint);
}
