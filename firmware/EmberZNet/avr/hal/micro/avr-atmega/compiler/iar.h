/** @file iar.h
 * @brief Compiler/platform-specific definitions and typedefs for the
 * IAR AVR C compiler.
 *
 *  See also @ref micro.
 *
 * This file should be included first in all source files by
 * setting the preprocessor macro PLATFORM_HEADER to point to it.
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com-->
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.       *80*-->
 */
 
/** @addtogroup micro
 *
 * See also iar.h.
 *
 */

#ifndef __IAR_H__
#define __IAR_H__

#ifndef __ICCAVR__
  #error Improper PLATFORM_HEADER
#endif


#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #include <inavr.h>
  #define ENABLE_BIT_DEFINITIONS
  #include <ioavr.h>
  #include <stdarg.h>
#endif  // DOXYGEN_SHOULD_SKIP_THIS

// suppress warnings about unknown pragmas
//  (as they may be pragmas known to other platforms)
#pragma diag_suppress = pe161

/** \name Master Variable Types
 * These are a set of typedefs to make the size of all variable declarations
 * explicitly known.
 */
//@{
/**
 * @brief A typedef to make the size of the variable explicitly known.
 */
typedef unsigned char  boolean;
typedef unsigned char  int8u;
typedef signed   char  int8s;
typedef unsigned int   int16u;
typedef signed   int   int16s;
typedef unsigned long  int32u;
typedef signed   long  int32s;
typedef unsigned int  PointerType;
//@} \\END MASTER VARIABLE TYPES


/** \name Master Program Memory Declarations
 * These are a set of defines for simple declarations of program memory.
 */
//@{
#if defined(__HAS_ELPM__) || defined(DOXYGEN_SHOULD_SKIP_THIS)
  /**
   * @brief Standard program memory delcaration.
   */
  #define PGM     __farflash
  
  /**
   * @brief Char pointer to program memory declaration.
   */
  #define PGM_P   const char __farflash *
  
  /**
   * @brief Unsigned char pointer to program memory declaration.
   */
  #define PGM_PU  const unsigned char __farflash *
#else
  #define PGM     __flash
  #define PGM_P   const char __flash *
  #define PGM_PU  const unsigned char __flash *
#endif


/**
 * @brief Sometimes a second PGM is needed in a declaration.  Having two
 * 'const' declarations generates a warning so we have a second PGM that turns
 * into nothing under gcc.
 */
#define PGM_NO_CONST PGM
//@} \\END MASTER PROGRAM MEMORY DECLARATIONS


////////////////////////////////////////////////////////////////////////////////
/** \name Miscellaneous Macros
 */
////////////////////////////////////////////////////////////////////////////////
//@{

/**
 * @brief Define the parameters to main(), and for those functions that
 *   are passed the arguments from main().
 */
#define MAIN_FUNCTION_PARAMETERS void
#define MAIN_FUNCTION_ARGUMENTS  


/**
 * @brief A convenient method for code to know what endiannes processor
 * it is running on.  For the AVR, we are little endian.
 */
#define BIGENDIAN_CPU  FALSE


/**
 * @brief A friendlier name for the compiler's intrinsic for not
 * stripping.
 */
#define NO_STRIPPING  __root


/**
 * @brief A friendlier name for the compiler's intrinsic for eeprom
 * reference.
 */
#define EEPROM __eeprom


#ifndef __SOURCEFILE__
  /**
   * @brief The __SOURCEFILE__ macro is used by asserts to list the
   * filename if it isn't otherwise defined, set it to the compiler intrinsic
   * which specifies the whole filename and path of the sourcefile
   */
  #define __SOURCEFILE__ __FILE__
#endif


#undef assert
#if !defined(SIMPLER_ASSERT_REBOOT) || defined(DOXYGEN_SHOULD_SKIP_THIS)
  /**
   * @brief A prototype definition for use by the assert macro. (see
   * hal/micro/micro.h)
   */
  void halInternalAssertFailed(PGM_P filename, int linenumber);
  
  /**
   * @brief A custom implementation of the C language assert macro.
   * This macro implements the conditional evaluation and calls the function
   * halInternalAssertFailed(). (see hal/micro/micro.h)
   */
  #define assert(condition) \
        do { if (! (condition)) \
             halInternalAssertFailed(__SOURCEFILE__, __LINE__); } while(0)
#else
  #define assert(condition) \
            do { if( !(condition) ) while(1){} } while(0)
#endif


#ifndef BOOTLOADER
  #undef __delay_cycles
  /**
   * @brief __delay_cycles() is an intrinsic IAR WB call; however, we
   * have explicity disallowed it since it is too specific to the system clock.
   * \note Please use halCommonDelayMicroseconds() instead, because it correctly
   * accounts for various system clock speeds.
   */
  #define __delay_cycles(x)  please_use_halCommonDelayMicroseconds_instead_of_delay_cycles
