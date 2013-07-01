/*
 * File: em2420.c
 * Description: Hal support functions for the em2420 radio
 *
 * Reminder: The term SFD refers to the Start Frame Delimiter.
 *
 * Author(s): Lee Taylor, lee@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <inavr.h>
#include <ioavr.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/micro/em2420.h"

#include "hal/hal.h" 

extern boolean emRadioNeedsRebooting;


void halStack2420EnableInterrupts(void)
{
  // Rising edge trigger
  //  EICRB |= (BIT(ISC40)|BIT(ISC41));
  // Low level trigger
  EICRB &= ~(BIT(ISC40)|BIT(ISC41));
  halStack2420EnableRxInterrupt();

  // Rising edge input capture
  TCCR1B |= (BIT(ICES1));
  // Enable input capture interrupt (SFD)
  TIMSK |= (BIT(TICIE1));
  // Clear the flag by writing one
  TIFR = BIT(ICF1);
}

void halStack2420EnableRxInterrupt(void)
{
  // Enable INT4 (FIFOP)
  EIMSK |= (BIT(INT4));
  // Clear the flag by writing one
  EIFR = BIT(INT4); 
}

void halStack2420DisableInterrupts(void)
{
  halStack2420DisableRxInterrupt();

  // Disable input capture interrupt (SFD)
  TIMSK &= ~(BIT(TICIE1));
  // Clear the flag by writing one
  TIFR = BIT(ICF1);
}

void halStack2420DisableRxInterrupt(void)
{
  // Disable INT4 (FIFOP)
  EIMSK &= ~(BIT(INT4));
  // Clear the flag by writing one
  EIFR = BIT(INT4);
}

#ifndef BOOTLOADER

#pragma vector = INT4_vect
__interrupt void radioReceiveInterrupt(void)
{
  // FIFOP can legitimately be asserted at the end of the ISR if
  // packets are received in rapid succession. We use a counter to
  // distinguish this condition from a "stuck" radio.
  static int8u previousConsecutivePackets = 0;
  emberRadioReceiveIsr();

  if ((RAD_FIFO_PIN & BIT(RAD_FIFOP)) == 0) {
    if (previousConsecutivePackets == 2) {
      // We don't reboot the radio here because it takes too much
      // time (62 mS reported by Wally). Set flag instead.
      emRadioNeedsRebooting = TRUE;
      previousConsecutivePackets = 0;
    } else {
      previousConsecutivePackets += 1;
    }
  } else {
    previousConsecutivePackets = 0;
  }
}

#pragma vector = TIMER1_CAPT_vect
__interrupt void radioSfdInterrupt(void)
{
  INT_DEBUG_BEG_MISC_INT();
  halInternalSfdCaptureIsr();
  INT_DEBUG_END_MISC_INT();
}

#endif // !BOOTLOADER
