/** @file hal/micro/cortexm3/bootloader/app-bootloader.h
 * See @ref cbh_app for detailed documentation.
 * 
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.       *80* -->
 */

/** @addtogroup cbh_app
 * @brief EM35x application bootloader and generic EEPROM Interface
 *
 * See app-bootloader.h for source code.
 *@{
 */
 
#ifndef __EM350_APP_BOOTLOADER_H__
#define __EM350_APP_BOOTLOADER_H__

///////////////////////////////////////////////////////////////////////////////
/** @name Required Custom Functions
 *@{
 */
/** @brief Drives the app bootloader.  If the ::runRecovery parameter is ::TRUE,
 * the recovery mode should be activated, otherwise it should attempt to
 * install an image.  This function should not return.  It should always exit
 * by resetting the the bootloader.
 *
 * @param runRecovery If ::TRUE, recover mode is activated.  Otherwise, normal
 * image installation is activated.
 */
void bootloaderAction(boolean runRecovery);
/**@} */

///////////////////////////////////////////////////////////////////////////////
/** @name Available Bootloader Library Functions
 * Functions implemented by the bootloader library that may be used by
 * custom functions.
 *@{
 */

/** @brief Activates ::recoveryMode to receive a new image over xmodem.
 * @return ::BL_SUCCESS if an image was successfully received.
 */
BL_Status recoveryMode(void);

/** @brief Processes an image in the external eeprom.
 * @param install If ::FALSE, it will simply validate the image without
 * touching main flash.  If ::TRUE, the image will be programmed to main flash.
 * @return ::BL_SUCCESS if an image was successfully installed/validated
 */
BL_Status processImage(boolean install);
/**@} */


#endif //__EM350_APP_BOOTLOADER_H__

/**@} end of group*/

