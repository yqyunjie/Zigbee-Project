/*
 * File: micro.c
 * Description: AVR Atmega128 micro specific HAL functions
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
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "stack/include/ember-debug.h"

#include "hal/hal.h"
#include "app/util/serial/serial.h"

// reserved ram is special, see reserved-ram.h
__no_init __root int16u panID                          @ HAL_RESERVED_RAM_PAN_ID;
__no_init __root int16u nodeID                         @ HAL_RESERVED_RAM_NODE_ID;
__no_init __root int8u  radioChannel                   @ HAL_RESERVED_RAM_CHANNEL;
__no_init __root volatile int8u  emResetCause          @ HAL_RESERVED_RAM_RESET_CAUSE;
__no_init __root volatile int8u  emResetCauseMarker    @ HAL_RESERVED_RAM_RESET_CAUSE_B;
__no_init __root volatile int16u emResetRStack         @ HAL_RESERVED_RAM_RSTACK_L;
__no_init __root volatile int16u emResetCStack         @ HAL_RESERVED_RAM_CSTACK_L;

#ifdef INTERRUPT_DEBUGGING
void interruptDebugInitGpio(void)
{
  DDRC |= 0x3C;               // PC2-5 are LED2-5
  PORTC &= ~0x3C;             // note that this turns on the LEDs
  DDRE |= 0x84;               // PE2 is GPIO 3, PE7 is GPIO 5
  PORTE &= ~0x84;
  DDRG |= 0x07;               // PG0-2 are GPIO 0-2
  PORTG &= ~0x07;
}
#else
#define interruptDebugInitGpio() do {} while (0)
#endif

void halInit(void)
{
  #ifndef DISABLE_WATCHDOG
    halInternalEnableWatchDog();
  #endif

  halInternalStartSystemTimer();

  //Building an AVR app against the EM250 PHY indicates we're building a host
  //app because the EM250 PHY implies an EM260 NCP.  The Host does not use
  //this, though.
  #ifndef PHY_EM250
  halInternalStartSymbolTimer();
  #endif

  halInternalInitBoard();

  interruptDebugInitGpio();
}

void halInternalForceWatchdogReset(int8u causeMask)
{
  emResetCauseMarker = BIT(RESET_CAUSE_SELF_WDOG)| causeMask;
  // instead of jumping to the reset vector and having to
  // re init all the atmel control registers ourselves, we
  // force a watchdog reset with a minimum timeout
  WDTCR = BIT(WDCE) + BIT(WDE);
  WDTCR = BIT(WDE);    // ~18ms
  for (;;) { }   // wait for the WDT to get us
}

void halReboot(void)
{
  halInternalForceWatchdogReset(BIT(RESET_CAUSE_SOFTWARE_BIT));
}

void halPowerDown(void)
{
  halInternalPowerDownBoard();

  halInternalPowerDownUart();

  halInternalDisableWatchDog(MICRO_DISABLE_WATCH_DOG_KEY);
}

void halPowerUp(void)
{
  #ifndef DISABLE_WATCHDOG
    halInternalEnableWatchDog();
  #endif

  halInternalPowerUpUart();

  halInternalPowerUpBoard();
}

void halStackProcessBootCount(void)
{
  //Building an AVR app against the EM250 PHY indicates we're building a host
  //app because the EM250 PHY implies an EM260 NCP.  The Host does not use
  //this, though.
  #ifndef PHY_EM250
  //Note: Becuase this always counts up at every boot (called from emberInit),
  //and non-volatile storage has a finite number of write cycles, this will
  //eventually stop working.  Disable this token call if non-volatile write
  //cycles need to be used sparingly.
  halCommonIncrementCounterToken(TOKEN_STACK_BOOT_COUNTER);
  #endif
}

#pragma optimize=0
void halInternalEnableWatchDog(void)
{
  // The watchdog timer is a *separate* 1 MHz clock that is independent of the
  // other timers.
  // The following two WDTCR operations must occur within 4 cycles of each
  // other.  The ATOMIC block will prevent interrupts from delaying the second 
  // operation.  The pragma before this function prevents the compiler from
  // optimizing the first half of the ATOMIC and the the first of the watchdog
  // operations via a function call.  See bugzid: 8723.
  ATOMIC(
    WDTCR = BIT(WDCE) + BIT(WDE);
    // WDP=7 yields a watchdog period of 2 million cycles, or 2 seconds.
    WDTCR = BIT(WDE) + BIT(WDP2) + BIT(WDP1) + BIT(WDP0);
  );
}

#pragma optimize=0
void halInternalDisableWatchDog(int8u magicKey)
{
  // The following two WDTCR operations must occur within 4 cycles of each
  // other.  The ATOMIC block will prevent interrupts from delaying the second 
  // operation.  The pragma before this function prevents the compiler from
  // optimizing the first half of the ATOMIC and the the first of the watchdog
  // operations via a function call.  See bugzid: 8723.
  if (magicKey == MICRO_DISABLE_WATCH_DOG_KEY) {
    ATOMIC(
      WDTCR = BIT(WDCE) + BIT(WDE);
      WDTCR = 0;
    );
  }
}


#if !defined(EMBER_ASSERT_OUTPUT_DISABLED)
void halDumpStack(int8u max)
{
  int8u i;
  int8u l,h;
  int16u pcW;
  int32u pcB;

  pcW = SP;
  emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, "SP=%2x", pcW);
  //stack trace:
  for (i=0; i<max; i++) {
    h = *((int8u*)(SP+2*i+1));
    l = *((int8u*)(SP+2*i+2));
    pcW = HIGH_LOW_TO_INT(h,l);
    pcB = pcW;
    pcB = pcB << 1;
    emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, " %4x", pcB);
  }
}
#endif

void halInternalAssertFailed(PGM_P filename, int linenumber)
{
  halResetWatchdog();              // In case we're close to running out.

#ifdef DEBUG // no option to suppress the debug message.
  //emberDebug messages require interrupts to operate, but if our interrupts
  //are off we cant use a debug message.  Therefor, supress these and fall
  //through to the normal assert mechanism.
  if(!INTERRUPTS_ARE_OFF()) {
    emberDebugAssert(filename, linenumber);
    halResetWatchdog();              // In case we're close to running out.
    emberDebugMemoryDump((int8u *)0, (int8u *)4095);  // entire ram
  }
#endif

#if !defined(EMBER_ASSERT_OUTPUT_DISABLED)
  emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, 
                              "\r\n[ASSERT:%p:%d:", 
                              filename, 
                              linenumber);
  halDumpStack(10); //stack trace
#ifdef ENABLE_DATA_LOGGING
  emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, ";");
  halPrintLogData(EMBER_ASSERT_SERIAL_PORT); // And log data
#endif
  emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, "]\r\n");
#ifdef DEBUG_ASSERT
  // dump number of buffer failure
  emberSerialGuaranteedPrintf(EMBER_ASSERT_SERIAL_PORT, 
                              "#buffer failure %x \r\n", bufferFailure);
  // dump failure stack
  dumpFailure();
#endif // DEBUG_ASSERT  
#endif

  halInternalForceWatchdogReset(BIT(RESET_CAUSE_ASSERT_BIT));
}


EmberResetType halGetResetInfo(void)
{
  static int8u cause_mark = 0xFF;
  if ( cause_mark == 0xFF ) {
    cause_mark = emResetCauseMarker;
    emResetCauseMarker = 0;
  }
  switch ( emResetCause ) {
    case BIT(PORF):  return RESET_POWERON;
    case BIT(EXTRF): return RESET_EXTERNAL;
    case BIT(WDRF):
      if ( cause_mark & BIT(RESET_CAUSE_SELF_WDOG) ) {
        if ( cause_mark & BIT(RESET_CAUSE_ASSERT_BIT) ) {
          return RESET_ASSERT;
        } else if ( cause_mark & BIT(RESET_CAUSE_CSTACK_BIT) ) {
          return RESET_CSTACK;
        } else if ( cause_mark & BIT(RESET_CAUSE_RSTACK_BIT) ) {
          return RESET_RSTACK;
        } else if ( cause_mark & BIT(RESET_CAUSE_BOOTLOADER_BIT) ) {
          return RESET_BOOTLOADER;
        } else if ( cause_mark & BIT(RESET_CAUSE_SOFTWARE_BIT) ) {
          return RESET_SOFTWARE;
        } else {
          return RESET_UNKNOWN;
        }
      }
      return RESET_WATCHDOG;
    case BIT(BORF):  return RESET_BROWNOUT;
#ifdef JTDR
    case BIT(JTDR):  return RESET_JTAG;
#else
    case BIT(JTRF):  return RESET_JTAG;
#endif
  }
  return RESET_UNKNOWN;
}

PGM_P halGetResetString(void)
{
  static PGM_P resetString[] = { "PCZERO", "PWR", "EXT", "WDOG", "BOD", "JTAG", 
                                 "RSTK", "CSTK", "ASSERT", "BL", "SW" };
  switch ( halGetResetInfo() ) {
    case RESET_POWERON:
      return(resetString[1]);
    case RESET_EXTERNAL:
      return(resetString[2]);
    case RESET_WATCHDOG:
      return(resetString[3]);
    case RESET_BROWNOUT:
      return(resetString[4]);
    case RESET_JTAG:
      return(resetString[5]);
    case RESET_RSTACK:
      return(resetString[6]);
    case RESET_CSTACK:
      return(resetString[7]);
    case RESET_ASSERT:
      return(resetString[8]);
    case RESET_BOOTLOADER:
      return(resetString[9]);
    case RESET_SOFTWARE:
      return(resetString[10]);
  }
  return(resetString[0]);
}


void halSleep(SleepModes sleepMode)
{
  // enable sleeping
  MCUCR |= BIT(SE);

  switch ( sleepMode )
  {
    case SLEEPMODE_IDLE:       // idle - sleep mode        (uart ints, timer ints, and external ints can wake us)
      MCUCR &= ~(BIT(SM1)|BIT(SM0));
      break;
    case SLEEPMODE_RESERVED:   // reserved - sleep mode
      MCUCR &= ~BIT(SM1); MCUCR |= BIT(SM0);
      break;
    case SLEEPMODE_POWERDOWN:  // power down - sleep mode  (only external ints can wake us)
      MCUCR |= BIT(SM1); MCUCR &= ~BIT(SM0);
      break;
    case SLEEPMODE_POWERSAVE:  // power save - sleep mode  (wake on ext ints, and optionally timer0)
      MCUCR |= BIT(SM1)|BIT(SM0);
      break;
  }
  // Note: The sleep instruction is executed before any pending interrupts.
  __enable_interrupt();
  __sleep();

  // disable sleeping
  MCUCR &= ~BIT(SE);
}


void halCommonDelayMicroseconds(int16u us)
{
  if (0==us) { // avoid 0xffff
    return;
  }
  while ( (us-=1) > 0 ) {  // 6 cycles
    // 6 for the loop +
    asm("mul r18,r18"); // 2
    // want 8 total (for 2 us)
  }
  // return is 4 cycles (but not accounted for)
}


void halStackTriggerDebugTimeSyncCapture( void )
{
  //Building an AVR app against the EM250 PHY indicates we're building a host
  //app because the EM250 PHY implies an EM260 NCP.  The Host does not use
  //this, though.
  #ifndef PHY_EM250
  SETBIT(DEBUGSYNC_TRIGGER_PORT, DEBUGSYNC_TRIGGER);
  SETBIT(DEBUGSYNC_TRIGGER_DDR, DEBUGSYNC_TRIGGER);
  CLEARBIT(DEBUGSYNC_TRIGGER_PORT, DEBUGSYNC_TRIGGER);
  SETBIT(DEBUGSYNC_TRIGGER_PORT, DEBUGSYNC_TRIGGER);
  #endif
}
