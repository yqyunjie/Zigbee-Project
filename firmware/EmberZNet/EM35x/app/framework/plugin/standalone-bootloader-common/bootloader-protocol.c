// *****************************************************************************
// * bootloader-protocol.c
// *
// * This file defines the proprietary Ember standalone bootloader messages.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "bootloader-protocol.h"

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------
// Functions

boolean emberAfPluginStandaloneBootloaderCommonCheckIncomingMessage(int8u length,
                                                                   int8u* message)
{
  // This is assumed to be an Ember bootload message.  In other words,
  // the MAC data payload.
  if (length < BOOTLOAD_MESSAGE_OVERHEAD) {
    bootloadPrintln("Error: Got short bootload message, length: %d", length);
    return FALSE;
  }

  if (message[OFFSET_VERSION] != BOOTLOAD_PROTOCOL_VERSION) {
    bootloadPrintln("Error: Protocol version in bootload message (%d) does not match mine (%d).", 
                    message[OFFSET_VERSION],
                    BOOTLOAD_PROTOCOL_VERSION);
    return FALSE;
  }
  
  return TRUE;
}

EmberStatus emberAfPluginStandaloneBootloaderCommonSendMessage(boolean isBroadcast,
                                                               EmberEUI64 targetEui,
                                                               int8u length,
                                                               int8u* message)
{
  EmberStatus status = emAfSendBootloadMessage(isBroadcast,
                                               targetEui,
                                               length,
                                               message);
  if (EMBER_SUCCESS != status) {
    bootloadPrintln("Failed to send bootload message type: 0x%X, status: 0x%X", 
                    message[1], 
                    status);
  }
  return status;
}

int8u emberAfPluginStandaloneBootloaderCommonMakeHeader(int8u *message, int8u type)
{
  //common header values
  message[0] = BOOTLOAD_PROTOCOL_VERSION;
  message[1] = type;

  // for XMODEM_QUERY and XMODEM_EOT messages, this represents the end of the
  // header.  However, for XMODEM_QRESP, XMODEM_SOH, XMODEM_ACK, XMODEM_NAK
  // messages, there are additional values that need to be added.
  // Note that the application will not have to handle creation of 
  // over the air XMODEM_ACK and XMODEM_NAK since these are all handled by 
  // the bootloader on the target node.

  return 2;
}

// Make sure we have a NULL delemiter and ignore 0xFF characters
void emAfStandaloneBootloaderCommonPrintHardwareTag(int8u* text)
{
  int8u hardwareTagString[EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH + 1];
  int8u i;
  MEMSET(hardwareTagString, 0, EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH + 1);
  for (i = 0; i < EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH; i++) {
    if (text[i] == 0xFF) {
      // Last
      i = EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH;
    } else {
      hardwareTagString[i] = text[i];
    }
  }
  bootloadPrintln("%s", hardwareTagString);
}
