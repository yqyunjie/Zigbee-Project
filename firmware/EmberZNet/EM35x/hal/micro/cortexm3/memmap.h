/** @file hal/micro/cortexm3/memmap.h
 * @brief EM300 series memory map definitions used by the full hal
 *
 * <!-- Copyright 2009-2011 by Ember Corporation.        All rights reserved.-->
 */
#ifndef __MEMMAP_H__
#define __MEMMAP_H__

// Include the chip specific definitions
#ifndef LOADER
  #if defined (CORTEXM3_EM351)
    #include "hal/micro/cortexm3/em35x/em351/memmap.h"
  #elif defined (CORTEXM3_EM357) || defined(EMBER_TEST)
    #include "hal/micro/cortexm3/em35x/em357/memmap.h"
  #elif defined (CORTEXM3_EM3581)
    #include "hal/micro/cortexm3/em35x/em3581/memmap.h"
  #elif defined (CORTEXM3_EM3582)
    #include "hal/micro/cortexm3/em35x/em3582/memmap.h"
  #elif defined (CORTEXM3_EM3585)
    #include "hal/micro/cortexm3/em35x/em3585/memmap.h"
  #elif defined (CORTEXM3_EM3586)
    #include "hal/micro/cortexm3/em35x/em3586/memmap.h"
  #elif defined (CORTEXM3_EM3588)
    #include "hal/micro/cortexm3/em35x/em3588/memmap.h"
  #elif defined (CORTEXM3_EM359)
    #include "hal/micro/cortexm3/em35x/em359/memmap.h"
  #elif defined (CORTEXM3_STM32W108)
    #include "hal/micro/cortexm3/stm32w108/memmap.h"
  #else
    #error no appropriate micro defined
  #endif
#endif

//=============================================================================
// A union that describes the entries of the vector table.  The union is needed
// since the first entry is the stack pointer and the remainder are function
// pointers.
//
// Normally the vectorTable below would require entries such as:
//   { .topOfStack = x },
//   { .ptrToHandler = y },
// But since ptrToHandler is defined first in the union, this is the default
// type which means we don't need to use the full, explicit entry.  This makes
// the vector table easier to read because it's simply a list of the handler
// functions.  topOfStack, though, is the second definition in the union so
// the full entry must be used in the vectorTable.
//=============================================================================
typedef union
{
  void (*ptrToHandler)(void);
  void *topOfStack;
} HalVectorTableType;


// ****************************************************************************
// If any of these address table definitions ever need to change, it is highly
// desirable to only add new entries, and only add them on to the end of an
// existing address table... this will provide the best compatibility with
// any existing code which may utilize the tables, and which may not be able to 
// be updated to understand a new format (example: bootloader which reads the 
// application address table)

// Generic Address table definition which describes leading fields which
// are common to all address table types
typedef struct {
  void *topOfStack;
  void (*resetVector)(void);
  void (*nmiHandler)(void);
  void (*hardFaultHandler)(void);
  int16u type;
  int16u version;
  const HalVectorTableType *vectorTable;
  // Followed by more fields depending on the specific address table type
} HalBaseAddressTableType;

#ifdef MINIMAL_HAL
  // Minimal hal only references the FAT
  #include "memmap-fat.h"
#else
  // Full hal references all address tables
  #include "memmap-tables.h"
#endif

#endif //__MEMMMAP_H__

