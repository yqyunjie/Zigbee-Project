//=============================================================================
// FILE
//   cstartup.c - Startup and low-level utility code for Ember's Cortex based
//                SOCs when using the IAR toolchain.
//
// DESCRIPTION
//   This file defines the basic information needed to go from reset up to
//   the main() found in C code.
//
//[[   Author: Brooks Barrett, Lee Taylor  ]]
//   Copyright 2009 by Ember Corporation. All rights reserved.             *80*
//=============================================================================

#include PLATFORM_HEADER
#include "hal/micro/cortexm3/diagnostic.h"
#include "hal/micro/cortexm3/mpu.h"
#include "hal/micro/micro.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/cstartup-iar-common.h"
#include "hal/micro/cortexm3/internal-storage.h"

#include "stack/include/ember-types.h"
#include "hal/micro/bootloader-interface.h"

#ifdef EMBER_STACK_IP
  #define SOFTWARE_VERSION 0
  #define EMBER_BUILD_NUMBER 0
#else
  // Pull in the SOFTWARE_VERSION from the stack
  #include "stack/config/config.h"
#endif

// Define the CUSTOMER_APPLICATION_VERSION if it wasn't set
#ifndef CUSTOMER_APPLICATION_VERSION
  #define CUSTOMER_APPLICATION_VERSION 0
#endif
// Define the CUSTOMER_BOOTLOADER_VERSION if it wasn't set
#ifndef CUSTOMER_BOOTLOADER_VERSION
  #define CUSTOMER_BOOTLOADER_VERSION 0
#endif

// Verify the various bootloader options that may be specified.  Use of some
//   options is now deprecated and will be removed in a future release.
// On the 35x platform, the use of these options is only important to specify
//   the size of the bootloader, rather than the bootloader type.
// By default, the lack of any option will indicate an 8k bootloader size
// The NULL_BTL option indicates no bootloader is used.
#ifdef APP_BTL
  #pragma message("The APP_BTL build option is deprecated.  Removing this option will build for any 8k bootloader type.")
#endif
#ifdef SERIAL_UART_BTL
  #pragma message("The SERIAL_UART_BTL build option is deprecated.  Removing this option will build for any 8k bootloader type.")
#endif
#ifdef SERIAL_OTA_BTL
  #pragma message("The SERIAL_UART_OTA build option is deprecated.  Removing this option will build for any 8k bootloader type.")
#endif
#ifdef NULL_BTL
  // Fully supported, no error
#endif
#ifdef SMALL_BTL
  #error SMALL_BTL is not supported
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
  #if (! defined(EMBER_STACK_IP))
    // Pro Stack

    // Right now we define the stack size to be for the worst case scenario,
    // ECC.  The ECC library uses stack for its calculations.  Empirically I have
    // seen it use as much as 1504 bytes for the 'key bit generate' operation.
    // So we add a 25% buffer: 1504 * 1.25 = 1880
    // Later we may want to conditionally change the stack based on whether
    // or not the customer is building a Smart Energy Application.
    #define CSTACK_SIZE  (470)  // *4 = 1880 bytes

  #else
    // IP Stack
    #define CSTACK_SIZE (950) // *4 = 3800 bytes
  #endif // !EMBER_STACK_IP
#endif
__root __no_init int32u cstackMemory[CSTACK_SIZE] @ __CSTACK__;

#ifndef HTOL_EM3XX
  // Create an array to hold space for the guard region. Do not actually use this
  // array in code as we will move the guard region around programatically. This
  // is only here so that the linker takes into account the size of the guard
  // region when configuring the RAM.
  #pragma data_alignment=HEAP_GUARD_REGION_SIZE_BYTES
  __root __no_init int8u guardRegionPlaceHolder[HEAP_GUARD_REGION_SIZE_BYTES] @ __GUARD_REGION__;
#endif

