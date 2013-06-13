// *****************************************************************************
// * ota-storage-eeprom-driver-page-erase.c
// *
// * This code is intended for EEPROM devices that do not support 
// * read-modify-write and must perform a page erase prior to writing data.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"

#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/plugin/ota-client/ota-client.h"
#include "app/framework/plugin/eeprom/eeprom.h"

//#define DEBUG_PRINT
#define OTA_STORAGE_EEPROM_INTERNAL_HEADER
#include "ota-storage-eeprom.h"
#undef OTA_STORAGE_EEPROM_INTERNAL_HEADER

#if !defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_READ_MODIFY_WRITE_SUPPORT)

#if defined(EMBER_TEST)
#include "hal/micro/unix/simulation/fake-eeprom.h"
#endif

//------------------------------------------------------------------------------
// Globals

static int32s lastRecordedByteMaskIndex = -1;
static boolean lastRecordedByteMaskIndexKnown = FALSE;

static int32u currentEraseOffset;
static int32u endEraseOffset;
static boolean newEraseOperation;

static EmberAfEventSleepControl storedSleepControl;

// this arbitrary size is to limit the amount we store on the call stack
#define BYTE_MASK_READ_SIZE 20

//------------------------------------------------------------------------------
// Forward declarations

#define startEraseOperation(begin, end) \
  eraseOperation(TRUE, /* start new erase? */ \
                 (begin),                     \
                 (end))

#define continueEraseOperation() \
  eraseOperation(FALSE,   /* start new erase?       */                \
                 0,       /* begin offset (ignored) */                \
                 0)       /* end offset (ignored)   */                \

//------------------------------------------------------------------------------

// We want to get the log(2, PAGE_SIZE) so that we can use bitwise shifts
// instead of multiple and divide for various page size related operations.
// For the xap, 32-bit divide/modulus requires a software library and eats up
// a lot of flash.
static int8u determinePageSizeLog(void)
{
  int8u pageSizeLog;
  for (pageSizeLog = 0;
       (1 << pageSizeLog) < (emberAfPluginEepromInfo()->pageSize); 
       pageSizeLog++) {
  }
  //  debugPrint("PageSizeLog: %d", pageSizeLog);
  return pageSizeLog;
}

static boolean checkDelay(boolean mustSetTimer)
{
  if (emberAfPluginEepromBusy() || mustSetTimer) {
    int32u delay = emberAfPluginEepromInfo()->pageEraseMs >> 2;
    if (delay == 0) {
      delay = 1;
    }
    debugPrint("Waiting %d ms for erase to complete.", delay);
    emberAfEventControlSetDelay(&emberAfPluginOtaStorageSimpleEepromPageEraseEventControl,
                                delay);
    return TRUE;
  }

  return FALSE;
}

// Returns TRUE for success (erase operation continuing or completed)
// Returns FALSE for error (erase not started).
static boolean eraseOperation(boolean startNewErase,
                              int32u beginOffset,
                              int32u endOffset)
{
  boolean success = TRUE;

  EMBER_TEST_ASSERT(!startNewErase
                    || (startNewErase
                        && emberAfPluginOtaStorageSimpleEepromPageEraseEventControl.status == EMBER_EVENT_INACTIVE));

  // In case the first time we are called the EEPROM is busy,
  // we will delay.  However we haven't erased the first page
  // yet so we must take care not to increment the offset yet.

  if (startNewErase) {
    newEraseOperation = TRUE;
    currentEraseOffset = beginOffset;
    endEraseOffset = endOffset;
    otaPrintln("Starting erase from offset 0x%4X to 0x%4X",
               beginOffset,
               endEraseOffset);
    storedSleepControl = emberAfGetDefaultSleepControlCallback();
    emberAfSetDefaultSleepControlCallback(EMBER_AF_STAY_AWAKE);
  }

  if (checkDelay(FALSE)) {  // must set timer?
    return TRUE;
  }

  if (!newEraseOperation) {
    currentEraseOffset += emberAfPluginEepromInfo()->pageSize;
  }

  if (currentEraseOffset < endEraseOffset) {
    int8u status;
    debugPrint("Erasing page %d of %d",
               (currentEraseOffset >> determinePageSizeLog()) + 1,
               (endEraseOffset >> determinePageSizeLog()));
    status = emberAfPluginEepromErase(currentEraseOffset, emberAfPluginEepromInfo()->pageSize);
    success = (status == EEPROM_SUCCESS);
    newEraseOperation = FALSE;
    if (success) {
      checkDelay(TRUE); // must set timer?
      return TRUE;
    }
    otaPrintln("Could not start ERASE! (0x%X)", status);
  }

  emberAfSetDefaultSleepControl(storedSleepControl);

  otaPrintln("EEPROM Erase complete");

  if (!emAfOtaStorageCheckDownloadMetaData()) {
    // This was a full erase that wiped the meta-data.
    emAfOtaStorageWriteDownloadMetaData();
  }

  emberAfPluginOtaStorageSimpleEepromEraseCompleteCallback(success);
  return TRUE;
}

