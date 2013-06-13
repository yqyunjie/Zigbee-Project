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
// External Declarations

void emAesEncrypt(int8u *block, int8u *key);

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------
// Functions



EmberStatus emAfSendBootloadMessage(boolean isBroadcast,
                                    EmberEUI64 destEui64,
                                    int8u length,
                                    int8u* message)
{
  EmberStatus status;
  EmberMessageBuffer buffer = emberFillLinkedBuffers(message,
                                                     length);
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    return EMBER_NO_BUFFERS;
  }

  status = emberSendBootloadMessage(isBroadcast, destEui64, buffer);
  emberReleaseMessageBuffer(buffer);
  return status;
}


void emberIncomingBootloadMessageHandler(EmberEUI64 longId, 
                                         EmberMessageBuffer message)
{
  int8u incomingBlock[MAX_BOOTLOAD_MESSAGE_SIZE];
  int8u length = emberMessageBufferLength(message);
  if (length > MAX_BOOTLOAD_MESSAGE_SIZE) {
    bootloadPrintln("Bootload message too long (%d > %d), dropping!", 
                    length, 
                    MAX_BOOTLOAD_MESSAGE_SIZE);
    return;
  }
  emberCopyFromLinkedBuffers(message, 
                             0,       // start index
                             incomingBlock, 
                             length);

  emberAfPluginStandaloneBootloaderCommonIncomingMessageCallback(longId,
                                                                 length,
                                                                 incomingBlock);
}


void emberBootloadTransmitCompleteHandler(EmberMessageBuffer message,
                                          EmberStatus status)
{
  if (status != EMBER_SUCCESS) {
    int8u commandId = 0xFF;
    if (emberMessageBufferLength(message) >= 2) {
      commandId = emberGetLinkedBuffersByte(message, 1);
    }
    bootloadPrintln("Bootload message (0x%X) send failed: 0x%X", 
                    commandId,
                    status);
  }
}

void emAfStandaloneBootloaderClientEncrypt(int8u* block, int8u* key)
{
  emAesEncrypt(block, key);
}
