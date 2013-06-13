// *****************************************************************************
// * ota-client-page-request.c
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading device specific file.
// * This file handles the page request feature for the client.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "ota-client-page-request.h"
#include "ota-client.h"

// This is by no means a super flexible implementation of page requests.
// The problem we have to try to solve is how to reasonably keep track
// of lots of blocks of data that could vary in size.

// The client needs to know how many blocks are going to be sent by the server
// so that it can keep track of ones it missed.  However the server
// is the one that ultimately dictates the size.  Therefore we only support a
// server that uses the same block size as we suggest to it (via the page
// request).  To insure that the server doesn't change the block size
// on us (due to source routing), we ask for smaller blocks than the 
// 50 byte maximum we know can fit in an Image Block Response packet without
// a source route.

// Also 32-bit divide and modulus can be expensive (e.g. on the XAP) so wherever
// possible we try to use 16-bit math instead.

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_USE_PAGE_REQUEST)

//------------------------------------------------------------------------------
// Globals

#if EM_AF_PAGE_REQUEST_BLOCK_SIZE != 32
  // The code only works with this block size
  #error "EM_AF_PAGE_REQUEST_BLOCK_SIZE MUST BE 32"
#endif

#define BITS_PER_BITMASK 16

#if defined (PAGE_REQUEST_TEST)
  static int16u PAGE_SIZE = EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE;
  static int16u MAX_BLOCKS_IN_PAGE = 1024 / EM_AF_PAGE_REQUEST_BLOCK_SIZE;

  static int16u BITMASK_COUNT;

  // Set the bitmask count to the largest possible size for initialization
  // of the blocksReceivedBitmask array.  We will dynamically set the
  // BITMASK_COUNT for testing only.
  #define BITMASK_COUNT_CONST (4096 / 16)

#else

  // Supported page sizes:  1024, 2048, and 4096
  // These are the only ones we test.
  #define PAGE_SIZE EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE

  #define MAX_BLOCKS_IN_PAGE (PAGE_SIZE / EM_AF_PAGE_REQUEST_BLOCK_SIZE)
  #define BITMASK_COUNT (MAX_BLOCKS_IN_PAGE / 16\
                         + ((PAGE_SIZE % BITS_PER_BITMASK == 0) \
                            ? 0                                 \
                            : 1))
  #define BITMASK_COUNT_CONST BITMASK_COUNT
#endif

static int16u blocksReceivedBitmask[BITMASK_COUNT_CONST];
static EmAfPageRequestClientStatus pageRequestStatus = EM_AF_NO_PAGE_REQUEST;
static int32u pageRequestOffset = 0;

// For the last page, the number of blocks may be less than MAX_BLOCKS_IN_PAGE
static int16u blocksInThisPage;

#define handlingPageRequest() (pageRequestStatus != EM_AF_NO_PAGE_REQUEST)

// #define EXTRA_DEBUG_PRINTING
#if defined (EXTRA_DEBUG_PRINTING) || defined(EMBER_SCRIPTED_TEST)
  #define EXTRA_DEBUG(x) do { x; } while (0)
#else
  #define EXTRA_DEBUG(X)
#endif

// It doesn't seem to add a lot of benefit in exposing this formally in 
// AppBuilder.  However if it matters, it can be overriden.
// Normally it is always set in this file.
#if !defined(PAGE_REQUEST_MISSED_PACKET_DELAY_MS)
  #define PAGE_REQUEST_MISSED_PACKET_DELAY_MS 10L
#endif

#define EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_TIMEOUT_MS \
  (EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_TIMEOUT_SECONDS * MILLISECOND_TICKS_PER_SECOND)

//------------------------------------------------------------------------------
// Functions

