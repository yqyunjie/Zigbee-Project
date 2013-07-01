/** @file bootloader-interface-standalone-v1.h
 * See @ref alone_bootload for documentation.
 * 
 * @brief Definition of the interface to the standalone bootloader.
 *
 * Some functions in this file return an ::EmberStatus value. See 
 * error-def.h for definitions of all ::EmberStatus return values.
 *
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup alone_bootload
 *
 * See bootloader-interface-standalone-v1.h for source code.
 * 
 *@{
 */

#ifndef __BOOTLOADER_STANDALONE_V1_H__
#define __BOOTLOADER_STANDALONE_V1_H__

/**
 * A "signature" to identify the bootloader type and help catch
 * any app/bootloader mismatch.
 */
#define BOOTLOADER_TYPE_STANDALONE   0xBEEF  
// BOOTLOADER_TYPE_APP is defined in bootloader-interface-app.h

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Automatically used by the trap functions for triggering the bootloader
#define BOOTLOADER_APP_REQUEST_KEY 0xFA27

// Constants used by the standalone bootloader
#define FLASH_TAB_POINTERS                        0x00F0
#define FLASH_TAB_POINTERS_DEFAULT_TOKEN_TAB_PTR  0      //DEPRECATED
#define FLASH_TAB_POINTERS_UPPER_TOKEN_TAB_PTR    2      //DEPRECATED
#define FLASH_TAB_POINTERS_BOOT_TRAP_TAB_PTR      4
#define FLASH_TAB_POINTERS_HELPERFUNC_TAB         6
#define BOOTLOADER_TRAPTAB_SIGNATURE              0xBEEF

#ifdef __HAS_ELPM__
  #define LPM_HIGH(x,y) \
    do {                \
      int8u reg;        \
      reg = RAMPZ;      \
      y = __extended_load_program_memory(x); \
      RAMPZ = reg;      \
    } while(0)

//  #define LPM_HIGH __extended_load_program_memory
  #define FLASH_HIGH __farflash
#else
  #define LPM_HIGH(x,y) \
    do { \
      y = __load_program_memory(x); \
    } while(0)

  #define FLASH_HIGH __flash
#endif // __HAS_ELPM__




/** @name Bootloader Accessor Functions */
//@{

/** @brief Enumerations for possible bootloader trap 
 * identifiers.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
enum StandaloneBootloaderTraps
#else
typedef int8u StandaloneBootloaderTraps;
enum
#endif
{
  BOOT_TRAP_APP_START = 1,
  BOOT_TRAP_FLASH_ADJUST = 3
};
#endif // DOXYGEN_SHOULD_SKIP_THIS


/** @brief Calls the bootloader trap functions and may be used for
 *  either the stand-alone or application bootloader.  
 *  Each trap function
 *  has a wrapper macro in hal/micro/.../.../micro.h.  Use the wrapper
 *  functions under normal circumstances because they improve readability
 *  and automatically check for bootloader type. 
 *  
 * @param assumedBootloaderType There are different traps for the
 * stand-alone and application bootloaders.  This guards against calling a trap
 * from the wrong bootloader type.
 *
 * @param trapNumber  Trap identifier.
 * 
 * @param ... Parameter list corresponding to the trap.
 *  
 * @return  An error if the trap identifier is invalid, or ::EMBER_SUCCESS.
 */
EmberStatus halInternalStandaloneBootloaderTrap(int16u assumedBootloaderType,
                                                int8u trapNumber, ...);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Launches the bootloader.
 * @note This function is \b DEPRECATED. 
 */
void bootloadAppStart(void);
#endif // DOXYGEN_SHOULD_SKIP_THIS

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Trap for accessing bootloadAppStart()
  *
 *  @param space   Size of block, in words.  Must be 0 to 128.
 *
 *  @param segment Byte address pointer. Must be an even address and within
 * the first 64k bytes of flash address space. Usually use to point to the
 * beginning of a flash block.
 *  
 *  @param ramPtr  Byte address pointer. Must be within the first 64k bytes of
 * flash address space.
 *
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_SUCCESS if the segment is written.
 */
