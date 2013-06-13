// *****************************************************************************
// * address-table-host.c
// *
// * This code provides support for managing the address table for the HOST.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "address-table-management.h"

#define FREE_EUI64 {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

static EmberEUI64 addressTable[EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE];
boolean initPending = TRUE;

void emberAfPluginAddressTableInitCallback(void)
{
}

void emberAfPluginAddressTableNcpInitCallback(boolean memoryAllocation)
{
  int8u index;
  EmberEUI64 freeEui = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  if (memoryAllocation)
    return;

  if (initPending) {
    // Initialize all the entries to all 0xFFs. All 0xFFs means that the entry
    // is unused.
    MEMSET(addressTable, 0xFF, EUI64_SIZE * EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE);
    initPending = FALSE;
    return;
  }

  // When the NCP resets the address table at the NCP is empty (it is only
  // stored in RAM). We re-add all the non-empty entries at the NCP.
  for(index=0; index<EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; index++) {
    if (MEMCOMPARE(addressTable[index], freeEui, EUI64_SIZE) != 0) {
      if (emberSetAddressTableRemoteEui64(index, addressTable[index])
          != EMBER_SUCCESS)
        assert(0);  // We expect the host and the NCP table to always match, so
                    // we should always be able to add an entry at the NCP here.
    }
  }
}

int8u emberAfPluginAddressTableAddEntry(EmberEUI64 entry)
{
  int8u index = emberAfPluginAddressTableLookupByEui64(entry);

  if (index != EMBER_NULL_ADDRESS_TABLE_INDEX)
    return index;

  for(index=0; index<EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; index++) {
    EmberEUI64 freeEui = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    if (MEMCOMPARE(addressTable[index], freeEui, EUI64_SIZE) == 0) {
      MEMCOPY(addressTable[index], entry, EUI64_SIZE);
      // Set the corresponding entry at the NCP
      if (emberSetAddressTableRemoteEui64(index, entry) != EMBER_SUCCESS)
        assert(0);  // We expect the host and the NCP table to always match, so
                    // we should always be able to add an entry at the NCP here.
      return index;
    }
  }

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

EmberStatus emberAfPluginAddressTableRemoveEntry(EmberEUI64 entry)
{
  int8u index = emberAfPluginAddressTableLookupByEui64(entry);

  if (index == EMBER_NULL_ADDRESS_TABLE_INDEX)
    return EMBER_INVALID_CALL;

  MEMSET(addressTable[index], 0xFF, EUI64_SIZE);

  // Delete the entry at the NCP
  emberSetAddressTableRemoteNodeId(index, EMBER_TABLE_ENTRY_UNUSED_NODE_ID);

  return EMBER_SUCCESS;
}

int8u emberAfPluginAddressTableLookupByEui64(EmberEUI64 entry)
{
  int8u index;
  for(index=0; index<EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; index++) {
    if (MEMCOMPARE(addressTable[index], entry, EUI64_SIZE) == 0)
      return index;
  }

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

EmberStatus emberAfPluginAddressTableLookupByIndex(int8u index,
                                                   EmberEUI64 entry)
{
  if (index < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE) {
    MEMCOPY(entry, addressTable[index], EUI64_SIZE);
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}
