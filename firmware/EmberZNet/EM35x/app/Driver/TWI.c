// *******************************************************************
//  TWI.c
//
//  TWI Driver.c
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
 *        Macro
 *----------------------------------------------------------------------------*/
#define TWI_SDA(port, bit) PORT##port##_PIN(bit)
#define TWI_SCL(port, bit) PORT##port##_PIN(bit)
#define SC2_PORT_SDA A
#define SC2_BIT_SDA 1

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/
int32u twi_status;
/*----------------------------------------------------------------------------
 *        Function(s)
 *----------------------------------------------------------------------------*/
void twi_init(void)
{
   halGpioConfig(TWI_SDA(A, 1), GPIOCFG_OUT_ALT_OD ); //SDA PA1
   halGpioConfig(TWI_SCL(A, 2), GPIOCFG_OUT_ALT_OD ); //SCL PA2

   SC2_MODE = SC2_MODE_I2C;
   /*
    * LIN: 14  EXP: 3  100kbps
    * LIN: 15  EXP: 1  375kbps
    * LIN: 14  EXP: 1  400kbps
    */
   SC2_RATELIN = 14; //100kbps
   SC2_RATEEXP = 3;

   SC2_INTMODE = SC_TXIDLELEVEL | SC_TXFREELEVEL | SC_RXVALLEVEL;
   INT_SC2CFG = INT_SCRXFIN | INT_SCTXFIN | INT_SCCMDFIN | INT_SCNAK;
   INT_CFGSET = INT_SC2;
}

void twi_wr(int32u addr, int8u data)
{
   //twi_status = 0;
   SC2_TWICTRL1 = SC_TWISTART;   //start bit

   //while( !( twi_status & SC_TWICMDFIN) );  //wait for S/P complete

   //if (len ){
   //   SC2_TWICTRL1 = SC_TWISEND;   //start send
   //}
 	//while(len){
      SC2_DATA = addr << 1;
	  SC2_TWICTRL1 = SC_TWISEND;   //start send
      //len--;
      while( !( twi_status & SC_TWITXFIN ) );

	  SC2_DATA = data;
	  SC2_TWICTRL1 = SC_TWISEND;   //start send
      //len--;
      while( !( twi_status & SC_TWITXFIN ) );
   //}

   SC2_TWICTRL1 = SC_TWISTOP;   //start stop
}

void halSc2Isr(void)
{
   twi_status = SC2_TWISTAT;
   INT_SC2FLAG = 0xFFFFFFFF;
}
//eof
