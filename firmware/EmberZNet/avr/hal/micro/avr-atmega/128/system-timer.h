/** @file hal/micro/avr-atmega/128/system-timer.h
 * 
 * @brief Functions that provide access to the system clock for
 * ATmega128 microcontroller.
 *
 * See @ref system_timer for documentation.
 *
 * <!-- Copyright 2006 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup system_timer
 * 
 * See also 128/system-timer.c for source code.
 *
 *@{
 */

#ifndef __128_SYSTEM_TIMER_H__
#define __128_SYSTEM_TIMER_H__

#ifndef __SYSTEM_TIMER_H__
#error do not include this file directly - include micro/system-timer.h
#endif

/** @brief Set the current system time
 *
 * @param time  A 32 bit value, expressed in milliseconds, that will
 * become the current system time.
 */
void halCommonSetSystemTime(int32u time);

#endif //__128_SYSTEM_TIMER_H__

/** @} END addtogroup  */
