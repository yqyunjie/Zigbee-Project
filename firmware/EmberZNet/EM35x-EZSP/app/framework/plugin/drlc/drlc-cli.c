// *******************************************************************
// * drlc-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/drlc/demand-response-load-control.h"
#include "app/framework/plugin/drlc/load-control-event-table.h"

static void opt(void);
static void print(void);
static void clear(void);

EmberCommandEntry emberAfPluginDrlcOptCommands[] = {
  emberCommandEntryAction("in", opt, "uw", ""),
  emberCommandEntryAction("out", opt, "uw", ""),
  emberCommandEntryTerminator(),
};

EmberCommandEntry emberAfPluginDrlcCommands[] = {
  emberCommandEntryAction("opt", NULL, (PGM_P)emberAfPluginDrlcOptCommands, ""),
  emberCommandEntryAction("print", print, "u", ""),
  emberCommandEntryAction("clear", clear, "u", ""),
  emberCommandEntryTerminator(),
};

// plugin drlc opt <in | out> <endpoint:1> <id:4>
static void opt(void)
{
  emAfLoadControlEventOptInOrOut((int8u)emberUnsignedCommandArgument(0),
                                 emberUnsignedCommandArgument(1),
                                 emberCurrentCommand->name[0] == 'i');
}

// plugin drlc print <endpoint:1>
static void print(void) 
{
  emAfLoadControlEventTablePrint((int8u)emberUnsignedCommandArgument(0));
}

// plugin drlc clear <endpoint:1>
static void clear(void)
{
  emAfLoadControlEventTableClear((int8u)emberUnsignedCommandArgument(0));

  emberAfCorePrintln("Events cleared");
}
