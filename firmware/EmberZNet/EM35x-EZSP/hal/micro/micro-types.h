/** @file hal/micro/micro-types.h
 * 
 * @brief This file handles defines and enums related to the all micros.
 *
 * <!-- Copyright 2004-2013 by Silicon Laboratories. All rights reserved.*80*-->
 */

#ifndef __MICRO_DEFINES_H__
#define __MICRO_DEFINES_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// Below is a list of PLATFORM and MICRO values that are used to define the
// chips our code runs on. These values are used in ebl headers, bootloader
// query response messages, and possibly other places in the code


// -----------------------------------------------------------------------------
// PLAT 1 was the AVR ATMega, no longer supported (still used for EMBER_TEST)
// for PLAT 1, MICRO 1 is the AVR ATMega 64
// for PLAT 1, MICRO 2 is the AVR ATMega 128
// for PLAT 1, MICRO 3 is the AVR ATMega 32

// PLAT 2 is the XAP2b
// for PLAT 2, MICRO 1 is the em250
// for PLAT 2, MICRO 2 is the em260

// PLAT 4 is the Cortex-M3
// for PLAT 4, MICRO 1 is the em350
// for PLAT 4, MICRO 2 is the em360
// for PLAT 4, MICRO 3 is the em357
// for PLAT 4, MICRO 4 is the em367
// for PLAT 4, MICRO 5 is the em351
// for PLAT 4, MICRO 6 is the em35x
// for PLAT 4, MICRO 7 is the stm32w108
// for PLAT 4, MICRO 8 is the em3588
// for PLAT 4, MICRO 9 is the em359
// for PLAT 4, MICRO 10 is the em3581
// for PLAT 4, MICRO 11 is the em3582
// for PLAT 4, MICRO 12 is the em3585
// for PLAT 4, MICRO 13 is the em3586

// PLAT 5 is C8051
// for PLAT 5, MICRO 1 is the siF930
// for PLAT 5, MICRO 2 is the siF960
// -----------------------------------------------------------------------------


// Create an enum for the platform types
enum {
  EMBER_PLATFORM_UNKNOWN    = 0,
	EMBER_PLATFORM_AVR_ATMEGA = 1,    // *also used for simulation*
  EMBER_PLATFORM_XAP2B      = 2,
	EMBER_PLATFORM_MSP_430    = 3,
	EMBER_PLATFORM_CORTEXM3   = 4,
	EMBER_PLATFORM_C8051      = 5,
  EMBER_PLATFORM_MAX_VALUE
};
typedef int16u EmberPlatformEnum;

#define EMBER_PLATFORM_STRINGS \
    "Unknown",                 \
	  "Test",										 \
    "XAP2b",                   \
    "MSP 430",                 \
    "CortexM3",                \
    "C8051",                   \
    NULL

// Create an enum for all of the different XAP2b micros we support
enum {
	EMBER_MICRO_AVR_ATMEGA_UNKNOWN	 = 0,
	EMBER_MICRO_AVR_ATMEGA_64		     = 1,
	EMBER_MICRO_AVR_ATMEGA_128		   = 2,
	EMBER_MICRO_AVR_ATMEGA_32		     = 3,
	EMBER_MICRO_AVR_ATMEGA_MAX_VALUE
};
typedef int16u EmberMicroAvrAtmegaEnum;

#define EMBER_MICRO_AVR_ATMEGA_STRINGS \
    "Unknown",                         \
    "avr64",                           \
    "avr128",                          \
    "avr32",                           \
    NULL,

// Create an enum for all of the different XAP2b micros we support
enum {
  EMBER_MICRO_XAP2B_UNKNOWN   = 0,
  EMBER_MICRO_XAP2B_EM250     = 1,
  EMBER_MICRO_XAP2B_EM260     = 2,
  EMBER_MICRO_XAP2B_MAX_VALUE
};
typedef int16u EmberMicroXap2bEnum;

#define EMBER_MICRO_XAP2B_STRINGS \
    "Unknown",                    \
		"em250",											\
		"em260",											\
		"em3586",											\
    NULL,

