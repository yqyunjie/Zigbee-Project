/** @file hal/micro/system-timer.h
 * See @ref system_timer for documentation.
 * 
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup system_timer
 * @brief Functions that provide access to the system clock.
 * See system-timer.h for source code.
 *
 * A single system tick (as returned by ::halCommonGetInt16uMillisecondTick() and
 * ::halCommonGetInt32uMillisecondTick() ) is approximately 1 millisecond.
 *
 * - When used with a 32.768kHz crystal, the system tick is 0.976 milliseconds.
 *
 * - When used with a 3.6864MHz crystal, the system tick is 1.111 milliseconds.
 *
 * A single quarter-second tick (as returned by ::halCommonGetInt16uQuarterSecondTick() )
 * is approximately 0.25 seconds.
 *
 * The values used by the time support functions will wrap after an interval.
 * The length of the interval depends on the length of the tick and the number
 * of bits in the value. However, there is no issue when comparing time deltas
 * of less than half this interval with a subtraction, if all data types are the
 * same.
 *
 *@{
 */

#ifndef __SYSTEM_TIMER_H__
#define __SYSTEM_TIMER_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#if defined( EMBER_TEST )
  #include "unix/simulation/system-timer-sim.h"
#elif defined(AVR_ATMEGA_32)
  #include "avr-atmega/32/system-timer.h"
#elif defined(AVR_ATMEGA_128)
  #include "avr-atmega/128/system-timer.h"
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS


/**
 * @brief Initializes the system tick.
 *
 * @return Time to update the async registers after RTC is started (units of 100 
 * microseconds).
 */
int16u halInternalStartSystemTimer(void);


/**
 * @brief Returns the current system time in system ticks, as a 16-bit
 * value.
 *
 * @return The least significant 16 bits of the current system time, in system
 * ticks.
 */
#pragma pagezero_on  // place this function in zero-page memory for xap 
int16u halCommonGetInt16uMillisecondTick(void);
#pragma pagezero_off

/**
 * @brief Returns the current system time in system ticks, as a 32-bit
 * value.
 *
 * @nostackusage
 *
 * @return The least significant 32 bits of the current system time, in 
 * system ticks.
 */
int32u halCommonGetInt32uMillisecondTick(void);

/**
 * @brief Returns the current system time in quarter second ticks, as a
 * 16-bit value.
 *
 * @nostackusage
 *
 * @return The least significant 16 bits of the current system time, in system
 * ticks multiplied by 256.
 */
int16u halCommonGetInt16uQuarterSecondTick(void);

/**
 * @brief Uses the system timer to enter ::SLEEPMODE_POWERSAVE for
 * approximately the specified amount of time (provided in quarter seconds).
 *
 * This function returns EMBER_SUCCESS and the duration parameter is
 * decremented to 0 after sleeping for the specified amount of time.  If an
 * interrupt occurs that brings the chip out of sleep, the function returns
 * ::EMBER_SLEEP_INTERRUPTED and the duration parameter reports the amount of
 * time remaining out of the original request.
 *
 * @note This routine always enables interrupts.
 *
 * @note The maximum sleep time of the hardware is limited on AVR-based
 * platforms to 8 seconds, and on EM250-based platforms to 64 seconds.
 * Any sleep duration greater than this limit will wake up briefly (e.g.
 * 16 microseconds) to reenable another sleep cycle.
 *
 * @nostackusage
 *
 * @param duration The amount of time, expressed in quarter seconds, that the
 * micro should be placed into ::SLEEPMODE_POWERSAVE.  When the function returns,
 * this parameter provides the amount of time remaining out of the original
 * sleep time request (normally the return value will be 0).
 * 
 * @return An EmberStatus value indicating the success or
 *  failure of the command.
 */
EmberStatus halSleepForQuarterSeconds(int32u *duration);

/**
 * @brief Uses the system timer to enter ::SLEEPMODE_IDLE for
 * approximately the specified amount of time (provided in milliseconds).
 *
 * This function returns ::EMBER_SUCCESS and the duration parameter is
 * decremented to 0 after idling for the specified amount of time.  If an
 * interrupt occurs that brings the chip out of idle, the function returns
 * ::EMBER_SLEEP_INTERRUPTED and the duration parameter reports the amount of
 * time remaining out of the original request.
 *
 * @note This routine always enables interrupts.
 *
 * @nostackusage
 *
 * @param duration  The amount of time, expressed in milliseconds, that the
 * micro should be placed into ::SLEEPMODE_IDLE.  When the function returns,
 * this parameter provides the amount of time remaining out of the original
 * idle time request (normally the return value will be 0).
 *
 * @return An EmberStatus value indicating the success or
 *  failure of the command.
 */
EmberStatus halIdleForMilliseconds(int32u *duration);


#endif //__SYSTEM_TIMER_H__

/**@} //END addtogroup 
 */




