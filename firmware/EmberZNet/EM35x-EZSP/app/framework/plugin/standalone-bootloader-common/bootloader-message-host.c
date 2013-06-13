// *****************************************************************************
// * bootloader-message-host.c
// *
// * This file defines the interface to the host to send Ember proprietary 
// * bootloader messages.
// * 
// * Copyright 2012 by Silicon Labs. All rights reserved.                   *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "bootloader-protocol.h"
#include "standalone-bootloader-common-callback.h"

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------
// Functions


EmberStatus emAfSendBootloadMessage(boolean isBroadcast,
                                    EmberEUI64 destEui64,
                                    int8u length,
                                    int8u* message)
{
  return ezspSendBootloadMessage(isBroadcast,
                                 destEui64,
                                 length,
                                 message);
}

void ezspIncomingBootloadMessageHandler(EmberEUI64 longId,
                                        int8u lastHopLqi,
                                        int8s lastHopRssi,
                                        int8u messageLength,
                                        int8u* messageContents)
{
  if (messageLength > MAX_BOOTLOAD_MESSAGE_SIZE) {
    bootloadPrintln("Bootload message too long (%d > %d), dropping!", 
                    messageLength, 
                    MAX_BOOTLOAD_MESSAGE_SIZE);
    return;
  }

  emberAfPluginStandaloneBootloaderCommonIncomingMessageCallback(longId,
                                                                 messageLength,
                                                                 messageContents);
}
                                        

void ezspBootloadTransmitCompleteHandler(EmberStatus status,
                                         int8u messageLength,
                                         int8u *messageContents)
{
  if (status != EMBER_SUCCESS) {
    int8u commandId = 0xFF;
    if (messageLength >= 2) {
      commandId = messageContents[1];
    }
    bootloadPrintln("Bootload message (0x%X) send failed: 0x%X", 
                    commandId,
                    status);
  }
}

void emAfStandaloneBootloaderClientEncrypt(int8u* block, int8u* key)
{
  int8u temp[EMBER_ENCRYPTION_KEY_SIZE];
  ezspAesEncrypt(block, key, temp);
  MEMCOPY(block, temp, EMBER_ENCRYPTION_KEY_SIZE);
}
