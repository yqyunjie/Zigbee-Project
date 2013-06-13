// *****************************************************************************
// * ota-client.c
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading device specific file.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/util/util.h"
#include "app/framework/util/common.h"
#include "app/framework/plugin/ota-common/ota.h"

#include "app/framework/util/af-main.h"

#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/plugin/ota-client/ota-client.h"
#include "app/framework/plugin/ota-client-policy/ota-client-policy.h"
#include "ota-client-signature-verify.h"
#include "ota-client-page-request.h"
#include "app/framework/plugin/partner-link-key-exchange/partner-link-key-exchange.h"

#if defined(EZSP_HOST)
  // For emberIeeeAddressRequest()
  #include "app/util/zigbee-framework/zigbee-device-host.h"
#else
  #include "stack/include/ember.h"
#endif

//------------------------------------------------------------------------------
// Globals

enum BootloadState
{
  BOOTLOAD_STATE_NONE,
  BOOTLOAD_STATE_DISCOVER_SERVER,
  BOOTLOAD_STATE_GET_SERVER_EUI,
  BOOTLOAD_STATE_OBTAIN_LINK_KEY,
  BOOTLOAD_STATE_QUERY_NEXT_IMAGE,
  BOOTLOAD_STATE_DOWNLOAD,
  BOOTLOAD_STATE_VERIFY_IMAGE,
  BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE,
  BOOTLOAD_STATE_COUNTDOWN_TO_UPGRADE,
};
typedef int8u BootloadState;

static PGM_P bootloadStateNames[] = {
  "None",
  "Discovering OTA Server",
  "Get OTA Server EUI",
  "Obtain link key",
  "Querying Next Image",
  "Downloading Image",
  "Verifying Image",
  "Waiting for Upgrade message",
  "Countdown to Upgrade",
};

// This relates the bootload state above to the status that is externally
// reported via the attribute.
static PGM int8u bootloadStateToExternalState[] = {
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_DOWNLOAD_IN_PROGRESS,
  OTA_UPGRADE_STATUS_DOWNLOAD_COMPLETE,
  OTA_UPGRADE_STATUS_WAIT,
  OTA_UPGRADE_STATUS_COUNTDOWN
};

#define UNDEFINED_ENDPOINT 0xFF

static BootloadState currentBootloadState = BOOTLOAD_STATE_NONE;

static int8u myEndpoint = UNDEFINED_ENDPOINT;
static int8u serverEndpoint = UNDEFINED_ENDPOINT;
static EmberNodeId serverNodeId = EMBER_UNKNOWN_NODE_ID;
static int8u errors = 0;
static int32u totalImageSize = 0;
static EmberAfOtaImageId currentDownloadFile;
static int16u hardwareVersion;

#define WAIT_FOR_UPGRADE_MESSAGE 0xFFFFFFFF

#define ZCL_COMMAND_ID_INDEX 2

#define IMAGE_NOTIFY_NO_PAYLOAD              0
#define IMAGE_NOTIFY_MANUFACTURER_ONLY_TYPE  1
#define IMAGE_NOTIFY_MFG_AND_IMAGE_TYPE      2
#define IMAGE_NOTIFY_FULL_VERSION_TYPE       3
#define IMAGE_NOTIFY_LAST_VALID_TYPE         IMAGE_NOTIFY_FULL_VERSION_TYPE       



// These lengths correspond to the #defines above.
static PGM int8u imageNotifyPayloadLengths[] = {
  (EMBER_AF_ZCL_OVERHEAD + 2),  // Payload and Jitter only.
  (EMBER_AF_ZCL_OVERHEAD + 4),  // and MFG ID
  (EMBER_AF_ZCL_OVERHEAD + 6),  // and Image Type ID
  (EMBER_AF_ZCL_OVERHEAD + 10), // and Version
};

// Values per the spec.
#define IMAGE_BLOCK_RESPONSE_SUCCESS_MIN_LENGTH      (EMBER_AF_ZCL_OVERHEAD + 14)
#define UPGRADE_END_RESPONSE_MIN_LENGTH              (EMBER_AF_ZCL_OVERHEAD + 16)
#define QUERY_NEXT_IMAGE_SUCCESS_RESPONSE_MIN_LENGTH (EMBER_AF_ZCL_OVERHEAD + 13)

static boolean waitingForResponse = FALSE;
static int32u nextEventTimer;

#define WAITING_FOR_MESSAGE      TRUE
#define NO_MESSAGE_RESPONSE_WAIT FALSE

// Maximum amount of data that the client is willing to accept in one packet
// The server can always give us less data (sizing the response based on its
// send parameters, eg. source routing).  But it can't give us more.
// Since no 
#define MAX_CLIENT_DATA_SIZE        63  // in bytes

// We only support the Zigbbe Pro stack version.
#define STACK_VERSION ZIGBEE_PRO_STACK_VERSION

// Handy defines to make the code more readable.
#define TIMEOUT_REACHED  TRUE
#define START_NEW_TIMER  FALSE

// queryNextImage() defines
typedef enum {
  DELAY_BEFORE_SEND_QUERY = 0,
  SEND_QUERY_NOW = 1,
} SendNextQueryState;

// This defines how long to wait for a message response before considering it
// an error.  It also defines how long a sleepy stays awake waiting for
// a message.
#define MESSAGE_TIMEOUT_MS 3000L

#define IMAGE_BLOCK_ABORT_LENGTH                   (EMBER_AF_ZCL_OVERHEAD + 1)
#define IMAGE_BLOCK_RESPONSE_SUCCESS_MIN_LENGTH    (EMBER_AF_ZCL_OVERHEAD + 14)

// This is the length WITHOUT the Minimum Block Request Period
#define IMAGE_BLOCK_RESPONSE_WAIT_FOR_DATA_LENGTH_MIN  (EMBER_AF_ZCL_OVERHEAD + 8)

#define IMAGE_BLOCK_RESPONSE_WAIT_FOR_DATA_MIN_BLOCK_RESPONSE_LENGTH \
  (IMAGE_BLOCK_RESPONSE_WAIT_FOR_DATA_LENGTH_MIN + 2)

// Maximum wait time that the client would wait to retrive data or to be
// upgraded. If the server is not ready for the operation, it should tell the
// client to wait (again).  But the client would only wait up to the value 
// defined below.  Regarding upgrading, current OTA spec recommends that the
// client should query the server every 60 mins even though it is told to wait 
// longer.
#define TIMEOUT_MAX_WAIT_TIME_MS   (60 * MINUTES_IN_MS)

// If the server sent us an invalid delay time for the an image block
// download, this is how long we will delay before getting the
// next block.
#define CALCULATE_TIME_ERROR_IMAGE_BLOCK_DELAY_MS (5 * MINUTES_IN_MS)

// If the server sent us an invalid delay time for the upgrade end response,
// we use this value for the next request.
#define CALCULATE_TIME_ERROR_UPGRADE_END_RESPONSE_DELAY_MS (1 * HOURS_IN_MS)

// When the server asks us to wait indefinitely to apply an upgrade,
// we will ask them again in this much time.
#define WAIT_FOR_UPGRADE_DELAY_MS (1 * HOURS_IN_MS)

// How often a print is done indicating client download progress.
// e.g. "Download 5% complete"
#define DOWNLOAD_PERCENTAGE_UPDATE_RATE 5

#define MESSAGE_TIMEOUT_BACKOFF_MS (2 * SECONDS_IN_MS)

// Even when we are told to upgrade immediately, we want to insure there
// is a chance for the APS retries and ZCL response to get back to their 
// senders.
#define IMMEDIATE_UPGRADE_DELAY_MS (3 * SECONDS_IN_MS)

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_USE_PAGE_REQUEST)
  #define USE_PAGE_REQUEST_DEFAULT TRUE
#else
  #define USE_PAGE_REQUEST_DEFAULT FALSE
#endif

// HA 1.2 feature
static int16u minBlockRequestPeriod;

// This is not a CONST because even if the client supports page request,
// the server may not.  So we will dynamically turn off sending page requests
// when it is enabled and the server doesn't support it.
static boolean usePageRequest = USE_PAGE_REQUEST_DEFAULT;

typedef enum {
  NO_CUSTOM_VERIFY,
  NEW_CUSTOM_VERIFY,
  CUSTOM_VERIFY_IN_PROGRESS,
} CustomVerifyStatus;
static boolean customVerifyStatus = NO_CUSTOM_VERIFY;

#if defined(EMBER_TEST)
// If set to 100, will not stop download.
int8u emAfOtaClientStopDownloadPercentage = 100;
#endif

//------------------------------------------------------------------------------
// Forward Declarations