#endif

/**
 * @brief Set debug level based on whether or the DEBUG is defined.
 * For the AVR, no debugging support is included if DEBUG is not defined.
 */
#ifndef DEBUG_LEVEL
  #ifdef DEBUG
    #define DEBUG_LEVEL FULL_DEBUG
  #else
    #define DEBUG_LEVEL NO_DEBUG
  #endif
#endif

/**
 * @brief Macro to reset the watchdog timer.  Note:  be very very
 * careful when using this as you can easily get into an infinite loop if you
 * are not careful.
 */
#define halResetWatchdog()  __watchdog_reset()


/**
 * @brief Define __attribute__ to nothing since it isn't handled by IAR.
 */
#define __attribute__(nothing)


/**
 * @brief Declare a variable as unused to avoid a warning.  Has no effect
 * in IAR builds
 */
#define UNUSED


/**
 * @brief Some platforms need to cast enum values that have the high bit set.
 */
#define SIGNED_ENUM


/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedTimePasses()
/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedTimePassesMs(x)
/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedSerialTimePasses()

////////////////////////////////////////////////////////////////////////////////
//@}  // end of Miscellaneous Macros
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/** \name Global Interrupt Manipulation Macros
 */
////////////////////////////////////////////////////////////////////////////////
//@{
#ifdef BOOTLOADER
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
    // The bootloader does not use interrupts
    #define DECLARE_INTERRUPT_STATE
    #define DISABLE_INTERRUPTS() do { } while(0)
    #define RESTORE_INTERRUPTS() do { } while(0)
    #define INTERRUPTS_ON() do { } while(0)
    #define INTERRUPTS_OFF() do { } while(0)
    #define INTERRUPTS_ARE_OFF() (FALSE)
    #define ATOMIC(blah) { blah }
    #define ATOMIC_LITE(blah) { blah }
    #define HANDLE_PENDING_INTERRUPTS() do { } while(0)
  #endif  // DOXYGEN_SHOULD_SKIP_THIS
