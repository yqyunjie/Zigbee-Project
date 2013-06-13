/*
 * File: bootloader-app.c
 * Description: EM35x application bootloader
 *
 * Copyright 2009 by Ember Corporation. All rights reserved.                *80*
 */
//[[ Author(s): David Iacobone, diacobone@ember.com, Lee Taylor, lee@ember.com ]]

#include PLATFORM_HEADER
#include "bootloader-common.h"
#include "bootloader-serial.h"
#include "bootloader-gpio.h"
#include "app-bootloader.h"
#include "hal/micro/micro.h"


void bootloaderAction(boolean runRecovery)
{
  BL_Status status;

  BL_STATE_UP();   // indicate bootloader is up
  BLDEBUG_PRINT("in app bootloader\r\n");

  if(runRecovery) {
    while(1) {
      status = recoveryMode();
      if(status == BL_SUCCESS) {
        // complete image downloaded
        serPutStr("\r\nOk\r\n");
        serPutFlush();
        // reset to reactivate app bootloader to flash image.
        halInternalSysReset(RESET_BOOTLOADER_BOOTLOAD);  // never returns
      } else {
        serPutStr("\r\nERR ");
        serPutHex(status);
        serPutStr("\r\n");
      }
    }
  } else {
    // We were either reset with the BOOTLOAD_BOOTLOAD type, or no valid
    //  image was detected but recovery mode was not activated, so
    // Attempt to install an image from the external eeprom
    
    // first verify the remote image is valid before we disturb flash
    if ( (status = processImage(FALSE)) == BL_SUCCESS ) {
      BLDEBUG_PRINT("image CRC ok\r\n");
    
      // now actually program the new image to main flash
      if ( (status = processImage(TRUE)) == BL_SUCCESS ) {
        BLDEBUG_PRINT("image flashed ok\r\n");
        BL_STATE_DOWNLOAD_SUCCESS();
        
        // reset and run the new image
        halInternalSysReset(RESET_BOOTLOADER_GO);
      } else {
        // flashing failed
        BLDEBUG_PRINT("process image failed ");
      }
    } else {
      // new image verification failed
      BLDEBUG_PRINT("image CRC failed ");
    }
    
    BL_STATE_DOWNLOAD_FAILURE();   // indicate download failure
    (void)status;  // avoid warning about unused status in non BL_DEBUG builds
    BLDEBUG(serPutHex(status));
    BLDEBUG_PRINT("\r\n");
    BLDEBUG_PRINT("reboot to app\r\n");

    // reset with badimage type.  If there is no valid app image anymore,
    //   the bootloader cstartup will detect it and we'll end up back in the
    //   app bootloader and recovery mode will be activated.
    BL_STATE_DOWN();   // going down
    halInternalSysReset(RESET_BOOTLOADER_BADIMAGE);
  }
}
