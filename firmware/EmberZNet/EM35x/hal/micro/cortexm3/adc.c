/*
 * File: adc.c
 * Description: EM3XX-specific ADC HAL functions
 *
 *
 * Copyright 2008 by Ember Corporation. All rights reserved.                *80*
 */
#include PLATFORM_HEADER
#include "stack/include/error.h"
#include "hal/micro/micro-common.h"
#include "hal/micro/cortexm3/micro-common.h"
#include "micro/adc.h"

#if (NUM_ADC_USERS > 8)
  #error NUM_ADC_USERS must not be greater than 8, or int8u variables in adc.c must be changed
#endif

static int16u adcData;             // conversion result written by DMA
static int8u adcPendingRequests;   // bitmap of pending requests
volatile static int8u adcPendingConversion; // id of pending conversion
static int8u adcReadingValid;      // bitmap of valid adcReadings
static int16u adcReadings[NUM_ADC_USERS];
static int16u adcConfig[NUM_ADC_USERS];
static boolean adcCalibrated;
static int16s Nvss;
static int16s Nvdd;
static int16u adcStaticConfig;

void halAdcSetClock(boolean slow)
{
  if (slow) {
    adcStaticConfig |= ADC_1MHZCLK_MASK;
  } else {
    adcStaticConfig &= ~ADC_1MHZCLK_MASK;
  }
}

boolean halAdcGetClock(void)
{
  return (boolean)(adcStaticConfig & ADC_1MHZCLK_MASK);
}


// Define a channel field that combines ADC_MUXP and ADC_MUXN
#define ADC_CHAN        (ADC_MUXP | ADC_MUXN)
#define ADC_CHAN_BIT    ADC_MUXN_BIT

void halAdcIsr(void)
{
  int8u i;
  int8u conversion = adcPendingConversion; //fix 'volatile' warning; costs no flash

  // make sure data is ready and the desired conversion is valid
  if ( (INT_ADCFLAG & INT_ADCULDFULL)
        && (conversion < NUM_ADC_USERS) ) {
    adcReadings[conversion] = adcData;
    adcReadingValid |= BIT(conversion); // mark the reading as valid
    // setup the next conversion if any
    if (adcPendingRequests) {
      for (i = 0; i < NUM_ADC_USERS; i++) {
        if (BIT(i) & adcPendingRequests) {
          adcPendingConversion = i;     // set pending conversion
          adcPendingRequests ^= BIT(i); //clear request: conversion is starting
          ADC_CFG = adcConfig[i];
          break; //conversion started, so we're done here (only one at a time)
        }
      }
    } else {                                // no conversion to do
      ADC_CFG = 0;                          // disable adc
      adcPendingConversion = NUM_ADC_USERS; //nothing pending, so go "idle"
    }
  }
  INT_ADCFLAG = 0xFFFF;
  asm("DMB");
}

// An internal support routine called from functions below.
// Returns the user number of the started conversion, or NUM_ADC_USERS
// otherwise.
ADCUser startNextConversion(void)
{
  int8u i;

  ATOMIC (
    // start the next requested conversion if any
    if (adcPendingRequests && !(ADC_CFG & ADC_ENABLE)) {
      for (i = 0; i < NUM_ADC_USERS; i++) {
        if ( BIT(i) & adcPendingRequests) {
          adcPendingConversion = i;     // set pending conversion
          adcPendingRequests ^= BIT(i); // clear request
          ADC_CFG = adcConfig[i];       // set the configuration to desired
          INT_ADCFLAG = 0xFFFF;
          INT_CFGSET = INT_ADC;
          break;
        }
      }
    } else {
      i = NUM_ADC_USERS;
    }
  )
  return i;
}

void halInternalInitAdc(void)
{
  // reset the state variables
  adcPendingRequests = 0;
  adcPendingConversion = NUM_ADC_USERS;
  adcCalibrated = FALSE;
  adcStaticConfig = ADC_1MHZCLK | ADC_ENABLE; // init config: 1MHz, low voltage

  // set all adcReadings as invalid
  adcReadingValid = 0;

  // turn off the ADC
  ADC_CFG = 0;                   // disable ADC, turn off HV buffers
  ADC_OFFSET = ADC_OFFSET_RESET;
  ADC_GAIN = ADC_GAIN_RESET;
  ADC_DMACFG = ADC_DMARST;
  ADC_DMABEG = (int32u)&adcData;
  ADC_DMASIZE = 1;
  ADC_DMACFG = (ADC_DMAAUTOWRAP | ADC_DMALOAD);

  // clear the ADC interrupts and enable
  INT_ADCCFG = INT_ADCULDFULL;
  INT_ADCFLAG = 0xFFFF;
  INT_CFGSET = INT_ADC;

  emberCalibrateVref();
}