// Create an enum for all of the different CORTEXM3 micros we support
enum {
  EMBER_MICRO_CORTEXM3_UNKNOWN   = 0,
  EMBER_MICRO_CORTEXM3_EM350     = 1,
  EMBER_MICRO_CORTEXM3_EM360     = 2,
  EMBER_MICRO_CORTEXM3_EM357     = 3,
  EMBER_MICRO_CORTEXM3_EM367     = 4,
  EMBER_MICRO_CORTEXM3_EM351     = 5,
  EMBER_MICRO_CORTEXM3_EM35X     = 6,
  EMBER_MICRO_CORTEXM3_STM32W108 = 7,
  EMBER_MICRO_CORTEXM3_EM3588    = 8,
  EMBER_MICRO_CORTEXM3_EM359     = 9,
  EMBER_MICRO_CORTEXM3_EM3581    = 10,
  EMBER_MICRO_CORTEXM3_EM3582    = 11,
  EMBER_MICRO_CORTEXM3_EM3585    = 12,
  EMBER_MICRO_CORTEXM3_EM3586    = 13,
  EMBER_MICRO_CORTEXM3_MAX_VALUE
};
typedef int16u EmberMicroCortexM3Enum;

#define EMBER_MICRO_CORTEXM3_STRINGS \
    "Unknown",                       \
    "em350",                         \
    "em360",                         \
    "em357",                         \
    "em367",                         \
    "em351",                         \
    "em35x",                         \
    "stm32w108",                     \
    "em3588",                        \
    "em359",                         \
    "em3581",                        \
    "em3582",                        \
    "em3585",                        \
    "em3586",                        \
    NULL,

// Create an enum for all of the different C8051 micros we support
enum {
  EMBER_MICRO_C8051_UNKNOWN   = 0,
	EMBER_MICRO_C8051_SIF930    = 1,
	EMBER_MICRO_C8051_SIF960		= 2,
  EMBER_MICRO_C8051_MAX_VALUE
};
typedef int16u EmberMicroC8051Enum;

#define EMBER_MICRO_C8051_STRINGS \
    "Unknown",                    \
		"siF930",											\
		"siF960",											\
    NULL,

// A dummy type declared to allow generically passing around this
// data type.  Micro specific enums (such as EmberMicroCortexM3Enum)
// are required to be the same size as this. 
typedef int16u EmberMicroEnum;


// Determine what micro and platform that we're supposed to target using the
// defines passed in at build time. Then set the PLAT and MICRO defines based
// on what was passed in
#if ((! defined(EZSP_HOST)) && (! defined(UNIX_HOST)))

#if defined( EMBER_TEST )
  #define PLAT EMBER_PLATFORM_AVR_ATMEGA
  #define MICRO 2
#elif defined (AVR_ATMEGA)
  #define PLAT EMBER_PLATFORM_AVR_ATMEGA
  #if defined (AVR_ATMEGA_64)
    #define MICRO EMBER_MICRO_AVR_ATMEGA_64
  #elif defined (AVR_ATMEGA_128)
    #define MICRO EMBER_MICRO_AVR_ATMEGA_128
  #elif defined (AVR_ATMEGA_32)
    #define MICRO EMBER_MICRO_AVR_ATMEGA_32
  #endif
#elif defined (XAP2B)
  #define PLAT EMBER_PLATFORM_XAP2B
  #if defined (XAP2B_EM250)
    #define MICRO EMBER_MICRO_XAP2B_EM250
  #elif defined (XAP2B_EM260)
    #define MICRO EMBER_MICRO_XAP2B_EM260
  #endif
#elif defined (MSP430)
  #define PLAT EMBER_PLATFORM_MSP_430
  #if defined (MSP430_1612)
    #define MICRO 1
  #elif defined (MSP430_1471)
    #define MICRO 2
  #endif