static boolean isMultipleOfPageSize(int32u address)
{
  int32u pageSizeBits = ((1 << determinePageSizeLog()) - 1);
  return ((pageSizeBits & address) == 0);
}

void emAfOtaStorageEepromInit(void)
{
  int16u expectedCapabilities = (EEPROM_CAPABILITIES_PAGE_ERASE_REQD
                                 | EEPROM_CAPABILITIES_ERASE_SUPPORTED);
  int32u spaceReservedForOta = (EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_END
                                - EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START);
  const HalEepromInformationType *info = emberAfPluginEepromInfo();

  // NOTE: if the info pointer is NULL it's a good indicator that your data
  // flash isn't properly connected and jumpered in or that your bootloader
  // is too old to support EEPROM info.
  assert(info != NULL);
  assert(expectedCapabilities
         == (info->capabilitiesMask & expectedCapabilities));
  assert(isMultipleOfPageSize(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START));
  assert(isMultipleOfPageSize(spaceReservedForOta));

  // Need to make sure that the bytemask used to store each
  // fully downloaded page is big enough to hold all the pages we have been
  // allocated.
  assert((MAX_BYTEMASK_LENGTH / emAfOtaStorageDriverGetWordSize())
         >= (spaceReservedForOta >> determinePageSizeLog()));
}

void emberAfPluginOtaStorageSimpleEepromPageEraseEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginOtaStorageSimpleEepromPageEraseEventControl);
  continueEraseOperation();
}

static int32s getByteMaskIndexFromEeprom(void)
{
  int8u byteMask[BYTE_MASK_READ_SIZE];
  int32u readOffset = IMAGE_INFO_START + SAVED_DOWNLOAD_OFFSET_INDEX;
  int16u byteMaskIndex;
  int8u wordSize = emAfOtaStorageDriverGetWordSize();

  for (byteMaskIndex = 0; 
       byteMaskIndex < MAX_BYTEMASK_LENGTH;
       byteMaskIndex += BYTE_MASK_READ_SIZE,
         readOffset  += BYTE_MASK_READ_SIZE) {
    int8u i;
    int8u status = emberAfPluginEepromRead(readOffset,
                                  byteMask,
                                  BYTE_MASK_READ_SIZE);
    debugPrint("Bytemask read status: 0x%X", status);
    EMBER_TEST_ASSERT(status == 0);
    
    if (byteMaskIndex == 0 && byteMask[0] == 0xFF) {
      debugFlush();
      debugPrint("All bytes in bytemask erased, assuming index of -1");
      return -1;
    }

    for (i = 0; i < BYTE_MASK_READ_SIZE; i+=wordSize) {
      if (byteMask[i] == 0xFF) {
        int16u index = (byteMaskIndex + i - 1) / wordSize;
        debugPrint("Last Download offset Bytemask index: %d", 
                   index);
        return (index);
      }
    }
  }
  
  debugPrint("Error in determining bytemask index, assuming -1");
  return -1;
}

// The bytemask notes the real EEPROM offset of the pages that have been fully 
// downloaded.  Each downloaded page is recorded as a 0 byte.  The OTA offset
// is determined based on the SOC_BOOTLOADING_SUPPORT.  In that case the
// first page is considered to be the combination of the portion of the OTA
// image at the bottom of the EEPROM space (the OTA header) and the first
// full flash page at the top of the EEPROM space (the EBL and its data).
// Without SOC bootloading support the OTA offset is equivalent to number
// of EEPROM pages written minus the overhead of the meta-data (namely this
// bytemask and some other data).
static int32u getOffsetFromByteMaskIndex(int32s byteMaskIndex)
{
  // To convert to the number of fully written pages from the bytemask index
  // we must add 1.
  int32s writtenPages = byteMaskIndex + 1;
  int32u otaOffset = (((int32u)(writtenPages)) << determinePageSizeLog());

  debugPrint("Unadjusted offset:    0x%4X", otaOffset);

  if (otaOffset != 0) {
#if defined(SOC_BOOTLOADING_SUPPORT)
    otaOffset += emAfGetEblStartOffset();
#else
    otaOffset -= OTA_HEADER_INDEX;
#endif
  }

  debugFlush();
  debugPrint("Last OTA Download offset: 0x%4X", otaOffset);
  debugFlush();

  return otaOffset;
}