EmberStatus halStartAdcConversion(ADCUser id,
                                  ADCReferenceType reference,
                                  ADCChannelType channel,
                                  ADCRateType rate)
{

   if(reference != ADC_REF_INT)
    return EMBER_ERR_FATAL;

  // save the chosen configuration for this user
  adcConfig[id] = ( ((rate << ADC_PERIOD_BIT) & ADC_PERIOD)
                  | ((channel << ADC_CHAN_BIT) & ADC_CHAN)
                  | adcStaticConfig);

  // if the user already has a pending request, overwrite params
  if (adcPendingRequests & BIT(id)) {
    return EMBER_ADC_CONVERSION_DEFERRED;
  }

  ATOMIC (
    // otherwise, queue the transaction
    adcPendingRequests |= BIT(id);
    // try and start the conversion if there is not one happening
    adcReadingValid &= ~BIT(id);
  )
  if (startNextConversion() == id)
    return EMBER_ADC_CONVERSION_BUSY;
  else
    return EMBER_ADC_CONVERSION_DEFERRED;
}

EmberStatus halRequestAdcData(ADCUser id, int16u *value)
{
  //Both the ADC interrupt and the global interrupt need to be enabled,
  //otherwise the ADC ISR cannot be serviced.
  boolean intsAreOff = ( INTERRUPTS_ARE_OFF()
                        || !(INT_CFGSET & INT_ADC)
                        || !(INT_ADCCFG & INT_ADCULDFULL) );
  EmberStatus stat;

  ATOMIC (
    // If interupts are disabled but the flag is set,
    // manually run the isr...
    //FIXME -= is this valid???
    if( intsAreOff
      && ( (INT_CFGSET & INT_ADC) && (INT_ADCCFG & INT_ADCULDFULL) )) {
      halAdcIsr();
    }

    // check if we are done
    if (BIT(id) & adcReadingValid) {
      *value = adcReadings[id];
      adcReadingValid ^= BIT(id);
      stat = EMBER_ADC_CONVERSION_DONE;
    } else if (adcPendingRequests & BIT(id)) {
      stat = EMBER_ADC_CONVERSION_DEFERRED;
    } else if (adcPendingConversion == id) {
      stat = EMBER_ADC_CONVERSION_BUSY;
    } else {
      stat = EMBER_ADC_NO_CONVERSION_PENDING;
    }
  )
  return stat;
}

EmberStatus halReadAdcBlocking(ADCUser id, int16u *value)
{
  EmberStatus stat;

  do {
    stat = halRequestAdcData(id, value);
    if (stat == EMBER_ADC_NO_CONVERSION_PENDING)
      break;
  } while(stat != EMBER_ADC_CONVERSION_DONE);
  return stat;
}

EmberStatus halAdcCalibrate(ADCUser id)
{
  EmberStatus stat;

  halStartAdcConversion(id,
                        ADC_REF_INT,
                        ADC_SOURCE_GND_VREF2,
                        ADC_CONVERSION_TIME_US_4096);
  stat = halReadAdcBlocking(id, (int16u *)(&Nvss));
  if (stat == EMBER_ADC_CONVERSION_DONE) {
    halStartAdcConversion(id,
                          ADC_REF_INT,
                          ADC_SOURCE_VREG2_VREF2,
                          ADC_CONVERSION_TIME_US_4096);
    stat = halReadAdcBlocking(id, (int16u *)(&Nvdd));
  }
  if (stat == EMBER_ADC_CONVERSION_DONE) {
    Nvdd -= Nvss;
    adcCalibrated = TRUE;
  } else {
    adcCalibrated = FALSE;
    stat = EMBER_ERR_FATAL;
  }
  return stat;
}

// Use the ratio of the sample reading to the of VDD_PADSA/2, a 'known'
// value (nominally 900mV in normal mode, but slightly higher in boost mode)
// to convert to 100uV units.
// FIXME: support external Vref
//        use #define of Vref, ignore VDD_PADSA
//[[ FIXME: support EM350 high voltage range
//        use Vref-Vref/2 to calibrate, or multiply result by 4.
//   (Not supported due to errata)
//]]
int16s halConvertValueToVolts(int16u value)
{
  int32s N;
  int16s V;
  int32s nvalue;

  if (!adcCalibrated) {
    halAdcCalibrate(ADC_USER_LQI);
  }
  if (adcCalibrated) {
    assert(Nvdd);
    nvalue = value - Nvss;
    // Convert input value (minus ground) to a fraction of VDD/2.
    N = ((nvalue << 16) + Nvdd/2) / Nvdd;
    // Calculate voltage with: V = (N * Vreg/2) / (2^16)
    // Mutiplying by Vreg/2*10 makes the result units of 100 uVolts
    // (in fixed point E-4 which allows for 13.5 bits vs millivolts
    // which is only 10.2 bits).
    V = (int16s)((N*((int32s)halInternalGetVreg())*5) >> 16);
  } else {
     V = -32768;
  }
  return V;
}
