//=============================================================================
// FILE
//   cstartup-boot.c - Startup and low-level utility code for the Cortex-M3
//                     bootloader with the IAR tool chain.
//
// DESCRIPTION
//   This file defines the basic information needed to go from reset up to
//   the main() found in C code.  cstartup-iar-boot-asm instantiates the
//   actual reset vector since hand crafted assembly code is required to
//   get from the bootloader to the application cstartup when exiting deep
//   sleep.
//
//[[   Author: Brooks Barrett, Dave Iacobone, Lee Taylor  ]]
//   Copyright 2009 by Ember Corporation. All rights reserved.             *80*
//=============================================================================

#include PLATFORM_HEADER
#include "hal/micro/cortexm3/diagnostic.h"
#include "hal/micro/cortexm3/mpu.h"
#include "hal/micro/micro.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/cstartup-iar-common.h"
#include "hal/micro/cortexm3/bootloader/ebl.h"
#include "hal/micro/bootloader-eeprom.h"
#include "hal/micro/cortexm3/bootloader/bootloader-gpio.h"

#include "stack/include/ember-types.h"
#include "hal/micro/bootloader-interface.h"
#include "hal/micro/cortexm3/bootloader/bootloader-configuration.h"

// Pull in the SOFTWARE_VERSION from the stack
#include "stack/config/config.h"

// Define the CUSTOMER_BOOTLOADER_VERSION if it wasn't set
#ifndef CUSTOMER_BOOTLOADER_VERSION
  // If this is an internal build use the Ember version number
  #ifdef JAMBUILD
    #define CUSTOMER_BOOTLOADER_VERSION (0xFFFFFFFF)
  #else
    #warning CUSTOMER_BOOTLOADER_VERSION is not defined. To track bootloader builds set this in bootloader-configuration.h
    #define CUSTOMER_BOOTLOADER_VERSION 0
  #endif
#endif

//=============================================================================
// Define the size of the call stack and define a block of memory for it.
// 
// Place the cstack area in a segment named CSTACK.  This segment is
// defined soley for the purpose of placing the stack.  Since this area
// is manually placed, it cannot be part of the normal data initialization
// and therefore must be declared '__no_init'.  Refer to reset handler for the
//initialization code and iar-cfg-common.icf for segment placement in memory.
// 
// halResetInfo, used to store crash information and bootloader parameters, is
// overlayed on top of the base of this segment so it can be overwritten by the 
// call stack.
// This assumes that the stack will not go that deep between reset and
// use of the crash or the bootloader data.
//=============================================================================
#ifndef CSTACK_SIZE
  #define CSTACK_SIZE  (256)  // *4 = 1024 bytes
#endif
__root __no_init int32u cstackMemory[CSTACK_SIZE] @ __CSTACK__;

// Reset cause and bootloader parameters live in a special RAM segment that is
// not modified during startup.  This segment is overlayed on top of the
// bottom of the cstack.
__root __no_init HalResetInfoType halResetInfo @ __RESETINFO__;

//=============================================================================
// The bootloader doesn't use interrupts, so only a very simple fault handler
//  is implemented
//=============================================================================
void halNmiHardFaultIsr(void)
{
  halInternalSysReset(RESET_BOOTLOADER_FATAL);
}

//=============================================================================
// Utility function used when setting up to jump to installed application
//=============================================================================
//static void setStackPointer(int32u address)
void setStackPointer(int32u address)
{
  asm("MOVS SP, r0");
}

//=============================================================================
// Simplified reset cause detection apis
//=============================================================================
static int16u savedResetCause;

int16u halGetExtendedResetInfo(void)
{
  return savedResetCause;
}

//=============================================================================
// Simplified assert mechanism  (Not actually used, but kept for posterity)
//=============================================================================
void halInternalAssertFailed(PGM_P filename, int linenumber)
{
  halResetInfo.crash.data.assertInfo.file = (const char *)halResetInfo.crash.R0;
  halResetInfo.crash.data.assertInfo.line = halResetInfo.crash.R1;
  halInternalSysReset(RESET_BOOTLOADER_FATAL);
}