static void recordUpgradeStatus(BootloadState state);
static void putImageInfoInMessage(void);
static void startServerDiscovery(void);
static void euiLookupErrorOccurred(void);
static void euiLookup(void);
static void queryNextImage(SendNextQueryState nextQuery,
                           boolean queryErrorOccurred);
static void continueImageDownload(void);
static void continueImageVerification(EmberAfImageVerifyStatus status);
static void askServerToRunUpgrade(boolean timeout);
static void runUpgrade(void);
static int32u updateCurrentOffset(int32u currentOffset);
static void updateDownloadFileVersion(int32u version);
static boolean downloadAndVerifyFinish(EmberAfOtaDownloadResult result);
static void determineNextState(void);
static EmberAfStatus imageNotifyParse(boolean broadcast, 
                                      int8u* buffer, 
                                      int8u index, 
                                      int8u length);
static EmberAfStatus queryNextImageResponseParse(int8u* buffer, 
                                                 int8u index, 
                                                 int8u length);
static EmberAfStatus imageBlockResponseParse(int8u* buffer, 
                                             int8u index, 
                                             int8u length);
static EmberAfStatus upgradeEndResponseParse(int8u status,
                                             int8u* buffer, 
                                             int8u length);
static boolean calculateTimer(int32u currentTime, 
                              int32u targetTime, 
                              int32u* returnTime);

//------------------------------------------------------------------------------

static void updateMinBlockRequestPeriodAttribute(int16u value)
{
  minBlockRequestPeriod = value;

#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_MINIMUM_BLOCK_REQUEST_PERIOD_ATTRIBUTE)
  {
    emberAfWriteAttribute(myEndpoint,
                          ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                          ZCL_MINIMUM_BLOCK_REQUEST_PERIOD_ATTRIBUTE_ID, 
                          CLUSTER_MASK_CLIENT,
                          (int8u*)&minBlockRequestPeriod,
                          ZCL_INT16U_ATTRIBUTE_TYPE);
  }
#endif
}

void emberAfOtaBootloadClusterClientInitCallback(int8u endpoint)
{
  if (myEndpoint != UNDEFINED_ENDPOINT) {
    // We have already been initialized
    return;
  }
  myEndpoint = endpoint;

  emberAfOtaStorageInitCallback();

#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_CURRENT_ZIGBEE_STACK_VERSION_ATTRIBUTE)
  {
    int16u currentZigbeeStackVersion = ZIGBEE_PRO_STACK_VERSION;
    emberAfWriteAttribute(myEndpoint,
                          ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                          ZCL_CURRENT_ZIGBEE_STACK_VERSION_ATTRIBUTE_ID, 
                          CLUSTER_MASK_CLIENT,
                          (int8u*)&currentZigbeeStackVersion,
                          ZCL_INT16U_ATTRIBUTE_TYPE);
  }
#endif

  // Assume we can download as fast as we want until told otherwise by server.
  updateMinBlockRequestPeriodAttribute(0); // minBlockRequestPeriod.
}

// Returns whether or not a timer has been set.  For timer = 0, will return FALSE
static boolean setTimer(int32u timeMs)
{
  int32u timer;

  // When waiting for page request replies I want to use the timer that is defined
  // in App. Builder and passed through to here since I am expecting multiple
  // messages coming back from the server.
  if (waitingForResponse
      && emAfGetCurrentPageRequestStatus() != EM_AF_WAITING_PAGE_REQUEST_REPLIES) {
    timer = MESSAGE_TIMEOUT_MS;
    nextEventTimer = timeMs;
  } else {
    timer = timeMs;
    nextEventTimer = 0;
  }

  // A timer set at 0 means we are not counting down to some event.
  // We may still be in an active state of waiting, but are waiting
  // for an event to fire rather than a timer to expire.  For example, service
  // discovery will generate a callback when it is complete.  No need for
  // keeping track of time here as well.
  if (timer != 0) {
    //    otaPrintln("Setting timer: 0x%4X ms", timer);
    EmberAfEventSleepControl sleepControl = EMBER_AF_OK_TO_NAP;
    if (currentBootloadState == BOOTLOAD_STATE_VERIFY_IMAGE) {
      sleepControl = EMBER_AF_STAY_AWAKE;
    } else if (!waitingForResponse
               && (emAfGetCurrentPageRequestStatus() 
                   == EM_AF_NO_PAGE_REQUEST)) {
      sleepControl = EMBER_AF_OK_TO_HIBERNATE;
    }
    emberAfScheduleClusterTick(myEndpoint,
                               ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                               EMBER_AF_CLIENT_CLUSTER_TICK,
                               timer,
                               sleepControl);
  }
  return (timer != 0);
}

static void otaClientTick(void)
{
  boolean errorOccurred = FALSE;
  // Getting here means either we timed out our last operation,
  // or we need to kick off a periodic event.

  emAfPageRequestTimerExpired();
   
  if (waitingForResponse) {
    otaPrintln("Timeout waiting for message.");
    errorOccurred = TRUE;
    errors++;
    waitingForResponse = FALSE;
      
    if (currentBootloadState == BOOTLOAD_STATE_DOWNLOAD) {
      // Especially important if the download delay is 0, we want
      // to backoff a little and make sure we don't continue to
      // blast the server with requests.
      setTimer((nextEventTimer > MESSAGE_TIMEOUT_BACKOFF_MS)
               ? nextEventTimer
               : MESSAGE_TIMEOUT_BACKOFF_MS);
      return;
    }
  }

  switch (currentBootloadState) {
  case BOOTLOAD_STATE_DISCOVER_SERVER:
    startServerDiscovery();
    break;
  case BOOTLOAD_STATE_GET_SERVER_EUI:
    euiLookupErrorOccurred();
    break;
  case BOOTLOAD_STATE_QUERY_NEXT_IMAGE:
    queryNextImage(SEND_QUERY_NOW, 
                   errorOccurred);
    break;
  case BOOTLOAD_STATE_DOWNLOAD:
    continueImageDownload();
    break;
  case BOOTLOAD_STATE_VERIFY_IMAGE:
    continueImageVerification(EMBER_AF_IMAGE_VERIFY_IN_PROGRESS);
    break;
  case BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE:
    askServerToRunUpgrade(TIMEOUT_REACHED);
    break;
  case BOOTLOAD_STATE_COUNTDOWN_TO_UPGRADE:
    runUpgrade();
  default:
    // Do nothing.  Invalid state
    break;
  }
}

void emberAfPluginOtaStorageSimpleEepromEraseCompleteCallback(boolean success)
{
  if (!success) {
    downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);

  } else if (currentBootloadState == BOOTLOAD_STATE_DOWNLOAD) {
    // If an erase was triggered for some other reason besides preparing
    // to start a download, we don't want to trigger the OTA client state
    // machine.
    otaClientTick();
  }
}

void emberAfOtaBootloadClusterClientTickCallback(int8u endpoint)
{
  // Since there is only one instance of the OTA bootload cluster allowed,
  // we don't care about the endpoint value.
  otaClientTick();
}

static void restartServerDiscoveryAfterDelay(void)
{
  setTimer(EMBER_AF_OTA_SERVER_DISCOVERY_DELAY_MS);
  recordUpgradeStatus(BOOTLOAD_STATE_DISCOVER_SERVER);
}

#if defined(EMBER_AF_HAS_SECURITY_PROFILE_SE)
static void otaClientPartnerLinkKeyCallback(boolean success)
{
  if (success) {
    determineNextState();
    return;
  } 

  restartServerDiscoveryAfterDelay();
}
#endif

static void getPartnerLinkKey(void)
{
#if defined(EMBER_AF_HAS_SECURITY_PROFILE_SE)
  EmberEUI64 serverEui64;
  int8u i;
  EmberAfAttributeType attributeType;

  if (!emberAfIsCurrentSecurityProfileSmartEnergy()
      || serverNodeId == EMBER_TRUST_CENTER_NODE_ID) {
    goto partnerLinkKeyDone;
  }

  emberAfReadClientAttribute(myEndpoint,
                             ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                             ZCL_UPGRADE_SERVER_ID_ATTRIBUTE_ID,
                             serverEui64,
                             EUI64_SIZE);

  for (i = 0; i < emberAfGetKeyTableSize(); i++) {
    EmberKeyStruct keyStruct;
    if (EMBER_SUCCESS == emberGetKeyTableEntry(i, &keyStruct)
        && EMBER_APPLICATION_LINK_KEY == keyStruct.type
        && 0 == MEMCOMPARE(keyStruct.partnerEUI64, serverEui64, EUI64_SIZE)) {
      goto partnerLinkKeyDone;
    }
  }

  // Spec is not clear whether the actual key establishment endpoint 
  // needs to be used, so we just use 1.
  if (EMBER_SUCCESS
      == emberAfInitiatePartnerLinkKeyExchange(serverNodeId,
                                               1,
                                               otaClientPartnerLinkKeyCallback)) {
    return;
  }
  
  // If we fail to initiate partner link key, it could be because 
  // we don't have any link key spots or the server is no longer online.
  // We just retry the entire operation again.
  restartServerDiscoveryAfterDelay();
  return;

 partnerLinkKeyDone:
#endif

  determineNextState();
  return;
}

