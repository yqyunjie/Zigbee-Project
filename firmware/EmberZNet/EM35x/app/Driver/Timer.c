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
   halGpioConfig(PORTA_PIN(3), GPIOCFG_OUT_ALT );
   GPIO_PACLR = PA3;
   /*
   * 0: PCLK clock source
   * 1: calibrated 1 kHz
   * 2: 32 kHz reference clock
   * 3: TIM2CLK pin
   */
   TIM2_OR = 0 << TIM_EXTRIGSEL_BIT;

   /*
   * fCK_PSC / (2 ^ TIM_PSC), TIM_PSC = 0 ~ 15
   * prescaler = 1 ~ 32768
   */
   TIM2_PSC = 0 << TIM_PSC_BIT;

   /*
   *
   */
   TIM2_CR2 = 3 << TIM_MMS_BIT;

   /*
   * counter initial
   * CNT is TMR counter
   * ARR is auto load conter
   * CCR is compare/capture counter
   */
   TIM2_CNT = 0;
   TIM2_ARR = 100;
   TIM2_CCR2 = 90;

   /*
   * CCR2 Generation, PA3.
   * Update Generation .
   */
   TIM2_EGR = ( 1 << TIM_CC2G_BIT ) /*| ( 1 << TIM_UG_BIT )*/;

   /*
   * CCR2 output Polarity active low
   * CCR2 output enable
   */
   TIM2_CCER = ( 1 << TIM_CC2E_BIT ) /*| ( 1 << TIM_CC2P_BIT)*/;

   /*
   * 6: PWM mode 1
   */
   TIM2_CCMR1 = 6 << TIM_OC2M_BIT;

   /*
   * Capture or compare 2 interrupt enable
   * TIM2, Top -Level Set Interrupts enable
   *
   */
   INT_TIM2CFG = 1 << INT_TIMCC2IF_BIT;
   INT_CFGSET = INT_TIM2;	//enable timer2 interrupt

   /*
   * Auto Reload enable
   * TMR enable
   */
   TIM2_CR1 = TIM_ARBE | TIM_CEN;
}

void halTimer2Isr(void)
{

  //clear interrupt
  halToggleLed(BOARDLED0);
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