#else  // BOOTLOADER
  
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
    // A series of macros for the diagnostics of the global interrupt state.
    // These macros either enable or disable the debugging code in the
    // Interrupt Manipulation Macros as well as define the two pins used for
    // indicating the entry and exit of critical sections.

    //UNCOMMENT the below #define to enable interrupt debugging.
    //#define INTERRUPT_DEBUGGING
    #ifdef INTERRUPT_DEBUGGING
      #define INT_DEBUG(x)  x
    #else
      #define INT_DEBUG(x)
    #endif

    #ifdef INTERRUPT_DEBUGGING
      #define CLR_PORT_BIT(p, b) PORT##p &= ~BIT(b)
      #define SET_PORT_BIT(p, b) PORT##p |= BIT(b)
      #define BLIP_PORT_BIT(p, b) PORT##p |= BIT(b); PORT##p &= ~BIT(b)
    #else
      #define CLR_PORT_BIT(p, b)
      #define SET_PORT_BIT(p, b)
      #define BLIP_PORT_BIT(p, b)
    #endif

    // GPIO outputs for measuring the duration of global interrupt state and
    // specific interrupt service routines. The comments below give the meaning
    // of the output when it is HIGH. 
    // Pin assignments refer to the dev0222 board.
    // LED 2 (PORTC.2):   interrupts are disabled (includes ATOMIC blocks)
    // LED 3 (PORTC.3):   radio receive isr
    // LED 4 (PORTC.4):   radio transmit isr (symbol timer compare)
    // LED 5 (PORTC.5):   an isr other than radio, uart or system timer
    // GPIO 0 (PORTG.0):  uart 1 receive isr
    // GPIO 1 (PORTG.1):  uart 1 transmit isr
    // GPIO 2 (PORTG.2):  general purpose time marker
    // GPIO 3 (PORTE.2):  system/sleep timer isr
    // GPIO 5 (PORTE.7):  uart 1 receive isr error
    #define INT_DEBUG_INTS_OFF()        SET_PORT_BIT(C, 2)
    #define INT_DEBUG_INTS_ON()         CLR_PORT_BIT(C, 2)
    #define INT_DEBUG_BEG_RADIO_RX()    INT_DEBUG_INTS_OFF(); SET_PORT_BIT(C, 3)
    #define INT_DEBUG_END_RADIO_RX()    CLR_PORT_BIT(C, 3); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_BEG_SYMBOL_CMP()  INT_DEBUG_INTS_OFF(); SET_PORT_BIT(C,4)
    #define INT_DEBUG_END_SYMBOL_CMP()  CLR_PORT_BIT(C,4); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_BEG_MISC_INT()    INT_DEBUG_INTS_OFF(); SET_PORT_BIT(C, 5)
    #define INT_DEBUG_END_MISC_INT()    CLR_PORT_BIT(C, 5); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_BEG_UART1_RX()    INT_DEBUG_INTS_OFF(); SET_PORT_BIT(G, 0)
    #define INT_DEBUG_END_UART1_RX()    CLR_PORT_BIT(G, 0); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_BEG_UART1_TX()    INT_DEBUG_INTS_OFF(); SET_PORT_BIT(G, 1)
    #define INT_DEBUG_END_UART1_TX()    CLR_PORT_BIT(G, 1); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_TIME_MARKER(n)    BLIP_PORT_BIT(G, 2)
    #define INT_DEBUG_BEG_SYSTIME_CMP() INT_DEBUG_INTS_OFF(); SET_PORT_BIT(E, 2)
    #define INT_DEBUG_END_SYSTIME_CMP() CLR_PORT_BIT(E, 2); INT_DEBUG_INTS_ON()
    #define INT_DEBUG_BEG_UART1_RXERR() SET_PORT_BIT(E, 7)
    #define INT_DEBUG_END_UART1_RXERR() CLR_PORT_BIT(E, 7)



    //UNCOMMENT the below #define to enable timing diabled interrupt blocks,
    // except for "LITE" blocks that are never timed.
    //#define ENABLE_ATOMIC_CLOCK

  #endif  // DOXYGEN_SHOULD_SKIP_THIS
  
    //The core Global Interrupt Manipulation Macros start here.

    #ifdef ENABLE_ATOMIC_CLOCK
      #define START_ATOMIC_CLOCK()      \
        do {                            \
          if (INTERRUPTS_WERE_ON())     \
                halStartAtomicClock();  \
          }                             \
        while(0)
      #define STOP_ATOMIC_CLOCK()       \
        do {                            \
          if (INTERRUPTS_WERE_ON())     \
                halStopAtomicClock();   \
          }                             \
        while(0)
    #else
      #define START_ATOMIC_CLOCK()
      #define STOP_ATOMIC_CLOCK()
    #endif

    /**
     * @brief This macro should be called in the local variable
     * declarations section of any function which calls DISABLE_INTERRUPTS()
     * or RESTORE_INTERRUPTS().
     */
    #define DECLARE_INTERRUPT_STATE int8u _emIsrState
    #define DECLARE_INTERRUPT_STATE_LITE int8u _emIsrState


    /**
     * @brief Disable interrupts, saving the previous state so it can be
     * later restored with RESTORE_INTERRUPTS().
     * \note Do not fail to call RESTORE_INTERRUPTS().
     * \note It is safe to nest this call.
     * \note Use the LITE version only if you are certain interrupts will 
     *  not be disabled for long, and pair with RESTORE_INTERRUPTS_LITE().
     */
    #define DISABLE_INTERRUPTS()        \
      _emIsrState = __save_interrupt(); \
      do {                              \
        INT_DEBUG_INTS_OFF();           \
        __disable_interrupt();          \
        START_ATOMIC_CLOCK();           \
      } while(0)

    #define DISABLE_INTERRUPTS_LITE()   \
      _emIsrState = __save_interrupt(); \
      do {                              \
        INT_DEBUG_INTS_OFF();           \
        __disable_interrupt();          \
      } while(0)


    /** 
     * @brief Restore the global interrupt state previously saved by
     * DISABLE_INTERRUPTS()
     * \note Do not call without having first called DISABLE_INTERRUPTS()
     * to have saved the state.
     * \note It is safe to nest this call.
     * \note Use the LITE version only if you are certain interrupts will 
     *  not be disabled very long, and pair with DISABLE_INTERRUPTS_LITE().
     */
    #define RESTORE_INTERRUPTS()          \
      do {                                \
        STOP_ATOMIC_CLOCK();              \
        INT_DEBUG(                        \
          if (INTERRUPTS_WERE_ON())       \
            INT_DEBUG_INTS_ON();          \
        )                                 \
        __restore_interrupt(_emIsrState); \
      } while(0)

    #define RESTORE_INTERRUPTS_LITE()     \
      do {                                \
        INT_DEBUG(                        \
          if (INTERRUPTS_WERE_ON())       \
            INT_DEBUG_INTS_ON();          \
        )                                 \
        __restore_interrupt(_emIsrState); \
      } while(0)


    /**
     * @brief Enable global interrupts without regard to the current or
     * previous state.
     */
    #define INTERRUPTS_ON()   \
      do {                    \
        INT_DEBUG_INTS_ON();  \
        __enable_interrupt(); \
      } while(0)


    /**
     * @brief Disable global interrupts without regard to the current or
     * previous state.
     */
    #define INTERRUPTS_OFF()   \
      do {                     \
        __disable_interrupt(); \
        INT_DEBUG_INTS_OFF();  \
      } while(0)


    /**
     * @returns TRUE if global interrupt flag was enabled when 
     * DISABLE_INTERRUPTS() was called.
     */
    #define INTERRUPTS_WERE_ON() (_emIsrState & BIT(7))


    /**
     * @returns TRUE if the current global interrupt flag is enabled.
     */
    #define INTERRUPTS_ARE_OFF() (!(__save_interrupt() & BIT(7))) 


    /**
     * @brief A block of code may be made atomic by wrapping it with this
     * macro.  Something which is atomic cannot be interrupted by interrupts.
     * \note Use the LITE version only if you are certain interrupts will 
     *  not be disabled very long.
     */

    #define ATOMIC(code)          \
      {                           \
        DECLARE_INTERRUPT_STATE;  \
        DISABLE_INTERRUPTS();     \
        { code }                  \
        RESTORE_INTERRUPTS();     \
      }

    #define ATOMIC_LITE(code)         \
      {                               \
        DECLARE_INTERRUPT_STATE_LITE; \
        DISABLE_INTERRUPTS_LITE();    \
        { code }                      \
        RESTORE_INTERRUPTS_LITE();    \
      }


    /**
     * @brief Allows up to two pending interrupts to be executed. 
     * Usually this would be called at a safe point while interrupts are 
     * disabled (such as within an ISR).
     * 
     * Takes no action if interrupts are already enabled.
     * 
     * The implementation accounts for an AVR related oddity, wherein one 
     * instruction must be executed (a NOP in this instance) before global
     * interrupts are allowed to occur.
     */
    #define HANDLE_PENDING_INTERRUPTS() \
      do {                              \
        if (INTERRUPTS_ARE_OFF()) {     \
          INTERRUPTS_ON();              \
          __no_operation();             \
          __no_operation();             \
          __no_operation();             \
          INTERRUPTS_OFF();             \
        }                               \
      } while (0)