static void updateCurrentImageAttributes(EmberAfOtaImageId* imageId)
{
#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_MANUFACTURER_ID_ATTRIBUTE)
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_MANUFACTURER_ID_ATTRIBUTE_ID,
                        CLUSTER_MASK_CLIENT,
                        (int8u*)&(imageId->manufacturerId),
                        ZCL_INT16U_ATTRIBUTE_TYPE);
#endif

#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_IMAGE_TYPE_ID_ATTRIBUTE)
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_IMAGE_TYPE_ID_ATTRIBUTE_ID,
                        CLUSTER_MASK_CLIENT,
                        (int8u*)&(imageId->imageTypeId),
                        ZCL_INT16U_ATTRIBUTE_TYPE);
#endif

#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_CURRENT_FILE_VERSION_ATTRIBUTE)
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_CURRENT_FILE_VERSION_ATTRIBUTE_ID,
                        CLUSTER_MASK_CLIENT,
                        (int8u*)&(imageId->firmwareVersion),
                        ZCL_INT32U_ATTRIBUTE_TYPE);
#endif
}

static int32u getCurrentOffset(void)
{
  int32u offset;
  int8u dataType = ZCL_INT32U_ATTRIBUTE_TYPE;
  emberAfReadAttribute(myEndpoint, 
                       ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                       ZCL_FILE_OFFSET_ATTRIBUTE_ID,
                       CLUSTER_MASK_CLIENT,
                       (int8u*)&offset,
                       4,
                       &dataType);
  return offset;
}

static int32u updateCurrentOffset(int32u currentOffset)
{
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_FILE_OFFSET_ATTRIBUTE_ID, 
                        CLUSTER_MASK_CLIENT,
                        (int8u*)&currentOffset,
                        ZCL_INT32U_ATTRIBUTE_TYPE);
  return currentOffset;
}

static void updateDownloadFileVersion(int32u version)
{
#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_DOWNLOADED_FILE_VERSION_ATTRIBUTE)
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_DOWNLOADED_FILE_VERSION_ATTRIBUTE_ID, 
                        CLUSTER_MASK_CLIENT,
                        (int8u*)(&version),
                        ZCL_INT32U_ATTRIBUTE_TYPE);
#endif
  currentDownloadFile.firmwareVersion = version;
}

// It is expected this is called when registration has successfully
// completed.
void emberAfOtaClientStartCallback(void)
{
  if (currentBootloadState == BOOTLOAD_STATE_NONE) {
    startServerDiscovery();
  }
}

void emAfOtaClientStop(void)
{
  downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);
  recordUpgradeStatus(BOOTLOAD_STATE_NONE);
  waitingForResponse = FALSE;

  emberAfDeactivateClusterTick(myEndpoint,
                               ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                               EMBER_AF_CLIENT_CLUSTER_TICK);
}

void emAfOtaClientPrintState(void)
{
  otaPrintln(" State:   %p",
             bootloadStateNames[currentBootloadState]);
  otaPrintln(" Waiting for response: %p",
             (waitingForResponse ? "yes" : "no"));
  if (waitingForResponse) {
    otaPrintln(" Next Event Timer: %d ms", nextEventTimer);
  }
  otaPrintln(" Current Download Offset: 0x%4X (%d%%)", 
             getCurrentOffset(),
             emAfCalculatePercentage(getCurrentOffset(),
                                     totalImageSize));
}

static void recordServerEuiAndGoToNextState(EmberEUI64 eui64)
{
  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_UPGRADE_SERVER_ID_ATTRIBUTE_ID, 
                        CLUSTER_MASK_CLIENT,
                        eui64,
                        ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE);
  otaPrintln("OTA Cluster: setting IEEE address of OTA cluster");
  getPartnerLinkKey();;
}

void emAfOtaClientServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult *result)
{
  // We only look at the first result.  How multiple OTA servers are handled
  // has not been spelled out by the spec yet.
  const EmberAfEndpointList* epList =
    (const EmberAfEndpointList*)result->responseData;
 
  waitingForResponse = FALSE;

  // Since the OTA cluster only uses broadcast discoveries for Match descriptor
  // and Unicast discoveries for the IEEE, we can differentiate the request
  // type based on the result.

  if (result->status == EMBER_AF_BROADCAST_SERVICE_DISCOVERY_COMPLETE) {
    if (serverEndpoint == UNDEFINED_ENDPOINT) {
      // We did not find an OTA server yet, so wait a while before trying
      // again.  Hopefully one will appear on the network later.
      restartServerDiscoveryAfterDelay();
    } else {
      euiLookup();
    }
    return;
  } else if (result->status
             == EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED) {
    if (epList->count > 0) {
      serverEndpoint = epList->list[0];
      serverNodeId = result->matchAddress;
      otaPrintln("Setting OTA Server to 0x%2X", serverNodeId);
    }

  } else if (result->status
             == EMBER_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE) {
    // Assumed IEEE address request
    EmberEUI64 eui64;
    MEMCOPY(eui64, result->responseData, EUI64_SIZE);
    recordServerEuiAndGoToNextState(eui64);

  } else { // Assume Unicast timeout of IEEE address request
    euiLookupErrorOccurred();
  }
}

static void startServerDiscovery(void)
{
  EmberStatus status;
  EmberAfProfileId appProfile;

  recordUpgradeStatus(BOOTLOAD_STATE_DISCOVER_SERVER);
  serverEndpoint = UNDEFINED_ENDPOINT;
  
  // Figure out the right app profile ID based on the endpoint descriptor
  appProfile = emberAfProfileIdFromIndex( 
                 emberAfIndexFromEndpoint(myEndpoint) );

  status = 
    emberAfFindDevicesByProfileAndCluster(
      EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS,
      appProfile,
      ZCL_OTA_BOOTLOAD_CLUSTER_ID,
      EMBER_AF_SERVER_CLUSTER_DISCOVERY,
      emAfOtaClientServiceDiscoveryCallback);

  if (status != EMBER_SUCCESS) {
    otaPrintln("Failed to initiate service discovery.");
    waitingForResponse = FALSE;
  } else {
    waitingForResponse = TRUE;
  }

  errors = 0;
  setTimer(EMBER_AF_OTA_SERVER_DISCOVERY_DELAY_MS);
}

static void euiLookupErrorOccurred(void)
{
  // The timer has expired and we don't know the server's EUI. 
  // We treat the server discovery + EUI lookup as a single operation
  // and if either fails we delay a long time to allow any potential network
  // issues to be resolved.
  waitingForResponse = FALSE;
  restartServerDiscoveryAfterDelay();
  return;
}

static void euiLookup(void)
{
  EmberEUI64 eui64;
  EmberStatus status;

  recordUpgradeStatus(BOOTLOAD_STATE_GET_SERVER_EUI);
  status = emberLookupEui64ByNodeId(serverNodeId, eui64);

  if (status != EMBER_SUCCESS) { 
    // New discovery of the Server's EUI
    status = emberAfFindIeeeAddress(serverNodeId,
                                    emAfOtaClientServiceDiscoveryCallback);
    if (status != EMBER_SUCCESS) {
      // Discovery was never initiated.
      euiLookupErrorOccurred();
      return;
    }
    recordUpgradeStatus(BOOTLOAD_STATE_GET_SERVER_EUI);
    waitingForResponse = TRUE;

  } // Else
    // We happen to know the server's EUI without discovery, keep going.

  recordServerEuiAndGoToNextState(eui64);
}

static void putImageInfoInMessage(void)
{
  emberAfPutInt16uInResp(currentDownloadFile.manufacturerId);
  emberAfPutInt16uInResp(currentDownloadFile.imageTypeId);
  emberAfPutInt32uInResp(currentDownloadFile.firmwareVersion);
}

