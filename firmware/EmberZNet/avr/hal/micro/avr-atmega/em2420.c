/*
 * File: em2420.c
 * Description: Hal support functions for the em2420 radio
 *
 * Reminder: The term SFD refers to the Start Frame Delimiter.
 *
 * Author(s): Lee Taylor, lee@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <inavr.h>
#include <ioavr.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h" 
#include "hal/micro/em2420.h"


#ifdef PACKET_TRACE
  #define PACKET_TRACE_ASSERT_CS()                        \
    do {                                                  \
        SETBIT(PACKET_TRACE_CS_DDR, PACKET_TRACE_CS);     \
        CLEARBIT(PACKET_TRACE_CS_PORT, PACKET_TRACE_CS);  \
       } while(0)

  #define PACKET_TRACE_DEASSERT_CS()                    \
    do {                                                \
        SETBIT(PACKET_TRACE_CS_PORT, PACKET_TRACE_CS);  \
       } while(0)
#else
  #define PACKET_TRACE_ASSERT_CS()
  #define PACKET_TRACE_DEASSERT_CS()
#endif

//sorry, this looks bad
//the timing below is too precise to handle any other way
//this was all manually tweaked/verified on a scope
//-WBB 9/28/2004
#define radPowerOn()                    \
  do{                                   \
    int16u cycles = 0x14d;              \
    int8u i;                            \
    CLEARBIT(RAD_VREG_PORT,RAD_VREG);   \
    SETBIT(RAD_VREG_DDR,RAD_VREG);      \
    while(cycles--) {                   \
      SETBIT(RAD_VREG_PORT,RAD_VREG);   \
      i=6;                              \
      do {                              \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
      } while(--i);                     \
      CLEARBIT(RAD_VREG_PORT,RAD_VREG); \
      i=0x14;                           \
      do {                              \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
        asm("nop");                     \
      } while(--i);                     \
    }                                   \
    SETBIT(RAD_VREG_PORT,RAD_VREG);     \
  }while(0)

#define radPowerOff()                   \
  do {                                  \
    CLEARBIT(RAD_VREG_PORT,RAD_VREG);   \
    SETBIT(RAD_VREG_DDR,RAD_VREG);      \
  } while(0)

void emberHalRadioHoldReset(void)
{
  CLEARBIT(RAD_RESETn_PORT,RAD_RESETn);
  SETBIT(RAD_RESETn_DDR,RAD_RESETn);
}

void emberHalRadioReleaseReset(void)
{
  SETBIT(RAD_RESETn_PORT,RAD_RESETn);
}

void halStack2420Init(void)
{  
  emberHalRadioHoldReset();
 
  radPowerOn();
 
  halCommonDelayMicroseconds(7500);  // fogbugz: 602
  
  // set PSEL high
  SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  // set PCLK low
  SETBIT(RAD_SPI_PORT,RAD_SPI_PCLK);
  SETBIT(RAD_SPI_PORT,RAD_SPI_PDI);
  // set PCLK, PALE, and PDATA as outputs
  SETBIT(RAD_SPI_DDR,RAD_SPI_PSEL);
  SETBIT(RAD_SPI_DDR,RAD_SPI_PCLK); 
  SETBIT(RAD_SPI_DDR,RAD_SPI_PDI);
  // Enable SPI, slave, setup(falling)-sample(rising), msb first, sc/4 (1mbit), no interrupt
  SPCR = BIT(SPE) | /*BIT(DORD) | */BIT(MSTR);/* | BIT(CPHA)*/;
#ifndef PACKET_TRACE
  // Note:  not including this will only slow byte downloads by about 20%.
  SPSR = BIT(SPI2X); // crank spi to max speed (sclk/2) or 2mbit (assuming 4Mhz sclk)
#endif
  
  emberHalRadioReleaseReset();  

  // Initialize packet trace pins.
  halStack2420InitPacketTrace();
}

