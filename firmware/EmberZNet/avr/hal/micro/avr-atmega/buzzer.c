/** @file hal/micro/avr-atmega/buzzer.c
 *  @brief  Sample API functions for playing tunes on a piezo buzzer
 * 
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.       *80*-->   
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"


//the mega32 uses a 3.6864MHz crystal, everything else uses 8MHz
#ifdef AVR_ATMEGA_32
  #define PRESCALAR_BITS  (BIT(CS22)|BIT(CS20)) //clk/128
#else
  #define PRESCALAR_BITS  (BIT(CS22))           //clk/256
#endif


// Tune state
int8u PGM *currentTune = NULL;
int8u tunePos = 0;
int16u currentDuration = 0;
boolean tuneDone=TRUE;

void halPlayTune_P(int8u PGM *tune, boolean bkg)
{
  TCCR2 = BIT(WGM21) | BIT(COM20); // no clock (timer stopped)
  TIMSK &= ~(BIT(OCIE2));  // No interrupts
  currentTune = tune; 
  OCR2 = tune[0];
  // Magic duration calculation based on frequency
  currentDuration = ((int16u)200*(int16u)tune[1])/(tune[0]/15);
  tunePos = 2;  // First note is already set up
  tuneDone = FALSE;
  ATOMIC_LITE(
    TIMSK |= BIT(OCIE2);  // start the interrupts comin'!
    TCCR2 = BIT(WGM21) | BIT(COM20) | PRESCALAR_BITS; // Enable Timer
  )
  while( (!bkg) && (!tuneDone)) {
    halResetWatchdog();
  }
}


#pragma vector = TIMER2_COMP_vect
__interrupt void tuneDelayInterrupt(void)
{
  INT_DEBUG_BEG_MISC_INT();
  if(currentDuration-- == 0) {
    if(currentTune[tunePos+1]) {
      if(currentTune[tunePos]) {
        // note
        OCR2 = currentTune[tunePos];
        TCCR2 = BIT(WGM21) | BIT(COM20) | PRESCALAR_BITS; // Enable Timer
        // work some magic to determine the duration based upon the frequency
        //  of the note we are currently playing.
        currentDuration = ((int16u)200*(int16u)currentTune[tunePos+1])/(currentTune[tunePos]/15);
      } else {
        // pause
        OCR2 = 0xFF;
        TCCR2 = BIT(WGM21) | PRESCALAR_BITS; // Enable Timer, no pin toggle
        currentDuration = ((int16u)200*(int16u)currentTune[tunePos+1])/(0xFF/15);
      }
      tunePos += 2;
    } else {
      // End tune
      TCCR2 = BIT(WGM21) | BIT(COM20); // no clock (timer stopped)
      TIMSK &= ~(BIT(OCIE2));
      TCCR2 &= ~(BIT(COM20) | BIT(COM21)); // disable the timer override.
      tuneDone = TRUE;
    }
  }
  INT_DEBUG_END_MISC_INT();
}

int8u PGM hereIamTune[] = {
  NOTE_B4,  1,
  0,        1,
  NOTE_B4,  1,
  0,        1,
  NOTE_B4,  1,
  0,        1,
  NOTE_B5,  5,
  0,        0
};



void halStackIndicatePresence(void)
{
  halPlayTune_P(hereIamTune,TRUE);
}


