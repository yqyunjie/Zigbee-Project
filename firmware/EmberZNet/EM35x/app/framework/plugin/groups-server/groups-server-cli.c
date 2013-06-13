// *******************************************************************
// * groups-server-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

static void print(void);

EmberCommandEntry emberAfPluginGroupsServerCommands[] = {
  emberCommandEntryAction("print",  print, "", "Print the state of the groups table."),
  emberCommandEntryTerminator(),
};

// plugin groups-server print
static void print(void) 
{
  int8u i;
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry entry;
    emberGetBinding(i, &entry);
    if (entry.type == EMBER_MULTICAST_BINDING) {
      emberAfCorePrintln("ep[%x] id[%2x]", entry.local, 
                         HIGH_LOW_TO_INT(entry.identifier[1], entry.identifier[0]));
    }
  }
}
