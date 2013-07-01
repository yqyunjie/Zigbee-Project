/**
 * File: uart.c
 * Description: HAL Uart abstraction
 *
 * Culprit(s):  Lee Taylor (lee@ember.com)
 *              Jeff Mathews (jeff@ember.com)
 *
 * Copyright 2008 by Ember Corporation.  All rights reserved.               *80*
 */

#include PLATFORM_HEADER

#ifdef EZSP_HOST
  #include "stack/include/ember-types.h"
  #include "stack/include/error.h"
  #include "stack/include/ember-debug.h"
#else
  #include "stack/include/ember.h"
#endif

#include "hal/hal.h" 
#include "app/util/serial/serial.h"
#include "hal/micro/avr-atmega/uart.h"

#ifdef DEBUG
  // Debug builds redirect the ports to different physical ports, allowing
  //  for "virtual uart" support.
  // For example, when EMBER_SERIAL0_DEBUG is defined:
  //  "serial port"  "physical uart"  "Description"
  //        2              0          The port used for the debug channel
  //        1              1          Ordinary serial port
  //        0           virtual       uart is redirect to debug channel messages
  #if defined(EMBER_SERIAL0_DEBUG)
    #define PHYSICAL_PORT_0  2
    #define EM_PHYSICAL_SERIAL0_MODE EMBER_SERIAL2_MODE
    #define PHYSICAL_PORT_1  1
    #define EM_PHYSICAL_SERIAL1_MODE EMBER_SERIAL1_MODE
    #ifdef EMBER_SERIAL0_MODE
      #define ENABLE_VIRTUAL_PORT
      #define VIRTUAL_PORT   0
      #define EM_VIRTUAL_MODE EMBER_SERIAL0_MODE
    #endif
  #elif defined(EMBER_SERIAL1_DEBUG)
    #define PHYSICAL_PORT_0  0
    #define EM_PHYSICAL_SERIAL0_MODE EMBER_SERIAL0_MODE
    #define PHYSICAL_PORT_1  2
    #define EM_PHYSICAL_SERIAL1_MODE EMBER_SERIAL2_MODE
    #ifdef EMBER_SERIAL1_MODE
      #define ENABLE_VIRTUAL_PORT
      #define VIRTUAL_PORT   1
      #define EM_VIRTUAL_MODE EMBER_SERIAL1_MODE
    #endif
  #else
    #error no debug port defined
  #endif
#else
  // Non-debug builds do no remapping
  #define PHYSICAL_PORT_0  0
  #define EM_PHYSICAL_SERIAL0_MODE EMBER_SERIAL0_MODE
  #define PHYSICAL_PORT_1  1
  #define EM_PHYSICAL_SERIAL1_MODE EMBER_SERIAL1_MODE
  // VIRTUAL_PORT explicitly not defined
#endif

// Allow some code to be disabled (and flash saved) if
//  a port is unused or in low-level driver mode
#if (EM_PHYSICAL_SERIAL0_MODE == EMBER_SERIAL_UNUSED) || \
    (EM_PHYSICAL_SERIAL0_MODE == EMBER_SERIAL_LOWLEVEL)
  #define EM_PHYSICAL_SERIAL0_DISABLED
#endif
#if (EM_PHYSICAL_SERIAL1_MODE == EMBER_SERIAL_UNUSED) || \
    (EM_PHYSICAL_SERIAL1_MODE == EMBER_SERIAL_LOWLEVEL)
  #define EM_PHYSICAL_SERIAL1_DISABLED
#endif

#define INVALID 65535 // frequency error exceeds +/-0.5%, given sysclock

// Save flash if both are disabled
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED) || \
    !defined(EM_PHYSICAL_SERIAL1_DISABLED)     

#if defined(EZSP_UART) && !defined(EMBER_SERIAL1_XONXOFF)
#error EZSP-UART requires XON/XOFF flow control!
#endif

