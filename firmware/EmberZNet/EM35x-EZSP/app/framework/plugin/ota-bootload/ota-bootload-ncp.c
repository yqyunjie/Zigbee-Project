// ****************************************************************************
// * ota-bootload-ncp.c
// *
// * Routines for bootloading an NCP UART.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/util/common.h"
#include "app/framework/util/attribute-storage.h"
#include "enums.h"
#include "app/framework/util/af-main.h"

#include "app/framework/plugin/ota-common/ota.h"
#include "app/framework/plugin/ota-storage-common/ota-storage.h"
#include "hal/micro/bootloader-interface-standalone.h"

#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include "app/ezsp-uart-host/ash-host-io.h"

#include "ota-bootload-ncp.h"
#include "ota-bootload-xmodem.h"

//------------------------------------------------------------------------------
// Globals

// Xmodem requires all blocks be 128 bytes in size
#define TRANSFER_BLOCK_SIZE 128

// Until we have a formal facility for this code, we just use core.
#define bootloadPrintln(...) emberAfCorePrintln(__VA_ARGS__)
#define bootloadDebugExec(x) emberAfCoreDebugExec(x)
#if defined(EMBER_AF_PRINT_CORE)
  #define BOOTLOAD_PRINT_ENABLED
#endif

// We arbitrarily chose 5% as the minimum update amount when we are
// transfering the file to the NCP.  This provides a good amount of feedback
// during the process but not too much.
#define BOOTLOAD_PERCENTAGE_UPDATE  5

#if defined(GATEWAY_APP)
  // UART assumed
  #define START_IMMEDIATELY FALSE
#else 
  // SPI assumed
  #define START_IMMEDIATELY TRUE
#endif

//------------------------------------------------------------------------------
// Forward Declarations

static boolean transferFile(const EmberAfOtaImageId* id, int16u ncpUpgradeTagId);

//------------------------------------------------------------------------------

// This hands control of the entire application to this code to perform
// the bootloading.  It will not return until the bootload has completed
// or it has failed.

int8u emberAfOtaBootloadCallback(const EmberAfOtaImageId* id,
                                 int16u ncpUpgradeTagId)
{
  boolean success = TRUE;
  EzspStatus status;
  bootloadPrintln("Launching standalone bootloader...");

  status = 
    ezspLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
  if (status != EMBER_SUCCESS) {
    bootloadPrintln("Launch failed: 0x%X", status);
    return 1;
  }
  ezspClose();

  bootloadPrintln("Starting bootloader communications.");
  emberAfCoreFlush();
  if (!emAfStartNcpBootloaderCommunications()) {
    success = FALSE;
    bootloadPrintln("Failed to start bootloading communications.");
    emberAfCoreFlush();
  } else {
    success = transferFile(id, ncpUpgradeTagId);

    emberAfCoreFlush();

    // Regardless of success or failure we reboot the NCP in hopes
    // of returning the system back to its previous state.
    
    // Use &= here to preserve the possible failed status returned
    // by transferFile()
    success &= emAfRebootNcpAfterBootload();
  }

  emAfPostNcpBootload(success);

  return 0;
}

static boolean transferFile(const EmberAfOtaImageId* id,
                            int16u ncpUpgradeTagId)
{
  int32u offset;
  int32u endOffset;

  // We extract the EBL (upgrade image) from the OTA file format
  // and pass it to the bootloader.  The bootloader has no knowledge
  // of OTA files, only EBL images.

  bootloadPrintln("Transferring EBL file to NCP...");

  if (EMBER_AF_OTA_STORAGE_SUCCESS
      != emAfOtaStorageGetTagOffsetAndSize(id,
                                           ncpUpgradeTagId,
                                           &offset,
                                           &endOffset)) {
    bootloadPrintln("Failed to get offset and size for tag 0x%2X inside OTA file.",
                    ncpUpgradeTagId);
    return FALSE;
  }
  
  endOffset += offset;

  emAfPrintPercentageSetStartAndEnd(offset, endOffset);
  bootloadPrintln("EBL data start: 0x%4X, end: 0x%4X, size: %d bytes",
                  offset,
                  endOffset,
                  endOffset - offset);
                    
  emAfInitXmodemState(START_IMMEDIATELY);

  while (offset < endOffset) {
    int32u returnedLength;
    int32u readSize = TRANSFER_BLOCK_SIZE;
    int8u block[TRANSFER_BLOCK_SIZE];

    MEMSET(block, 0, TRANSFER_BLOCK_SIZE);

    if ((endOffset - offset) < TRANSFER_BLOCK_SIZE) {
      readSize = endOffset - offset;
    }

    halResetWatchdog();

    if (EMBER_AF_OTA_STORAGE_SUCCESS
        != emberAfOtaStorageReadImageDataCallback(id,
                                                  offset,
                                                  readSize,
                                                  block,
                                                  &returnedLength)
        || returnedLength != readSize) {
      bootloadPrintln("Failed to read image data at offset 0x%4X", offset);
      return FALSE;
    }

    offset += returnedLength;
    if (!emAfSendXmodemData(block, 
                            (int16u)returnedLength, 
                            (offset == endOffset))) {   // finish?
      bootloadPrintln("Failed to send data to NCP.");
      emberAfCoreFlush();
      return FALSE;
    }
    
    emAfPrintPercentageUpdate("Transfer", 
                              BOOTLOAD_PERCENTAGE_UPDATE,
                              offset);
  }

  bootloadPrintln("Transfer completed successfully.");
  return TRUE;
}

