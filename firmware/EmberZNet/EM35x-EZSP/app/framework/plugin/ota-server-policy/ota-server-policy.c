// *******************************************************************
// * ota-server-policy.c
// *
// * A sample policy file that implements the callbacks for the 
// * Zigbee Over-the-air bootload cluster server.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-server/ota-server.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/util/util.h"
#include "app/framework/util/common.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

typedef enum {
  UPGRADE_IF_SERVER_HAS_NEWER = 0,
  DOWNGRADE_IF_SERVER_HAS_OLDER = 1,
  REINSTALL_IF_SERVER_HAS_SAME = 2,
  NO_NEXT_VERSION = 3,
} NextVersionPolicy;
#define QUERY_POLICY_MAX NO_NEXT_VERSION

static PGM_P nextVersionPolicyStrings[] = {
  "Upgrade if server has newer",
  "Downgrade if server has older",
  "Reinstall if server has same",
  "No next version",
};

static NextVersionPolicy nextVersionPolicy = UPGRADE_IF_SERVER_HAS_NEWER;

// Image Block Response Message Format
// Status Code: 1-byte
// Manuf Code:  2-bytes
// Image Type:  2-bytes
// File Ver:    4-bytes
// File Offset: 4-bytes
// Data Size:   1-byte
// Data:        variable
#define IMAGE_BLOCK_RESPONSE_OVERHEAD (EMBER_AF_ZCL_OVERHEAD + 14)

#if defined EM_AF_TEST_HARNESS_CODE
typedef enum {
  SEND_BLOCK = 0,
  DELAY_DOWNLOAD_ONCE = 1,
  ABORT_DOWNNLOAD = 2,
} ImageBlockRequestPolicy;
#define BLOCK_REQUEST_POLICY_MAX ABORT_DOWNNLOAD

static ImageBlockRequestPolicy imageBlockRequestPolicy = SEND_BLOCK;

PGM_P imageBlockRequestPolicyStrings[] = {
  "Send block",
  "Delay download once",
  "Abort download",
};

#define IMAGE_BLOCK_REQUEST_DELAY_TIME_SECONDS (2 * 60)
#endif

typedef enum {
  UPGRADE_NOW = 0,
  UPGRADE_SOON = 1,
  UPGRADE_ASK_ME_LATER = 2,
  UPGRADE_ABORT = 3,
} UpgradePolicy;
#define UPGRADE_POLICY_MAX  UPGRADE_ABORT

UpgradePolicy upgradePolicy = UPGRADE_NOW;
#define UPGRADE_SOON_TIME_SECONDS (2 * 60)

PGM_P upgradePolicyStrings[] = {
  "Upgrade Now",
  "Upgrade In a few minutes",
  "Ask me later to upgrade",
  "Abort upgrade",
};

// This corresponds to the enumerated UpgradePolicy list.
PGM int32u upgradeTimes[] = {
  0,                            // Now
  UPGRADE_SOON_TIME_SECONDS,    // in a little while
  0xFFFFFFFFL,                  // go ask your father (wait forever)
  0,                            // unused 
};

static int16u missedBlockModulus = 0;

#if defined(EMBER_AF_PLUGIN_OTA_SERVER_PAGE_REQUEST_SUPPORT)
  #define PAGE_REQUEST_STATUS_CODE EMBER_ZCL_STATUS_SUCCESS
  #define PAGE_REQUEST_COMPILE_TIME_SUPPORT "yes"
#else
  #define PAGE_REQUEST_STATUS_CODE EMBER_ZCL_STATUS_UNSUP_CLUSTER_COMMAND
  #define PAGE_REQUEST_COMPILE_TIME_SUPPORT "no"
#endif

static int8u pageRequestStatus = PAGE_REQUEST_STATUS_CODE;

static int16u imageBlockRequestMinRequestPeriodSeconds = 0;

#define SERVER_AND_CLIENT_SUPPORT_MIN_BLOCK_REQUEST \
  (EMBER_AF_IMAGE_BLOCK_REQUEST_MIN_BLOCK_REQUEST_SUPPORTED_BY_CLIENT \
   | EMBER_AF_IMAGE_BLOCK_REQUEST_MIN_BLOCK_REQUEST_SUPPORTED_BY_SERVER)

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------

void emAfOtaServerPolicyPrint(void)
{
  otaPrintln("OTA Server Policies");
  otaPrintln("Query Policy: %p (%d)", 
             nextVersionPolicyStrings[nextVersionPolicy],
             nextVersionPolicy);

#if defined EM_AF_TEST_HARNESS_CODE
  otaPrintln("Image Block Request Policy: %p (%d)", 
             imageBlockRequestPolicyStrings[imageBlockRequestPolicy],
             imageBlockRequestPolicy);
  if (imageBlockRequestPolicy == DELAY_DOWNLOAD_ONCE) {
    otaPrintln("  Delay time: %d seconds", 
               IMAGE_BLOCK_REQUEST_DELAY_TIME_SECONDS);
  }
  otaPrintln("Page Request Block Missed Modulus: %d", missedBlockModulus);
#else
  otaPrintln("Image Block Request Policy: Send block");
#endif  
  otaPrintln("Image Block Request Min Period: %d seconds", 
             imageBlockRequestMinRequestPeriodSeconds);
  emberAfCoreFlush();

  otaPrintln("Page Request Code Compiled in: %p",
             PAGE_REQUEST_COMPILE_TIME_SUPPORT);
  otaPrintln("Page Request Runtime Status Code: 0x%X",
             pageRequestStatus);
  emberAfCoreFlush();

  otaPrintln("Upgrade Request Policy: %p (%d)", 
             upgradePolicyStrings[upgradePolicy],
             upgradePolicy);
  if (upgradePolicy == UPGRADE_SOON) {
    otaPrintln("  (%d seconds)", UPGRADE_SOON_TIME_SECONDS);
  }
  emberAfCoreFlush();
}

