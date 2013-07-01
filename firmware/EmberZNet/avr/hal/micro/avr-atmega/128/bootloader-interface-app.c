/*
 * File: bootloader-interface-app.c
 * Description: AVR Atmega128-specific application bootloader HAL functions
 *
 * Author(s): Lee Taylor, lee@ember.com,
 *            Jeff Mathews, jm@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>

#include <stdarg.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "include/error.h"

#include "hal/hal.h"
#include "app/util/serial/serial.h"

// We need different limits for halBootloaderWriteDownloadSpace() when
// writing normal and recovery images.
boolean emWritingRecoveryImage = FALSE;

static void bootloaderReadSegment(PGM_PU nvmSegmentBaseAddr,
                                  int8u *ramDest)
{
  int8u byteCounter; // Segments are only 64 bytes long.

  for (byteCounter = 0; byteCounter <  BOOTLOADER_SEGMENT_SIZE; byteCounter++, ramDest++, nvmSegmentBaseAddr++) {
    *ramDest = halInternalReadFlashByte(nvmSegmentBaseAddr);
  }

}

EmberStatus halInternalReadNvmSegment(int16u segment,
                                int8u *dest,
                                int8u memorySpace)
{
  PGM_PU nvmAddress;

  // The address boundry between Application Space (in the lower half of
  // memory) and the Download Space (in the upper half of memory) is 0x10000.
  // At this point, instead of addresses we are still working with segment
  // numbers.  A segment is a 64 byte chunk of data and a segment number
  // must be an offset within one of the Spaces.  The memorySpace parameter
  // is used to select which Space the segment offsets inside of.
  // Dividing the boundry address by the segment size, yields 0x400 as the
  // boundry in terms of segments.  Since the segment number can never cross
  // the boundry, we error check the segment number here.
  if (segment >= 0x400)
    return EMBER_ERR_FATAL;

  // We *cannot* add 0x10000 to the pointer below because
  // of IAR compiler weirdness.
  if (memorySpace == BOOTLOADER_DOWNLOAD_SPACE)
    segment += 0x400;

#pragma diag_suppress=Pe1053 //Suppress warning -IAR says this is only solution
  nvmAddress = (PGM_PU)(((int32u)segment)
                        << BOOTLOADER_SEGMENT_SIZE_LOG2);
#pragma diag_default=Pe1053 //Suppress warning -IAR says this is only solution

  bootloaderReadSegment(nvmAddress, dest);

  return EMBER_SUCCESS;
}

EmberStatus halInternalAppBootloaderTrap(int16u assumedBootloaderType,
                                          int8u trapNumber, ...)
{
  int16u actualBootloaderType;
  int16u funcAddr;

// Check Bootloader Type
#pragma diag_suppress=Pe1053 //Suppress warning -IAR says this is only solution
  actualBootloaderType = halInternalReadFlashWord((PGM_PU)APP_TRAPTABLE_POINTER);
#pragma diag_default=Pe1053 //Suppress warning -IAR says this is only solution
  if (assumedBootloaderType != actualBootloaderType )
    return EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD;

#pragma diag_suppress=Pe1053 //Suppress warning -IAR says this is only solution
  funcAddr = halInternalReadFlashWord((PGM_PU)(APP_TRAPTABLE_POINTER +
                                         (trapNumber<<1)));
#pragma diag_default=Pe1053 //Suppress warning -IAR says this is only solution
  {
    EmberStatus status;
    DECLARE_INTERRUPT_STATE;

    DISABLE_INTERRUPTS();
    switch(trapNumber) {
    case BOOT_TRAP_NVM_WRITE: {
      va_list args;
      int8u space;
      int16u segment;
      int8u* ramSrc;
      int8u (*writeNvmSegment)(int16u segment, int8u* ramSrc, int8u memSpace)
        = (int8u(*)(int16u segment, int8u* ramSrc, int8u memSpace))funcAddr;
      va_start(args, trapNumber);
      segment = va_arg(args, int16u);
      ramSrc = va_arg(args, int8u*);
      space = va_arg(args, int8u);
      va_end(args);
      if ((segment < BOOTLOADER_RECOVERY_IMAGE_SEGMENT)
          || emWritingRecoveryImage) {
        status = writeNvmSegment(segment, ramSrc, space);
      } else
        status = EMBER_ERR_FATAL;
      break;
    }
    case BOOT_TRAP_INSTALL_IMAGE: {
      va_list args;
      int16u programSegment, downloadSegment;
      int8u imageSpace;
      int8u (*installNewImage)(int16u programSegment, int16u downloadSegment, int8u memorySpace)
        = (int8u(*)(int16u programSegment, int16u downloadSegment, int8u memorySpace))funcAddr;
      va_start(args, trapNumber);
      programSegment = va_arg(args, int16u);
      downloadSegment = va_arg(args, int16u);
      imageSpace = va_arg(args, int8u);
      va_end(args);
      status = installNewImage(programSegment, downloadSegment, imageSpace);
      break;
    }
    default:
      status = EMBER_ERR_BOOTLOADER_TRAP_UNKNOWN;
    }
    RESTORE_INTERRUPTS();
    return status;
  }
}
