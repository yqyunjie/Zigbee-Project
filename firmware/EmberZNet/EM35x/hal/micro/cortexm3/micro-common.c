/*
 * File: micro.c
 * Description: EM3XX micro specific HAL functions common to 
 *  full and minimal hal
 *
 *
 * Copyright 2013 Silicon Laboratories, Inc.                                *80*
 */
//[[ Author(s): Brooks Barrett, Lee Taylor ]]


#include PLATFORM_HEADER
#include "include/error.h"
#include "hal/micro/micro-common.h"
#include "hal/micro/cortexm3/micro-common.h"
#if defined(BOARD_HEADER) && !defined(MINIMAL_HAL)
  #include BOARD_HEADER
#endif //defined(BOARD_HEADER)

//NOTE:  The reason ENABLE_OSC32K gets translated into an int8u is so that
//       haltest can override the build time configuration and force use of
//       a different OSC32K mode when running the "RTC" command which is used
//       to check proper connectivity of the 32kHz XTAL or external clock.
#define DO_EXPAND(sym)  1 ## sym
#define EXPAND(sym)     DO_EXPAND(sym)
#define CHECK_BLANK_DEFINE(sym) (1 == EXPAND(sym))
#ifdef  ENABLE_OSC32K
  #if CHECK_BLANK_DEFINE(ENABLE_OSC32K) // Empty #define => OSC32K_CRYSTAL
    #undef  ENABLE_OSC32K
    #define ENABLE_OSC32K OSC32K_CRYSTAL // Default if ENABLE_OSC32K is blank
  #endif
#else//!ENABLE_OSC32K
  #define ENABLE_OSC32K OSC32K_DISABLE // Default if BOARD_HEADER doesn't say
#endif//ENABLE_OSC32K
int8u useOsc32k = ENABLE_OSC32K;
#ifndef OSC32K_STARTUP_DELAY_MS
  #define OSC32K_STARTUP_DELAY_MS (0) // Not all BOARD_HEADERs define this
#endif//OSC32K_STARTUP_DELAY_MS

void halInternalEnableWatchDog(void)
{
  //Just to be on the safe side, restart the watchdog before enabling it
  WDOG_RESET = 1;
  WDOG_KEY = 0xEABE;
  WDOG_CFG = WDOG_ENABLE;
}

void halInternalResetWatchDog(void)
{
  //Writing any value will restart the watchdog
  WDOG_RESET = 1;
}

void halInternalDisableWatchDog(int8u magicKey)
{
  if (magicKey == MICRO_DISABLE_WATCH_DOG_KEY) {
    WDOG_KEY = 0xDEAD;
    WDOG_CFG = WDOG_DISABLE;
  }
}

boolean halInternalWatchDogEnabled(void)
{
  if(WDOG_CFG&WDOG_ENABLE) {
    return TRUE;
  } else {
    return FALSE;
  }
}

void halGpioConfig(int32u io, int32u config)
{
  static volatile int32u *const configRegs[] = 
    { (volatile int32u *)GPIO_PACFGL_ADDR,
      (volatile int32u *)GPIO_PACFGH_ADDR,
      (volatile int32u *)GPIO_PBCFGL_ADDR,
      (volatile int32u *)GPIO_PBCFGH_ADDR,
      (volatile int32u *)GPIO_PCCFGL_ADDR,
      (volatile int32u *)GPIO_PCCFGH_ADDR };
  int32u portcfg;
  portcfg = *configRegs[io/4];                // get current config                   
  portcfg = portcfg & ~((0xF)<<((io&3)*4));   // mask out config of this pin
  *configRegs[io/4] = portcfg | (config <<((io&3)*4));
}

//[[ Most of the system-timer functionality is part of the hal-library
//  This functionality is kept public because it depends on configuration 
//  options defined in the BOARD_HEADER.  Only for the full HAL, though. In
//  the minimal HAL if the user has to supply the two ifdefs they do so
//  in whatever manner they choose.
//]]
int16u halInternalStartSystemTimer(void)
{
  //Since the SleepTMR is the only timer maintained during deep sleep, it is
  //used as the System Timer (RTC).  We maintain a 32 bit hardware timer
  //configured for a tick value time of 1024 ticks/second (0.9765625 ms/tick)
  //using either the 10 kHz internal SlowRC clock divided and calibrated to
  //1024 Hz or the external 32.768 kHz crystal divided to 1024 Hz.
  //With a tick time of ~1ms, this 32bit timer will wrap after ~48.5 days.
  
  //disable top-level interrupt while configuring
  INT_CFGCLR = INT_SLEEPTMR;
  
  if (useOsc32k > OSC32K_DISABLE) {
    if (useOsc32k == OSC32K_DIGITAL) {
      //Disable both OSC32K and SLOWRC if using external digital clock input
      SLEEPTMR_CLKEN = 0;
    } else { // OSC32K_CRYSTAL || OSC32K_SINE_1V
      //Enable the 32kHz XTAL (and disable SlowRC since it is not needed)
      SLEEPTMR_CLKEN = SLEEPTMR_CLK32KEN;
    }
    //Sleep timer configuration is the same for crystal and external clock
    SLEEPTMR_CFG = (SLEEPTMR_ENABLE            | //enable TMR
                   (0 << SLEEPTMR_DBGPAUSE_BIT)| //TMR not paused when halted
                   (5 << SLEEPTMR_CLKDIV_BIT)  | //divide down to 1024Hz
                   (1 << SLEEPTMR_CLKSEL_BIT)) ; //select CLK32K external clock
    halCommonDelayMilliseconds(OSC32K_STARTUP_DELAY_MS);
  } else {
    //Enable the SlowRC (and disable 32kHz XTAL since it is not needed)
    SLEEPTMR_CLKEN = SLEEPTMR_CLK10KEN;
    SLEEPTMR_CFG = (SLEEPTMR_ENABLE            | //enable TMR
                   (0 << SLEEPTMR_DBGPAUSE_BIT)| //TMR not paused when halted
                   (0 << SLEEPTMR_CLKDIV_BIT)  | //already 1024Hz
                   (0 << SLEEPTMR_CLKSEL_BIT)) ; //select CLK1K internal SlowRC
    #ifndef DISABLE_RC_CALIBRATION
      halInternalCalibrateSlowRc(); //calibrate SlowRC to 1024Hz
    #endif//DISABLE_RC_CALIBRATION
  }
  
  //clear out any stale interrupts
  INT_SLEEPTMRFLAG = (INT_SLEEPTMRWRAP | INT_SLEEPTMRCMPA | INT_SLEEPTMRCMPB);
  //turn off second level interrupts.  they will be enabled elsewhere as needed
  INT_SLEEPTMRCFG = INT_SLEEPTMRCFG_RESET;
  //enable top-level interrupt
  INT_CFGSET = INT_SLEEPTMR;
  
  return 0;
}


