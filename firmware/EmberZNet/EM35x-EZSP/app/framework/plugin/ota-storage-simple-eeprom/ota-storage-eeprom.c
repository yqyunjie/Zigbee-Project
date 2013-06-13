// *****************************************************************************
// * ota-storage-eeprom-driver.c
// *
// * This is an integration of the simple OTA storage driver with the low-level
// * EEPROM driver.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "app/framework/plugin/eeprom/eeprom.h"

//#define DEBUG_PRINT
#define OTA_STORAGE_EEPROM_INTERNAL_HEADER
#include "ota-storage-eeprom.h"
#undef OTA_STORAGE_EEPROM_INTERNAL_HEADER

#if defined(EMBER_TEST)
#include "hal/micro/unix/simulation/fake-eeprom.h"
#endif

//------------------------------------------------------------------------------
// Globals

#if defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_READ_MODIFY_WRITE_SUPPORT)
  #define COMPILED_FOR_READ_MODIFY_WRITE TRUE
#else
  #define COMPILED_FOR_READ_MODIFY_WRITE FALSE
#endif

// For debugging only
#define DATA_SIZE 48

// Only needed for Page-erase-required flash parts.
EmberEventControl emberAfPluginOtaStorageSimpleEepromPageEraseEventControl;

#define OTA_EEPROM_SIZE                                       \
  (EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_END      \
   - EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START)

//------------------------------------------------------------------------------

#if defined(DEBUG_PRINT)
  static void printImageInfoStartData(void);
  static void printDataBlock(const int8u* block);
#else
  #define printImageInfoStartData()
  #define printDataBlock(x)
#endif

//------------------------------------------------------------------------------

#if defined(EMBER_TEST)

void emAfSetupFakeEepromForSimulation(void)
{
  setupFakeEeprom(OTA_EEPROM_SIZE,
                  EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_STORAGE_START,    // offset
                  2048,                                                       // page size
                  emberAfPluginEepromInfo()->pageEraseMs,                              
                  (COMPILED_FOR_READ_MODIFY_WRITE == FALSE),
                  2);  // word size
}

#endif

int32u emAfOtaStorageReadInt32uFromEeprom(int32u realOffset)
{
  int8u value[4];
  emberAfPluginEepromRead(realOffset, value, 4);
  return (value[0]
          + ((int32u)value[1] << 8)
          + ((int32u)value[2] << 16)
          + ((int32u)value[3] << 24));
}

void emAfOtaStorageWriteInt32uToEeprom(int32u value, int32u realOffset)
{
  int32u oldValue = emAfOtaStorageReadInt32uFromEeprom(realOffset);
  if (oldValue != value) {
    int8u data[4];
    data[0] = value;
    data[1] = (int8u)(value >> 8);
    data[2] = (int8u)(value >> 16);
    data[3] = (int8u)(value >> 24);

    emberAfPluginEepromWrite(realOffset, data, 4);
  }
}

#if defined (SOC_BOOTLOADING_SUPPORT)

int32u emAfGetEblStartOffset(void)
{
  return emAfOtaStorageReadInt32uFromEeprom(IMAGE_INFO_START + EBL_START_OFFSET_INDEX);
}

static void setEblStartOffset(int32u eblStart)
{
  debugPrint("Writing EBL start offset of 0x%4X to EEPROM offset 0x%4X",
             eblStart,
             IMAGE_INFO_START + EBL_START_OFFSET_INDEX);
  debugFlush();
  emAfOtaStorageWriteInt32uToEeprom(eblStart, IMAGE_INFO_START + EBL_START_OFFSET_INDEX);

  #if defined(DEBUG_PRINT)
  {
    int32u offset = emAfGetEblStartOffset();
    debugPrint("EBL Start Offset: 0x%4X", offset);
  }
  #endif
}
#endif // SOC_BOOTLOADING_SUPPORT

boolean emberAfOtaStorageDriverInitCallback(void)
{
  // Older drivers do not have an EEPROM info structure that we can reference
  // so we must just assume they are okay.  
  if (emberAfPluginEepromInfo() != NULL) {
    assert(emberAfPluginEepromInfo()->partSize >= OTA_EEPROM_SIZE);
  }
  emAfOtaStorageEepromInit();

  return TRUE;
}