//=============================================================================
// Check to see if we should jump to the app's halEntryPoint.  If not, when
// this function returns the bootloader halEntryPoint will then invoke
// bootloaderEntryPoint.
//=============================================================================
void checkAppJumpToApp(void)
{
  boolean softwareReset;
  // Determine if we are going to run the bootloader, or if we're
  // going to jump right to the application
  // All of this reset type detection logic is done inline rather than calling
  // helper functions to keep stack usage to the absolute minimum.
  // Cannot use any globals here, as they are not yet initialized.
  if(bootloadForceActivation()) {
    // override the reset type with a FORCED reset cause and return
    //  so that the bootloader is activated
    halResetInfo.crash.resetReason = RESET_BOOTLOADER_FORCE;
    // also need to mark the signature as valid because we may be resetting
    //  due to something other than a software reset type
    halResetInfo.crash.resetSignature = RESET_VALID_SIGNATURE;
    return;
  }
  
  softwareReset = ((RESET_EVENT & RESET_SW_MASK) == RESET_SW_MASK);
  if( (!softwareReset) ||
      ( ( (softwareReset) && 
          (halResetInfo.crash.resetSignature == RESET_VALID_SIGNATURE) 
        ) &&
        ( (halResetInfo.crash.resetReason != RESET_BOOTLOADER_BOOTLOAD) &&
          (halResetInfo.crash.resetReason != RESET_BOOTLOADER_OTAVALID) &&
          (halResetInfo.crash.resetReason != RESET_FIB_BOOTLOADER) &&
          (halResetInfo.crash.resetReason != RESET_FIB_BAUDRATE) 
        )
      )
    ) {     
    // That logic said...
    //  If the bootloadForceActivation() API is false and 
    //  ( if its _not_ a software reset, then we get in here and jump to the app, 
    //    or if it _is_ a valid sofware reset event, but its not one
    //    of the reset types that would activate the bootloader,
    //    then we also get in here and jump to the app.. )
    
    // Now, we also must test to make sure that we have a valid application
    //  image, or else we will fall out and run the bootloader
    //TODO -- Should this address table check should be beefed up before 
    //         declaring the app is valid and jumping to it?
    if ( (halAppAddressTable.baseTable.type) == APP_ADDRESS_TABLE_TYPE ) {
      // relocate the stack pointer
      setStackPointer((int32u)halAppAddressTable.baseTable.topOfStack);

      // Jump to the application's reset vector to start it.
      //
      // Notes about the NO_OPERATION() after the jump to app:
      //
      //   If this jump to the app's resetVector() is the last code in this
      //   function, IAR 6.40.2 with full optimization generates a POP
      //   instruction just before the branch instruction to the app's starting
      //   address, probably as part of a tail chaining optimization.
      //
      //   This POP changes the app's stack pointer to be offset from where it
      //   should be: This was the cause of FogBugz 14918, which affected
      //   bootloaders in EmberZNet 4.7.0 and 4.7.1.
      //
      //   To prevent this from happening we insert a NO_OPERATION() after the
      //   function call. This inserts a single NOP instruction.
      //
      //   As a funny side effect, adding the NO_OPERATION() also makes this
      //   function smaller.
      //
      //   We can't merely disable all optimizations because the code gets too
      //   big.
      halAppAddressTable.baseTable.resetVector();

      NO_OPERATION(); // required to prevent stack pointer corruption
    }
  }
  // If we get here, then reset type checks indicate we should run the bootloader
  //  either it was a software reset saying that the bootloader should run
  //  or no valid installed image was found.
}


