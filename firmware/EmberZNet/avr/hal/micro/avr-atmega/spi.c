/** @file hal/micro/avr-atmega/spi.c
 * @brief Serial Peripheral Interface functions
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 * <!-- Copyright 2005 by Ember Corporation. All rights reserved.-->          
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h" 

void halCommonSpiWrite(int8u data)
{
  int8u tempbyte;

  tempbyte = data;  

  SPDR = tempbyte;
  while(!(SPSR & (BIT(7))))
  ;
  SPSR |= BIT(7);// SPIF
     
}

int8u halCommonSpiRead(void)
{
  int8u data = 0;
  int8u tempbyte;
 
  tempbyte = 0xFF;  

  SPDR = tempbyte;
  while(!(SPSR & (BIT(7))))
  ;
  SPSR |= BIT(7);// SPIF
     
  data = SPDR;

  return data;
}

int8u halCommonSpiReadWrite(int8u data)
{
   int8u statusdata = 0;
   int8u tempbyte;
     
   tempbyte = data;  
   
   SPDR = tempbyte;
   while(!(SPSR & (BIT(7))))
   ;
   SPSR |= BIT(7);// SPIF
   
   statusdata = SPDR;
  
  return statusdata;
}
