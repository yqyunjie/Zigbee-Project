// *****************************************************************************
// * ota-client-policy.c
// *
// * This file handle how the application can configure and interact with the OTA
// * cluster.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************


#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/util/util.h"
#include "app/framework/util/common.h"

#include "ota-client-policy.h"

#if defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_CLIENT)

// Right now this sample policy only supports a single set of version 
// information for the device, which will be supplied to the OTA cluster in 
// order to query and download a new image when it is available.  This does not 
// support multiple products with multiple download images.

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_INCLUDE_HARDWARE_VERSION)
  #define HARDWARE_VERSION EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_HARDWARE_VERSION
#else
  #define HARDWARE_VERSION EMBER_AF_INVALID_HARDWARE_VERSION
#endif

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------

void emberAfOtaClientVersionInfoCallback(EmberAfOtaImageId* currentImageInfo,
                                         int16u* hardwareVersion)
{
  // This callback is fired when a new query and download is initiated.
  // The application will fill in the currentImageInfo with their manufacturer 
  // ID, image type ID, and current software version number to use in that
  // query. The deviceSpecificFileEui64 can be ignored.

  // It may be necessary to dynamically determine this data by talking to
  // another device, as is the case with a host talking to an NCP device.

  // The OTA client plugin will cache the data returned by this callback
  // and use it for the subsequent transaction, which could be a query
  // or a query and download.  Therefore it is possible to instruct the 
  // OTA client cluster code to query about multiple images by returning
  // different values.

  MEMSET(currentImageInfo, 0, sizeof(EmberAfOtaImageId));
  currentImageInfo->manufacturerId  = EMBER_AF_MANUFACTURER_CODE;
  currentImageInfo->imageTypeId     = EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_IMAGE_TYPE_ID;
  currentImageInfo->firmwareVersion = EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION;

  if (hardwareVersion != NULL) {
    *hardwareVersion = HARDWARE_VERSION;
  }
}

EmberAfImageVerifyStatus emberAfOtaClientCustomVerifyCallback(boolean newVerification,
                                                              const EmberAfOtaImageId* id)
{
  // Manufacturing specific checks can be made to the image in this function to
  // determine if it is valid.  This function is called AFTER cryptographic 
  // checks have passed.  If the cryptographic checks failed, this function will
  // never be called.
  
  // The function shall return one of the following based on its own 
  // verification process.
  // 1) EMBER_AF_IMAGE_GOOD - the image has passed all checks
  // 2) EMBER_AF_IMAGE_BAD  - the image is not valid 
  // 3) EMBER_AF_IMAGE_VERIFY_IN_PROGRESS - the image is valid so far, but more
  //      checks are needed.  This callback shall be re-executed later to 
  //      continue verification.  This allows other code in the framework to run.

  // Note that EBL verification is an SoC-only feature.

#if !defined(EZSP_HOST) && defined(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_EBL_VERIFICATION)
  int16u pages;

  // For sleepies, we must re-initalize the EEPROM / bootloader
  // after each nap/hibernate.  This call will only re-initalize the EEPROM
  // if that is the case.
  emberAfEepromInitCallback();

  if (newVerification) {
    otaPrintln("Starting EBL verification");
    halAppBootloaderImageIsValidReset();
  }
  pages = halAppBootloaderImageIsValid();
  if (pages == BL_IMAGE_IS_VALID_CONTINUE) {
    return EMBER_AF_IMAGE_VERIFY_IN_PROGRESS;
  } else if (pages == 0) {
    otaPrintln("EBL failed verification.");
    return EMBER_AF_IMAGE_BAD;
  }
  otaPrintln("EBL passed verification.");
#endif

  return EMBER_AF_IMAGE_GOOD;
}

boolean emberAfOtaClientDownloadCompleteCallback(EmberAfOtaDownloadResult result,
                                                 const EmberAfOtaImageId* id)
{
  // At this point the image has been completely downloaded and cryptographic 
  // checks (if applicable) have been performed.
  // Manufacturer verification callback has also been called and passed.

  if (result != EMBER_AF_OTA_DOWNLOAD_AND_VERIFY_SUCCESS) {
    emberAfOtaBootloadClusterPrintln("Download failed.");
    return TRUE;   // return value is ignored
  }

  // If the client wants to abort for some reason then it can do so here
  // and return FALSE.  Otherwise it should give the "go ahead" by returning
  // TRUE.
  
  return TRUE;
}

void emberAfOtaClientBootloadCallback(const EmberAfOtaImageId* id)
{
  // OTA Server has told us to bootload.
  // Any final preperation prior to the bootload should be done here.
  // It is assumed that the device will reset in most all cases.

  int32u offset;
  int32u endOffset;

  if (EMBER_AF_OTA_STORAGE_SUCCESS
      != emAfOtaStorageGetTagOffsetAndSize(id,
                                           OTA_TAG_UPGRADE_IMAGE,
                                           &offset,
                                           &endOffset)) {
    emberAfCoreFlush();
    otaPrintln("Image does not contain an Upgrade Image Tag (0x%2X).  Skipping upgrade.", 
               OTA_TAG_UPGRADE_IMAGE);
    return;
  }

  otaPrintln("Executing bootload callback.");
  emberSerialWaitSend(APP_SERIAL);

  // This routine will NOT return unless we failed to launch the bootloader.
  emberAfOtaBootloadCallback(id, OTA_TAG_UPGRADE_IMAGE);
}

#endif // defined(ZCL_USING_OTA_BOOTLOAD_CLUSTER_CLIENT)
