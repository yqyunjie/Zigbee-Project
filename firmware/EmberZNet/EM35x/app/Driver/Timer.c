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
   TIM2_ARR_REG = 2;	//auto load value
   TIM2_OR_REG = 2;		//32k clock source
   TIM2_PSC_REG = 0x2 << TIM_PSC_BIT;	//prescaler, 1 << 2, 4
   TIM2_CR2_REG = 0x4 << TIM_MMS_BIT;

   TIM2_CNT_REG = 0;
   TIM2_ARR_REG = 2;	//auto load value

   TIM2_EGR_REG = TIM_CC2G_BITS << TIM_CC2G_BIT;	//CCR2IF set if output, PA3

   TIM1_CCMR1_REG = 3 << TIM_OC2M_BIT;	//toggle mode if match

   INT_TIM2CFG_REG = 1 << INT_TIMCC2IF_BIT;		//Capture or compare 2 interrupt enable

   INT_CFGSET_REG = INT_TIM2;	//enable timer2 interrupt

   TIM2_CCER = TIM_CC2E;	//enable output

   TIM2_CR1_REG = TIM_ARBE | TIM_CEN;
}

void halTimer2Isr(void)
{

  //clear interrupt
  INT_TIM2FLAG = 0xFFFFFFFF;
}

//eof
