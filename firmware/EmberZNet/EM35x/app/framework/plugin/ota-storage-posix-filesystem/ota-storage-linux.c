/*******************************************************************************
 * Over The Air [Cluster] Upgrade Image Storage
 * 
 * This file details the API for talking to a platform specific upgrade image 
 * storage device.  
 * 
 * This implements the image storage for a standard POSIX-style operating system 
 * with an underlying filesystem.  It creates a linked list cache of all
 * the OTA image headers and uses that for quick access to requests for an
 * image.  Full image data requires that we read the file from disk.
 *
 * It can also be a OTA client storage device, receiving bytes over the air
 * and storing them to a temporary file.  Once that is done is can validate the
 * image by adding it to its linked list cache.  This parses the file and checks
 * that it is a well formed image.
 *
 *   Copyright 2009 by Ember Corporation. All rights reserved.              *80*
 ******************************************************************************/

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/common.h"

#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"

#include "ota-storage-linux.h"

#if defined (IMAGE_BUILDER)
  // For our PC tool, we use a simpler #define to turn on this code.
  #define GATEWAY_APP
#endif

#if defined(GATEWAY_APP)
// These includes are wrapped inside the #ifdef because they are platform
// specific.

#define _GNU_SOURCE     // strnlen
#include <string.h>     // ""

#include <stdio.h>      // fopen, fread, fwrite, fclose, fseek, fprintf
#include <sys/types.h>  // stat
#include <sys/stat.h>   // ""
#include <unistd.h>     // ""
#include <stdarg.h>     // va_start, va_end

#include <stdlib.h>     // malloc, free
#include <errno.h>      // errno, strerror

#include <dirent.h>     // opendir, readdir

#ifdef __APPLE__
#define strnlen(string, n) strlen((string))
#endif

//------------------------------------------------------------------------------
// Globals

// This will always end with a '/'.  The code will append one if not present.
static char* storageDevice = NULL;
static boolean storageDeviceIsDirectory = FALSE;
static char* tempStorageFilepath  = NULL;

static const char* tempStorageFile = "temporary-storage.ota";

typedef struct {
  EmberAfOtaHeader* header;
  char* filepath;
  const char* filenameStart;  // ptr to data in 'filepath'
  struct OtaImage* next;
  struct OtaImage* prev;
  off_t fileSize;
} OtaImage;

static OtaImage* imageListFirst = NULL;
static OtaImage* imageListLast = NULL;
static int8u imageCount = 0;

#define OTA_MAX_FILENAME_LENGTH 1000

static const int8u otaFileMagicNumberBytes[] = {
  (int8u)(OTA_FILE_MAGIC_NUMBER),
  (int8u)(OTA_FILE_MAGIC_NUMBER >> 8),
  (int8u)(OTA_FILE_MAGIC_NUMBER >> 16),
  (int8u)(OTA_FILE_MAGIC_NUMBER >> 24),
};

static const int16u otaFileVersionNumber = 0x0100;

#define ALWAYS_PRESENT_MASK 0xFFFF

enum FieldType{
  INVALID_FIELD    = 0,
  INTEGER_FIELD    = 1,
  BYTE_ARRAY_FIELD = 2,
  STRING_FIELD     = 3,
};

static const char* fieldNames[] = {
  "INVALID",
  "integer",
  "byte array",
  "string",
};

typedef struct {
  const char* name;
  enum FieldType type;
  int16u length;
  int16u maskForOptionalField;
  void* location;
  boolean found;
} EmberAfOtaHeaderFieldDefinition;

// This global is used to define what is in the OTA header and help map that
// to the 'EmberAfOtaHeader' data structure
static EmberAfOtaHeaderFieldDefinition otaHeaderFieldDefinitions[] = {
  // Magic number ommitted on purpose.  Always has the same value.
  //  { "Magic Number",             INTEGER_FIELD,    4,  ALWAYS_PRESENT_MASK, NULL, FALSE },
  { "Header Version",           INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Header Length",            INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Field Control",            INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Manufacturer ID",          INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Image Type",               INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Firmware Version",         INTEGER_FIELD,    4,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Zigbee Stack Version",     INTEGER_FIELD,    2,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Header String",            STRING_FIELD,     32, ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Total Image Size",         INTEGER_FIELD,    4,  ALWAYS_PRESENT_MASK,                            NULL, FALSE, },
  { "Security Credentials",     INTEGER_FIELD,    1,  SECURITY_CREDENTIAL_VERSION_FIELD_PRESENT_MASK, NULL, FALSE, },
  { "Upgrade File Destination", BYTE_ARRAY_FIELD, 8,  DEVICE_SPECIFIC_FILE_PRESENT_MASK,              NULL, FALSE, },
  { "Minimum Hardware Version", INTEGER_FIELD,    2,  HARDWARE_VERSIONS_PRESENT_MASK,                 NULL, FALSE, },
  { "Maximum Hardware Version", INTEGER_FIELD,    2,  HARDWARE_VERSIONS_PRESENT_MASK,                 NULL, FALSE, },

  // Tag Data information omitted
  
  { NULL, INVALID_FIELD, 0, 0, NULL, FALSE },
};
enum FieldIndex {
  // Don't define all index numbers so the compiler
  // automatically numbers them.  We just define the first.
  HEADER_VERSION_INDEX = 0,
  HEADER_LENGTH_INDEX,
  FIELD_CONTROL_INDEX,
  MANUFACTURER_ID_INDEX,
  IMAGE_TYPE_INDEX,
  FIRMWARE_VERSION_INDEX,
  ZIGBEE_STACK_VERSION_INDEX,
  HEADER_STRING_INDEX,
  TOTAL_IMAGE_SIZE_INDEX,
  SECURITY_CREDENTIALS_INDEX,
  UPGRADE_FILE_DESTINATION_INDEX,
  MINIMUM_HARDWARE_VERSION_INDEX,
  MAXIMUM_HARDWARE_VERSION_INDEX,
};

#define MAX_FILEPATH_LENGTH 1024

static OtaImage* iterator = NULL;

#if defined(WIN32)
  #define portableMkdir(x) _mkdir(x)
#else
  #define portableMkdir(x) \
    mkdir((x), S_IRUSR | S_IWUSR | S_IXUSR) /* permissions (o=rwx) */
#endif

static EmAfOtaStorageLinuxConfig config = {
  FALSE,  // memoryDebug
  FALSE,  // fileDebug
  FALSE,  // fieldDebug
  TRUE,   // ignoreFilesWithUnderscorePrefix
  TRUE,   // printFileDiscoveryOrRemoval
  NULL,   // fileAddedHandler
};

