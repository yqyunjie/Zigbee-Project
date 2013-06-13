// *******************************************************************
// * ota-server-page-request.c
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading specific file.
// *
// * This handles the optional feature of a device requesting a
// * a full page (of EEPROM) and getting multiple image block responses.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "ota-server.h"
#include "app/framework/util/util.h"
#include "app/framework/util/common.h"
#include "app/framework/plugin/ota-server-policy/ota-server-policy.h"

#if defined(EMBER_AF_PLUGIN_OTA_SERVER_PAGE_REQUEST_SUPPORT)

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------

#define MAXIMUM_PAGE_SIZE 1024

static EmberNodeId requesterNodeId = EMBER_NULL_NODE_ID;
static int32u requesterBaseOffset;
static int16u requesterPageSize;
static int16u totalBytesSent;
static int8u requesterMaxDataSize;
static EmberAfOtaImageId requesterImageId;
static boolean handlingPageRequest = FALSE;
static int16u requesterResponseSpacing;

#define SHORTEST_SEND_RATE 10L  // ms.

// -----------------------------------------------------------------------------
// Forward Declarations
// -----------------------------------------------------------------------------

static void sendBlockRequest(void);
static void abortPageRequest(void);

#if defined(EM_AF_TEST_HARNESS_CODE)
  #define pageRequestTickCallback(x,y)             \
    emAfServerPageRequestTickCallback(x,y )
#else
  #define pageRequestTickCallback(x,y) TRUE
#endif

// -----------------------------------------------------------------------------

int8u emAfOtaPageRequestHandler(int8u endpoint,
                                const EmberAfOtaImageId* id,
                                int32u offset,
                                int8u maxDataSize,
                                int16u pageSize,
                                int16u responseSpacing)
{
  int32u totalSize;
  int8u status;
  emberAfOtaBootloadClusterPrintln("RX ImagePageReq mfgId:%2x imageType:%2x, file:%4x, offset:%4x dataSize:%d pageSize%2x spacing:%d",
                                   id->manufacturerId, 
                                   id->imageTypeId, 
                                   id->firmwareVersion, 
                                   offset,
                                   maxDataSize,
                                   pageSize, 
                                   responseSpacing);

  // Only allow 1 page request at a time.
  if (requesterNodeId != EMBER_NULL_NODE_ID) {
    otaPrintln("2nd page request not supported");
    return EMBER_ZCL_STATUS_FAILURE;
  }

  status = emberAfOtaPageRequestServerPolicyCallback();
  if (status) {
    return status;
  }
  
  MEMCOPY(&requesterImageId, id, sizeof(EmberAfOtaImageId));
  totalSize = emberAfOtaStorageGetTotalImageSizeCallback(id);

  if (totalSize == 0) {
    return EMBER_ZCL_STATUS_NOT_FOUND;
  } else if (offset > totalSize || (maxDataSize > pageSize)) {
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }

  requesterNodeId = emberAfResponseDestination;
  requesterBaseOffset = offset;
  requesterPageSize = pageSize;
  requesterMaxDataSize = maxDataSize;
  requesterResponseSpacing = (responseSpacing < SHORTEST_SEND_RATE
                              ? SHORTEST_SEND_RATE
                              : responseSpacing);
  totalBytesSent = 0;
  
  emAfOtaPageRequestTick(endpoint);

  return EMBER_ZCL_STATUS_SUCCESS;
}

void emAfOtaPageRequestTick(int8u endpoint)
{
  if (requesterNodeId == EMBER_NULL_NODE_ID) {
    return;
  }

  sendBlockRequest();
  emberAfScheduleClusterTick(endpoint,
                             ZCL_OTA_BOOTLOAD_CLUSTER_ID,
                             EMBER_AF_SERVER_CLUSTER_TICK,
                             requesterResponseSpacing,
                             EMBER_AF_OK_TO_NAP);
}