#endif // BOOTLOADER
////////////////////////////////////////////////////////////////////////////////
//@}  // end of Global Interrupt Manipulation Macros
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/** \name C Standard Library Memory Utilities
 * These should be used in place of the standard library functions.
 * 
 * These functions have the same parameters and expected results as their C
 * Standard Library equivalents but may take advantage of certain implementation
 * optimizations.
 * 
 * Unless otherwise noted, these functions are utilized by the EmberStack and are 
 * therefore required to be implemented in the HAL. Additionally, unless otherwise
 * noted, applications that find these functions useful may utilze them.
 */
////////////////////////////////////////////////////////////////////////////////
//@{

/**
 * @brief Refer to the C stdlib memcpy().
 */
void halCommonMemCopy(void *dest, const void *src, int8u bytes);


/**
 * @brief Same as the C stdlib memcpy(), but handles copying from const 
 * program space.
 */
void halCommonMemPGMCopy(void* dest, void PGM *source, int8u bytes);


/**
 * @brief Refer to the C stdlib memset().
 */
void halCommonMemSet(void *dest, int8u val, int16u bytes);


/**
 * @brief Refer to the C stdlib memcmp().
 */
int8s halCommonMemCompare(const void *source0, const void *source1, int8u bytes);


/**
 * @brief Works like C stdlib memcmp(), but takes a flash space source
 * parameter.
 */
int8s halCommonMemPGMCompare(const void *source0, void PGM *source1, int8u bytes);

/**
 * @brief Friendly convenience macro pointing to the full HAL function.
 */
#define MEMSET(d,v,l)  halCommonMemSet(d,v,l)
#define MEMCOPY(d,s,l) halCommonMemCopy(d,s,l)
#define MEMCOMPARE(s0,s1,l) halCommonMemCompare(s0, s1, l)
#define MEMPGMCOMPARE(s0,s1,l) halCommonMemPGMCompare(s0, s1, l)

////////////////////////////////////////////////////////////////////////////////
//@}  // end of C Standard Library Memory Utilities
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Some platforms can perform divide and modulus operations on 32 bit 
 * quantities more efficiently when the divisor is only a 16 bit quantity.
 * C compilers will always promote the divisor to 32 bits before performing the
 * operation, so the following utility functions are instead required to take 
 * advantage of this optimisation.
 * The AVR/IAR platform does not currently have such an optimisation.
 */  
#define halCommonUDiv32By16(x, y) ((int16u) (((int32u) (x)) / ((int16u) (y))))
#define halCommonSDiv32By16(x, y) ((int16s) (((int32s) (x)) / ((int16s) (y))))
#define halCommonUMod32By16(x, y) ((int16u) (((int32u) (x)) % ((int16u) (y))))
#define halCommonSMod32By16(x, y) ((int16s) (((int32s) (x)) % ((int16s) (y))))

#endif // __EMBER_CONFIG_H__
