/***************************************************************************//**
 * @file
 * @brief PWM Peripheral API for EM35x from Silicon Labs.
 * @author kaiser.ren, renkaikaiser@163.com
 *******************************************************************************
 * @section License
 * <b>(C) Copyright by kaiser.ren, All rights reserved.</b>
 *******************************************************************************
 ******************************************************************************/
#ifndef _TIMER_H_
#define _TIMER_H_

/*----------------------------------------------------------------------------
 *        Typedef
 *----------------------------------------------------------------------------*/
/** Clock select. */
typedef enum
{
  timerRemapC4PA2    = 0,
  timerRemapC4PB4    = TIM_REMAPC4,
  timerRemapC3PA1    = 0,
  timerRemapC3PB3    = TIM_REMAPC3,
  timerRemapC2PA3    = 0,
  timerRemapC2PB2    = TIM_REMAPC2,
  timerRemapC1PA0    = 0,
  timerRemapC1PB1    = TIM_REMAPC1,
  timerTIMxCLKEN     = TIM_CLKMSKEN,
  timerTIMxCLK       = 3,
  timerREF32KHZ      = 2,
  timerCAL1KHZ       = 1,
  timerPCK12MHZ      = 0
} PWM_ClkSel_TypeDef;

/** PWM modulation **/
typedef struct{
	int32u freq;
	int32u duty;
}PWM_Modulation_TypeDef;

/** Prescaler. */
typedef enum
{
  timerPrescale1    = 0,     /**< Divide by 1. */
  timerPrescale2    = 1,     /**< Divide by 2. */
  timerPrescale4    = 2,     /**< Divide by 4. */
  timerPrescale8    = 3,     /**< Divide by 8. */
  timerPrescale16   = 4,    /**< Divide by 16. */
  timerPrescale32   = 5,    /**< Divide by 32. */
  timerPrescale64   = 6,    /**< Divide by 64. */
  timerPrescale128  = 7,   /**< Divide by 128. */
  timerPrescale256  = 8,   /**< Divide by 256. */
  timerPrescale512  = 9,   /**< Divide by 512. */
  timerPrescale1024 = 10,  /**< Divide by 1024. */
  timerPrescale2048  = 11,   /**< Divide by 2048. */
  timerPrescale4096  = 12,   /**< Divide by 4096. */
  timerPrescale8192  = 13,   /**< Divide by 8192. */
  timerPrescale16384  = 14,   /**< Divide by 16384. */
  timerPrescale32768  = 15,   /**< Divide by 32768. */
} PWM_Prescale_TypeDef;

typedef struct{
   int32u chTMR;
   int32u chCCR;
   PWM_ClkSel_TypeDef clkSel;
   PWM_Prescale_TypeDef prescale;
   PWM_Modulation_TypeDef mod;
}PWM_TypeDef;
/*----------------------------------------------------------------------------
 *        Prototype
 *----------------------------------------------------------------------------*/
void PWM_Init(const PWM_TypeDef* pwm);
void PWM_Adjust( const PWM_TypeDef* pwm );
void pwm_init(int32u freq , int32u duty);
#endif /*_LANGUAGE_H_*/
