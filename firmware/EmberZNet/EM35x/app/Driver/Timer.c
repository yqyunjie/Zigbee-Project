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
//@code
#define PWM_PIN       PORTA_PIN(7)
#define SET_PWM_PIN() GPIO_PASET = PA7
#define CLR_PWM_PIN() GPIO_PACLR = PA7

void PWM_GpoiInit(void)
{


    halGpioConfig(PWM_PIN, GPIOCFG_OUT_ALT );
    CLR_PWM_PIN();
}

void PWM_Init(void)
{
	PWM_GpoiInit();
    GPIO_DBGCFG &= (~GPIO_EXTREGEN_MASK);

    TIM1_EGR |= TIM_UG;

    //According to emulator.h, buzzer is on pin 15 which is mapped
    //to channel 2 of TMR1
    TIM1_OR = 0; //use 12MHz clock
    TIM1_PSC = 5; // 2^5=32 -> 12MHz/32 = 375kHz = 2.6us tick
    //TIM1_EGR = 1; // trigger update event to load new prescaler value
    //TIM1_CCMR1  = 0; //start from a zeroed configuration
    TIM1_CCMR2  = 0; //start from a zeroed configuration
    //Output waveform: toggle on CNT reaching TOP
    //TIM1_CCMR1 |= (0x6 << TIM_OC2M_BIT);
    TIM1_CCMR2 |= (0x6 << TIM_OC4M_BIT);

    TIM1_ARR = 100<<1; //magical conversion to match our tick period
    //TIM1_CCR2 = 30<<1;
    TIM1_CCR4 = 30<<1;
    TIM1_CNT = 0; //force the counter back to zero to prevent missing BUZZER_TOP

    TIM1_CCER = TIM_CC4E; //enable output on channel 4
    //TIM1_CCER |= TIM_CC2E; //enable output on channel 4
    TIM1_CR1 |= TIM_CEN; //enable counting
}
//eof
