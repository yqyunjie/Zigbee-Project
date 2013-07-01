/*
 * File: system-timer.c
 * Description: AVR Atmega128 micro specific HAL functions
 *
 * Author(s): Lee Taylor, lee@ember.com,
 *            Jeff Mathews, jm@ember.com
 *
 * Copyright 2008 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>

#include <stdarg.h>

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"

#include "hal/hal.h"

// Uncomment the line below to change the system clock prescaler for 1 ms ticks
//#define MSEC_SYSTEM_TICKS   // set up system timer for ~1 msec input clock

// Alway use msec ticks in EZSP UART to reduce contention with the uart rx isr
// in the radio rx and tx isrs' HANDLE_PENDING_INTERRUPTS invocations
#if defined(EZSP_UART) && !defined(MSEC_SYSTEM_TICKS)
#define MSEC_SYSTEM_TICKS
#endif

static int32u systemTick;
//Simple time conversion macros used by halSleepForQuarterSeconds
#define CONVERT_QS_TO_TICKS(x) (x << 3)
#define CONVERT_TICKS_TO_QS(x) (x >> 3)
#define TIMER_MAX_QS           32
//A simple flag used by halSleepForQuarterSeconds to check that it has exited
//from sleep mode at the request of the expected timer interrupt.
static boolean timerInterruptOccurred = FALSE;
//sleepingIsActive is used as a semaphore so that systemTick is only increment
//once when sleeping exits (since it can be incremented by both sleep code
//and getmillisecondtick).
boolean sleepingIsActive = FALSE;
//We must save the systemTick when sleeping because the ISRs will still
//increment the real systemTick, and we need to work independently of that
int32u savedSystemTick;
//We must save TCNT0 when sleeping because sleep operation will still
//increment the real TNCT0, and we need to work independently of that
int8u  savedTCNT0;

/**
 * Set up the 8-bit timer 0 to use external 32.768kHz crystal. 
 * Using a prescaler value of 32, one tick represents 976.6 ms, and an
 * overflow occurs every 250 ms.
 * Using a prescalar value of 1, one tick represents 30.52 us, and an
 * overflow occurs every 7.8125 ms.
 */
int16u halInternalStartSystemTimer(void)
{
  int16u asyncTime = 0;
  DECLARE_INTERRUPT_STATE_LITE;
  DISABLE_INTERRUPTS_LITE();
  __watchdog_reset(); // if the wdog is enabled, give us 2 seconds starting now.
  TIMSK &= ~(BIT(OCIE0)|BIT(TOIE0)); // disable tim0 ints
  TCNT0 = 0;                         // tim0 counter register
#ifdef MSEC_SYSTEM_TICKS
  TCCR0 = 3;                         // 32:1 prescale
#else
  TCCR0 = 1;                         // 1:1 prescale
#endif
  ASSR |= BIT(AS0);                  // clock source (async ext xtal)
  TIFR = (BIT(OCF0)|BIT(TOV0));      // clear (write 1) int flags
  // We use output compare with match set to FF rather than oveflow
  //  because the asynchronous operation of the timer requires 1 timer
  //  cycle to trigger the interrupt.
  OCR0 = 0xFF;
  // The crystal can take a little while to actually start up, so we wait for it
  // We don't know how long we have to wait for the crystal to startup, but
  // we don't want to wait very long.  The assert is designed to inform us the
  // crystal took too long to startup.  The 1.5 second timing is used when the
  // watchdog is active, otherwise we would see a watchdog reset which could
  // be very confusing and take a while to find.  The 6.5 second timing is
  // used when the watchdog isn't active (generally a test application) to
  // be more forgiving, but a delay of that length still probably means
  // something is wrong.
  while ( ASSR & 0x7 ) { // wait for asych update of TCNT0/OCR0/TCCR0
    halCommonDelayMicroseconds(100);
    // Setup an assert to trigger before any watchdog comes around so we know
    // what the cause is
    #ifdef DISABLE_WATCHDOG
      assert( asyncTime++ < 65535 );  // 6.5 seconds (max before 16bit overflow)
    #else
      assert( asyncTime++ < 15000 );  // 1.5 seconds (must be < wdog timeout)
    #endif
  }
  TIMSK |= BIT(OCIE0);  // enable tim0 output compare int
  RESTORE_INTERRUPTS_LITE();

  systemTick = 0;
  return asyncTime;
}

/**
 * Return a 16 bit real time clock value. The clock's increment is 0.977 ms
 *  and it rolls over every 64 sec.
 */
int16u halCommonGetInt16uMillisecondTick(void)
{
  return (int16u)halCommonGetInt32uMillisecondTick();
}

int16u halCommonGetInt16uQuarterSecondTick(void)
{
  return (int16u)(halCommonGetInt32uMillisecondTick() >> 8);
}

