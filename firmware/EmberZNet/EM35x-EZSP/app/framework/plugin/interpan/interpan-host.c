// *******************************************************************
// * interpan-host.c
// *
// * Host-specific code related to the reception and processing of interpan
// * messages.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "interpan.h"
#include "app/framework/util/af-main.h"

//------------------------------------------------------------------------------

void ezspMacFilterMatchMessageHandler(int8u filterIndexMatch,
                                      EmberMacPassthroughType legacyPassthroughType,
                                      int8u lastHopLqi,
                                      int8s lastHopRssi,
                                      int8u messageLength,
                                      int8u *messageContents)
{
  emAfPluginInterpanProcessMessage(messageLength,
                                   messageContents);
}

EmberStatus emAfPluginInterpanSendRawMessage(int8u length, int8u* message)
{
  return ezspSendRawMessage(length, message);
}

void emberAfPluginInterpanInitCallback(void)
{
}

void emberAfPluginInterpanNcpInitCallback(boolean memoryAllocation)
{
  EmberMacFilterMatchData filters[] = {
    EMBER_AF_PLUGIN_INTERPAN_FILTER_LIST
  };
  EzspStatus status;

  if (memoryAllocation) {
    status = ezspSetConfigurationValue(EZSP_CONFIG_MAC_FILTER_TABLE_SIZE,
                                       (sizeof(filters)
                                        / sizeof(EmberMacFilterMatchData)));
    if (status != EZSP_SUCCESS) {
      emberAfAppPrintln("%p%p failed 0x%x",
                        "Error: ",
                        "Sizing MAC filter table",
                        status);
      return;
    }
  } else {
    int8u value[2 * sizeof(filters) / sizeof(EmberMacFilterMatchData)];
    int8u i;
    for (i = 0; i < sizeof(value) / 2; i++) {
      value[i * 2]     =  LOW_BYTE(filters[i]);
      value[i * 2 + 1] = HIGH_BYTE(filters[i]);
    }
    status = ezspSetValue(EZSP_VALUE_MAC_FILTER_LIST, sizeof(value), value);
    if (status != EZSP_SUCCESS) {
      emberAfAppPrintln("%p%p failed 0x%x",
                        "Error: ",
                        "Setting MAC filter",
                        status);
      return;
    }
  }
}

EmberStatus emAfInterpanApsCryptMessage(boolean encrypt,
                                        int8u* message,
                                        int8u* messageLength,
                                        int8u apsHeaderEndIndex,
                                        EmberEUI64 remoteEui64)
{
#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_APS_ENCRYPTED_MESSAGES)
  #error Not supported by EZSP
#endif

  // Feature not yet supported on EZSP.
  return EMBER_LIBRARY_NOT_PRESENT;
}
