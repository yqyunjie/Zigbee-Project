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
#include "Timer.h"
#include "hal/micro/led.h"

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/
const int16u gpioPWM[2][4] =
{
  	{PORTB_PIN(6), PORTB_PIN(7), PORTA_PIN(6),PORTA_PIN(7)},
	{PORTA_PIN(0), PORTA_PIN(3), PORTA_PIN(1),PORTA_PIN(2)}
};

/*----------------------------------------------------------------------------
 *        Function(s)
 *----------------------------------------------------------------------------*/
void PWM_Adjust( const PWM_TypeDef* pwm )
{
   int32u offset = BLOCK_TIM2_BASE - BLOCK_TIM1_BASE;
   int32u chT = pwm->chTMR - 1;
   int32u chC = pwm->chCCR - 1;
	/*
   * counter initial
   * CNT is TMR counter
   * ARR is auto load conter
   * CCR is compare/capture counter
   */
   *( (volatile int32u *)(TIM1_CNT_ADDR + offset * chT) ) = 0;
   *((volatile int32u *) ( TIM1_ARR_ADDR + offset * chT ) ) = 12000000ul/pwm->mod.freq;
   *((volatile int32u *) ( ( TIM1_CCR1_ADDR + chC*4) + offset * chT ) ) = 120000ul * pwm->mod.duty /pwm->mod.freq;
}

void PWM_Init(const PWM_TypeDef* pwm)
{
   int32u offset = BLOCK_TIM2_BASE - BLOCK_TIM1_BASE;
   int32u chT = pwm->chTMR - 1;
   int32u chC = pwm->chCCR - 1;
   int32u addr, value;

   halGpioConfig(gpioPWM[chT][chC], GPIOCFG_OUT_ALT );

   /* clock source   */
   *((volatile int32u *)(TIM1_OR_ADDR + offset * chT) ) = pwm->clkSel;

   /** prescaler = 1 ~ 32768 */
   *((volatile int32u *)( TIM1_PSC_ADDR + offset * chT) ) = pwm->prescale ;


   /*
   * CCR2 output Polarity active low
   * CCR2 output enable
   */
   value = 1 << (chC*4) ;
   *((volatile int32u *)( TIM1_CCER_ADDR + offset * chT) ) = value ;

   /*
   * 6: PWM mode 1
   */
   addr = (TIM1_CCMR1_ADDR + chC/2*4) + offset * chT;
   value = 6 << (TIM_OC1M_BIT + chC % 2 * 8);
   *(volatile int32u*)addr = value;

   /** adjust pwm modulation.  */
   PWM_Adjust(pwm);

   /*
   * Capture or compare 2 interrupt enable
   * TIM2, Top -Level Set Interrupts enable
   */
   if( pwm->chTMR == 1 ){
      INT_TIM1CFG = 1 << pwm->chCCR;
      INT_CFGSET = INT_TIM1;	//enable timer1 interrupt
   }
   else if( 2 == pwm->chTMR ){
      INT_TIM2CFG = 1 << pwm->chCCR;
      INT_CFGSET = INT_TIM2;	//enable timer2 interrupt
   }

   /** Auto Reload enable and TMR enable*/
   *((volatile int32u *)( TIM1_CR1_ADDR + offset * chT ) ) = TIM_ARBE | TIM_CEN;
}

/*
* Clock Source is PCK 12Mhz and prescaler is 1
* 1 Ticker = 1/12us
* ARR = 50 ~ 0xFFFF 240kHz ~ 183Hz
*/
void pwm_init(int32u freq , int32u duty)
{
   halGpioConfig(PORTA_PIN(3), GPIOCFG_OUT_ALT );
   GPIO_PACLR = PA3;
   /*
   * 0: PCLK clock source
   * 1: calibrated 1 kHz
   * 2: 32 kHz reference clock
   * 3: TIM2CLK pin
   */
   TIM2_OR = 0 << TIM_EXTRIGSEL_BIT;   //12Mhz

   /*
   * fCK_PSC / (2 ^ TIM_PSC), TIM_PSC = 0 ~ 15
   * prescaler = 1 ~ 32768
   */
   TIM2_PSC = 0 << TIM_PSC_BIT;

   /*
   *
   */
   //TIM2_CR2 = 3 << TIM_MMS_BIT;

   /*
   * counter initial
   * CNT is TMR counter
   * ARR is auto load conter
   * CCR is compare/capture counter
   */
   TIM2_CNT = 0;
   TIM2_ARR = 12000000ul/freq;
   //TIM2_ARR = 100; //115khz
   TIM2_CCR2 = 120000ul * duty /freq;
   //TIM2_CCR2 = 0xefff;

   /*
   * CCR2 Generation, PA3.
   * Update Generation .
   */
   //TIM2_EGR = ( 1 << TIM_CC2G_BIT ) /*| ( 1 << TIM_UG_BIT )*/;

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

void halTimer1Isr(void)
{

  //clear interrupt
  halToggleLed(BOARDLED1);
  INT_TIM1FLAG = 0xFFFFFFFF;
}

void halTimer2Isr(void)
{

  //clear interrupt
  halToggleLed(BOARDLED0);
  INT_TIM2FLAG = 0xFFFFFFFF;
}
//eof
