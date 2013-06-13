/** @file hal/micro/micro.h
 * 
 * @brief Full HAL functions common across all microcontroller-specific files.
 * See @ref micro for documentation.
 *
 * Some functions in this file return an ::EmberStatus value. 
 * See error-def.h for definitions of all ::EmberStatus return values.
 *
 * <!-- Copyright 2004-2011 by Ember Corporation. All rights reserved.   *80*-->
 */
 
/** @addtogroup micro
 * Many of the supplied example applications use these microcontroller functions.
 * See hal/micro/micro.h for source code.
 *
 * @note The term SFD refers to the Start Frame Delimiter.
 *@{
 */

#ifndef __MICRO_H__
#define __MICRO_H__

#include "hal/micro/generic/em2xx-reset-defs.h"
#include "hal/micro/micro-types.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Make sure that a proper plat/micro combination was selected if we aren't
// building for a host processor
#if ((! defined(EZSP_HOST)) && (! defined(UNIX_HOST)))

#ifndef PLAT
  #error no platform defined, or unsupported
#endif
#ifndef MICRO
  #error no micro defined, or unsupported
#endif

#endif // ((! defined(EZSP_HOST)) && (! defined(UNIX_HOST)))

// Make sure that a proper phy was selected
#ifndef PHY
  #error no phy defined, or unsupported
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS

/** @brief Called from emberInit and provides a means for the HAL
 * to increment a boot counter, most commonly in non-volatile memory.
 *
 * This is useful while debugging to determine the number of resets that might
 * be seen over a period of time.  Exposing this functionality allows the
 * application to disable or alter processing of the boot counter if, for
 * example, the application is expecting a lot of resets that could wear
 * out non-volatile storage or some
 *
 * @stackusage Called from emberInit only as helpful debugging information.
 * This should be left enabled by default, but this function can also be
 * reduced to a simple return statement if boot counting is not desired.
 */
void halStackProcessBootCount(void);

/** @brief Gets information about what caused the microcontroller to reset. 
 *
 * @return A code identifying the cause of the reset.
 */
int8u halGetResetInfo(void);

/** @brief Calls ::halGetResetInfo() and supplies a string describing it.
 *
 * @appusage Useful for diagnostic printing of text just after program 
 * initialization.
 *
 * @return A pointer to a program space string.
 */
PGM_P halGetResetString(void);

/** @brief Calls ::halGetExtendedResetInfo() and translates the EM35x reset 
 *  code to the corresponding value used by the EM2XX HAL. EM35x reset codes 
 * not present in the EM2XX are returned after being OR'ed with 0x80.
 *
 * @appusage Used by the EZSP host as a platform-independent NCP reset code.
 *
 * @return The EM2XX reset code, or a new EM3xx code if B7 is set.
 */
#ifdef CORTEXM3
  int8u halGetEm2xxResetInfo(void);
#else
  #define halGetEm2xxResetInfo() halGetResetInfo()
#endif


/** @brief Called implicitly through the standard C language assert() macro. 
 * An implementation where notification is, for instance, sent over the serial port
 * can provide meaningful and useful debugging information. 
 * @note Liberal usage of assert() consumes flash space.
 * 
 * @param filename   Name of the file throwing the assert.
 * 
 * @param linenumber Line number that threw the assert.
 */
void halInternalAssertFailed(PGM_P filename, int linenumber);


#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include "micro-common.h"

#if defined( EMBER_TEST )
  #include "hal/micro/unix/simulation/micro.h"
  #include "hal/micro/unix/simulation/bootloader.h"
#elif defined(AVR_ATMEGA)
  #if defined(AVR_ATMEGA_64) || defined(AVR_ATMEGA_128)
    #include "avr-atmega/128/micro.h"
    #include "avr-atmega/128/bootloader.h"
  #elif defined(AVR_ATMEGA_32)
    #include "avr-atmega/32/micro.h"
  #else
    // default, assume 128
    #include "avr-atmega/128/micro.h"
    #include "avr-atmega/128/bootloader.h"
  #endif
#elif defined(MSP430_1612)
  #include "msp430/1612/micro.h"
  #include "msp430/1612/bootloader.h"
#elif defined(MSP430_1471)
  #include "msp430/1471/micro.h"
  #include "msp430/1471/bootloader.h"
#elif defined(XAP2B)
  #include "xap2b/em250/micro.h"
#elif defined(CORTEXM3)
  #include "cortexm3/micro.h"
#elif defined(C8051)
  #include "c8051/micro.h"
#elif ((defined(EZSP_HOST) || defined(UNIX_HOST)))
  #include "hal/micro/unix/host/micro.h"
#else
  #error no platform or micro defined
#endif

// the number of ticks (as returned from halCommonGetInt32uMillisecondTick)
// that represent an actual second. This can vary on different platforms.
// It must be defined by the host system.
#ifndef MILLISECOND_TICKS_PER_SECOND
  #define MILLISECOND_TICKS_PER_SECOND 1024UL
// See bug 10232
//  #error "MILLISECOND_TICKS_PER_SECOND is not defined in micro.h!"
#endif

#endif // DOXYGEN_SHOULD_SKIP_THIS

#endif //__MICRO_H__

/** @} END micro group  */
  

