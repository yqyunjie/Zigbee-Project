/*
 * File: bootloader-interface-standalone.c
 * Description: AVR Atmega128-specific standalone bootloader HAL functions
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

// The following must match X_TRAPTABPSTART in boot-cfg.xcl
#ifdef AVR_ATMEGA_64
#define BOOTLOADER_TRAP_TABLE_PTR                 0xDFC0
#else
#define BOOTLOADER_TRAP_TABLE_PTR                 0xFFC0
#endif

// reserved ram is special, see reserved-ram.h
__no_init __root int16u bootloaderApplicationRequestKey @ HAL_RESERVED_RAM_BOOTLOADER_KEY;
__no_init __root int8u  gatewayID[8]   @ HAL_RESERVED_RAM_GATEWAY_ID;
__no_init __root int8u  eui64[8]       @ HAL_RESERVED_RAM_EUI64_ID;
__no_init __root int8u  radioPower     @ HAL_RESERVED_RAM_POWER;
__no_init __root int8u  radioBands     @ HAL_RESERVED_RAM_BANDS;
__no_init __root int8u  radioChannel   @ HAL_RESERVED_RAM_CHANNEL;
__no_init __root int8u  bootloaderMode @ HAL_RESERVED_RAM_MODE;

EmberStatus halInternalStandaloneBootloaderTrap(int16u assumedBootloaderType,
                                                int8u trapNumber, ...)
{
  int16u actualBootloaderType;
  int16u funcAddr;

  // Check Bootloader Type
  actualBootloaderType = halInternalReadFlashWord((PGM_PU)BOOTLOADER_TRAP_TABLE_PTR);
  if (assumedBootloaderType != actualBootloaderType )
    return EMBER_ERR_BOOTLOADER_TRAP_TABLE_BAD;

  funcAddr = halInternalReadFlashWord((PGM_PU)(BOOTLOADER_TRAP_TABLE_PTR +
                                         (trapNumber<<1)));

  switch (trapNumber) {
  case BOOT_TRAP_APP_START: {
    void (*bootloadAppStart)(void) = (void(*)(void))funcAddr;
    bootloadAppStart();
    break; }
  case BOOT_TRAP_FLASH_ADJUST: {
    va_list args;
    int16u* dst;
    int16u* src;
    int8u size;
    void (*halInternalAdjustFlashBlock)(int16u* dst, int16u* src, int8u size) =
      (void(*)(int16u* dst, int16u* src, int8u size))funcAddr;
    va_start(args, trapNumber);
    dst = va_arg(args, int16u*);
    src = va_arg(args, int16u*);
    size = va_arg(args, int8u);
    va_end(args);
    halInternalAdjustFlashBlock(dst, src, size);
    break; }
  default:
    return EMBER_ERR_BOOTLOADER_TRAP_UNKNOWN;
  }
  return EMBER_SUCCESS;
}

static void loadTokensToReservedRAM(void)
{
  tokTypeMfgEui64 eui64Tok;
  tokTypeMfgRadioBandsSupported bands;
  halCommonGetToken(&eui64Tok, TOKEN_MFG_EUI_64);
  MEMCOPY(eui64, eui64Tok, sizeof(eui64));
  radioPower = emberGetRadioPower();
  halCommonGetToken(&bands, TOKEN_MFG_RADIO_BANDS_SUPPORTED);
  radioBands = bands;

  // Can't rely on the tokens because as of flare branch, the radioChannel
  // token isn't updated in real time.
  radioChannel = emberGetRadioChannel() - 11;
}

EmberStatus halLaunchStandaloneBootloaderV1(int8u mode, int8u *eui64)
{
  int8u i;

  if (halInternalReadFlashWord((PGM_PU)BOOTLOADER_TRAP_TABLE_PTR)
          == BOOTLOADER_TYPE_STANDALONE) {

    INTERRUPTS_OFF();
    halInternalDisableWatchDog(MICRO_DISABLE_WATCH_DOG_KEY);
    // using this technique to get into the bootloader, means that
    // it's segment initialization has occurred correctly.
    bootloaderApplicationRequestKey = BOOTLOADER_APP_REQUEST_KEY;

    loadTokensToReservedRAM();

    for(i=0; i<8; i++) {
      if(eui64 == NULL) {
        gatewayID[i] = i;
      } else {
        gatewayID[i] = eui64[i];
      }
    }

    bootloaderMode = mode;

  #if __HAS_ELPM__
    asm("jmp 0x1E000"); // jump to bootloader-reset-vec
  #else
    asm("jmp 0xe000");
  #endif
    // If successful, this function never returns:
    // we just placate the compiler.
    return EMBER_SUCCESS;
  }

  return EMBER_ERR_FATAL;
}
