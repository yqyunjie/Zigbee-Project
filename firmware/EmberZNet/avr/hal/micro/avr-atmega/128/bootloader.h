/** @file hal/micro/avr-atmega/128/bootloader.h
 * 
 * @brief Micro-specific functions used in conjunction with either bootloader.
 *
 * Unless otherwise noted, the EmberNet stack does not use these functions, 
 * and therefore the HAL is not required to implement them. However, many of
 * the supplied example applications do use them.
 *
 * Some functions in this file return an EmberStatus value. See 
 * error-def.h for definitions of all EmberStatus return values.
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Definitions relevant to the application bootloader.
// Defines the starting segment offset for the download segment 
//  within the program space.
#define BOOTLOADER_DEFAULT_DOWNLOAD_SEGMENT      0
// Defines the starting segment offset for the currently executing program
//  within the program space.
#define BOOTLOADER_DEFAULT_PROGRAM_SEGMENT       4
// Defines the starting segment offset for the recovery image
//  within the program space.
#define BOOTLOADER_RECOVERY_IMAGE_SEGMENT        0x03E0


// This points to where the trap table is stored in the internal flash
#define APP_TRAPTABLE_POINTER                    0x1FFC0

// Definition for the recovery image (a bit not used by UCSR1A for errors).
#define NO_SERIAL_DATA 0x80

/** @name Flash Accessor Macros and Functions
 * \b Note: All int16u values are little-endian.
 */
//@{

#if __HAS_ELPM__
  #define halInternalReadFlashByte(addr)   __extended_load_program_memory(addr)
  #define halInternalReadFlashWord(addr)   HIGH_LOW_TO_INT(                    \
                                      __extended_load_program_memory((addr)+1),\
                                      __extended_load_program_memory(addr) )
#else // __HAS_ELPM__

/** @description Reads a byte from flash.
 * 
 * @param addr: A 16-bit address in program space.
 *  
 * @return An int8u value.
 */
  #define halInternalReadFlashByte(addr)   __load_program_memory(addr)

/** @description Reads a word from flash.
 * 
 * @param addr: A 16-bit address in program space.
 * 
 * @return An int16u value.
 */
#define halInternalReadFlashWord(addr)   HIGH_LOW_TO_INT(              \
                                      __load_program_memory((addr)+1), \
                                      __load_program_memory(addr) )
#endif  // __HAS_ELPM__

//@}  // end of Flash Accessor Macros, Functions, and Constants

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif // __BOOTLOADER_H_
