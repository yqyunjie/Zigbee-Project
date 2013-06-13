/*
 * File: bootloader-interface-app.c
 * Description: Cortex M3 application bootloader interface.
 *              Provides routines used by applications to access and verify
 *              the bootload image.
 *
 * Copyright 2007-2012 by Ember Corporation. All rights reserved.           *80*
 *
 * Notes:
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "hal/micro/cortexm3/bootloader/bootloader-common.h"
#include "hal/micro/cortexm3/bootloader/ebl.h"
#include "hal/micro/cortexm3/memmap.h"

static void verifyAppBlVersion(int16u version)
{
  assert(BOOTLOADER_BASE_TYPE(halBootloaderAddressTable.bootloaderType) 
           == BL_TYPE_APPLICATION &&
         halBootloaderAddressTable.baseTable.version >= version);
}

int8u halAppBootloaderInit(void)
{
  // version 0x0105 is the version where the eeprom shared code was
  //   added to the BAT
  verifyAppBlVersion(0x0105);

  // version 0x0108 is where eepromInit was updated with a return value
  if(halBootloaderAddressTable.baseTable.version >= 0x0108) {
    return halBootloaderAddressTable.eepromInit();
  } else {
    halBootloaderAddressTable.eepromInit();
    return EEPROM_SUCCESS;
  }
}

HalEepromInformationType fixedEepromInfo;

const HalEepromInformationType *halAppBootloaderInfo(void)
{
  // version 0x0105 is the version where the eeprom shared code was
  //   added to the BAT
  verifyAppBlVersion(0x0105);

  // version 0x0108 is where support for this API was added
  if(halBootloaderAddressTable.baseTable.version >= 0x0108) {
    // For internal flash bootloaders we need to ask the app for the size of
    // internal storage since the bootloader doesn't know at build time, but only
    // if we have actually set an internal storage bottom in this app. If not, then
    // we return the default struct which has a size of 0.
    if((halBootloaderAddressTable.bootloaderType == BL_EXT_TYPE_APP_LOCAL_STORAGE) &&
       ((int32u)halAppAddressTable.internalStorageBottom > MFB_BOTTOM)) {
      HalEepromInformationType *temp = (HalEepromInformationType*)halBootloaderAddressTable.eepromInfo();
      MEMCOPY(&fixedEepromInfo, temp, sizeof(fixedEepromInfo));
      fixedEepromInfo.partSize = (MFB_TOP - (int32u)halAppAddressTable.internalStorageBottom + 1);
      return &fixedEepromInfo;
    } else {
      return (HalEepromInformationType *)halBootloaderAddressTable.eepromInfo();
    }
  } else {
    // earlier versions did not support this API, so we can only return NULL
    return NULL;   
  }
}

void halAppBootloaderShutdown(void)
{
  // version 0x0107 is the version where the eeprom drivers were updated
  //  to handle shutdown
  verifyAppBlVersion(0x0107);

  halBootloaderAddressTable.eepromShutdown();
}

EepromStateType eepromState;

#if EEPROM_PAGE_SIZE < EBL_MIN_TAG_SIZE
  #error EEPROM_PAGE_SIZE smaller than EBL_MIN_TAG_SIZE
#endif
static int8u buff[EEPROM_PAGE_SIZE];
EblConfigType eblConfig;


void halAppBootloaderImageIsValidReset(void)
{
  // version 0x0104 is the version where the eblProcess shared code was
  //   moved to its final location in the BAT
  verifyAppBlVersion(0x0104);

  eepromState.address = EEPROM_IMAGE_START;
  eepromState.pages = 0;
  eepromState.pageBufFinger = 0;
  eepromState.pageBufLen = 0;
  halBootloaderAddressTable.eblProcessInit(&eblConfig,  
                                           &eepromState, 
                                           buff, 
                                           EBL_MIN_TAG_SIZE, 
                                           TRUE);
}

int16u halAppBootloaderImageIsValid(void)
{
  BL_Status status;
  // version 0x0104 is the version where the eblProcess shared code was
  //   moved to its final location in the BAT
  verifyAppBlVersion(0x0104);
  
  status = halBootloaderAddressTable.eblProcess(halBootloaderAddressTable.eblDataFuncs, 
                                                &eblConfig,
                                                NULL);
  if(status == BL_EBL_CONTINUE) {
    return BL_IMAGE_IS_VALID_CONTINUE;
  } else if(status == BL_SUCCESS) {
    return eepromState.pages;
  } else {
    // error, return invalid
    return 0;
  }
}




EmberStatus halAppBootloaderInstallNewImage(void)
{
  if(BOOTLOADER_BASE_TYPE(halBootloaderAddressTable.bootloaderType) 
     == BL_TYPE_APPLICATION) {
    // will not return
    halInternalSysReset(RESET_BOOTLOADER_BOOTLOAD);
  }
  return EMBER_ERR_FATAL;
}


int8u halAppBootloaderWriteRawStorage(int32u address, 
                                      const int8u *data, 
                                      int16u len)
{
  // version 0x0107 is the version where the eeprom drivers were updated
  //  to handle arbitrary address/data/len values
  verifyAppBlVersion(0x0107);
         
  return halBootloaderAddressTable.eepromWrite(address, data, len);
}

int8u halAppBootloaderReadRawStorage(int32u address, int8u *data, int16u len)
{
  // version 0x0107 is the version where the eeprom drivers were updated
  //  to handle arbitrary address/data/len values
  verifyAppBlVersion(0x0107);
         
  return halBootloaderAddressTable.eepromRead(address, data, len);
}

int8u halAppBootloaderEraseRawStorage(int32u address, int32u len)
{
  // version 0x0107 is the version where the eeprom drivers were updated
  //  to handle arbitrary address/data/len values.  Should have at least
  //  that version to be calling this API.
  verifyAppBlVersion(0x0107);

  // version 0x0108 is where support for erasing storage was added
  if(halBootloaderAddressTable.baseTable.version >= 0x0108) {
    return halBootloaderAddressTable.eepromErase(address, len);
  } else {
    // earlier versions did not support this API, but earlier drivers also
    //  did not require erasing, so we return an error
    return EEPROM_ERR_NO_ERASE_SUPPORT;
  }  
}

boolean halAppBootloaderStorageBusy(void)
{
  // version 0x0107 is the version where the eeprom drivers were updated
  //  to handle arbitrary address/data/len values.  Should have at least
  //  that version to be calling this API.
  verifyAppBlVersion(0x0107);

  // version 0x0108 is where support for busy detection was added
  if(halBootloaderAddressTable.baseTable.version >= 0x0108) {
    return halBootloaderAddressTable.eepromBusy();
  } else {
    // earlier versions did not support this API, so we can only assume the
    //  operation is complete and return FALSE.
    return FALSE;
  }  
}


// DEPRECATED
// halAppBootloaderReadDownloadSpace
//
// Convert pageToBeRead to an address and call EEPROM read function. 
//
// Parameters:
// pageToBeRead - pass in the page to be read, starting at 0. This will be
//                converted to the appropriate address. Pages are
//                EEPROM_PAGE_SIZE long.
// destRamBuffer - a pointer to the buffer to write to
//
// Returns EEPROM_SUCCESS or EEPROM_ERR 
//
int8u halAppBootloaderReadDownloadSpace(int16u pageToBeRead,
                                        int8u* destRamBuffer)
{
  int32u address;

  // convert page to address
  address = (pageToBeRead * (int32u)EEPROM_PAGE_SIZE) + EEPROM_IMAGE_START;

  // read a page
  return halAppBootloaderReadRawStorage(address,destRamBuffer,EEPROM_PAGE_SIZE);
}

// DEPRECATED
// halAppBootloaderWriteDownloadSpace
// 
// Convert pageToBeWritten to an address and call EEPROM write function
//
// Parameters
// pageToBeWritten - pass in the page to be read, starting at 0. This will
//                   be converted to the appropriate address. Pages are
//                   EEPROM_PAGE_SIZE long.
// RamPtr - a pointer to the data to be written
//
// Returns EEPROM_SUCCESS or EEPROM_ERR
//
// 
int8u halAppBootloaderWriteDownloadSpace(int16u pageToBeWritten,
                                         int8u* RamPtr)
{
  int32u address;

  // convert page to address
  address = (pageToBeWritten * (int32u)EEPROM_PAGE_SIZE) + EEPROM_IMAGE_START;

  // write a page
  return halAppBootloaderWriteRawStorage(address, RamPtr, EEPROM_PAGE_SIZE);
}

// halAppBootloaderGetImageData
// 
// Reads the timestamp and userData section of the saved application image 
// header in EEPROM.
//
// Parameters
// timestamp - time the image was created
//
// userData - the 32 character user data field
//
int8u halAppBootloaderGetImageData(int32u *timestamp, int8u  *userData)
{
  int8u i;
  HalAppAddressTableType *aat;

  // read the first page of the app in EEPROM
  if (halAppBootloaderReadDownloadSpace(EEPROM_FIRST_PAGE, buff) == EEPROM_SUCCESS
      && ( NTOHS(((eblHdr3xx_t *)buff)->signature) == IMAGE_SIGNATURE) ) {

    aat = (HalAppAddressTableType *)(((eblHdr3xx_t *)buff)->aatBuff);
    // save the timestamp
    if (timestamp != NULL)
      *timestamp = aat->timestamp;

    // save the image info
    if (userData != NULL) {
      for (i = 0; i < IMAGE_INFO_MAXLEN; i++) {
        userData[i] = aat->imageInfo[i];
      }
    }
    return EEPROM_SUCCESS;
  } else
    return EEPROM_ERR;
}

// halAppBootloaderGetVersion
//
// Returns the application bootloader version
//
int16u halAppBootloaderGetVersion(void)
{
  if(BOOTLOADER_BASE_TYPE(halBootloaderAddressTable.bootloaderType) 
     == BL_TYPE_APPLICATION) {
    return halBootloaderAddressTable.bootloaderVersion;
  } else {
    return BOOTLOADER_INVALID_VERSION;
  }
}

// halAppBootloaderGetRecoveryVersion
//
// Returns the recovery image version
//
int16u halAppBootloaderGetRecoveryVersion(void)
{
  // the recovery image is part of the app bootloader itself on the 35x
  return halAppBootloaderGetVersion();
}


