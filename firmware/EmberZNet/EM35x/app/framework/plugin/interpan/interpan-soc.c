// *******************************************************************
// * interpan-soc.c
// *
// * SOC-specific code related to the reception and processing of interpan
// * messages.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "interpan.h"
#include "app/framework/util/af-main.h"

static const EmberMacFilterMatchData filters[] = {
  EMBER_AF_PLUGIN_INTERPAN_FILTER_LIST
  (EMBER_MAC_FILTER_MATCH_END), // terminator
};

//------------------------------------------------------------------------------

void emberMacFilterMatchMessageHandler(const EmberMacFilterMatchStruct *macFilterMatchStruct)
{
  int8u data[EMBER_AF_MAXIMUM_INTERPAN_LENGTH];
  int8u length;

  length = emAfCopyMessageIntoRamBuffer(macFilterMatchStruct->message,
                                        data,
                                        EMBER_AF_MAXIMUM_INTERPAN_LENGTH);
  if (length == 0) {
    return;
  }
  
  emAfPluginInterpanProcessMessage(length,
                                   data);
}

EmberStatus emAfPluginInterpanSendRawMessage(int8u length, int8u* message)
{
  EmberStatus status;
  EmberMessageBuffer buffer = emberFillLinkedBuffers(message, length);
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    return EMBER_NO_BUFFERS;
  }

  status = emberSendRawMessage(buffer);
  emberReleaseMessageBuffer(buffer);
  return status;
}

void emberAfPluginInterpanInitCallback(void)
{
  EmberStatus status = emberSetMacFilterMatchList(filters);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Setting MAC filter",
                      status);
  }
}

// Because the stack only handles message buffers we must convert
// the message into buffers before passing it to the stack.  Then
// we must copy the message back into the flat array afterwards.

// NOTE:  It is expected that when encrypting, the message buffer
// pointed to by *apsFrame is big enough to hold additional
// space for the Auxiliary security header and the MIC.

EmberStatus emAfInterpanApsCryptMessage(boolean encrypt,
                                        int8u* apsFrame,
                                        int8u* messageLength,
                                        int8u apsHeaderLength,
                                        EmberEUI64 remoteEui64)
{
  EmberStatus status = EMBER_LIBRARY_NOT_PRESENT;

#if defined(EMBER_AF_PLUGIN_INTERPAN_ALLOW_APS_ENCRYPTED_MESSAGES)

  EmberMessageBuffer buffer = emberFillLinkedBuffers(apsFrame,
                                                     *messageLength);
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    return EMBER_NO_BUFFERS;
  }

  status = emberApsCryptMessage(encrypt,
                                buffer,
                                apsHeaderLength,
                                remoteEui64);
  if (status == EMBER_SUCCESS) {
    // It is expected that when encrypting, the message is big enough to hold 
    // the additional data (AUX header and MIC)
    // Decrypting will shrink the message, removing the AUX header and MIC.
    emberCopyFromLinkedBuffers(buffer, 
                               0, 
                               apsFrame, 
                               emberMessageBufferLength(buffer));
    *messageLength = emberMessageBufferLength(buffer);
  }
  emberReleaseMessageBuffer(buffer);

#endif

  return status;
}