#define halBootloadAppStart(space, segment, ramPtr);
#else
#define halBootloadAppStart(space, segment, ramPtr)            \
   halInternalStandaloneBootloaderTrap(BOOTLOADER_TYPE_APP,   \
                                       BOOT_TRAP_APP_START)
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief Modifies a block of flash without losing existing data.
 *   
 * @note \c dst and \c size may be set so that they span multiple pages. Any
 * remaining portion of those pages will remain unchanged other than having 
 * been erased and rewritten.
 * 
 * @param dst   Byte address pointer. Must be an even address and within the first 
 * 64k bytes of flash address space. Usually use to point to the beginning of a
 * flash block.
 *     
 * @param src   Byte address pointer. Must be within the first 64k bytes of 
 * flash address space. 
 *     
 * @param size  Size of block, in words. Must be 0 to 128.
 */
void halInternalAdjustFlashBlock(int16u* dst, int16u* src, int8u size);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Trap for accessing halInternalAdjustFlashBlock()
 *
 *  @param space    Size of block, in words.  Must be 0 to 128.
 *
 *  @param segment  Byte address pointer. Must be an even address and within
 * the first 64k bytes of flash address space. Usually use to point to the
 * beginning of a flash block.
 *  
 *  @param ramPtr   Byte address pointer. Must be within the first 64k bytes of
 * flash address space.
 *
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_SUCCESS if the segment is written.
 */
#define halBootloadAdjustFlash(space, segment, ramPtr);
#else
#define halBootloadAdjustFlash(space, segment, ramPtr)         \
   halInternalStandaloneBootloaderTrap(BOOTLOADER_TYPE_APP,   \
                                       BOOT_TRAP_FLASH_ADJUST, \
                                       (segment), (ramPtr), (space))
#endif // DOXYGEN_SHOULD_SKIP_THIS

 // Note:  bootloaderMode 0 means go into passive bootloader mode.  Note:
  //          this includes single hop pass thru.
  //        bootloaderMode 1 means clone to node in gatewayID
  //        bootloaderMode 2 means look for an errant node and clone to it.
  //        bootloaderMode 3 means look for an errant node and do pass thru.
  //        bootloaderMode 4 (not valid here) is later on used for single hop

enum {
  BOOTLOADER_MODE_MENU                 = 0x00,
  BOOTLOADER_MODE_PASSTHROUGH          = 0x00,
  BOOTLOADER_MODE_CLONE                = 0x01,
  BOOTLOADER_MODE_CLONE_RECOVERY       = 0x02,
  BOOTLOADER_MODE_PASSTHROUGH_RECOVERY = 0x03
};

/** @brief Quits the current application and launches the stand-alone
 * bootloader (if installed) in a non-over-the-air mode (usually the serial
 * port). The function returns an error if the stand-alone bootloader is not
 * present.
 *   
 * @param mode  Controls the mode in which the standalone bootloader will
 * run.  See the app. note for full details.  Options are:
 * - ::BOOTLOADER_MODE_MENU:  Allow user to upload an image via XMODEM, either to
 *   the current node or via single hop over the air passthrough mode.
 * - ::BOOTLOADER_MODE_PASSTHROUGH_RECOVERY:  Special mode to allow uploading an
 *   image over the air to a node that has been lost as a result of an 
 *   unsuccessful bootload.
 * - ::BOOTLOADER_MODE_CLONE:  Upload the current node image to the new node.
 * - ::BOOTLOADER_MODE_CLONE_RECOVERY:  Special mode to allow cloning of the
 *   current node's image to a node that has been lost as a result of an 
 *   unsuccessful bootload.
 *
 * @param eui64  For ::BOOTLOADER_MODE_CLONE and ::BOOTLOADER_MODE_MENU,
 * this parameter specifies the EUI64 of the other node for single hop over
 * the air bootloading.
 * 
 * @return An error if the stand-alone bootloader is not present, or 
 * ::EMBER_SUCCESS.
 */
EmberStatus halLaunchStandaloneBootloaderV1(int8u mode, int8u *eui64);

//@}  // end of Bootloader Accessor Functions

#endif //__BOOTLOADER_STANDALONE_V1_H__

/**@} // END addtogroup
 */
