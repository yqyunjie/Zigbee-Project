/*
 * File: standalone-bootloader-custom.c
 * Description: Customizeable portions of the standalone bootloader
 *
 *
 * Copyright 2009 by Ember Corporation. All rights reserved.                *80*
 */
//[[ Author(s): David Iacobone, diacobone@ember.com
//              Lee Taylor, lee@ember.com
//]]

#include PLATFORM_HEADER
#include "bootloader-gpio.h"
#include "bootloader-common.h"
#include "bootloader-serial.h"
#include "hal/micro/cortexm3/memmap.h"
#include "standalone-bootloader.h"
#include "hal/micro/micro.h"


#ifdef CORTEXM3_EM351
  #define MY_EM3XX "EM351 "
#elif defined CORTEXM3_EM357
  #define MY_EM3XX "EM357 "
#elif defined CORTEXM3_EM3581
  #define MY_EM3XX "EM3581"
#elif defined CORTEXM3_EM3582
  #define MY_EM3XX "EM3582"
#elif defined CORTEXM3_EM3585
  #define MY_EM3XX "EM3585"
#elif defined CORTEXM3_EM3586
  #define MY_EM3XX "EM3586"
#elif defined CORTEXM3_EM3588
  #define MY_EM3XX "EM3588"
#elif defined CORTEXM3_EM359
  #define MY_EM3XX "EM359"
#elif defined CORTEXM3_STM32W108
  #define MY_EM3XX "STM32W "
#else
  #error Unknown micro selected
#endif

#ifdef BTL_HAS_RADIO
  #define MY_RADIO "Serial/OTA "
#else
  #define MY_RADIO "Serial "
#endif

// print the menu
static void printMenu(void)
{
  int32s offset;
  static const char *bootName = "\r\n" MY_EM3XX MY_RADIO "Bootloader v";
  static const char *menu_string = 
          "\r\n"
          "1. upload ebl\r\n"
          "2. run\r\n"
          "3. ebl info\r\n"
          "BL > ";

  serPutStr(bootName);
  
  // Print out the full version number for this bootloader
  offset = sizeof(halBootloaderAddressTable.bootloaderVersion)*8 - 4; // Point to the first nibble
  while(offset >= 0) {
    serPutDecimal((halBootloaderAddressTable.bootloaderVersion>>offset) & 0xF);
    if(offset > 0) {
      serPutChar('.');
    } 
    offset -= 4;
  }
  
  // Print out the build number for this bootloader
  serPutStr(" b");
  serPutDecimal(halBootloaderAddressTable.bootloaderBuild);
 
  serPutStr(menu_string);
}


static void printImageInfo(void)
{
  int8u i = 0;
  int8u ch;

  serPutStr("\r\n");

  if(halAppAddressTable.baseTable.type != APP_ADDRESS_TABLE_TYPE) {
    serPutStr("** no app **\r\n"); 
    return;
  }

  while ((i < 32) && ( (ch = halAppAddressTable.imageInfo[i++]) != 0) )
    serPutChar(ch);

  serPutStr("\r\n");
}

// The approximate number of menu loop iterations in one second
//  (Each loop iteration was measured at about 13.5uS)
#define ONE_SECOND_LOOP_ITERATIONS  (74000)

void bootloaderMenu(void)
{
  int8u ch;
  int16u status;

  halResetWatchdog();
  serGetFlush();

  #ifdef BTL_HAS_RADIO
    initOtaState();
    // Initialize the rebootTimeout counter to about 15 minutes
    int32u rebootTimeout = 15*60*ONE_SECOND_LOOP_ITERATIONS;
  #endif

  BL_STATE_UP();   // indicate bootloader is up

  while(1) {
    // loop polling the serial and radio channels for bootload activity
    halResetWatchdog();
    BL_STATE_POLLING_LOOP();   // indicate we're polling for input

    #ifdef BTL_HAS_RADIO  // Note: RADIO is not yet supported
      status = checkOtaStart();
      if (status == BL_SUCCESS) { 
        serPutStr("\r\nOTA upload ");
        status = receiveImage(COMM_RADIO);

        if (status == BL_SUCCESS) {
          BL_STATE_DOWNLOAD_SUCCESS();   // indicate successful download
          serPutStr("complete\r\n");
          serPutFlush();
          BL_STATE_DOWN();   // going down
          return; // return will reset to newly downloaded application
        } else {
          BL_STATE_DOWNLOAD_FAILURE();   // indicate download failure
          serPutStr("error 0x");
          serPutHexInt(status);
          serPutStr("\r\n");
        }
        initOtaState();  // reset OTA state for next attempt
      }
      if(!rebootTimeout--) {
        // The bootloader phy does not include any extra code to account for
        //  temperature variation.  Instead of increasing complexity to monitor
        //  this, we just periodically reboot which will cause all calibrations
        //  to be re-done.  We reboot using the same reset type so that channel,
        //  panid and additional cal data passed from an application will 
        //  continue to be used.
        halInternalSysReset(halGetExtendedResetInfo());
      }
    #endif
    
    // check serial interface for a command entry
    status = serGetChar(&ch);
    if(status == BL_SUCCESS) {
      serPutChar(ch); // echo

      switch(ch) {
        case '1': // upload app
          serPutStr("\r\nbegin upload\r\n");
          status = receiveImage(COMM_SERIAL);

          serPutStr("\r\nSerial upload ");
          if(status == BL_SUCCESS) {
            BL_STATE_DOWNLOAD_SUCCESS();   // indicate successful download
            serPutStr("complete\r\n");
          } else {
            BL_STATE_DOWNLOAD_FAILURE();   // indicate download failure
            serPutStr("aborted\r\n");
            if (status & BLOCKERR_MASK) {
              serPutStr("\r\nXModem block error 0x");
            } else if (status & BL_ERR_MASK) {
              serPutStr("\r\ndownload file error 0x");
            } else {
              serPutStr("\r\nerror 0x");
            }
            serPutHexInt(status);
            serPutStr("\r\n");
          }
          break;
        case '2': // jump to app
          BL_STATE_DOWN();
          return;  // return will reset to the application
        case '3': // image info
          printImageInfo();
          break;
        case '\r':
        case '\n':
          printMenu();
          break;
        default:
          //ignored
          break;
      }
      serGetFlush();  // flush other incoming chars

    }
  }
}