// Reset cause and crash info live in a special RAM segment that is
// not modified during startup.  This segment is overlayed on top of the
// bottom of the cstack.
__root __no_init HalResetInfoType halResetInfo @ __RESETINFO__;

// If space is needed in the flash for data storage like for the local storage
// bootloader then create an array here to hold a place for this data.
#if INTERNAL_STORAGE_SIZE_B > 0
  // Define the storage region as an uninitialized array in the
  // __INTERNAL_STORAGE__ region which the linker knows how to place.
  VAR_AT_SEGMENT(__root __no_init int8u internalStorage[INTERNAL_STORAGE_SIZE_B], __INTERNAL_STORAGE__);
#endif

//=============================================================================
// Declare the address tables which will always live at well known addresses
//=============================================================================
__root __no_init const HalFixedAddressTableType halFixedAddressTable @ __FAT__; 

#ifdef NULL_BTL
// In the case of a NULL_BTL application, we define a dummy BAT
__root const HalBootloaderAddressTableType halBootloaderAddressTable @ __BAT__ = {
  { __section_end("CSTACK"),
    halEntryPoint,
    halNmiIsr,
    halHardFaultIsr,
    BOOTLOADER_ADDRESS_TABLE_TYPE,
    BAT_NULL_VERSION,
    NULL                    // No other vector table.
  },
  BL_EXT_TYPE_NULL,           //int16u bootloaderType;
  BOOTLOADER_INVALID_VERSION, //int16u bootloaderVersion;  
  &halAppAddressTable,
  PLAT,   //int8u platInfo;   // type of platform, defined in micro.h
  MICRO,  //int8u microInfo;  // type of micro, defined in micro.h
  PHY,    //int8u phyInfo;    // type of phy, defined in micro.h
  0,      //int8u reserved;   // reserved for future use  
  NULL,   // eblProcessInit
  NULL,   // eblProcess
  NULL,   // eblDataFuncs
  NULL,   // eepromInit    
  NULL,   // eepromRead    
  NULL,   // eepromWrite   
  NULL,   // eepromShutdown
  NULL,   // eepromInfo    
  NULL,   // eepromErase   
  NULL,   // eepromBusy    
  EMBER_BUILD_NUMBER, // int16u softwareBuild;
  0,                  // int16u reserved2;
  CUSTOMER_BOOTLOADER_VERSION  // int32u customerBootloaderVersion;
};
#else
// otherwise we just define a variable that maps to the real bootloader BAT
__root __no_init const HalBootloaderAddressTableType halBootloaderAddressTable @ __BAT__;
#endif

