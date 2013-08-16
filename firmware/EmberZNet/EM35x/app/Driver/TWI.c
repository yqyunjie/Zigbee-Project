// *******************************************************************
//  TWI.c
//
//  TWI Driver.c
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************

#include <stdio.h>
#include "app/Driver/common.h"
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
   /*
    * LIN: 14  EXP: 3  100kbps
    * LIN: 15  EXP: 1  375kbps
    * LIN: 14  EXP: 1  400kbps
    */
   SC2_RATELIN = 14; //100kbps
   SC2_RATEEXP = 3;

   /*
    * SDA and SCL MUST BE open-drain.
    */
   halGpioConfig(TWI_SDA(A, 1), GPIOCFG_OUT_ALT_OD ); //SDA PA1
   halGpioConfig(TWI_SCL(A, 2), GPIOCFG_OUT_ALT_OD ); //SCL PA2

   /*
    * Enable INT
    */
   //SC2_INTMODE = SC_TXIDLELEVEL | SC_TXFREELEVEL | SC_RXVALLEVEL;
   //INT_SC2CFG = INT_SCRXFIN | INT_SCTXFIN | INT_SCCMDFIN | INT_SCNAK;
   //INT_CFGSET = INT_SC2;

   /*
    * Enable TWI mode.
    */
   SC2_MODE = SC2_MODE_I2C;
}

/******************************************************************************\
 * twi write one byte.
\******************************************************************************/
void twi_wr(int32u addr, int8u data)
{
   int8u i;

   SC2_TWICTRL1 = SC_TWISTART;   //start bit
   while( !( SC2_TWISTAT & SC_TWICMDFIN) );  //wait for S/P complete

   SC2_DATA = addr << 1;
   SC2_TWICTRL1 = SC_TWISEND;   //start send
   while( !( SC2_TWISTAT & SC_TWITXFIN ) );
   i = 100;
   while(i) i--;

   SC2_DATA = data;
   SC2_TWICTRL1 = SC_TWISEND;   //start send
   while( !( SC2_TWISTAT & SC_TWITXFIN ) );

   SC2_TWICTRL1 = SC_TWISTOP;   //start stop

	i = 50;
   	while(i) i--;
}

/******************************************************************************\
 * twi read action .
\******************************************************************************/
void twi_rd(int32u addr)
{
  	int8u uaddr;
  	int8u msb, lsb;

	SC2_TWICTRL1 = SC_TWISTART;   //start bit
	while( !( SC2_TWISTAT & SC_TWICMDFIN) );  //wait for S/P complete

	uaddr = ( addr << 1 ) | 0x01;
	SC2_DATA = uaddr;
   	SC2_TWICTRL1 = SC_TWISEND;   //start send
	while( !( SC2_TWISTAT & SC_TWITXFIN ) );

	SC2_TWICTRL1 = SC_TWIRECV;   //start receive
	while( !( SC2_TWISTAT & SC_TWIRXFIN ) );
	msb = SC2_DATA;
	SC2_TWICTRL2 = SC_TWIACK;	//ACK

	SC2_TWICTRL1 = SC_TWIRECV;   //start receive
	while( !( SC2_TWISTAT & SC_TWIRXFIN ) );
	lsb = SC2_DATA;

	SC2_TWICTRL1 = SC_TWISTOP;   //start stop

    emberSerialPrintf(APP_SERIAL, "TWI Read addr = 0x%X msb = 0x%X  lsb = 0x%X\r\n", (int8u)addr, msb, lsb);
}

void halSc2Isr(void)
{
   twi_status = SC2_TWISTAT;
   INT_SC2FLAG = 0xFFFFFFFF;
}
//eof