const char* messagePrefix = NULL;  // prefix for all printed messages

// Debug
static int allocations = 0;

#define APPEND_OFFSET (0xFFFFFFFFL)

//------------------------------------------------------------------------------
// Forward Declarations

static EmberAfOtaStorageStatus createDefaultStorageDirectory(void);
static EmberAfOtaStorageStatus initImageDirectory(void);
static OtaImage* addImageFileToList(const char* filename, 
                                    boolean printImageInfo);
static OtaImage* findImageById(const EmberAfOtaImageId* id);
static void freeOtaImage(OtaImage* image);
static void mapHeaderFieldDefinitionToDataStruct(EmberAfOtaHeader* header);
static void unmapHeaderFieldDefinitions(void);
static EmberAfOtaStorageStatus readHeaderDataFromBuffer(EmberAfOtaHeaderFieldDefinition* definition,
                                                 int8u* bufferPtr,
                                                 int32s headerLengthRemaining,
                                                 int32s actualBufferDataRemaining);
static int16u calculateOtaFileHeaderLength(EmberAfOtaHeader* header);
static int8u* writeHeaderDataToBuffer(EmberAfOtaHeaderFieldDefinition* definition,
                                      int8u* bufferPtr);
static EmberAfOtaHeader* readImageHeader(const char* filename);
static OtaImage* imageSearchInternal(const EmberAfOtaImageId* id);
static EmberAfOtaImageId getIteratorImageId(void);
static EmberAfOtaStorageStatus writeRawData(int32u offset,
                                            const char* filepath,
                                            int32u length,
                                            const int8u* data);
static OtaImage* findImageByFilename(const char* tempFilepath);
static void removeImage(OtaImage* image);

static void* myMalloc(size_t size, const char* allocName);
static void myFree(void* ptr);
static void error(const char* formatString, ...);
static void note(const char* formatString, ...);
static void debug(boolean debugOn, const char* formatString, ...);

static const char defaultStorageDirectory[] = "ota-files";
static boolean initDone = FALSE;
static boolean createStorageDirectory = TRUE;

//==============================================================================
// Public API
//==============================================================================

