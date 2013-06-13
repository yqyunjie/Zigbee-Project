// *****************************************************************************
// * address-table-cli.c
// *
// * This code provides support for managing the address table.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

#include "address-table-management.h"

static void addCommand(void);
static void removeCommand(void);
static void lookupCommand(void);

EmberCommandEntry emberAfPluginAddressTableCommands[] = {
  emberCommandEntryAction("add", addCommand,  "b", "Add an entry to the address table."),
  emberCommandEntryAction("remove", removeCommand,  "b", "Remove an entry from the address table."),
  emberCommandEntryAction("lookup", lookupCommand,  "b", "Search for an entry in the address table."),
  emberCommandEntryTerminator(),
};

static void addCommand(void)
{
  int8u index;
  EmberEUI64 entry;
  emberCopyEui64Argument(0, entry);

  index = emberAfPluginAddressTableAddEntry(entry);

  if (index == EMBER_NULL_ADDRESS_TABLE_INDEX) {
    emberAfCorePrintln("Table full, entry not added");
  } else {
    emberAfCorePrintln("Entry added at position 0x%x", index);
  }
}

static void removeCommand(void)
{
  EmberStatus status;
  EmberEUI64 entry;
  emberCopyEui64Argument(0, entry);

  status = emberAfPluginAddressTableRemoveEntry(entry);

  if (status == EMBER_SUCCESS) {
    emberAfCorePrintln("Entry removed");
  } else {
    emberAfCorePrintln("Entry removal failed");
  }
}

static void lookupCommand(void)
{
  int8u index;
  EmberEUI64 entry;
  emberCopyEui64Argument(0, entry);
  index = emberAfPluginAddressTableLookupByEui64(entry);

  if (index == EMBER_NULL_ADDRESS_TABLE_INDEX)
    emberAfCorePrintln("Entry not found");
  else
    emberAfCorePrintln("Found entry at position 0x%x", index);
}

