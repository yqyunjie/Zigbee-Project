/***************************************************************************//**
 * @file
 * @brief TWI Peripheral API for EM35x from Silicon Labs.
 * @author kaiser.ren, renkaikaiser@163.com
 *******************************************************************************
 * @section License
 * <b>(C) Copyright by kaiser.ren, All rights reserved.</b>
 *******************************************************************************
 ******************************************************************************/

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

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/
/** TWI peripheral GPIOx for SCx . */
const int16u gpioTWI[2][2] =
{
	{PORTA_PIN(1), PORTA_PIN(2)}      ,
	{PORTB_PIN(1), PORTB_PIN(2)}
};

/** TWI peripheral speed definition for SCx . */
const int32u freqTWI[][3] =
{
  { 11, 10,  7500},   //1000hz
  { 15, 7 ,  1000},   //6000hz
  { 15, 6 ,  500},	 //12KHz
  { 14, 5 ,  250},  	//25KHz
  { 14, 4, 	 125},		//50KHz
  { 14, 3, 	 60}, 	//100KHz
  { 15, 1,   60}, 	//375KHz
  { 14, 1,   60}		//400KHz
};

static int32u twiWaitPeriod;
/*----------------------------------------------------------------------------
 *        Function(s)
 *----------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief
 *   Set current configured TWI bus frequency.
 *
 * @details
 *   This frequency is only of relevance when acting as master.
 *
 * @param[in] twi
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_BusFreqSet(TWI_SCx_TypeDef ch, TWI_BusFreq_TypeDef speed)
{
   int32u offset = ch - SC2_BASE_ADDR;

   *(volatile int32u*)(SC2_RATELIN_ADDR + offset) = freqTWI[speed][0];
   *(volatile int32u*)(SC2_RATEEXP_ADDR + offset) = freqTWI[speed][1];
	twiWaitPeriod = freqTWI[speed][2];
}

/***************************************************************************//**
 * @brief
 *   Initialize TWI.
 *
 * @param[in] i2c
 *   TBD.
 *
 * @param[in] init
 *   NULL
 ******************************************************************************/
void TWI_Init(TWI_SCx_TypeDef ch)
{
   int32u offset = ch - SC2_BASE_ADDR;
   int8u io = offset / ( SC1_BASE_ADDR - SC2_BASE_ADDR);

   /* SDA and SCL MUST be open-drain. */
   halGpioConfig( gpioTWI[io][0], GPIOCFG_OUT_ALT_OD );
   halGpioConfig( gpioTWI[io][1], GPIOCFG_OUT_ALT_OD );

   /* TWI clock setup. */
   TWI_BusFreqSet(ch, twiSCLK_100KHZ) ;

   *(volatile int32u*)( SC2_MODE_ADDR + offset ) = SC2_MODE_I2C;
}

/******************************************************************************\
 * twi initial..
\******************************************************************************/
#if 0
void twi_init(void)
{
   /*
    * LIN: 14  EXP: 3  100kbps
    * LIN: 15  EXP: 1  375kbps
    * LIN: 14  EXP: 1  400kbps
    */
   SC2_RATELIN = 14; //100kbps
   SC2_RATEEXP = 1;

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
#endif

/***************************************************************************//**
 * @brief
 *  TWI Write (single master mode only).
 *
 * @details
 *
 * @param[in] TWI
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_Wr(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data)
{
	int8u* ptr = data;
	int32u offset = ch - SC2_BASE_ADDR;

   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTART;   						//start bit
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) );  	//wait for S/P complete

   *( volatile int32u* )( SC2_DATA_ADDR + offset ) = addr << 1;                   			//load address = addr << 1;
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   							//start send
   while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );

	//multi-byte loop
	while(len){
   	*( volatile int32u* )( SC2_DATA_ADDR + offset ) = *ptr;											//load data
   	*( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   							//start send
   	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );		//wait for
		len--;
		ptr++;
   }

   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTOP;   							//start stop
   while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) ); 	//wait for S/P complete

}

/***************************************************************************//**
 * @brief
 *  TWI Read (single master mode only).
 *
 * @details
 *
 * @param[in] TWI
 *   TBD
 *
 * @return
 *   NULL
 ******************************************************************************/
void TWI_Rd(TWI_SCx_TypeDef ch, int8u addr, int8u len, int8u* data)
{
  	int8u uaddr;
	int8u* ptr = data;
	int16u i;
	int32u offset = ch - SC2_BASE_ADDR;

	*( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTART;   						//start bit
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWICMDFIN) );  	//wait for S/P complete

	uaddr = ( addr << 1 ) | 0x01;																				//address with RD bit flag
	*( volatile int32u* )( SC2_DATA_ADDR + offset ) = uaddr;                   				//load address
   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISEND;   							//start send
	while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWITXFIN ) );		//wait for WR finished
   emberSerialPrintf(APP_SERIAL, "0x%X ", (int8u)addr);

	//multi-byte loop
   while(len){
      *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWIRECV;   						//start receive
      while( !( ( *( volatile int32u* )( SC2_TWISTAT_ADDR + offset ) ) & SC_TWIRXFIN ) ); //wait for REV finished
      *ptr = *( volatile int32u* )( SC2_DATA_ADDR + offset );										//read data from TWI bus
      emberSerialPrintf(APP_SERIAL, "0x%X ", *ptr);
      ptr++;
      len--;
      if(len > 0)
         *( volatile int32u* )( SC2_TWICTRL2_ADDR + offset ) = SC_TWIACK;						//ACK
   }

   *( volatile int32u* )( SC2_TWICTRL1_ADDR + offset ) = SC_TWISTOP;   //start stop
   i = twiWaitPeriod;
	while(i){i--;}
   emberSerialPrintf(APP_SERIAL, "\r\n");
}


/*void TWI_Transfer(TWI_WRBuf_TypeDef* rdBuf)
{
   int16u i;
  	int8u uaddr = rdBuf->addr;
	int8u len0 = rdBuf->buf[0].len;
  	int8u* ptr0 = rdBuf->buf[0].data;

	SC2_TWICTRL1 = SC_TWISTART;   				//start bit
	while( !( SC2_TWISTAT & SC_TWICMDFIN) );  //wait for S/P complete

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
   emberSerialPrintf(APP_SERIAL, "\r\n");
}*/

//eof