static BootloadState determineDownloadFileStatus(void)
{
  int32u currentOffset;
  EmberAfOtaStorageStatus 
    status = emberAfOtaStorageCheckTempDataCallback(&currentOffset, 
                                                    &totalImageSize,
                                                    &currentDownloadFile);

  if (status == EMBER_AF_OTA_STORAGE_PARTIAL_FILE_FOUND) {
    otaPrintFlush();
    otaPrintln("Partial file download found, continuing from offset 0x%4X",
               currentOffset);
    otaPrintFlush();
    updateCurrentOffset(currentOffset);
    updateDownloadFileVersion(currentDownloadFile.firmwareVersion);
    emAfPrintPercentageSetStartAndEnd(0, totalImageSize);
    return BOOTLOAD_STATE_DOWNLOAD;
  } else if (status == EMBER_AF_OTA_STORAGE_SUCCESS) {
    EmberAfOtaImageId currentVersionInfo;
    otaPrintln("Found fully downloaded file in storage (version 0x%4X).", 
               currentDownloadFile.firmwareVersion);
    emberAfOtaClientVersionInfoCallback(&currentVersionInfo, NULL);
    if (currentVersionInfo.firmwareVersion != currentDownloadFile.firmwareVersion) {
      otaPrintln("Found file in storage with different version (0x%4X) than current version (0x%4X)",
                 currentDownloadFile.firmwareVersion,
                 currentVersionInfo.firmwareVersion);
      return BOOTLOAD_STATE_VERIFY_IMAGE;
    } else {
      otaPrintln("File in storage is same as current running version (0x%4X)",
                 currentVersionInfo.firmwareVersion);
    }
  } else {
    otaPrintln("No image found in storage.");    
  }

  emberAfAppFlush();

  return BOOTLOAD_STATE_QUERY_NEXT_IMAGE;
}

static void determineNextState(void)
{
  currentBootloadState = determineDownloadFileStatus();

  switch (currentBootloadState) {
  case BOOTLOAD_STATE_QUERY_NEXT_IMAGE:
    queryNextImage(SEND_QUERY_NOW,
                   FALSE);   // query error?
    break;
  case BOOTLOAD_STATE_DOWNLOAD: 
    {
      EmberAfOtaStorageStatus status = emberAfOtaStorageDriverPrepareToResumeDownloadCallback();
      errors = 0;
      recordUpgradeStatus(BOOTLOAD_STATE_DOWNLOAD);
      if (status == EMBER_AF_OTA_STORAGE_SUCCESS) {
        continueImageDownload();
      } else if (status != EMBER_AF_OTA_STORAGE_OPERATION_IN_PROGRESS) {
        downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);

      } // For operation-in-progress we fall-through and wait for the callback 
        // later.
    }
    break;
  case BOOTLOAD_STATE_VERIFY_IMAGE:
    continueImageVerification(EMBER_AF_IMAGE_UNKNOWN);
    break;
  case BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE:
    askServerToRunUpgrade(START_NEW_TIMER);
    break;
  default:
    // Do nothing.  No other states should reach here.
    break;
  }
}

static void recordUpgradeStatus(BootloadState state)
{
  int8u upgradeStatus = bootloadStateToExternalState[state];
  if (currentBootloadState != state) {
    emberAfCoreFlush();
    otaPrintln("Bootload state: %p", 
               bootloadStateNames[state]);
    emberAfCoreFlush();
  }
  currentBootloadState = state;

  emberAfWriteAttribute(myEndpoint,
                        ZCL_OTA_BOOTLOAD_CLUSTER_ID, 
                        ZCL_IMAGE_UPGRADE_STATUS_ATTRIBUTE_ID, 
                        CLUSTER_MASK_CLIENT,
                        (int8u*)&upgradeStatus,
                        ZCL_ENUM8_ATTRIBUTE_TYPE);
}

static EmberAfStatus commandParse(boolean defaultResponse,
                                  EmberAfClusterCommand* message)
{
  int8u commandId = (message->buffer[(ZCL_COMMAND_ID_INDEX
                                      + (int8u)defaultResponse)]);
  int8u index = EMBER_AF_ZCL_OVERHEAD;

  if (commandId > EM_AF_OTA_MAX_COMMAND_ID) {
    otaPrintln("Bad OTA command: 0x%X", commandId);
    return EMBER_ZCL_STATUS_INVALID_FIELD;
  }

  if (!defaultResponse && message->bufLen < emAfOtaMinMessageLengths[commandId]) {
    otaPrintln("OTA command 0x%X too short (len %d < min %d)",
               commandId,
               message->bufLen,
               emAfOtaMinMessageLengths[commandId]);
    return EMBER_ZCL_STATUS_MALFORMED_COMMAND;
  }

  if (message->source != serverNodeId) {
    otaPrintln("OTA command from unrecognized server 0x%2X.  My OTA server: 0x%2X",
               message->source,
               serverNodeId);
    return EMBER_ZCL_STATUS_NOT_AUTHORIZED;
  }

  // While not all command validation has taken place at this point, we 
  // flag that we are not waiting for a response anymore.  We want to make sure
  // that our sleepy can go to sleep if it wants.  The likelihood of an
  // invalid command followed by a properly formatted one is extremely low.
  // Either the server knows the correct format or it doesn't.
  waitingForResponse = FALSE;

  // NOTE:  In order to support Default Response, we list the CLIENT commands here
  // as well in case the server used that to respond.
  switch (commandId) {
  case ZCL_IMAGE_NOTIFY_COMMAND_ID: {
    boolean broadcast = (EMBER_INCOMING_UNICAST != message->type);
    if (currentBootloadState != BOOTLOAD_STATE_QUERY_NEXT_IMAGE) {
      otaPrintln("Got unexpected %p.  Ignored.",
                 "Image notify");
      return EMBER_ZCL_STATUS_FAILURE;
    }
    return imageNotifyParse(broadcast, message->buffer, index, message->bufLen);
  }
  case ZCL_QUERY_NEXT_IMAGE_RESPONSE_COMMAND_ID: 
  case ZCL_QUERY_NEXT_IMAGE_REQUEST_COMMAND_ID: {
    if (currentBootloadState != BOOTLOAD_STATE_QUERY_NEXT_IMAGE) {
      otaPrintln("Got unexpected %p.  Ignored.",
                 "Query next image response");
      return EMBER_ZCL_STATUS_FAILURE;
    }
    return queryNextImageResponseParse(message->buffer, 
                                       index + defaultResponse,
                                       message->bufLen);
  }
  case ZCL_IMAGE_PAGE_REQUEST_COMMAND_ID: 
  case ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID:
  case ZCL_IMAGE_BLOCK_RESPONSE_COMMAND_ID: {
    if (currentBootloadState != BOOTLOAD_STATE_DOWNLOAD) {
      otaPrintln("Got unexpected %p.  Ignored.",
                 "Image block response");
      return EMBER_ZCL_STATUS_FAILURE;
    }
    return imageBlockResponseParse(message->buffer, 
                                   index + defaultResponse,
                                   message->bufLen);
  }
  case ZCL_UPGRADE_END_RESPONSE_COMMAND_ID:
  case ZCL_UPGRADE_END_REQUEST_COMMAND_ID: {
    if (currentBootloadState != BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE) {
      otaPrintln("Got unexpected %p.  Ignored.",
                 "Upgrade end response");
      return EMBER_ZCL_STATUS_FAILURE;
    }
    return upgradeEndResponseParse((defaultResponse
                                    ? message->buffer[index]
                                    : EMBER_ZCL_STATUS_SUCCESS),
                                   message->buffer, 
                                   message->bufLen);
  }
  default:
    // Fall through. Already printed info about the bad command ID.
    break;
  }
  return EMBER_ZCL_STATUS_UNSUP_CLUSTER_COMMAND;
}

boolean emberAfOtaClientIncomingMessageRawCallback(EmberAfClusterCommand* message)
{
  EmberAfStatus zclStatus = commandParse(FALSE,   // default response?
                                         message); 
  if (zclStatus) {
    emberAfOtaBootloadClusterFlush();
    emberAfOtaBootloadClusterPrintln("%p: failed parsing OTA cmd 0x%x", 
                                     "Error",
                                     message->commandId);
    if (message->type == EMBER_INCOMING_UNICAST) {
      // We don't want to respond to invalid broadcast messages with
      // a default response.
      return EMBER_ZCL_STATUS_SUCCESS;
    }
    emberAfSendDefaultResponse(message, zclStatus);
  }

  return TRUE;
}

void emberAfOtaBootloadClusterClientDefaultResponseCallback(int8u endpoint, 
                                                            int8u commandId, 
                                                            EmberAfStatus status)
{
  EmberAfClusterCommand* command = emberAfCurrentCommand();

  if (endpoint != myEndpoint) {
    return;
  }
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    // The only default response we care about is non-success values.
    // That will mean the server failed processing for some reason,
    // or wants to abort.  Successful responses will send a non-default
    // response message.
    return;
  }
  otaPrintln("OTA Default response to command ID 0x%X, status 0x%X",
             commandId,
             status);

  commandParse(TRUE, // default response?
               command);
}

