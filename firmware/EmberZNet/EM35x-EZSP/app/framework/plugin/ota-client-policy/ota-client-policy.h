// *****************************************************************************
// * ota-client-policy.h
// *
// * Config for Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading device specific file.
// *
// * This file defines the interface for the customer's application to
// * control the behavior of the OTA client.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************


// Note: EMBER_AF_MANUFACTURER_CODE defined in client's config

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_IMAGE_TYPE_ID)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_IMAGE_TYPE_ID    0x5678
#endif

#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_FIRMWARE_VERSION 0x00000005L
#endif

#define SECONDS_IN_MS (1000L)
#define MINUTES_IN_MS (60 * SECONDS_IN_MS)
#define HOURS_IN_MS (60 * MINUTES_IN_MS)

// By default if hardware version is not defined, it is not used.
// Most products do not limit upgrade images based on the version.
// Instead they have different images for different hardware.  However
// this can provide support for an image that only supports certain hardware
// revision numbers.
#define EMBER_AF_INVALID_HARDWARE_VERSION 0xFFFF
#if !defined(EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_HARDWARE_VERSION)
  #define EMBER_AF_PLUGIN_OTA_CLIENT_POLICY_HARDWARE_VERSION EMBER_AF_INVALID_HARDWARE_VERSION
#endif

//------------------------------------------------------------------------------
// Custom Application Callbacks

enum {
  EMBER_AF_OTA_DOWNLOAD_AND_VERIFY_SUCCESS = 0,
  EMBER_AF_OTA_DOWNLOAD_TIME_OUT           = 1,
  EMBER_AF_OTA_VERIFY_FAILED               = 2,
  EMBER_AF_OTA_SERVER_ABORTED              = 3,
  EMBER_AF_OTA_CLIENT_ABORTED              = 4,
};
typedef int8u EmberAfOtaDownloadResult;


