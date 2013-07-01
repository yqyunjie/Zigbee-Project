/** @file hal/micro/avr-atmega/led.c
 *  @brief  Sample API funtions for controlling LEDs
 *  
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.       *80*-->   
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"

void halInternalInitLed(void)
{
  // Initialize the LEDs ports for output 
  LED_DDR |= BOARDLED_MASK;
  // turn the LEDs off
  LED_PORT |= BOARDLED_MASK;
}

void halToggleLed (HalBoardLed led)
{
  // c lanuguage xor on an i/o port doesn't make avr machine code 
  // which is atomic. so to avoid contention with other code using
  // the other pins for other purposes, we disable interrupts to
  // make the read io register, xor and write io atomic.
  
  // make sure the bit specified is a valid led
  if( (BOARDLED_MASK & BIT(led)) != 0 )
  {
    ATOMIC_LITE(
      LED_PORT ^= BIT(led);
    )
  }
}

void halSetLed (HalBoardLed led)
{
  // this should all compile to an atomic assembly instruction
  if( (BOARDLED_MASK & BIT(led)) != 0 )
  {
      LED_PORT &= ~BIT(led);
  }
}

void halClearLed (HalBoardLed led)
{
  // this should all compile to an atomic assembly instruction  
  if( (BOARDLED_MASK & BIT(led)) != 0 )
  {
      LED_PORT |= BIT(led);
  }
}

void halStackIndicateActivity(boolean turnOn)
{
  if(turnOn) {
    halSetLed(BOARD_ACTIVITY_LED);
  } else {
    halClearLed(BOARD_ACTIVITY_LED);
  }  
}