#pragma vector = TIMER0_COMP_vect
__interrupt void TIMER0_COMP_interrupt (void)
{
  INT_DEBUG_BEG_SYSTIME_CMP();  // if enabled, flag entry to isr on a output pin
  timerInterruptOccurred = TRUE;
  ++systemTick;
  INT_DEBUG_END_SYSTIME_CMP();  // if enabled, flag exit from isr on a output pin
}

#ifdef MSEC_SYSTEM_TICKS
int32u halCommonGetInt32uMillisecondTick(void)
{
  int32u t;
  int8u c;
  ATOMIC_LITE(
    if(sleepingIsActive) {
      //capture the savedSystemTick and adjust for the latest sleep cycle
      t = savedSystemTick + CONVERT_TICKS_TO_QS(TCNT0);
      //t is now systemTick proper, finish with the equation used below
      t = (t << 8) | savedTCNT0;
      //the real savedSystemTick and systemTick will be restored later
    } else {
      // read before testing if the interrupt is pending, in case it goes pending
      // right after we read it
      c = TCNT0;
      if (TIFR & BIT(OCF0)) {
        // account for interrupt that hasn't yet been able to occur
        // have to re-read TCNT0 because it has wrapped.
        c = TCNT0;
        t = ((systemTick + 1) << 8) | c;
      } else {
        t = (systemTick << 8) | c;
      }
    }
  ) //ATOMIC_LITE
  return t;
}
#else
int32u halCommonGetInt32uMillisecondTick(void)
{
  int32u t;
  int8u c;
  ATOMIC_LITE(
    if(sleepingIsActive) {
      //capture the savedSystemTick and adjust for the latest sleep cycle
      t = savedSystemTick + (CONVERT_TICKS_TO_QS(TCNT0)*32);
      //account for the lower 3 bits of TCNT0
      t += ((TCNT0&0x7)*4);
      //t is now systemTick proper, finish with the equation used below
      t = (t << 3) | (savedTCNT0 >> 5);
      //the real savedSystemTick and systemTick will be restored later
    } else {
      // read before testing if the interrupt is pending, in case it goes pending
      // right after we read it
      c = TCNT0;
      if (TIFR & BIT(OCF0)) {
        // account for interrupt that hasn't yet been able to occur
        //  have to re-read TCNT0 because it has wrapped.
        c = TCNT0;
        t = ((systemTick + 1) << 3) | (c >> 5);
      } else {
        t = (systemTick << 3) | (c >> 5);
      }
    }
  ) //ATOMIC_LITE
  return t;
}
#endif

#pragma vector = TIMER0_OVF_vect
__interrupt void TIMER0_OVF_interrupt (void)
{
  INT_DEBUG_BEG_MISC_INT();
  timerInterruptOccurred = TRUE;
  INT_DEBUG_END_MISC_INT();
}

// Stub to make em260 app work on the AVR.
EmberStatus halIdleForMilliseconds(int32u *duration)
{
  INTERRUPTS_ON();
  return EMBER_SLEEP_INTERRUPTED;
}