#ifdef AVR_ATMEGA_32
  //The ATmega32L operates at 3.6864MHz; correct baud settings
  const int16u PGM baudSettings[] = { 
    767,      // 300 0.0%
    383,      // 600 0.0%
    255,      // 900 0.0%
    191,      // 1200 0.0%
    95,       // 2400 0.0%
    47,       // 4800 0.0% 
    23,       // 9600 0.0%
    15,       // 14.4k 0.0% 
    11,       // 19.2k 0.0% 
    7,        // 28.8k 0.0%
    5,        // 38.4k 0.0%
    INVALID,  // 50.0k
    3,        // 57.6k 0.0%
    2,        // 76.8k 0.0%
    INVALID,  // 100.0k
    1,        // 115.2k 0.0%
    #ifdef EMBER_SERIAL_BAUD_CUSTOM
      EMBER_SERIAL_BAUD_CUSTOM //Hook for custom baud rate, see BOARD_HEADER
    #else
      5  //if EMBER_SERIAL_BAUD_CUSTOM is undefined, make BAUD_CUSTOM 38.4k
    #endif
  };
#else //AVR_ATMEGA_32
  //Everything else operates at 8MHz
  // avr datasheet table 83. U2X=0
  const int16u PGM baudSettings[] = { 
    1666,     // 300  0.0%
    832,      // 600  0.0%
    555,      // 900 -0.1%
    415,      // 1200 0.1%
    207,      // 2400 0.2%
    103,      // 4800 0.2% 
    51,       // 9600 0.2%
    INVALID,  // 14.4k -0.8% 
    25,       // 19.2k 0.2% 
    INVALID,  // 28.8k 2.1%
    12,       // 38.4k 0.2%
    9,        // 50.0k 0.0%
    8,        // 57.6k -3.5% use only if data received has 2 stop bits
    INVALID,  // 76.8k -7.0%
    4,        // 100.0k 0.0%%
    INVALID,   // 115.2k 8.5%
    #ifdef EMBER_SERIAL_BAUD_CUSTOM
      EMBER_SERIAL_BAUD_CUSTOM //Hook for custom baud rate, see BOARD_HEADER
    #else
      12  //if EMBER_SERIAL_BAUD_CUSTOM is undefined, make BAUD_CUSTOM 38.4k
    #endif
  };
#endif //AVR_ATMEGA_32

#endif // DISABLED

#ifdef EMBER_SERIAL1_XONXOFF
#if defined(EM_PHYSICAL_SERIAL1_DISABLED) || \
  (EM_PHYSICAL_SERIAL1_MODE != EMBER_SERIAL_FIFO)
#error "Illegal serial port 1 configuration"
#endif

static void halInternalForceXon(void);  // forward declaration

static int8s xcmdCount;         // number of XON/XOFFs sent to host:
                                // XOFFs count -1, XONs count +1, 0 = XON state
static int8u xonXoffTxByte;     // if non-zero, transmitted ahead of queued data
static int8u xonTimer;          // time since data rx'ed from the host (1/4 secs)

#define ASCII_XON         0x11  // requests host to pause sending 
#define ASCII_XOFF        0x13  // requests host to resume sending
#define XON_REFRESH_TIME  8     // delay between repeat XONs (1/4 sec units)
#define XON_REFRESH_COUNT 3     // max number of repeat XONs to send after 1st

// Define thresholds for XON/XOFF flow control in terms of queue used values
#if (EMBER_SERIAL1_RX_QUEUE_SIZE == 128)
  #define XON_LIMIT       24    // send an XON
  #define XOFF_LIMIT      80    // send an XOFF
#elif (EMBER_SERIAL1_RX_QUEUE_SIZE == 64)
  #define XON_LIMIT       4 
  #define XOFF_LIMIT      36
#elif (EMBER_SERIAL1_RX_QUEUE_SIZE == 32)
  #define XON_LIMIT       2
  #define XOFF_LIMIT      12
#else
  #error "Serial port 1 receive FIFO too small!"
#endif

#endif

// Bit-vector to indicate which ports have transmitted data.
// The transmit complete flag (TXC) is only useful after the first transmission.
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED) || \
    !defined(EM_PHYSICAL_SERIAL1_DISABLED)     
static int8u txStarted = 0;
#endif

#if !defined(EM_PHYSICAL_SERIAL0_DISABLED) || \
    !defined(EM_PHYSICAL_SERIAL1_DISABLED)     