static int32s getByteMaskIndexFromOtaOffset(int32u otaOffset)
{
  int32s adjustment;

#if defined(SOC_BOOTLOADING)
  adjustment = emAfGetEblStartOffset();
#else
  adjustment = 0 - OTA_HEADER_INDEX;
#endif

  if (otaOffset < (emberAfPluginEepromInfo()->pageSize + adjustment)) {
    return -1;
  }

  return (((otaOffset + adjustment) >> determinePageSizeLog()) - 1);
}

void emAfStorageEepromUpdateDownloadOffset(int32u otaOffsetNew, boolean finalOffset)
{
  int32s byteMaskIndexNew = getByteMaskIndexFromOtaOffset(otaOffsetNew);

  if (finalOffset
      && byteMaskIndexNew == lastRecordedByteMaskIndex) {
    byteMaskIndexNew++;
  }
  
  if (byteMaskIndexNew > lastRecordedByteMaskIndex) {
    int8u status;
    int8u byteArray[2] = { 0, 0 };

    debugFlush();
    debugPrint("Writing Last Download offset bytemask, new (old): %d (%d)",
               byteMaskIndexNew,
               lastRecordedByteMaskIndex);
    debugFlush();
    debugPrint("OTA Offsets, new (old): 0x%4X (0x%4X)",
               otaOffsetNew,
               getOffsetFromByteMaskIndex(lastRecordedByteMaskIndex));
    debugFlush();

    status = emberAfPluginEepromWrite((IMAGE_INFO_START
                              + SAVED_DOWNLOAD_OFFSET_INDEX
                              + (byteMaskIndexNew
                                 * emAfOtaStorageDriverGetWordSize())),
                             byteArray,
                             emAfOtaStorageDriverGetWordSize());
    debugPrint("EEPROM Write status: 0x%X", status);
    EMBER_TEST_ASSERT(status == 0);

    lastRecordedByteMaskIndex = getByteMaskIndexFromEeprom();
    
    EMBER_TEST_ASSERT(lastRecordedByteMaskIndex == byteMaskIndexNew);
  }
}

EmberAfOtaStorageStatus emberAfOtaStorageDriverInvalidateImageCallback(void)
{
  lastRecordedByteMaskIndex = -1;
  lastRecordedByteMaskIndexKnown = FALSE;

  return (startEraseOperation(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START,
                              EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_END)
          ? EMBER_AF_OTA_STORAGE_OPERATION_IN_PROGRESS
          : EMBER_AF_OTA_STORAGE_ERROR);
}

int32u emberAfOtaStorageDriverRetrieveLastStoredOffsetCallback(void)
{
  if (!emAfOtaStorageCheckDownloadMetaData()) {
    return 0;
  }

  // Since retrieving the last download offset from the bytemask
  // may involve multiple halEepromRead() calls and this may be slow, 
  // we cache the offset.

  if (!lastRecordedByteMaskIndexKnown) {
    lastRecordedByteMaskIndex = getByteMaskIndexFromEeprom();
    lastRecordedByteMaskIndexKnown = TRUE;
  }
  return getOffsetFromByteMaskIndex(lastRecordedByteMaskIndex);
}

EmberAfOtaStorageStatus emberAfOtaStorageDriverPrepareToResumeDownloadCallback(void)
{
  int32u pageOffsetStart;
  
  if (lastRecordedByteMaskIndex < 0) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  
  pageOffsetStart = (lastRecordedByteMaskIndex + 1) << determinePageSizeLog();
  
  return (startEraseOperation(pageOffsetStart,
                              pageOffsetStart + emberAfPluginEepromInfo()->pageSize)
          ? EMBER_AF_OTA_STORAGE_OPERATION_IN_PROGRESS
          : EMBER_AF_OTA_STORAGE_ERROR);
}

#if defined(DEBUG_PRINT)
void emAfEepromTest(void)
{
  // This function works only for blocking IO calls

  int16u page = 0;
  int8u writeBuffer[16];
  int16u i;
  int8u status;

  int16u writes = emberAfPluginEepromInfo()->pageSize / 16;

  status = emberAfPluginEepromErase(page * emberAfPluginEepromInfo()->pageSize,
                                    emberAfPluginEepromInfo()->pageSize);
  if (status != 0) {
    debugPrint("Failed to erase page %d, status: 0x%X", page, status);
    return;
  }

  debugPrint("Number of writes: %d", writes);

  for (i = 0; i < writes; i++) {
    MEMSET(writeBuffer, i, 16);
    status = emberAfPluginEepromWrite(page + (i * 16),
                                      writeBuffer, 
                                      16);
    debugPrint("Write address 0x%4X, length %d, status: 0x%X",
               page + i,
               16,
               status);
    if (status != 0) {
      return;
    }
  }
  debugPrint("All data written successfully.");
}
#endif

#endif // #if !defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_READ_MODIFY_WRITE_SUPPORT)

