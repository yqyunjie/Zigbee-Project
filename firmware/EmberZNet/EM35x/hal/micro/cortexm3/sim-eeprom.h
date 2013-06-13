/** @file hal/micro/cortexm3/sim-eeprom.h
 * @brief Simulated EEPROM system for wear leveling token storage across flash.
 * See @ref simeeprom2 for documentation.
 *
 * Copyright 2007-2011 by Ember Corporation. All rights reserved.           *80*
 */
#ifndef __PLAT_SIM_EEPROM_H__
#define __PLAT_SIM_EEPROM_H__

#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/flash.h"
 
/** @addtogroup simeeprom
 * By default, the EM35x Simulated EEPROM is designed to consume 8kB of
 * upper flash within which it will perform wear leveling.  It is possible
 * to reduce the reserved flash to only 4kB by defining EMBER_SIM_EEPROM_4KB
 * when building your application.
 *
 * While there is no specific ::EMBER_SIM_EEPROM_8KB, it is possible to use
 * the define ::EMBER_SIM_EEPROM_8KB for clarity in your application.
 *
 * @note Switching between an 8kB and 4kB Simulated EEPROM should be done
 * by rebuilding the entire application to ensure all sources recognize
 * the change.  The Simualted EEPROM is designed to seamlessly move
 * between 8kB and 4kB without erasing flash first.
 *
 * \b
 *
 * @note Switching between 8kB and 4kB Simulated EEPROM will <b>
 * automatically erase all </b> Simulated EEPROM tokens and reload
 * them from default values.
 */

/** @addtogroup simeeprom2
 * @note <b>Simulated EEPROM 2 is only available for use in the NCP.</b>
 * @note Simulated EEPROM and Simulated EEPROM 2 functions cannot be
 * intermixed; SimEE and SimEE2 are mutually exclusive.  The functions
 * in @ref simeeprom cannot be used with
 * SimEE2 and the functons in @ref simeeprom2 cannot be used
 * with SimEE.
 *
 * The Simulated EEPROM 2 system (typically referred to as SimEE2) is
 * designed to operate under the @ref tokens
 * API and provide a non-volatile storage system.  Since the flash write cycles
 * are finite, the SimEE2's primary purpose is to perform wear
 * leveling across several hardware flash pages, ultimately increasing the
 * number of times tokens may be written before a hardware failure.
 *
 * Compiling the application with the define EMBER_SIMEE2 will switch
 * the application from using the original SimEE to SimEE2.
 *
 * @note Only the NCP is capable of upgrading it's existing SimEE data
 * to SimEE2.  It's not possible to downgrade from SimEE2.
 *
 * The Simulated EEPROM 2 needs to periodically perform a page erase 
 * operation to reclaim storage area for future token writes.  The page
 * erase operation requires an ATOMIC block of 21ms.  Since this is such a long
 * time to not be able to service any interrupts, the page erase operation is
 * under application control providing the application the opportunity to
 * decide when to perform the operation and complete any special handling
 * that might be needed.
 *
 * @note The best, safest, and recommended practice is for the application
 * to regularly and always call the function halSimEeprom2ErasePage() when the
 * application can expect and deal with the page erase delay.
 * halSimEeprom2ErasePage() will immediately return if there is nothing to
 * erase.  If there is something that needs to be erased, doing so
 * as regularly and as soon as possible will keep the SimEE2 in the
 * healthiest state possible.
 *
 * SimEE2  differs from the
 * original SimEE in terms of size and speed.  SimEE2 holds more data
 * but at the expense of consuming more overall flash to support a new
 * wear levelling technique.  SimEE2's worst case execution time under
 * normal behavior is faster and finite, but the average execution time is
 * longer.
 *
 * See hal/micro/cortexm3/sim-eeprom.h for source code.
 *@{ 
 */

 
