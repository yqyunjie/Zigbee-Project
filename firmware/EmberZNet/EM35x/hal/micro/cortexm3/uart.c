/** @file hal/micro/cortexm3/uart.c
 *  @brief EM3XX UART Library.
 *
 * <!-- Author(s): Brooks Barrett -->
 * <!-- Copyright 2009-2010 by Ember Corporation. All rights reserved.  *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "hal/micro/micro-types.h"

#if (! defined(EMBER_STACK_IP))
#include "stack/include/packet-buffer.h"
#endif

#include "app/util/serial/serial.h"
#include "hal/micro/cortexm3/usb.h"

#if defined(EZSP_UART) && \
    !defined(EMBER_SERIAL1_RTSCTS) && \
    !defined(EMBER_SERIAL1_XONXOFF)
  #error EZSP-UART requires either RTS/CTS or XON/XOFF flow control!
#endif

#ifdef EMBER_SERIAL1_RTSCTS
  #if EMBER_SERIAL1_MODE != EMBER_SERIAL_BUFFER
  #error "Illegal serial port 1 configuration"
  #endif
#endif

#ifdef EMBER_SERIAL1_XONXOFF
  #if EMBER_SERIAL1_MODE != EMBER_SERIAL_FIFO
  #error "Illegal serial port 1 configuration"
  #endif

  static void halInternalUart1ForceXon(void); // forward declaration

  static int8s xcmdCount;     // num XONs sent to host, written only by tx isr
                              //-1 means an XOFF was sent last
                              // 0 means ready to rx, but no XON has been sent
                              // n>0 means ready to rx, and n XONs have been sent
  static int8u xonXoffTxByte; // if non-zero, an XON or XOFF byte to send ahead
                              // of tx queue - cleared when byte is sent
  static int8u xonTimer;      // time when last data rx'ed from host, or when
                              // an XON was sent (in 1/4 ticks)

  #define ASCII_XON         0x11  // requests host to pause sending 
  #define ASCII_XOFF        0x13  // requests host to resume sending
  #define XON_REFRESH_TIME  8     // delay between repeat XONs (1/4 sec units)
  #define XON_REFRESH_COUNT 3     // max number of repeat XONs to send after 1st

  // Define thresholds for XON/XOFF flow control in terms of queue used values
  // Take into account the 4 byte transmit FIFO
  #if (EMBER_SERIAL1_RX_QUEUE_SIZE == 128)
    #define XON_LIMIT       16    // send an XON
    #define XOFF_LIMIT      96    // send an XOFF
  #elif (EMBER_SERIAL1_RX_QUEUE_SIZE == 64)
    #define XON_LIMIT       8
    #define XOFF_LIMIT      36
  #elif (EMBER_SERIAL1_RX_QUEUE_SIZE == 32)
    #define XON_LIMIT       2 
    #define XOFF_LIMIT      8
  #elif (EMBER_SERIAL1_RX_QUEUE_SIZE > 32)
    #define XON_LIMIT       (EMBER_SERIAL1_RX_QUEUE_SIZE/8)
    #define XOFF_LIMIT      (EMBER_SERIAL1_RX_QUEUE_SIZE*3/4)
  #else
    #error "Serial port 1 receive buffer too small!"
  #endif
#endif  // EMBER_SERIAL1_XONXOFF

////////////////////// SOFTUART Pin and Speed definitions //////////////////////
//use a logic analyzer and trial and error to determine these values if
//the SysTick time changes or you want to try a different baud
//These were found using EMU 0x50
#define FULL_BIT_TIME_PCLK  0x4E0  //9600 baud with FLKC @ PCLK(12MHz)
#define START_BIT_TIME_PCLK 0x09C  //9600 baud with FLKC @ PCLK(12MHz)
#define FULL_BIT_TIME_SCLK  0x9C0  //9600 baud with FLKC @ SCLK(24MHz)
#define START_BIT_TIME_SCLK 0x138  //9600 baud with FLKC @ SCLK(24MHz)
//USE PB6 (GPIO22) for TXD
#define CONFIG_SOFT_UART_TX_BIT() \
  GPIO_PCCFGH = (GPIO_PCCFGH&(~PC6_CFG_MASK)) | (1 << PC6_CFG_BIT)
#define SOFT_UART_TX_BIT(bit)  GPIO_PCOUT = (GPIO_PCOUT&(~PC6_MASK))|((bit)<<PC6_BIT)
//USE PB7 (GPIO23) for RXD
#define CONFIG_SOFT_UART_RX_BIT() \
  GPIO_PCCFGH = (GPIO_PCCFGH&(~PC7_CFG_MASK)) | (4 << PC7_CFG_BIT)
#define SOFT_UART_RX_BIT  ((GPIO_PCIN&PC7)>>PC7_BIT)
////////////////////// SOFTUART Pin and Speed definitions //////////////////////


#if defined(EMBER_SERIAL1_RTSCTS) && (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
  void halInternalUart1RxCheckRts( void );
#else
  #define halInternalUart1RxCheckRts()
#endif

// Allow some code to be disabled (and flash saved) if
//  a port is unused or in low-level driver mode
#if (EMBER_SERIAL0_MODE == EMBER_SERIAL_UNUSED)
  #define EM_SERIAL0_DISABLED
#endif
#if (EMBER_SERIAL0_MODE == EMBER_SERIAL_LOWLEVEL)
  #error Serial 0 (Virtual Uart) does not support LOWLEVEL mode
#endif
#if (EMBER_SERIAL1_MODE == EMBER_SERIAL_UNUSED) || \
    (EMBER_SERIAL1_MODE == EMBER_SERIAL_LOWLEVEL)
  #define EM_SERIAL1_DISABLED
#endif
#if (EMBER_SERIAL3_MODE == EMBER_SERIAL_UNUSED)
  #define EM_SERIAL3_DISABLED
#endif

#define INVALID -1 // frequency error exceeds +/-0.5%, given sysclock

// Save flash if ports are undefined
#if     !defined(EM_SERIAL1_DISABLED)

  const int16s baudSettings[] = {
      // Negative values indicate 0.5 fractional bit should be set
(int16s)40000L,  // 300     0.00%(40000.00) // N.B. this is -25536 special-case
     20000,      // 600     0.00%(20000.00)
    -13333,      // 900    -0.01%(13333.50 but desire 13333.33; slow by 0.01%)
     10000,      // 1200    0.00%(10000.00)
      5000,      // 2400    0.00% (5000.00)
      2500,      // 4800    0.00% (2500.00)
      1250,      // 9600    0.00% (1250.00)
      -833,      // 14.4k  -0.02% ( 833.50 but desire 833.33; slow by 0.02%)
       625,      // 19.2k   0.00% ( 625.00)
      -416,      // 28.8k  +0.04% ( 416.50 but desire 416.67; fast by 0.04%)
      -312,      // 38.4k   0.00% ( 312.50)
       240,      // 50.0k   0.00% ( 240.00)
      -208,      // 57.6k  -0.08% ( 208.50 but desire 208.33; slow by 0.08%)
       156,      // 76.8k  +0.16% ( 156.00 but desire 156.25; fast by 0.16%)
       120,      // 100.0k  0.00% ( 120.00)
       104,      // 115.2k +0.16% ( 104.00 but desire 104.17; fast by 0.16%)
        52,      // 230.4k +0.16% (  52.00 but desire  52.08; fast by 0.16%)
        26,      // 460.8k +0.16% (  26.00 but desire  26.04; fast by 0.16%)
       #ifdef EMBER_SERIAL_BAUD_CUSTOM
(int16s) EMBER_SERIAL_BAUD_CUSTOM, //Hook for custom baud rate, see BOARD_HEADER
       #else
        13,      // 921.6k +0.16% (  13.00 but desire  13.02; fast by 0.16%)
       #endif
  };

  #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
    //State information for RX DMA Buffer operation
    static const int16u fifoSize = EMBER_SERIAL1_RX_QUEUE_SIZE;
    static const int16u rxStartIndexB = (EMBER_SERIAL1_RX_QUEUE_SIZE/2);
    static int16u prevCountA=0;
    static int16u prevCountB=0;
    static boolean waitingForTailA = FALSE;
    static boolean waitingForTailB = FALSE;
    static boolean waitingForInputToB = FALSE;
  #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)

#endif//!defined(EM_SERIAL1_DISABLED)

// Forward prototype for TX
void halInternalUart1TxIsr(void);

//Prototype for Error Marking
static void uartErrorMark(int8u port, int8u errors);

#if (!defined(EM_SERIAL0_DISABLED) ||\
     !defined(EM_SERIAL1_DISABLED) ||\
     !defined(EM_SERIAL3_DISABLED))
EmberStatus halInternalUartInit(int8u port,
                                SerialBaudRate rate,
                                SerialParity parity,
                                int8u stopBits)
{
  #if !defined(EM_SERIAL0_DISABLED)
    if (port == 0) {
      // Nothing special to do since the debug channel handles this
      return EMBER_SUCCESS;
    }
  #endif//!defined(EM_SERIAL0_DISABLED)

  #if !defined(EM_SERIAL1_DISABLED)
    #ifdef SOFTUART
      //make sure the TX bit starts at idle high
      SOFT_UART_TX_BIT(1);
      CONFIG_SOFT_UART_TX_BIT();
      CONFIG_SOFT_UART_RX_BIT();
    #else //SOFTUART
      if (port == 1) { 
      int32u tempcfg;

      if ( (rate >= sizeof(baudSettings)/sizeof(*baudSettings)) ||
          (baudSettings[rate] == INVALID) ) {
        return EMBER_SERIAL_INVALID_BAUD_RATE;
      }

      if (baudSettings[rate] < 0 && // Negative says set .5 fractional bit
    #ifdef  EMBER_SERIAL_BAUD_CUSTOM
    #if     (EMBER_SERIAL_BAUD_CUSTOM > 32767L)
          rate != BAUD_CUSTOM &&
    #endif//(EMBER_SERIAL_BAUD_CUSTOM > 32767L)
    #endif//EMBER_SERIAL_BAUD_CUSTOM
          rate != BAUD_300) { // except BAUD_300 special-case to save flash
        SC1_UARTPER  = -baudSettings[rate];
        SC1_UARTFRAC = 1;
      } else {
        SC1_UARTPER  = baudSettings[rate];
        SC1_UARTFRAC = 0;
      }

      // Default is always 8 data bits irrespective of parity setting,
      // according to Lee, but hack overloads high-order nibble of stopBits to
      // allow user to specify desired number of data bits:  7 or 8 (default).
      if (((stopBits & 0xF0) >> 4) == 7) {
        tempcfg = 0;
      } else {
        tempcfg = SC_UART8BIT;
      }
      
      if (parity == PARITY_ODD) {
        tempcfg |= SC_UARTPAR | SC_UARTODD;
      } else if( parity == PARITY_EVEN ) {
        tempcfg |= SC_UARTPAR;
      }

      if ((stopBits & 0x0F) >= 2) {
        tempcfg |= SC_UART2STP;
      }
      SC1_UARTCFG = tempcfg;

      SC1_MODE = SC1_MODE_UART;

      // Int configuration here must also be done in halInternalPowerUpUart()
      #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
        // Make the RX Valid interrupt level sensitive (instead of edge)
        // SC1_INTMODE = SC_SPIRXVALMODE;
        // Enable just RX interrupts; TX interrupts are controlled separately
        INT_SC1CFG |= (INT_SCRXVAL   |
                       INT_SCRXOVF   |
                       INT_SC1FRMERR |
                       INT_SC1PARERR);
        INT_SC1FLAG = 0xFFFF; // Clear any stale interrupts
        INT_CFGSET = INT_SC1;
        #ifdef EMBER_SERIAL1_XONXOFF
          halInternalUart1ForceXon();
        #endif
      #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
        {
          //setup the addresses for receive DMA
          EmSerialFifoQueue *q = emSerialRxQueues[1];
          int32u startAddress  = (int32u)q->fifo;
          SC1_RXBEGA =  startAddress;
          SC1_RXENDA = (startAddress + fifoSize/2 - 1);
          SC1_RXBEGB = (startAddress + fifoSize/2);
          SC1_RXENDB = (startAddress + fifoSize - 1);
          
          //activate DMA
          SC1_DMACTRL = (SC_RXLODA | SC_RXLODB);
        }
        #ifndef EZSP_UART
          INT_SC1CFG |= (INT_SCRXOVF   |
                         INT_SC1FRMERR |
                         INT_SC1PARERR);
        #endif
        // The receive side of buffer mode does not require any interrupts.
        // The transmit side of buffer mode requires interrupts, which
        // will be configured on demand in halInternalStartUartTx(), so just
        // enable the top level SC1 interrupt for the transmit side.
        INT_SC1FLAG = 0xFFFF; // Clear any stale interrupts
        INT_CFGSET = INT_SC1; // Enable top-level interrupt

        #ifdef EMBER_SERIAL1_RTSCTS
          // Software-based RTS/CTS needs interrupts on DMA buffer unloading.
          INT_SC1CFG |= (INT_SCRXULDA | INT_SCRXULDB);
          SC1_UARTCFG |= (SC_UARTFLOW | SC_UARTRTS);
        #endif
      #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)

      return EMBER_SUCCESS;
      }
    #endif //SOFTUART
  #endif//!defined(EM_SERIAL1_DISABLED)

  #if !defined(EM_SERIAL3_DISABLED)
    #if defined (CORTEXM3_EM35X_USB)
      if ( port == 3 ) {
        halResetWatchdog();
        
        usbConfigEnumerate();
        
        //It is necessary to wait for the COM port on the host to become
        //active before serial port3 can be used.
        int16u startTime = halCommonGetInt16uMillisecondTick();
        while(!comPortActive()) {
          //Give ourselves a healthy 1 second for a COM port to open.
          if(elapsedTimeInt16u(startTime,
                               halCommonGetInt16uMillisecondTick()) > 1000) {
            return EMBER_SERIAL_INVALID_PORT;
          }
        }
        
        return EMBER_SUCCESS;
      }
    #endif
  #endif//!defined(EM_SERIAL3_DISABLED)
  
  return EMBER_SERIAL_INVALID_PORT;
}
#endif//(!defined(EM_SERIAL0_DISABLED) || !defined(EM_SERIAL1_DISABLED))

#ifdef SOFTUART
//this requires use of the SysTick counter and will destory interrupt latency!
static void softwareUartTxByte(int8u byte)
{
  int8u i;
  int16u fullBitTime;
  
  if (CPU_CLKSEL) {
    fullBitTime = FULL_BIT_TIME_SCLK;
  } else {
    fullBitTime = FULL_BIT_TIME_PCLK;
  }

#ifdef EMBER_EMU_TEST
    // If we're on the emulator the clock runs at half speed
    fullBitTime /= 2;
#endif

  ATOMIC(
    ST_RVR = fullBitTime; //set the SysTick reload value register
    //enable core clock reference and the counter itself
    ST_CSR = (ST_CSR_CLKSOURCE | ST_CSR_ENABLE);
    while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 1bit time
    
    //go low for start bit
    SOFT_UART_TX_BIT(0); //go low for start bit
    while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 1bit time
    
    //loop over all 8 data bits transmitting each
    for (i=0;i<8;i++) {
      SOFT_UART_TX_BIT(byte&0x1); //data bit
      while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 1bit time
      byte = (byte>>1);
    }

    SOFT_UART_TX_BIT(1); //stop bit
    while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 1bit time

    //disable SysTick
    ST_CSR = 0;
  )
}
#endif //SOFTUART

void halInternalStartUartTx(int8u port)
{
  #if     !defined(EM_SERIAL0_DISABLED)
    if ( port == 0 ) {
      #if EMBER_SERIAL0_MODE == EMBER_SERIAL_FIFO
        EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[0];
        assert(q->tail == 0);
        emDebugSendVuartMessage(q->fifo, q->used);
        q->used = 0;
        q->head = 0;
        return;
      #endif
      #if EMBER_SERIAL0_MODE == EMBER_SERIAL_BUFFER
        EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[0];
        assert(q->nextByte == NULL);
        emSerialBufferNextMessageIsr(q);
        while (q->nextByte != NULL) {
          emDebugSendVuartMessage(q->nextByte, (q->lastByte-q->nextByte)+1);
          emSerialBufferNextBlockIsr(q,0);
        }
        return;
      #endif
    }
  #endif//!defined(EM_SERIAL0_DISABLED)

  #if     !defined(EM_SERIAL1_DISABLED)
    #ifdef SOFTUART
      if (port == 1) {
        EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[1];
        // Always configure the bit because other operations might have
        // tried to compromise it
        SOFT_UART_TX_BIT(1);
        CONFIG_SOFT_UART_TX_BIT();
        while (q->used > 0) {
          int8u byte = FIFO_DEQUEUE(q, emSerialTxQueueWraps[1]);
          softwareUartTxByte(byte);
        }
        return;
      }
    #else //SOFTUART
      // If the port is configured, go ahead and start transmit
      if ( (port == 1) && (SC1_MODE == SC1_MODE_UART) ) {
        #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
          // Ensure UART TX interrupts are enabled,
          // and call the ISR to send any pending output
          ATOMIC(
            // Enable TX interrupts
            INT_SC1CFG |= (INT_SCTXFREE | INT_SCTXIDLE);
            // Pretend we got a tx interrupt
            halInternalUart1TxIsr();
          )
        #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
          // Ensure UART TX interrupts are enabled,
          // and call the ISR to send any pending output
          ATOMIC(
            INT_SC1CFG |= (INT_SCTXULDA | INT_SCTXULDB | INT_SCTXIDLE);
            // Pretend we got a tx interrupt
            halInternalUart1TxIsr();
          )
        #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
        return;
      }
    #endif //SOFTUART
  #endif//!defined(EM_SERIAL1_DISABLED)

  #if !defined(EM_SERIAL3_DISABLED)
    #if defined (CORTEXM3_EM35X_USB)
      if ( port == 3 ) {
        #if EMBER_SERIAL3_MODE == EMBER_SERIAL_FIFO

          //Call into the usb.c driver which will operate on serial
          //port 3's Q to transmit data.
          usbTxData();

          return;
        #endif
      }
    #endif
  #endif//!defined(EM_SERIAL3_DISABLED)

}

void halInternalStopUartTx(int8u port)
{
  // Nothing for port 0 (virtual uart)

  #if !defined(EM_SERIAL1_DISABLED)
    if (port == 1) {
      #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
        // Disable TX Interrupts
        INT_SC1CFG &= ~(INT_SCTXFREE | INT_SCTXIDLE);
      #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
        // Ensure DMA operations are complete before shutting off interrupts,
        // otherwise we might miss an important interrupt and cause a
        // packet buffer leak, e.g.
        while (SC1_DMACTRL & (SC_TXLODA | SC_TXLODB)) {}
        while ( !(SC1_UARTSTAT & SC_UARTTXIDLE) ) {}
        // Disable TX Interrupts
        INT_SC1CFG &= ~(INT_SCTXULDA | INT_SCTXULDB | INT_SCTXIDLE);
      #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
    }
  #endif//!defined(EM_SERIAL1_DISABLED)
}


//full blocking, no queue overflow issues, can be used in or out of int context
//does not return until character is transmitted.
EmberStatus halInternalForceWriteUartData(int8u port, int8u *data, int8u length)
{
  #if !defined(EM_SERIAL0_DISABLED)
    if (port == 0) {
      emDebugSendVuartMessage(data, length);
      return EMBER_SUCCESS;
    }
  #endif//!defined(EM_SERIAL0_DISABLED)

  #if !defined(EM_SERIAL1_DISABLED)
    #ifdef SOFTUART
      if (port == 1) {
        //always configure the bit because other operations might have
        //tried to compromise it
        SOFT_UART_TX_BIT(1);
        CONFIG_SOFT_UART_TX_BIT();
        while (length--) {
          SC1_DATA = *data;
          softwareUartTxByte(*data);
          data++;
        }
        return EMBER_SUCCESS;
      }
    #else //SOFTUART
      //if the port is configured, go ahead and transmit
      if ( (port == 1) && (SC1_MODE == SC1_MODE_UART) ) {
        while (length--) {
          //spin until data register has room for more data
          while ((SC1_UARTSTAT&SC_UARTTXFREE)!=SC_UARTTXFREE) {}
          SC1_DATA = *data;
          data++;
        }
  
        //spin until TX complete (TX is idle)
        while ((SC1_UARTSTAT&SC_UARTTXIDLE)!=SC_UARTTXIDLE) {}
  
        return EMBER_SUCCESS;
      }
    #endif //SOFTUART
  #endif//!defined(EM_SERIAL1_DISABLED)
  
  #if !defined(EM_SERIAL3_DISABLED)
    #if defined (CORTEXM3_EM35X_USB)
      if(port == 3) {
        //This function will block until done sending all the data.
        usbForceTxData(data, length);
        return EMBER_SUCCESS;
      }
    #endif
  #endif//!defined(EM_SERIAL3_DISABLED)

  return EMBER_SERIAL_INVALID_PORT;
}

// Useful for waiting on serial port characters before interrupts have been
// turned on.
EmberStatus halInternalForceReadUartByte(int8u port, int8u* dataByte)
{
  EmberStatus err = EMBER_SUCCESS;

  #if !defined(EM_SERIAL0_DISABLED)
    if ( port == 0 ) {
      EmSerialFifoQueue *q = emSerialRxQueues[0];
      ATOMIC(
        if (q->used == 0) {
          WAKE_CORE = WAKE_CORE_FIELD;
        }
        if (q->used > 0) {
          *dataByte = FIFO_DEQUEUE(q, emSerialRxQueueWraps[0]);
        } else {
          err = EMBER_SERIAL_RX_EMPTY;
        }
      )
    }
  #endif//!defined(EM_SERIAL0_DISABLED)

  #if !defined(EM_SERIAL1_DISABLED)
    if ( port == 1 ) {
      #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
        if ( SC1_UARTSTAT & SC_UARTRXVAL ) {
          *dataByte = (int8u) SC1_DATA;
        } else {
          err = EMBER_SERIAL_RX_EMPTY;
        }
      #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
        //When in buffer mode, the DMA channel is active and the RXVALID bit (as
        //used above in FIFO mode) will never get set.  To maintain the DMA/Buffer
        //model of operation, we need to break the conceptual model in this function
        //and make a function call upwards away from the hardware.  The ReadByte
        //function calls back down into halInternalUartRxPump and forces the
        //sequencing of the serial queues and the DMA buffer, resulting in a forced
        //read byte being returned if it is there.
        if (emberSerialReadByte(1, dataByte)!=EMBER_SUCCESS) {
          err = EMBER_SERIAL_RX_EMPTY;
        }
      #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
    }
  #endif//!defined(EM_SERIAL1_DISABLED)

  return err;
}

// blocks until the text actually goes out
void halInternalWaitUartTxComplete(int8u port)
{
  halResetWatchdog();

  // Nothing to do for port 0 (virtual uart)

  #if !defined(EM_SERIAL1_DISABLED) 
    if ( port == 1 ) {
      while ( !(SC1_UARTSTAT & SC_UARTTXIDLE) ) {}
      return;
    }
  #endif//!defined(EM_SERIAL1_DISABLED) 
}


// Debug Channel calls this ISR to push up data it has received
void halStackReceiveVuartMessage(int8u *data, int8u length)
{
  #if !defined(EM_SERIAL0_DISABLED)
    EmSerialFifoQueue *q = emSerialRxQueues[0];

    while (length--) {
      //Use (emSerialRxQueueSizes - 1) so that the FIFO never completely fills
      //and the head never wraps around to the tail
      if ((q->used < (emSerialRxQueueSizes[0] - 1))) {
        FIFO_ENQUEUE(q,*data++,emSerialRxQueueWraps[0]);
      } else {    
        uartErrorMark(0, EMBER_SERIAL_RX_OVERFLOW);
        return;  // no sense in trying to enqueue the rest
      }
    }
  #else  //!defined(EM_SERIAL0_DISABLED)
    return;  // serial 0 not used, drop any input
  #endif //!defined(EM_SERIAL0_DISABLED)
}


#if (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
void halInternalRestartUart1Dma(void)
{
  //Reset the DMA software and restart it.
  EmSerialFifoQueue *q = emSerialRxQueues[1];
  int32u startAddress = (int32u)q->fifo;
  int8u head;
  int8u tail;
  int8u loadA = 0;
  int8u loadB = 0;
  prevCountA = 0;
  prevCountB = 0;
  waitingForTailA = FALSE;
  waitingForTailB = FALSE;
  waitingForInputToB = FALSE;
  //reload all defaults addresses - they will be adjusted below if needed
  SC1_DMACTRL = SC_RXDMARST;
  SC1_RXBEGA =  startAddress;
  SC1_RXENDA = (startAddress + fifoSize/2 - 1);
  SC1_RXBEGB =  (startAddress + fifoSize/2);
  SC1_RXENDB = (startAddress + fifoSize - 1);
  
  //adjust buffer addresses as needed and reload available buffers
  if ( q->used != fifoSize ) {
    //we can only reload if the FIFO isn't full!
    //the FIFO is not empty or full, figure out what to do:
    //at this point we know we always have to adjust ST_ADDR to the head
    //we need to know which buffer the head is in, and always load that buff
    if ((q->head)<rxStartIndexB) {
      SC1_RXBEGA = startAddress + (q->head);
      loadA++;
    } else {
      SC1_RXBEGB = startAddress + (q->head);
      loadB++;
    }
    //check to see if the head and the tail are not in the same buffer
    if((q->tail)/(rxStartIndexB)) {
      tail = TRUE;  //Tail in B buffer
    } else {
      tail = FALSE; //Tail in A buffer
    }
    
    if((q->head)/(rxStartIndexB)) {
      head = TRUE;  //Head in B buffer
    } else {
      head = FALSE; //Head in A buffer
    }

    if ( tail != head ) {
      //the head and the tail are in different buffers
      //we need to flag the buffer the tail is in so the Pump function does
      //not try to reenable it until it has been drained like normal.
      if ((q->tail)<rxStartIndexB) {
        waitingForTailA = TRUE;
      } else {
        waitingForTailB = TRUE;
      }
    } else {
      //the head and the tail are in the same buffers
      if (q->used <= rxStartIndexB) {
        //The serial FIFO is less no more than half full!
        if (!loadB) {
          //the head is in B, and we're capable of loading A
          //BUT: we can't activate A because the DMA defaults to A first,
          //  and it needs to start using B first to fill from the head
          //  SO, only load A if B hasn't been marked yet for loading.
          loadA++;
        } else {
          //B is loaded and waiting for data, A is being supressed until
          //B receives at least one byte so A doesn't prematurely load and
          //steal bytes meant for B first.
          waitingForTailA = TRUE;
          waitingForInputToB = TRUE;
        }
        //We can always loadB at this point thanks to our waiting* flags.
        loadB++;
      } else {
        //The serial FIFO is more than half full!
        //Since this case requires moving an end address of a buffer, which
        //severely breaks DMA'ing into a FIFO, we cannot do anything.
        //Doing nothing is ok because we are more than half full anyways,
        //and under normal operation we would only load a buffer when our
        //used count is less than half full.
        //Configure so the Pump function takes over when the serial FIFO drains
        SC1_RXBEGA =  startAddress;
        SC1_RXBEGB =  (startAddress + fifoSize/2);
        loadA = 0;
        loadB = 0;
        waitingForTailA = TRUE;
        waitingForTailB = TRUE;
      }
    }
    
    //Address are set, flags are set, DMA is ready, so now we load buffers
    if (loadA) {
      SC1_DMACTRL = SC_RXLODA;
    }
    if (loadB) {
      SC1_DMACTRL = SC_RXLODB;
    }
  } else {
    //we're full!!  doh!  have to wait for the FIFO to drain
    waitingForTailA = TRUE;
    waitingForTailB = TRUE;
  }
}
#endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)


#if !defined(EM_SERIAL1_DISABLED)
void halInternalUart1RxIsr(int16u causes)
{
  #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
    EmSerialFifoQueue *q = emSerialRxQueues[1];

    // At present we really don't care which interrupt(s)
    // occurred, just that one did.  Loop reading RXVALID
    // data (loop is necessary for bursty data otherwise
    // we could leave with RXVALID and not get another
    // RXVALID interrupt), processing any errors noted
    // along the way.
    while ( SC1_UARTSTAT & SC_UARTRXVAL ) {
      int8u errors = SC1_UARTSTAT & (SC_UARTFRMERR |
                                     SC_UARTRXOVF  |
                                     SC_UARTPARERR );
      int8u incoming = (int8u) SC1_DATA;

      if ( (errors == 0) && (q->used < (EMBER_SERIAL1_RX_QUEUE_SIZE-1)) ) {
#ifdef EMBER_SERIAL1_XONXOFF
        // Discard any XON or XOFF bytes received
        if ( (incoming != ASCII_XON) && (incoming != ASCII_XOFF) ) {
          FIFO_ENQUEUE(q, incoming, emSerialRxQueueWraps[1]);
        }
#else
        FIFO_ENQUEUE(q, incoming, emSerialRxQueueWraps[1]);
#endif
      } else {
        // Translate error code
        if ( errors == 0 ) {
          errors = EMBER_SERIAL_RX_OVERFLOW;
          HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_OVERFLOW_ERROR);
        } else if ( errors & SC_UARTRXOVF ) {
          errors = EMBER_SERIAL_RX_OVERRUN_ERROR;
          HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_OVERRUN_ERROR);
        } else if ( errors & SC_UARTFRMERR ) {
          errors = EMBER_SERIAL_RX_FRAME_ERROR;
          HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_FRAMING_ERROR);
        } else if ( errors & SC_UARTPARERR ) {
          errors = EMBER_SERIAL_RX_PARITY_ERROR;
        } else { // unknown
          errors = EMBER_ERR_FATAL;
        }
        uartErrorMark(1, errors);
      }
#ifdef EMBER_SERIAL1_XONXOFF
      if ((q->used >= XOFF_LIMIT) && (xcmdCount >= 0))  {
        xonXoffTxByte = ASCII_XOFF;
        halInternalStartUartTx(1);
      }
#endif
    } // end of while ( SC1_UARTSTAT & SC1_UARTRXVAL )

  #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
    #ifdef EMBER_SERIAL1_RTSCTS
      // If RTS is controlled by sw, this ISR is called when a buffer unloads.
      if (causes & (INT_SCRXULDA | INT_SCRXULDB)) {
        // Deassert RTS if the rx queue tail is not in an active DMA buffer:
        // if it is, then there's at least one empty DMA buffer 
        if ( !( (emSerialRxQueues[1]->tail < EMBER_SERIAL1_RX_QUEUE_SIZE/2) &&
               (SC1_DMASTAT & SC_RXACTA) ) &&
             !( (emSerialRxQueues[1]->tail >= EMBER_SERIAL1_RX_QUEUE_SIZE/2) 
                && (SC1_DMASTAT & SC_RXACTB) ) ) {
          SC1_UARTCFG &= ~SC_UARTRTS;        // deassert RTS
        }
      #ifdef EZSP_UART
        if ( ( (causes & INT_SCRXULDA) && (SC1_DMASTAT & SC_RXOVFA) ) ||
             ( (causes & INT_SCRXULDB) && (SC1_DMASTAT & SC_RXOVFB) ) ) {
          HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_OVERFLOW_ERROR);
        }
        if ( ( (causes & INT_SCRXULDA) && (SC1_DMASTAT & SC_RXFRMA) ) ||
             ( (causes & INT_SCRXULDB) && (SC1_DMASTAT & SC_RXFRMB) ) ) {
          HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_FRAMING_ERROR);
        }
      #else//!EZSP_UART
        causes &= ~(INT_SCRXULDA | INT_SCRXULDB);
        if (causes == 0) { // if no errors in addition, all done
          return;
        }
      #endif//EZSP_UART
      }
    #endif  //#ifdef EMBER_SERIAL1_RTSCTS
    #ifndef EZSP_UART
    //Load all of the hardware status, then immediately reset so we can process
    //what happened without worrying about new data changing these values.
    //We're in an error condition anyways, so it is ok to have the DMA disabled
    //for a while (less than 80us, while 4 bytes @ 115.2kbps is 350us)
    {
      EmSerialFifoQueue *q = emSerialRxQueues[1];
      int16u status  = SC1_DMASTAT;
      int16u errCntA = SC1_RXERRA;
      int16u errCntB = SC1_RXERRB;
      int32u errorIdx = EMBER_SERIAL1_RX_QUEUE_SIZE*2;
      int32u tempIdx;
      int32u startAddress = (int32u)q->fifo;

      //interrupts acknowledged at the start of the master SC1 ISR
      int16u intSrc  = causes;
      int8u errorType = EMBER_SUCCESS;

      SC1_DMACTRL = SC_RXDMARST;  //to clear error
      //state fully captured, DMA reset, now we process error and restart
    
      if ( intSrc & INT_SCRXOVF ) {
        //Read the data register four times to clear
        //the RXOVERRUN condition and empty the FIFO, giving us 4 bytes
        //worth of time (from this point) to reenable the DMA.
        (void) SC1_DATA;
        (void) SC1_DATA;
        (void) SC1_DATA;
        (void) SC1_DATA;

        if ( status & ( SC_RXFRMA
                     | SC_RXFRMB
                     | SC_RXPARA
                     | SC_RXPARB ) ) {
          //We just emptied hardware FIFO so the overrun condition is cleared.
          //Byte errors require special handling to roll back the serial FIFO.
          goto dealWithByteError;
        }
      
      //record the error type
      emSerialRxError[1] = EMBER_SERIAL_RX_OVERRUN_ERROR;
      
      //check for a retriggering of the Rx overflow, don't advance FIFO if so
      if ( !(waitingForTailA && waitingForTailB) ) {
        //first, move head to end of buffer head is in
        //second, move head to end of other buffer if tail is not in other buffer
        if ((q->head)<rxStartIndexB) {
          //head inside A
          q->used += (rxStartIndexB - q->head);
          q->head = (rxStartIndexB);
          if ((q->tail)<rxStartIndexB) {
            //tail not inside of B
            q->used += rxStartIndexB;
            q->head = 0;
          }
        } else {
          //head inside B
          q->used += (fifoSize - q->head);
          q->head = 0;
          if ((q->tail)>=rxStartIndexB) {
            //tail is not inside of A
            q->used += rxStartIndexB;
            q->head = rxStartIndexB;
          }
        }
      }
      
      //Record the error position in the serial FIFO
      if (q->used != fifoSize) {
        //mark the byte at q->head as the error
        emSerialRxErrorIndex[1] = q->head;
      } else {
        //Since the FIFO is full, the error index needs special handling
        //so there is no conflict between the head and tail looking at the same
        //index which needs to be marked as an error.
        emSerialRxErrorIndex[1] = RX_FIFO_FULL;
      }

      //By now the error is accounted for and the DMA hardware is reset.
      //By definition, the overrun error means we have no room left, therefore
      //we can't reenable the DMA.  Reset the previous counter states, and set
      //the waitingForTail flags to TRUE - this tells the Pump function we have
      //data to process.  The Pump function will reenable the buffers as they
      //become available, just like normal.
      prevCountA = 0;
      prevCountB = 0;
      waitingForInputToB = FALSE;
      waitingForTailA = TRUE;
      waitingForTailB = TRUE;
      //from this point we fall through to the end of the Isr and return.

      } else {
      dealWithByteError:
        //We have a byte error to deal with and possibly more than one byte error,
        //of different types in different DMA buffers, so check each error flag.
        //All four error checks translate the DMA buffer's error position to their
        //position in the serial FIFO, and compares the error locations to find
        //the first error to occur after the head of the FIFO.  This error is the
        //error condition that is stored and operated on.
        if ( status & SC_RXFRMA ) {
          tempIdx = errCntA;
          if (tempIdx < q->head) {
            tempIdx += fifoSize;
          }
          if (tempIdx<errorIdx) {
            errorIdx = tempIdx;
          }
          errorType = EMBER_SERIAL_RX_FRAME_ERROR;
        }
        if ( status & SC_RXFRMB ) {
          tempIdx = (errCntB + SC1_RXBEGB) - startAddress;
          if (tempIdx < q->head) {
            tempIdx += fifoSize;
          }
          if (tempIdx<errorIdx) {
            errorIdx = tempIdx;
          }
          errorType = EMBER_SERIAL_RX_FRAME_ERROR;
        }
        if ( status & SC_RXPARA ) {
          tempIdx = errCntA;
          if (tempIdx < q->head) {
            tempIdx += fifoSize;
          }
          if (tempIdx<errorIdx) {
            errorIdx = tempIdx;
          }
          errorType = EMBER_SERIAL_RX_PARITY_ERROR;
        }
        if ( status & SC_RXPARB ) {
          tempIdx = (errCntB + SC1_RXBEGB) - startAddress;
          if (tempIdx < q->head) {
            tempIdx += fifoSize;
          }
          if (tempIdx<errorIdx) {
            errorIdx = tempIdx;
          }
          errorType = EMBER_SERIAL_RX_PARITY_ERROR;
        }
        
        //We now know the type and location of the first error.
        //Move up to the error location and increase the used count.
        q->head = errorIdx;
        if (q->head < q->tail) {
          q->used = ((q->head + fifoSize) - q->tail);
        } else {
          q->used = (q->head - q->tail);
        }
        
        //Mark the byte at q->head as the error
        emSerialRxError[1] = errorType;
        if (q->used != fifoSize) {
          //mark the byte at q->head as the error
          emSerialRxErrorIndex[1] = q->head;
        } else {
          //Since the FIFO is full, the error index needs special handling
          //so there is no conflict between the head and tail looking at the same
          //index which needs to be marked as an error.
          emSerialRxErrorIndex[1] = RX_FIFO_FULL;
        }
        
        //By now the error is accounted for and the DMA hardware is reset.
        halInternalRestartUart1Dma();
      }
    }
    #endif // #ifndef EZSP_UART
  #endif //(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
}
#endif//!defined(EM_SERIAL1_DISABLED)


#if !defined(EM_SERIAL3_DISABLED)
    #if defined (CORTEXM3_EM35X_USB)
    void halInternalUart3RxIsr(int8u *rxData, int8u length)
    {
      EmSerialFifoQueue *q = emSerialRxQueues[3];
      
      while(length--) {
        if(q->used < (EMBER_SERIAL3_RX_QUEUE_SIZE-1)) {
          FIFO_ENQUEUE(q, *rxData, emSerialRxQueueWraps[3]);
          rxData++;
        } else {
          uartErrorMark(3, EMBER_SERIAL_RX_OVERFLOW);
          return;
        }
      }
    }
  #endif
#endif


#ifdef SOFTUART
//this requires use of the SysTick counter and will destory interrupt latency!
static int8u softwareUartRxByte(void)
{
  int8u i;
  int8u bit;
  int8u byte = 0;
  int16u startBitTime, fullBitTime;
  
  if (CPU_CLKSEL) {
    startBitTime = START_BIT_TIME_SCLK;
    fullBitTime = FULL_BIT_TIME_SCLK;
  } else {
    startBitTime = START_BIT_TIME_PCLK;
    fullBitTime = FULL_BIT_TIME_PCLK;
  }

#ifdef EMBER_EMU_TEST
    // If we're on the emulator the clock runs at half speed
    startBitTime /= 2;
    fullBitTime /= 2;
#endif

  ATOMIC(
    INTERRUPTS_ON();
    //we can only begin receiveing if the input is idle high
    while (SOFT_UART_RX_BIT != 1) {}
    //now wait for our start bit
    while (SOFT_UART_RX_BIT != 0) {}
    INTERRUPTS_OFF();
    
    //set reload value such that move to the center of an incoming bit
    ST_RVR = startBitTime;
    ST_CVR = 0; //writing the current value will cause it to reset to zero
    //enable core clock reference and the counter itself
    ST_CSR = (ST_CSR_CLKSOURCE | ST_CSR_ENABLE);
    while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 0.5bit time
    //set reload value such that move 1bit time
    ST_RVR = fullBitTime;
    ST_CVR = 0; //writing the current value will cause it to reset to zero
    
    //loop 8 times recieving all 8 bits and building up the byte
    for (i=0;i<8;i++) {
      while ((ST_CSR&ST_CSR_COUNTFLAG) != ST_CSR_COUNTFLAG) {} //wait 1bit time
      bit = SOFT_UART_RX_BIT; //get the data bit
      bit = ((bit&0x1)<<7);
      byte = (byte>>1)|(bit);
    }

    //disable SysTick
    ST_CSR = 0;
  )
  return byte;
}
#endif //SOFTUART

void halInternalUartRxPump(int8u port)
{
  #ifdef SOFTUART
    if (port == 1) {
      EmSerialFifoQueue *q = emSerialRxQueues[1];
      int8u errors;
      int8u byte;

      //always configure the bit because other operations might have
      //tried to compromise it
      CONFIG_SOFT_UART_RX_BIT();

      //this will block waiting for a start bit!
      byte = softwareUartRxByte();
      
      if (q->used < (EMBER_SERIAL1_RX_QUEUE_SIZE-1)) {
          FIFO_ENQUEUE(q, byte, emSerialRxQueueWraps[1]);
      } else {
        errors = EMBER_SERIAL_RX_OVERFLOW;
        uartErrorMark(1, errors);
      }
    }
  #else //SOFTUART
    #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
      if ( port == 1 ) {
        EmSerialFifoQueue *q = emSerialRxQueues[1];
        int8u tail,head;
        int16u count=0;
        int8u loadA;
        int8u loadB;
        //Load all of the hardware status, so we can process what happened
        //without worrying about new data changing these values.
        int8u dmaStatus = SC1_DMACTRL;
        int16u currCountA = SC1_RXCNTA;
        int16u currCountB = SC1_RXCNTB;
    
        //Normal check to see if A has any data
        if (prevCountA != currCountA) {
          //Update the counters and head location for the new data
          count = (currCountA - prevCountA);
          q->used += count;
          q->head = (q->head + count) % emSerialRxQueueSizes[1];
          prevCountA = currCountA;
          waitingForTailA = TRUE;
        }
        //Normal check to see if B has any data at all
        if (prevCountB != currCountB) {
          //Update the counters and head location for the new data
          count = (currCountB - prevCountB);
          q->used += count;
          q->head = (q->head + count) % emSerialRxQueueSizes[1];
          prevCountB = currCountB;
          waitingForTailB = TRUE;
          waitingForInputToB = FALSE;
        }
    
    
        //if the used count is greater than half the buffer size, nothing can be done
        if (q->used > rxStartIndexB) {
          return;
        }
        //if nothing is in the FIFO, we can reload both if needed
        if (q->used == 0) {
          loadA = TRUE;
          loadB = TRUE;
          goto reloadBuffers;
        }
        //0 < used < bufferSize, so figure out where tail and head are
        if((q->tail)/(rxStartIndexB)) { 
          tail = TRUE;  //Tail in B buffer
        } else {
          tail = FALSE; //Tail in A buffer
        }
        
        if(((int16u)(q->head - 1))/(rxStartIndexB)) {
          head = TRUE;  //Head in B buffer
        } else {
          head = FALSE; //Head in A buffer
        }
        
        //To load, the tail must be in the same buffer as the head so we don't
        //overwrite any bytes that haven't drained from the serial FIFO yet.
        if (tail!=head) {
          halInternalUart1RxCheckRts();
          return;
        }
        // Recall tail TRUE means data is inside B
        loadA = tail;
        loadB = !tail;
    reloadBuffers:
        //check if the buffers need to be reloaded
        if ( (loadA) && (!waitingForInputToB) ) {
          if ( (dmaStatus&SC_RXLODA)
              != SC_RXLODA) {
            //An error interrupt can move the addresses of the buffer
            //during the flush/reset/reload operation.  At this point the
            //buffer is clear of any usage, so we can reset the addresses
            SC1_RXBEGA = (int32u)q->fifo;
            SC1_RXENDA = (int32u)(q->fifo + fifoSize/2 - 1);
            prevCountA = 0;
            waitingForTailA = FALSE;
            SC1_DMACTRL = SC_RXLODA;
          }
        }
        if (loadB) {
          if ( (dmaStatus&SC_RXLODB)
              != SC_RXLODB) {
            //An error interrupt can move the addresses of the buffer
            //during the flush/reset/reload operation.  At this point the
            //buffer is clear of any usage, so we can reset the addresses
            SC1_RXBEGB = (int32u)(q->fifo + fifoSize/2);
            SC1_RXENDB = (int32u)(q->fifo + fifoSize - 1);
            prevCountB = 0;
            waitingForTailB = FALSE;
            SC1_DMACTRL = SC_RXLODB;
          }
        }
          halInternalUart1RxCheckRts();
      }
    #endif //(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
  #endif //SOFTUART
}

#if defined(EMBER_SERIAL1_RTSCTS) && (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
void halInternalUart1RxCheckRts(void)
{
  // Verify RTS is controlled by SW (not AUTO mode), and isn't already asserted.
  // (The logic to deassert RTS is in halInternalUart1RxIsr().)
  if ((SC1_UARTCFG & (SC_UARTFLOW | SC_UARTAUTO | SC_UARTRTS)) == SC_UARTFLOW) {
    // Assert RTS if the rx queue tail is in an active (or pending) DMA buffer,
    // because this means the other DMA buffer is empty.
    ATOMIC (
      if ( ( (emSerialRxQueues[1]->tail < EMBER_SERIAL1_RX_QUEUE_SIZE/2) &&
             (SC1_DMACTRL & SC_RXLODA) ) ||
           ( (emSerialRxQueues[1]->tail >= EMBER_SERIAL1_RX_QUEUE_SIZE/2) 
              && (SC1_DMACTRL & SC_RXLODB) ) ) {
          SC1_UARTCFG |= SC_UARTRTS;          // assert RTS
      }
    )
  }
}
#endif

#ifdef EMBER_SERIAL1_RTSCTS
boolean halInternalUart1FlowControlRxIsEnabled(void)
{
  return ( (SC1_UARTCFG & (SC_UARTFLOW | SC_UARTAUTO | SC_UARTRTS)) ==
           (SC_UARTFLOW | SC_UARTRTS) );
}
#endif
#ifdef EMBER_SERIAL1_XONXOFF
boolean halInternalUart1FlowControlRxIsEnabled(void)
{
  xonTimer = halCommonGetInt16uQuarterSecondTick(); //FIXME move into new func?
  return ( (xonXoffTxByte == 0) && (xcmdCount > 0) );
}

boolean halInternalUart1XonRefreshDone(void)
{
  return (xcmdCount == XON_REFRESH_COUNT);
}
#endif

boolean halInternalUart1TxIsIdle(void)
{
  return ( (SC1_MODE == SC1_MODE_UART) && 
           ((SC1_UARTSTAT & SC_UARTTXIDLE) != 0) );
}

#if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
// If called outside of an ISR, it should be from within an ATOMIC block.
void halInternalUart1TxIsr(void)
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[1];

  // At present we really don't care which interrupt(s)
  // occurred, just that one did.  Loop while there is
  // room to send more data and we've got more data to
  // send.  For UART there is no error detection.

#ifdef EMBER_SERIAL1_XONXOFF
  // Sending an XON or XOFF takes priority over data in the tx queue.
  if (xonXoffTxByte && (SC1_UARTSTAT & SC_UARTTXFREE) ) {
    SC1_DATA = xonXoffTxByte;
    if (xonXoffTxByte == ASCII_XOFF) {
      xcmdCount = -1;
      HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_XOFF);
    } else {
      xcmdCount = (xcmdCount < 0) ? 1: xcmdCount + 1;
    }
    xonXoffTxByte = 0;    // clear to indicate XON/XOFF was sent
  }
#endif
  while ( (q->used > 0) && (SC1_UARTSTAT & SC_UARTTXFREE) ) {
    SC1_DATA = FIFO_DEQUEUE(q, emSerialTxQueueWraps[1]);
  }
}
#elif   (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
// If called outside of an ISR, it should be from within an ATOMIC block.
void halInternalUart1TxIsr(void)
{
  EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[1];
  static EmberMessageBuffer holdBuf[2] = { EMBER_NULL_MESSAGE_BUFFER,
                                           EMBER_NULL_MESSAGE_BUFFER };

  // The only interrupts we care about here are UNLOAD's and IDLE.
  // Our algorithm doesn't really care which interrupt occurred,
  // or even if one really didn't.  If there is data to send and
  // a DMA channel available to send it, then out it goes.

  assert( !((q->used == 0) && (q->nextByte != NULL)) );
  while ( q->used > 0 ) {

    if ( q->nextByte == NULL ) {
      // new message pending, but nextByte not set up yet
      emSerialBufferNextMessageIsr(q);
    }

    // Something to send: do we have a DMA channel to send it on?
    // Probe for an available channel by checking the channel's
    // SC1_DMACTRL.TX_LOAD   == 0 (channel unloaded) &&
    // SC1_DMASTAT.TX_ACTIVE == 0 (channel not active)
    // The latter check should be superfluous but is a safety mechanism.
    if ( !(SC1_DMACTRL & SC_TXLODA) &&
         !(SC1_DMASTAT & SC_TXACTA) ) {
      // Channel A is available
      SC1_TXBEGA  = (int32u)q->nextByte;
      SC1_TXENDA = (int32u)q->lastByte;
      INT_SC1FLAG = INT_SCTXULDA; // Ack if pending
      SC1_DMACTRL = SC_TXLODA;
      // Release previously held buffer and hold the newly-loaded one
      // so we can safely use emSerialBufferNextBlockIsr() to check for
      // more data to send without the risk of reusing a buffer we're
      // in the process of DMA-ing.
      if (holdBuf[0] != EMBER_NULL_MESSAGE_BUFFER)
        emberReleaseMessageBuffer(holdBuf[0]);
      holdBuf[0] = q->currentBuffer;
      emberHoldMessageBuffer(holdBuf[0]);
      emSerialBufferNextBlockIsr(q, 1);
    } else
    if ( !(SC1_DMACTRL & SC_TXLODB) &&
         !(SC1_DMASTAT & SC_TXACTB) ) {
      // Channel B is available
      SC1_TXBEGB  = (int32u)q->nextByte;
      SC1_TXENDB = (int32u)q->lastByte;
      INT_SC1FLAG = INT_SCTXULDB; // Ack if pending
      SC1_DMACTRL = SC_TXLODB;
      // Release previously held buffer and hold the newly-loaded one
      // so we can safely use emSerialBufferNextBlockIsr() to check for
      // more data to send without the risk of reusing a buffer we're
      // in the process of DMA-ing.
      if (holdBuf[1] != EMBER_NULL_MESSAGE_BUFFER)
        emberReleaseMessageBuffer(holdBuf[1]);
      holdBuf[1] = q->currentBuffer;
      emberHoldMessageBuffer(holdBuf[1]);
      emSerialBufferNextBlockIsr(q, 1);
    } else {
      // No channels available; can't send anything now so break out of loop
      break;
    }

  } // while ( q->used > 0 )

  // Release previously-held buffer(s) from an earlier DMA operation
  // if that channel is now free (i.e. it's completed the DMA and we
  // didn't need to use that channel for more output in this call).
  if ( (holdBuf[0] != EMBER_NULL_MESSAGE_BUFFER) &&
       !(SC1_DMACTRL & SC_TXLODA) &&
       !(SC1_DMASTAT & SC_TXACTA) ) {
    emberReleaseMessageBuffer(holdBuf[0]);
    holdBuf[0] = EMBER_NULL_MESSAGE_BUFFER;
  }
  if ( (holdBuf[1] != EMBER_NULL_MESSAGE_BUFFER) &&
       !(SC1_DMACTRL & SC_TXLODB) &&
       !(SC1_DMASTAT & SC_TXACTB) ) {
    emberReleaseMessageBuffer(holdBuf[1]);
    holdBuf[1] = EMBER_NULL_MESSAGE_BUFFER;
  }
}
#endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)


#if !defined(EM_SERIAL1_DISABLED)
  //The following registers are the only SC1-UART registers that need to be
  //saved across deep sleep cycles.  All other SC1-UART registers are 
  //reenabled or restarted using more complex init or restart algorithms.
  static int32u  SC1_UARTPER_SAVED;
  static int32u  SC1_UARTFRAC_SAVED;
  static int32u  SC1_UARTCFG_SAVED;
#endif//!defined(EM_SERIAL1_DISABLED)

void halInternalPowerDownUart(void)
{
  #if !defined(EM_SERIAL1_DISABLED)
    SC1_UARTPER_SAVED = SC1_UARTPER;
    SC1_UARTFRAC_SAVED = SC1_UARTFRAC;
    SC1_UARTCFG_SAVED = SC1_UARTCFG;
  #endif//!defined(EM_SERIAL1_DISABLED)
}

void halInternalPowerUpUart(void)
{
  #if !defined(EM_SERIAL1_DISABLED)
    SC1_UARTPER = SC1_UARTPER_SAVED;
    SC1_UARTFRAC = SC1_UARTFRAC_SAVED;
    SC1_UARTCFG = SC1_UARTCFG_SAVED;

    SC1_MODE = SC1_MODE_UART;

    // Int configuration here must match that of halInternalUartInit()
    #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
      // Make the RX Valid interrupt level sensitive (instead of edge)
      // SC1_INTMODE = SC_SPIRXVALMODE;
      // Enable just RX interrupts; TX interrupts are controlled separately
      INT_SC1CFG |= (INT_SCRXVAL   |
                     INT_SCRXOVF   |
                     INT_SC1FRMERR |
                     INT_SC1PARERR);
      INT_SC1FLAG = 0xFFFF; // Clear any stale interrupts
      INT_CFGSET = INT_SC1;
    #elif (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
      halInternalRestartUart1Dma();
      #ifndef EZSP_UART
        INT_SC1CFG |= (INT_SCRXOVF   |
                       INT_SC1FRMERR |
                       INT_SC1PARERR);
      #endif
      // The receive side of buffer mode does not require any interrupts.
      // The transmit side of buffer mode requires interrupts, which
      // will be configured on demand in halInternalStartUartTx(), so just
      // enable the top level SC1 interrupt for the transmit side.
      INT_SC1FLAG = 0xFFFF; // Clear any stale interrupts
      INT_CFGSET = INT_SC1; // Enable top-level interrupt

      #ifdef EMBER_SERIAL1_RTSCTS
        // Software-based RTS/CTS needs interrupts on DMA buffer unloading.
        INT_SC1CFG |= (INT_SCRXULDA | INT_SCRXULDB);
      #endif
    #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
  #endif//!defined(EM_SERIAL1_DISABLED)
  
  #if !defined(EM_SERIAL3_DISABLED)
    #if defined (CORTEXM3_EM35X_USB)
      //Remember, halInternalPowerUpUart does not return anything.  Powering
      //up the USB requires going through its normal configuration and
      //enumeration process.
      usbConfigEnumerate();
    #endif
  #endif//!defined(EM_SERIAL3_DISABLED)

}


void halInternalRestartUart(void)
{
  #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
    halInternalRestartUart1Dma();
  #endif//(EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER)
}

#if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO) && defined(EMBER_SERIAL1_XONXOFF)
void halInternalUartFlowControl(int8u port)
{
  if (port == 1) {
    int16u used = emSerialRxQueues[1]->used;
    int8u time = halCommonGetInt16uQuarterSecondTick();

    if (used) {
      xonTimer = time;
    }
    // Send an XON if the rx queue is below the XON threshold
    // and an XOFF had been sent that needs to be reversed
    ATOMIC(
      if ( (xcmdCount == -1) && (used <= XON_LIMIT) ) {
        halInternalUart1ForceXon();
      } else if ( (used == 0) && 
                  ((int8u)(time - xonTimer) >= XON_REFRESH_TIME) && 
                  (xcmdCount < XON_REFRESH_COUNT) ) {
        halInternalUart1ForceXon();
      }
    )
  }
}
#endif

#ifdef EMBER_SERIAL1_XONXOFF
// Must be called from within an ATOMIC block.
static void halInternalUart1ForceXon(void)
{
  if (xonXoffTxByte == ASCII_XOFF) {  // if XOFF waiting to be sent, cancel it
    xonXoffTxByte = 0;
    xcmdCount = 0;
  } else {                            // else, send XON and record the time
    xonXoffTxByte = ASCII_XON;
    halInternalStartUartTx(1);
  }
  xonTimer = halCommonGetInt16uQuarterSecondTick();
}
#endif

#if !defined(EM_SERIAL1_DISABLED)
void halSc1Isr(void)
{
  int32u interrupt;

  //this read and mask is performed in two steps otherwise the compiler
  //will complain about undefined order of volatile access
  interrupt = INT_SC1FLAG;
  interrupt &= INT_SC1CFG;
  
  #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
    while (interrupt != 0) {
  #endif // (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
  
  INT_SC1FLAG = interrupt; // acknowledge the interrupts early
  
  // RX events
  if ( interrupt & (INT_SCRXVAL   | // RX has data
                    INT_SCRXOVF   | // RX Overrun error
                    INT_SCRXFIN   | // RX done [TWI]
                    INT_SCNAK     | // RX Nack [TWI]
                    INT_SCRXULDA  | // RX DMA A has data
                    INT_SCRXULDB  | // RX DMA B has data
                    INT_SC1FRMERR | // RX Frame error
                    INT_SC1PARERR ) // RX Parity error
     ) {
    halInternalUart1RxIsr(interrupt);
  }
  
  // TX events
  if ( interrupt & (INT_SCTXFREE | // TX has room
                    INT_SCTXIDLE | // TX idle (more room)
                    INT_SCTXUND  | // TX Underrun [SPI/TWI]
                    INT_SCTXFIN  | // TX complete [TWI]
                    INT_SCCMDFIN | // TX Start/Stop done [TWI]
                    INT_SCTXULDA | // TX DMA A has room
                    INT_SCTXULDB ) // TX DMA B has room
     ) {
    halInternalUart1TxIsr();
  }
  
  #if (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
    interrupt = INT_SC1FLAG;
    interrupt &= INT_SC1CFG;
  }
  #endif // (EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO)
}
#endif//!defined(EM_SERIAL1_DISABLED)

static void uartErrorMark(int8u port, int8u errors) 
{
  EmSerialFifoQueue *q = emSerialRxQueues[port];
  
  // save error code & location in queue
  if ( emSerialRxError[port] == EMBER_SUCCESS ) {
    emSerialRxErrorIndex[port] = q->head;
    emSerialRxError[port] = errors;
  } else {
    // Flush back to previous error location & update value
    q->head = emSerialRxErrorIndex[port];
    emSerialRxError[port] = errors;
    if(q->head < q->tail) {
      q->used = (emSerialRxQueueSizes[port] - q->tail) + q->head;   
    } else {
      q->used = q->head - q->tail;
    }
  }
}

