/** @file hal/micro/bootloader-interface-standalone.h
 * See @ref alone_bootload for documentation.
 *
 * <!-- Copyright 2006 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup alone_bootload
 * Definition of the interface to the standalone bootloader.
 *
 * See bootloader-interface-standalone.h for source code.
 * 
 * Some functions in this file return an ::EmberStatus value. See 
 * error-def.h for definitions of all ::EmberStatus return values.
 *@{
 */

#ifndef __BOOTLOADER_STANDALONE_H__
#define __BOOTLOADER_STANDALONE_H__

#define BOOTLOADER_INVALID_VERSION 0xFFFF

/** @brief Detects if the standalone bootloader is installed, and if so
 *    returns the installed version.
 *
 *  A returned version of 0x1234 would indicate version 1.2 build 34
 *
 * @return ::BOOTLOADER_INVALID_VERSION if the standalone bootloader is not
 *    present, or the version of the installed standalone bootloader.
 */
int16u halGetStandaloneBootloaderVersion(void);


#define STANDALONE_BOOTLOADER_NORMAL_MODE   1
#define STANDALONE_BOOTLOADER_RECOVERY_MODE 0

/** @brief Quits the current application and launches the standalone
 * bootloader (if installed). The function returns an error if the standalone
 * bootloader is not present.
 *   
 * @param mode  Controls the mode in which the standalone bootloader will run.  See the app. note for full details.  Options are:
 *   - ::STANDALONE_BOOTLOADER_NORMAL_MODE
 *     Will listen for an over-the-air
 *     image transfer on the current channel with current power settings.
 *   - ::STANDALONE_BOOTLOADER_RECOVERY_MODE
 *     Will listen for an over-the-air
 *     image transfer on the default channel with default power settings.
 *   .
 *   Both modes also allow an image transfer to begin via serial xmodem.
 * 
 * @return  An error if the standalone bootloader is not present, or 
 * EMBER_SUCCESS.
 */
EmberStatus halLaunchStandaloneBootloader(int8u mode);


#ifndef XAP2B
  // for AVR standalone-bootloader-v1 compatibility
  #define halLaunchStandaloneBootloader(mode) \
   halLaunchStandaloneBootloaderV1(((mode==STANDALONE_BOOTLOADER_NORMAL_MODE)? \
            BOOTLOADER_MODE_MENU:BOOTLOADER_MODE_PASSTHROUGH_RECOVERY), NULL)
  #include "hal/micro/avr-atmega/bootloader-interface-standalone-v1.h"
#endif // AVR_ATMEGA

#endif //__BOOTLOADER_STANDALONE_H__

/**@} // END addtogroup
 */