void halStack2420PowerDown(void)
{
  CLEARBIT(RAD_RESETn_PORT,RAD_RESETn);

  // PSEL (chip select) is active low, so set it high to minimize current draw
  SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  CLEARBIT(RAD_SPI_PORT,RAD_SPI_PCLK);
  CLEARBIT(RAD_SPI_PORT,RAD_SPI_PDI);
  //0220 boards still need to have the radio power regulator disabled.
  #ifdef  BOARD_DEV0220
    SETBIT(PORTG,2);
  #endif//BOARD_DEV0220
  radPowerOff();
}

boolean halStack2420GetFifoPin(void)
{
  return (RAD_FIFO_PIN & BIT(RAD_FIFO));
}

boolean halStack2420GetFifopPin(void)
{
  return (RAD_FIFOP_PIN & BIT(RAD_FIFOP));
}

boolean halStack2420GetCcaPin(void)
{
  return (RAD_CCA_PIN & BIT(RAD_CCA));
}

#ifdef HALTEST
   boolean halStack2420GetSfdPin(void)
   {
     return (RAD_SFD_PIN & BIT(RAD_SFD));
   }
#endif

int8u halStack2420SetRegister(int16u addr, int16u data)
{
    int8u status;
    int8u waddr;
    int8u datalsb,datamsb;

    waddr = addr;
    waddr &= ~(BIT(7));
    waddr &= ~(BIT(6));
    
    datalsb = data;
    datamsb = (data>>8);
    
    ATOMIC_LITE(
      CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
      status = halCommonSpiReadWrite(waddr);
      halCommonSpiWrite(datamsb);
      halCommonSpiWrite(datalsb);
      SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    )
    return status;
}
/*
int8u halStack2420SetRegister(int16u addr, int8u datalsb, int8u datamsb)
{
    int8u status;
    int8u waddr;

    waddr = addr;
    waddr &= ~(BIT(7));
    waddr &= ~(BIT(6));
      
    ATOMIC_LITE(
      CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
      status = halCommonSpiReadWrite(waddr);
      halCommonSpiWrite(datamsb);
      halCommonSpiWrite(datalsb);
      SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    )
    return status;
}
*/
int16u halStack2420GetRegister(int8u addr)
{
  int16u val;
  int16u tempval;
  int8u waddr;

  waddr = addr;
  waddr &= ~(BIT(7));
  waddr |= (BIT(6));
  
  ATOMIC_LITE(
    CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    halCommonSpiReadWrite(waddr);
    tempval = halCommonSpiRead();
    tempval = (tempval<<8);
    val = halCommonSpiRead();
    val |= tempval;
    SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  )
  return val;
}

int8u halStack2420Strobe(int8u addr)
{
  int8u status;
  int8u waddr;

  waddr = addr;
  waddr &= ~(BIT(7));
  waddr &= ~(BIT(6));
  

  ATOMIC_LITE(
    CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    status = halCommonSpiReadWrite(waddr);
    SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  )
return status;
}

int8u halStack2420WriteFifo(int8u* buff, int8u addr, int8u numbytes)
{
   int8u status;
   int8u ch, i;
   int8u waddr;
   
   waddr = addr;
   waddr &= ~(BIT(7));
   waddr &= ~(BIT(6));

   ATOMIC_LITE(
     CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
     status = halCommonSpiReadWrite((addr));
     PACKET_TRACE_ASSERT_CS();
      
     for(i=0; i<numbytes; i++) {
       ch = buff[i];
       halCommonSpiWrite(ch);
       // we handle pending ints since this takes about 12us per byte on a 4MHz sysclk
       HANDLE_PENDING_INTERRUPTS();
     }   
     SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
   ) //ATOMIC_LITE

   PACKET_TRACE_DEASSERT_CS();

   return status;

}