EmberStatus halInternalUartInit(int8u port,
                                SerialBaudRate rate,
                                SerialParity parity,
                                int8u stopbits)
{
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  int8u ucsrc;  // support for the mega8 (see below)
#endif

  if(rate > BAUD_115200 || baudSettings[rate] == INVALID) 
    return EMBER_SERIAL_INVALID_BAUD_RATE;

  // on the MEGA8, the UBRRH and the UCSRC registers share the same memory
  // location.  To write to the UCSRC, bit 7 must be a one.  The read cycle is very
  // cryptic by comparison.  The only way to read the UCSRC register on the MEGA8
  // is to read from this memory location in two successive clock cycles.  The
  // first read is always from UBRRH, the second read is from UCSRC.  
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  if(port == PHYSICAL_PORT_0) {
#if defined(AVR_ATMEGA_8) || defined(AVR_ATMEGA_32)
    ucsrc = UBRRH; // required on the MEGA 8.  Do not delete!
#endif
    ucsrc = UCSR0C;
#if defined(AVR_ATMEGA_8) || defined(AVR_ATMEGA_32)
    ucsrc |= BIT(URSEL);
#endif
    
    if(parity == PARITY_ODD) {
      ucsrc |= (BIT(UPM00) | BIT(UPM01));
    } else if(parity == PARITY_EVEN) {
      ucsrc |= BIT(UPM01);
      ucsrc &= ~(BIT(UPM00));
    }
    if(stopbits == 2) {
      ucsrc |= BIT(USBS0);
    }

    UCSR0C = ucsrc;

    UBRR0H  = (baudSettings[rate]>>8)&0xFF; 
    UBRR0L  = (baudSettings[rate])&0xFF; 
    // Enable UART transmitter and receiver
    UCSR0B = BIT(RXCIE0) | BIT(TXEN0) | BIT(RXEN0);
    // Clear(one) UART transmit complete bit (TXC)
    UCSR0A = BIT(TXC0);  // Flag cleared by writing a one
    // Clear txStarted flag for port 0
    txStarted &= ~BIT(PHYSICAL_PORT_0);
    return EMBER_SUCCESS;
  } 
#endif // SERIAL0

#if !defined(AVR_ATMEGA_8) && !defined(AVR_ATMEGA_32) && !defined(EM_PHYSICAL_SERIAL1_DISABLED)  
  if(port == PHYSICAL_PORT_1) {
    if(parity == PARITY_ODD) {
      UCSR1C |= (BIT(UPM10) | BIT(UPM11));
    } else if(parity == PARITY_EVEN) {
      UCSR1C |= BIT(UPM11);
      UCSR1C &= ~(BIT(UPM10));
    }
    if(stopbits == 2) {
      UCSR1C |= BIT(USBS1);
    }
    UCSR1C |= BIT(2);
    UCSR1C |= BIT(1);
    UBRR1H  = (baudSettings[rate]>>8)&0xFF; 
    UBRR1L  = (baudSettings[rate])&0xFF; 
//    UBRR1L  = (baudSettings[rate]/2);
    // Enable UART transmitter and receiver
    UCSR1B = BIT(RXCIE1) | BIT(TXEN1) | BIT(RXEN1);
    // Clear(one) UART transmit complete bit (TXC)
    UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
    // Clear txStarted flag for port 1
    txStarted &= ~BIT(PHYSICAL_PORT_1);
#ifdef EMBER_SERIAL1_XONXOFF
    halInternalForceXon();
#endif
    return EMBER_SUCCESS;
  } 
#endif

#ifdef ENABLE_VIRTUAL_PORT  
  if(port == VIRTUAL_PORT) {
    // nothing special to do
    return EMBER_SUCCESS;
  }
#endif

  return EMBER_SERIAL_INVALID_PORT;
}
#endif // If both ports are low-level

#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  static int8u savedUart0State;
#endif
#if !defined(AVR_ATMEGA_8) && !defined(EM_PHYSICAL_SERIAL1_DISABLED)  
  static int8u savedUart1State;
#endif

void halInternalPowerDownUart(void)
{
  #if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
    savedUart0State = UCSR0B;
    UCSR0B &= ~(BIT(RXCIE0) | BIT(TXCIE0) | BIT(UDRIE0) | BIT(RXEN0) | BIT(TXEN0));
  #endif
  #if !defined(AVR_ATMEGA_8) && !defined(EM_PHYSICAL_SERIAL1_DISABLED)  
    savedUart1State = UCSR1B;
    UCSR1B &= ~(BIT(RXCIE1) | BIT(TXCIE1) | BIT(UDRIE1) | BIT(RXEN1) | BIT(TXEN1));
  #endif
}

