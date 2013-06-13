// *****************************************************************************
// * eeprom.c
// *
// * Code for manipulating the EEPROM from the Application Framework
// * In particular, sleepies that use the EEPROM will require re-initialization
// * of the driver code after they wake up from sleep.  This code helps
// * manage the state of the driver. 
// *
// * Copyright 2012 by Silicon Labs.      All rights reserved.              *80*
// *****************************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#include "app/framework/include/af.h"
#include "app/framework/plugin/eeprom/eeprom.h"

//------------------------------------------------------------------------------
// Globals

static boolean eepromInitialized = FALSE;

// NOTE:
// In EmberZNet 4.3 we required the code required that the 
// underlying EEPROM driver MUST have support for arbitrary page writes
// (i.e. writes that cross page boundaries and or are smaller than the page size)
// Specifically the OTA Storage EEPROM Driver plugin code for storing OTA images
// requires this.

// This is no longer a requirement due to the fact that we have formal page-erase
// support built into the OTA code.  However for systems using a read-modify-write
// driver we have support here.

// The App. Bootloader for the 35x SOC prior to 4.3 did NOT have read-modify-write support.
// Therefore the shared app. bootloader EEPROM routines CANNOT be used; a copy
// of the EEPROM driver must be included with the application to support the OTA
// cluster.  
// The 4.3 App. bootloader for the 35x does have arbitrary page-write support
// and thus the shared EEPROM routines may be used on the 35x SOC.

// The 250 has no shared bootloader EEPROM routines and so the application
// must include a copy of the EEPROM driver.  The Host co-processor based models
// must also include an EEPROM driver in their application that has arbitrary 
// page-write support.

#if defined(EZSP_HOST) \
 || defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM_ENABLE_SOC_APP_BOOTLOADER_COMPATIBILITY_MODE)
  #define eepromInit() halEepromInit()
  #define eepromWrite(address, data, len) \
    halEepromWrite((address), (data), (len))
  #define eepromRead(address, data, len) \
    halEepromRead((address), (data), (len))
  #define eepromErase(address,len) \
    halEepromErase((address), (len))
  #define eepromBusy() \
    halEepromBusy()
  #define eepromInfo() \
    halEepromInfo()
  #define eepromShutdown() \
    halEepromShutdown()

#else // EM35x SOC with 4.3 bootloader or later
  #define eepromInit() halAppBootloaderInit()
  #define eepromWrite(address, data, len) \
    halAppBootloaderWriteRawStorage((address), (data), (len))
  #define eepromRead(address, data, len) \
    halAppBootloaderReadRawStorage((address), (data), (len))
  #define eepromErase(address, len) \
    halAppBootloaderEraseRawStorage((address), (len))
  #define eepromBusy()                            \
    halAppBootloaderStorageBusy()
  #define eepromInfo() \
    halAppBootloaderInfo()
  #define eepromShutdown() \
    halAppBootloaderShutdown() 
#endif

//------------------------------------------------------------------------------

// Sleepies will need a re-initialization of the driver after sleep,
// so this code helps manage that state and automatically re-init the driver
// if it is needed.

static boolean isEepromInitialized(void)
{
  return eepromInitialized;
}

void emberAfPluginEepromNoteInitializedState(boolean state)
{
  eepromInitialized = state;
}

void emberAfPluginEepromInit(void)
{
  if (isEepromInitialized()) {
    return;
  }
  
  eepromInit();
  emberAfPluginEepromNoteInitializedState(TRUE);
}

const HalEepromInformationType* emberAfPluginEepromInfo(void)
{
  emberAfPluginEepromInit();
  return eepromInfo();
}

int8u emberAfPluginEepromWrite(int32u address, const int8u *data, int16u totalLength)
{
  int8u status;
  emberAfPluginEepromInit();
  status = eepromWrite(address, data, totalLength);

  EMBER_TEST_ASSERT(status == EEPROM_SUCCESS);

  return status;
}

int8u emberAfPluginEepromRead(int32u address, int8u *data, int16u totalLength)
{
  int8u status;
  emberAfPluginEepromInit();
  status = eepromRead(address, data, totalLength);

  EMBER_TEST_ASSERT(status == EEPROM_SUCCESS);

  return status;
}

int8u emberAfPluginEepromErase(int32u address, int16u totalLength)
{
  int8u status;
  emberAfPluginEepromInit();
  status = eepromErase(address, totalLength);

  EMBER_TEST_ASSERT(status == EEPROM_SUCCESS);

  return status;
}

boolean emberAfPluginEepromBusy(void)
{
  emberAfPluginEepromInit();
  return eepromBusy();
}

// Returns TRUE if shutdown was done, returns FALSE if shutdown was not
// necessary.
boolean emberAfPluginEepromShutdown(void)
{
  if (!isEepromInitialized()) {
    return FALSE;
  }

  eepromShutdown();
  emberAfPluginEepromNoteInitializedState(FALSE);
  return TRUE;
}

//------------------------------------------------------------------------------
// These callbacks are defined so that other parts of the system can make
// these calls regardless of whether the EEPROM is actually used.  A
// user generated callback will be created if they don't use
// the plugin.
void emberAfEepromInitCallback(void)
{
  emberAfPluginEepromInit();
}

void emberAfEepromNoteInitializedStateCallback(boolean state)
{
  emberAfPluginEepromNoteInitializedState(state);
}

void emberAfEepromShutdownCallback(void)
{
  emberAfPluginEepromShutdown();
}
