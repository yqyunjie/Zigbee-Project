/** @file hal/micro/bootloader-interface-app.h
 * 
 * @brief Definition of the interface to the application bootloader.
 *
 * See @ref app_bootload for documentation.
 *
 * Some functions in this file return an ::EmberStatus value. See 
 * error-def.h for definitions of all ::EmberStatus return values.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup app_bootload
 *
 * See hal/micro/bootloader-interface-app.h for source code.
 *@{
 */

#ifndef __BOOTLOADER_APP_H__
#define __BOOTLOADER_APP_H__

/**
 * @brief A "signature" to identify the bootloader type and help catch
 * any app/bootloader mismatch.
 */
#define BOOTLOADER_TYPE_APP 0xB0FF
// BOOTLOADER_TYPE_STANDALONE is defined in bootloader-interface-standalone.h

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define MICRO_NVM_PAGE_SIZE 256

// This is the working unit of data for the app bootloader.  We want it as
// big as possible, but it must be a factor of the NVM page size and
// fit into a single Zigbee packet.  We choose  2^6 = 64 bytes.
#define BOOTLOADER_SEGMENT_SIZE_LOG2  6 
#define BOOTLOADER_SEGMENT_SIZE       (1 << BOOTLOADER_SEGMENT_SIZE_LOG2)

#endif // DOXYGEN_SHOULD_SKIP_THIS


/** @brief The flash that is used for the application bootloader is
 * divided into program space and download space.  
 *
 * The program space is where the currently executing program resides and is
 *  always internal.  The recovery image is also stored as part of the program
 *  space, however it may actually be stored externally.
 *
 * The download space is where a new image being downloaded is stored while
 *  it is still being received.  It may be internal flash or an external serial
 *  eeprom or flash device.
 *
 * These spaces are then further divided in to segments 
 *  of BOOTLOADER_SEGMENT_SIZE bytes.  These segments are used to specify where
 *  in the different spaces the images that are stored in them start and end.
 *  The precise values of the segments commonly used are defined in the
 *  bootloader.h header file for the platform being used.
 */
#define BOOTLOADER_PROGRAM_SPACE   0

/** @brief The flash that is used for the application bootloader is
 * divided into program space and download space.  
 *
 * The program space is where the currently executing program resides and is
 *  always internal.  The recovery image is also stored as part of the program
 *  space, however it may actually be stored externally.
 *
 * The download space is where a new image being downloaded is stored while
 *  it is still being received.  It may be internal flash or an external serial
 *  eeprom or flash device.
 *
 * These spaces are then further divided in to segments 
 *  of BOOTLOADER_SEGMENT_SIZE bytes.  These segments are used to specify where
 *  in the different spaces the images that are stored in them start and end.
 *  The precise values of the segments commonly used are defined in the
 *  bootloader.h header file for the platform being used.
 */
#define BOOTLOADER_DOWNLOAD_SPACE  1

#ifndef DOXYGEN_SHOULD_SKIP_THIS

extern boolean emWritingRecoveryImage;
#define halEnableRecoveryWrite() emWritingRecoveryImage = TRUE
#define halDisableRecoveryWrite() emWritingRecoveryImage = FALSE

#endif // DOXYGEN_SHOULD_SKIP_THIS


/** @name Bootloader Accessor Functions 
 *@{
 */

/** @brief Enumerations for possible bootloader trap 
 * identifiers.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
enum AppBootloaderTraps
#else
typedef int8u AppBootloaderTraps;
enum
#endif
{
  /** Reserved for trap table signature */
  BOOT_TRAP_RESERVED        = 0,
  /** Changed from deprecated BOOT_TRAP_APP_START */
  BOOT_TRAP_NVM_WRITE       = 1,    
  /** New traps */ 
  BOOT_TRAP_INSTALL_IMAGE   = 2,
};


/** @brief Calls the bootloader trap functions and may be used for
 *  either the stand-alone or application bootloader.  Each trap function
 *  has a wrapper macro in hal/micro/.../.../micro.h.  Use the wrapper
 *  functions under normal circumstances because they improve readability
 *  and automatically check for bootloader type. 
 *  
 * @param assumedBootloaderType  There are different traps for the
 * stand-alone and application bootloaders.  This guards against calling a trap
 * from the wrong bootloader type.
 *
 * @param trapNumber              Trap identifier.
 * 
 * @param ...                     Parameter list corresponding to the trap.
 *  
 * @return  An error if the trap identifier is invalid, or ::EMBER_SUCCESS.
 */