void halInternalPowerUpUart(void)
{
  #if !defined(EM_PHYSICAL_SERIAL0_DISABLED) || \
      !defined(EM_PHYSICAL_SERIAL1_DISABLED)
    txStarted = 0;
  #endif
  #if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
    UCSR0B = savedUart0State;
  #endif
  #if !defined(AVR_ATMEGA_8) && !defined(EM_PHYSICAL_SERIAL1_DISABLED)  
    UCSR1B = savedUart1State;
  #endif
}

void halInternalStartUartTx(int8u port)
{
  switch(port) {
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  case PHYSICAL_PORT_0:
    // Enable Interrupt
    UCSR0B |= BIT(UDRIE0);
    txStarted |= BIT(PHYSICAL_PORT_0);
    break;
#endif
#if !defined(EM_PHYSICAL_SERIAL1_DISABLED)
  case PHYSICAL_PORT_1:
    UCSR1B |= BIT(UDRIE1);
    txStarted |= BIT(PHYSICAL_PORT_1);
    break;
#endif
#ifdef ENABLE_VIRTUAL_PORT
  case VIRTUAL_PORT: 
    {
#if EM_VIRTUAL_MODE == EMBER_SERIAL_FIFO
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[VIRTUAL_PORT];
      assert(q->tail == 0);
      emDebugSendVuartMessage(q->fifo, q->used);
      q->used = 0;
      q->head = 0;
      break;
#endif
#if EM_VIRTUAL_MODE == EMBER_SERIAL_BUFFER
      EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[VIRTUAL_PORT];
      assert(q->nextByte == NULL);
      emSerialBufferNextMessageIsr(q);
      while(q->nextByte != NULL) {
        emDebugSendVuartMessage(q->nextByte, (q->lastByte-q->nextByte)+1);
        emSerialBufferNextBlockIsr(q,VIRTUAL_PORT);
      }
      break;
#endif
    }
#endif
  }
}

void halInternalStopUartTx(int8u port)
{
  switch(port) {
#if EM_PHYSICAL_SERIAL0_MODE != EMBER_SERIAL_UNUSED
  case PHYSICAL_PORT_0:
    // Disable Interrupt
    UCSR0B &= ~BIT(UDRIE0);
    break;
#endif
#if EM_PHYSICAL_SERIAL1_MODE != EMBER_SERIAL_UNUSED
  case PHYSICAL_PORT_1:
    UCSR1B &= ~BIT(UDRIE1);
    break;
#endif
  //Nothing to do with VIRTUAL_PORT
  }
}

#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
// If there is a serial transmission error, the location in the q where
//  it occurred is remembered so that the error can be reported to the
//  application at the appropriate time.  If a second error occurs before the
//  first is reported we discard everything that arrived between the two errors
#pragma vector = USART0_RXC_vect
__interrupt void halInternalUart0RxIsr(void)
{
  EmSerialFifoQueue *q = emSerialRxQueues[PHYSICAL_PORT_0];
  int8u errors = UCSR0A & (BIT(FE0) | BIT(DOR0) | BIT(UPE0));
  int8u incoming = UDR0;
  INT_DEBUG_BEG_MISC_INT();

  // Since queue sizes are powers-of-two the Mask is the same as
  //  the queue size minus 1, so is safe to use in this comparison
  if((errors == 0) && (q->used < emSerialRxQueueMasks[PHYSICAL_PORT_0])) {
    FIFO_ENQUEUE(q,incoming,emSerialRxQueueMasks[PHYSICAL_PORT_0]);
  } else {
    //translate error code
    if( (errors == 0) )
      errors = EMBER_SERIAL_RX_OVERFLOW;
    else if( (errors & BIT(DOR0)) )
      errors = EMBER_SERIAL_RX_OVERRUN_ERROR;
    else if( (errors & BIT(FE0)) )
      errors = EMBER_SERIAL_RX_FRAME_ERROR;
    else if( (errors & BIT(UPE0)) )
      errors = EMBER_SERIAL_RX_PARITY_ERROR;
    else // unknown
      errors = EMBER_ERR_FATAL;
    
    // save error code & location in queue
    if(emSerialRxError[PHYSICAL_PORT_0] == EMBER_SUCCESS) {
      emSerialRxErrorIndex[PHYSICAL_PORT_0] = q->head;
      emSerialRxError[PHYSICAL_PORT_0] = errors;
    } else {
      // Flush back to previous error location & update value
      q->head = emSerialRxErrorIndex[PHYSICAL_PORT_0];
      emSerialRxError[PHYSICAL_PORT_0] = errors;
      q->used = ((q->head - q->tail) & emSerialRxQueueMasks[PHYSICAL_PORT_0]);
    }
  }
  INT_DEBUG_END_MISC_INT();
}
#endif

