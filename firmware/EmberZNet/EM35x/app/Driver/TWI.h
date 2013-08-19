// *******************************************************************
//  TWI.h
//
//  char repository.
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************
#ifndef _TWI_H_
#define _TWI_H_

/*----------------------------------------------------------------------------
 *        Macro
 *----------------------------------------------------------------------------*/
// device address on i2c bus
#define LM73_DEVICE_ADDRESS  (0x92 >> 1)
#define TSL2550_DEVICE_ADDRESS (0X39)
#define AD7414_DEVICE_ADDRESS (0x48)
   
// SCx block base address    
#define SC1_BASE_ADDR 0x4000C800u
#define SC2_BASE_ADDR 0x4000C000u
/*----------------------------------------------------------------------------
 *        Typedef
 *----------------------------------------------------------------------------*/
typedef enum{
   twi100Khzby12Mhz = 0,
   twi375Khzby12Mhz = 1,
   twi400Khzby12Mhz = 2
}TWI_BusFreq_TypeDef;

typedef enum{
   twiSC1 = SC1_BASE_ADDR,
   twiSC2 = SC2_BASE_ADDR
}TWI_SCx_TypeDef;

/*----------------------------------------------------------------------------
 *        Prototype
 *----------------------------------------------------------------------------*/
void twi_init(void);
void twi_wr(int32u addr, int8u data);
/******************************************************************************\
 * twi read action .
\******************************************************************************/
void twi_rd(int32u addr);
#endif /*_TWI_H_*/
