/** @file hal/host/cortexm3/adc.c
 * @brief STM32F102RET host ADC driver operating on top of ST's Standard
 * Peripheral Library.
 *
 * <!-- Copyright 2010 by Ember Corporation. All rights reserved.        *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/error.h"
#include "hal/hal.h"


void halInternalInitAdc(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  ADC_InitTypeDef ADC_InitStructure;
  
  //Configure TEMP_ENABLE GPIO as push-pull output, defaulting to active/on
  GPIO_WriteBit(TEMP_ENABLE_PORT, TEMP_ENABLE_PIN, Bit_SET);
  GPIO_InitStructure.GPIO_Pin = TEMP_ENABLE_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(TEMP_ENABLE_PORT, &GPIO_InitStructure);
  
  //Configure TEMP_SENSE GPIO as analog input
  GPIO_InitStructure.GPIO_Pin = TEMP_SENSOR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(TEMP_SENSOR_PORT, &GPIO_InitStructure);
  
  //Divide down the APB2 clock (72MHz) to get an ADC clock of 12MHz (<= 14MHz)
  RCC_ADCCLKConfig(RCC_PCLK2_Div6);
  
  //Configure ADC to be in independent, single conversion mode
  ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
  ADC_InitStructure.ADC_ScanConvMode = DISABLE;
  ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
  ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
  ADC_InitStructure.ADC_NbrOfChannel = 1;
  ADC_Init(TEMP_SENSOR_ADC, &ADC_InitStructure);
  
  //Configure sample times to longest (recommended for temp measure)
  ADC_RegularChannelConfig(TEMP_SENSOR_ADC,
                           TEMP_SENSOR_ADC_CHAN,
                           1,
                           ADC_SampleTime_239Cycles5);
  
  //Enable ADC
  ADC_Cmd(TEMP_SENSOR_ADC, ENABLE);
  
  //Reset ADC calibration registers
  ADC_ResetCalibration(TEMP_SENSOR_ADC);
  
  //Wait for cal reset to complete
  while(ADC_GetResetCalibrationStatus(TEMP_SENSOR_ADC) == SET) {}
  
  //Calibrate the ADC
  ADC_StartCalibration(TEMP_SENSOR_ADC);
  
  //Wait for cal to complete
  while(ADC_GetCalibrationStatus(TEMP_SENSOR_ADC) == SET) {}
  
  //This driver does not use interrupts; it's purely polling.
}


//NOTE: This implementation of ADC sampling is blocking
int16u halSampleAdc(void)
{
  int16u retVal;
  
  //Start conversion on select ADC peripheral
  ADC_SoftwareStartConvCmd(TEMP_SENSOR_ADC, ENABLE);
  
  //Loop while we wait for conversion to finish
  while(ADC_GetFlagStatus(TEMP_SENSOR_ADC, ADC_FLAG_EOC) == RESET);
  
  //Get conversion value
  //The end of conversion (EOC) flag is reset in the process
  retVal = ADC_GetConversionValue(TEMP_SENSOR_ADC);
  
  return retVal;
}

//Voltage output is in a fixed point 4 digit format. I.E. 860 is 0.0860
int16s halConvertValueToVolts(int16u value)
{
  int32u N;
  int16s V;
  
  //VREF is 3300.0 mV
  N = (int32u)(((int32u)value) * ((int32u)33000));
  V = N / 0xFFF;
  
  return V;
}