/** @brief The Simulated EEPROM 2 callback function, implemented by the
 * application.
 *
 * @param status  An ::EmberStatus error code indicating one of the conditions
 * described below.
 *
 * This callback will report an EmberStatus of
 * ::EMBER_SIM_EEPROM_ERASE_PAGE_GREEN whenever a token is set and a page needs
 * to be erased.  If the main application loop does not periodically
 * call halSimEeprom2ErasePage(), it is best to then erase a page in
 * response to ::EMBER_SIM_EEPROM_ERASE_PAGE_GREEN.
 *
 * This callback will report an EmberStatus of ::EMBER_SIM_EEPROM_ERASE_PAGE_RED
 * when the pages <i>must</i> be erased to prevent data loss.
 * halSimEeprom2ErasePage() needs to be called until it returns 0 to indicate
 * there are no more pages that need to be erased.  Ignoring
 * this indication and not erasing the pages will cause dropping the new data
 * trying to be written.
 *
 * This callback will report an EmberStatus of ::EMBER_SIM_EEPROM_FULL when
 * the new data cannot be written due to unerased pages.  <b>Not erasing
 * pages regularly, not erasing in response to
 * ::EMBER_SIM_EEPROM_ERASE_PAGE_GREEN, or not erasing in response to
 * ::EMBER_SIM_EEPROM_ERASE_PAGE_RED will cause
 * ::EMBER_SIM_EEPROM_FULL and the new data will be lost!.</b>  Any future
 * write attempts will be lost as well.
 *
 * This callback will report an EmberStatus of ::EMBER_SIM_EEPROM_REPAIRING
 * when the Simulated EEPROM 2 needs to repair itself.  While there's nothing
 * for an app to do when the SimEE2 is going to repair itself (SimEE2 has to
 * be fully functional for the rest of the system to work), alert the
 * application to the fact that repairing is occuring.  There are debugging
 * scenarios where an app might want to know that repairing is happening;
 * such as monitoring frequency.
 * @note  Common situations will trigger an expected repair, such as using
 *        a new chip or changing token definitions.
 *
 * If the callback ever reports the status ::EMBER_ERR_FLASH_WRITE_INHIBITED or
 * ::EMBER_ERR_FLASH_VERIFY_FAILED, this indicates a catastrophic failure in
 * flash writing, meaning either the address being written is not empty or the
 * write itself has failed.  If ::EMBER_ERR_FLASH_WRITE_INHIBITED is 
 * encountered, the function ::halInternalSimEeRepair(FALSE) should be called 
 * and the chip should then be reset to allow proper initialization to recover.
 * If ::EMBER_ERR_FLASH_VERIFY_FAILED is encountered the Simulated EEPROM 2 (and
 * tokens) on the specific chip with this error should not be trusted anymore.
 *
 */
void halSimEeprom2Callback(EmberStatus status);

/** @brief Erase a hardware flash page, if needed.
 * 
 * The Simulated EEPROM 2 will periodically
 * request through the ::halSimEeprom2Callback() that a page be erased.  The
 * Simulated EEPROM 2 will never erase a page itself (which could result in
 * data loss)
 * and relies entirely on the application to call this function to trigger
 * a page erase (only one erase per call to this function).
 * 
 * This function can be 
 * called at anytime from anywhere in the application and will only take effect
 * if needed (otherwise it will return immediately).  Since this function takes
 * 21ms to erase a hardware page during which interrupts cannot be serviced,
 * it is preferable to call this function while in a state that can withstand
 * being unresponsive for so long.  The best, safest, and recommended practice
 * is for the application
 * to regularly and always call the function halSimEeprom2ErasePage() when the
 * application can expect and deal with the page erase delay.
 * halSimEeprom2ErasePage() will immediately return if there is nothing to
 * erase.  If there is something that needs to be erased, doing so
 * as regularly and as soon as possible will keep the SimEE2 in the
 * healthiest state possible. 
 * 
 * @return  A count of how many virtual pages are left to be erased.  This
 * return value allows for calling code to easily loop over this function
 * until the function returns 0.
 */
int8u halSimEeprom2ErasePage(void);

/** @brief Provides two basic statistics.
 *  - The number of unused words until SimEE2 is full
 *  - The total page use count
 * 
 * There is a lot
 * of management and state processing involved with the Simulated EEPROM 2,
 * and most of it has no practical purpose in the application.  These two
 * parameters provide a simple metric for knowing how soon the Simulated
 * EEPROM 2 will be full (::freeWordsInCurrPage)
 * and how many times (approximatly) SimEE2 has rotated pysical flash
 * pages (::totalPageUseCount).
 *
 * @param freeWordsUntilFull  Number of unused words
 * available to SimEE2 until the SimEE2 is full and would trigger an
 * ::EMBER_SIM_EEPROM_ERASE_PAGE_RED then ::EMBER_SIM_EEPROM_FULL callback.
 *
 * @param totalPageUseCount  The value of the highest page counter 
 * indicating how many times the Simulated EEPROM
 * has rotated physical flash pages (and approximate write cycles).
 */
void halSimEeprom2Status(int16u *freeWordsUntilFull, int16u *totalPageUseCount);

/**@} // END simeeprom2 group
 */


#ifndef DOXYGEN_SHOULD_SKIP_THIS

// See the sim-eeprom-size.h file for the logic used to determine the ultimate
// size of the simeeprom.
#include "hal/micro/cortexm3/sim-eeprom-size.h"