// The buffer must point to the start of the image notify command,
// not the start of the ZCL frame.  It is assumed that the length
// of the buffer is minimum for image notify command.
static EmberAfStatus imageNotifyParse(boolean broadcast, 
                                      int8u* buffer, 
                                      int8u index, 
                                      int8u length)
{
  int16u manufacturerId;
  int16u imageTypeId;
  EmberAfOtaImageId myId;
  int8u payloadType = emberAfGetInt8u(buffer, index, length);
  int8u queryJitter = emberAfGetInt8u(buffer, index + 1, length);
  index += 2;

  if (!broadcast) {
    // Spec says to always respond to unicasts regardless of the parameters.
    otaPrintln("%p unicast, querying",
               "Image notify command");
    goto sendQuery;
  }

  emberAfOtaClientVersionInfoCallback(&myId, NULL);

  // We assume that if the message is broadcast then our ZCL
  // code will NOT send the default response.
    
  if (payloadType > IMAGE_NOTIFY_LAST_VALID_TYPE) {
    otaPrintln("%p %p payload type 0x%X", 
               "Invalid",
               "Image notify command",
               payloadType);
    return EMBER_ZCL_STATUS_SUCCESS;
  }
  if (queryJitter < 1 || queryJitter > 100) {
    otaPrintln("%p %p: out of range jitter %d", 
               "Invalid",
               "Image notify command",
               queryJitter);
    return EMBER_ZCL_STATUS_SUCCESS;
  }
  if (length != imageNotifyPayloadLengths[payloadType]) {
    otaPrintln("%p %p: payload length doesn't match type 0x%X (%d < %d)",
               "Invalid",
               "Image notify command",
               payloadType,
               length,
               imageNotifyPayloadLengths[payloadType]);
    // Although this truly is an error, we don't send a response because it could
    // be a broadcast.
    return EMBER_ZCL_STATUS_SUCCESS;
  }
  
  if (payloadType >= IMAGE_NOTIFY_MANUFACTURER_ONLY_TYPE) {
    manufacturerId = emberAfGetInt16u(buffer, index, length);
    index += 2;
    if (manufacturerId != myId.manufacturerId) {
      otaPrintln("%p %p due to non-matching manufacturer ID",
                 "Ignoring",
                 "Image notify command");
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }

  if (payloadType >= IMAGE_NOTIFY_MFG_AND_IMAGE_TYPE) {
    imageTypeId = emberAfGetInt16u(buffer, index, length);
    index += 2;
    if (imageTypeId != myId.imageTypeId) {
      otaPrintln("%p %p due to non-matching image type ID",
                 "Ignoring",
                 "Image notify command");
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }

  if (payloadType >= IMAGE_NOTIFY_FULL_VERSION_TYPE) {
    // Could add some additional checking about the minimum allowed version
    // number, but it can still be caught after the download.
    int32u version = emberAfGetInt32u(buffer, index, length);
    index += 4;
    if (version == myId.firmwareVersion) {
      // Spec. says that if the firmware version matches, we should ignore.
      // A matching version number would be a re-install, which can only
      // be done via image notify by a unicast.  The server can force
      // a mass upgrade or downgrade by sending out a different version
      // than what devices have.
      otaPrintln("%p %p due to matching firmware version",
                 "Ignoring",
                 "Image notify command");
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }

  // Check QueryJitter value.  For QueryJitter value less than a 'must response'
  // value (value of 100), we need to introduce jitter in our reply by picking
  // a random number between 1 and 100.  We only send reply if the value picked
  // is less than or equal to the QueryJitter value.
  if (queryJitter < 100) {
    int8u random = (((int8u)halCommonGetRandom())%100) + 1;
    if(random > queryJitter) {
      otaPrintln("%p %p, Rx'd Jitter (0x%x), Picked Jitter (0x%x)",
                 "Ignoring",
                 "Image notify command",
                 queryJitter, 
                 random);
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }

 sendQuery:
  // By saying "timeout reached" we want to indicate that a new query should kick off
  // immediately.
  queryNextImage(SEND_QUERY_NOW,
                 FALSE);  // query error occurred?
  return EMBER_ZCL_STATUS_SUCCESS;
}

static void startDownload(int32u newVersion)
{
  EmberAfOtaStorageStatus status;
  otaPrintln("Starting download, Version 0x%4X",
             newVersion);
  emAfPrintPercentageSetStartAndEnd(0, totalImageSize);
  updateDownloadFileVersion(newVersion);
  updateCurrentOffset(0);
  updateMinBlockRequestPeriodAttribute(0);

  recordUpgradeStatus(BOOTLOAD_STATE_DOWNLOAD);

  errors = 0;

  status = emberAfOtaStorageClearTempDataCallback();

  if (status == EMBER_AF_OTA_STORAGE_SUCCESS) {

    // For the "OTA Storage Simple" plugin, there is only one image.
    // A call to clear the temp data (above) will also wipe out a fully
    // downloaded image.  So we don't actually have to call 
    // emberAfOtaStorageDeleteImageCallback() as well.  We only worry
    // about a result of EMBER_AF_OPERATION_IN_PROGRESS returned from the first
    // delete.

    // For the "OTA POSIX Filesystem" plugin there may be multiple
    // images, so we may have to delete the temp data AND another image
    // on disk.  However we assume that delete operations are extremely
    // fast and thus won't return EMBER_AF_OPERATION_IN_PROGRESS.

    EmberAfOtaImageId oldImageId = emberAfOtaStorageSearchCallback(currentDownloadFile.manufacturerId,
                                                                   currentDownloadFile.imageTypeId,
                                                                   (hardwareVersion == 0xFFFF
                                                                    ? NULL
                                                                    : &hardwareVersion));

    if (emberAfIsOtaImageIdValid(&oldImageId)) { 
      // Wipe out any existing file matching the same values as the one we 
      // are going to download.

      emberAfOtaStorageDeleteImageCallback(&oldImageId);
    }
    continueImageDownload();

  } else if (status == EMBER_AF_OTA_STORAGE_OPERATION_IN_PROGRESS) {
    // Disable the timer since we are waiting for the erase event to complete
    // and execute our callback, not the timer to expire.
    emberAfDeactivateClusterTick(myEndpoint,
                                 ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                                 EMBER_AF_CLIENT_CLUSTER_TICK);

  } else {
    otaPrintln("Error: Failed to erase old temp data.");
    downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);
  }
}

// We expect that the minimum length for this command has already been checked.
static EmberAfStatus queryNextImageResponseParse(int8u* buffer, 
                                                 int8u index, 
                                                 int8u length)
{
  int8u status;
  EmberAfStatus zclStatus;
  EmberAfOtaImageId imageId;
  boolean isError = FALSE;

  status = emberAfGetInt8u(buffer, index, length);
  index++;
  
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    otaPrintln("%p returned 0x%X.  No new image to download.", 
               "Query next image response",
               status);
    zclStatus = EMBER_ZCL_STATUS_SUCCESS;
    goto queryNextImageResponseDone;
  }
  if (length < QUERY_NEXT_IMAGE_SUCCESS_RESPONSE_MIN_LENGTH) {
    otaPrintln("%p too short (%d < %d)",
               "Query next image response",
               length,
               QUERY_NEXT_IMAGE_SUCCESS_RESPONSE_MIN_LENGTH);
    zclStatus = EMBER_ZCL_STATUS_MALFORMED_COMMAND;
    isError = TRUE;
    goto queryNextImageResponseDone;
  }
  otaPrintln("%p: New image is available for download.",
             "Query next image response");
  
  index += emAfOtaParseImageIdFromMessage(&imageId,
                                          &(buffer[index]),
                                          length - index);
  totalImageSize = emberAfGetInt32u(buffer, index, length);

  if (imageId.manufacturerId != currentDownloadFile.manufacturerId
      || imageId.imageTypeId != currentDownloadFile.imageTypeId
      || totalImageSize == 0) {
    otaPrintln("%p is not using my image info.",
               "Query next image response");
    isError = TRUE;
    zclStatus = EMBER_ZCL_STATUS_INVALID_FIELD;
    goto queryNextImageResponseDone;

  } else if (totalImageSize > emberAfOtaStorageDriverMaxDownloadSizeCallback()) {
    otaPrintln("ERROR: Next Image is too big to store (0x%4X > 0x%4X)",
               totalImageSize,
               emberAfOtaStorageDriverMaxDownloadSizeCallback());
    isError = TRUE;
    zclStatus = EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
    goto queryNextImageResponseDone;
  } else {
    startDownload(imageId.firmwareVersion);
    return EMBER_ZCL_STATUS_SUCCESS;
  }

 queryNextImageResponseDone:
  queryNextImage(DELAY_BEFORE_SEND_QUERY,
                 isError);
  return zclStatus;
}

static void sendMessage(int8u cmdId, int8u upgradeEndStatus, int32u timer)
{
  int8u fieldControl = 0;
  EmberAfProfileId appProfile;

  // Figure out the right app profile ID based on the endpoint descriptor
  appProfile = emberAfProfileIdFromIndex(emberAfIndexFromEndpoint(myEndpoint));

  // Basic ZCL header information
  appResponseLength = 0;
  emberAfResponseApsFrame.profileId = appProfile;
  emberAfResponseApsFrame.sourceEndpoint = myEndpoint;
  emberAfPutInt8uInResp(ZCL_CLUSTER_SPECIFIC_COMMAND
                        | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER);
  emberAfPutInt8uInResp(emberAfNextSequence());
  emberAfPutInt8uInResp(cmdId);
  waitingForResponse = TRUE;

  switch(cmdId) {
    case ZCL_QUERY_NEXT_IMAGE_REQUEST_COMMAND_ID: {
      if (hardwareVersion != EMBER_AF_INVALID_HARDWARE_VERSION) {
        fieldControl |= OTA_HW_VERSION_BIT_MASK;
      }
      emberAfPutInt8uInResp(fieldControl);
      putImageInfoInMessage();
      if (hardwareVersion != EMBER_AF_INVALID_HARDWARE_VERSION) {
        emberAfPutInt16uInResp(hardwareVersion);
      }
    }
    break;

    case ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID:
    case ZCL_IMAGE_PAGE_REQUEST_COMMAND_ID: 
      emberAfPutInt8uInResp(fieldControl);
      putImageInfoInMessage();
      emberAfPutInt32uInResp(getCurrentOffset());
      emberAfPutInt8uInResp(usePageRequest
                            ? EM_AF_PAGE_REQUEST_BLOCK_SIZE
                            : MAX_CLIENT_DATA_SIZE);

      if (cmdId == ZCL_IMAGE_PAGE_REQUEST_COMMAND_ID) {
        emberAfPutInt16uInResp(EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE);        
        emberAfPutInt16uInResp(EMBER_AF_OTA_CLIENT_PAGE_REQUEST_SPACING_MS);
      } else {
#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_MINIMUM_BLOCK_REQUEST_PERIOD_ATTRIBUTE)
        emberAfPutInt16uInResp(minBlockRequestPeriod);
#endif
      }
      break;

    case ZCL_UPGRADE_END_REQUEST_COMMAND_ID:
      otaPrintln("Sending Upgrade End request.");
      emberAfCoreFlush();
      emberAfPutInt8uInResp(upgradeEndStatus);
      putImageInfoInMessage();
      if (upgradeEndStatus != 0) {
        waitingForResponse = FALSE;
      }
      break;

    case ZCL_QUERY_SPECIFIC_FILE_REQUEST_COMMAND_ID: {
      EmberEUI64 myEui64;
      emberAfGetEui64(myEui64);
      emberAfPutBlockInResp(myEui64, EUI64_SIZE);
      putImageInfoInMessage();
      emberAfPutInt16uInResp(ZIGBEE_PRO_STACK_VERSION);
      }
      break;

    default:
      otaPrintln("%p: invalid cmdId 0x%x", "Error", cmdId);
      return;
  } //end switch statement

  emberAfResponseApsFrame.clusterId = ZCL_OTA_BOOTLOAD_CLUSTER_ID;
  emberAfResponseApsFrame.sourceEndpoint = myEndpoint;
  emberAfResponseApsFrame.destinationEndpoint = serverEndpoint;
  emberAfResponseApsFrame.options |= EMBER_APS_OPTION_RETRY;
  emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, serverNodeId);
  setTimer(timer);
}

static void queryNextImage(SendNextQueryState sendQuery, 
                           boolean queryErrorOccurred)
{
  recordUpgradeStatus(BOOTLOAD_STATE_QUERY_NEXT_IMAGE);

  if (serverNodeId == EMBER_UNKNOWN_NODE_ID) {
    otaPrintln("Server unknown, falling back to discovery.");
    restartServerDiscoveryAfterDelay();
    return;
  }

  if (queryErrorOccurred) {
    errors++;
    if (errors >= EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_ERROR_THRESHOLD) {
      otaPrintln("Maximum query errors hit (%d), starting search for new OTA server.\n",
                 EMBER_AF_PLUGIN_OTA_CLIENT_QUERY_ERROR_THRESHOLD);
      restartServerDiscoveryAfterDelay();
      return;
    }
  }

  if (sendQuery == SEND_QUERY_NOW) {
    if (!queryErrorOccurred) {
      errors = 0;
    }

    // Ask the client what image info to use in the query and
    // subsequent download.
    emberAfOtaClientVersionInfoCallback(&currentDownloadFile, 
                                        &hardwareVersion);
    updateCurrentImageAttributes(&currentDownloadFile);

    sendMessage(ZCL_QUERY_NEXT_IMAGE_REQUEST_COMMAND_ID,
                0,                            // upgrade end status (ignored)
                EMBER_AF_OTA_QUERY_DELAY_MS);
  } else {
    setTimer(EMBER_AF_OTA_QUERY_DELAY_MS);
  }
}

static void continueImageDownload(void)
{
  int8u commandId = ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID;
  int32u timer = EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS;
  boolean send = TRUE;

  recordUpgradeStatus(BOOTLOAD_STATE_DOWNLOAD);

  if (errors >= EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_ERROR_THRESHOLD) {
    otaPrintln("Maximum number of download errors reach (%d), aborting.",
               errors);
    downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);
    return;
  }

  if (usePageRequest) {
    // Set the current offset for page request
    // or, Get the current offset if retrying image blocks
    EmAfPageRequestClientStatus status = emAfGetCurrentPageRequestStatus();
    if (status == EM_AF_NO_PAGE_REQUEST) {
      timer = emAfInitPageRequestClient(getCurrentOffset(),
                                        totalImageSize);
      commandId = ZCL_IMAGE_PAGE_REQUEST_COMMAND_ID;
    } else {
      int32u offset;
      if (EM_AF_PAGE_REQUEST_ERROR
          == emAfNextMissedBlockRequestOffset(&offset)) {
        // Server is unreachable because page request caused us to get 0
        // response packets from the server when we should have received
        // a lot.
        errors++;
        send = FALSE;
      } else {
        updateCurrentOffset(offset);
      }
    }
  }

  if (send) {
    sendMessage(commandId,
                0,         // upgrade end status (ignored)
                timer);
  } else {
    setTimer(timer);
  }
}

