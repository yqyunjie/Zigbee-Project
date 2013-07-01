/** @file hal/micro/micro.h
 * 
 * @brief Functions common across all microcontroller-specific files.
 * See @ref micro for documentation.
 *
 * Some functions in this file return an ::EmberStatus value. 
 * See error-def.h for definitions of all ::EmberStatus return values.
 *
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

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
#elif defined(EZSP_HOST)
  #include "hal/micro/unix/host/micro.h"
#else
  #error no platform or micro defined
#endif

#ifndef EZSP_HOST
// If we are not a Host processor, Define distinct literal values for
// each platform and micro.  The Host processor is not tied to a
// specific platform or micro since it uses EZSP.  These values are
// used in the bootloader query response message.

// PLAT 1 is the AVR ATMega
// for PLAT 1, MICRO 1 is the AVR ATMega 64
// for PLAT 1, MICRO 2 is the AVR ATMega 128
// for PLAT 1, MICRO 3 is the AVR ATMega 32

// PLAT 2 is the XAP2b
// for PLAT 2, MICRO 1 is the em250
// for PLAT 2, MICRO 2 is the em260

#if defined( EMBER_TEST )
  #define PLAT 1
  #define MICRO 2
#elif defined (AVR_ATMEGA)
  #define PLAT 1
  #if defined (AVR_ATMEGA_64)
    #define MICRO 1
  #elif defined (AVR_ATMEGA_128)
    #define MICRO 2
  #elif defined (AVR_ATMEGA_32)
    #define MICRO 3
  #endif
#elif defined (XAP2B)
  #define PLAT 2
  #if defined (XAP2B_EM250)
    #define MICRO 1
  #elif defined (XAP2B_EM260)
    #define MICRO 2
  #endif
#elif defined (MSP430)
  #define PLAT 3
  #if defined (MSP430_1612)
    #define MICRO 1
  #elif defined (MSP430_1471)
    #define MICRO 2
  #endif
#else
  #error no platform defined, or unsupported
#endif

#ifndef MICRO
  #error no micro defined, or unsupported
#endif

#endif //EZSP_HOST

#ifndef EZSP_HOST
// If we are not a Host processor, define distinct literal values for each phy.
// The Host processor is not tied to a specific PHY since it uses EZSP.
// These values are used in the bootloader query response message.
// PHY 1 is the em2420
// PHY 2 is the em250

#if defined (PHY_EM2420) || defined (PHY_EM2420B)
  #define PHY 1
#elif defined (PHY_EM250) || defined (PHY_EM250B)
  #define PHY 2
#else
  #error no phy defined, or unsupported
#endif

#endif //EZSP_HOST

#endif // DOXYGEN_SHOULD_SKIP_THIS


/** @brief Initializes microcontroller-specific peripherals.
*/
void halInit(void);

/** @brief Restarts the microcontroller and therefore everything else.
*/
void halReboot(void);

/** @brief Powers up microcontroller peripherals and board peripherals.
*/
void halPowerUp(void);

/** @brief Powers down microcontroller peripherals and board peripherals.
*/
void halPowerDown(void);

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
 * This information is provided to the stack when it is initialized.
 *  
 * @appusage Call this function as soon after reset as possible, to ensure 
 * that the proper information is returned.
 * 
 * @return An enumeration defined in ember.h.
 */
EmberResetType halGetResetInfo(void);

/** @brief Calls ::halGetResetInfo() and supplies a string describing it.
 *
 * @appusage Useful for diagnostic printing of text just after program 
 * initialization.
 *
 * @return A pointer to a program space string.
 */
PGM_P halGetResetString(void);

/** @brief The value that must be passed as the single parameter to 
 *  ::halInternalDisableWatchDog() in order to sucessfully disable the watchdog 
 *  timer.
 */ 
#define MICRO_DISABLE_WATCH_DOG_KEY 0xA5

/** @brief Enables the watchdog timer.
 */
void halInternalEnableWatchDog(void);

/** @brief Disables the watchdog timer.
 *
 * @note To prevent the watchdog from being disabled accidentally, 
 * a magic key must be provided.
 * 
 * @param magicKey  A value (::MICRO_DISABLE_WATCH_DOG_KEY) that enables the function.
 */
void halInternalDisableWatchDog(int8u magicKey);

/** @brief Determines whether the watchdog has been enabled or disabled.
 */
boolean halInternalWatchDogEnabled( void );


#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Enumerations for the possible microcontroller sleep modes. 
 */
enum SleepModes
#else
typedef int8u SleepModes;
enum
#endif
{
   SLEEPMODE_IDLE,
   SLEEPMODE_RESERVED,
   SLEEPMODE_POWERDOWN,
   SLEEPMODE_POWERSAVE
};

/** @brief Puts the microcontroller to sleep in a specified mode.
 *
 * @note This routine always enables interrupts.
 *
 * @param sleepMode  A microcontroller sleep mode 
 * 
 * @sa ::SleepModes
 */
void halSleep(SleepModes sleepMode);


/** @name Time Support Functions
 *@{
 */

/** @brief Blocks the current thread of execution for the specified
 * amount of time, in microseconds.
 *
 * The function is implemented with cycle-counted busy loops
 * and is intended to create the short delays required when interfacing with
 * hardware peripherals.
 *
 * @param us  The specified time, in microseconds. 
              Values should be between 1 and 32767 microseconds.
 */
void halCommonDelayMicroseconds(int16u us);

/** @} END Time Support Functions  */


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


// the number of ticks (as returned from halCommonGetInt32uMillisecondTick)
// that represent an actual second. This can vary on different platforms.
// Provide a default here in case the platform micro.h doesnt provide one.
#ifndef MILLISECOND_TICKS_PER_SECOND
  #define MILLISECOND_TICKS_PER_SECOND 1024
#endif


#endif //__MICRO_H__

/** @} END micro group  */
  