#elif defined (CORTEXM3)
  #define PLAT EMBER_PLATFORM_CORTEXM3
  #if defined (CORTEXM3_EM350)
    #define MICRO EMBER_MICRO_CORTEXM3_EM350
  #elif defined (CORTEXM3_EM360)
    #define MICRO EMBER_MICRO_CORTEXM3_EM360
  #elif defined (CORTEXM3_EM357)
    #define MICRO EMBER_MICRO_CORTEXM3_EM357
  #elif defined (CORTEXM3_EM367)
    #define MICRO EMBER_MICRO_CORTEXM3_EM367
  #elif defined (CORTEXM3_EM351)
    #define MICRO EMBER_MICRO_CORTEXM3_EM351
  #elif defined (CORTEXM3_EM35X)
    #define MICRO EMBER_MICRO_CORTEXM3_EM35X
  #elif defined (CORTEXM3_STM32W108)
    #define MICRO EMBER_MICRO_CORTEXM3_STM32W108
  #elif defined (CORTEXM3_EM3581)
    #define MICRO EMBER_MICRO_CORTEXM3_EM3581
  #elif defined (CORTEXM3_EM3582)
    #define MICRO EMBER_MICRO_CORTEXM3_EM3582
  #elif defined (CORTEXM3_EM3585)
    #define MICRO EMBER_MICRO_CORTEXM3_EM3585
  #elif defined (CORTEXM3_EM3586)
    #define MICRO EMBER_MICRO_CORTEXM3_EM3586
  #elif defined (CORTEXM3_EM3588)
    #define MICRO EMBER_MICRO_CORTEXM3_EM3588
  #elif defined (CORTEXM3_EM359)
    #define MICRO EMBER_MICRO_CORTEXM3_EM359
  #endif
#elif defined (C8051)
  #define PLAT EMBER_PLATFORM_C8051
  #if defined (C8051_SIF930)
    #define MICRO EMBER_MICRO_C8051_SIF930
  #elif defined (C8051_SIF960)
    #define MICRO EMBER_MICRO_C8051_SIF960
  #endif
#endif

#endif // ((! defined(EZSP_HOST)) && (! defined(UNIX_HOST)))

// Define distinct literal values for each phy.
// These values are used in the bootloader query response message.
// PHY 0 is a null phy
// PHY 1 is the em2420 (no longer supported)
// PHY 2 is the em250
// PHY 3 is the em3xx
// PHY 4 is the si446x
enum {
  EMBER_PHY_NULL   = 0,
  EMBER_PHY_EM2420 = 1,
  EMBER_PHY_EM250  = 2,
  EMBER_PHY_EM3XX  = 3,
  EMBER_PHY_SI446X = 4,
  EMBER_PHY_MAX_VALUE
};
typedef int16u EmberPhyEnum;

#define EMBER_PHY_STRINGS                   \
  "NULL",                                   \
  "em2420",                                 \
  "em250",                                  \
  "em3XX",                                  \
  "si446x",                                 \
  NULL

#if defined (PHY_EM2420) || defined (PHY_EM2420B)
  #error EM2420 is no longer supported.
#elif defined (PHY_EM250) || defined (PHY_EM250B)
  #define PHY EMBER_PHY_EM250
#elif defined (PHY_EM3XX)
  #define PHY EMBER_PHY_EM3XX
#elif defined (PHY_NULL)
  #define PHY EMBER_PHY_NULL
#elif defined (PHY_SI446X_US) || defined( PHY_SI446X_EU ) || defined( PHY_SI446X_CN ) || defined( PHY_SI446X_JP )
  #define PHY EMBER_PHY_SI446X
#endif

typedef struct {
  EmberPlatformEnum platform;
  EmberMicroEnum    micro;
  EmberPhyEnum      phy;
} EmberChipTypeStruct;


// Now that we've figured out our plat/micro/phy combination create any feature
// defines that we need here.
//   NOTE: Eventually we may want to move these into separate include files for
//         each micro in the micro's directory. This would help keep this file
//         clean and require less #if ... work
#if defined(PLAT) && defined(MICRO)

  // Create a define for all of the EM358X variants so that we don't have
  // giant macros
  #if defined (CORTEXM3_EM3581) || defined (CORTEXM3_EM3582) || \
      defined (CORTEXM3_EM3585) || defined (CORTEXM3_EM3586) || \
      defined (CORTEXM3_EM3588)
    #define CORTEXM3_EM358X 1
  #endif

  // Create a define for EM358X variants that support USB
  #if defined (CORTEXM3_EM3582) || defined (CORTEXM3_EM3586) || \
      defined (CORTEXM3_EM3588) || defined (CORTEXM3_EM359)
    #define CORTEXM3_EM35X_USB 1
  #endif

#endif // defined(PLAT) && defined(MICRO)

#endif // DOXYGEN_SHOULD_SKIP_THIS
#endif //__MICRO_DEFINES_H__

