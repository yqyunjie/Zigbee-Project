/** @file hal/micro/unix/compiler/gcc.h
 * See @ref platform_common and @ref unix_gcc_config for documentation.
 *
 * <!-- Copyright 2003-2010 by Ember Corporation. All rights reserved.   *80*-->
 */

/** @addtogroup unix_gcc_config
 * @brief Compiler and Platform specific definitions and typedefs for the
 *  the Unix GCC compiler.
 *
 * @note ATOMIC and interrupt manipulation macros are defined to have no
 * affect.
 *
 * @note gcc.h should be included first in all source files by setting the
 *  preprocessor macro PLATFORM_HEADER to point to it.  gcc.h automatically
 *  includes platform-common.h.
 *
 * See @ref platform_common for common documentation.
 *
 * See gcc.h for source code.
 *@{
 */

#ifndef __GCC_H__
#define __GCC_H__

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
typedef unsigned short int16u;
typedef signed   short int16s;
typedef unsigned int   int32u;
typedef signed   int   int32s;
typedef unsigned long long int64u;
typedef signed   long long int64s;
typedef unsigned long  PointerType;
//@} \\END MASTER VARIABLE TYPES


/**
 * @brief 8051 memory segments stubs.
 */
#define SEG_DATA
#define SEG_IDATA
#define SEG_XDATA
#define SEG_PDATA
#define SEG_CODE
#define SEG_BDATA

/**
 * @brief Denotes that this platform supports 64-bit data-types.
 */
#define HAL_HAS_INT64

/**
 * @brief Use the Master Program Memory Declarations from platform-common.h
 */
#define _HAL_USE_COMMON_PGM_

/**
 * @brief A definition stating what the endianess of the platform is.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
#define BIGENDIAN_CPU FALSE
#else
  #if defined(__i386__)
    #define BIGENDIAN_CPU  FALSE
  #elif defined(__APPLE__)
    #define BIGENDIAN_CPU  TRUE
  #elif defined(__ARM7__)
    #define BIGENDIAN_CPU  FALSE
  #elif defined(__x86_64__)
    #define BIGENDIAN_CPU  FALSE
  #elif defined(__arm__)
    #if defined(__BIG_ENDIAN)
      #define BIGENDIAN_CPU  TRUE
    #else
      #define BIGENDIAN_CPU  FALSE
    #endif
  #else
    #error endianess not defined
  #endif
#endif


#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #define NO_STRIPPING
  #define EEPROM

  #if (! defined(EMBER_STACK_IP))
    #ifndef DEBUG_LEVEL
      #ifdef DEBUG
        #define DEBUG_LEVEL FULL_DEBUG
      #else
        #define DEBUG_LEVEL NO_DEBUG
      #endif
    #endif
  #else
    #ifndef DEBUG_LEVEL
      #if defined(DEBUG) && defined(DEBUG_OFF)
        #error "DEBUG and DEBUG_OFF cannot be both be defined!"
      #elif defined(DEBUG)
        #define DEBUG_LEVEL FULL_DEBUG
      #elif defined(DEBUG_OFF)
        #define DEBUG_LEVEL NO_DEBUG
      #else
        #define DEBUG_LEVEL BASIC_DEBUG
      #endif
    #endif
  #endif

  // Always include stdio.h and assert.h if running under Unix so that they
  // can be used when debugging.
  #include <stdio.h>
  #include <assert.h>
  #include <stdarg.h>

  #define NOP()
  #define DECLARE_INTERRUPT_STATE
  #define DECLARE_INTERRUPT_STATE_LITE
  #define DISABLE_INTERRUPTS() do { } while(0)
  #define DISABLE_INTERRUPTS_LITE() do { } while(0)
  #define RESTORE_INTERRUPTS() do { } while(0)
  #define RESTORE_INTERRUPTS_LITE() do { } while(0)
  #define INTERRUPTS_ON() do { } while(0)
  #define INTERRUPTS_OFF() do { } while(0)
#if defined(EMBER_TEST)
  #define INTERRUPTS_ARE_OFF() (TRUE)
#else
  #define INTERRUPTS_ARE_OFF() (FALSE)
#endif
  #define ATOMIC(blah) { blah }
  #define ATOMIC_LITE(blah) { blah }
  #define HANDLE_PENDING_INTERRUPTS() do { } while(0)

  #define LOG_MESSAGE_DUMP

  #define UNUSED __attribute__ ((unused))
  #define SIGNED_ENUM

  // think different
  #ifdef __APPLE__
  #define __unix__
  #endif

  #ifdef WIN32
  // undefine this here too
  #define __attribute__(foo)
  #endif
  
  #if defined(EMBER_TEST)
    #define MAIN_FUNCTION_PARAMETERS void
    #define MAIN_FUNCTION_ARGUMENTS  
  #else
    #define MAIN_FUNCTION_PARAMETERS int argc, char* argv[]
    #define MAIN_FUNCTION_ARGUMENTS  argc, argv
    #define MAIN_FUNCTION_HAS_STANDARD_ARGUMENTS
  #endif
  
  // Called by application main loops to let the simulator simulate.
  // Not used on real hardware.
  void simulatedTimePasses(void);
  void simulatedTimePassesMs(int32u timeToNextAppEvent);
  // Called by the serial code when it wants to block.
  void simulatedSerialTimePasses(void);
#endif //DOXYGEN_SHOULD_SKIP_THIS


/** \name Watchdog Prototypes
 * Define the watchdog macro and internal function to simply be
 * stubs to satisfy those programs that have no HAL (i.e. scripted tests)
 * and those that want to reference real HAL functions (simulation binaries
 * and Unix host applications) we define both halResetWatchdog() and
 * halInternalResetWatchdog().  The former is used by most of the scripted
 * tests while the latter is used by simulation and real host applications.
 */
//@{
/** @brief Watchdog stub prototype.
 */
#define halResetWatchdog()
void halInternalResetWatchDog(void);
//@} //end of Watchdog Prototypes


/** \name C Standard Library Memory Utilities
 * These should be used in place of the standard library functions.
 */
//@{
/**
 * @brief All of the ember defined macros/functions simply redirect to
 * the full C Standard Library.
 */
#include <string.h>
#define MEMSET(d,v,l)  memset(d,v,l)
#define MEMCOPY(d,s,l)       memmove((d),(s),(l))
#define MEMFASTCOPY(d,s,l) MEMCOPY(d,s,l)
#define MEMCOMPARE(s0,s1,l) memcmp(s0, s1, l)
#define MEMPGMCOMPARE(s0,s1,l) memcmp(s0, s1, l)
#define halCommonMemPGMCopy(d, s, l) MEMCOPY((d), (s), (l))
#define halCommonMemPGMCompare(s1, s2, l) MEMCOMPARE((s1), (s2), (l))
//@}  // end of C Standard Library Memory Utilities
 
/**
 * @brief Use the Divide and Modulus Operations from platform-common.h
 */
#define _HAL_USE_COMMON_DIVMOD_

/**
 * @brief Include platform-common.h last to pick up defaults and common definitions.
 */
#define PLATCOMMONOKTOINCLUDE
  #include "hal/micro/generic/compiler/platform-common.h"
#undef PLATCOMMONOKTOINCLUDE

#endif //__GCC_H__

/**@} //END addtogroup 
 */