__root const HalAppAddressTableType halAppAddressTable @ __AAT__ = {
  { __section_end("CSTACK"),
    halEntryPoint,
    halNmiIsr,
    halHardFaultIsr,
    APP_ADDRESS_TABLE_TYPE,
    AAT_VERSION,
    __vector_table
  },
  PLAT,  //int8u platInfo;   // type of platform, defined in micro.h
  MICRO, //int8u microInfo;  // type of micro, defined in micro.h
  PHY,   //int8u phyInfo;    // type of phy, defined in micro.h
  sizeof(HalAppAddressTableType),  // size of aat itself
  SOFTWARE_VERSION,   // int16u softwareVersion
  EMBER_BUILD_NUMBER, // int16u softwareBuild
  0,  //int32u timestamp; // Unix epoch time of .ebl file, filled in by ebl gen
  "", //int8u imageInfo[IMAGE_INFO_SZ];  // string, filled in by ebl generation
  0,  //int32u imageCrc;  // CRC over following pageRanges, filled in by ebl gen
  { {0xFF, 0xFF},   //pageRange_t pageRanges[6];  // Flash pages used by app, 
    {0xFF, 0xFF},                                 // filled in by ebl gen
    {0xFF, 0xFF}, 
    {0xFF, 0xFF}, 
    {0xFF, 0xFF}, 
    {0xFF, 0xFF} 
  }, 
  __section_begin(__SIMEE__),                         //void *simeeBottom;
  CUSTOMER_APPLICATION_VERSION,                       //int32u customerApplicationVersion;
  __section_begin(__INTERNAL_STORAGE__),              //void *internalStorageBottom;
  {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,    // bootloaderReserved
   0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
  __section_begin(__DEBUG_CHANNEL__),   //void *debugChannelBottom;
  __section_begin(__NO_INIT__),         //void *noInitBottom;
  __section_end(__BSS__),               //void *appRamTop; NO LONGER USED! (set to __BSS__ for 3xx convert)
  __section_end(__BSS__),               //void *globalTop;
  __section_end(__CSTACK__),            //void *cstackTop;  
  __section_end(__DATA_INIT__),         //void *initcTop;
  __section_end(__TEXT__),              //void *codeTop;
  __section_begin(__CSTACK__),          //void *cstackBottom;
  __section_end(__EMHEAP_OVERLAY__),    //void *heapTop;
  __section_end(__SIMEE__),             //void *simeeTop;
  __section_end(__DEBUG_CHANNEL__)      //void *debugChannelTop;
};



//=============================================================================
// Define the vector table as a HalVectorTableType.  __root ensures the compiler
// will not strip the table.  const ensures the table is placed into flash.
// @ "INTVEC" tells the compiler/linker to place the vector table in the INTVEC
// segment which holds the reset/interrupt vectors at address 0x00000000.
// 
// All Handlers point to a corresponding ISR.  The ISRs are prototyped above.
// The file isr-stubs.s79 provides a weak definition for all ISRs.  To
// "register" its own ISR, an application simply has to define the function
// and the weak stub will be overridden.
//
// The list of handlers are extracted from the NVIC configuration file.  The
// order of the handlers in the NVIC configuration file is critical since it
// translates to the order they are placed into the vector table here.
//=============================================================================
__root const HalVectorTableType __vector_table[] @ __INTVEC__ =
{
  { .topOfStack = __section_end(__CSTACK__) },
  #ifndef INTERRUPT_DEBUGGING
    #define EXCEPTION(vectorNumber, functionName, priorityLevel) \
      functionName,
  #else //INTERRUPT_DEBUGGING  
    // The interrupt debug behavior inserts a special shim handler before
    // the actual interrupt.  The shim handler then redirects to the 
    // actual table, defined below
    #define EXCEPTION(vectorNumber, functionName, priorityLevel) \
      halInternalIntDebuggingIsr,
    // PERM_EXCEPTION is used for any vectors that cannot be redirected
    // throught the shim handler.  (such as the reset vector)
    #define PERM_EXCEPTION(vectorNumber, functionName, priorityLevel) \
      functionName,
  #endif //INTERRUPT_DEBUGGING
  #include NVIC_CONFIG
  #undef  EXCEPTION
  #undef PERM_EXCEPTION
};

// halInternalClassifyReset() records the cause of the last reset here
static int16u savedResetCause;

void halInternalClassifyReset(void) 
{
  // Table used to convert from RESET_EVENT register bits to reset types
  static const int16u resetEventTable[] = {
    RESET_POWERON_HV,            // bit 0: RESET_PWRHV_BIT
    RESET_POWERON_LV,            // bit 1: RESET_PWRLV_BIT
    RESET_EXTERNAL_PIN,          // bit 2: RESET_NRESET_BIT
    RESET_WATCHDOG_EXPIRED,      // bit 3: RESET_WDOG_BIT
    RESET_SOFTWARE,              // bit 4: RESET_SW_BIT
    RESET_BOOTLOADER_DEEPSLEEP,  // bit 5: RESET_DSLEEP_BIT
    RESET_FATAL_OPTIONBYTE,      // bit 6: RESET_OPTBYTEFAIL_BIT
    RESET_FATAL_LOCKUP,          // bit 7: RESET_CPULOCKUP_BIT
  };
  
  // It is possible for RSTB and POWER_HV to be set at the same time, which
  // happens when RSTB releases between HV power good and LV power good. (All
  // other reset events are mutually exclusive.) When both RSTB and POWER_HV
  // are set, POWER_HV should be returned as the cause. The bit test order --
  // from LSB to MSB -- ensures that it will.
  int16u resetEvent = RESET_EVENT & 
                        ( RESET_CPULOCKUP_MASK   |
                          RESET_OPTBYTEFAIL_MASK |
                          RESET_DSLEEP_MASK      |
                          RESET_SW_MASK          |
                          RESET_WDOG_MASK        |
                          RESET_NRESET_MASK      |
                          RESET_PWRLV_MASK       |
                          RESET_PWRHV_MASK );

  int16u cause = RESET_UNKNOWN;
  int i;

  for (i = 0; i < sizeof(resetEventTable)/sizeof(resetEventTable[0]); i++) {
    if (resetEvent & (1 << i)) {
      cause = resetEventTable[i];
      break;
    }
  }

  if (cause == RESET_SOFTWARE) {
    if((halResetInfo.crash.resetSignature == RESET_VALID_SIGNATURE) &&
       (RESET_BASE_TYPE(halResetInfo.crash.resetReason) < NUM_RESET_BASE_TYPES)) {
      // The extended reset cause is recovered from RAM
      // This can be trusted because the hardware reset event was software
      //  and additionally because the signature is valid
      savedResetCause = halResetInfo.crash.resetReason;
    } else {
      savedResetCause = RESET_SOFTWARE_UNKNOWN;
    }
    // mark the signature as invalid 
    halResetInfo.crash.resetSignature = RESET_INVALID_SIGNATURE;
  } else if (    (cause == RESET_BOOTLOADER_DEEPSLEEP) 
              && (halResetInfo.crash.resetSignature == RESET_VALID_SIGNATURE)
              && (halResetInfo.crash.resetReason == RESET_BOOTLOADER_DEEPSLEEP)) {
    // Save the crash info for bootloader deep sleep (even though it's not used
    // yet) and invalidate the resetSignature.
    halResetInfo.crash.resetSignature = RESET_INVALID_SIGNATURE;
    savedResetCause = halResetInfo.crash.resetReason;
  } else {
    savedResetCause = cause;
  }
}  

int8u halGetResetInfo(void)
{
  return RESET_BASE_TYPE(savedResetCause);
}

int16u halGetExtendedResetInfo(void)
{
  return savedResetCause;
}



#ifdef INTERRUPT_DEBUGGING
//=============================================================================
// If interrupt debugging is enabled, the actual ISRs are listed in this
// secondary interrupt table.  The halInternalIntDebuggingIsr will use this
// table to jump to the appropriate handler
//=============================================================================
__root const HalVectorTableType __real_vector_table[] = 
{
  { .topOfStack = __section_end("CSTACK") },
  #define EXCEPTION(vectorNumber, functionName, priorityLevel) \
    functionName,
    #include NVIC_CONFIG
  #undef EXCEPTION
};

//[[  Macro used with the interrupt latency debugging hooks
  #define ISR_TX_BIT(bit)  GPIO_PAOUT = (GPIO_PAOUT&(~PA7_MASK))|((bit)<<PA7_BIT)
//]]

//=============================================================================
// The halInternalDebuggingIsr will intercept all exceptions in order to 
// set interrupt debugging IO flags so that interrupt latency and other timings
// may be measured with a logic analyzer
//=============================================================================
void halInternalIntDebuggingIsr(void)
{
  boolean prevState = I_STATE(I_PORT,I_PIN);
  int32u exception;
  
  //[[ Additional debug output for printing, via printf, the name of the
  //   ISR that has been invoked and the time it took (in us).  The ISR must
  //   exceed a time threshold to print the name.
  #ifdef PRINT_ISR_NAME
  int32u beginTime = MAC_TIMER;
  int32u endTime;
  #endif //PRINT_ISR_NAME
  //]]
 
  I_SET(I_PORT, I_PIN);

  // call the actual exception we were supposed to go to.  The exception
  // number is conveniently available in the SCS_ICSR register
  exception = (SCS_ICSR & SCS_ICSR_VECACTIVE_MASK) >> SCS_ICSR_VECACTIVE_BIT;
  
  //[[  This is a little bit of additional debug output that serially shows
  //     which exception was triggered, so that long ISRs can be determined
  //     from a logic analyzer trace
  #if 0
  {
    int32u byte = (exception<<3) | 0x2;
    int32u i;
    for(i=0;i<11;i++) {
     ISR_TX_BIT(byte&0x1); //data bit
      byte = (byte>>1);
    }
    ISR_TX_BIT(1); //stop bit
  }
  #endif
  //]]

  __real_vector_table[exception].ptrToHandler();

  // In order to deal with the possibility of nesting, only clear the status
  // output if it was clear when we entered
  if(!prevState)
    I_CLR(I_PORT, I_PIN);
  //[[ Some additional debug output to show the ISR exited
  #if 0
  else {
    ISR_TX_BIT(0);
    ISR_TX_BIT(1);
  }
  #endif
  //]]
  
  //[[ Additional debug output for printing, via printf, the name of the
  //   ISR that has been invoked and the time it took (in us).  The ISR must
  //   exceed a time threshold to print the name.
  #ifdef PRINT_ISR_NAME
  endTime = MAC_TIMER;
  if((endTime-beginTime)>150) {
    EmberStatus emberSerialGuaranteedPrintf(int8u port, PGM_P formatString, ...);
    emberSerialGuaranteedPrintf(1, "[%d:", (endTime-beginTime));
    switch(INT_ACTIVE) {
      case INT_DEBUG:
        emberSerialGuaranteedPrintf(1, "DEBUG");
      break;
      case INT_IRQD:
        emberSerialGuaranteedPrintf(1, "IRQD");
      break;
      case INT_IRQC:
        emberSerialGuaranteedPrintf(1, "IRQC");
      break;
      case INT_IRQB:
        emberSerialGuaranteedPrintf(1, "IRQB");
      break;
      case INT_IRQA:
        emberSerialGuaranteedPrintf(1, "IRQA");
      break;
      case INT_ADC:
        emberSerialGuaranteedPrintf(1, "ADC");
      break;
      case INT_MACRX:
        emberSerialGuaranteedPrintf(1, "MACRX");
      break;
      case INT_MACTX:
        emberSerialGuaranteedPrintf(1, "MACTX");
      break;
      case INT_MACTMR:
        emberSerialGuaranteedPrintf(1, "MACTMR");
      break;
      case INT_SEC:
        emberSerialGuaranteedPrintf(1, "SEC");
      break;
      case INT_SC2:
        emberSerialGuaranteedPrintf(1, "SC1");
      break;
      case INT_SC1:
        emberSerialGuaranteedPrintf(1, "SC1");
      break;
      case INT_SLEEPTMR:
        emberSerialGuaranteedPrintf(1, "SLEEPTMR");
      break;
      case INT_BB:
        emberSerialGuaranteedPrintf(1, "BB");
      break;
      case INT_MGMT:
        emberSerialGuaranteedPrintf(1, "MGMT");
      break;
      case INT_TIM2:
        emberSerialGuaranteedPrintf(1, "TIM2");
      break;
      case INT_TIM1:
        emberSerialGuaranteedPrintf(1, "TIM1");
      break;
    }
    emberSerialGuaranteedPrintf(1, "]");
  }
  #endif //PRINT_ISR_NAME
  //]]
}
#endif //INTERRUPT_DEBUGGING



