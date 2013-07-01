/**************************************************************
 **             - __low_level_init.c -
 **
 **     Special initializations that are performed before
 **     segment initialization. It is also possible to
 **     completely block the normal segment initialization.
 **
 **     Used with iccAVR.
 **
 **     $Revision: 1.2 $
 **
 **************************************************************/

extern void *__RSTACK_in_external_ram;
#pragma segment="RSTACK"
#pragma segment="CSTACK"

/**************************************************************
 **
 ** How to implement a low-level initialization function in C
 ** =========================================================
 **
 ** 1) Only use local auto variables.
 ** 2) Don't use global or static variables.
 ** 3) Don't use global or static objects (EC++ only).
 ** 4) Don't use agregate initializers, e.g. struct a b = {1};
 ** 5) Don't call any library functions (function calls that
 **    the compiler generates, e.g. to do integer math, are OK).
 ** 6) Setup the RSTACK as is appropriate! See code below.
 **
 **************************************************************/

#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>   // grabs mcu specific SPR defs
#include <inavr.h>

#include "micro/avr-atmega/compiler/iar.h"  // for the ember data types
#define __MICRO_H__
#include "micro/avr-atmega/128/reserved-ram.h"   // for the reserved ram locations

#define BIT(x) (1 << (x))

#ifdef ENABLE_XRAM
  #undef SRAM_END 
  #define SRAM_END EXT_SRAM_END
#endif


#ifdef __cplusplus
extern "C" {
#endif
char __low_level_init()
{
  /* Uncomment the statement below if the RSTACK */
  /* segment has been placed in external SRAM!   */

  /* __require(__RSTACK_in_external_ram); */

  /* If the low-level initialization routine is  */
  /* written in assembler, the line above should */
  /* be written as:                              */
  /*     EXTERN  __RSTACK_in_external_ram        */
  /*     REQUIRE __RSTACK_in_external_ram        */

  /* Add your custom setup here. */
  
  /*
  since the begining of time at ember we've noticed that nodes can get into a state 
  whereby they throw an assert, then asssert several more times in rapid succession.

  normally, our iar compiler produces code for bootup (before main) that sets up the 
  near_i(static and global variables with non-zero initial values)and near_z(static 
  and global variable without initial value or zero initial values) segments.  it 
  doesn't zero out the rstack, cstack, or unused ram segments, which we can do 
  manually in low_level_init.c. it also doesn't reset all the micro's sfrs to their 
  default power on values, which we do by forcing a watch dog reset.

  for now, we'll take the effort to zero everything at low_level_init to help with 
  these issues.
  */
  
  register int8u* p;

  // might not be in a clean reset scenario, ensure global ints are off
  __disable_interrupt(); 

  // handle the reset cause
  // (detection takes place in cstartup.s90)
  if ( emResetCause == 0 ) { // unknown reset?
    // 1. what has likely happened is that the stack crashed or the pc wrapped
    //    we need to record that, we did with the emResetCausePrevious setting
    // 2. record if the stack was out of bounds
    // 3. we force a wdog reset to get a clean reset where peripherals and such 
    //    all back into their power on defaults.

    // we're self wdog'in, record that
    emResetCauseMarker = BIT(RESET_CAUSE_SELF_WDOG);

    // test if the stack crashed
    if ( (void *)emResetRStack < __segment_begin("RSTACK") || (void *)emResetRStack > __segment_end("RSTACK") )
      emResetCauseMarker |= BIT(RESET_CAUSE_RSTACK_BIT);
    if ( (void *)emResetCStack < __segment_begin("CSTACK") || (void *)emResetCStack > __segment_end("CSTACK") )
      emResetCauseMarker |= BIT(RESET_CAUSE_CSTACK_BIT);

    // force a wdog reset
    WDTCR = BIT(WDCE) + BIT(WDE); 
    WDTCR = BIT(WDE);     // fastest timeout about ~18ms
    for (;;) { }          // wait for the wdog to get us  
  }
  
  // zero all of internal and external sram.
  // be careful not to clobber our return location (which is the first and only 
  // entry on RSTACK)!
  for (p = (unsigned char*)SRAM_BASE; p <= (unsigned char*)__segment_end("RSTACK") - 3 ; p++)
    *p = 0x00;
  for (p = (unsigned char*)__segment_end("RSTACK"); p <= (unsigned char*)SRAM_END ; p++)
    *p = 0x00;
  
  /* Return 1 to indicate that normal segment */
  /* initialization should be performed. If   */
  /* normal segment initialization should not */
  /* be performed, return 0.                  */
  return 1;
}
#ifdef __cplusplus
}
#endif

