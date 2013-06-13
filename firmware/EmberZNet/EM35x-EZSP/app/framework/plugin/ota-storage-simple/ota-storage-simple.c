// *****************************************************************************
// * ota-storage-simple.c
// *
// * This is the implementation of a very simple storage interface for either
// * OTA client or server.  It keeps track of only one file.  The actual storage
// * device is defined by the developer, but it is assumed to be a device
// * with a continuous block of space (such as an EEPROM).  
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "callback.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"

#include "app/framework/util/util.h"
#include "app/framework/util/common.h"

//------------------------------------------------------------------------------
// Globals

// Either we have a full image that can be used by a server to send to clients
// and can be used by clients to bootload, or we have a partial image that can
// be used by clients in the process of downloading a file.
static boolean storageHasFullImage;

// We cache certain image info since it will be accessed often.
static EmberAfOtaImageId cachedImageId;
static int32u cachedImageSize;

static PGM int8u magicNumberBytes[] = { 0x1e, 0xf1, 0xee, 0x0b };
#define TOTAL_MAGIC_NUMBER_BYTES  4

// This is the minimum length we need to obtain maunfacturer ID, image type ID
// and version
#define IMAGE_INFO_HEADER_LENGTH (VERSION_OFFSET + 4)

//------------------------------------------------------------------------------
// Forward Declarations

static EmberAfOtaStorageStatus readAndValidateStoredImage(EmberAfOtaHeader* returnData,
                                                          boolean printMagicNumberError);
static EmberAfOtaImageId getCurrentImageId(void);
static int16u getLittleEndianInt16uFromBlock(const int8u* block);
static int32u getLittleEndianInt32uFromBlock(const int8u* block);
static boolean initDone = FALSE;

//------------------------------------------------------------------------------

