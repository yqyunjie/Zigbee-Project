/*
 * File: symbol-timer.c
 * Description: AVR Atmega128 micro specific HAL functions
 *
 * Author(s): Lee Taylor, lee@ember.com,
 *            Jeff Mathews, jm@ember.com
 *
 * Copyright 2005 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>

#include <stdarg.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "include/error.h"

#include "hal/hal.h"
#include "app/util/serial/serial.h"

static int16u syncTick;
static int32u syncTimeOffset;
static int32u syncCapturedTime;

// This macro quantifies the relation between a symbol and a tick.  A symbol
// is 16us, while the MAC timer generates 8us ticks.  These pair of
// macros asbtract that conversion making the code easier to read.
#define CONVERT_SYMBOLS_TO_TICKS(x) x = x << 1
#define CONVERT_TICKS_TO_SYMBOLS(x) x = x >> 1

#define INTERNAL_RC_1MHZ_MODE 0x01
#define INTERNAL_RC_2MHZ_MODE 0x02
#define INTERNAL_RC_4MHZ_MODE 0x03
#define INTERNAL_RC_8MHZ_MODE 0x04
#define CKSEL_BITS_MASK       0x07

/**
 * Orders a callback after a certain delay.
 * This function is designed expressly for the MAC backoff algorithm.
 * Delay is specified in symbol times
 */
// Uses timer 1 compare interrupt A
void halStackOrderInt16uSymbolDelayA(int16u symbols)
{
  // make sure there is no currently pending delay
  //The MAC can only handle a single CSMA packet being transmitted at a time.
  //A second delay means the MAC has fatally errored.
  assert( (TIMSK & BIT(OCIE1A)) == 0 );
  CONVERT_SYMBOLS_TO_TICKS(symbols);
  // minimum delay required, or else compare will not happen until wrap-around
  if(symbols == 0) symbols = 1;

  ATOMIC_LITE(
    OCR1A = (TCNT1 + symbols);
    TIFR = BIT(OCF1A);  // clear any pending interrupts
  )
  TIMSK |= BIT(OCIE1A); // enable timer 1 compare interrupt A
}

void halStackOrderNextSymbolDelayA(int16u symbols)
{
  int16u timerControl1;
  int16u outputControl1;

  // make sure there is no currently pending delay
  //The MAC can only handle a single CSMA packet being transmitted at a time.
  //A second delay means the MAC has fatally errored.
  assert( (TIMSK & BIT(OCIE1A)) == 0 );
  TIFR = BIT(OCF1A);  // clear any pending interrupts
  CONVERT_SYMBOLS_TO_TICKS(symbols);
  ATOMIC_LITE( // To clarify volatile access.  Actually saves flash!
  timerControl1 = TCNT1;
  outputControl1 = OCR1A;
  // minimum delay required, or else compare will not happen until wrap-around
  if((timerControl1 - outputControl1) > symbols)
    OCR1A = timerControl1 + 1;
  else
    OCR1A = (OCR1A + symbols);
  ) // ATOMIC_LITE
  TIMSK |= BIT(OCIE1A); // enable timer 1 compare interrupt A
}

void halStackCancelSymbolDelayA(void)
{
  ATOMIC_LITE(
    // Disable interrupt (timer 1 compare A interrupt).
    TIMSK &= ~BIT(OCIE1A);
    // Clear pending interrupt.
    TIFR = BIT(OCF1A);
  )
}

#pragma vector = TIMER1_COMPA_vect
__interrupt void TIMER1_COMPA_interrupt (void)
{
  INT_DEBUG_BEG_SYMBOL_CMP(); // if enabled, flag entry to isr on a output pin
  // disable timer 1 compare interrupt
  TIMSK &= ~BIT(OCIE1A);
  halStackSymbolDelayAIsr();
  INT_DEBUG_END_SYMBOL_CMP(); // if enabled, flag exit from isr on a output pin
}

// Timer 1 is used for the mac timer
void halInternalStartSymbolTimer(void)
{
  // disable all interrupts
  TIMSK &= ~(BIT(TICIE1)|BIT(OCIE1A)|BIT(TOIE1));
  ETIMSK &= ~(BIT(OCIE1C));
  // disable any special modes
  TCCR1A = 0;
  // set prescale to clk/64
  // This creates a resolution of 8us on 8Mhz boards
  TCCR1B = (BIT(CS11) | BIT(CS10));
  // reset counter register (16 bit)
  TCNT1 = 0;
  // clear (write 1) any pending interrupt flags
  TIFR = (BIT(ICF1)|BIT(OCF1A)|BIT(TOV1));
  ETIFR = (BIT(OCF1C));
  // enable overflow interrupt
  TIMSK |= BIT(TOIE1);
  syncTick = 0;
  syncTimeOffset = 0;
  syncCapturedTime = 0;
}

// Need these next two functions for packet trace.
#pragma vector = TIMER1_OVF_vect
__interrupt void TIMER1_OVF_interrupt (void)
{
  INT_DEBUG_BEG_MISC_INT();
  ++syncTick;
  INT_DEBUG_END_MISC_INT();
}

void halInternalSfdCaptureIsr(void)
{
  syncCapturedTime = halStackGetInt32uSymbolTick();
  
  // The following call is only used when debugging the debug channel time
  //  synchronization between the node, dev kit board, and packet trace
  //  it is left here for easy resurrection when needed but is not a
  //  requirement of any hal implementation.
  //emDebugTimeSyncTest();
}

int32u halStackGetInt32uSymbolTick(void)
{
  int32u t;
  int16u c;
  ATOMIC_LITE(
    // read before testing if the interrupt is pending, in case it goes pending
    // right after we read it
    c = TCNT1;
    if (TIFR & BIT(TOV1)) {
      // account for interrupt that hasn't yet been able to occur
      //  have to re-read TCNT1 because it has wrapped.
      c = TCNT1;
      t = syncTick + 1;
    } else {
      t = syncTick;
    }
    t = t << 16;
    t = (t | c);
  ) //ATOMIC_LITE
  CONVERT_TICKS_TO_SYMBOLS(t);
  t += syncTimeOffset;
  return t;
}

int32u halStackGetInt32uSfdCapture(void)
{
  int32u t;
  ATOMIC_LITE(
    // make an atomic copy in case halInternalSfdCaptureIsr interrupts us
    t = syncCapturedTime;
  )
  return t;
}