// Returns true if the operation crosses the break in the OTA image
// due to Layout 2.  Otherwise returns FALSE.  Modifies
// the OTA offset and turns it into the real EEPROM offset.
// This will be based on the start offset of the EEPROM (since the user
// may have allocated a subset of the EEPROM for OTA and not positioned
// the OTA data at offset 0), and after the image info meta-data.
boolean emAfOtaStorageDriverGetRealOffset(int32u* offset, 
                                          int32u* length)
{
  boolean spansBreak = FALSE;
  int32u realOffset = IMAGE_INFO_START + OTA_HEADER_INDEX + *offset;

#if defined(SOC_BOOTLOADING_SUPPORT)
  int32u eblOffset = emAfGetEblStartOffset();

  if (*offset < eblOffset) {
    // Layout 2, before the break in the OTA image, but spans the break.
    if ((*offset + *length) > eblOffset) {
      spansBreak = TRUE;
      *length = eblOffset - *offset;
    } // Else
      //   Data before the break in the OTA image, but doesn't span the break.
  } else {
    // Layout 2 starting after the break in the image
    realOffset = EEPROM_START + *offset - eblOffset;
  }
#else
  // Layout 1.  Do nothing more than what we have done already.
#endif

  *offset = realOffset;
  return spansBreak;
}


// This ugly code must handle the worst case scenario
// where we are trying to read/write a block of data
// in Layout 2 which spans the break in the OTA image.
// In that case we must perform 2 read/write operations
// to make it work.

static boolean readWritePrimitive(boolean read,
                                  const int8u* writeData,
                                  int32u offset,
                                  int32u length,
                                  int8u* readData)
{
  int8u count = 1;
  int8u i;
  int32u realLength = length;
  int32u realOffset = offset;
  
  boolean spansBreak = emAfOtaStorageDriverGetRealOffset(&realOffset, &realLength);
  if (spansBreak) {
    count = 2;
  }

  // Because the EEPROM code only handles the length as a 16-bit number,
  // we catch that error case.
  if (length > 65535) {
    return FALSE;
  }
 
  for (i = 0; i < count; i++) {
    int8u status;

    debugFlush();
    debugPrint("%p realOffset: 0x%4X, realLength: %l", 
               (read ? "read" : "write"),
               realOffset, 
               realLength);
    debugFlush();

    if (read) {
      status = emberAfPluginEepromRead(realOffset, readData, (int16u)realLength);
    } else {
      status = emberAfPluginEepromWrite(realOffset, writeData, (int16u)realLength);
    }

    if (status != EEPROM_SUCCESS) {
      return FALSE;
    }

    if (count > 1) {
      // Layout 2 only, and spans the break
      realOffset = EEPROM_START;
      if (read) {
        readData += realLength;
      } else {
        writeData += realLength;
      }
      realLength = length - realLength;
    }
  }
  return TRUE;
}

// NOTE:  The magic number here is the "Ember" magic number.
//   It is not the same as the OTA file magic number.
//   It is used solely to verify the validity of the 
//   meta-data stored ahead of the OTA file.
boolean emAfOtaStorageCheckDownloadMetaData(void)
{
  int8u magicNumberExpected[] = { MAGIC_NUMBER , VERSION_NUMBER };
  int8u magicNumberActual[MAGIC_NUMBER_SIZE + VERSION_NUMBER_SIZE];

  emberAfPluginEepromRead(IMAGE_INFO_START + MAGIC_NUMBER_OFFSET,
                 magicNumberActual,
                 MAGIC_NUMBER_SIZE + VERSION_NUMBER_SIZE);
  if (0 != MEMCOMPARE(magicNumberExpected, 
                      magicNumberActual, 
                      MAGIC_NUMBER_SIZE + VERSION_NUMBER_SIZE)) {
    debugPrint("Magic Number or version for download meta-data is invalid");
    debugFlush();
    return FALSE;
  }

  return TRUE;
}

// NOTE:  The magic number referenced here is the "Ember" Magic number.
// See comment above "emAfOtaStorageCheckDownloadMetaData()".
void emAfOtaStorageWriteDownloadMetaData(void)
{
  int8u magicNumber[] = { MAGIC_NUMBER , VERSION_NUMBER };
  debugPrint("Writing download meta-data (magic number and version)");
  debugFlush();
  emberAfPluginEepromWrite(IMAGE_INFO_START + MAGIC_NUMBER_OFFSET,
                  magicNumber,
                  MAGIC_NUMBER_SIZE + VERSION_NUMBER_SIZE);
}

