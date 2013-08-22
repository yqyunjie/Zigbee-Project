// *******************************************************************
//  TWI.c
//
//  TWI Driver.c
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************

#include <stdio.h>
#include "app/Driver/common.h"
#include "app/Driver/TWI.h"
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
const int16u gpioTWI[2][2] =
{
  	{PORTB_PIN(1), PORTB_PIN(2)},
	{PORTA_PIN(1), PORTA_PIN(2)}
};

const int8u freqTWI[3][2] =
{
  { 14, 3 },   //100khz
  { 15, 1 },   //375khz
  { 14, 3 }
};

//int32u Sc2_status;
/*----------------------------------------------------------------------------
 *        Function(s)
 *----------------------------------------------------------------------------*/
/******************************************************************************\
 * TWI Initial.
\******************************************************************************/
void TWI_BusFreqSet(TWI_SCx_TypeDef ch, TWI_BusFreq_TypeDef speed)
{
   int32u offset = ch - SC2_BASE_ADDR;

   *(volatile int32u*)(SC2_RATELIN_ADDR + offset) = freqTWI[speed][0];
   *(volatile int32u*)(SC2_RATEEXP_ADDR + offset) = freqTWI[speed][1];
}

/******************************************************************************\
 * TWI Initial.
\******************************************************************************/
void TWI_Init(TWI_SCx_TypeDef ch)
{
   int32u offset = ch - SC2_BASE_ADDR;
   int8u io = offset / ( SC1_BASE_ADDR - SC2_BASE_ADDR);

   /* SDA and SCL MUST be open-drain. */
   halGpioConfig( gpioTWI[io][0], GPIOCFG_OUT_ALT_OD );
   halGpioConfig( gpioTWI[io][1], GPIOCFG_OUT_ALT_OD );

   /* TWI clock setup. */
   TWI_BusFreqSet(ch, twi100Khzby12Mhz) ;

   *(volatile int32u*)( SC2_MODE_ADDR + offset ) = SC2_MODE_I2C;
}

/******************************************************************************\
 * twi initial..
\******************************************************************************/
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
 * twi write by mutiple-tyes.
\******************************************************************************/
void twi_wr(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data)
{
	int8u* ptr = data;
	int32u offset = ch - SC2_BASE_ADDR;

	//Sc2_status = 0;
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTART;   						//start bit
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) );  	//wait for S/P complete

   *( volatile int32u* )( SC2_DATA_ADDR + offset ) = addr << 1;                   					//load address = addr << 1;
	//Sc2_status = 0;
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   							//start send
   while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );

	while(len){
   	*( volatile int32u* )( SC2_DATA_ADDR + offset ) = *ptr;											//load data
		//Sc2_status = 0;
   	*( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   																				//start send
   	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );		//wait for
		len--;
		ptr++;
   }

	//Sc2_status = 0;
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTOP;   							//start stop
   while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) ); 	//wait for S/P complete

}

/******************************************************************************\
 * twi read action .
\******************************************************************************/
void twi_rd(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data)
{
  	int8u uaddr;
	int8u* ptr = data;
	int16u i;
	int32u offset = ch - SC2_BASE_ADDR;

	*( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTART;   				//start bit
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) );  //wait for S/P complete

	uaddr = ( addr << 1 ) | 0x01;																	//address with RD bit flag
	*( volatile int32u* )( SC2_DATA_ADDR + offset ) = uaddr;                   	//load address
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   				//start send
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );		//wait for WR finished
   emberSerialPrintf(APP_SERIAL, "0x%X ", (int8u)addr);

   while(len){
      *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWIRECV;   //start receive
      while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWIRXFIN ) );
      *ptr = *( volatile int32u* )( SC2_DATA_ADDR + offset );
      emberSerialPrintf(APP_SERIAL, "0x%X ", *ptr);
      ptr++;
      len--;
      if(len > 0)
         *( volatile int32u* )( SC2_TWICTRL2_ADDR + offset ) = SC_TWIACK;	//ACK
   }

   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTOP;   //start stop
   i = 300;
	while(i){i--;}
   emberSerialPrintf(APP_SERIAL, "\r\n");
}

void TWI_RDx(TWI_WRBuf_TypeDef* rdBuf)
{
   int16u i;
  	int8u uaddr = rdBuf->addr;
	int8u len0 = rdBuf->buf[0].len;
  	int8u* ptr0 = rdBuf->buf[0].data;

	SC2_TWICTRL1 = SC_TWISTART;   				//start bit
	while( !( SC2_TWISTAT & SC_TWICMDFIN) );  //wait for S/P complete
/*
	uaddr = ( uaddr << 1 ) | 0x01;
	SC2_DATA = uaddr;
   SC2_TWICTRL1 = SC_TWISEND;   //start send
	while( !( SC2_TWISTAT & SC_TWITXFIN ) );
   emberSerialPrintf(APP_SERIAL, "TWI Read addr = 0x%X ", (int8u)uaddr);
   while(len){
      SC2_TWICTRL1 = SC_TWIRECV;   //start receive
      while( !( SC2_TWISTAT & SC_TWIRXFIN ) );
      *ptr = SC2_DATA;
      emberSerialPrintf(APP_SERIAL, "0x%X ", *ptr);
      ptr++;
      len--;
      if(len > 0)
         SC2_TWICTRL2 = SC_TWIACK;	//ACK
   }

   SC2_TWICTRL1 = SC_TWISTOP;   //start stop
   i = 300;
	while(i){i--;}
   emberSerialPrintf(APP_SERIAL, "\r\n"); */
}

//void halSc2Isr(void)
//{
//   Sc2_status = SC2_TWISTAT;
//   INT_SC2FLAG = 0xFFFFFFFF;
//}
//eof