#if EM_PHYSICAL_SERIAL0_MODE == EMBER_SERIAL_FIFO
#pragma vector = USART0_UDRE_vect
__interrupt void halInternalUart0TxIsr(void)
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[PHYSICAL_PORT_0];
  INT_DEBUG_BEG_MISC_INT();

  if(q->used > 0) {
    // Clear TX complete flag (TXCn) before transmission (before UDR is written)
    // so that it can be checked by halInternalWaitUartTxComplete.
    UCSR0A = BIT(TXC0);  // Flag cleared by writing a one
    UDR0 = FIFO_DEQUEUE(q,emSerialTxQueueMasks[PHYSICAL_PORT_0]);
  } else {
    // nothing to send, disable interrupt
    UCSR0B &= ~(BIT(UDRIE0));
  } 
  INT_DEBUG_END_MISC_INT();
}
#elif EM_PHYSICAL_SERIAL0_MODE == EMBER_SERIAL_BUFFER
#pragma vector = USART0_UDRE_vect
__interrupt void halInternalUart0TxIsr(void)
{
  EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[PHYSICAL_PORT_0];
  INT_DEBUG_BEG_MISC_INT();

  if(q->nextByte == NULL) {
    if(q->used > 0) {
      // new message pending, but nextByte not set up yet
      emSerialBufferNextMessageIsr(q);
    } else {
      // Nothing to send, disable interrupt & return
      UCSR0B &= ~(BIT(UDRIE0));
      return;
    }
  }

  UCSR0A = BIT(TXC0);  // Flag cleared by writing a one
  UDR0 = *q->nextByte;
  if(q->lastByte != q->nextByte)
    q->nextByte++;
  else
    emSerialBufferNextBlockIsr(q,PHYSICAL_PORT_0);
  INT_DEBUG_END_MISC_INT();
}
#endif

#if !defined(EM_PHYSICAL_SERIAL1_DISABLED)
#pragma vector = USART1_RXC_vect
__interrupt void halInternalUart1RxIsr(void)
{
  EmSerialFifoQueue *q = emSerialRxQueues[PHYSICAL_PORT_1];

  INT_DEBUG_BEG_UART1_RX();   // if enabled, flag entry to isr on a output pin
  while (UCSR1A & BIT(RXC1)) {
    int8u errors = UCSR1A & (BIT(FE1) | BIT(DOR1) | BIT(UPE1));
    int8u incoming = UDR1;

    // Since queue sizes are powers-of-two the Mask is the same as
    //  the queue size minus 1, so is safe to use in this comparison
    if((errors == 0) && (q->used < emSerialRxQueueMasks[PHYSICAL_PORT_1])) {
  #ifdef EMBER_SERIAL1_XONXOFF
      if ( (incoming != ASCII_XON) && (incoming != ASCII_XOFF) ) {
        FIFO_ENQUEUE(q,incoming,emSerialRxQueueMasks[PHYSICAL_PORT_1]);
      }
  #else
      FIFO_ENQUEUE(q,incoming,emSerialRxQueueMasks[PHYSICAL_PORT_1]);
  #endif
    } else {
      //translate uart status bits into serial error code
      INT_DEBUG_BEG_UART1_RXERR();    // if enabled flag rxc error on output pin
      if( (errors & BIT(DOR1)) ) {
        errors = EMBER_SERIAL_RX_OVERRUN_ERROR;
        HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_OVERRUN_ERROR);
      } else if( (errors & BIT(FE1)) ) {
        errors = EMBER_SERIAL_RX_FRAME_ERROR;
        HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_FRAMING_ERROR);
      } else if( (errors & BIT(UPE1)) ) {
        errors = EMBER_SERIAL_RX_PARITY_ERROR;
      } else {
        errors = EMBER_SERIAL_RX_OVERFLOW;
        HANDLE_ASH_ERROR(EMBER_COUNTER_ASH_OVERFLOW_ERROR);
      }
      // save error code & location in queue
      if(emSerialRxError[PHYSICAL_PORT_1] == EMBER_SUCCESS) {
        emSerialRxErrorIndex[PHYSICAL_PORT_1] = q->head;
        emSerialRxError[PHYSICAL_PORT_1] = errors;
      } else {
        // Flush back to previous error location & update value
        q->head = emSerialRxErrorIndex[PHYSICAL_PORT_1];
        emSerialRxError[PHYSICAL_PORT_1] = errors;
        q->used = ((q->head - q->tail) & emSerialRxQueueMasks[PHYSICAL_PORT_1]);
      }
      INT_DEBUG_END_UART1_RXERR();    // if enabled, end error indication
    }
  }     // end of while (UCSR1A & BIT(RXC))