boolean emberAfOtaStorageDriverReadCallback(int32u offset, 
                                            int32u length,
                                            int8u* returnData)
{
  return readWritePrimitive(TRUE,   // read?
                            NULL,   // writeData pointer
                            offset,
                            length,
                            returnData);
}

static boolean socBootloaderSupportWriteHandler(const int8u* dataToWrite,
                                                int32u offset, 
                                                int32u length)
{
#if defined(SOC_BOOTLOADING_SUPPORT)
  int16u headerLength;
  debugPrint("socBootloaderSupportWriteHandler()");
  if (offset == 0) {
    if (length < (HEADER_LENGTH_OFFSET + HEADER_LENGTH_FIELD_LENGTH)) {
      // The expectation is that the first write of download data has at least
      // the header length in it.  Otherwise we can't determine where the EBL
      // starting point is.
      debugPrint("Write to offset 0 is too short!  Must be at least %d bytes",
                 HEADER_LENGTH_OFFSET + HEADER_LENGTH_FIELD_LENGTH);
      return FALSE;
    }
    headerLength = ((dataToWrite[HEADER_LENGTH_OFFSET]
                     + (dataToWrite[HEADER_LENGTH_OFFSET+1] << 8))
                    + TAG_OVERHEAD); 
    setEblStartOffset(headerLength);
  }
#endif // defined(SOC_BOOTLOADING_SUPPORT)

  return TRUE;
}

boolean emberAfOtaStorageDriverWriteCallback(const int8u* dataToWrite,
                                             int32u offset, 
                                             int32u length)
{
  if (!socBootloaderSupportWriteHandler(dataToWrite,
                                        offset,
                                        length)) {
    return FALSE;
  }

  if (readWritePrimitive(FALSE,        // read?
                         dataToWrite,
                         offset,
                         length,
                         NULL)) {      // readData pointer
    emAfStorageEepromUpdateDownloadOffset(offset + length,
                                          FALSE);  // final offset?
    return TRUE;
  }
  return FALSE;
}

void emberAfOtaStorageDriverDownloadFinishCallback(int32u finalOffset)
{
  emAfStorageEepromUpdateDownloadOffset(finalOffset, 
                                        TRUE);  // final offset?
  return;
}

int32u emberAfOtaStorageDriverMaxDownloadSizeCallback(void)
{
  return (OTA_EEPROM_SIZE - MAX_IMAGE_INFO_AND_OTA_HEADER_SIZE);
}

int8u emAfOtaStorageDriverGetWordSize(void)
{
  // The word size in the part was added much later in the driver
  // support.  Therefore we assume the word size is 1 for those devices
  // that don't support the parameter because in prior releases
  // we only released drivers that supported that word size.
  const HalEepromInformationType* part = emberAfPluginEepromInfo();
  int8u wordSize = 1;
  if (part->version >= EEPROM_INFO_MIN_VERSION_WITH_WORD_SIZE_SUPPORT) {
    wordSize = part->wordSizeBytes;
  }
  return wordSize;
}

static void printEepromInfo(void)
{
  const HalEepromInformationType* part = emberAfPluginEepromInfo();
  otaPrintln("\nEEPROM Info");
  otaPrintFlush();
  if (part == NULL) {
    otaPrintln("Not available (older bootloader)"); 
    otaPrintFlush();
  } else {
    int8u wordSize = emAfOtaStorageDriverGetWordSize();
    boolean confirmedWordSize = FALSE;
    if (part->version >= EEPROM_INFO_MIN_VERSION_WITH_WORD_SIZE_SUPPORT) {
      confirmedWordSize = TRUE;
    }
    otaPrintln("Part Description:          %p", part->partDescription);
    otaPrintln("Capabilities:              0x%2X", part->capabilitiesMask);
    otaPrintln("Page Erase time (ms):      %d", part->pageEraseMs);
    otaPrintln("Part Erase time (ms):      %d", part->partEraseMs);
    otaPrintln("Page size (bytes):         %l", part->pageSize);
    otaPrintln("Part size (bytes):         %l", part->partSize);
    otaPrintln("Word size (bytes):         %d (%p)", 
               wordSize,
               (confirmedWordSize
                ? "confirmed" 
                : "assumed"));
  }
}