EmberStatus halInternalAppBootloaderTrap(int16u assumedBootloaderType, 
                                         int8u trapNumber, ...);

#ifdef DOXYGEN_SHOULD_SKIP_THIS

/** @brief Writes an image segment to download space memory.  Due
 *  to optimizations in the flash write function, images should be written
 *  as consecutive segments and should usually begin with 
 *  ::BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT.
 *
 *  @param segment  The segment number to be written.  
 *
 *  @param ramSrc   An int8u array containing the segment to be written.
 *  
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_SUCCESS if the segment is written.
 */
EmberStatus  halBootloaderWriteDownloadSpace(int16u segment, int8u *ramSrc);

#else
#define halBootloaderWriteDownloadSpace(segment, ramSrc)                  \
   halInternalAppBootloaderTrap(BOOTLOADER_TYPE_APP, BOOT_TRAP_NVM_WRITE, \
                                (segment), (ramSrc), BOOTLOADER_DOWNLOAD_SPACE)
#endif // DOXYGEN_SHOULD_SKIP_THIS


#ifdef DOXYGEN_SHOULD_SKIP_THIS

/** @brief Writes an image segment to memory, specifying download or
 *  program space.  Due to optimizations in the flash write function, images
 *  should be written as consecutive segments and should usually begin with 
 *  ::BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT or ::BOOTLOADER_DEFAULT_PROGRAM_SEGMENT
 *
 *  @param space    Either ::BOOTLOADER_DOWNLOAD_SPACE or ::BOOTLOADER_PROGRAM_SPACE.
 *
 *  @param segment  The segment number to be written.  
 *  
 *  @param ramSrc   An int8u array containing the segment to be written.
 *
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_SUCCESS if the segment is written.
 */
EmberStatus halBootloaderWriteNvm(int8u space,
                                            int16u segment, 
                                            int8u *ramSrc);
#else
#define halBootloaderWriteNvm(space, segment, ramSrc)                         \
   halInternalAppBootloaderTrap(BOOTLOADER_TYPE_APP, BOOT_TRAP_NVM_WRITE,     \
                                (segment), (ramSrc), (space))
#endif // DOXYGEN_SHOULD_SKIP_THIS


#ifdef DOXYGEN_SHOULD_SKIP_THIS

/** @brief Installs the image which starts at a specified segment
 *  in download space.  Usually, the segment is BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT.
 *
 *  @param downloadSegment  The starting segment of the new image.
 *
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_ERR_BOOTLOADER_NO_IMAGE if no image is found.
 *  ::EMBER_SUCCESS if the image installed successfully.
 */
EmberStatus halBootloaderInstallNewImage(int16u downloadSegment);
#else
#define halBootloaderInstallNewImage(dlSegment)                     \
   halInternalAppBootloaderTrap(BOOTLOADER_TYPE_APP,                \
                                BOOT_TRAP_INSTALL_IMAGE,            \
                                BOOTLOADER_DEFAULT_PROGRAM_SEGMENT, \
                                (dlSegment),                        \
                                BOOTLOADER_DOWNLOAD_SPACE)
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief Reads a bootloader segment from memory.  
 *
 *  @param segment      The segment to be read.
 *  @param dest         Pointer to destination RAM buffer 
 *                      ::BOOTLOADER_SEGMENT_SIZE bytes long.
 *  @param memorySpace  MSP430 ignores this parameter.
 *
 *  @return ::EMBER_SUCCESS or ::EMBER_ERR_FATAL if the segment is out of range.
 */
EmberStatus halInternalReadNvmSegment(int16u segment,
                                      int8u *dest,
                                      int8u memorySpace);

/** @brief Reads an image segment from download space memory.
 *
 *  @param segment  The segment number to be read.
 *
 *  @param ramDest  An int8u array to store the segment.
 *  
 *  @return ::EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD if the trap table is
 *  corrupt or if the application bootloader API is not installed.
 *  ::EMBER_ERR_FATAL if the segment is out of range.
 *  ::EMBER_SUCCESS if read with no error.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
EmberStatus  halBootloaderReadDownloadSpace(int16u segment, int8u *ramDest)
#else
#define halBootloaderReadDownloadSpace(segment, ramDest) \
  halInternalReadNvmSegment((segment), (ramDest), BOOTLOADER_DOWNLOAD_SPACE)
#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @} END addtogroup Bootloader Accessor Functions */


#endif // __BOOTLOADER_APP_H_


/** @}  END addtogroup */