#ifdef EMBER_SERIAL1_XONXOFF
  if ((q->used >= XOFF_LIMIT) && (xcmdCount >= 0))  {
    xonXoffTxByte = ASCII_XOFF;
    halInternalStartUartTx(1);
  }
#endif
  INT_DEBUG_END_UART1_RX();   // if enabled, flag exit from isr on a output pin
}
#endif  // !defined(EM_PHYSICAL_SERIAL1_DISABLED)

#if EM_PHYSICAL_SERIAL1_MODE == EMBER_SERIAL_FIFO
#ifdef EMBER_SERIAL1_XONXOFF
#pragma vector = USART1_UDRE_vect
__interrupt void halInternalUart1TxIsr(void)
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[PHYSICAL_PORT_1];
  INT_DEBUG_BEG_UART1_TX();   // if enabled, flag entry to isr on a output pin
  do {
    if (xonXoffTxByte) {    // top priority is to send an XON/XOFF
      // Clear TX complete flag (TXCn) before transmission (before UDR is written)
      // so that it can be checked by halInternalWaitUartTxComplete.
      UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
      UDR1 = xonXoffTxByte;
      if (xonXoffTxByte == ASCII_XOFF) {
        xcmdCount = -1;
        HANDLE_ASH_ERROR(EMBER_COUNTER_UTILITY); // ** for test only **
      } else {
        xcmdCount++;
      }
      xonXoffTxByte = 0;    // clear to indicate byte was sent
    } else if (q->used) {   // send next byte from transmit buffer
      // Clear TX complete flag (TXCn) before transmission (before UDR is written)
      // so that it can be checked by halInternalWaitUartTxComplete.
      UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
      UDR1 = FIFO_DEQUEUE(q,emSerialTxQueueMasks[PHYSICAL_PORT_1]);
    } else {                // nothing to send, disable interrupt
      UCSR1B &= ~(BIT(UDRIE1));
    }
  }  while ( (UCSR1A & BIT(UDRE1)) && q->used );
  INT_DEBUG_END_UART1_TX();   // if enabled, flag exit from isr on a output pin
}
  
