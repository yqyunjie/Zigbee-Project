// *****************************************************************************
// * standalone-bootloader-server.c
// *
// * This file defines the server behavior for the Ember proprietary bootloader
// * protocol.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/standalone-bootloader-common/bootloader-protocol.h"
#include "standalone-bootloader-server.h"
#include "standalone-bootloader-server-callback.h"
#include "app/framework/plugin/xmodem-sender/xmodem-sender.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"

#include "app/framework/plugin/ota-common/ota.h"

//------------------------------------------------------------------------------
// Globals

static const EmberEUI64 broadcastEui64 = { 
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF 
};

static const EmberKeyData bootloadKey = {
  EMBER_AF_PLUGIN_STANDALONE_BOOTLOADER_SERVER_ENCRYPTION_KEY,
};

typedef enum {
  SERVER_BOOTLOAD_STATE_NONE = 0,
  SERVER_BOOTLOAD_STATE_UNICAST_QUERY_SENT,
  SERVER_BOOTLOAD_STATE_LAUNCH_REQUEST_SENT,
  SERVER_BOOTLOAD_STATE_DELAY_BEFORE_CHECK_IF_BOOTLOADER_ACTIVE,
  SERVER_BOOTLOAD_STATE_CHECK_IF_BOOTLOADER_ACTIVE_SENT,
  SERVER_BOOTLOAD_STATE_SENDING_DATA,
} BootloadServerState;

static  PGM_P serverStateStrings[] = {
  "None",
  "Unicast Query Sent",
  "Launch Request Sent",
  "Delay before checking if bootloader active",
  "Check if Bootloader active sent",
  "Sending Data",
};

static BootloadServerState serverBootloadState = SERVER_BOOTLOAD_STATE_NONE;

// We don't make these configurable since they could effect the ability
// to work with bootloaders already in the field.  And bootloaders
// are not updated.
#define QUERY_TIMEOUT_SECONDS 2
#define LAUNCH_REQUEST_TIMEOUT_SECONDS 2
#define DELAY_BEFORE_CHECKING_IF_BOOTLOADER_ACTIVE_SECONDS 2
#define DELAY_BEFORE_SENDING_BOOTLOAD_DATA_SECONDS 2

EmberEventControl emberAfPluginStandaloneBootloaderServerMyEventEventControl;

// We remember the last target client as an optimization.
// This way a CLI user can broadcast the query and then simply bootload the 
// lone nearby client.
// However the application may tell any specific node to bootload via its 
// long ID.  This performs a unicast query to obtain the current bootloader 
// data just in case.
static EmberAfStandaloneBootloaderQueryResponseData targetClient;

// Since the max 802.15.4 packet size is 128 bytes, we can't fit a single
// Xmodem packet plus overhead in an over-the-air packet.  Split them in half.
#define OTA_XMODEM_MESSAGE_SIZE 64

static EmberAfOtaImageId imageToBootload;
static int32u tagOffset;
static int32u tagSize;

//------------------------------------------------------------------------------
// External Declarations

//------------------------------------------------------------------------------
// Functions

void emAfStandaloneBootloaderServerPrintStatus(void)
{
  bootloadPrintln("Server State: %p", serverStateStrings[serverBootloadState]);
  bootloadPrint("Key Data: ");
  emberAfPrintZigbeeKey(emberKeyContents(&bootloadKey));
}

static void resetServerState(boolean success)
{
  emberAfPluginStandaloneBootloaderServerFinishedCallback(success);

  // We don't clear the targetClient struct so it can potentially be re-used
  // in a subsequent bootload.
  serverBootloadState = SERVER_BOOTLOAD_STATE_NONE;
  emberEventControlSetInactive(emberAfPluginStandaloneBootloaderServerMyEventEventControl);
}

static boolean isTargetClientDataValid(void)
{
  return (0 != MEMCOMPARE(broadcastEui64, targetClient.eui64, EUI64_SIZE));
}

void emberAfPluginStandaloneBootloaderServerInitCallback(void)
{
  MEMSET(&targetClient, 0xFF, sizeof(EmberAfStandaloneBootloaderQueryResponseData));
}

