/*
 * File: gcc.h
 * Description: Micro/Compiler specifics for the Unix platform.
 *
 * Author(s): Lee Taylor, lee@ember.com
 *
 * Copyright 2003 by Ember Corporation. All rights reserved.                *80*
 */
#ifndef __GCC_H__
#define __GCC_H__

// EMBER_TEST may not be defined for generic-host MICROs (i.e. UART hosts)

//*********************************************************************
  // Would this definition be more appropriate?  It used to be used for 
  //   some of this stuff
  //#if (defined __unix__ || defined WIN32 || defined __APPLE__) 

  typedef unsigned char  boolean;
  typedef unsigned char  int8u;
  typedef signed   char  int8s;
  typedef unsigned short int16u;
  typedef signed   short int16s;
  typedef unsigned int   int32u;
  typedef signed   int   int32s;
  typedef unsigned int  PointerType;
  #define PGM     const
  #define PGM_P   const char *
  #define PGM_PU  const unsigned char *
  // Sometimes a second PGM is needed in a declaration.  Having two 'const'
  // declarations generates a warning so we have a second PGM that turns
  // into nothing under gcc.
  #define PGM_NO_CONST

  #if defined(__i386__)
    #define BIGENDIAN_CPU  FALSE
  #elif defined(__APPLE__)
    #define BIGENDIAN_CPU  TRUE
  #elif defined(__ARM7__)
    #define BIGENDIAN_CPU  FALSE
  #else
    #error endianess not defined
  #endif

  #define NO_STRIPPING
  #define EEPROM

  #ifndef DEBUG_LEVEL
    #ifdef DEBUG
      #define DEBUG_LEVEL FULL_DEBUG
    #else
      #define DEBUG_LEVEL NO_DEBUG
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
  #define INTERRUPTS_ARE_OFF() (FALSE)
  #define ATOMIC(blah) { blah }
  #define ATOMIC_LITE(blah) { blah }
  #define HANDLE_PENDING_INTERRUPTS() do { } while(0)

  #define LOG_MESSAGE_DUMP

  #define UNUSED __attribute__ ((unused))
  #define SIGNED_ENUM

  // think different
  #ifdef __APPLE__ 
  // undefine this here too
  #define __attribute__(foo)
  #define __unix__
  #endif

  #ifdef WIN32
  // undefine this here too
  #define __attribute__(foo)
  #endif

  #include <string.h>
  #define MEMSET(d,v,l)  memset(d,v,l)
  // We use our own memcopy instead of the standard one to maintain the
  // same behavior on the micro and on PCs.  Our version allows the
  // destination to overlap the source on either side.
  #define MEMCOPY(d,s,l)       memmove((d),(s),(l))
  #define MEMFASTCOPY(d,s,l) MEMCOPY(d,s,l)
  #define MEMCOMPARE(s0,s1,l) memcmp(s0, s1, l)
  #define MEMPGMCOMPARE(s0,s1,l) memcmp(s0, s1, l)
  #define halResetWatchdog()

  #define halCommonMemPGMCopy(d, s, l) MEMCOPY((d), (s), (l))
  #define halCommonMemPGMCompare(s1, s2, l) MEMCOMPARE((s1), (s2), (l))
 
  // Called by application main loops to let the simulator simulate.
  // Not used on real hardware.
  void simulatedTimePasses(void);
  void simulatedTimePassesMs(int32u timeToNextAppEvent);
  // Called by the serial code when it wants to block.
  void simulatedSerialTimePasses(void);

  #define halCommonUDiv32By16(x, y) ((int16u) (((int32u) (x)) / ((int16u) (y))))
  #define halCommonSDiv32By16(x, y) ((int16s) (((int32s) (x)) / ((int16s) (y))))
  #define halCommonUMod32By16(x, y) ((int16u) (((int32u) (x)) % ((int16u) (y))))
  #define halCommonSMod32By16(x, y) ((int16s) (((int32s) (x)) % ((int16s) (y))))

  #if defined(EMBER_TEST)
    #define MAIN_FUNCTION_PARAMETERS void
    #define MAIN_FUNCTION_ARGUMENTS  
  #else
    #define MAIN_FUNCTION_PARAMETERS int argc, char* argv[]
    #define MAIN_FUNCTION_ARGUMENTS  argc, argv
  #endif

#endif //__GCC_H__