EmberStatus halSleepForQuarterSeconds(int32u *duration)
{
  static int32u sleepOverflowCount;
  EmberStatus status = EMBER_SUCCESS;

  //We're munging the system timer, so to play it safe we're saving the
  //entire timer state to restore at the end, and we increment the counters
  //to account (as closely as possible) for the time that has elapsed while
  //asleep
  int8u  savedTIMSK = TIMSK;
  int8u  savedTCCR0 = TCCR0;
  int8u  savedOCR0  = OCR0;
  savedTCNT0 = TCNT0;
  savedSystemTick = systemTick;

  //go atomic to prevent errant interrupts while reconfiguring the timer
  ATOMIC_LITE(
    TIMSK &= ~(BIT(OCIE0)|BIT(TOIE0)); // disable tim0 ints
    TCNT0 = 0;                         // zero tim0 counter register
    //By using a prescalar value of 1024 (on the 32768Hz clock), one tick
    //represents 31.25ms, and an overflow occurs every 8 seconds
    TCCR0 = 7;                         // CLK/1024
    ASSR |= BIT(AS0);                  // clock source (async ext xtal)
    TIFR = (BIT(OCF0)|BIT(TOV0));      // clear (write 1) int flags
    //overflow occurs every 8 seconds, which is 32 quarter-seconds.
    sleepOverflowCount = 0; //last execution could have left this non-zero
    sleepOverflowCount = ((*duration)/TIMER_MAX_QS); //number of OVF's to do
    OCR0 = CONVERT_QS_TO_TICKS((*duration)%TIMER_MAX_QS); //final compare count

    while ( ASSR & 0x7 ) {} // wait for asych update of TCNT0/OCR0/TCCR0
    
    sleepingIsActive = TRUE;
    timerInterruptOccurred = FALSE;
    if(sleepOverflowCount) {
      TIMSK |= BIT(TOIE0);  // enable tim0 overflow int
    } else {
      TIMSK |= BIT(OCIE0);  // enable tim0 output compare int
    }
  ) // ATOMIC_LITE
  
#ifdef MSEC_SYSTEM_TICKS
  while(*duration > 0) {
    halSleep(SLEEPMODE_POWERSAVE);
    
    TIMSK &= ~(BIT(OCIE0)|BIT(TOIE0)); // disable tim0 ints
    //if we didn't come out of sleep via a compare or overflow interrupt,
    //it was an abnormal sleep interruption; report the event
    if(!timerInterruptOccurred) {
      status = EMBER_SLEEP_INTERRUPTED;
      // update duration to account for how long last sleep was
      *duration -= CONVERT_TICKS_TO_QS(TCNT0);
      //how much did we sleep? systemTick is a quarter second
      savedSystemTick += CONVERT_TICKS_TO_QS(TCNT0);
      break;
    } else {
      if(sleepOverflowCount--) {
        *duration -= TIMER_MAX_QS;
        //how much did we sleep? systemTick is a quarter second
        savedSystemTick += TIMER_MAX_QS;
      } else {
        *duration -= CONVERT_TICKS_TO_QS(OCR0);
        //how much did we sleep? systemTick is a quarter second
        savedSystemTick += CONVERT_TICKS_TO_QS(OCR0);
      }
      timerInterruptOccurred = FALSE;
      if(sleepOverflowCount) {
        TIMSK |= BIT(TOIE0);  // enable tim0 overflow int
      } else if(!sleepOverflowCount && (*duration>0)){
        TIMSK |= BIT(OCIE0);  // enable tim0 output compare int
      }
    }
  }
  
#else
  while(*duration > 0) {
    halSleep(SLEEPMODE_POWERSAVE);
    
    TIMSK &= ~(BIT(OCIE0)|BIT(TOIE0)); // disable tim0 ints
    //if we didn't come out of sleep via a compare or overflow interrupt,
    //it was an abnormal sleep interruption; report the event
    if(!timerInterruptOccurred) {
      status = EMBER_SLEEP_INTERRUPTED;
      // update duration to account for how long last sleep was
      *duration -= CONVERT_TICKS_TO_QS(TCNT0);
      //how much did we sleep? systemTick is 7.8125ms.  1 qs = 32 systemTicks
      savedSystemTick += (CONVERT_TICKS_TO_QS(TCNT0)*32);
      break;
    } else {
      if(sleepOverflowCount--) {
        *duration -= TIMER_MAX_QS;
        //how much did we sleep? systemTick is 7.8125ms.  1 qs = 32 systemTicks
        savedSystemTick += (TIMER_MAX_QS*32);
      } else {
        *duration -= CONVERT_TICKS_TO_QS(OCR0);
        //how much did we sleep? systemTick is 7.8125ms.  1 qs = 32 systemTicks
        savedSystemTick += (CONVERT_TICKS_TO_QS(OCR0)*32);
      }
      timerInterruptOccurred = FALSE;
      if(sleepOverflowCount) {
        TIMSK |= BIT(TOIE0);  // enable tim0 overflow int
      } else if(!sleepOverflowCount && (*duration>0)){
        TIMSK |= BIT(OCIE0);  // enable tim0 output compare int
      }
    }
  }
  
  //at this point, the lower 3 bits of TCNT0 have not been accounted for
  //TCNT0 is currently configured for a 31.25ms tick.  31.25 ms = 4 systemTicks
  savedSystemTick += ((TCNT0&0x7)*4);
#endif  

  //restore the timer's configuration
  //we can get away with simply restoring because the original configuration
  //tick size was so much smaller than the sleep tick size it's not possible
  //to maintain that accuracy.  Just put it back to where it was.
  ATOMIC_LITE(
    sleepingIsActive = FALSE;
    systemTick = savedSystemTick;
    TCNT0 = savedTCNT0;
    TIMSK = savedTIMSK;
    TCCR0 = savedTCCR0;
    OCR0  = savedOCR0;
    while ( ASSR & 0x7 ) {} // wait for asych update of TCNT0/OCR0/TCCR0
  ) // ATOMIC_LITE
  return status;
}

void halCommonSetSystemTime(int32u time)
{
  ATOMIC_LITE(
    TCNT0 = (int8u)time << 5;
    if (TIFR & BIT(OCF0)) {
      // account for interrupt that hasn't yet been able to occur
      systemTick = (time >> 3) - 1;
    } else {
      systemTick = (time >> 3);
    }
  )
}
