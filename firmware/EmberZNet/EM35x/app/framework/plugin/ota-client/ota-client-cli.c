// *******************************************************************
// * ota-client-cli.c
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading specific file. 
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-common/ota-cli.h"
#include "app/framework/plugin/ota-client/ota-client.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"

#include "app/framework/plugin/ota-client/ota-client-signature-verify.h"
#include "app/framework/plugin/ota-client-policy/ota-client-policy.h"


/**
 * @addtogroup cli
 * @{
 */
/**
 * @brief OTA Client CLI commands. Used to drive the OTA client.
 *        <b>plugin ota-client bootload &lt;index&gt;</b>
 *        - <i> bootloads the image at the specified index by calling the OTA
 *              bootload callback.
 *           - index - int8u. The index at which to bootload the image.</i>
 *
 *        <b>plugin ota-client verify &lt;index&gt;</b>
 *        - <i> Performs signature verification on the image at the specified
 *              index.
 *           - index - int8u. The index of the image to be verified.</i>
 *
 *        <b>plugin ota-client info</b>
 *        - <i>Prints the manufacturer ID, Image Type ID, and Version information
 *             that are used when a query next image is sent to the server by the
 *             client.</i>
 *
 *        <b>plugin ota-client start</b>
 *        - <i>Starts the ota client state machine. The state machine discovers
 *             the OTA server, queries for new images, downloads the images
 *             and waits for the server command to upgrade.</i>
 *
 *        <b>plugin ota-client stop</b>
 *        - <i>Stops the OTA state machine.</i>
 *
 *        <b>plugin ota-client status</b>
 *        - <i>Prints information on the current state of the OTA client
 *             download.</i>
 *
 *        <b>plugin ota-client block-test</b>
 *        - <i></i>
 *
 *        <b>plugin ota-client page-request</b>
 *        - <i></i>
 *
 */
#define EMBER_AF_DOXYGEN_CLI__OTA_CLIENT_COMMANDS
/** @} END addtogroup */

#ifdef EMBER_AF_PLUGIN_OTA_CLIENT

//------------------------------------------------------------------------------
// Forward Declarations

static void otaCliBootload(void);
static void otaPrintClientInfo(void);
static void otaCliVerify(void);
static void otaStartClientCommand(void);
static void otaStopClientCommand(void);
static void setPageRequest(void);

#if defined(EMBER_TEST)
static void setPausePercentage(void);
#endif

#if defined(EMBER_TEST)
#define PAUSE_AT_COMMAND emberCommandEntryAction("pause-at",     setPausePercentage, "u" ,""), 
#else
#define PAUSE_AT_COMMAND
#endif

#define OTA_CLIENT_COMMANDS  \
  emberCommandEntryAction("bootload",     otaCliBootload, "u", ""), \
  emberCommandEntryAction("verify",       otaCliVerify,   "u", ""), \
  emberCommandEntryAction("info",         otaPrintClientInfo,    "", ""), \
  emberCommandEntryAction("start",        otaStartClientCommand, "", ""), \
  emberCommandEntryAction("stop",         otaStopClientCommand,  "", ""), \
  emberCommandEntryAction("status",       emAfOtaClientPrintState, "", ""), \
  emberCommandEntryAction("block-test",   emAfSendImageBlockRequestTest, "", ""), \
  emberCommandEntryAction("page-request", setPageRequest, "u", ""), \
  PAUSE_AT_COMMAND \
  emberCommandEntryTerminator(), 

//------------------------------------------------------------------------------
// Globals
EmberCommandEntry emberAfPluginOtaClientCommands[] = {

  OTA_CLIENT_COMMANDS

  emberCommandEntryTerminator(),
};

//------------------------------------------------------------------------------
// Functions

static void otaStartStopClientCommand(boolean starting)
{
  emberAfCorePrintln("%p" "ing OTA client state machine",
                     starting ? "start" : "stopp");
  if (starting) {
    emberAfOtaClientStartCallback(); 
  } else {
    emAfOtaClientStop();
  }
}