static EmberStatus sendQuery(const EmberEUI64 targetEui)
{
  EmberEUI64 eui64Copy;
  int8u outgoingBlock[MAX_BOOTLOAD_MESSAGE_SIZE];
  int8u length = emberAfPluginStandaloneBootloaderCommonMakeHeader(outgoingBlock, XMODEM_QUERY);
  boolean isBroadcast = (0 == MEMCOMPARE(broadcastEui64, targetEui, EUI64_SIZE));

  // We need to cast away the const to avoid compiler warning.
  MEMCOPY(eui64Copy, targetEui, EUI64_SIZE);
  
  return emberAfPluginStandaloneBootloaderCommonSendMessage(isBroadcast,
                                                            eui64Copy,
                                                            length,
                                                            outgoingBlock);
}

// Query all nearby nodes for bootload info.
EmberStatus emberAfPluginStandaloneBootloaderServerBroadcastQuery(void)
{
  return sendQuery(broadcastEui64);
}

static void decodeAndPrintServerMessageType(int8u command)
{
  const boolean printAcks = FALSE;
  int8u id = 0xFF;
  PGM_P commandStrings[] = {
    "Query Response",
    "Auth Challenge",
    "SOH",
    "EOT",
    "ACK",
    "NAK",
    "CANCEL",
    "BLOCK_OK",
    "FILEDONE",
  };

  switch (command) {
  case (XMODEM_QRESP): 
    id = 0;
    break;
  case (XMODEM_AUTH_CHALLENGE): 
    id = 1;
    break;
  case (XMODEM_SOH):
    id = 2;
    break;
  case (XMODEM_EOT):
    id = 3;
    break;
  case (XMODEM_ACK):
    id = 4;
    break;
  case (XMODEM_NAK):
    id = 5;
    break;
  case (XMODEM_CANCEL):
    id = 6;
    break;
  case (XMODEM_BLOCKOK):
    id = 7;
    break;
  case (XMODEM_FILEDONE):
    id = 8;
    break;

  default:
    break;
  }
  if (printAcks || command != XMODEM_ACK) {
    bootloadPrintln("Received Standalone Bootloader message (%d): %p",
                    command,
                    (id == 0xFF
                     ? "??"
                     : commandStrings[id]));
  }
}

static void printTargetClientInfo(PGM_P prefixString)
{
  bootloadPrint("%p", prefixString);
  if (isTargetClientDataValid()) {
    emberAfPrintBigEndianEui64(targetClient.eui64);
    bootloadPrintln("\n  Bootloader Active: %p", 
                    (targetClient.bootloaderActive
                     ? "yes"
                     : "no"));
    bootloadPrintln("  MFG ID:       0x%2X", targetClient.mfgId);
    bootloadPrint("  Hardware Tag: ");
    emAfStandaloneBootloaderCommonPrintHardwareTag(targetClient.hardwareTag);
    bootloadPrintln("  Capabilities: 0x%X",  targetClient.capabilities);
    bootloadPrintln("  Platform:     0x%X",  targetClient.platform);
    bootloadPrintln("  Micro:        0x%X",  targetClient.micro);
    bootloadPrintln("  Phy:          0x%X",  targetClient.phy);
  } else {
    bootloadPrintln("-");
  }
}

void emAfStandaloneBootloaderServerPrintTargetClientInfoCommand(void)
{
  printTargetClientInfo("Current target client: ");
}

static void sendLaunchRequest(void)
{
  int8u outgoingBlock[LAUNCH_REQUEST_LENGTH];
  int8u index = emberAfPluginStandaloneBootloaderCommonMakeHeader(outgoingBlock, XMODEM_LAUNCH_REQUEST);
  EmberStatus status;
  bootloadPrintln("Sending launch request to current target");
  outgoingBlock[index++] = LOW_BYTE(targetClient.mfgId);
  outgoingBlock[index++] = HIGH_BYTE(targetClient.mfgId);
  MEMCOPY(&(outgoingBlock[index]), 
          targetClient.hardwareTag, 
          EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH);
  index += EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH;

  status = emberAfPluginStandaloneBootloaderCommonSendMessage(FALSE, // is broadcast?
                                                              targetClient.eui64,
                                                              index,
                                                              outgoingBlock);
  if (status == EMBER_SUCCESS) {
    serverBootloadState = SERVER_BOOTLOAD_STATE_LAUNCH_REQUEST_SENT;
    emberEventControlSetDelayQS(emberAfPluginStandaloneBootloaderServerMyEventEventControl,
                                LAUNCH_REQUEST_TIMEOUT_SECONDS << 2);
  } else {
    resetServerState(FALSE);
  }
}