int32u emAfInitPageRequestClient(int32u offsetForPageRequest, 
                                 int32u totalImageSize)
{
  int8u i;
  for (i = 0; i < BITMASK_COUNT; i++) { 
   blocksReceivedBitmask[i] = 0;
  }
  pageRequestOffset = offsetForPageRequest;

  if (offsetForPageRequest + PAGE_SIZE > totalImageSize) {
    int16u remainingSize = (int16u)(totalImageSize - offsetForPageRequest);
    blocksInThisPage = remainingSize / EM_AF_PAGE_REQUEST_BLOCK_SIZE;
    blocksInThisPage += ((remainingSize % EM_AF_PAGE_REQUEST_BLOCK_SIZE == 0)
                         ? 0
                         : 1);
  } else {
    blocksInThisPage = MAX_BLOCKS_IN_PAGE;
  }
  pageRequestStatus = EM_AF_WAITING_PAGE_REQUEST_REPLIES;
  return EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_TIMEOUT_MS;
}

EmAfPageRequestClientStatus emAfGetCurrentPageRequestStatus(void)
{
  return pageRequestStatus;
}

void emAfPageRequestTimerExpired(void)
{
  if (pageRequestStatus == EM_AF_WAITING_PAGE_REQUEST_REPLIES) {
    otaPrintln("Page request timer expired.  Checking for missed blocks."); 
    pageRequestStatus = EM_AF_RETRY_MISSED_PACKETS;
  }
}

int32u emAfGetPageSize(void)
{
  return PAGE_SIZE;
}

int32u emAfGetPageRequestMissedPacketDelayMs(void)
{
  return PAGE_REQUEST_MISSED_PACKET_DELAY_MS;
}

static void convertBitNumberToBitmask(int16u absoluteBitNumber,
                                      int8u* bitmaskIndex,
                                      int16u* mask)
{
  int16u relativeBitPosition;
  *bitmaskIndex = absoluteBitNumber / BITS_PER_BITMASK;
  relativeBitPosition = (absoluteBitNumber - (*bitmaskIndex * BITS_PER_BITMASK));
  *mask = 1 << relativeBitPosition;
}

#if defined(__ICCARM__)
// Bug: 12578:  Workaround for IAR compiler bug
//   The IAR compiler 5.4 has a bug in it where high optimizations will
//   mistakenly remove a NULL pointer check when inside a FOR loop.   So we
//   turn off high optimizations for this function since it contains that kind
//   of code.
#pragma optimize=medium
#endif // defined(__ICCARM__)
static int16u numBlocksReceived(int32u* returnOffset)
{
  int16u blockNumber;
  int8u bitmaskIndex = 0;
  int16u bitValue;
  int16u blocksReceived = 0;
  if (returnOffset) {
    *returnOffset = pageRequestOffset;
  }
  for (blockNumber = 0; blockNumber < blocksInThisPage; blockNumber++) {
    if ((blockNumber >= BITS_PER_BITMASK)
        && (blockNumber % BITS_PER_BITMASK == 0)) {
      bitmaskIndex++;
    }
    bitValue = (blocksReceivedBitmask[bitmaskIndex] 
                >> (blockNumber % BITS_PER_BITMASK));
    if ((bitValue & 0x0001) == 0) {
      return blocksReceived;
    }
    if (returnOffset) {
      *returnOffset += EM_AF_PAGE_REQUEST_BLOCK_SIZE;
    }
    blocksReceived++;
  }
  return blocksReceived;
}

static boolean allBlocksReceived(int16u numReceived)
{
  return (numReceived == blocksInThisPage);
}

void emAfAbortPageRequest(void)
{
  // Note:  Don't zero out the pageRequestOffset so it can be retrieved later.
  pageRequestStatus = EM_AF_NO_PAGE_REQUEST;
}

int32u emAfGetFinishedPageRequestOffset(void)
{
  return pageRequestOffset + EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SIZE;
}