// A callback fired by the verification code.
void emAfOtaVerifyStoredDataFinish(EmberAfImageVerifyStatus status)
{
  if (currentBootloadState == BOOTLOAD_STATE_VERIFY_IMAGE) {
    continueImageVerification(status);
  }
}

static boolean downloadAndVerifyFinish(EmberAfOtaDownloadResult result)
{
  static PGM int8u zclStatusFromResult[] = {
    EMBER_ZCL_STATUS_SUCCESS,
    EMBER_ZCL_STATUS_ABORT,
    EMBER_ZCL_STATUS_INVALID_IMAGE,
    EMBER_ZCL_STATUS_ABORT,
    EMBER_ZCL_STATUS_ABORT,
  };
  int8u zclStatus;
  boolean goAhead;

  if (currentBootloadState <= BOOTLOAD_STATE_QUERY_NEXT_IMAGE) {
    // We don't really care about the return code here.  
    // The important thing is that we don't call the client's download
    // complete handler.
    return TRUE;
  }

  goAhead = emberAfOtaClientDownloadCompleteCallback(result,
                                                     &currentDownloadFile);

  if (result == EMBER_AF_OTA_DOWNLOAD_AND_VERIFY_SUCCESS
      && !goAhead) {
    otaPrintln("Client verification failed.");
    result = EMBER_AF_OTA_CLIENT_ABORTED;
  }
  zclStatus = zclStatusFromResult[result];

  // We could automatically invalidate the image in temporary storage
  // when it is declared a bad image.
  // This can be done by the download complete callback instead.

  if (zclStatus == EMBER_ZCL_STATUS_SUCCESS) {
    askServerToRunUpgrade(START_NEW_TIMER);
  } else if (result != EMBER_AF_OTA_SERVER_ABORTED
             && currentBootloadState != BOOTLOAD_STATE_QUERY_NEXT_IMAGE) {
    // Report to the server that the download has failed.
    sendMessage(ZCL_UPGRADE_END_REQUEST_COMMAND_ID, 
                zclStatus,
                EMBER_AF_RUN_UPGRADE_REQUEST_DELAY_MS);
  }
  
  if (zclStatus != EMBER_ZCL_STATUS_SUCCESS) {
    waitingForResponse = FALSE;
    queryNextImage(DELAY_BEFORE_SEND_QUERY,
                   FALSE);  // query error?
  }
  return (result == EMBER_ZCL_STATUS_SUCCESS);
}

