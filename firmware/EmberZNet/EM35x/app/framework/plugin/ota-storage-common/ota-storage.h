// *****************************************************************************
// * ota-server-storage.h
// *
// * This file defines the interface to a Over-the-air (OTA) storage device.  It
// * can be used by either a server or client.  The server can store 0 or more
// * files that are indexed uniquely by an identifier made up of their Version
// * Number, Manufacturer ID, and Device ID.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#define OTA_MINIMUM_HEADER_LENGTH (20 + 32 + 4)
// Optional fields are: security credentials, upgrade dest, and HW versions
#define OTA_MAXIMUM_HEADER_LENGTH (OTA_MINIMUM_HEADER_LENGTH + 1 + 8 + 4)

// For EEPROM parts with 2-byte word sizes we need to make sure we read
// on word boundaries.
#define OTA_MAXIMUM_HEADER_LENGTH_2_BYTE_ALIGNED (OTA_MAXIMUM_HEADER_LENGTH + 1)

#define OTA_FILE_MAGIC_NUMBER        0x0BEEF11EL

#define MAGIC_NUMBER_OFFSET    0
#define HEADER_VERSION_OFFSET  4
#define HEADER_LENGTH_OFFSET   6
#define FIELD_CONTROL_OFFSET   8
#define MANUFACTURER_ID_OFFSET 10
#define IMAGE_TYPE_ID_OFFSET   12
#define VERSION_OFFSET         14
#define STACK_VERSION_OFFSET   18
#define HEADER_STRING_OFFSET   20
#define IMAGE_SIZE_OFFSET      52
#define OPTIONAL_FIELDS_OFFSET 56
// The rest are optional fields.

#define HEADER_LENGTH_FIELD_LENGTH 2

#define TAG_OVERHEAD (2 + 4)   // 2 bytes for the tag ID, 4 bytes for the length

#define OTA_HEADER_VERSION 0x0100

#define SECURITY_CREDENTIAL_VERSION_FIELD_PRESENT_MASK 0x0001
#define DEVICE_SPECIFIC_FILE_PRESENT_MASK              0x0002
#define HARDWARE_VERSIONS_PRESENT_MASK                 0x0004

#define headerHasSecurityCredentials(header) \
  ((header)->fieldControl & SECURITY_CREDENTIAL_VERSION_FIELD_PRESENT_MASK)
#define headerHasUpgradeFileDest(header)  \
  ((header)->fieldControl & DEVICE_SPECIFIC_FILE_PRESENT_MASK)
#define headerHasHardwareVersions(header) \
  ((header)->fieldControl & HARDWARE_VERSIONS_PRESENT_MASK)


// This size does NOT include the tag overhead.
#define SIGNATURE_TAG_DATA_SIZE (EUI64_SIZE + EMBER_SIGNATURE_SIZE)

#define INVALID_MANUFACTURER_ID  0xFFFF
#define INVALID_DEVICE_ID        0xFFFF
#define INVALID_FIRMWARE_VERSION 0xFFFFFFFFL
#define INVALID_EUI64 { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#define INVALID_OTA_IMAGE_ID    \
  { INVALID_MANUFACTURER_ID,    \
    INVALID_DEVICE_ID,          \
    INVALID_FIRMWARE_VERSION,   \
    INVALID_EUI64,              \
  }

//------------------------------------------------------------------------------

// Initialization
// (For the POSIX implementation the device will be a file or directory)
EmberAfOtaStorageStatus emAfOtaStorageSetDevice(const void* device);
void emAfOtaStorageClose(void);

const char* emAfOtaStorageGetFilepath(const EmberAfOtaImageId* id);

EmberAfOtaStorageStatus emAfOtaStorageAddImageFile(const char* filename);


// Creating (two options)
//  - Create a file based on a passed "EmberAfOtaHeader" structure, stored
//      at the filename passed to the function.  This is usually done by a 
//      PC tool.
//  - Create a file based on raw data (presumably received over the air)
//      This will be stored in a single static temp file.
EmberAfOtaStorageStatus emAfOtaStorageCreateImage(EmberAfOtaHeader* header, 
                                                  const char* filename);
EmberAfOtaStorageStatus emAfOtaStorageAppendImageData(const char* filename,
                                                      int32u length, 
                                                      const int8u* data);

//------------------------------------------------------------------------------
// Generic routines that are independent of the actual storage mechanism.

EmberAfOtaStorageStatus emAfOtaStorageGetHeaderLengthAndImageSize(const EmberAfOtaImageId* id,
                                                                  int32u *returnHeaderLength,
                                                                  int32u *returnImageSize);

EmberAfOtaImageId emAfOtaStorageGetImageIdFromHeader(const EmberAfOtaHeader* header);

// Returns the offset and size of the actual data (does not include
// tag meta-data) in the specified tag.
EmberAfOtaStorageStatus emAfOtaStorageGetTagOffsetAndSize(const EmberAfOtaImageId* id,
                                                          int16u tag,
                                                          int32u* returnTagOffset,
                                                          int32u* returnTagSize);

EmberAfOtaStorageStatus emAfOtaStorageGetTagDataFromImage(const EmberAfOtaImageId* id,
                                                          int16u tag,
                                                          int8u* returnData,
                                                          int32u* returnDataLength,
                                                          int32u maxReturnDataLength);

// This gets the OTA header as it is formatted in the file, including
// the magic number.
EmberAfOtaStorageStatus emAfOtaStorageGetRawHeaderData(const EmberAfOtaImageId* id,
                                                       int8u* returnData,
                                                       int32u* returnDataLength,
                                                       int32u maxReturnDataLength);

// This retrieves a list of all tags in the file and their lengths.
// It will read at most 'maxTags' and return that array data in tagInfo.
EmberAfOtaStorageStatus emAfOtaStorageReadAllTagInfo(const EmberAfOtaImageId* id,
                                                     EmberAfTagData* tagInfo,
                                                     int16u maxTags,
                                                     int16u* totalTags);

boolean emberAfIsOtaImageIdValid(const EmberAfOtaImageId* idToCompare);

// This should be moved into the plugin callbacks file.
EmberAfOtaStorageStatus emberAfOtaStorageDeleteImageCallback(const EmberAfOtaImageId* id);

void emAfOtaStorageInfoPrint(void);
void emAfOtaStorageDriverInfoPrint(void);

int32u emberAfOtaStorageDriverMaxDownloadSizeCallback(void);

//------------------------------------------------------------------------------
// Internal (for debugging malloc() and free())
int remainingAllocations(void);

