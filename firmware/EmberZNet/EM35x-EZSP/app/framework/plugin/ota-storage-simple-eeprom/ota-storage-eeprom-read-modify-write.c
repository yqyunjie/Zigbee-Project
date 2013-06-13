// *****************************************************************************
// * ota-storage-eeprom-driver-read-modify-write.c
// *
// * This code is intended for EEPROM devices that support read-modify-write
// * of arbitrary page sizes, and across page boundaries.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/plugin/eeprom/eeprom.h"

#define OTA_STORAGE_EEPROM_INTERNAL_HEADER
#include "ota-storage-eeprom.h"
#undef OTA_STORAGE_EEPROM_INTERNAL_HEADER

#if defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_READ_MODIFY_WRITE_SUPPORT)

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------

int32u emberAfOtaStorageDriverRetrieveLastStoredOffsetCallback(void)
{
  int32u offset;

  if (!emAfOtaStorageCheckDownloadMetaData()) {
    return 0;
  }

  offset = emAfOtaStorageReadInt32uFromEeprom(IMAGE_INFO_START
                                              + SAVED_DOWNLOAD_OFFSET_INDEX);
  if (offset == 0xFFFFFFFFL) {
    return 0;
  }
  return offset;
}

void emAfStorageEepromUpdateDownloadOffset(int32u offset, boolean finalOffset)
{
  int32u oldDownloadOffset = 
    emberAfOtaStorageDriverRetrieveLastStoredOffsetCallback();

  if (finalOffset
      || offset == 0
      || (offset > SAVE_RATE
          && (oldDownloadOffset + SAVE_RATE) <= offset)) {
    // The actual offset we are writing TO is the second parameter.
    // The data we are writing (first param) also happens to be an offset but
    // is not a location for the write operation in this context.
    debugFlush();
    debugPrint("Recording download offset: 0x%4X", offset);
    debugFlush();

    emAfOtaStorageWriteInt32uToEeprom(offset, IMAGE_INFO_START + SAVED_DOWNLOAD_OFFSET_INDEX);
    //printImageInfoStartData();
  }
}

EmberAfOtaStorageStatus emberAfOtaStorageDriverInvalidateImageCallback(void)
{
  int8u zeroMagicNumber[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  if (!emAfOtaStorageCheckDownloadMetaData()) {
    emAfOtaStorageWriteDownloadMetaData();
  }
  
  // Wipe out the magic number in the OTA file and the Header length field.
  // EEPROM driver requires a write of at least 8 bytes in length.
  if (!emberAfOtaStorageDriverWriteCallback(zeroMagicNumber, 
                                            0,      // offset
                                            sizeof(zeroMagicNumber))){    // length
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  // Set the offset to 0 to indicate on reboot that there is no previous image
  // to resume downloading.
  emberAfOtaStorageDriverDownloadFinishCallback(0);

  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

void emberAfPluginOtaStorageSimpleEepromPageEraseEventHandler(void)
{
  // This event should never fire.
}

void emAfOtaStorageEepromInit(void)
{
  // Older drivers do not have an EEPROM info structure that we can reference
  // so we must just assume they are okay.  
  if (emberAfPluginEepromInfo() != NULL) {
    // OTA code must match the capabilities of the part.  This code
    // assumes that a page erase prior to writing data is NOT required.
    assert((emberAfPluginEepromInfo()->capabilitiesMask
            & EEPROM_CAPABILITIES_PAGE_ERASE_REQD)
           == 0);
  }
}

EmberAfOtaStorageStatus emberAfOtaStorageDriverPrepareToResumeDownloadCallback(void)
{
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

#endif // #if defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_READ_MODIFY_WRITE_SUPPORT)
