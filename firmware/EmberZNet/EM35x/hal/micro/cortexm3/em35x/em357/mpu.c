//=============================================================================
// FILE
//   mpu.c - Functions to setup and control the Cortex-M3 MPU
//
// DESCRIPTION
//   This file contains the definitions, data table and software used to
//   setup the MPU.
//
//   Author:
//   Copyright 2008 by Ember Corporation. All rights reserved.             *80*
//=============================================================================

#include PLATFORM_HEADER
#include "hal/micro/cortexm3/mpu.h"
#include "hal/micro/micro.h"

#define FLASH_REGION    (0x08000000 + 0x10)
#define PERIPH_REGION   (0x40000000 + 0x11)
#define USERPER_REGION  (0x40008000 + 0x12)
#define SRAM_REGION     (0x20000000 + 0x13)
#define GUARD_REGION    (0x20000000 + 0x14)
#define SPARE0_REGION   (0x20000000 + 0x15)
#define SPARE1_REGION   (0x20000000 + 0x16)
#define SPARE2_REGION   (0x20000000 + 0x17)

//=============================================================================
// Define the data used to initialize the MPU. Each of the 8 MPU regions
// has a programmable size and various attributes. A region must be a power of
// two in size, and its base address must be a multiple of that size. Regions
// are divided into 8 equal-sized sub-regions that can be individually disabled.
// A region is defined by what is written to MPU_BASE and MPU_ATTR.
// MPU_BASE holds the region base address, with some low order bits ignored
// depending on the region size. If B4 is set, then B3:0 set the region number.
// The MPU_ATTR fields are:
// XN (1 bit) - set to disable instruction execution
// AP (2 bits) - selects Privilege & User access- None, Read-only or Read-Write
// TEX,S,C,B (6 bits) - configures memory type, write ordering, shareable, ...
// SRD (8 bits) - a set bit disables the corresponding sub-region
// SIZE (5 bits) - specifies the region size as a power of two
// ENABLE (1 bit) - set to enable the region, except any disabled sub-regions
//=============================================================================

// Define MPU regions - note that the default vector table at address 0 and
// the ARM PPB peripherals are always accessible in privileged mode.
static mpu_t mpuConfig[NUM_MPU_REGIONS] =
{
  // Region 0 - Flash, including main, fixed and customer info blocks: 
  //            execute, normal, not shareable
  // Enabled sub-regions: 08000000 - 0802FFFF, 08040000 - 0804FFFF
  { FLASH_REGION,   MATTR(0, PRO_URO, MEM_NORMAL, 0xE8, SIZE_512K, 1) },

  // Region 1 - System peripherals: no execute, non-shared device
//[[
#ifdef EMBER_EMU_TEST
  // Enabled sub-regions: 40000000 - 4000FFFF
  { PERIPH_REGION,  MATTR(1, PRW_URO, MEM_DEVICE, 0x00, SIZE_64K,  1) },
#else
//]]
  // Enabled sub-regions: 40000000 - 40008000, 4000A000 - 4000FFFF
  { PERIPH_REGION,  MATTR(1, PRW_URO, MEM_DEVICE, 0x10, SIZE_64K,  1) },
//[[
#endif
//]]

  // Region 2 - User peripherals: no execute, non-shared device
  // Enabled sub-regions: 4000A000 - 4000FFFF
  { USERPER_REGION, MATTR(1, PRW_URW, MEM_DEVICE, 0x03, SIZE_32K,  1) },

  // Region 3 - SRAM: no execute, normal, not shareable
  // Enabled sub-regions: 20000000 - 20002FFF
  { SRAM_REGION,    MATTR(1, PRW_URW, MEM_NORMAL, 0xC0, SIZE_16K,  1) },

  // Region 4 - Guard region between the heap and stack
  { GUARD_REGION,   MATTR(1, PNA_UNA, MEM_NORMAL, 0x00, HEAP_GUARD_REGION_SIZE, 0) },

  // Regions 5-7 - unused: disabled (otherwise set up for SRAM)
  { SPARE0_REGION,  MATTR(1, PRW_URW, MEM_NORMAL, 0x00, SIZE_1K,   0) },
  { SPARE1_REGION,  MATTR(1, PRW_URW, MEM_NORMAL, 0x00, SIZE_1K,   0) },
  { SPARE2_REGION,  MATTR(1, PRW_URW, MEM_NORMAL, 0x00, SIZE_1K,   0) }
};

// Load the base address and attributes of all 8 MPU regions, then enable
// the MPU. Even though interrupts should be disabled when this function is
// called, the region loading is performed in an atomic block in case they
// are not disabled. After the regions have been defined, the MPU is enabled.
// To be safe, memory barrier instructions are included to make sure that
// the new MPU setup is in effect before returning to the caller.
//
// Note that the PRIVDEFENA bit is not set in the MPU_CTRL register so the 
// default privileged memory map is not enabled. Disabling the default
// memory map enables faults on core accesses (other than vector reads) to 
// the address ranges shown below.
//
//  Address range
//  Flash (ICODE and DCODE)   no access allowed (read, write or execute)
//    00000000 to 0001FFFF    no access allowed (read, write or execute)
//    00200000 to 00029FFF    write and execute not allowed
//    0002A000 to 07FFFFFF    no access allowed (read, write or execute)
//    08000000 to 081FFFFF    write not allowed
//    08020000 to 1FFFFFFF    no access allowed (read, write or execute)
//  SRAM
//    20000000 to 20001FFF    execute not allowed
//    20002000 to 3FFFFFFF    no access allowed (read, write or execute)
//  Peripheral
//    40010000 to 5FFFFFFF    no access allowed (read, write or execute)
//  External Device / External RAM
//    60000000 to DFFFFFFF    no access allowed (read, write or execute)

void halInternalEnableMPU(void)
{
  halInternalLoadMPU(mpuConfig);
}

void halInternalLoadMPU(mpu_t *mp)
{
  int8u i;

  ATOMIC (
    MPU_CTRL = 0;       // enable default map while MPU is updated
    for (i = 0; i < NUM_MPU_REGIONS; i++) {
      MPU_BASE = mp->base;
      MPU_ATTR = mp->attr;
      mp++;
    }
    MPU_CTRL = MPU_CTRL_ENABLE;
    _executeBarrierInstructions();
  )

}

void halInternalDisableMPU(void)
{
  MPU_CTRL = 0;
  _executeBarrierInstructions();
}

void halInternalSetMPUGuardRegionStart(int32u baseAddress)
{
  // Add the region number to the base address so that we can load using the
  // quicker method. This will also get rid of any low order bits which are
  // invalid in the base address anyways.
  baseAddress &= 0xFFFFFFE0;                // Clear the lower 5 bits for region num
  baseAddress |= GUARD_REGION & 0x0000001F; // Use the guard region's number

  // Set the base address for the MPU guard region
  ATOMIC(
    mpuConfig[GUARD_REGION & 0xF].base = baseAddress;
    mpuConfig[GUARD_REGION & 0xF].attr = MATTR(1, PNA_UNA, MEM_NORMAL, 0x00, HEAP_GUARD_REGION_SIZE, 1);
    );
}

boolean halInternalIAmAnEmulator(void)
{
  boolean retval;

  ATOMIC(
    MPU_CTRL = 0;
    _executeBarrierInstructions();
    retval =  ((I_AM_AN_EMULATOR & 1) == 1);
    MPU_CTRL = MPU_CTRL_ENABLE;
    _executeBarrierInstructions();
  )
  return retval;
}