// Our storage device examines all files in the passed directory, or simply
// loads a single file into its header cache.
EmberAfOtaStorageStatus emAfOtaSetStorageDevice(const void* device)
{
  if (initDone) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  if (device == NULL) {
    // We interpet this to mean, don't use a storage directory.
    // This option is useful when creating files via the PC
    // tool.
    createStorageDirectory = FALSE;
    return EMBER_AF_OTA_STORAGE_SUCCESS;
  }

  const char* directoryOrFile = (const char*)device;
  int length = strnlen(directoryOrFile, MAX_FILEPATH_LENGTH + 1);
  if (MAX_FILEPATH_LENGTH < length) {
    error("Storage directory path too long (max = %d)!\n",
           MAX_FILEPATH_LENGTH);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  // Add 1 for '\0' and 1 for '/'.  This may or may not be necessary 
  // because the path already has it or it is only a file.
  storageDevice = myMalloc(length + 2,
                           "emAfSetStorageDevice(): storageDevice");
  if (storageDevice == NULL) {
    error("Could not allocate %d bytes!\n", length);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  if (directoryOrFile[length - 1] == '/') {
    // We don't want to copy the '/' yet, since stat() will
    // complain if we pass it in.
    length--;
  }
  MEMSET(storageDevice, 0, length + 2);
  strncpy(storageDevice, directoryOrFile, length);

  struct stat statInfo;
  int returnValue = stat(storageDevice, &statInfo);
  debug(config.fileDebug, 
        "Checking for existence of '%s'\n",
        storageDevice);
  if (returnValue != 0) {
    error("Could not read storage device '%s'. %s\n", 
          directoryOrFile,
          strerror(errno));
    myFree(storageDevice);
    storageDevice = NULL;
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  if (S_ISDIR(statInfo.st_mode)) {
    storageDeviceIsDirectory = TRUE;
    storageDevice[length] = '/';
  }
  debug(config.fileDebug, "Storage device set to '%s'.\n", storageDevice);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

void emAfOtaStorageGetConfig(EmAfOtaStorageLinuxConfig* currentConfig)
{
  memcpy(currentConfig, &config, sizeof(EmAfOtaStorageLinuxConfig));
}

void emAfOtaStorageSetConfig(const EmAfOtaStorageLinuxConfig* newConfig)
{
  memcpy(&config, newConfig, sizeof(EmAfOtaStorageLinuxConfig));
}

EmberAfOtaStorageStatus emberAfOtaStorageInitCallback(void)
{
  if (initDone) {
    return EMBER_AF_OTA_STORAGE_SUCCESS;
  }

  if (config.fileDebug) {
    char cwd[MAX_FILEPATH_LENGTH];
    debug(config.fileDebug, "Current Working Directory: %s\n",
          (NULL == getcwd(cwd, MAX_FILEPATH_LENGTH)
           ? "UNKNOWN!"
           : cwd));
  }

  EmberAfOtaStorageStatus status;

  if (storageDevice == NULL) {
    if (createStorageDirectory) {
      status = createDefaultStorageDirectory();
      if (status != EMBER_AF_OTA_STORAGE_SUCCESS) {
        return status;
      }
    } else {
      return EMBER_AF_OTA_STORAGE_SUCCESS;
    }
  }

  if (storageDeviceIsDirectory) {
    status = initImageDirectory();
  } else {
    OtaImage* newImage = addImageFileToList(storageDevice, TRUE);
    if (config.fileAddedHandler != NULL
        && newImage != NULL) {
      (config.fileAddedHandler)(newImage->header);
    }
    status = (NULL == newImage
              ? EMBER_AF_OTA_STORAGE_ERROR
              : EMBER_AF_OTA_STORAGE_SUCCESS);
  }

  iterator = NULL; // Must be initialized via 
                   // otaStorageIteratorReset()

  if (status == EMBER_AF_OTA_STORAGE_SUCCESS) {
    initDone = TRUE;
  }

  return status;
}

void emAfOtaStorageClose(void)
{
  OtaImage* ptr = imageListLast;
  while (ptr != NULL) {
    OtaImage* current = ptr;
    ptr = (OtaImage*)ptr->prev;
    freeOtaImage(current);
  }
  imageListLast = NULL;
  imageListFirst = NULL;
  
  if (storageDevice != NULL) {
    myFree(storageDevice);
    storageDevice = NULL;
  }
  if (tempStorageFilepath != NULL) {
    myFree(tempStorageFilepath);
    tempStorageFilepath = NULL;
  }
  initDone = FALSE;
  imageCount = 0;
}

int8u emberAfOtaStorageGetCountCallback(void)
{
  return imageCount;
}

EmberAfOtaImageId emberAfOtaStorageSearchCallback(int16u manufacturerId, 
                                                  int16u manufacturerDeviceId,
                                                  const int16u* hardwareVersion)
{
  EmberAfOtaImageId id = {
    manufacturerId,
    manufacturerDeviceId,
    INVALID_FIRMWARE_VERSION,
    INVALID_EUI64,
  };
  OtaImage* image = imageSearchInternal(&id);
  if (image == NULL) {
    return emberAfInvalidImageId;
  }
  // We assume that if there is no hardware version information in the header
  // then it can be used on ANY hardware version.  The behavior of the server
  // in this case is not spelled out by the specification.
  if (hardwareVersion != NULL
      && headerHasHardwareVersions(image->header)
      && !(*hardwareVersion >= image->header->minimumHardwareVersion
           && *hardwareVersion <= image->header->maximumHardwareVersion)) {
    return emberAfInvalidImageId;
  }
  id.firmwareVersion = image->header->firmwareVersion;
  return id;
}

const char* emAfOtaStorageGetFilepath(const EmberAfOtaImageId* id)
{
  OtaImage* image = findImageById(id);
  if (image == NULL) {
    return NULL;
  }
  return image->filepath;
}

EmberAfOtaImageId emberAfOtaStorageIteratorFirstCallback(void)
{
  iterator = imageListFirst;
  return getIteratorImageId();
}

EmberAfOtaImageId emberAfOtaStorageIteratorNextCallback(void)
{
  if (iterator != NULL) {
    iterator = (OtaImage*)iterator->next;
  }
  return getIteratorImageId();
}

EmberAfOtaStorageStatus emberAfOtaStorageReadImageDataCallback(const EmberAfOtaImageId* id, 
                                                               int32u offset, 
                                                               int32u length, 
                                                               int8u* returnData, 
                                                               int32u* returnedLength)
{
  OtaImage* image = imageSearchInternal(id);
  if (image == NULL) {
    error("No such Image (Mfg ID: 0x%04X, Image ID: 0x%04X, Version: 0x%08X\n",
          id->manufacturerId, 
          id->imageTypeId,
          id->firmwareVersion);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  // Windows requires the 'b' (binary) as part of the mode so that line endings
  // are not truncated.  POSIX ignores this.
  FILE* fileHandle = fopen(image->filepath, "rb");
  if (fileHandle == NULL) {
    error("Failed to open file '%s' for reading: %s\n",
          image->filenameStart,
          strerror(errno));
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  if (0 != fseek(fileHandle, offset, SEEK_SET)) {
    error("Failed to seek in file '%s' to offset %d\n",
          image->filenameStart,
          offset);
    fclose(fileHandle);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  *returnedLength = fread(returnData, 1, length, fileHandle);
  fclose(fileHandle);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emAfOtaStorageCreateImage(EmberAfOtaHeader* header, 
                                                  const char* filename)
{
  // Windows requires the 'b' (binary) as part of the mode so that line endings
  // are not truncated.  POSIX ignores this.
  FILE* fileHandle = fopen(filename, "wb");
  if (fileHandle == NULL) {
    error("Could not open file '%s' for writing: %s\n",
          filename,
          strerror(errno));
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  int8u buffer[OTA_MAXIMUM_HEADER_LENGTH];
  int8u* bufferPtr = buffer;

  memcpy(bufferPtr, otaFileMagicNumberBytes, 4);
  bufferPtr += 4;
  bufferPtr[0] = (int8u)otaFileVersionNumber;
  bufferPtr[1] = (int8u)(otaFileVersionNumber >> 8);
  bufferPtr += 2;

  mapHeaderFieldDefinitionToDataStruct(header);
  header->headerLength = calculateOtaFileHeaderLength(header);
  header->imageSize += header->headerLength;

  EmberAfOtaHeaderFieldDefinition* definition = &(otaHeaderFieldDefinitions[HEADER_LENGTH_INDEX]);
  while (definition->name != NULL) {
    debug(config.memoryDebug, 
          "Writing Header Field: %s, bufferPtr: 0x%08X\n", 
          definition->name,
          (unsigned int)bufferPtr);

    bufferPtr = writeHeaderDataToBuffer(definition,
                                        bufferPtr);

    // I have run into stack corruption bugs in the past so this
    // is to try and catch any new issues.
    assert(bufferPtr - buffer <= OTA_MAXIMUM_HEADER_LENGTH);

    definition++;
  }
  unmapHeaderFieldDefinitions();
  
  size_t written = fwrite(buffer, 1, header->headerLength, fileHandle);
  if (written != header->headerLength) {
    error("Wrote only %d bytes but expected to write %d.\n",
          written,
          header->headerLength);
    fclose(fileHandle);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  fflush(fileHandle);
  fclose(fileHandle);
  
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emAfOtaStorageAddImageFile(const char* filename)
{
  return (NULL == addImageFileToList(filename,
                                     FALSE)    // print image info?
          ? EMBER_AF_OTA_STORAGE_ERROR
          : EMBER_AF_OTA_STORAGE_SUCCESS);
}

EmberAfOtaStorageStatus emberAfOtaStorageDeleteImageCallback(const EmberAfOtaImageId* id)
{
  OtaImage* image = imageSearchInternal(id);
  if (image == NULL) {
    error("No such Image (Mfg ID: 0x%04X, Image ID: 0x%04X, Version: 0x%08X\n",
          id->manufacturerId, 
          id->imageTypeId,
          id->firmwareVersion);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  if (config.printFileDiscoveryOrRemoval) {
    note("Image '%s' removed from storage list.  NOT deleted from disk.\n", 
         image->filenameStart);
  }
  removeImage(image);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

// Although the call is to "clear" temp data we actually end up allocating
// space for the filename and creating an empty file.
EmberAfOtaStorageStatus emberAfOtaStorageClearTempDataCallback(void)
{
  EmberAfOtaStorageStatus status = EMBER_AF_OTA_STORAGE_ERROR;
  FILE* fileHandle = NULL;

  if (!storageDeviceIsDirectory) {
    error("Cannot create temp. OTA data because storage device is a file, not a directory.\n");
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  if (storageDevice == NULL) {
    error("No storage device defined!");
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  if (tempStorageFilepath == NULL) {
    // Add 1 to make sure we have room for a NULL terminating character
    int tempFilepathLength = (strlen(storageDevice)
                              + strlen(tempStorageFile) + 1);
    if (tempFilepathLength > MAX_FILEPATH_LENGTH) {
      goto clearTempDataCallbackDone;
    }
    tempStorageFilepath = myMalloc(tempFilepathLength,
                                   "otaStorageCreateTempData(): tempFilepath");
    if (tempStorageFilepath == NULL) {
      goto clearTempDataCallbackDone;
    }
    snprintf(tempStorageFilepath, 
             tempFilepathLength,
             "%s%s",
             storageDevice,
             tempStorageFile);
  }

  OtaImage* image = findImageByFilename(tempStorageFilepath);
  if (image) {
    removeImage(image);
  }
  // Windows requires the 'b' (binary) as part of the mode so that line endings
  // are not truncated.  POSIX ignores this.
  fileHandle = fopen(tempStorageFilepath,
                     "wb");  // truncate the file to zero length
  if (fileHandle == NULL) {
    error("Could not open temporary file '%s' for writing: %s\n",
          tempStorageFilepath,
          strerror(errno));
  } else {
    status = EMBER_AF_OTA_STORAGE_SUCCESS;
  }

 clearTempDataCallbackDone:
  // tempStorageFilepath is used throughout the life of the storage
  // device, so we only need to free it if there was an error, 
  // or when the storage device changed.  We expect we can
  // only change the storage device if we first call emAfOtaStorageClose()
  // which frees the tempStorageFilepath data as well.
  if ((status != EMBER_AF_OTA_STORAGE_SUCCESS)
      && tempStorageFilepath) {
    myFree(tempStorageFilepath);
    tempStorageFilepath = NULL;
  }
  if (fileHandle) {
    fclose(fileHandle);
  }
  return status;
}

EmberAfOtaStorageStatus emberAfOtaStorageWriteTempDataCallback(int32u offset,
                                                               int32u length,
                                                               const int8u* data)
{
  if (tempStorageFilepath == NULL) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  return writeRawData(offset, tempStorageFilepath, length, data);
}

EmberAfOtaStorageStatus emberAfOtaStorageCheckTempDataCallback(int32u* returnOffset,
                                                               int32u* returnTotalSize,
                                                               EmberAfOtaImageId* returnOtaImageId)
{
  OtaImage* image;

  if (tempStorageFilepath == NULL) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  
  image = addImageFileToList(tempStorageFilepath, TRUE);
  if (image == NULL) {
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  *returnTotalSize = image->header->imageSize;
  *returnOffset = image->fileSize;
  MEMSET(returnOtaImageId, 0, sizeof(EmberAfOtaImageId));
  *returnOtaImageId = emAfOtaStorageGetImageIdFromHeader(image->header);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

EmberAfOtaStorageStatus emAfOtaStorageAppendImageData(const char* filename,
                                                      int32u length, 
                                                      const int8u* data)
{
  OtaImage* image = findImageByFilename(filename);
  if (image != NULL) {
    // The file is already in use by the storage device, so it cannot
    // be modified.
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  
  return writeRawData(APPEND_OFFSET, filename, length, data);
}

int remainingAllocations(void)
{
  return allocations;
}

EmberAfOtaStorageStatus emberAfOtaStorageGetFullHeaderCallback(const EmberAfOtaImageId* id,
                                                               EmberAfOtaHeader* returnData)
{
  OtaImage* image = imageSearchInternal(id);
  if (image == NULL) {
    return FALSE;
  }
  MEMCOPY(returnData, image->header, sizeof(EmberAfOtaHeader));
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

int32u emberAfOtaStorageGetTotalImageSizeCallback(const EmberAfOtaImageId* id)
{
  OtaImage* image = imageSearchInternal(id);
  if (image == NULL) {
    return 0;
  }
  return image->fileSize;
}

EmberAfOtaStorageStatus emberAfOtaStorageFinishDownloadCallback(int32u offset)
{
  // Not used.  The OS filesystem keeps track of the latest download offset.
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

void emAfOtaStorageInfoPrint(void)
{
  note("Storage Module:     OTA POSIX Filesystem Storage Module\n");
  note("Storage Directory:  %s\n", defaultStorageDirectory);
}

int32u emberAfOtaStorageDriverMaxDownloadSizeCallback(void)
{
  // In theory we are limited by the local disk space, but for now
  // assume there is no limit.
  return 0xFFFFFFFFUL;
}

EmberAfOtaStorageStatus emberAfOtaStorageDriverPrepareToResumeDownloadCallback(void)
{
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

//==============================================================================
// Internal Functions
//==============================================================================

static EmberAfOtaStorageStatus createDefaultStorageDirectory(void)
{
  struct stat statInfo;
  int returnValue = stat(defaultStorageDirectory, &statInfo);
  if (returnValue == 0) {
    if (!S_ISDIR(statInfo.st_mode)) {
      error("Default storage directory '%s' is not a directory!\n",
            defaultStorageDirectory);
      return EMBER_AF_OTA_STORAGE_ERROR;
    }

    debug(config.fileDebug, 
          "Default storage directory already exists '%s'\n",
          defaultStorageDirectory);
  } else {
    //   Does not exist, therefore we must create it.
    debug(config.fileDebug,
          "Creating default storage directory '%s'\n",
          defaultStorageDirectory);

    int status = portableMkdir(defaultStorageDirectory);

    if (status != 0) {
      error("Could not create default directory '%s': %s\n",
            defaultStorageDirectory,
            strerror(errno));
      return EMBER_AF_OTA_STORAGE_ERROR;
    }
  }
  return emAfOtaSetStorageDevice(defaultStorageDirectory);
}

static boolean doEui64sMatch(const int8u* firstEui64,
                             const int8u* secondEui64)
{
  return (0 == MEMCOMPARE(firstEui64, secondEui64, EUI64_SIZE));
}

static OtaImage* findImageByFilename(const char* tempFilepath)
{
  OtaImage* ptr = imageListFirst;
  while (ptr != NULL) {
    if (0 == strcmp(ptr->filepath, tempFilepath)) {
      return ptr;
    }
    ptr = (OtaImage*)ptr->next;
  }
  return NULL;
}

static EmberAfOtaStorageStatus writeRawData(int32u offset,
                                            const char* filepath,
                                            int32u length,
                                            const int8u* data)
{
  // Windows requires the 'b' (binary) as part of the mode so that line endings
  // are not truncated.  POSIX ignores this.
  FILE* fileHandle = fopen(filepath, 
                           "r+b");
  EmberAfOtaStorageStatus status;
  int whence = SEEK_SET;

  if (fileHandle == NULL) {
    error("Could not open file '%s' for writing: %s\n",
          filepath,
          strerror(errno));
    goto writeEnd;
  }

  if (offset == APPEND_OFFSET) {
    offset = 0;
    whence = SEEK_END;
  }

  if (0 != fseek(fileHandle, offset, whence)) {
    error("Could not seek to offset 0x%08X (%s) in file '%s': %s\n",
          offset,
          (whence == SEEK_END ? "SEEK_END" : "SEEK_SET"),
          filepath,
          strerror(errno));
    goto writeEnd;
  }

  size_t written = fwrite(data, 1, length, fileHandle);
  if (written != length) {
    error("Tried to write %d bytes but wrote %d\n", length, written);
    goto writeEnd;
  }
  status = EMBER_AF_OTA_STORAGE_SUCCESS;

 writeEnd:
  if (fileHandle) {
    fclose(fileHandle);
  }
  return status;
}

static void removeImage(OtaImage* image)
{
  OtaImage* before = (OtaImage*)image->prev;
  OtaImage* after = (OtaImage*)image->next;
  if (before) {
    before->next = (struct OtaImage*)after;
  }
  if (after) {
    after->prev = (struct OtaImage*)before;
  }
  if (image == imageListFirst) {
    imageListFirst = after;
  }
  if (image == imageListLast) {
    imageListLast = before;
  }
  freeOtaImage(image);
  imageCount--;
}

static void printEui64(int8u* eui64)
{
  note("%02X%02X%02X%02X%02X%02X%02X%02X",
       eui64[0],
       eui64[1],
       eui64[2],
       eui64[3],
       eui64[4],
       eui64[5],
       eui64[6],
       eui64[7]);
}

static void printHeaderInfo(const EmberAfOtaHeader* header)
{
  if (!config.printFileDiscoveryOrRemoval) {
    return;
  }

  //  printf("  Header Version:  0x%04X\n",
  //         header->headerVersion);
  //  printf("  Header Length:   0x%04X\n",
  //         header->headerLength);
  //  printf("  Field Control:   0x%04X\n",
  //         header->fieldControl);
  note("  Manufacturer ID: 0x%04X\n",
       header->manufacturerId);
  note("  Image Type ID:   0x%04X\n",
       header->imageTypeId);
  note("  Version:         0x%08X\n",
       header->firmwareVersion);
  note("  Header String:   %s\n", 
       header->headerString);
  //  printf("\n");
}

static OtaImage* addImageFileToList(const char* filename, 
                                    boolean printImageInfo)
{
  OtaImage* newImage = (OtaImage*)myMalloc(sizeof(OtaImage), 
                                           "addImageFileToList():OtaImage");
  if (newImage == NULL) {
    return NULL;
  }
  memset(newImage, 0, sizeof(OtaImage));
  
  struct stat statInfo;
  if (0 != stat(filename, &statInfo)) {
    myFree(newImage);
    return NULL;
  }
  newImage->fileSize = statInfo.st_size;

  int length = 1 + strnlen(filename, OTA_MAX_FILENAME_LENGTH);
  newImage->filepath = myMalloc(length, "filename");
  if (newImage->filepath == NULL) {
    goto dontAdd;
  }
  strncpy(newImage->filepath, filename, length);
  newImage->filepath[length - 1] = '\0';
  newImage->filenameStart = strrchr(newImage->filepath,
                                    '/');
  if (newImage->filenameStart == NULL) {
    newImage->filenameStart = newImage->filepath;
  } else {
    newImage->filenameStart++;  // +1 for the '/' character
  }

  newImage->header = readImageHeader(filename);
  if (newImage->header == NULL) {
    goto dontAdd;
  }

  EmberAfOtaImageId new = { 
    newImage->header->manufacturerId,
    newImage->header->imageTypeId,
    INVALID_FIRMWARE_VERSION,
    INVALID_EUI64,
  };
  OtaImage* test;
  if (headerHasUpgradeFileDest(newImage->header)) {
    MEMCOPY(new.deviceSpecificFileEui64,
            newImage->header->upgradeFileDestination,
            EUI64_SIZE);
  }
  test = imageSearchInternal(&new);
  
  if (test != NULL) {
    if (newImage->header->firmwareVersion
        < test->header->firmwareVersion) {
      note("Image '%s' has earlier version number (0x%08X) than existing one '%s' (0x%08X).  Skipping.\n",
           newImage->filenameStart,
           newImage->header->firmwareVersion,
           test->filenameStart,
           test->header->firmwareVersion);
      goto dontAdd;
    } else if (newImage->header->firmwareVersion
               > test->header->firmwareVersion) {
      note("Image '%s' has newer version number (0x%08X) than existing one '%s' (0x%08X).  Replacing existing one.\n",
           newImage->filenameStart,
           newImage->header->firmwareVersion,
           test->filenameStart,
           test->header->firmwareVersion);
      removeImage(test);
    } else { // Same version.
      if (newImage->header->upgradeFileDestination
          || test->header->upgradeFileDestination) {
        if (!newImage->header->upgradeFileDestination
            || !test->header->upgradeFileDestination) {
          // Do nothing.  The two images don't match, so we can add it.
        } else if (doEui64sMatch(newImage->header->upgradeFileDestination,
                                 test->header->upgradeFileDestination)) {
          note("Image '%s' has the same version number (0x%08X) and upgrade destination (",
                 newImage->filenameStart,
                 newImage->header->firmwareVersion);
          printEui64(newImage->header->upgradeFileDestination);
          note(") as an existing one\n");
          goto dontAdd;
        }
      } else {
        printf("Image '%s' has the same version number (0x%08X) as existing one.\n",
               newImage->filenameStart,
               newImage->header->firmwareVersion);
        goto dontAdd;
      }
    }
  }

  if (imageListFirst == NULL) {
    imageListFirst = newImage;
    imageListLast = newImage;
  } else {
    imageListLast->next = (struct OtaImage*)newImage;
    newImage->prev = (struct OtaImage*)imageListLast;
    imageListLast = newImage;
  }

  if (printImageInfo) {
    printHeaderInfo(newImage->header);
  }
  
  imageCount++;
  return newImage;

 dontAdd:
  freeOtaImage(newImage);
  return NULL;
}

// This function assumes that the file pointer is at the start of the
// file.  It will modify the pointer in the passed FILE*
static boolean checkMagicNumber(FILE* fileHandle, boolean printError)
{
  int8u magicNumber[4];
  if (1 != fread((void*)&magicNumber, 4, 1, fileHandle)) {
    return FALSE;
  }
  if (0 != memcmp(magicNumber, otaFileMagicNumberBytes, 4)) {
    if (printError) {
      error("File has bad magic number.\n");
    }
    return FALSE;
  }
  return TRUE;
}

static EmberAfOtaStorageStatus initImageDirectory(void)
{
  DIR* dir = opendir(storageDevice);
  if (dir == NULL) {
    error("Could not open directory: %s\n", strerror(errno));
    return EMBER_AF_OTA_STORAGE_ERROR;
  }

  debug(config.fileDebug, "Opened Storage Directory: %s\n", storageDevice);

  struct dirent* dirEntry = readdir(dir);
  while (dirEntry != NULL) {
    FILE* fileHandle = NULL;
    debug(config.fileDebug, "Considering file '%s'\n", dirEntry->d_name);

    // +2 for trailing '/' and '\0'
    int pathLength = strlen(storageDevice) + strlen(dirEntry->d_name) + 2;
    if (pathLength > MAX_FILEPATH_LENGTH) {
      error("Filepath too long (max: %d) skipping file '%s'",
            MAX_FILEPATH_LENGTH,
            dirEntry->d_name);
      goto continueReadingDir;
    }
    char* filePath = myMalloc(pathLength, "initImageDirectory(): filepath");
    if (filePath == NULL) {
      error("Failed to allocate memory for filepath.\n");
      goto continueReadingDir;
    }
    
    sprintf(filePath, "%s%s", 
            storageDevice, 
            dirEntry->d_name);
    debug(config.fileDebug, "Full filepath: '%s'\n", filePath);

    struct stat buffer;
    if (0 != stat(filePath, &buffer)) {
      fprintf(stderr,
              "Error: Could not stat file '%s': %s\n", 
              filePath,
              strerror(errno));
      goto continueReadingDir;
    } else if (S_ISDIR(buffer.st_mode) || !S_ISREG(buffer.st_mode)) {
      debug(config.fileDebug, 
            "Ignoring '%s' because it is not a regular file.\n",
            dirEntry->d_name);

      goto continueReadingDir;
    }
    
    // NOTE:  dirent.d_name may have a limited length due to POSIX compliance.
    // Not sure if it will work for all possible filenames.  However
    // the length does NOT include the directory portion, so it should be
    // able to store most all filenames (<256 characters).

    // Windows requires the 'b' (binary) as part of the mode so that line endings
    // are not truncated.  POSIX ignores this.
    fileHandle = fopen(filePath, "rb");
    if (fileHandle == NULL) {
      error("Could not open file '%s' for reading: %s\n",
            filePath,
            strerror(errno));
      goto continueReadingDir;
    }

    if (!checkMagicNumber(fileHandle, FALSE)) {
      goto continueReadingDir;
    }
    if (config.ignoreFilesWithUnderscorePrefix
        && dirEntry->d_name[0] == '_') {
      // As a means of making this program omit certain OTA files from 
      // processing, we arbitrarily choose to ignore files starting with '_'.
      // This is done in part to be able to store multiple OTA files in
      // the same directory that have the same unique manufacturer and image 
      // type ID.  Normally when this code finds a second image with the
      // same manufacturer and image type ID it picks the one with latest 
      // version number. 
      // By changing the file name we can keep the file intact and
      // have this code just skip it.
      printf("Ignoring OTA file '%s' since it starts with '_'.\n",
             dirEntry->d_name);
      goto continueReadingDir;
    }
    if (config.printFileDiscoveryOrRemoval) {
      note("Found OTA file '%s'\n", dirEntry->d_name);
    }

    // We don't really care about the return code because we want to keep trying
    // to add files.
    OtaImage* newImage = addImageFileToList(filePath, TRUE);
    if (config.fileAddedHandler != NULL
        && newImage != NULL) {
      (config.fileAddedHandler)(newImage->header);
    }

  continueReadingDir:
    if (fileHandle) {
      fclose(fileHandle);
      fileHandle = NULL;
    }
    if (filePath) {
      myFree(filePath);
      filePath = NULL;
    }
    dirEntry = readdir(dir);
  }
  if (config.printFileDiscoveryOrRemoval) {
    printf("Found %d files\n\n", imageCount);
  }
  closedir(dir);
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

static void freeIfNotNull(void** ptr)
{
  if (*ptr != NULL) {
    myFree(*ptr);
    *ptr = NULL;
  }
}

static void freeOtaImage(OtaImage* image)
{
  if (image == NULL) {
    return;
  }
  freeIfNotNull((void**)&(image->header));
  freeIfNotNull((void**)&(image->filepath));
  myFree(image);
}

static EmberAfOtaHeader* readImageHeader(const char* filename)
{
  EmberAfOtaHeader* header = NULL;
  // Windows requires the 'b' (binary) as part of the mode so that line endings
  // are not truncated.  POSIX ignores this.
  FILE* fileHandle = fopen(filename, "rb");
  if (fileHandle == NULL) {
    goto imageReadError;
  }

  header = (EmberAfOtaHeader*)myMalloc(sizeof(EmberAfOtaHeader), "readImageHeader():OtaImage");
  if (header == NULL) {
    goto imageReadError;
  }
  memset(header, 0, sizeof(EmberAfOtaHeader));
  if (!checkMagicNumber(fileHandle, TRUE)) {
    goto imageReadError;
  }
  // In all the following code, we subtract 4 from the lengths because we have
  // already read the magic number and therefore do not need to include it in
  // our calculations.
  int8u buffer[OTA_MAXIMUM_HEADER_LENGTH - 4];
  int8u* bufferPtr = buffer;
  int dataRead = fread((void*)bufferPtr, 
                       1,                             // block size (bytes)
                       OTA_MAXIMUM_HEADER_LENGTH - 4, // count
                       fileHandle);
  if (dataRead < (OTA_MINIMUM_HEADER_LENGTH - 4)) {
    error("OTA header is too short (length = %d but should be a minimum of %d)\n", 
          dataRead,
          OTA_MINIMUM_HEADER_LENGTH - 4);
    goto imageReadError;
  }
  mapHeaderFieldDefinitionToDataStruct(header);

  // Read the Version and length first so we can use those to validate the rest
  // of the image.
  EmberAfOtaStorageStatus status;
  status = readHeaderDataFromBuffer(&(otaHeaderFieldDefinitions[HEADER_VERSION_INDEX]),
                                    bufferPtr,
                                    otaHeaderFieldDefinitions[HEADER_VERSION_INDEX].length,
                                    otaHeaderFieldDefinitions[HEADER_VERSION_INDEX].length);
  if (status != EMBER_AF_OTA_STORAGE_SUCCESS) {
    goto imageReadError;
  }
  if (header->headerVersion != otaFileVersionNumber) {
    error("Unknown header version number 0x%04X in file, cannot parse.\n", header->headerVersion);
    goto imageReadError;    
  }
  bufferPtr += 2;  // header version field length
  status = readHeaderDataFromBuffer(&(otaHeaderFieldDefinitions[HEADER_LENGTH_INDEX]),
                                    bufferPtr,
                                    otaHeaderFieldDefinitions[HEADER_LENGTH_INDEX].length,
                                    otaHeaderFieldDefinitions[HEADER_LENGTH_INDEX].length);
  if (status != EMBER_AF_OTA_STORAGE_SUCCESS) {
    goto imageReadError;
  }
  bufferPtr += 2;  // header length field length

  // subtract 4 for length of "header length" and "header version" fields.
  int32s lengthRemaining = header->headerLength - 4; 
  dataRead -= 4;

  EmberAfOtaHeaderFieldDefinition* fieldPtr = &(otaHeaderFieldDefinitions[2]);
  while (fieldPtr->name != NULL && dataRead > 0 && lengthRemaining > 0) {
    EmberAfOtaStorageStatus status = readHeaderDataFromBuffer(fieldPtr,
                                                       bufferPtr,
                                                       lengthRemaining,
                                                       dataRead);
    if (status != EMBER_AF_OTA_STORAGE_SUCCESS) {
      goto imageReadError;
    }
    if (fieldPtr->found == TRUE) {
      lengthRemaining -= fieldPtr->length;
      bufferPtr += fieldPtr->length;
      dataRead -= fieldPtr->length;
    } 
    fieldPtr++;
  }
  // Insure Header string is NULL terminated.  The structure has one extra
  // byte than the EMBER_AF_OTA_MAX_HEADER_STRING_LENGTH
  header->headerString[EMBER_AF_OTA_MAX_HEADER_STRING_LENGTH] = '\0';

  fieldPtr = &(otaHeaderFieldDefinitions[0]);
  while (fieldPtr->name != NULL) {
    if (fieldPtr->maskForOptionalField == ALWAYS_PRESENT_MASK
        && !fieldPtr->found) {
      error("Missing field '%s' from OTA header.\n", fieldPtr->name);
      goto imageReadError;
    }
    fieldPtr++;
  }

  unmapHeaderFieldDefinitions();
  fclose(fileHandle);

  //  printHeaderInfo(header);
  
  return header;

  imageReadError:
    unmapHeaderFieldDefinitions();
    freeIfNotNull((void**)&header);
    fclose(fileHandle);
    return NULL;
}

static EmberAfOtaStorageStatus readHeaderDataFromBuffer(EmberAfOtaHeaderFieldDefinition* definition,
                                                 int8u* bufferPtr,
                                                 int32s headerLengthRemaining,
                                                 int32s actualBufferDataRemaining)
{
  if (definition->maskForOptionalField != ALWAYS_PRESENT_MASK) {
    int16u fieldControl = *(int16u*)(otaHeaderFieldDefinitions[FIELD_CONTROL_INDEX].location);
    if (! (fieldControl & definition->maskForOptionalField)) {
      // No more processing.
      return EMBER_AF_OTA_STORAGE_SUCCESS;
    }
  }
  if (headerLengthRemaining < definition->length
      || actualBufferDataRemaining < definition->length) {
      error("OTA Header does not contain enough data for field '%s'\n",
            definition->name);
      return EMBER_AF_OTA_STORAGE_ERROR;
  }

  if (definition->type == INTEGER_FIELD) {
    // Unfortunately we have to break up parsing of different integer lengths
    // into separate pieces of code because of the way the data
    // may be packed in the data structure.  
    // Previously we tried to just use a generic 'int32u*' to point
    // to either the location of a 32-bit, 16-bit, or 8-bit value and then write
    // the data appropriately. However if the location could only store a 16-bit
    // value and we are referring to it as an int32u*, then we may write to the 
    // wrong bytes depending on the how the data is packed.
    // Using the correct pointer based on the length sidesteps that problem.
    if (1 == definition->length) {
      int8u *value = (int8u*)(definition->location);
      *value = bufferPtr[0];
    } else if (2 == definition->length) {
      int16u *value = (int16u*)(definition->location);
      *value = (bufferPtr[0]
                + (bufferPtr[1] << 8));
    } else if (4 == definition->length) {
      int32u *value = (int32u*)(definition->location);
      *value = (bufferPtr[0]
                + (bufferPtr[1] << 8)
                + (bufferPtr[2] << 16)
                + (bufferPtr[3] << 24));
    } else {
      error("Unsupported data value length '%d' for type '%s'.\n",
            definition->length,
            definition->name);
      return EMBER_AF_OTA_STORAGE_ERROR;
    }
  } else if (definition->type == BYTE_ARRAY_FIELD
             || definition->type == STRING_FIELD) {
    memcpy(definition->location, bufferPtr, definition->length);
  } else {
    // Programatic error
    error("Unkown field type '%d'\n",
          definition->type);
    return EMBER_AF_OTA_STORAGE_ERROR;
  }
  definition->found = TRUE;
  return EMBER_AF_OTA_STORAGE_SUCCESS;
}

static void mapHeaderFieldDefinitionToDataStruct(EmberAfOtaHeader* header)
{
  otaHeaderFieldDefinitions[HEADER_VERSION_INDEX].location = &(header->headerVersion);
  otaHeaderFieldDefinitions[HEADER_LENGTH_INDEX].location = &(header->headerLength);
  otaHeaderFieldDefinitions[FIELD_CONTROL_INDEX].location = &(header->fieldControl);
  otaHeaderFieldDefinitions[MANUFACTURER_ID_INDEX].location = &(header->manufacturerId);
  otaHeaderFieldDefinitions[IMAGE_TYPE_INDEX].location = &(header->imageTypeId);
  otaHeaderFieldDefinitions[FIRMWARE_VERSION_INDEX].location = &(header->firmwareVersion);
  otaHeaderFieldDefinitions[ZIGBEE_STACK_VERSION_INDEX].location = &(header->zigbeeStackVersion);
  otaHeaderFieldDefinitions[TOTAL_IMAGE_SIZE_INDEX].location = &(header->imageSize);
  otaHeaderFieldDefinitions[SECURITY_CREDENTIALS_INDEX].location = &(header->securityCredentials);
  otaHeaderFieldDefinitions[MINIMUM_HARDWARE_VERSION_INDEX].location = &(header->minimumHardwareVersion);
  otaHeaderFieldDefinitions[MAXIMUM_HARDWARE_VERSION_INDEX].location = &(header->maximumHardwareVersion);

  // For byte arrays and strings, those are already pointers and we do not want
  // with another layer of indirection
  otaHeaderFieldDefinitions[HEADER_STRING_INDEX].location = header->headerString;
  otaHeaderFieldDefinitions[UPGRADE_FILE_DESTINATION_INDEX].location = header->upgradeFileDestination;
}

static void unmapHeaderFieldDefinitions(void)
{
  EmberAfOtaHeaderFieldDefinition* ptr = otaHeaderFieldDefinitions;
  while (ptr->name != NULL) {
    ptr->location = NULL;
    ptr->found = FALSE;
    ptr++;
  }
}

static OtaImage* findImageById(const EmberAfOtaImageId* id)
{
  OtaImage* ptr = imageListFirst;
  while (ptr != NULL) {
    if (id->manufacturerId == ptr->header->manufacturerId
        && id->imageTypeId == ptr->header->imageTypeId
        && id->firmwareVersion == ptr->header->firmwareVersion) {
      return ptr;
    }
    ptr = (OtaImage*)ptr->next;
  }
  return NULL;
}

static int8u* writeHeaderDataToBuffer(EmberAfOtaHeaderFieldDefinition* definition,
                                      int8u* bufferPtr)
{
  if (definition->maskForOptionalField != ALWAYS_PRESENT_MASK) {
    int16u fieldControl = *(int16u*)(otaHeaderFieldDefinitions[FIELD_CONTROL_INDEX].location);
    if (! (fieldControl & definition->maskForOptionalField)) {
      debug(config.fieldDebug, "Skipping field %s\n", definition->name);

      // No more processing.
      return bufferPtr;
    }
  }
  debug(config.fieldDebug,
        "Writing field %s, %s field, length %d\n", 
        definition->name, 
        fieldNames[definition->type],
        definition->length);

  if (definition->type == BYTE_ARRAY_FIELD
      || definition->type == STRING_FIELD) {
    memcpy(bufferPtr, definition->location, definition->length);
  } else if (definition->type == INTEGER_FIELD) {
    if (definition->length == 1) {
      int8u *value = definition->location;
      bufferPtr[0] = *value;
    } else if (definition->length == 2) {
      int16u *value = definition->location;
      bufferPtr[0] = (int8u)(*value);
      bufferPtr[1] = (int8u)(*value >> 8);
    } else if (definition->length == 4) {
      int32u *value = definition->location;
      bufferPtr[0] = (int8u)(*value);
      bufferPtr[1] = (int8u)(*value >> 8);
      bufferPtr[2] = (int8u)(*value >> 16);
      bufferPtr[3] = (int8u)(*value >> 24);
    } else {
      assert(0);
    }
  }
  return (bufferPtr + definition->length);
}


static int16u calculateOtaFileHeaderLength(EmberAfOtaHeader* header)
{
  int16u length = 4; // the size of the magic number
  EmberAfOtaHeaderFieldDefinition* definition = &(otaHeaderFieldDefinitions[HEADER_VERSION_INDEX]);
  while (definition->name != NULL) {
    if (definition->maskForOptionalField == ALWAYS_PRESENT_MASK
        || (header->fieldControl & definition->maskForOptionalField)) {
      length += definition->length;
    }
    definition++;
  }
  return length;
}

static OtaImage* imageSearchInternal(const EmberAfOtaImageId* id)
{
  OtaImage* ptr = imageListFirst;
  OtaImage* newest = NULL;
  while (ptr != NULL) {
    /*
    note("imageSearchInternal: Considering file '%s' (MFG: 0x%04X, Image: 0x%04X)\n", 
         ptr->filenameStart,
         ptr->header->manufacturerId,
         ptr->header->imageTypeId);
    */
    if (ptr->header->manufacturerId == id->manufacturerId
        && (ptr->header->imageTypeId == id->imageTypeId)) {
      if (id->firmwareVersion == INVALID_FIRMWARE_VERSION) {
        newest = ptr;
      } else if (ptr->header->firmwareVersion
                 == id->firmwareVersion) {
        newest = ptr;
      }

      // Check whether the upgrade file dest the search
      // and the one in the file.
      if (newest) {
        if (doEui64sMatch(emberAfInvalidImageId.deviceSpecificFileEui64,
                          id->deviceSpecificFileEui64)
            && !headerHasUpgradeFileDest(newest->header)) {
          // Keep going.  There is no upgrade file dest in either the
          // search criteria or the file.  This match may or may not be
          // the latest version number.
        } else if (headerHasUpgradeFileDest(newest->header)
                   && doEui64sMatch(id->deviceSpecificFileEui64,
                                    newest->header->upgradeFileDestination)) {
          // Because there is an exact match on the EUI64, we know
          // this is the only one that can match the request and can
          // return that now.
          return newest;
        } else {
          // There is an upgrade file dest in either the file or the search
          // criteria but not in the other.  Therefore this cannot be a
          // match.
          newest = NULL;
        }
      }
    }
    ptr = (OtaImage*)ptr->next;
  }
  return newest;
}

static EmberAfOtaImageId getIteratorImageId(void)
{
  if (iterator == NULL) {
    return emberAfInvalidImageId;
  }
  return emAfOtaStorageGetImageIdFromHeader(iterator->header);
}

//------------------------------------------------------------------------------
// DEBUG

static void* myMalloc(size_t size, const char* allocName)
{
  void* returnValue = malloc(size);
  if (returnValue != NULL) {
    allocations++;
    debug(config.memoryDebug, 
          "[myMalloc] %s, %d bytes (0x%08X)\n", 
          allocName, size, returnValue);
  }
  return returnValue;
}

static void myFree(void* ptr)
{
  debug(config.memoryDebug, "[myFree] 0x%08X\n", ptr);
  free(ptr);
  allocations--;
}

//------------------------------------------------------------------------------
// Print routines.

static void message(FILE* stream, 
                    boolean error, 
                    const char* formatString, 
                    va_list ap)
{
  if (messagePrefix) {
    fprintf(stream, "[%s] ", messagePrefix);
  }
  if (error) {
    fprintf(stream, "Error: ");
  }
  vfprintf(stream, formatString, ap);
  fflush(stream);
}

static void note(const char* formatString, ...)
{
  va_list ap;
  va_start(ap, formatString);
  message(stdout, FALSE, formatString, ap);
  va_end(ap);
}

static void debug(boolean debugOn, const char* formatString, ...)
{
  if (debugOn) {
    va_list ap;
    va_start(ap, formatString);
    message(stdout, FALSE, formatString, ap);
    va_end(ap);
  }
}

static void error(const char* formatString, ...)
{
  va_list ap;
  va_start(ap, formatString);
  message(stderr, TRUE, formatString, ap);
  va_end(ap);
}

#endif  // defined(GATEWAY_APP)