EmberAfOtaStorageStatus emberAfOtaStorageInitCallback(void)
{
  EmberAfOtaHeader header;

  if (initDone) {
    return EMBER_AF_OTA_STORAGE_SUCCESS;
  }

  if (!emberAfOtaStorageDriverInitCallback()) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  storageHasFullImage = (EMBER_AF_OTA_STORAGE_SUCCESS
                         == readAndValidateStoredImage(&header, 
                                                       FALSE));  // print magic number error?

  initDone = TRUE;

  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

int8u emberAfOtaStorageGetCountCallback(void)
{
  return (storageHasFullImage ? 1 : 0);
}

EmberAfOtaImageId emberAfOtaStorageSearchCallback(int16u manufacturerId, 
                                                  int16u manufacturerDeviceId,
                                                  const int16u* hardwareVersion)
{
  if (manufacturerId != cachedImageId.manufacturerId
      || manufacturerDeviceId != cachedImageId.imageTypeId) {
    return emberAfInvalidImageId;
  }

  if (hardwareVersion) {
    EmberAfOtaHeader header;
    if (EMBER_AF_OTA_STORAGE_SUCCESS
        != readAndValidateStoredImage(&header, 
                                      FALSE)) {  // print magic number error?
      return emberAfInvalidImageId;
    }
    if (headerHasHardwareVersions(&header)
        && (header.minimumHardwareVersion > *hardwareVersion
            || header.maximumHardwareVersion < *hardwareVersion)) {
      return emberAfInvalidImageId;
    }
  }
  return cachedImageId;
}

EmberAfOtaImageId emberAfOtaStorageIteratorFirstCallback(void)
{
  return getCurrentImageId();
}

EmberAfOtaImageId emberAfOtaStorageIteratorNextCallback(void)
{
  return emberAfInvalidImageId;
}

EmberAfOtaStorageStatus emberAfOtaStorageGetFullHeaderCallback(const EmberAfOtaImageId* id,
                                                               EmberAfOtaHeader* returnData)
{
  EmberAfOtaStorageStatus status;

  if (!storageHasFullImage) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  status = readAndValidateStoredImage(returnData, 
                                      FALSE);  // print magic number error?
  return (EMBER_AF_OTA_STORAGE_SUCCESS == status
          && 0 == MEMCOMPARE(&cachedImageId, id, sizeof(EmberAfOtaImageId))
          ? EMBER_AF_OTA_STORAGE_SUCCESS
          : EMBER_AF_OTA_STORAGE_ERROR);
}

int32u emberAfOtaStorageGetTotalImageSizeCallback(const EmberAfOtaImageId* id)
{
  if (storageHasFullImage
      && (0 == MEMCOMPARE(&cachedImageId, id, sizeof(EmberAfOtaImageId)))) {
    return cachedImageSize;
  }
  return 0;
}

static void clearCachedId(void)
{
  MEMSET(&cachedImageId, 0, sizeof(EmberAfOtaImageId));
}

static EmberAfOtaStorageStatus readAndValidateStoredImage(EmberAfOtaHeader* returnData,
                                                          boolean printMagicNumberError)
{
  int8u data[OTA_MAXIMUM_HEADER_LENGTH_2_BYTE_ALIGNED];
  int8u indexOrReadLength = 0;
  int8u parseData;

  // Read up to the maximum OTA header size even if it is not that big.
  // The code will not process the extra bytes unless the header indicates
  // optional fields are used.
  if (!emberAfOtaStorageDriverReadCallback(0,   // offset
                                           OTA_MAXIMUM_HEADER_LENGTH_2_BYTE_ALIGNED,
                                           data)) {
    otaPrintln("emberAfOtaStorageDriverReadCallback() failed!");
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  MEMSET(returnData, 0, sizeof(EmberAfOtaHeader));

  if (0 != MEMPGMCOMPARE(data, magicNumberBytes, TOTAL_MAGIC_NUMBER_BYTES)) {
    if (printMagicNumberError) {
      otaPrintln("Bad magic number in file: 0x%X 0x%X 0x%X 0x%X",
                 data[0],
                 data[1],
                 data[2],
                 data[3]);
    }
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  returnData->headerVersion = 
    getLittleEndianInt16uFromBlock(data + HEADER_VERSION_OFFSET);

  if (returnData->headerVersion != OTA_HEADER_VERSION) {
    otaPrintln("Bad version in file 0x%2X", returnData->headerVersion);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  returnData->headerLength = 
    getLittleEndianInt16uFromBlock(data + HEADER_LENGTH_OFFSET);
  returnData->fieldControl = 
    getLittleEndianInt16uFromBlock(data + FIELD_CONTROL_OFFSET);
  returnData->manufacturerId = 
    getLittleEndianInt16uFromBlock(data + MANUFACTURER_ID_OFFSET);
  returnData->imageTypeId = 
    getLittleEndianInt16uFromBlock(data + IMAGE_TYPE_ID_OFFSET);
  returnData->firmwareVersion = 
    getLittleEndianInt32uFromBlock(data + VERSION_OFFSET);
  returnData->zigbeeStackVersion = 
    getLittleEndianInt16uFromBlock(data + STACK_VERSION_OFFSET);

  // We add +1 because the actual length of the string in the data structure
  // is longer.  This is to account for a 32-byte string in the OTA header
  // that does NOT have a NULL terminator.
  MEMSET(returnData->headerString,  0, EMBER_AF_OTA_MAX_HEADER_STRING_LENGTH + 1);
  MEMCOPY(returnData->headerString, 
          data + HEADER_STRING_OFFSET, 
          EMBER_AF_OTA_MAX_HEADER_STRING_LENGTH);

  returnData->imageSize = 
    getLittleEndianInt32uFromBlock(data + IMAGE_SIZE_OFFSET);
  cachedImageSize = returnData->imageSize;

  clearCachedId();
  cachedImageId.manufacturerId  = returnData->manufacturerId;
  cachedImageId.imageTypeId     = returnData->imageTypeId;
  cachedImageId.firmwareVersion = returnData->firmwareVersion;

  if (returnData->fieldControl == 0) {
    return EMBER_AF_OTA_STORAGE_SUCCESS;
  }

  // In an attempt to save flash space, reuse the conditional code here.
  // First time through we figure out the total size of the optional fields
  // to read from flash.
  // Second time through we extract the data and copy into the data struct.
  for (parseData = 0; parseData < 2; parseData++) {
    if (headerHasSecurityCredentials(returnData)) {
      if (parseData) {
        returnData->securityCredentials = data[indexOrReadLength];
      }
      indexOrReadLength++;
    }
    if (headerHasUpgradeFileDest(returnData)) {
      if (parseData) {
        MEMCOPY(returnData->upgradeFileDestination, 
                data + indexOrReadLength, 
                EUI64_SIZE);
        MEMCOPY(cachedImageId.deviceSpecificFileEui64, 
                data + indexOrReadLength, 
                EUI64_SIZE);
      }
      indexOrReadLength += EUI64_SIZE;
    }
    if (headerHasHardwareVersions(returnData)) {
      if (parseData) {
        returnData->minimumHardwareVersion = 
          getLittleEndianInt16uFromBlock(data + indexOrReadLength);
        returnData->maximumHardwareVersion = 
          getLittleEndianInt16uFromBlock(data + indexOrReadLength + 2);
      }
      indexOrReadLength += 4;
    }

    if (parseData) {
      // We are done.  Don't read data from the storage device.
      continue;
    }

    if (indexOrReadLength > OTA_MAXIMUM_HEADER_LENGTH_2_BYTE_ALIGNED) {
      otaPrintln("Error: readAndValidateStoredImage() tried to read more data than it could hold.");
      return EMBER_AF_OTA_STORAGE_ERROR;
    }

    if (!emberAfOtaStorageDriverReadCallback(OPTIONAL_FIELDS_OFFSET,
                                             indexOrReadLength, // now it's a read length
                                             data)) {
      otaPrintln("emberAfOtaStorageDriverReadCallback() FAILED!");
      return EMBER_AF_OTA_STORAGE_ERROR;
    }
    indexOrReadLength = OPTIONAL_FIELDS_OFFSET;  // now it's an index again

  } // end for loop

  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emberAfOtaStorageReadImageDataCallback(const EmberAfOtaImageId* id,
                                                               int32u offset,
                                                               int32u length, 
                                                               int8u* returnData,
                                                               int32u* returnedLength)
{
  if (0 != MEMCOMPARE(id, &cachedImageId, sizeof(EmberAfOtaImageId))) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  length = (length > (cachedImageSize - offset)
            ? (cachedImageSize - offset)
            : length);

  if (!emberAfOtaStorageDriverReadCallback(offset,
                                            length,
                                            returnData)) {
    otaPrintln("emberAfOtaStorageDriverReadCallback() FAILED!");
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  *returnedLength = length;
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emberAfOtaStorageFinishDownloadCallback(int32u offset)
{
  emberAfOtaStorageDriverDownloadFinishCallback(offset);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emberAfOtaStorageClearTempDataCallback(void)
{
  storageHasFullImage = FALSE;

  return emberAfOtaStorageDriverInvalidateImageCallback();  
}

EmberAfOtaStorageStatus emberAfOtaStorageWriteTempDataCallback(int32u offset,
                                                               int32u length,
                                                               const int8u* data)
{
  return (emberAfOtaStorageDriverWriteCallback(data,
                                               offset,
                                               length)
          ? EMBER_AF_OTA_STORAGE_SUCCESS
          : EMBER_AF_OTA_STORAGE_ERROR);
}

EmberAfOtaStorageStatus emberAfOtaStorageCheckTempDataCallback(int32u* returnOffset,
                                                               int32u* returnTotalSize,
                                                               EmberAfOtaImageId* returnOtaImageId)
{
  // Right now, we don't support continuing a download after a reboot.  Either the
  // entire image is in flash or we assume nothing has been downloaded.
  EmberAfOtaHeader header;
  int32u lastOffset = emberAfOtaStorageDriverRetrieveLastStoredOffsetCallback();
  otaPrintln("Last offset downloaded: 0x%4X", lastOffset);
  if (lastOffset > OTA_MINIMUM_HEADER_LENGTH
      && (EMBER_AF_OTA_STORAGE_SUCCESS
          == readAndValidateStoredImage(&header, 
                                        TRUE))) {  // print magic number error?
    *returnTotalSize = header.imageSize;
    *returnOtaImageId = emAfOtaStorageGetImageIdFromHeader(&header);
    if (lastOffset >= header.imageSize) {
      storageHasFullImage = TRUE;
      *returnOffset = header.imageSize;
    } else {
      *returnOffset = lastOffset;
      return EMBER_AF_OTA_STORAGE_PARTIAL_FILE_FOUND;
    }
    return EMBER_AF_OTA_STORAGE_SUCCESS;
  }
  storageHasFullImage = FALSE;

  return EMBER_AF_OTA_STORAGE_ERROR;
}

EmberAfOtaStorageStatus emberAfOtaStorageDeleteImageCallback(const EmberAfOtaImageId* id)
{
  EmberAfOtaStorageStatus status;

  if (storageHasFullImage) {
    status = emberAfOtaStorageDriverInvalidateImageCallback();
  }

  storageHasFullImage = FALSE;
  clearCachedId();
  
  return status;
}

void emAfOtaStorageInfoPrint(void)
{
  otaPrintln("Storage Module: OTA Simple Storage Plugin");
  otaPrintln("Images Stored:  %d of 1\n",
             (storageHasFullImage ? 1 : 0));
  emAfOtaStorageDriverInfoPrint();
}

static EmberAfOtaImageId getCurrentImageId(void)
{
  if (!storageHasFullImage) {
    return emberAfInvalidImageId;
  }
  return cachedImageId;
}

static int16u getLittleEndianInt16uFromBlock(const int8u* block)
{
  return (block[0] + (((int16u)(block[1])) << 8));
}

static int32u getLittleEndianInt32uFromBlock(const int8u* block)
{
  int32u value = (getLittleEndianInt16uFromBlock(block)
                  + (((int32u)getLittleEndianInt16uFromBlock(block + 2))
                     << 16));
  return value;
}

