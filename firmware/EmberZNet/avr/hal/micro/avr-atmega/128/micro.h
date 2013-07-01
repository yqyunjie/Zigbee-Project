/** @file hal/micro/avr-atmega/128/micro.h
 * @brief Utility and convenience functions for ATmega128 microcontroller
 * 
 *  See also @ref micro.
 *
 * Unless otherwise noted, the EmberNet stack does not use these functions,
 * and therefore the HAL is not required to implement them. However, many of
 * the supplied example applications do use them.
 *
 * @note These HAL files/functions are also used for the ATmega64 
 * microcontroller since the only difference between the ATmega128 and ATmega64 
 * is the size of the flash.
 *
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup micro
 *
 * See also 128/micro.h.
 *
 */


#ifndef __128_MICRO_H__
#define __128_MICRO_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#ifndef __MICRO_H__
#error do not include this file directly - include micro/micro.h
#endif

#include "micro/avr-atmega/128/reserved-ram.h"

////////////
// Serial definitions

// Debug builds use 3 serial ports, one of which is a "virtual" port since the
//  physical port is actually used for the debug channel itself
#if defined(DEBUG) && !defined(STACK)
  #define EM_NUM_SERIAL_PORTS 3
  // Configures a specified serial port to operate in debug mode
  #ifdef EMBER_SERIAL2_DEBUG
    #error serial 2 not valid as a debug port
  #endif
  #if (!(defined(EMBER_SERIAL0_DEBUG))) && \
      (!(defined(EMBER_SERIAL1_DEBUG)))
    #error No valid EMBER_SERIALn_DEBUG definition present
  #endif
  #ifdef EMBER_SERIAL0_DEBUG
    #define EMBER_SERIAL2_MODE EMBER_SERIAL_BUFFER
    #define EMBER_SERIAL2_TX_QUEUE_SIZE 16
    #define EMBER_SERIAL2_RX_QUEUE_SIZE 32 // TBD, what is minimum size?
  #endif
  #ifdef EMBER_SERIAL1_DEBUG
    #define EMBER_SERIAL2_MODE EMBER_SERIAL_BUFFER
    #define EMBER_SERIAL2_TX_QUEUE_SIZE 16
    #define EMBER_SERIAL2_RX_QUEUE_SIZE 32 // TBD, what is minimum size?
  #endif
#else
  #define EM_NUM_SERIAL_PORTS 2
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS


/**
 * @brief Used by the debug stack to trigger the backchannel timer
 * capture circuitry.
 */
void halStackTriggerDebugTimeSyncCapture(void);


#ifndef DOXYGEN_SHOULD_SKIP_THIS

// note: you must actually reserve the ram in low-level-init.c and 
// all the linker scripts. this just defines the addresses
//#define NORMAL_RAM_END             0x10E0 

// ATMega128 / 64 differences

#ifdef AVR_ATMEGA_64
#define ADCSR ADCSRA  // For ATMEGA64, it is called ADCSRA
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS

// the number of ticks (as returned from halCommonGetInt32uMillisecondTick)
// that represent an actual second. This can vary on different platforms.
#define MILLISECOND_TICKS_PER_SECOND 1024

#endif //__128_MICRO_H__