int8u halStack2420ReadFifo(int8u *buff, int8u addr, int8u numbytes)
{
  int8u status;
  int8u i;
  int8u waddr;
  
  waddr = addr;
  waddr &= ~(BIT(7));
  waddr |= (BIT(6));

  ATOMIC_LITE(
    CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    status = halCommonSpiReadWrite(waddr);
    for(i=0; i<numbytes; i++) {
      buff[i] = halCommonSpiRead();
      // we handle pending ints since this takes about 12us per byte on a 4MHz sysclk
      HANDLE_PENDING_INTERRUPTS();
    }   
    SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  )
  return status;
} //ATOMIC_LITE

static boolean readFifoInProgressFlag = 0;

int8u halStack2420StartReadFifo(int8u addr)
{
  int8u status;
  int8u waddr;

  readFifoInProgressFlag = 1;
  
  waddr = addr;
  waddr &= ~(BIT(7));
  waddr |= (BIT(6));
  
  CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
  status = halCommonSpiReadWrite(waddr);

  PACKET_TRACE_ASSERT_CS();
    
  return status;
}

void halStack2420EndReadFifo(void) 
{
   PACKET_TRACE_DEASSERT_CS();
   SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);

   readFifoInProgressFlag = 0;
}

boolean halStack2420ReadFifoInProgress(void)
{
  return readFifoInProgressFlag;
}

int8u halStack2420StartWriteRam(int16u addr) 
{
      int8u status;
      int8u addresslsb;
      int16u tempaddr;
      int8u addressmsb;
      
      addresslsb = addr;
      addresslsb |= BIT(7);
      tempaddr = (addr >> 1);
      addressmsb = tempaddr;
      addressmsb &= ~(BIT(5));
      
      CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
      status=halCommonSpiReadWrite(addresslsb);
      halCommonSpiWrite(addressmsb);
      
      return status;
}

int8u halStack2420StartReadRam(int16u addr) 
{
      int8u status;
      int8u addresslsb;
      int16u tempaddr;
      int8u addressmsb;
      
      addresslsb = addr;
      addresslsb |= BIT(7);
      tempaddr = (addr >> 1);
      addressmsb = tempaddr;
      addressmsb |= (BIT(5));
           
      CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
      status=halCommonSpiReadWrite(addresslsb);
      halCommonSpiWrite(addressmsb);
      
      return status;
}

void halStack2420EndRam(void) 
{
   SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
}

#define MAX_BLOCKING_BYTES 4

int8u halStack2420WriteRamSpecifyOrder(int8u* buff, int16u addr, 
                                  int8u numbytes, boolean reverse)
{
   int8u status;
   int8u ii;
   int8u addresslsb;
   int8u addressmsb;

    if (reverse) {
     buff += (numbytes - 1);
   }
  
    while (numbytes > 0) {
      int8u bytesThisTime = ((numbytes > MAX_BLOCKING_BYTES) ?
                             MAX_BLOCKING_BYTES :
                             numbytes);

      addresslsb = addr & 0xFF;
      // Bit 7 set for RAM access
      addresslsb |= BIT(7); 
      // Shifting one bit appears to be a hack to make the
      // Chipcon address defines work: bits 7 and 6 should be
      // 00 for TX FIFO, 01 for RX FIFO, and 10 for security. --Cliff
      addressmsb = (addr >> 1) & 0xFF; 
      // Clearing bit 5 specifies a read and write operation.
      addressmsb &= ~(BIT(5));
      
     ATOMIC_LITE(
       CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
       status=halCommonSpiReadWrite(addresslsb);
       halCommonSpiWrite(addressmsb);
     
       for(ii = 0; ii < bytesThisTime; ii++) {
         int8u ch;

         ch = *buff;
         buff = (reverse ? (buff - 1) : (buff + 1));
         halCommonSpiWrite(ch);
       }
     
       SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    ) // ATOMIC_LITE
    numbytes -= bytesThisTime;
    addr += bytesThisTime;
    }
    return status;
}

