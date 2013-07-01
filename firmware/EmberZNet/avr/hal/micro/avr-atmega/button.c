/** @file hal/micro/avr-atmega/button.c
 *  @brief  Sample API functions for using push-buttons.
 * 
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.       *80*-->   
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"

// This state is kept track of so the ISRs know whether they are looking
//  for rising or falling edges
int8u button0State, button1State;


void halInternalInitButton(void)
{  
  // Enable Pull-ups 
  BUTTON0_PORT |= BIT(BUTTON0);
  BUTTON1_PORT |= BIT(BUTTON1);
  // Configure for falling edge
  BUTTON0_CONFIG_FALLING_EDGE();
  BUTTON1_CONFIG_FALLING_EDGE();
  // Set default state
  button0State = BUTTON_RELEASED;
  button1State = BUTTON_RELEASED;

  // Enable Interrupts
  EIMSK |= BIT(BUTTON0_INT) | BIT(BUTTON1_INT);
}

int8u halButtonState(int8u button)
{
  // Note: this returns the "soft" state rather than reading the port
  //  so it jives with the interrupts and their callbacks
  switch(button) {
    case BUTTON0:
      return button0State;
    case BUTTON1:
      return button1State;
    default:
      return BUTTON_RELEASED;
  }
}

int8u halButtonPinState(int8u button)
{
  // Note: this returns the current state of the button's pin.  It may not
  // jive with the interrupts and their callbacks, but it is useful for
  // checking the state of the button on startup.
  switch(button) {
    case BUTTON0:
      return (BUTTON0_PIN & BIT(BUTTON0)) ? BUTTON_RELEASED : BUTTON_PRESSED;
    case BUTTON1:
      return (BUTTON1_PIN & BIT(BUTTON1)) ? BUTTON_RELEASED : BUTTON_PRESSED;
    default:
      return BUTTON_RELEASED;
  }
}

#pragma vector = BUTTON0_INT_VECT
__interrupt void but0Interrupt(void)
{
  INT_DEBUG_BEG_MISC_INT();
  if(button0State == BUTTON_RELEASED) {
    button0State = BUTTON_PRESSED;
    halButtonIsr(BUTTON0, BUTTON_PRESSED);

    // switch Interrupt to trigger on rising edge (release)
    BUTTON0_CONFIG_RISING_EDGE();
    
    // check see if it was released in the meantime, meaning we won't
    //   get the release interrupt
    if(BUTTON0_PIN & BIT(BUTTON0)) {
      // reconfigure back to falling edge (press)
      BUTTON0_CONFIG_FALLING_EDGE();
      
      // clear interrupt flag just in case it did get triggered in the short
      //  period between when we set up the interrupt and tested for release
      EIFR = BIT(BUTTON0_INT);

      // finally, call the released callback since we won't get the interrupt
      button0State = BUTTON_RELEASED;
      halButtonIsr(BUTTON0, BUTTON_RELEASED);
    }
  } else {
    button0State = BUTTON_RELEASED;
    halButtonIsr(BUTTON0, BUTTON_RELEASED);

    // reconfigure back to falling edge (press)
    BUTTON0_CONFIG_FALLING_EDGE();
  }
  INT_DEBUG_END_MISC_INT();
}


#pragma vector = BUTTON1_INT_VECT
__interrupt void but1Interrupt(void)
{
  INT_DEBUG_BEG_MISC_INT();
  if(button1State == BUTTON_RELEASED) {
    button1State = BUTTON_PRESSED;
    halButtonIsr(BUTTON1, BUTTON_PRESSED);

    // switch Interrupt to trigger on rising edge (release)
    BUTTON1_CONFIG_RISING_EDGE();
    
    // check see if it was released in the meantime, meaning we won't
    //   get the release interrupt
    if(BUTTON1_PIN & BIT(BUTTON1)) {
      // reconfigure back to falling edge (press)
      BUTTON1_CONFIG_FALLING_EDGE();
      
      // clear interrupt flag just in case it did get triggered in the short
      //  period between when we set up the interrupt and tested for release
      EIFR = BIT(BUTTON1_INT);

      // finally, call the released callback since we won't get the interrupt
      button1State = BUTTON_RELEASED;
      halButtonIsr(BUTTON1, BUTTON_RELEASED);
    }
  } else {
    button1State = BUTTON_RELEASED;
    halButtonIsr(BUTTON1, BUTTON_RELEASED);

    // reconfigure back to falling edge (press)
    BUTTON1_CONFIG_FALLING_EDGE();
  }
  INT_DEBUG_END_MISC_INT();
}