static void otaStartClientCommand(void)
{
  otaStartStopClientCommand(TRUE);
}

static void otaStopClientCommand(void)
{
  otaStartStopClientCommand(FALSE);
}

static void otaCliBootload(void)
{
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  EmberAfOtaImageId id = emAfOtaFindImageIdByIndex(index);
  if (!emberAfIsOtaImageIdValid(&id)) {
    otaPrintln("Error: No image at index %d", index);
    return;
  }
  emberAfOtaClientBootloadCallback(&id);
}

static void otaCliVerify(void)
{
#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT)
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  EmberAfOtaImageId id = emAfOtaFindImageIdByIndex(index);
  if (!emberAfIsOtaImageIdValid(&id)) {
    otaPrintln("Error: No image at index %d", index);
    return;
  }
  emAfOtaImageSignatureVerify(0,       // max number of hash calculations 
                              &id,     //   (0 = keep going until hashing is done)
                              TRUE);   // new verification?
#else
  otaPrintln("Not supported.");
#endif
}


static void otaPrintClientInfo(void)
{
  EmberAfOtaImageId myId;
  int16u hardwareVersion;
  emberAfOtaClientVersionInfoCallback(&myId, &hardwareVersion);
  otaPrintln("Client image query info");
  otaPrintln("Manuf ID:         0x%2X", myId.manufacturerId);
  otaPrintln("Image Type ID:    0x%2X", myId.imageTypeId);
  otaPrintln("Current Version:  0x%4X", myId.firmwareVersion);
  emberAfOtaBootloadClusterPrint(  "Hardware Version: ");
  if (hardwareVersion != EMBER_AF_INVALID_HARDWARE_VERSION) {
    otaPrintln("0x%2X", hardwareVersion);
  } else {
    otaPrintln("NA");
  }
  emberAfCoreFlush();

  otaPrintln("Query Delay ms:            %l", (int32u)EMBER_AF_OTA_QUERY_DELAY_MS);
  emberAfCoreFlush();
  otaPrintln("Server Discovery Delay ms: %l", (int32u)EMBER_AF_OTA_SERVER_DISCOVERY_DELAY_MS);
  otaPrintln("Download Delay ms:         %l", (int32u)EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS);
  otaPrintln("Run Upgrade Delay ms:      %l", (int32u)EMBER_AF_RUN_UPGRADE_REQUEST_DELAY_MS);
  emberAfCoreFlush();
  otaPrintln("Verify Delay ms:           %l", (int32u)EMBER_AF_PLUGIN_OTA_CLIENT_VERIFY_DELAY_MS);
  otaPrintln("Download Error Threshold:  %d",    EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_ERROR_THRESHOLD);
  otaPrintln("Upgrade Wait Threshold:    %d",    EMBER_AF_PLUGIN_OTA_CLIENT_UPGRADE_WAIT_THRESHOLD);
  emberAfCoreFlush();

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_USE_PAGE_REQUEST)
  otaPrintln("Use Page Request: %s", emAfUsingPageRequest() ? "yes": "no");
  otaPrintln("Page Request Size: %d bytes", 
             EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE);
  otaPrintln("Page Request Timeout: %d sec.", 
             EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_TIMEOUT_SECONDS);
#endif

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT)
  otaPrintln("");
  emAfOtaClientSignatureVerifyPrintSigners();
#endif
}

static void setPageRequest(void)
{
  boolean pageRequest = (boolean)emberUnsignedCommandArgument(0);
  emAfSetPageRequest(pageRequest);
}

#if defined(EMBER_TEST)
static void setPausePercentage(void)
{
  emAfOtaClientStopDownloadPercentage = (int8u)emberUnsignedCommandArgument(0);
}
#endif

#endif //EMBER_AF_PLUGIN_OTA_CLIENT