#else //EMBER_SERIAL1_XONXOFF
#pragma vector = USART1_UDRE_vect
__interrupt void halInternalUart1TxIsr(void)
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[PHYSICAL_PORT_1];
  INT_DEBUG_BEG_UART1_TX();   // if enabled, flag entry to isr on a output pin

  if (q->used > 0) {
    // Clear TX complete flag (TXCn) before transmission (before UDR is written)
    // so that it can be checked by halInternalWaitUartTxComplete.
    UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
    UDR1 = FIFO_DEQUEUE(q,emSerialTxQueueMasks[PHYSICAL_PORT_1]);
  } else {
    // nothing to send, disable interrupt
    UCSR1B &= ~(BIT(UDRIE1));
  }
  INT_DEBUG_END_UART1_TX();   // if enabled, flag exit from isr on a output pin
}
#endif // EMBER_SERIAL1_XONXOFF
#elif EM_PHYSICAL_SERIAL1_MODE == EMBER_SERIAL_BUFFER
#pragma vector = USART1_UDRE_vect
__interrupt void halInternalUart1TxIsr(void)
{
  EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[PHYSICAL_PORT_1];
  INT_DEBUG_BEG_MISC_INT();

  if(q->nextByte == NULL) {
    if(q->used > 0) {
      // new message pending, but nextByte not set up yet
      emSerialBufferNextMessageIsr(q);
    } else {
      // Nothing to send, disable interrupt & return
      UCSR1B &= ~(BIT(UDRIE1));
      return;
    }
  }

  UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
  UDR1 = *q->nextByte;
  if(q->lastByte != q->nextByte)
    q->nextByte++;
  else
    emSerialBufferNextBlockIsr(q,PHYSICAL_PORT_1);
  INT_DEBUG_END_MISC_INT();
}
#endif

#ifdef EMBER_SERIAL1_XONXOFF
void halInternalUartFlowControl(int8u port)
{
  if (port == 1) {
    int8u time = halCommonGetInt16uQuarterSecondTick();
    int8u used = emSerialRxQueues[1]->used;

    if (used) {
      xonTimer = time;
    }
    // Send an XON if the rx queue is below the XON threshold
    // and an XOFF had been sent that needs to be reversed
    if ( (xcmdCount == -1) && (used <= XON_LIMIT) ) {
      halInternalForceXon();
    } else if ( (used == 0) && 
                ((int8u)(time - xonTimer) > XON_REFRESH_TIME) && 
                (xcmdCount < XON_REFRESH_COUNT) ) {
      halInternalForceXon();
    }
  }
}

static void halInternalForceXon()
{
  ATOMIC_LITE (
    if (xonXoffTxByte == 0) {   // send an XON only if an XOFF is not pending
      xonXoffTxByte = ASCII_XON;
      halInternalStartUartTx(1);
    }
  )
  xonTimer = halCommonGetInt16uQuarterSecondTick();
}
#endif

#ifdef ENABLE_VIRTUAL_PORT
void halStackReceiveVuartMessage(int8u *data, int8u length)
{
  EmSerialFifoQueue *q = emSerialRxQueues[VIRTUAL_PORT];

  while(length--) {
    // Since queue sizes are powers-of-two the Mask is the same as
    //  the queue size minus 1, so is safe to use in this comparison
    if((q->used < emSerialRxQueueMasks[VIRTUAL_PORT])) {
      FIFO_ENQUEUE(q,*data++,emSerialRxQueueMasks[VIRTUAL_PORT]);
    } else {    
      // save error code & location in queue
      if(emSerialRxError[VIRTUAL_PORT] == EMBER_SUCCESS) {
        emSerialRxErrorIndex[VIRTUAL_PORT] = q->head;
        emSerialRxError[VIRTUAL_PORT] = EMBER_SERIAL_RX_OVERFLOW;
      } else {
        // Flush back to previous error location & update value
        q->head = emSerialRxErrorIndex[VIRTUAL_PORT];
        emSerialRxError[VIRTUAL_PORT] = EMBER_SERIAL_RX_OVERFLOW;
        q->used = ((q->head - q->tail) & emSerialRxQueueMasks[VIRTUAL_PORT]);
      }
      return;  // no sense in trying to enqueue the rest
    }
  }
}
#endif