static boolean determineNextSoftwareVersion(int32u versionServerHas,
                                            int32u versionClientHas)
{
  // Our system here controls whether we tell the client to
  // (A) upgrade, because we have a newer version
  // (B) downgrade, because we have an older version we want to install
  // (C) reinstall, because we have the same version you have currently
  // (D) do nothing (no 'next' image is avaiable)

  switch (nextVersionPolicy) {
  case UPGRADE_IF_SERVER_HAS_NEWER:
    if (versionServerHas > versionClientHas) {
      return TRUE;
    }
    break;
  case DOWNGRADE_IF_SERVER_HAS_OLDER:
    if (versionServerHas < versionClientHas) {
      return TRUE;
    }
    break;
  case REINSTALL_IF_SERVER_HAS_SAME:
    if (versionServerHas == versionClientHas) {
      return TRUE;
    }
    break;
  case NO_NEXT_VERSION:
  default:
    break;
  }
  return FALSE;
}

int8u emberAfOtaServerQueryCallback(const EmberAfOtaImageId* currentImageId,
                                    int16u* hardwareVersion,
                                    EmberAfOtaImageId* nextUpgradeImageId)
{
  // This function is called by the OTA cluster server to determine what
  // the 'next' version of software is for a particular device requesting
  // a new download image.  The server returns a status code indicating
  // EMBER_ZCL_STATUS_NO_IMAGE_AVAILABLE, or EMBER_ZCL_STATUS_SUCCESS
  // (new image is available).  It then also fills in the 'nextUpgradeImageId'
  // structure with the appropriate version.

  // The server can use whatever criteria it wants to dictate what
  // the 'next' version is and if it is currently available.
  // This sample does this based on a global policy value.
  
  int8u status = EMBER_ZCL_STATUS_NO_IMAGE_AVAILABLE;
  boolean hardwareVersionMismatch = FALSE;

  otaPrintln("QueryNextImageRequest mfgId:0x%2x mfgDeviceId:0x%2x, fw:0x%4x", 
             currentImageId->manufacturerId, 
             currentImageId->imageTypeId, 
             currentImageId->firmwareVersion);
  *nextUpgradeImageId
    = emberAfOtaStorageSearchCallback(currentImageId->manufacturerId,
                                      currentImageId->imageTypeId,
                                      hardwareVersion);
  
  if (emberAfIsOtaImageIdValid(nextUpgradeImageId)) {
    // We only perform a check if both the query and the
    // file have hardware version(s).  If one or the other doesn't
    // have them, we assume a match is still possible.
    if (hardwareVersion) {
      EmberAfOtaHeader header;
      emberAfOtaStorageGetFullHeaderCallback(nextUpgradeImageId,
                              &header);
      if (header.fieldControl & HARDWARE_VERSIONS_PRESENT_MASK) {
        if (*hardwareVersion < header.minimumHardwareVersion
            || header.maximumHardwareVersion < *hardwareVersion) {
          otaPrintln("Hardware version 0x%02X does not fall within the min (0x%02X) and max (0x%02X) hardware versions in the file.",
                     *hardwareVersion,
                     header.minimumHardwareVersion,
                     header.maximumHardwareVersion);
          hardwareVersionMismatch = TRUE;
        }
      }
    }
    // "!hardwareVersionMismatch" does not mean the hardware
    // versions match.  It just means we don't *disqualify* the image
    // as a potential upgrade candidate because the hardware is out
    // of range.
    if (!hardwareVersionMismatch) {

      status = (determineNextSoftwareVersion(nextUpgradeImageId->firmwareVersion,
                                             currentImageId->firmwareVersion)
                ? EMBER_ZCL_STATUS_SUCCESS
                : EMBER_ZCL_STATUS_NO_IMAGE_AVAILABLE);
      if (status == EMBER_ZCL_STATUS_SUCCESS) {
        otaPrintln("Next fw version is: 0x%4X", 
                   nextUpgradeImageId->firmwareVersion);
      }
    }
  }
  return status;
}