//This is confusing, so pay attention...  :-)
//The actual SimEE storage lives inside of simulatedEepromStorage and begins
//at the very bottom of simulatedEepromStorage and fills the entirety of
//this storage array.  The SimEE code, though, uses 16bit addresses for
//everything since it was originally written for the XAP2b.  On a 250,
//the base address was 0xF000 since this corresponded to the actual absolute
//address that was used in flash.  On a non-250, though, the base address
//is largely irrelevant since there is a translation shim layer that converts
//SimEE 16bit addresses into the real 32bit addresses needed to access flash.
//If you look at the translation shim layer in
//sim-eeprom-internal.c you'll see that the address used by the SimEE is
//subtracted by VPA_BASE (which is the same as SIMEE_BASE_ADDR_HW) to
//return back to the bottom of the simulatedEepromStorage area.
//[BugzId:14448 fix removes need for this to be anything but 0x0000]
#define SIMEE_BASE_ADDR_HW  0x0000

//Define a variable that holds the actual SimEE storage the linker will
//place at the proper location in flash.
extern int8u simulatedEepromStorage[SIMEE_SIZE_B];

//these parameters frame the sim-eeprom and are derived from the location
//of the sim-eeprom as defined in memmap.h
/**
 * @brief The size of a physical flash page, in SimEE addressing units.
 */
extern const int16u REAL_PAGE_SIZE;
/**
 * @brief The size of a Virtual Page, in SimEE addressing units.
 */
extern const int16u VIRTUAL_PAGE_SIZE;
/**
 * @brief The number of physical pages in a Virtual Page.
 */
extern const int8u REAL_PAGES_PER_VIRTUAL;
/**
 * @brief The bottom address of the Left Virtual Page.
 * Only used in Simulated EEPROM 1.
 */
extern const int16u LEFT_BASE;
/**
 * @brief The top address of the Left Virtual Page.
 * Only used in Simulated EEPROM 1.
 */
extern const int16u LEFT_TOP;
/**
 * @brief The bottom address of the Right Virtual Page.
 * Only used in Simulated EEPROM 1.
 */
extern const int16u RIGHT_BASE;
/**
 * @brief The top address of the Right Virtual Page.
 * Only used in Simulated EEPROM 1.
 */
extern const int16u RIGHT_TOP;
/**
 * @brief The bottom address of Virtual Page A.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPA_BASE;
/**
 * @brief The top address of Virtual Page A.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPA_TOP;
/**
 * @brief The bottom address of Virtual Page B.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPB_BASE;
/**
 * @brief The top address of Virtual Page B.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPB_TOP;
/**
 * @brief The bottom address of Virtual Page C.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPC_BASE;
/**
 * @brief The top address of Virtual Page C.
 * Only used in Simulated EEPROM 2.
 */
extern const int16u VPC_TOP;
/**
 * @brief The memory address at which point erasure requests transition
 * from being "GREEN" to "RED" when the freePtr crosses this address.
 * Only used in Simulated EEPROM 1.
 */
extern const int16u ERASE_CRITICAL_THRESHOLD;


/** @brief Gets a pointer to the token data.  
 *
 * @note This function only works on Cortex-M3 chips since it requires
 * being able to return a pointer to flash memory.
 *
 * @note Only the public function should be called since the public
 * function provides the correct parameters.
 *
 * The Simulated EEPROM uses a RAM Cache
 * to hold the current state of the Simulated EEPROM, including the location
 * of the freshest token data.  This function simply uses the ID and
 * index parameters to access the pointer RAM Cache.  The absolute flash
 * address stored in the pointer RAM Cache is extracted and ptr is
 * set to this address. 
 *
 * Here is an example of calling this function:
 * @code
 * tokTypeStackCalData calData = {0,};
 * tokTypeStackCalData *calDataToken = &calData;
 * halStackGetIdxTokenPtrOrData(&calDataToken,
 *                              TOKEN_STACK_CAL_DATA,
 *                              index);
 * @endcode
 *
 * @param ptr  A pointer to where the absolute address of the data should 
 * be placed.
 *
 * @param ID    The ID of the token to get data from.  Since the token system
 * is designed to enumerate the token names down to a simple 8 bit ID,
 * generally the TOKEN_name is the parameter used when invoking this function.
 *
 * @param index If token being accessed is indexed, this parameter is
 * combined with the ID to formulate both the RAM Cache lookup as well
 * as the desired InfoWord to look for.  If the token is not an indexed token,
 * this parameter is ignored.
 *
 * @param len   The number of bytes being worked on.  This should always be
 * the size of the token (available from both the sizeof() intrinsic as well
 * as the tokenSize[] array).
 *
 */
void halInternalSimEeGetPtr(void *ptr, int8u compileId, int8u index, int8u len);

#endif //__PLAT_SIM_EEPROM_H__

#endif //DOXYGEN_SHOULD_SKIP_THIS