EmAfPageRequestClientStatus emAfNoteReceivedBlockForPageRequestClient(int32u offset)
{
  int16u mask;
  int16u smallOffset;
  int8u bitmaskIndex;
  int16u absoluteBitNumber;
  if (!handlingPageRequest()) {
    return EM_AF_PAGE_REQUEST_ERROR;
  }
  if (offset < pageRequestOffset) {
    //    otaPrintln("Received offset 0x%4X is less than current page request offset 0x%4X, ignoring.",
    //               offset,
    //               pageRequestOffset);
    // We ignore this because we may receive blocks out of order, from other retries, etc.
    return EM_AF_BLOCK_ALREADY_RECEIVED;
  }
  offset -= pageRequestOffset;
  
  // To avoid 32-bit divide and modulus, since they are expensive on the XAP,
  // we cast into 16-bit.  Our page size is assumed to be a 16-bit number
  // so this is fine.
  smallOffset = (int16u)offset;

  if (smallOffset % EM_AF_PAGE_REQUEST_BLOCK_SIZE != 0) {
    otaPrintln("ERROR: Block offset for page request is not a multiple of %d",
               EM_AF_PAGE_REQUEST_BLOCK_SIZE);
    return EM_AF_PAGE_REQUEST_ERROR;
  }
  absoluteBitNumber = smallOffset / EM_AF_PAGE_REQUEST_BLOCK_SIZE;

  convertBitNumberToBitmask(absoluteBitNumber, &bitmaskIndex, &mask);
  if ((blocksReceivedBitmask[bitmaskIndex] & mask)) {
    EXTRA_DEBUG(otaPrintln("Already received block %d",
                           absoluteBitNumber + 1));
    return EM_AF_BLOCK_ALREADY_RECEIVED;
  }
  EXTRA_DEBUG(otaPrintln("Received block %d of %d in page request",
                         absoluteBitNumber + 1,
                         blocksInThisPage));
  
  blocksReceivedBitmask[bitmaskIndex] |= mask;

  if (allBlocksReceived(numBlocksReceived(NULL))) {
    EXTRA_DEBUG(otaPrintln("All blocks received.  Page request complete."));
    emAfAbortPageRequest();
    return EM_AF_PAGE_REQUEST_COMPLETE;
  }
  
  return pageRequestStatus;
}

EmAfPageRequestClientStatus emAfNextMissedBlockRequestOffset(int32u* nextOffset)
{
  int16u blocksReceived;
  if (pageRequestStatus != EM_AF_RETRY_MISSED_PACKETS) {
    return EM_AF_PAGE_REQUEST_ERROR;
  }

  blocksReceived = numBlocksReceived(nextOffset);
  if (allBlocksReceived(blocksReceived)) {
    pageRequestStatus = EM_AF_PAGE_REQUEST_COMPLETE;
  } else if (blocksReceived == 0) {
    // If we didn't get ANY blocks after a page request, odds are the
    // OTA server is unreachable.  No point in retrying.
    otaPrintln("All blocks missed.  Server seems unreachable.  Giving up.");
    emAfAbortPageRequest();
    return EM_AF_PAGE_REQUEST_ERROR;
  }
  return pageRequestStatus;
}

boolean emAfHandlingPageRequestClient(void)
{
  return handlingPageRequest();
}

//------------------------------------------------------------------------------

#else  // !defined(EMBER_AF_PLUGIN_OTA_CLIENT_PAGE_REQUEST_SUPPORT)

int32u emAfInitPageRequestClient(int32u offsetForPageRequest, 
                                 int32u totalImageSize)
{
  return 0;
}

boolean emAfHandlingPageRequestClient(void)
{
  return FALSE;
}

void emAfPageRequestTimerExpired(void)
{}

EmAfPageRequestClientStatus emAfNoteReceivedBlockForPageRequestClient(int32u offset)
{
  return EM_AF_PAGE_REQUEST_ERROR;
}

EmAfPageRequestClientStatus emAfNextMissedBlockRequestOffset(int32u* nextOffset)
{
  return EM_AF_PAGE_REQUEST_ERROR;
}

EmAfPageRequestClientStatus emAfGetCurrentPageRequestStatus(void)
{
  return EM_AF_NO_PAGE_REQUEST;
}

int32u emAfGetPageRequestMissedPacketDelayMs(void)
{
  return 0;
}

int32u emAfGetFinishedPageRequestOffset(void)
{
  return 0;
}

void emAfAbortPageRequest(void)
{
}

#endif // EMBER_AF_PLUGIN_OTA_CLIENT_USE_PAGE_REQUEST