static void continueImageVerification(EmberAfImageVerifyStatus status)
{
  recordUpgradeStatus(BOOTLOAD_STATE_VERIFY_IMAGE);

  if (status == EMBER_AF_IMAGE_UNKNOWN) {
    customVerifyStatus = NO_CUSTOM_VERIFY;

  } else if (status == EMBER_AF_IMAGE_GOOD) {
    // This is only called with status == GOOD when signature
    // verification has completed.  It is not called when
    // custom verification is done.
    customVerifyStatus = NEW_CUSTOM_VERIFY;
    status = EMBER_AF_IMAGE_VERIFY_IN_PROGRESS;
  }

  if (status == EMBER_AF_IMAGE_UNKNOWN
      || status == EMBER_AF_IMAGE_VERIFY_IN_PROGRESS) {
    int32u offset;
    int32u totalSize;
    EmberAfOtaImageId id;

    // First a basic sanity check of the image to insure
    // the file has completely downloaded and the file format
    // is correct.
    if (status == EMBER_AF_IMAGE_UNKNOWN
        && (EMBER_AF_OTA_STORAGE_SUCCESS
            != emberAfOtaStorageCheckTempDataCallback(&offset,
                                                      &totalSize,
                                                      &id))) {
      status = EMBER_AF_IMAGE_VERIFY_ERROR;
//      otaPrintln("emberAfOtaStorageCheckTempDataCallback() failed.");
      goto imageVerifyDone;
    }

    if (customVerifyStatus != NO_CUSTOM_VERIFY) {
      status = emberAfOtaClientCustomVerifyCallback((customVerifyStatus 
                                                     == NEW_CUSTOM_VERIFY),
                                                    &currentDownloadFile);
      customVerifyStatus = CUSTOM_VERIFY_IN_PROGRESS;
    } else {
      status = emAfOtaImageSignatureVerify(MAX_DIGEST_CALCULATIONS_PER_CALL,
                                           &currentDownloadFile,
                                           (status == EMBER_AF_IMAGE_UNKNOWN
                                            ? EMBER_AF_NEW_IMAGE_VERIFICATION
                                            : EMBER_AF_CONTINUE_IMAGE_VERIFY));
    }

    if (status == EMBER_AF_IMAGE_VERIFY_IN_PROGRESS) {
      setTimer(EMBER_AF_PLUGIN_OTA_CLIENT_VERIFY_DELAY_MS);
      return;
    } else if (status == EMBER_AF_IMAGE_VERIFY_WAIT) {
      setTimer(0);
      return;
    } else if (status == EMBER_AF_NO_IMAGE_VERIFY_SUPPORT) {
      otaPrintln("No signature verification support, assuming image is okay.");
      customVerifyStatus = NEW_CUSTOM_VERIFY;
      setTimer(EMBER_AF_PLUGIN_OTA_CLIENT_VERIFY_DELAY_MS);
      return;
    } else {
      otaPrintln("%p verification %p: 0x%X", 
                 (customVerifyStatus == NO_CUSTOM_VERIFY
                  ? "Signature"
                  : "Custom"),
                 (status == EMBER_AF_IMAGE_GOOD
                  ? "passed" 
                  : "FAILED"),
                 status);
    }
  }

 imageVerifyDone:
  downloadAndVerifyFinish((status == EMBER_AF_IMAGE_GOOD
                           ? EMBER_AF_OTA_DOWNLOAD_AND_VERIFY_SUCCESS
                           : EMBER_AF_OTA_VERIFY_FAILED));
}

static void askServerToRunUpgrade(boolean timeout)
{
  recordUpgradeStatus(BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE);

  if (!timeout) {
    errors = 0;
  }

  if (errors >= EMBER_AF_PLUGIN_OTA_CLIENT_UPGRADE_WAIT_THRESHOLD) {
    otaPrintln("Maximum upgrade requests made (%d) without response from server.");
    otaPrintln("Upgrading anyway");
    runUpgrade();
    return;
  }
  sendMessage(ZCL_UPGRADE_END_REQUEST_COMMAND_ID, 
              EMBER_ZCL_STATUS_SUCCESS,
              EMBER_AF_RUN_UPGRADE_REQUEST_DELAY_MS);
}

static boolean storeData(int32u offset, int32u length, const int8u* data)
{
  return (EMBER_AF_OTA_STORAGE_SUCCESS
          == emberAfOtaStorageWriteTempDataCallback(offset, length, data));
}

