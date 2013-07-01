/** @file hal\micro\avr-atmega\128\reserved-ram.h
 * 
 * @brief Memory allocations that allow for data sharing between the
 *   normally running application and bootloader 
 *
 *  See also @ref micro.
 *
 * By establishing a common memory "segment" we allow data exchange between
 * applications. for example the sample applications and the bootloader share
 * id information for over the air bootloading. 
 *
 * This is done by specifying hard coded ram addresses for key data, and a 
 * block or "segment" of ram which will not be erased by either application 
 * when they boot.
 *
 * There is a potential here for backwards compatilbity issues, since older
 * applications are  likely to "reserve" less ram, this is why it's a good
 * idea to reserve slightly more room than needed. although, assuming you're
 * only applications are "some app" and the bootloader, and it's only the
 * bootloader  that falls behind, and that it never even approaches the ram 
 * use limit - maybe not such an issue.
 *
 * The actual ram reserved block is specified in the linker scripts. 
 *
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup micro
 *
 * See also 128/reserved-ram.h.
 *
 */

/** @name Reserved RAM Definitions
 * These are reserved addresses for storing simple data for exchange between
 * applications.
 */
//@{
/**
 * @description A reserved address for storing simple data for exchange between
 * applications.
 */
#define HAL_RESERVED_RAM_BOOTLOADER_KEY     0x10FE    
#define HAL_RESERVED_RAM_PC_DIAG            0x10FC
#define HAL_RESERVED_RAM_GATEWAY_ID         0x10F4
#define HAL_RESERVED_RAM_EUI64_ID           0x10EC
#define HAL_RESERVED_RAM_PAN_ID             0x10EA
#define HAL_RESERVED_RAM_NODE_ID            0x10E8
#define HAL_RESERVED_RAM_CHANNEL            0x10E7
#define HAL_RESERVED_RAM_POWER              0x10E6
#define HAL_RESERVED_RAM_BANDS              0x10E5
#define HAL_RESERVED_RAM_MODE               0x10E4
#define HAL_RESERVED_RAM_RESET_CAUSE        0x10E3
#define HAL_RESERVED_RAM_RESET_CAUSE_B      0x10E2
#define HAL_RESERVED_RAM_RSTACK_H           0x10E1
#define HAL_RESERVED_RAM_RSTACK_L           0x10E0
#define HAL_RESERVED_RAM_CSTACK_H           0x10DF
#define HAL_RESERVED_RAM_CSTACK_L           0x10DE
//@}

/** @name Reset Cause Definitions
 * These are markers of possible reset causes.
 */
//@{
/**
 * @description A marker of a possible reset cause.
 */
#define RESET_CAUSE_SELF_WDOG      0
#define RESET_CAUSE_ASSERT_BIT     1
#define RESET_CAUSE_CSTACK_BIT     2
#define RESET_CAUSE_RSTACK_BIT     3
#define RESET_CAUSE_BOOTLOADER_BIT 4
#define RESET_CAUSE_SOFTWARE_BIT   5
//@}


/**
 * @description The start of RAM memory.
 *
 * There isn't currently an easy way to share this between the linker scripts
 * and the C code directly (since they aren't really segments). With that
 * said...  \b This must match the value in the linker scripts.
 */
#define SRAM_BASE       0x0100   // Start of ram memory


/**
 * @description The end of normal RAM memory.
 *
 * There isn't currently an easy way to share this between the linker scripts
 * and the C code directly (since they aren't really segments). With that
 * said...  \b This must match the value in the linker scripts.
 */
#define SRAM_END        0x10DD   // End of normal ram memory (~4k + SFR offset  - reserved ram)  


/**
 * @description The end of external RAM memory.
 *
 * There isn't currently an easy way to share this between the linker scripts
 * and the C code directly (since they aren't really segments). With that
 * said...  \b This must match the value in the linker scripts.
 */
#define EXT_SRAM_END    0x7FFF   // Largest possible external sram chip (32k)   


#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #ifndef __IAR_SYSTEMS_ASM__
  extern __no_init __root volatile int8u  emResetCause;
  extern __no_init __root volatile int8u  emResetCauseMarker;
  extern __no_init __root volatile int16u emResetRStack;
  extern __no_init __root volatile int16u emResetCStack;
  #endif
#endif //DOXYGEN_SHOULD_SKIP_THIS

