// *******************************************************************
//  Timer.c
//
//  Timer Driver.c
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************

#include <stdio.h>
#include PLATFORM_HEADER
#if !defined(MINIMAL_HAL) && defined(BOARD_HEADER)
  // full hal needs the board header to get pulled in
  #include "hal/micro/micro.h"
  #include BOARD_HEADER
#endif

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 *        Function(s)
 *----------------------------------------------------------------------------*/
void tmr_init(void)
{
   TIM2_ARR_REG = 0x1000;
   TIM2_PSC_REG = 0xF << TIM_PSC_BIT;
   TIM2_CR2_REG = 0x4 << TIM_MMS_BIT;
   TIM2_CR1_REG = TIM_ARBE | TIM_CEN;  
}


//eof