static EmberStatus xmodemSendRoutine(int8u* data, int8u length)
{
  int8u outgoingBlock[MAX_BOOTLOAD_MESSAGE_SIZE];
  int8u index = emberAfPluginStandaloneBootloaderCommonMakeHeader(outgoingBlock, 
                                                                  1);   // message type (ignored)
  
  // We ignore the type of message that we passed in the above function.  
  // The Xmodem plugin already formatted the entire buffer for us.
  index--;

  MEMCOPY(&(outgoingBlock[index]), data, length);
  index += length;
  return emberAfPluginStandaloneBootloaderCommonSendMessage(FALSE,
                                                            targetClient.eui64,
                                                            index,
                                                            outgoingBlock);
}

static EmberStatus getNextBootloaderBlock(int32u address, // relative to 0
                                          int8u length,
                                          int8u* returnData,
                                          int8u* returnLength,
                                          boolean* done)
{
  int32u realReturnLength;
  EmberAfOtaStorageStatus status;

  if (address == 0) {
    emAfPrintPercentageSetStartAndEnd(0,    // starting offset
                                      tagSize); // end offset
  }
  emAfPrintPercentageUpdate("Xmodem Transfer", 
                            5, // update frequency (e.g. every 5%)
                            address);

  status = 
    emberAfOtaStorageReadImageDataCallback(&imageToBootload,
                                           address + tagOffset,
                                           length,
                                           returnData,
                                           &realReturnLength);
  *returnLength = (int8u)realReturnLength;
  if (address + length > tagSize) {
    *done = TRUE;
  } else {
    *done = FALSE;
  }

  return (status == EMBER_AF_OTA_STORAGE_SUCCESS
          ? EMBER_SUCCESS
          : EMBER_ERR_FATAL);
}

static void xmodemComplete(boolean success)
{
  bootloadPrintln("Xmodem transfer complete, status: %p", (success ? "Success" : "FAILED"));
  resetServerState(success);
}

static void startSendingXmodemData(void)
{
  emberEventControlSetInactive(emberAfPluginStandaloneBootloaderServerMyEventEventControl);
  serverBootloadState = SERVER_BOOTLOAD_STATE_SENDING_DATA;
  if (EMBER_SUCCESS != emberAfPluginXmodemSenderStart(xmodemSendRoutine,
                                                      getNextBootloaderBlock,
                                                      xmodemComplete,
                                                      OTA_XMODEM_MESSAGE_SIZE,
                                                      FALSE)) { // wait for ready char?
    bootloadPrintln("Error: Could not start Xmodem transfer.  Aborting.");
    resetServerState(FALSE);
  }
}

// Passed message includes overhead bytes
static void processQueryResponse(EmberEUI64 longId,
                                 int8u* message)
{
  MEMSET(&(targetClient), 0, sizeof(EmberAfStandaloneBootloaderQueryResponseData));
  MEMCOPY(targetClient.eui64, longId, EUI64_SIZE);
  targetClient.bootloaderActive = message[QRESP_OFFSET_BL_ACTIVE];
  targetClient.mfgId = HIGH_LOW_TO_INT(message[QRESP_OFFSET_MFG_ID],
                                       message[QRESP_OFFSET_MFG_ID+1]);
  MEMCOPY(targetClient.hardwareTag, 
          &(message[QRESP_OFFSET_HARDWARE_TAG]), 
          EMBER_AF_STANDALONE_BOOTLOADER_HARDWARE_TAG_LENGTH);
  targetClient.capabilities = message[QRESP_OFFSET_BL_CAPS];
  targetClient.platform = message[QRESP_OFFSET_PLATFORM];
  targetClient.micro = message[QRESP_OFFSET_MICRO];
  targetClient.phy = message[QRESP_OFFSET_PHY];
  targetClient.bootloaderVersion = HIGH_LOW_TO_INT(message[QRESP_OFFSET_BL_VERSION],
                                                message[QRESP_OFFSET_BL_VERSION + 1]);

  printTargetClientInfo("Received Query response from: ");
  emberAfPluginStandaloneBootloaderServerQueryResponseCallback((serverBootloadState == SERVER_BOOTLOAD_STATE_NONE),  // broadcast?
                                                               &targetClient);

  if (serverBootloadState == SERVER_BOOTLOAD_STATE_UNICAST_QUERY_SENT) {
    // If the bootloader is already active in the device, skip to the chase and
    // start downloading new data to it.
    if (targetClient.bootloaderActive) {
      startSendingXmodemData();
    } else {
      sendLaunchRequest();
    }
  } else if (serverBootloadState == SERVER_BOOTLOAD_STATE_CHECK_IF_BOOTLOADER_ACTIVE_SENT) {
    if (targetClient.bootloaderActive) {
      startSendingXmodemData();
    } else {
      bootloadPrintln("Error: Bootloader of client not active.  Aborting.");
      resetServerState(FALSE);
    }
  }
}

