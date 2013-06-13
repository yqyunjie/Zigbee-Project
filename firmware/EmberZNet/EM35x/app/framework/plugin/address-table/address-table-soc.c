// *****************************************************************************
// * address-table-soc.c
// *
// * This code provides support for managing the address table.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "address-table-management.h"

void emberAfPluginAddressTableInitCallback(void)
{
}

int8u emberAfPluginAddressTableAddEntry(EmberEUI64 entry)
{
  int8u index = emberAfPluginAddressTableLookupByEui64(entry);

  if (index != EMBER_NULL_ADDRESS_TABLE_INDEX)
    return index;

  for(index=0; index<EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; index++){
    if (emberGetAddressTableRemoteNodeId(index)
        == EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      emberSetAddressTableRemoteEui64(index, entry);
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

  emberSetAddressTableRemoteNodeId(index, EMBER_TABLE_ENTRY_UNUSED_NODE_ID);

  return EMBER_SUCCESS;
}

int8u emberAfPluginAddressTableLookupByEui64(EmberEUI64 entry)
{
  int8u index;
  for(index=0; index<EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; index++) {
    EmberEUI64 temp;
    emberGetAddressTableRemoteEui64(index, temp);
    if (MEMCOMPARE(entry, temp, EUI64_SIZE) == 0
        && emberGetAddressTableRemoteNodeId(index)
           != EMBER_TABLE_ENTRY_UNUSED_NODE_ID)
      return index;
  }

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

EmberStatus emberAfPluginAddressTableLookupByIndex(int8u index,
                                                   EmberEUI64 entry)
{
  if (index < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE) {
    emberGetAddressTableRemoteEui64(index, entry);
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}