static EmberAfStatus imageBlockResponseParse(int8u* buffer, int8u index, int8u length)
{
  EmberAfOtaImageId imageId;
  int32u offset;
  int32u currentOffset;
  int8u dataSize;
  int32u timerMs = (EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS >= minBlockRequestPeriod
                    ? EMBER_AF_PLUGIN_OTA_CLIENT_DOWNLOAD_DELAY_MS
                    : (minBlockRequestPeriod << 10));  // seconds -> milliseconds
  int8u status = emberAfGetInt8u(buffer, index, length);
  int32u nextOffset;
  const int8u* imageData;
  EmAfPageRequestClientStatus pageRequestStatus;

  index++;

  if (buffer != NULL && status == EMBER_ZCL_STATUS_WAIT_FOR_DATA) {
    int32u currentTime;
    int32u requestTime;
    int32u calculatedTimer;

    if (length < IMAGE_BLOCK_RESPONSE_WAIT_FOR_DATA_LENGTH_MIN) {
      return EMBER_ZCL_STATUS_MALFORMED_COMMAND;
    }
    currentTime = emberAfGetInt32u(buffer, index, length);
    index += 4;
    requestTime = emberAfGetInt32u(buffer, index, length);
    index += 4;

    if (!calculateTimer(currentTime, requestTime, &calculatedTimer)) {
      // Error printed by above function.
      calculatedTimer = CALCULATE_TIME_ERROR_IMAGE_BLOCK_DELAY_MS; 
    }

    if (length >= IMAGE_BLOCK_RESPONSE_WAIT_FOR_DATA_MIN_BLOCK_RESPONSE_LENGTH) {
      int16u temp = emberAfGetInt16u(buffer, 
                                     index, 
                                     length);
      emberAfCoreFlush();
      otaPrintln("Updating Min Block Request Period to %d seconds",
                 temp);
      updateMinBlockRequestPeriodAttribute(temp);
    }

    emberAfCoreFlush();
    otaPrintln("Download delay by server %d ms", calculatedTimer);
    emberAfCoreFlush();
    setTimer(calculatedTimer);
    return EMBER_ZCL_STATUS_SUCCESS;
  } else if (status == EMBER_ZCL_STATUS_ABORT
             || status == EMBER_ZCL_STATUS_NO_IMAGE_AVAILABLE) {
    otaPrintln("Download aborted by server.");
    downloadAndVerifyFinish(EMBER_AF_OTA_SERVER_ABORTED);
    return EMBER_ZCL_STATUS_SUCCESS;
  } else if (status == EMBER_ZCL_STATUS_UNSUP_CLUSTER_COMMAND) {
    if (usePageRequest && emAfHandlingPageRequestClient()) {
      otaPrintln("Server doesn't support page request, only using block request.");
      usePageRequest = FALSE;
      emAfAbortPageRequest();
      continueImageDownload();
      return EMBER_ZCL_STATUS_SUCCESS;
    } else {
      otaPrintln("Server returned 'unsupported cluster command'.");
      downloadAndVerifyFinish(EMBER_AF_OTA_SERVER_ABORTED);
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  } else if (status != EMBER_ZCL_STATUS_SUCCESS) {
    otaPrintln("Unknown %p status code 0x%X", 
               "Image block response", 
               status);
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  } // Else status == success.  Keep going

  if (length < IMAGE_BLOCK_RESPONSE_SUCCESS_MIN_LENGTH) {
    return EMBER_ZCL_STATUS_MALFORMED_COMMAND;
  }

  index += emAfOtaParseImageIdFromMessage(&imageId,
                                          &(buffer[index]), 
                                          length);
  offset = emberAfGetInt32u(buffer, index, length);
  index += 4;
  dataSize = emberAfGetInt8u(buffer, index, length);
  index += 1;
  imageData = buffer + index;
  
  if ((length - index) < dataSize) {
    otaPrintln("%p has data size (%d) smaller than actual packet size (%d).",
               "Image block response",
               dataSize,
               length - index);
    return EMBER_ZCL_STATUS_MALFORMED_COMMAND;
  }
  
  if (!usePageRequest 
      || emAfGetCurrentPageRequestStatus() != EM_AF_WAITING_PAGE_REQUEST_REPLIES) {
    // For normal image block request transactions, all blocks should be in order.
    // For page request, we may receive them out of order, or just miss packets.
    currentOffset = getCurrentOffset();
    if (offset != currentOffset) {
      otaPrintln("%p error: Expected offset 0x%4X, but got 0x%4X.  Ignoring", 
                 "Image block response",
                 currentOffset, 
                 offset);
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }
  
  if (0 != MEMCOMPARE(&currentDownloadFile, 
                      &imageId, 
                      sizeof(EmberAfOtaImageId))
             || dataSize > MAX_CLIENT_DATA_SIZE) {
    otaPrintln("%p info did not match my expected info.  Dropping.",
               "Image block response");
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }

  errors = 0;

  // We want to make sure we don't write the same block twice.  For
  // page-erase-required EEPROM parts this won't work.
  if (usePageRequest && emAfHandlingPageRequestClient()) {
    pageRequestStatus = emAfNoteReceivedBlockForPageRequestClient(offset);
    if (pageRequestStatus == EM_AF_BLOCK_ALREADY_RECEIVED) {
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }

  if (!storeData(offset, dataSize, imageData)) {
    otaPrintln("Failed to write to storage device!");
    downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);
    // The downloadAndVerifyFinish() function will return it's only status
    // to the message.  No need to send ours.
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  nextOffset = offset + dataSize;

  if (usePageRequest && emAfHandlingPageRequestClient()) {
    if (pageRequestStatus == EM_AF_PAGE_REQUEST_ERROR) {
      downloadAndVerifyFinish(EMBER_AF_OTA_CLIENT_ABORTED);
      // We still return success to indicate we processed the message correctly.
      return EMBER_ZCL_STATUS_SUCCESS;

    } else if (pageRequestStatus == EM_AF_WAITING_PAGE_REQUEST_REPLIES) {
      return EMBER_ZCL_STATUS_SUCCESS;

    } else if (pageRequestStatus == EM_AF_PAGE_REQUEST_COMPLETE) {
      nextOffset = emAfGetFinishedPageRequestOffset();

    } else { // EM_AF_RETRY_MISSED_PACKETS 
      timerMs = emAfGetPageRequestMissedPacketDelayMs();
    }
  }

  offset = updateCurrentOffset(nextOffset);

#if defined(EMBER_TEST)
  {
    int8u percentageComplete = 
      emAfPrintPercentageUpdate("Download", 
                                DOWNLOAD_PERCENTAGE_UPDATE_RATE, 
                                offset);
    if (percentageComplete != 100 && percentageComplete >= emAfOtaClientStopDownloadPercentage) {
      otaPrintln("Artificially stopping download at %d%%", emAfOtaClientStopDownloadPercentage);
      return EMBER_ZCL_STATUS_SUCCESS;
    }
  }
#else
  emAfPrintPercentageUpdate("Download", 
                            DOWNLOAD_PERCENTAGE_UPDATE_RATE, 
                            offset);
#endif

  if (offset >= totalImageSize) {
    emberAfOtaStorageFinishDownloadCallback(offset);
    continueImageVerification(EMBER_AF_IMAGE_UNKNOWN);  
    return EMBER_ZCL_STATUS_SUCCESS;
  } else {
    if (!setTimer(timerMs)) {
      // Continue download right now
      continueImageDownload();
    } // else (fall through)
      // timer set, wait for next event to fire
  }
  return EMBER_ZCL_STATUS_SUCCESS;
}

static EmberAfStatus upgradeEndResponseParse(int8u status,
                                             int8u* buffer, 
                                             int8u length)
{
  EmberAfOtaImageId serverSentId;
  int32u waitTime;
  int32u currentTime, upgradeTime;
  int8u index = EMBER_AF_ZCL_OVERHEAD;

  if (status) {
    otaPrintln("Server aborted upgrade, status: 0x%X",
               status);
    downloadAndVerifyFinish(EMBER_AF_OTA_SERVER_ABORTED);
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  index += emAfOtaParseImageIdFromMessage(&serverSentId, 
                                          &(buffer[index]),
                                          length);

  if ((serverSentId.manufacturerId != currentDownloadFile.manufacturerId)
      && (serverSentId.manufacturerId != MFG_ID_WILD_CARD)) {
    emberAfOtaBootloadClusterPrint("Error: %p had invalid %p: ",
                                   "Upgrade end response",
                                   "manufacturer ID");
    otaPrintln("0x%2X", serverSentId.manufacturerId);
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }
  if ((serverSentId.imageTypeId != currentDownloadFile.imageTypeId) 
      && (serverSentId.imageTypeId != IMAGE_TYPE_WILD_CARD)) {
    emberAfOtaBootloadClusterPrint("Error: %p had invalid %p: ",
                                   "Upgrade end response",
                                   "image type ID");
    otaPrintln("0x%2X", serverSentId.imageTypeId);
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }
  if ((serverSentId.firmwareVersion != currentDownloadFile.firmwareVersion
       && serverSentId.firmwareVersion != FILE_VERSION_WILD_CARD)) {
    emberAfOtaBootloadClusterPrint("Error: %p had invalid %p: ",
                                   "Upgrade end response",
                                   "file version");
    otaPrintln("0x%4X", serverSentId.firmwareVersion);
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }
  currentTime = emberAfGetInt32u(buffer, index, length);
  index += 4;
  upgradeTime = emberAfGetInt32u(buffer, index, length);
  
  if (WAIT_FOR_UPGRADE_MESSAGE == upgradeTime) {
    recordUpgradeStatus(BOOTLOAD_STATE_WAITING_FOR_UPGRADE_MESSAGE);
    setTimer(WAIT_FOR_UPGRADE_DELAY_MS);
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  // NOTE:  Current Time and Upgrade Time are in SECONDS since epoch.
  // Our timer uses MILISECONDS.  calculateTimer() will give us the MS delay.

  if (!calculateTimer(currentTime, upgradeTime, &waitTime)) {
    waitTime = CALCULATE_TIME_ERROR_UPGRADE_END_RESPONSE_DELAY_MS;
  } else {
    // Even when we are told to upgrade immediately, we want to insure there
    // is a chance for the APS retries and ZCL response to get back to their 
    // senders.
    if (waitTime < IMMEDIATE_UPGRADE_DELAY_MS) {
      otaPrintln("Adding %d ms. delay for immediate upgrade.", 
                 IMMEDIATE_UPGRADE_DELAY_MS);
      waitTime = IMMEDIATE_UPGRADE_DELAY_MS;
    }
  }
  // Expect at this point waitTime != 0
  setTimer(waitTime);
  emberAfCoreFlush();
  otaPrintln("Countdown to upgrade: %d ms", waitTime);
  emberAfCoreFlush();
  recordUpgradeStatus(BOOTLOAD_STATE_COUNTDOWN_TO_UPGRADE);
  return EMBER_ZCL_STATUS_SUCCESS;
}

static boolean calculateTimer(int32u currentTime, 
                              int32u targetTime, 
                              int32u* returnTimeMs)
{
  int32u timeOut = 0;
  boolean validWaitTime = TRUE; 

  if (targetTime < currentTime) {
    otaPrintln("%p: invalid offset currentTime(0x%4X) > upgradeTime(0x%4X)",
               "Error",
               currentTime, 
               targetTime);
    return FALSE;
  } else {
    timeOut = targetTime - currentTime;
    otaPrintln("OTA Cluster: wait for %d s", timeOut);
  }
  otaPrintln("RXed timeOut 0x%4X s, MAX timeOut 0x%4X s",
             timeOut, 
             TIMEOUT_MAX_WAIT_TIME_MS >> 10);   // divide by ~1000
                                                // save flash by doing a bit shift
  timeOut *= 1000;

  if (timeOut > TIMEOUT_MAX_WAIT_TIME_MS) {
    timeOut = TIMEOUT_MAX_WAIT_TIME_MS;
  }
  *returnTimeMs = timeOut;    
  return validWaitTime;
}

static void runUpgrade(void)
{
  emberAfCoreFlush();
  otaPrintln("Applying upgrade");
  emberAfCoreFlush();
  
  emberAfOtaClientBootloadCallback(&currentDownloadFile);

  // If we returned, then something is wrong with the upgrade.  
  // It is expected that an invalid image file is deleted to prevent it
  // from being used a subsequent time.
  queryNextImage(START_NEW_TIMER, 
                 FALSE);  // query error?
}

// Sends an image block request for a file the server should 
// not have.  Test harness only (test case 9.5.6 - Missing File)
void emAfSendImageBlockRequestTest(void)
{
  if (currentBootloadState != BOOTLOAD_STATE_NONE) {
    otaPrintln("Image block request test only works when state is BOOTLOAD_STATE_NONE");
    return;
  }

  updateCurrentOffset(100);

  // Values from the test spec.
  currentDownloadFile.manufacturerId = 0xFFF0;
  currentDownloadFile.imageTypeId    = 0x0000;
  currentDownloadFile.firmwareVersion = 0xFFFFFFF0;
  sendMessage(ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID, 
              0,   // upgrade end status (ignored)
              0);  // timer
}

void emAfSetPageRequest(boolean pageRequestOn)
{
  usePageRequest = pageRequestOn;
}

boolean emAfUsingPageRequest(void)
{
  return usePageRequest;
}