static void processChallenge(EmberEUI64 longId,
                             int8u* message)
{
  int8u outgoingBlock[XMODEM_AUTH_RESPONSE_LENGTH];
  EmberStatus status;
  int8u index = emberAfPluginStandaloneBootloaderCommonMakeHeader(outgoingBlock,
                                                                  XMODEM_AUTH_RESPONSE);

  if (message[XMODEM_AUTH_CHALLENGE_REQUEST_INDEX] != CHALLENGE_REQUEST_VERSION) {
    bootloadPrintln("Error: Challenge request version (%d) not the same as mine (%d)",
                    message[XMODEM_AUTH_CHALLENGE_REQUEST_INDEX],
                    CHALLENGE_REQUEST_VERSION);
    resetServerState(FALSE);
    return;
  }

  MEMCOPY(&(outgoingBlock[index]), 
          &(message[BOOTLOAD_MESSAGE_OVERHEAD]),
          BOOTLOAD_AUTH_CHALLENGE_SIZE);

  emAfStandaloneBootloaderClientEncrypt(&(outgoingBlock[index]),
                                        (int8u*)emberKeyContents(&bootloadKey));

  index += BOOTLOAD_AUTH_RESPONSE_SIZE;
  status = emberAfPluginStandaloneBootloaderCommonSendMessage(FALSE,
                                                              longId,
                                                              index,
                                                              outgoingBlock);
  if (status == EMBER_SUCCESS) {
    serverBootloadState = SERVER_BOOTLOAD_STATE_DELAY_BEFORE_CHECK_IF_BOOTLOADER_ACTIVE;
    emberEventControlSetDelayQS(emberAfPluginStandaloneBootloaderServerMyEventEventControl,
                                DELAY_BEFORE_SENDING_BOOTLOAD_DATA_SECONDS << 2);
  } else {
    resetServerState(FALSE);
  }
}

EmberStatus emberAfPluginStandaloneBootloaderServerStartClientBootloadWithCurrentTarget(const EmberAfOtaImageId* id,
                                                                                        int16u tag)
{
  if (!isTargetClientDataValid()) {
    bootloadPrintln("Error: No current target.");
    return EMBER_ERR_FATAL;
  }

  return emberAfPluginStandaloneBootloaderServerStartClientBootload(targetClient.eui64,
                                                                    id,
                                                                    tag);
}

EmberStatus emberAfPluginStandaloneBootloaderServerStartClientBootload(EmberEUI64 longId,
                                                                       const EmberAfOtaImageId* id,
                                                                       int16u tag)
{
  EmberStatus status;

  if (serverBootloadState != SERVER_BOOTLOAD_STATE_NONE) {
    return EMBER_INVALID_CALL;
  }

  if (!emberAfIsOtaImageIdValid(id)) {
    bootloadPrintln("Error: Passed Image ID is invalid");
    return EMBER_ERR_FATAL;
  }
  if (0 == emberAfOtaStorageGetTotalImageSizeCallback(id)) {
    bootloadPrintln("Error:  Cannot find image ID within storage.");
    return EMBER_INVALID_CALL;
  }
  if (EMBER_AF_OTA_STORAGE_SUCCESS != emAfOtaStorageGetTagOffsetAndSize(id,
                                                                        tag,
                                                                        &tagOffset,
                                                                        &tagSize)) {
    bootloadPrintln("Error: Cannot find tag 0x%2X within passed image ID", tag);
    return EMBER_INVALID_CALL;
  }

  MEMCOPY(&imageToBootload, id, sizeof(EmberAfOtaImageId));

  status = sendQuery(longId);
  if (status == EMBER_SUCCESS) {
    serverBootloadState = SERVER_BOOTLOAD_STATE_UNICAST_QUERY_SENT;
    emberEventControlSetDelayQS(emberAfPluginStandaloneBootloaderServerMyEventEventControl,
                                QUERY_TIMEOUT_SECONDS << 2);
  } else {
    resetServerState(FALSE);
  }
  return status;
}