int8u halStack2420ReadRamSpecifyOrder(int8u* buff, int16u addr, int8u numbytes,
                                 boolean reverse)
{
  int8u status;
  int8u ii;
  int8u addresslsb;
  int8u addressmsb;
  
  if (reverse) {
    buff += (numbytes - 1);
  }


  while (numbytes > 0) {
    int8u bytesThisTime = ((numbytes > MAX_BLOCKING_BYTES) ?
                           MAX_BLOCKING_BYTES :
                           numbytes);
    
    addresslsb = addr & 0xFF;
    // Bit 7 set for RAM access
    addresslsb |= BIT(7); 
    // Shifting one bit appears to be a hack to make the
    // Chipcon address defines work: bits 7 and 6 should be
    // 00 for TX FIFO, 01 for RX FIFO, and 10 for security. --Cliff
    addressmsb = (addr >> 1) & 0xFF; 
    // Setting bit 5 specifies a read-only operation.
    addressmsb |= (BIT(5));
    ATOMIC(
      CLEARBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
      status=halCommonSpiReadWrite(addresslsb);
      halCommonSpiWrite(addressmsb);
      
      for(ii = 0; ii < bytesThisTime; ii++) {
        int8u ch;
        ch = halCommonSpiRead();
        *buff = ch;
        buff = (reverse ? (buff - 1)  : (buff + 1));
      }
      
      SETBIT(RAD_SPI_PORT,RAD_SPI_PSEL);
    ) // ATOMIC
    numbytes -= bytesThisTime;
    addr += bytesThisTime;
  }
  return status;
}


// functions to toggle the framing and direction for packet trace.
void halStack2420StartPacketTrace( int8u direction )
{
  #ifdef PACKET_TRACE
    // set up directions for frame and DIR pins.
    // note:  should be unnecessary, so we could save a few tics here.
    SETBIT(PACKET_TRACE_FRAME_DDR, PACKET_TRACE_FRAME); // frame
    SETBIT(PACKET_TRACE_DIR_DDR, PACKET_TRACE_DIR); // direction

    // only pull the direction line low if it is a transmission.
    if(direction == PACKET_TRACE_TRANSMIT_MODE)
      CLEARBIT(PACKET_TRACE_DIR_PORT, PACKET_TRACE_DIR);

    // Pull frame line low
    CLEARBIT(PACKET_TRACE_FRAME_PORT, PACKET_TRACE_FRAME);
  #endif
}

void halStack2420StopPacketTrace( void )
{
  #ifdef PACKET_TRACE
    // Pull both lines high
    SETBIT(PACKET_TRACE_DIR_PORT, PACKET_TRACE_DIR);
    SETBIT(PACKET_TRACE_FRAME_PORT, PACKET_TRACE_FRAME);
  #endif
}

void halStack2420InitPacketTrace( void )
{
  #ifdef PACKET_TRACE
    SETBIT(PACKET_TRACE_CS_PORT, PACKET_TRACE_CS);
    SETBIT(PACKET_TRACE_CS_DDR, PACKET_TRACE_CS);

    SETBIT(PACKET_TRACE_FRAME_PORT, PACKET_TRACE_FRAME);
    SETBIT(PACKET_TRACE_FRAME_DDR, PACKET_TRACE_FRAME);

    SETBIT(PACKET_TRACE_DIR_PORT, PACKET_TRACE_DIR);
    SETBIT(PACKET_TRACE_DIR_DDR, PACKET_TRACE_DIR);

    // transmit the micro type
#if 0 //disable for 2.1 release
    halStack2420StartPacketTrace(PACKET_TRACE_TRANSMIT_MODE);
    PACKET_TRACE_ASSERT_CS();
    halCommonSpiWrite(255); // 255 is an invalid length
    halCommonSpiWrite(0); // 0 means AVR
    PACKET_TRACE_DEASSERT_CS();
    halStack2420StopPacketTrace();
#endif
  #endif
}
