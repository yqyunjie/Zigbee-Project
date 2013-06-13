// *****************************************************************************
// * address-table-management.c
// *
// * This code provides support for managing the address table.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "address-table-management.h"

extern int8u *emAfZclBuffer;
extern int16u *emAfResponseLengthPtr;
extern EmberApsFrame *emAfCommandApsFrame;

EmberStatus emberAfSendUnicastToEui64(EmberEUI64 destination,
                                      EmberApsFrame *apsFrame,
                                      int16u messageLength,
                                      int8u *message)
{
  int8u index = emberAfPluginAddressTableLookupByEui64(destination);

  if (index == EMBER_NULL_ADDRESS_TABLE_INDEX)
    return EMBER_INVALID_CALL;

  return emberAfSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                            index,
                            apsFrame,
                            messageLength,
                            message);
}

EmberStatus emberAfSendCommandUnicastToEui64(EmberEUI64 destination)
{
  return emberAfSendUnicastToEui64(destination,
                            emAfCommandApsFrame,
                            *emAfResponseLengthPtr,
                            emAfZclBuffer);
}

EmberStatus emberAfGetCurrentSenderEui64(EmberEUI64 address)
{
  int8u index = emberAfGetAddressIndex();
  if (index == EMBER_NULL_ADDRESS_TABLE_INDEX)
    return EMBER_INVALID_CALL;
  else {
    return emberAfPluginAddressTableLookupByIndex(index, address);
  }
}