void emberAfPluginStandaloneBootloaderServerMyEventEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginStandaloneBootloaderServerMyEventEventControl);
  if (serverBootloadState == SERVER_BOOTLOAD_STATE_UNICAST_QUERY_SENT) {
    bootloadPrint("Error: Timed out waiting for device to respond to bootloader query");

  } else if (serverBootloadState == SERVER_BOOTLOAD_STATE_DELAY_BEFORE_CHECK_IF_BOOTLOADER_ACTIVE) {
    bootloadPrintln("Checking if bootloader is now active on client");
    if (EMBER_SUCCESS == sendQuery(targetClient.eui64)) {
      serverBootloadState = SERVER_BOOTLOAD_STATE_CHECK_IF_BOOTLOADER_ACTIVE_SENT;
      emberEventControlSetDelayQS(emberAfPluginStandaloneBootloaderServerMyEventEventControl,
                                  QUERY_TIMEOUT_SECONDS << 2);
    } else {
      bootloadPrintln("Error: Giving up bootload of client");
      resetServerState(FALSE);
    }
    return;
  }
  serverBootloadState = SERVER_BOOTLOAD_STATE_NONE;
}

static boolean validateSenderEui(EmberEUI64 longId)
{
  if (isTargetClientDataValid()) {
    return (0 == MEMCOMPARE(longId, targetClient.eui64, EUI64_SIZE));
  }

  // If target data hasn't been, then we allow anyone to send us bootloader messages.
  // With the assumption is that the server's state will prevent unwanted messages
  // from triggering real behavior.
  return TRUE;
}

boolean emberAfPluginStandaloneBootloaderCommonIncomingMessageCallback(EmberEUI64 longId,
                                                                       int8u length,
                                                                       int8u* message)
{
  if (!emberAfPluginStandaloneBootloaderCommonCheckIncomingMessage(length,
                                                                   message)) {
    return FALSE;
  }

  if (!validateSenderEui(longId)) {
    bootloadPrintln("Error: Got incoming bootload message from invalid sender");
    return FALSE;
  }

  decodeAndPrintServerMessageType(message[OFFSET_MESSAGE_TYPE]);

  switch (message[OFFSET_MESSAGE_TYPE]) {
  case (XMODEM_QRESP):{
    if (serverBootloadState <= SERVER_BOOTLOAD_STATE_UNICAST_QUERY_SENT
        || serverBootloadState == SERVER_BOOTLOAD_STATE_CHECK_IF_BOOTLOADER_ACTIVE_SENT) {
      if (length < QUERY_RESPONSE_LENGTH) {
        bootloadPrint("Error: Query response too short from: ");
        return TRUE;
      }
      processQueryResponse(longId,
                           message);
    }
    break;
  }
  case (XMODEM_AUTH_CHALLENGE): {
    if (serverBootloadState == SERVER_BOOTLOAD_STATE_LAUNCH_REQUEST_SENT) {
      if (length < XMODEM_AUTH_CHALLENGE_LENGTH) {
        bootloadPrintln("Error: Bootloader Challenge too short (%d < %d)",
                        length,
                        XMODEM_AUTH_CHALLENGE_LENGTH);
      } else {
        processChallenge(longId,
                         message);
      }
    }
    break;
  }

  default:
    if (serverBootloadState == SERVER_BOOTLOAD_STATE_SENDING_DATA) {
      emberAfPluginXmodemSenderIncomingBlock(&(message[OFFSET_MESSAGE_TYPE]), length);
    }
    break;
  }
  return TRUE;
}