void emAfOtaStorageDriverInfoPrint(void)
{
  int32u downloadOffset = emberAfOtaStorageDriverRetrieveLastStoredOffsetCallback();

  otaPrintln("Storage Driver:            OTA Simple Storage EEPROM Plugin");
  otaPrintFlush();
  otaPrintln("Read Modify Write Support: " READ_MODIFY_WRITE_SUPPORT_TEXT);
  otaPrintFlush();
  otaPrintln("SOC Bootloading Support:   " SOC_BOOTLOADING_SUPPORT_TEXT);
  otaPrintFlush();
  otaPrintln("Current Download Offset:   0x%4X", downloadOffset);

#if defined(SOC_BOOTLOADING_SUPPORT)
  otaPrintFlush();
  otaPrintln("EBL Start Offset:          0x%4X", emAfGetEblStartOffset());
  otaPrintFlush();
#endif

  otaPrintln("EEPROM Start:              0x%4X", EEPROM_START);
  otaPrintFlush();
  otaPrintln("EEPROM End:                0x%4X", EEPROM_END);
  otaPrintFlush();
  otaPrintln("Image Info Start:          0x%4X", IMAGE_INFO_START);
  otaPrintFlush();
  otaPrintln("Save Rate (bytes)          0x%4X", SAVE_RATE);
  otaPrintFlush();
  otaPrintln("Offset of download offset  0x%4X", IMAGE_INFO_START + SAVED_DOWNLOAD_OFFSET_INDEX);
  otaPrintFlush();
  otaPrintln("Offset of EBL offset:      0x%4X", IMAGE_INFO_START + EBL_START_OFFSET_INDEX);
  otaPrintFlush();
  otaPrintln("Offset of image start:     0x%4X", IMAGE_INFO_START + OTA_HEADER_INDEX);
  otaPrintFlush();

#if defined(DEBUG_PRINT)
  {
    int8u data[DATA_SIZE];

    otaPrintln("\nData at EEPROM Start");
    emberAfPluginEepromRead(EEPROM_START, data, DATA_SIZE);
    emberAfPrintCert(data);  // certs are 48 bytes long
    otaPrintFlush();
  }
  printImageInfoStartData();

#endif

  printEepromInfo();

}

#if defined(DEBUG_PRINT)

static void printImageInfoStartData(void)
{
  int8u data[DATA_SIZE];
  int8u i;
  otaPrintln("\nData at Image Info Start");
  otaPrintFlush();

  for (i = 0; i < MAX_IMAGE_INFO_AND_OTA_HEADER_SIZE; i+= DATA_SIZE) {
    otaPrintln("Read Offset: 0x%4X", (IMAGE_INFO_START + i));
    emberAfPluginEepromRead((IMAGE_INFO_START + i), data, DATA_SIZE);
    emberAfPrintCert(data);  // certs are 48 bytes long
    otaPrintFlush();
  }
}

static void printDataBlock(const int8u* block)
{
  int8u i;
  for (i = 0; i < DATA_SIZE; i+= 8) {
    otaPrintFlush();
    otaPrintln("%X %X %X %X %X %X %X %X",
               block[i],
               block[i+1],
               block[i+2],
               block[i+3],
               block[i+4],
               block[i+5],
               block[i+6],
               block[i+7]);
    otaPrintFlush();
  }
}

void emberAfPluginEepromTest(void)
{
  int8u data[DATA_SIZE];
  int8u i;
  int32u addressOffset = 0;
  int8u value;
  int8u length = 4;
  
  for (i = 0; i < 2; i++) {
    int32u address = addressOffset + (i * DATA_SIZE);
    value = 0x09 + i;
    MEMSET(data, value, DATA_SIZE);
    otaPrintln("Writing value 0x%X to address 0x%4X", value, address);
    emberAfPluginEepromWrite(address, data, DATA_SIZE);
    MEMSET(data, 0, DATA_SIZE);
    emberAfPluginEepromRead(address, data, DATA_SIZE);
    printDataBlock(data);
    otaPrintln("");
    addressOffset += 240;  // this is less than the ATMEL part's page
                           // size (256) which means read/write operations
                           // will span two pages
  }

  addressOffset = 0;
  value = 0x02;
  otaPrintln("Re-writing value 0x%X of length %d to address 0x%4X",
             value,
             length,
             addressOffset);
  MEMSET(data, value, DATA_SIZE);
  emberAfPluginEepromWrite(addressOffset, data, length);
  MEMSET(data, 0, DATA_SIZE);
  emberAfPluginEepromRead(addressOffset, data, DATA_SIZE);
  printDataBlock(data);
  otaPrintln("");
  //  writeInt32uToEeprom(value, addressOffset);
}

#endif // if defined(DEBUG_PRINT)
