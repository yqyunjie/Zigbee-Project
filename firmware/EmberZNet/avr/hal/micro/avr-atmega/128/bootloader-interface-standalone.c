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


// FIXME
// The following must match X_TRAPTABPSTART in boot-cfg.xcl
#define BOOTLOADER_TRAP_TABLE_PTR                 0x00F0  

// reserved ram is special, see reserved-ram.h
__no_init __root int16u bootloaderApplicationRequestKey @ HAL_RESERVED_RAM_BOOTLOADER_KEY;
__no_init __root int8u  gatewayID[8]   @ HAL_RESERVED_RAM_GATEWAY_ID;
__no_init __root int8u  eui64[8]       @ HAL_RESERVED_RAM_EUI64_ID;
__no_init __root int16u panID          @ HAL_RESERVED_RAM_PAN_ID;
__no_init __root int16u nodeID         @ HAL_RESERVED_RAM_NODE_ID;
__no_init __root int8u  radioPower     @ HAL_RESERVED_RAM_POWER;
__no_init __root int8u  radioBands     @ HAL_RESERVED_RAM_BANDS;
__no_init __root int8u  radioChannel   @ HAL_RESERVED_RAM_CHANNEL;
__no_init __root int8u  bootloaderMode @ HAL_RESERVED_RAM_MODE;


// Fetch stack versions

int16u halGetStandaloneBootloaderVersion(void)
{
  int16u actualBootloaderType;
  union {       // This is needed so that locked flash gives the same answer!
    int16u flashData[3];
    struct {
      int16u erasedData;
      int16u newVersion;
      int16u stackVersion;
    } n;
  } f;
  int32u dataBaddr = 0x1FFFA;
  int8u i;

  // FIXME
#pragma diag_suppress = pe1053
  actualBootloaderType = halInternalReadFlashWord((PGM_PU)BOOTLOADER_TRAP_TABLE_PTR);
  for(i=0; i<3; i++)
  {
    f.flashData[i] = halInternalReadFlashWord((PGM_PU)dataBaddr);
    dataBaddr += 2;
  }
#pragma diag_default = pe1053

#if defined(HALTEST) && defined(DCS1_DEBUG)
  emberSerialPrintf(serialPort,"newVersion=%2x, stackVersion=%2x\r\n",
                    f.n.newVersion, f.n.stackVersion);
  emberSerialWaitSend(serialPort);
  emberSerialPrintf(serialPort,"actualBootloaderType=%2x, erasedData=%2x\r\n",
                    actualBootloaderType, f.n.erasedData);
  emberSerialWaitSend(serialPort);
#endif

  if ((f.n.erasedData == f.n.newVersion) && 
      (f.n.erasedData == f.n.stackVersion))
  {
    if (actualBootloaderType == 0xBEEF)
      return 0x1000;  // flash is locked.  Best guess
    else
      return 0x2000;  // flash is locked.  Best guess
  } else if (f.n.newVersion != 0xFFFF)
    return f.n.newVersion;
  return f.n.stackVersion;
}

EmberStatus halInternalFlashBlockRead( int16u addr, int16u *data, int16u length )
{
#pragma diag_suppress = pe1053
  PGM_PU paddr = (PGM_PU)(((int32u)addr) << 1);
#pragma diag_default = pe1053

  while ( length-- > 0 )
  {
    *data++ = halInternalReadFlashWord(paddr); // translate and read
    paddr += 2;                                // And forward 2 bytes.
  }
  return EMBER_SUCCESS;
}

#ifdef FIXME
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
    int16u address;
    int16u* dst;
    int16u* src;
    int8u size;
    int8u page;

    void (*flashAdjustBlock)(int16u* dst, int16u* src, int8u size, int8u page) = 
      (void(*)(int16u* dst, int16u* src, int8u size, int8u page))funcAddr;

/*
      emberSerialPrintf(SER232,"vargs\r\n");
      emberSerialWaitSend(SER232);
*/
    va_start(args, trapNumber);
    address = va_arg(args, int16u);
    if(address & 0x8000) {
      page = 1;
    } else {
      page = 0;
    }
    dst = (int16u*) (address << 1);
    src = va_arg(args, int16u*);
    size = va_arg(args, int8u);
    va_end(args);
/*
      emberSerialPrintf(SER232,"dst:%2x,src:%2x,size:%d,page:%d",dst,src,size,page);
      emberSerialWaitSend(SER232);
*/
    ATOMIC(
      flashAdjustBlock(dst, src, size, page);
      )

      break; }
  default:
    return EMBER_ERR_BOOTLOADER_TRAP_UNKNOWN;
  }
  return EMBER_SUCCESS;
}
#endif

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
  panID = emberGetPanId();
  nodeID = emberGetNodeId();
}

EmberStatus halLaunchStandaloneBootloaderV1(int8u mode, int8u *eui64)
{
  int8u i;

#if defined(HALTEST) && defined(DCS1_DEBUG)
  emberSerialPrintf(serialPort,"halLaunchStandaloneBootloaderV1()\r\n");
  emberSerialWaitSend(serialPort);
  emberSerialPrintf(serialPort,"radioChannel=%x emberGetRadioChannel()=%x\r\n",
                    radioChannel, emberGetRadioChannel());
  emberSerialWaitSend(serialPort);
  loadTokensToReservedRAM();
  emberSerialPrintf(serialPort,"radioChannel=%x emberGetRadioChannel()=%x\r\n",
                    radioChannel, emberGetRadioChannel());
  emberSerialWaitSend(serialPort);
#endif

#pragma diag_suppress = pe1053
  if ( (halInternalReadFlashWord((PGM_PU)0x1FFFC) != 0xFFFF) || 
       (halInternalReadFlashWord((PGM_PU)BOOTLOADER_TRAP_TABLE_PTR)
            == BOOTLOADER_TYPE_STANDALONE) ) {
#pragma diag_default = pe1053

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

#ifndef FIXME
    halReboot();
#else
  #if __HAS_ELPM__
    asm("jmp 0x1E000"); // jump to bootloader-reset-vec
  #else
    asm("jmp 0xe000");
  #endif
#endif
    // If successful, this function never returns:
    // we just placate the compiler.
    return EMBER_SUCCESS;
  }

  return EMBER_ERR_FATAL;
}