int8u emberAfOtaServerBlockSizeCallback(EmberNodeId clientNodeId)
{
  // This function provides a way for the server to potentially
  // adjust the block size based on the client who is requesting.
  // In other words if we are using source routing we will limit
  // data returned by enough to put a source route into the message.

  EmberApsFrame apsFrame;
  int8u maxSize;
  apsFrame.options = EMBER_APS_OPTION_NONE;

  if (emberAfIsCurrentSecurityProfileSmartEnergy()) {
    apsFrame.options |= EMBER_APS_OPTION_ENCRYPTION;
  }

  maxSize = emberAfMaximumApsPayloadLength(EMBER_OUTGOING_DIRECT,
                                           clientNodeId,
                                           &apsFrame);
  maxSize -= IMAGE_BLOCK_RESPONSE_OVERHEAD;
  return maxSize;
}

int8u emAfOtaServerImageBlockRequestCallback(EmberAfImageBlockRequestCallbackStruct* data)
{
  if (SERVER_AND_CLIENT_SUPPORT_MIN_BLOCK_REQUEST
      == (data->bitmask & SERVER_AND_CLIENT_SUPPORT_MIN_BLOCK_REQUEST)) {
    if (data->minBlockRequestPeriod < imageBlockRequestMinRequestPeriodSeconds) {
      data->minBlockRequestPeriod = imageBlockRequestMinRequestPeriodSeconds;
      return EMBER_ZCL_STATUS_WAIT_FOR_DATA;
    }
  }

#if defined EM_AF_TEST_HARNESS_CODE
  // TEST Harness code
  // This will artificially delay once or abort the download as a
  // demonstration.  The test cases use this.

  if (imageBlockRequestPolicy == DELAY_DOWNLOAD_ONCE) {
    data->waitTimeMinutesResponse = IMAGE_BLOCK_REQUEST_DELAY_TIME_SECONDS;
    imageBlockRequestPolicy = SEND_BLOCK;    
    return EMBER_ZCL_STATUS_WAIT_FOR_DATA;
  } else if (data->offset > 0 && imageBlockRequestPolicy == ABORT_DOWNNLOAD) {
    // Only abort after the first block to insure the client handles
    // this correctly.
    return EMBER_ZCL_STATUS_ABORT;
  }
#endif

  return EMBER_ZCL_STATUS_SUCCESS;
}

boolean emberAfOtaServerUpgradeEndRequestCallback(EmberNodeId source,
                                                  int8u status,
                                                  int32u* returnValue,
                                                  const EmberAfOtaImageId* imageId)
{
  otaPrintln("Client 0x%2X indicated upgrade status: 0x%X",
             source,
             status);

  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    // If status != EMBER_ZCL_STATUS_SUCCESS then this callback is
    // only informative.  Return code will be ignored.
    return FALSE;
  }
  
  otaPrintln("Upgrade End Response: %p", upgradePolicyStrings[upgradePolicy]);
  
  if (upgradePolicy == UPGRADE_ABORT) {
    return FALSE;
  }
   
  *returnValue = upgradeTimes[upgradePolicy];
  return TRUE;
}

void emAfOtaServerSetQueryPolicy(int8u value)
{
  if (value <= QUERY_POLICY_MAX) {
    nextVersionPolicy = (NextVersionPolicy)value;
  }
}

void emAfOtaServerSetBlockRequestPolicy(int8u value)
{
#if defined EM_AF_TEST_HARNESS_CODE
  if (value <= BLOCK_REQUEST_POLICY_MAX) {
    imageBlockRequestPolicy = (ImageBlockRequestPolicy)value;
  }
#else
  otaPrintln("Unsupported.");
#endif
}

int8u emberAfOtaPageRequestServerPolicyCallback(void)
{
  return pageRequestStatus;
}

void emAfOtaServerSetPageRequestPolicy(int8u value)
{
  // This allows test code to be compiled with support for page request but
  // tell requesting devices it doesn't support it.
  pageRequestStatus = (value
                       ? EMBER_ZCL_STATUS_SUCCESS
                       : EMBER_ZCL_STATUS_UNSUP_CLUSTER_COMMAND);
}

void emAfOtaServerSetUpgradePolicy(int8u value)
{
  if (value <= UPGRADE_POLICY_MAX) {
    upgradePolicy = (UpgradePolicy)value;
  }
}

#if defined EM_AF_TEST_HARNESS_CODE

// There is no reason in production why certain block responses
// for a page request would need a callback, so that's why we wrap
// this whole function in a #define.
boolean emAfServerPageRequestTickCallback(int16u relativeOffset,
                                          int8u dataSize)
{
  int16u block = (relativeOffset + dataSize) / dataSize;
  emberAfCoreFlush();
  if (missedBlockModulus
      && (block % missedBlockModulus == 0)) {
    emberAfCoreFlush();
    otaPrintln("Inducing artificial failure for block %d", block);
    return FALSE;
  }
  return TRUE;
}

#endif


void emAfSetPageRequestMissedBlockModulus(int16u modulus)
{
#if defined(EM_AF_TEST_HARNESS_CODE)
  missedBlockModulus = modulus;
#else
  otaPrintln("Unsupported.");
#endif
}

void emAfOtaServerPolicySetMinBlockRequestPeriod(int16u minBlockRequestPeriodSeconds)
{
  imageBlockRequestMinRequestPeriodSeconds = minBlockRequestPeriodSeconds;
}