//=============================================================================
// The bootloader's equivalent of halEntryPoint.  This must occur after the
// lower level assembly code has verified we are not coming out of deep sleep.
//=============================================================================
void bootloaderEntryPoint(void)
{
  int8u *dataSourceStart;
  int8u *dataDestinationStart;
  int8u *dataDestinationEnd;
  boolean softwareReset;
  
  #ifdef CORTEXM3_EM350
    //[[
    #ifndef EMBER_EMU_TEST
    //WBB350FIXME -- FogBugz 9472 "32bit store followed by 8bit store results
    //               in upper 16bits of 32bit store being zero"
    //               The most effective short term workaround for this bug is
    //               to always run the CPU in the high speed mode (clk24m) since
    //               this spaces out the cycles on the AHB because the RAM
    //               always needs wait states in this mode. This has been
    //               verified on simulation and on chip.
    //]]
    CPU_CLKSEL = CPU_CLKSEL_FIELD;
    //[[
    #endif //EMBER_EMU_TEST
    //]]
  #endif //CORTEXM3_EM350
  
  //When the Cortex-M3 exits reset, interrupts are enable.  Explicitely
  //disable them immediately using the standard set PRIMASK instruction.
  //Injecting an assembly instruction this early does not effect optimization.
  asm("CPSID i");
  
  //It is quite possible that when the Cortex-M3 begins executing code the
  //Core Reset Vector Catch is still left enabled.  Because this VC would
  //cause us to halt at reset if another reset event tripped, we should
  //clear it as soon as possible.  If a debugger wants to halt at reset,
  //it will set this bit again.
  DEBUG_EMCR &= ~DEBUG_EMCR_VC_CORERESET;
  
  //Configure flash access for optimal current consumption early
  //during boot to save as much current as we can.
  halInternalConfigFlashAccess();
  
  softwareReset = ((RESET_EVENT & RESET_SW_MASK) == RESET_SW_MASK);

  //zero out the area of memory reserved for the stack
  if(softwareReset) {
    // if it was a software reset, make sure we start the initialization
    //  _after_ the overlayed reset info
    dataDestinationStart = __segment_end(__RESETINFO__);
  } else {
    // for any other reset type, the entire cstack can be initialized
    dataDestinationStart = __segment_begin(__CSTACK__);
  }
  dataDestinationEnd = __segment_end(__CSTACK__);
  while(dataDestinationStart < dataDestinationEnd) {
    //Fill with magic value interpreted by C-SPY's Stack View
    *dataDestinationStart++ = (int8u)STACK_FILL_VALUE;
  }
  //copy data initializers for non-zero static and globals from Flash to RAM.
  dataSourceStart = __segment_begin(__DATA_INIT__);
  dataDestinationStart = __segment_begin(__DATA__);
  dataDestinationEnd = __segment_end(__DATA__);
  while(dataDestinationStart < dataDestinationEnd) {
    *dataDestinationStart++ = *dataSourceStart++;
  }
  
  //zero out the static and globals in RAM that are zero-initialized.
  dataDestinationStart = __segment_begin(__BSS__);
  dataDestinationEnd = __segment_end(__BSS__);
  while(dataDestinationStart < dataDestinationEnd) {
    *dataDestinationStart++ = 0;
  }

  // Do a very simplified reset classification that really only cares 
  //  about valid bootloader and fib software reset types.  All others get 
  //  wacked to an unknown classification
  // If we have a valid force reset type regardless of the type of
  //  hardware reset, or if the hardware indicates a software reset and we have
  //  a valid FIB or BOOTLOADER reset type.
  // In the case of the FORCE reset type, it is ok to not check for the hardware
  //  indicating that there was a software reset at this stage, because that was
  //  verified above in checkAppJumpToApp().  The only reason we should get to
  //  this point in the code with this reset type is if bootloadForceActivation()
  //  actually returned true, which causes checkAppJumpToApp() override the
  //  reset type with RESET_BOOTLOADER_FORCE.
  if( (halResetInfo.crash.resetSignature == RESET_VALID_SIGNATURE) &&
      ( (halResetInfo.crash.resetReason == RESET_BOOTLOADER_FORCE) ||
        ( (softwareReset) &&
        ( (RESET_BASE_TYPE(halResetInfo.crash.resetReason) == RESET_FIB) ||
          (RESET_BASE_TYPE(halResetInfo.crash.resetReason) == RESET_BOOTLOADER) ) 
        ) 
      ) 
    ) {
    savedResetCause = halResetInfo.crash.resetReason;
  } else {
    savedResetCause = RESET_UNKNOWN_UNKNOWN;
  }
  // mark the signature as invalid 
  halResetInfo.crash.resetSignature = RESET_INVALID_SIGNATURE;

  main();         //branch to the real C code main()!
  
  // if main ever returns...
  halInternalSysReset(RESET_BOOTLOADER_FATAL);
}


//=============================================================================
// Declare the address tables which will always live at well known addresses
//=============================================================================
__root __no_init const HalFixedAddressTableType halFixedAddressTable @ __FAT__; 
__root __no_init const HalAppAddressTableType halAppAddressTable @ __AAT__;

extern EblDataFuncType eblDataFuncs;

//=============================================================================
// The bootloader address table includes all the important information 
//  about the installed bootloader
//=============================================================================
__root const HalBootloaderAddressTableType halBootloaderAddressTable @ __BAT__ = {
  { __section_end("CSTACK"),
    halEntryPoint,
    halNmiHardFaultIsr,
    halNmiHardFaultIsr,
    BOOTLOADER_ADDRESS_TABLE_TYPE,
    BAT_VERSION,
    NULL                    // No other vector table.
  },
  BOOTLOADER_TYPE,  //int16u bootloaderType;
  SOFTWARE_VERSION, //int16u bootloaderVersion;  
  &halAppAddressTable,
  PLAT,  //int8u platInfo;   // type of platform, defined in micro.h
  MICRO, //int8u microInfo;  // type of micro, defined in micro.h
  PHY,   //int8u phyInfo;    // type of phy, defined in micro.h
  0,     //int8u reserved;   // reserved for future use  
  eblProcessInit,
  eblProcess,
  &eblDataFuncs,
  #if (((BOOTLOADER_TYPE)>>8) == BL_TYPE_APPLICATION)
    halEepromInit,
    halEepromRead,
    halEepromWrite,
    halEepromShutdown,
    (const void *(*)(void))halEepromInfo,
    halEepromErase,
    halEepromBusy,
  #else
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
  #endif
  EMBER_BUILD_NUMBER, // int16u softwareBuild;
  0,                  // int16u reserved2;
  CUSTOMER_BOOTLOADER_VERSION  // int32u customerBootloaderVersion;
};