boolean emAfOtaPageRequestErrorHandler(void)
{
  if (handlingPageRequest) {
    abortPageRequest();
    return TRUE;
  }
  return FALSE;
}

static void abortPageRequest(void)
{
  requesterNodeId = EMBER_NULL_NODE_ID;
}

static void sendBlockRequest(void)
{
  int8u bytesSentThisTime = 0;
  int32u totalSize = emberAfOtaStorageGetTotalImageSizeCallback(&requesterImageId);
  int8u maxDataToSend;
  int32u bytesLeft;

  if (totalSize == 0) {
    // The image no longer exists.  
    abortPageRequest();
    return;
  }

  bytesLeft = totalSize - (requesterBaseOffset + totalBytesSent);
  
  // 3 possibilities for how much data to send
  //   - Up to requesterMaxSize
  //   - As many bytes are left in the file
  //   - As many bytes are left to fill up client's page size
  if ((requesterPageSize - totalBytesSent) > requesterMaxDataSize) {
    maxDataToSend = (bytesLeft > requesterMaxDataSize
                     ? requesterMaxDataSize
                     : (int8u)bytesLeft);
  } else {
    maxDataToSend = requesterPageSize - totalBytesSent;
  }

  emberAfResponseDestination = requesterNodeId;
  handlingPageRequest = TRUE;

  // To enable sending as fast as possible without the receiver
  // having to waste battery power by responding, we clear the
  // retry flag.
  emberAfResponseApsFrame.options &= ~EMBER_APS_OPTION_RETRY;

  if (pageRequestTickCallback(totalBytesSent,
                              requesterMaxDataSize)) {
    // Simulate a block request to the server that we will generate
    // a response to.
    EmberAfImageBlockRequestCallbackStruct callbackStruct;
    MEMSET(&callbackStruct, 0, sizeof(EmberAfImageBlockRequestCallbackStruct));
    callbackStruct.source = requesterNodeId;
    callbackStruct.id = &requesterImageId;
    callbackStruct.offset = requesterBaseOffset + totalBytesSent;
    callbackStruct.maxDataSize = maxDataToSend;

    // This is implied by the MEMSET().  We don't care about those options
    // when simulating the image block request commands.
    //    callbackStruct.bitmask = EMBER_AF_IMAGE_BLOCK_REQUEST_OPTIONS_NONE

    bytesSentThisTime = emAfOtaImageBlockRequestHandler(&callbackStruct);
    emberAfSendResponse();
  } else {
    bytesSentThisTime += maxDataToSend;
  }
  handlingPageRequest = FALSE;
  if (bytesSentThisTime == 0) {
    emberAfOtaBootloadClusterPrintln("Failed to send image block for page request");
    // We don't need to call abortPageRequest();
    // here because the server will call into our otaPageRequestErrorHandler()
    // if that occurs.
  } else {
    totalBytesSent += bytesSentThisTime;

    if (totalBytesSent >= totalSize
        || totalBytesSent >= requesterPageSize) {
      emberAfOtaBootloadClusterPrintln("Done sending blocks for page request.");
      abortPageRequest();
    }
  }
}

boolean emAfOtaServerHandlingPageRequest(void)
{
  return handlingPageRequest;
}

//------------------------------------------------------------------------------
#else // No page request support

int8u emAfOtaPageRequestHandler(int8u endpoint,
                                const EmberAfOtaImageId* id,
                                int32u offset,
                                int8u maxDataSize,
                                int16u pageSize,
                                int16u responseSpacing)
{
  return EMBER_ZCL_STATUS_UNSUP_CLUSTER_COMMAND;
}

boolean emAfOtaPageRequestErrorHandler(void)
{
  return FALSE;
}

void emAfOtaPageRequestTick(int8u endpoint)
{
}

boolean emAfOtaServerHandlingPageRequest(void)
{
  return FALSE;
}


#endif //  defined (EMBER_AF_PLUGIN_OTA_SERVER_PAGE_REQUEST_SUPPORT)
