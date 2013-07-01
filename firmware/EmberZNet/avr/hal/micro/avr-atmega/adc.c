/*
 * File: adc.c
 * Description: A/D converter driver for multiple users
 *  for the ATMega128 single-ended inputs
 *
 * Author(s): Andy Wheeler, andy@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */
#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/micro/adc.h"
#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>

#include "hal/hal.h"
#include "stack/include/error.h"

#if (NUM_ADC_USERS > 8)
#error NUM_ADC_USERS must not be greater than 8, or variables in adc.c must be changed
#endif
volatile static int8u pendingRequests;     // bitmap of pending requests
volatile static int8u pendingConversion;   // id of pending conversion
static int8u channel[NUM_ADC_USERS];
volatile static int8u readings[NUM_ADC_USERS];
volatile static int8u readingValid;        // bitmap of valid readings

#define CHANNEL_MASK 0x1F
#define REF_MASK 0xC0
#define LEFT_ADJUST_BIT 0x20

// interrupt code for ADC conversion complete
// copies out the data and starts up the next conversion if any
void halAdcIsr(void)
{
  int8u i;
  int8u conversion = pendingConversion; //fix 'volatile' warning; costs no flash

  // copy out the value
  if (conversion < NUM_ADC_USERS) {
    readings[conversion] = ADCH;
    readingValid |= BIT(conversion);
  }
  
  // setup the next conversion if any
  if (pendingRequests) {
    for (i=0;i<NUM_ADC_USERS;i++) {
      if ((pendingRequests>>i) & 1) {
        // set pending conversion
        pendingConversion = i;
        // clear pending request
        pendingRequests &= ~BIT(i);
        // set channel - bits 4:0, keep 7:5 the same
        ADMUX = channel[i];
        // start sampling
        ADCSR |= BIT(ADSC);
        break;
      }
    }
  } else {
    pendingConversion = NUM_ADC_USERS;
  }
}

// interrupt handler for ADC conversion complete
// Uses common code halAdcIsr
#pragma vector = ADC_vect                        
__interrupt void adcInterrupt(void)
{
  INT_DEBUG_BEG_MISC_INT();
  halAdcIsr();
  INT_DEBUG_END_MISC_INT();
}

// returns the user number of the started conversion, or
// NUM_ADC_USERS otherwise 
int8u startNextConversion() {
  int8u i;
  DECLARE_INTERRUPT_STATE;
  DISABLE_INTERRUPTS();
  // start the next requested conversion if any
  
  if (pendingRequests && !(ADCSR & BIT(ADSC))) {
    for (i=0;i<NUM_ADC_USERS;i++) {
      if ((pendingRequests>>i) & 1) {
        // set pending conversion
        pendingConversion = i;
        // clear pending request
        pendingRequests &= ~BIT(i);
        // set channel - bits 4:0, keep 7:5 the same
        ADMUX = channel[i];
        // start sampling
        ADCSR |= BIT(ADSC);
        RESTORE_INTERRUPTS();
        return i;
      }
    }
  }

  RESTORE_INTERRUPTS();
  return NUM_ADC_USERS;;
}

void halInternalInitAdc(void)
{
  // reset the state variables
  pendingRequests = 0;
  pendingConversion = NUM_ADC_USERS;
  
  // set all readings as invalid
  readingValid = 0;

  // setup the ADC for internal 2.56V operation
//  ADMUX |= BIT(REFS1)|BIT(REFS0);
 
  // setup the prescaler - this varies between boards
  // due to clock frequency, and should therefore
  // be parameterized somehow
  ADCSR = BIT(ADPS2)|BIT(ADPS0);  // div factor = 32

  // clear the ADC interrupts and enable
  ADCSR |= BIT(ADIF);
  ADCSR |= BIT(ADIE);

  // enable the ADC
  ADCSR |= BIT(ADEN);
}

void halInternalSleepAdc(void)
{
  // turn off the ADC
  ADCSR = 0;
  // deselect the reference to disable it
  ADMUX = 0;
}

EmberStatus halStartAdcConversion(ADCUser id,
                                  ADCReferenceType reference,
                                  ADCChannelType reqChannel,
                                  ADCRateType rate)
{

  if (rate != ADC_CONVERSION_TIME_US_256)
    return EMBER_ERR_FATAL;

  channel[id] = (reqChannel & CHANNEL_MASK) | (reference & REF_MASK) | 
    LEFT_ADJUST_BIT;
  
  // if the user already has a pending request, overwrite params
  if (pendingRequests & BIT(id)) {
    return EMBER_ADC_CONVERSION_DEFERRED;
  }
  
  // otherwise, queue the transaction
  // NOTE- order variables is set is important!!
  pendingRequests |= BIT(id);
    
  // try and start the conversion if there is not one happening
  readingValid &= ~BIT(id);
  if (startNextConversion() == id)
    return EMBER_ADC_CONVERSION_BUSY;
  else
    return EMBER_ADC_CONVERSION_DEFERRED;
}

EmberStatus halRequestAdcData(ADCUser id, int16u *value)
{
  boolean intsAreOff = INTERRUPTS_ARE_OFF();
  DECLARE_INTERRUPT_STATE;

  DISABLE_INTERRUPTS();
  
  // If interupts are disabled but the flag is set,
  // manually run the isr...
  if (intsAreOff && (ADCSR & BIT(ADIF))) {
    halAdcIsr();
    ADCSR |= BIT(ADIF);         // clear interupt flag
  }

  // check if we are done
  if (BIT(id) & readingValid) {
    *value = (int16u)readings[id];
    readingValid &= ~BIT(id);
    RESTORE_INTERRUPTS();
    return EMBER_ADC_CONVERSION_DONE;
  } else if (pendingConversion == id) {
    RESTORE_INTERRUPTS();
    return EMBER_ADC_CONVERSION_BUSY;
  } else if (pendingRequests & BIT(id)) {
    RESTORE_INTERRUPTS();
    return EMBER_ADC_CONVERSION_DEFERRED;
  } else {
    RESTORE_INTERRUPTS();
    return EMBER_ADC_NO_CONVERSION_PENDING;
  }
}

EmberStatus halReadAdcBlocking(ADCUser id, int16u *value)
{
  EmberStatus stat;

  do {
    stat = halRequestAdcData(id, value);
    if (stat == EMBER_ADC_NO_CONVERSION_PENDING)
      break;
  } while (stat != EMBER_ADC_CONVERSION_DONE);

  return stat;
}

EmberStatus halAdcCalibrate(ADCUser id)
{
  return EMBER_ADC_CONVERSION_DONE;
}

int16s halConvertValueToVolts(int16u data)
{
  // v = (d * 2.56) / 2^8
  return (int16s)data * 100;
}