// full blocking, no queue overflow issues, can be used in or out of int context
// does not return until data is transmitted.
// this is useful for things like assert messages, should not be used
//  during normal operation
EmberStatus halInternalForceWriteUartData(int8u port, int8u *data, int8u length)
{
  switch(port) {
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  case PHYSICAL_PORT_0:
    while(length--) {
      // Block until data register empty
      while ( !( UCSR0A & BIT(UDRE0)) ) {}
      // Set txStarted flag for port 0
      txStarted |= BIT(PHYSICAL_PORT_0);
      // Clear TX complete flag (TXCn) before UDR is written
      UCSR0A = BIT(TXC0);  // Flag cleared by writing a one
      UDR0 = *data;
      data++;
    }
    // Block until TX complete
    while( !(UCSR0A & BIT(TXC0)) ) { }
    break;
#endif
#if !defined(EM_PHYSICAL_SERIAL1_DISABLED)
  case PHYSICAL_PORT_1:
    while(length--) {
      // Block until data register empty
      while ( !( UCSR1A & BIT(UDRE1)) ) {}
      // Set txStarted flag for port 1
      txStarted |= BIT(PHYSICAL_PORT_1);
      // Clear TX complete flag (TXCn) before UDR is written
      UCSR1A = BIT(TXC1);  // Flag cleared by writing a one
      UDR1 = *data;
      data++;
    }
    // Block until TX complete
    while( !(UCSR1A & BIT(TXC1)) ) { }
    break;
#endif
#ifdef ENABLE_VIRTUAL_PORT
  case VIRTUAL_PORT:
    emDebugSendVuartMessage(data, length);
    break;
#endif
  default:
    return EMBER_SERIAL_INVALID_PORT;
  }
  return EMBER_SUCCESS;
}

// Useful for waiting on serial port characters before interrupts have been
// turned on.  
EmberStatus halInternalForceReadUartByte(int8u port, int8u* dataByte)
{
  EmberStatus err=EMBER_SERIAL_RX_EMPTY;

  switch(port) {
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
  case PHYSICAL_PORT_0:
    if ( UCSR0A & BIT(RXC0) ) {
      *dataByte = UDR0;
      err = EMBER_SUCCESS;
    }
    break;
#endif
#if !defined(EM_PHYSICAL_SERIAL1_DISABLED)
  case PHYSICAL_PORT_1:
    if ( UCSR1A & BIT(RXC1) ) {
      *dataByte = UDR1;
      err = EMBER_SUCCESS;
    }
    break;
#endif
#ifdef ENABLE_VIRTUAL_PORT
  case VIRTUAL_PORT:
    // not possible
    break;
#endif
  }
  return err;
}

// blocks until the text actually goes out
// Checks the flag TXCn (USART Transmit Complete)
// This flag bit is set when the entire frame in the Transmit Shift Register has
// been shifted out and there are no new data currently present in the transmit 
// buffer (UDRn). The TXCn flag bit is automatically cleared when a transmit 
// complete interrupt is executed, or it can be cleared by writing a one to its 
// bit location. The TXCn flag can generate a Transmit Complete interrupt.
void halInternalWaitUartTxComplete(int8u port)
{
   halResetWatchdog();
#if !defined(EM_PHYSICAL_SERIAL0_DISABLED)
   if (port == PHYSICAL_PORT_0) {
     // if no data has been transmitted on this port, then TXC will never be set
     // so blocking on it would be bad.
     if (txStarted & BIT(PHYSICAL_PORT_0)) {
       // last byte was shifted out of shift register
       while( !(UCSR0A & BIT(TXC0)) ) { }
     }
     return;
   }
#endif

#if !defined(EM_PHYSICAL_SERIAL1_DISABLED) 
   if (port == PHYSICAL_PORT_1) {
     if (txStarted & BIT(PHYSICAL_PORT_1)) {
       while( !(UCSR1A & BIT(TXC1)) ) { }
     }
     return;
   }
#endif
}

    
EmberStatus halUart0ReadByteTimeoutTiny(int8u *dataByte)
{
//a timer costs 20 bytes
//error detection costs 6 bytes
/*
    while( !(UCSR0A & BIT(RXC0)) ) {}
    if( !(UCSR0A & (BIT(FE0) | BIT(DOR0) | BIT(UPE0))) ) {
      *dataByte = UDR0;
      return EMBER_SUCCESS;
    } else {
      *dataByte = UDR0;
      return EMBER_SERIAL_RX_EMPTY;
    }
*/
  int16u count = 0;
  int8u bah = 0;

  // We do this because the do/while loop is not as optimized at
  // the top end, which is where we really need it to be
 try_again:
  if (UCSR0A & BIT(RXC0)) {
    int8u error = UCSR0A & (BIT(FE0) | BIT(DOR0) | BIT(UPE0));
    *dataByte = UDR0;
    return error;
  }
  if (++count == 0) {
    halResetWatchdog();
    if (++bah == 4) { // 80ms
      return EMBER_SERIAL_RX_EMPTY;
    } 
  }
  goto try_again;
}

