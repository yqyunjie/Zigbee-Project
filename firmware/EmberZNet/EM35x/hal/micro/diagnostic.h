/** @file hal/micro/diagnostic.h
 * See @ref diagnostics for detailed documentation.
 *
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */
 
/** @addtogroup diagnostics
 * @brief Program counter diagnostic functions.
 *
 * Unless otherwise noted, the EmberNet stack does not use these functions, 
 * and therefore the HAL is not required to implement them. However, many of
 * the supplied example applications do use them.
 *
 * Use these utilities with microcontrollers whose watchdog circuits do not make 
 * it possible to find out where they were executing when a watchdog was triggered.
 * Enabling the program counter (PC) diagnostics enables a periodic timer 
 * interrupt that saves the current PC to a reserved, unitialized memory address.  
 * This address can be read after a  reset. 
 *
 * Some functions in this file return an EmberStatus value. See 
 * error-def.h for definitions of all EmberStatus return values.
 *
 * See hal/micro/diagnostic.h for source code.
 *
 *@{
 */
 
#ifndef __DIAGNOSTIC_H__
#define __DIAGNOSTIC_H__

/** @brief Starts a timer that initiates sampling the 
 * microcontroller's program counter.
 */
void halStartPCDiagnostics(void);

/** @brief Stops the timer that was started by 
 * halStartPCDiagnostics().
 */
void halStopPCDiagnostics(void);

/** @brief Provides a captured PC location from the last run. 
 *   
 * @appusage Call this function when an application starts, before
 * calling halStartPCDiagnostics().
 *  
 * @return The most recently sampled program counter location. 
 */
int16u halGetPCDiagnostics(void);

#ifdef ENABLE_DATA_LOGGING
// A pair of utility functions for logging trace data
XAP2B_PAGEZERO_ON
void halLogData(int8u v1, int8u v2, int8u v3, int8u v4);
XAP2B_PAGEZERO_OFF
void halPrintLogData(int8u port);
#endif

#ifdef ENABLE_ATOMIC_CLOCK
#if defined(XAP2B)
void halResetAtomicClock(void);
void halStartAtomicClock(int16u intState);
void halStopAtomicClock(int16u intState);
void halPrintAtomicTiming(int8u port, boolean reset);
#elif defined (AVR_ATMEGA)
void halResetAtomicClock(void);
void halStartAtomicClock(void);
void halStopAtomicClock(void);
void halPrintAtomicTiming(int8u port, boolean reset);
#endif
#endif

#endif //__DIAGNOSTIC_H__

/**@} // End addtogroup
 */



