/**
 * File: serial.c
 * Description: High Level Serial Communications
 *
 * Culprit(s):  Lee Taylor (lee@ember.com)
 *              Richard Kelsey (kelsey@ember.com)
 *              Jeff Mathews (jeff@ember.com)
 *
 * Copyright 2004 by Ember Corporation.  All rights reserved.               *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"

//Host processors do not use Ember Message Buffers.
#ifndef EZSP_HOST
  #include "stack/include/packet-buffer.h"
#endif

#include "hal/hal.h"
#include "serial.h"

#include <stdarg.h>


//Documentary comments:
// To conserve precious flash, there is very little validity checking
//  on the given parameters.  Be sure not to use an invalid port number
//  or a port that is unused.
// Blocking routines will always wait for room (but not buffers - if buffers
//  cannot be allocated, no part of the message will be sent)
// Non-blocking routines will never wait for room, and may cause partial
//  messages to be sent.  If a contiguous message needs to be sent, available
//  space should be checked _before_ calling the appropriate write API.


//------------------------------------------------------
// Determine if blocking code needs to be enabled
#if defined(EMBER_SERIAL0_BLOCKING) || \
    defined(EMBER_SERIAL1_BLOCKING) || \
    defined(EMBER_SERIAL2_BLOCKING)
  #define EM_ENABLE_SERIAL_BLOCKING
#endif
#ifdef EMBER_SERIAL0_BLOCKING
  #define EM_SERIAL0_BLOCKSTATE TRUE
#else
  #define EM_SERIAL0_BLOCKSTATE FALSE
#endif
#ifdef EMBER_SERIAL1_BLOCKING
  #define EM_SERIAL1_BLOCKSTATE TRUE
#else
  #define EM_SERIAL1_BLOCKSTATE FALSE
#endif
#ifdef EMBER_SERIAL2_BLOCKING
  #define EM_SERIAL2_BLOCKSTATE TRUE
#else
  #define EM_SERIAL2_BLOCKSTATE FALSE
#endif

//------------------------------------------------------
// Memory allocations for Queue data structures

//Macros to define fifo and buffer queues, can't use a typedef becuase the size
// of the fifo array in the queues can change
#define DEFINE_FIFO_QUEUE(qSize, qName)             \
  static struct {                                   \
    /*! Indexes of next byte to send*/              \
    int8u head;                                     \
    /*! Index of where to enqueue next message*/    \
    int8u tail;                                     \
    /*! Number of bytes queued*/                    \
    volatile int8u used;                            \
    /*! FIFO of queue data*/                        \
    int8u fifo[qSize];                              \
  } qName;

#define DEFINE_BUFFER_QUEUE(qSize, qName)           \
  static struct {                                   \
    /*! Indexes of next message to send*/           \
    int8u head;                                     \
    /*! Index of where to enqueue next message*/    \
    int8u tail;                                     \
    /*! Number of messages queued*/                 \
    volatile int8u used;                            \
    int8u dead;                                     \
    EmberMessageBuffer currentBuffer;               \
    int8u *nextByte, *lastByte;                     \
    /*! FIFO of messages*/                          \
    EmSerialBufferQueueEntry fifo[qSize];           \
  } qName;


// Allocate Appropriate TX Queue for port 0
#if EMBER_SERIAL0_MODE == EMBER_SERIAL_FIFO
  DEFINE_FIFO_QUEUE(EMBER_SERIAL0_TX_QUEUE_SIZE,emSerial0TxQueue)
  #define EM_SERIAL0_TX_QUEUE_ADDR (&emSerial0TxQueue)
#elif EMBER_SERIAL0_MODE == EMBER_SERIAL_BUFFER
  DEFINE_BUFFER_QUEUE(EMBER_SERIAL0_TX_QUEUE_SIZE,emSerial0TxQueue)
  #define EM_SERIAL0_TX_QUEUE_ADDR (&emSerial0TxQueue)
#elif EMBER_SERIAL0_MODE == EMBER_SERIAL_UNUSED || \
      EMBER_SERIAL0_MODE == EMBER_SERIAL_LOWLEVEL
  #define EM_SERIAL0_TX_QUEUE_ADDR (NULL)
  #define EMBER_SERIAL0_TX_QUEUE_SIZE 0
  #define EMBER_SERIAL0_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 1
#if EMBER_SERIAL1_MODE == EMBER_SERIAL_FIFO
  DEFINE_FIFO_QUEUE(EMBER_SERIAL1_TX_QUEUE_SIZE,emSerial1TxQueue)
  #define EM_SERIAL1_TX_QUEUE_ADDR (&emSerial1TxQueue)
#elif EMBER_SERIAL1_MODE == EMBER_SERIAL_BUFFER
  DEFINE_BUFFER_QUEUE(EMBER_SERIAL1_TX_QUEUE_SIZE,emSerial1TxQueue)
  #define EM_SERIAL1_TX_QUEUE_ADDR (&emSerial1TxQueue)
#elif EMBER_SERIAL1_MODE == EMBER_SERIAL_UNUSED || \
      EMBER_SERIAL1_MODE == EMBER_SERIAL_LOWLEVEL 
  #define EM_SERIAL1_TX_QUEUE_ADDR (NULL)
  #define EMBER_SERIAL1_TX_QUEUE_SIZE 0
  #define EMBER_SERIAL1_RX_QUEUE_SIZE 0
#endif

// Allocate Appropriate TX Queue for port 2
#if EMBER_SERIAL2_MODE == EMBER_SERIAL_FIFO
  DEFINE_FIFO_QUEUE(EMBER_SERIAL2_TX_QUEUE_SIZE,emSerial2TxQueue)
  #define EM_SERIAL2_TX_QUEUE_ADDR (&emSerial2TxQueue)
#elif EMBER_SERIAL2_MODE == EMBER_SERIAL_BUFFER
  DEFINE_BUFFER_QUEUE(EMBER_SERIAL2_TX_QUEUE_SIZE,emSerial2TxQueue)
  #define EM_SERIAL2_TX_QUEUE_ADDR (&emSerial2TxQueue)
#elif EMBER_SERIAL2_MODE == EMBER_SERIAL_UNUSED || \
      EMBER_SERIAL2_MODE == EMBER_SERIAL_LOWLEVEL 
  #define EM_SERIAL2_TX_QUEUE_ADDR (NULL)
  #define EMBER_SERIAL2_TX_QUEUE_SIZE 0
  #define EMBER_SERIAL2_RX_QUEUE_SIZE 0
#endif

// Allocate RX Queues (Always FIFOs)
#if EMBER_SERIAL0_MODE != EMBER_SERIAL_UNUSED
  DEFINE_FIFO_QUEUE(EMBER_SERIAL0_RX_QUEUE_SIZE,emSerial0RxQueue)
  #define EM_SERIAL0_RX_QUEUE_ADDR (&emSerial0RxQueue)
#else
  #define EM_SERIAL0_RX_QUEUE_ADDR (NULL)
#endif

#if EMBER_SERIAL1_MODE != EMBER_SERIAL_UNUSED
  DEFINE_FIFO_QUEUE(EMBER_SERIAL1_RX_QUEUE_SIZE,emSerial1RxQueue)
  #define EM_SERIAL1_RX_QUEUE_ADDR (&emSerial1RxQueue)
#else
  #define EM_SERIAL1_RX_QUEUE_ADDR (NULL)
  #define emSerial1RxQueue (NULL)
#endif

#if EMBER_SERIAL2_MODE != EMBER_SERIAL_UNUSED
  DEFINE_FIFO_QUEUE(EMBER_SERIAL2_RX_QUEUE_SIZE,emSerial2RxQueue)
  #define EM_SERIAL2_RX_QUEUE_ADDR (&emSerial2RxQueue)
#else
  #define EM_SERIAL2_RX_QUEUE_ADDR (NULL)
  #define emSerial2RxQueue (NULL)
#endif

//------------------------------------------------------
// Easy access to data structures for a particular port

// The FOR_EACH_PORT(CAST,PREFIX_,_SUFFIX) macro will expand in to something like:
//    CAST(PREFIX_0_SUFFIX),
//    CAST(PREFIX_1_SUFFIX)
// with a line & number for each port of EM_NUM_SERIAL_PORTS

// Data structure for referencing TX Queues
//  (allows for different modes and queue sizes)
void *emSerialTxQueues[EM_NUM_SERIAL_PORTS] = 
  { FOR_EACH_PORT( (void *),EM_SERIAL,_TX_QUEUE_ADDR ) };

int8u PGM emSerialTxQueueSizes[EM_NUM_SERIAL_PORTS] =
  { FOR_EACH_PORT( ,EMBER_SERIAL,_TX_QUEUE_SIZE ) };

int8u PGM emSerialTxQueueMasks[EM_NUM_SERIAL_PORTS] =
  { FOR_EACH_PORT( (int8u),EMBER_SERIAL,_TX_QUEUE_SIZE-1 ) };

// Data structure for referencing RX Queues
//  (allows for different queue sizes)
EmSerialFifoQueue *emSerialRxQueues[EM_NUM_SERIAL_PORTS] = 
  { FOR_EACH_PORT( (EmSerialFifoQueue *),EM_SERIAL,_RX_QUEUE_ADDR ) };

// Only needed by the simulator, so we ifdef it to save two (2) bytes of flash.
#ifdef EMBER_TEST
int8u PGM emberSerialRxQueueSizes[EM_NUM_SERIAL_PORTS] =
  { FOR_EACH_PORT( ,EMBER_SERIAL,_RX_QUEUE_SIZE ) };
#endif

int8u PGM emSerialRxQueueMasks[EM_NUM_SERIAL_PORTS] =
  { FOR_EACH_PORT( (int8u),EMBER_SERIAL,_RX_QUEUE_SIZE-1 ) };

// In-flash data structure for determined port mode
int8u PGM emSerialPortModes[EM_NUM_SERIAL_PORTS] = 
  { FOR_EACH_PORT( ,EMBER_SERIAL,_MODE ) };

#ifdef EM_ENABLE_SERIAL_BLOCKING
// In-flash data structure for blocking mode
boolean PGM emSerialBlocking[EM_NUM_SERIAL_PORTS] =
  { FOR_EACH_PORT( ,EM_SERIAL,_BLOCKSTATE ) };
#endif

int8u emSerialRxError[EM_NUM_SERIAL_PORTS] = {EMBER_SUCCESS,};
int8u emSerialRxErrorIndex[EM_NUM_SERIAL_PORTS] = {0,};

//------------------------------------------------------
// Private functions

// --------------------------------
// A simple printf() implementation
// Supported format specifiers are:
//  %% - percent sign
//  %c - single byte character
//  %s - ram string
//  %p - flash string  (non-standard)
//  %u - 2-byte unsigned decimal
//  %d - 2-byte signed decimal
//  %x %2x %4x - 1, 2, 4 BYTE hex value (always 0 padded) (non-standard)
//    Non-standard behavior: Normally a number after a % is interpreted to be
//    a minimum character width, and the value is not zero padded unless
//    there is a zero before the minimum width value.
//    i.e. '%2x' for the int16u value 0xb prints " b", while '%02x' would print
//    "0b".
//    Ember assumes the number after the % and before the 'x' to be the number
//    of BYTES, and all hex values are left-justified zero padded.
// 
// A few macros and a function help make this readable:
//   - flush the local buffer to the output
//   - ensure that there is some room in the local buffer
//   - add a single byte to the local buffer
//   - convert a nibble to its ascii hex character
//   - convert an int16u to a decimal string
// Most of these only work within the emPrintfInternal() function.

// Current champion is %4x which writes 8 bytes.  (%s and %p can write
// more, but they do their own overflow checks).
#define LOCAL_BUFFER_SIZE 16
#define MAX_SINGLE_COMMAND_BYTES 8

#define flushBuffer() \
do { count = localBufferPointer - localBuffer;     \
     if (flushHandler(flushVar, localBuffer, count) != EMBER_SUCCESS) \
       goto fail;                                  \
     total += count;                               \
     localBufferPointer = localBuffer;             \
} while (FALSE)                                           

#define addByte(byte) \
do { *(localBufferPointer++) = (byte); } while (FALSE)

static int8u *printfWriteHex(int8u *charBuffer, int16u value, int8u charCount)
{
  int8u c = charCount;
  charBuffer += charCount;
  for (; c; c--) {
    int8u n = value & 0x0F;
    value = value >> 4;
    *(--charBuffer) = n + (n < 10
                           ? '0'
                           : 'A' - 10);
  }
  return charBuffer + charCount;
}

// Returns number of characters written
int8u emPrintfInternal(emPrintfFlushHandler flushHandler, 
                       int8u flushVar, 
                       PGM_P string, 
                       va_list args)
{
  int8u localBuffer[LOCAL_BUFFER_SIZE + MAX_SINGLE_COMMAND_BYTES];
  int8u *localBufferPointer = localBuffer;
  int8u *localBufferLimit = localBuffer + LOCAL_BUFFER_SIZE;
  int8u count;
  int8u total = 0;

  for (; *string; string++) {
    int8u next = *string;
    if (next != '%')
      addByte(next);
    else {
      string += 1;
      switch (*string) {
      case '%':
        // escape for printing "%"
        addByte('%');
        break;
      case 'c':
        // character
        addByte(va_arg(args, unsigned int) & 0xFF);
        break;
      case 's': {
        // string
        int8u len;
        int8u *arg = va_arg(args, int8u *);
        flushBuffer();
        for (len=0; arg[len] != '\0'; len++)
          ;
        if (flushHandler(flushVar, arg, len) != EMBER_SUCCESS)
          goto fail;
        total += len;
        break; }
      case 'p': {
        // flash string
        PGM_P arg = va_arg(args, PGM_P);
        while (TRUE) {
          int8u ch = *arg++;
          if (ch == '\0')
            break;
          *(localBufferPointer++) = ch;
          if (localBufferLimit <= localBufferPointer)
            flushBuffer();
        }
        break; }
      case 'u':         // unsigned 2-byte
      case 'd': {       // signed 2-byte
        int16u value = va_arg(args, int);
        if (value == 0)
          addByte('0');
        else {
          int8u i = 0;
          int8u numBuff[5]; // max int16u is 5 characters (65535)
          if (*string == 'd'
              && (value & 0x8000)) {
            addByte('-');
            if (value != 0x8000)
              value = (int16u) (- ((int16s) value));
          }
          while (value) {
            numBuff[i++] = value % 10;
            value /= 10;
          }
          while (i--)
            addByte(numBuff[i] + '0');
        }
        break;
      }
      case 'x':
      case 'X': {
        // single hex byte (always prints 2 chars, ex: 0A)
        int8u data = va_arg(args, int);
       
        localBufferPointer = printfWriteHex(localBufferPointer, data, 2);
        break; }
      case '2':
        // %2x only, 2 hex bytes (always prints 4 chars)
      case '4':
        // %4x only, 4 hex bytes (always prints 8 chars)
        string += 1;
        if (*string != 'x' && *string != 'X') {
          string -= 1;
        } else if (*(string - 1) == '2') {
          int16u data = va_arg(args, int);
          localBufferPointer = printfWriteHex(localBufferPointer, data, 4);
        } else {
          int32u data = va_arg(args, int32u);
          // On the AVR at least, the code size is smaller if we limit the
          // printfWriteHex() code to 16-bit numbers and call it twice in
          // this case.  Other processors may have a different tradeoff.
          localBufferPointer = printfWriteHex(localBufferPointer, 
                                              (int16u) (data >> 16), 
                                              4);
          localBufferPointer = printfWriteHex(localBufferPointer, 
                                              (int16u) data, 
                                              4);
        }
        break;
      case '\0':
        goto done;
      }
    }
    if (localBufferLimit <= localBufferPointer)
      flushBuffer();
  }
  
 done:
  #pragma warn -146  // Silence warning about unnecessary assignment.
  flushBuffer();
  #pragma warn +146
  return total;

 fail:
  return 0;
}

//------------------------------------------------------
// Buffered Serial utility APIs

#ifdef EM_ENABLE_SERIAL_BUFFER
// always executed in interrupt context
void emSerialBufferNextMessageIsr(EmSerialBufferQueue *q) 
{
  EmSerialBufferQueueEntry *e = &q->fifo[q->tail];

  q->currentBuffer = e->buffer;
  q->nextByte = emberLinkedBufferContents(q->currentBuffer) + e->startIndex;
  if((e->length + e->startIndex) > PACKET_BUFFER_SIZE) {
    q->lastByte = q->nextByte + ((PACKET_BUFFER_SIZE-1) - e->startIndex);
    e->length -= PACKET_BUFFER_SIZE - e->startIndex;
  } else {
    q->lastByte = q->nextByte + e->length - 1;
    e->length = 0;
  }
}
#endif

#ifdef EM_ENABLE_SERIAL_BUFFER
// always executed in interrupt context
void emSerialBufferNextBlockIsr(EmSerialBufferQueue *q, int8u port)
{
  EmSerialBufferQueueEntry *e = &q->fifo[q->tail];
  
  if(e->length != 0) {
    q->currentBuffer = emberStackBufferLink(q->currentBuffer);
    q->nextByte = emberLinkedBufferContents(q->currentBuffer);
    if(e->length > PACKET_BUFFER_SIZE) {
      q->lastByte = q->nextByte + 31;
      e->length -= PACKET_BUFFER_SIZE;
    } else {
      q->lastByte = q->nextByte + e->length - 1;
      e->length = 0;
    }
  } else {
    q->tail = ((q->tail+1) & emSerialTxQueueMasks[port]);
    q->dead++;
    q->used--;
    if(q->used)
      emSerialBufferNextMessageIsr(q);
    else
      q->nextByte = NULL;
  }
}
#endif

//------------------------------------------------------
// Serial initialization

EmberStatus emberSerialInit(int8u port, 
                            SerialBaudRate rate,
                            SerialParity parity,
                            int8u stopBits)
{
  EmSerialFifoQueue *rq;

#if EMBER_SERIAL0_MODE == EMBER_SERIAL_UNUSED
  if (port == 0) return EMBER_SERIAL_INVALID_PORT;
#endif
#if EMBER_SERIAL1_MODE == EMBER_SERIAL_UNUSED
  if (port == 1) return EMBER_SERIAL_INVALID_PORT;
#endif
#if EMBER_SERIAL2_MODE == EMBER_SERIAL_UNUSED
  if (port == 2) return EMBER_SERIAL_INVALID_PORT;
#endif
  if (port >= EM_NUM_SERIAL_PORTS) return EMBER_SERIAL_INVALID_PORT;

  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO: {
    EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
    ATOMIC_LITE(
      q->used = 0;
      q->head = 0;
      q->tail = 0;
    )
    break; }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER: {
    EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[port];
    ATOMIC_LITE(
      q->used = 0;
      q->head = 0;
      q->tail = 0;
      q->dead = 0;
      q->currentBuffer = EMBER_NULL_MESSAGE_BUFFER;
      q->nextByte = NULL;
      q->lastByte = NULL;
    )
    break; }
#endif
  default:
    return EMBER_SERIAL_INVALID_PORT;
    //break;  //statement is unreachable
  }

  rq = emSerialRxQueues[port];
  ATOMIC_LITE(
    rq->used = 0;
    rq->head = 0;
    rq->tail = 0;
    emSerialRxError[port] = EMBER_SUCCESS;
  )

  return halInternalUartInit(port, rate, parity, stopBits);
}

//------------------------------------------------------
// Serial Input

// returns # bytes available for reading
int8u emberSerialReadAvailable(int8u port)  
{
  halInternalUartRxPump(port);
  return emSerialRxQueues[port]->used;
}

EmberStatus emberSerialReadByte(int8u port, int8u *dataByte)
{
  int8u retval;
  EmSerialFifoQueue *q = emSerialRxQueues[port];

  if(emSerialPortModes[port] == EMBER_SERIAL_UNUSED) {
    return EMBER_ERR_FATAL;
  }

  if(emSerialRxError[port] != EMBER_SUCCESS) {
    if(emSerialRxErrorIndex[port] == q->tail) {
      ATOMIC_LITE(
        retval = emSerialRxError[port];
        emSerialRxError[port] = EMBER_SUCCESS;
      )
      return retval;
    }
  }
  
  halInternalUartRxPump(port);
  halInternalUartFlowControl(port);
  
  if(q->used > 0) {
    ATOMIC_LITE(
      *dataByte = FIFO_DEQUEUE(q,emSerialRxQueueMasks[port]);
    )
    if(emSerialRxError[port] != EMBER_SUCCESS) {
      //This index is used when there is an error when the FIFO is full.
      if(emSerialRxErrorIndex[port] == 0xFF) {
        //q->tail has advanced by one, we can now mark the head as the error
        emSerialRxErrorIndex[port] = q->head;
      }
    }
    return EMBER_SUCCESS;
  } else {
    return EMBER_SERIAL_RX_EMPTY;
  }
}

EmberStatus emberSerialReadPartialLine(int8u port, char *data, int8u max, int8u * index)
{
  EmberStatus err;
  int8u ch;

  if (((*index) == 0) || ((*index) >= max))
    data[0] = '\0';

  for (;;) {   
    err = emberSerialReadByte(port, &ch);

    // no new serial port char?, keep looping
    if (err) return err;

    // handle bogus characters
    if ( ch > 0x7F ) continue;

    // handle leading newline - fogBUGZ # 584
    if (((*index) == 0) &&
        ((ch == '\n') || (ch == 0))) continue;

    // Drop the CR, or NULL that is part of EOL sequence.
    if ((*index) >= max) {
      *index = 0;
      if ((ch == '\r') || (ch == 0)) continue;
    }

    // handle backspace
    if ( ch == 0x8 ) {
      if ( (*index) > 0 ) {
        // delete the last character from our string
        (*index)--;
        data[*index] = '\0';
        // echo backspace
        emberSerialWriteString(port, "\010 \010");
      }
      // don't add or process this character
      continue;
    }

    //if the string is about to overflow, fake in a CR
    if ( (*index) + 2 > max ) {
      ch = '\r';
    }

    emberSerialWriteByte(port, ch); // term char echo

    //upcase that char
    if ( ch>='a' && ch<='z') ch = ch - ('a'-'A');

    // build a string until we press enter
    if ( ( ch == '\r' ) || ( ch == '\n' ) ) {
      data[*index] = '\0';

      if (ch == '\r') {
        emberSerialWriteByte(port, '\n'); // "append" LF
        *index = 0;                       // Reset for next line; \n next
      } else {
        emberSerialWriteByte(port, '\r'); // "append" CR
        *index = max;                     // Reset for next line; \r,\0 next
      }

      return EMBER_SUCCESS;
    } 
      
    data[(*index)++] = ch;
  }
}

EmberStatus emberSerialReadLine(int8u port, char *data, int8u max)
{
  int8u index=0;

  while(emberSerialReadPartialLine(port, data, max, &index) != EMBER_SUCCESS)
    halResetWatchdog();
  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Serial Output

// returns # bytes (if fifo mode)/messages (if buffer mode) that can be written
int8u emberSerialWriteAvailable(int8u port)  
{
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO:
    return emSerialTxQueueSizes[port] - 
      ((EmSerialFifoQueue*)emSerialTxQueues[port])->used;
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER: {
    EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[port];
    int8u elementsUsed;
    int8u elementsDead; 
    ATOMIC_LITE( // To clarify the volatile access.
           elementsUsed = q->used;
           elementsDead = q->dead;
           )
    return emSerialTxQueueSizes[port] - (elementsUsed + elementsDead);
    }
#endif
  }
  return 0;
}

EmberStatus emberSerialWriteByte(int8u port, int8u dataByte)
{
  return emberSerialWriteData(port, &dataByte, 1);
}

#ifdef EM_ENABLE_SERIAL_FIFO
static boolean getOutputFifoSpace(EmSerialFifoQueue *q,
                                  int8u port,
                                  int8u extraByteCount)
{
  #ifdef EM_ENABLE_SERIAL_BLOCKING
  if (emSerialBlocking[port]) {
    while(q->used >= emSerialTxQueueSizes[port] - extraByteCount)
      simulatedSerialTimePasses();
  }
  #endif
    return (q->used < emSerialTxQueueSizes[port] - extraByteCount);
}
#endif

EmberStatus emberSerialWriteHex(int8u port, int8u dataByte)
{
  int8u hex[2];
  printfWriteHex(hex, dataByte, 2);
  return emberSerialWriteData(port, hex, 2);
}

EmberStatus emberSerialWriteString(int8u port, PGM_P string)
{
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO:
    {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
      while(*string != '\0') {

        if (! getOutputFifoSpace(q, port, 0))
          return EMBER_SERIAL_TX_OVERFLOW;
        ATOMIC_LITE(
          FIFO_ENQUEUE(q,*string,emSerialTxQueueMasks[port]);
        )
        string++;
      }
      // make sure the interrupt is enabled so it will be sent
      halInternalStartUartTx(port);
      return EMBER_SUCCESS;
    }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER:
    {
      EmberMessageBuffer buff = emberAllocateStackBuffer();
      if(buff != EMBER_NULL_MESSAGE_BUFFER) {
        EmberStatus stat;
        if((stat=emberAppendPgmStringToLinkedBuffers(buff, string)) 
           == EMBER_SUCCESS) {
          stat = emberSerialWriteBuffer(port, buff, 0, emberMessageBufferLength(buff));
        }
        // Refcounts may be manipulated in ISR if DMA used
        ATOMIC( emberReleaseMessageBuffer(buff); )
        return stat;
      }
      return EMBER_NO_BUFFERS;
    }
#endif
  default:
    return EMBER_ERR_FATAL;
  }
}

EmberStatus emberSerialPrintCarriageReturn(int8u port)
{
  return emberSerialPrintf(port, "\r\n");
}

EmberStatus emberSerialPrintfVarArg(int8u port, PGM_P formatString, va_list ap)
{
   EmberStatus stat = EMBER_SUCCESS;

   switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
   case EMBER_SERIAL_FIFO:
    {
      if(!emPrintfInternal(emberSerialWriteData, port, formatString, ap))
        stat = EMBER_ERR_FATAL;
      break;
    }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER:
    {
      EmberMessageBuffer buff = emberAllocateStackBuffer();
      if(buff == EMBER_NULL_MESSAGE_BUFFER) {
        stat = EMBER_NO_BUFFERS;
        break;
      }
      if(emPrintfInternal(emberAppendToLinkedBuffers, 
                          buff, 
                          formatString, 
                          ap)) {
        stat = emberSerialWriteBuffer(port,buff,0,emberMessageBufferLength(buff));
      } else {
        stat = EMBER_NO_BUFFERS;
      }
        // Refcounts may be manipulated in ISR if DMA used
      ATOMIC( emberReleaseMessageBuffer(buff); )

      break;
    }
#endif
  }     
  return stat;
}

EmberStatus emberSerialPrintf(int8u port, PGM_P formatString, ...)
{
  EmberStatus stat;
  va_list ap;
  #pragma warn -146  // Remove when bug 9646 is resolved.
  va_start (ap, formatString);
  stat = emberSerialPrintfVarArg(port, formatString, ap);
  va_end (ap);
  #pragma warn +146
  return stat;
}

EmberStatus emberSerialPrintfLine(int8u port, PGM_P formatString, ...)
{
  EmberStatus stat;
  va_list ap;
  #pragma warn -146  // Remove when bug 9646 is resolved.
  va_start (ap, formatString);
  stat = emberSerialPrintfVarArg(port, formatString, ap);
  va_end (ap);
  #pragma warn +146
  emberSerialPrintCarriageReturn(port);
  return stat;
}

EmberStatus emberSerialWriteData(int8u port, int8u *data, int8u length)
{
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO:
    {
      EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
 
      while(length--) {
        if (! getOutputFifoSpace(q, port, 0))
          return EMBER_SERIAL_TX_OVERFLOW;
        ATOMIC_LITE(
          FIFO_ENQUEUE(q,*data,emSerialTxQueueMasks[port]);
        )
        data++;
      }
      // make sure the interrupt is enabled so it will be sent
      halInternalStartUartTx(port);
      return EMBER_SUCCESS;
    }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER:
    {
      // Note: We must always copy this, even in buffer mode
      //  since it is ram based data and there are no reference counts
      //  or indication of when it is actually written out the serial
      //  we cannot trust that the data won't be changed after this call
      //  but before it was actually written out.
      EmberMessageBuffer buff = emberFillLinkedBuffers(data,length);
      if(buff != EMBER_NULL_MESSAGE_BUFFER) {
        EmberStatus stat = emberSerialWriteBuffer(port, buff, 0, emberMessageBufferLength(buff));
        // Refcounts may be manipulated in ISR if DMA used
        ATOMIC( emberReleaseMessageBuffer(buff); )
        return stat;
      } else 
        return EMBER_NO_BUFFERS;
    }
#endif
  default:
    return EMBER_ERR_FATAL;
  }
}

EmberStatus emberSerialWriteBuffer(int8u port, 
                                   EmberMessageBuffer buffer, 
                                   int8u start, 
                                   int8u length)
{
//Host processors do not use Ember Message Buffers.
#ifdef EZSP_HOST
  return EMBER_ERR_FATAL;  //This function is invalid.
#else //EZSP_HOST

  if(buffer == EMBER_NULL_MESSAGE_BUFFER)
    return EMBER_ERR_FATAL;
  if(length == 0)
    return EMBER_SUCCESS;

  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO:
    {
      for(;PACKET_BUFFER_SIZE <= start; start-=PACKET_BUFFER_SIZE)
        buffer = emberStackBufferLink(buffer);
      
      while (0 < length) {
        int8u remainingInBuffer = PACKET_BUFFER_SIZE - start;
        int8u bytes = (length < remainingInBuffer
                       ? length
                       : remainingInBuffer);
        emberSerialWriteData(port,
                             emberMessageBufferContents(buffer) + start,
                             bytes);
        length -= bytes;
        start = 0;
        buffer = emberStackBufferLink(buffer);
      }
      // make sure the interrupt is enabled so it will be sent
      halInternalStartUartTx(port);
      break;
    }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER:
    {
      EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[port];
      EmSerialBufferQueueEntry *e;
      int8u elementsUsed;
      int8u elementsDead;

      ATOMIC_LITE( // To clarify volatile access.
             elementsUsed = q->used;
             elementsDead = q->dead;
             )
      
      #ifdef EM_ENABLE_SERIAL_BLOCKING 
      if(emSerialBlocking[port]) {
        while((elementsUsed + elementsDead) >= emSerialTxQueueSizes[port]) {
          emberSerialBufferTick();
          //re-read the element counters after clocking the serial buffers
          ATOMIC_LITE( // To clarify volatile access.
                elementsUsed = q->used;
                elementsDead = q->dead;
                )
        }
      } else
      #endif
      if((elementsUsed + elementsDead) >= emSerialTxQueueSizes[port]) {
        if(elementsDead)
          emberSerialBufferTick();
        else
          return EMBER_SERIAL_TX_OVERFLOW;
      }

      for(;PACKET_BUFFER_SIZE <= start; start-=PACKET_BUFFER_SIZE)
        buffer = emberStackBufferLink(buffer);
      emberHoldMessageBuffer(buffer);
      
      e = &q->fifo[q->head];
      e->length = length;
      e->buffer = buffer;
      e->startIndex = start;
      q->head = ((q->head+1) & emSerialTxQueueMasks[port]);
      ATOMIC_LITE(
        q->used++;
      )
      halInternalStartUartTx(port);  
      break; 
    }
#endif
  default:
    return EMBER_ERR_FATAL;
  }
  return EMBER_SUCCESS;
#endif //EZSP_HOST
}

EmberStatus emberSerialWaitSend(int8u port)  // waits for all byte to be written out of a port
{
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO: {
    EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
    while(q->used)
      simulatedSerialTimePasses();
    break; }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER: {
    EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[port];
    while(q->used)
      simulatedSerialTimePasses();
    break; }
#endif
  }
  halInternalWaitUartTxComplete(port);
  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Guaranteed output

// The _only_ Guaranteed API:  The usage model for this api 
//   Does not require efficiency
EmberStatus emberSerialGuaranteedPrintf(int8u port, PGM_P formatString, ...)
{
  va_list ap;
  
  // prevent interrupt driven transmission from intermixing
  halInternalStopUartTx(port);
  #pragma warn -146  // Remove when bug 9646 is resolved.
  va_start(ap, formatString);
  emPrintfInternal(halInternalForceWriteUartData, port, formatString, ap);
  va_end(ap);
  #pragma warn +146

  // re-enable interrupt driven transmission if needed
  switch(emSerialPortModes[port]) {
#ifdef EM_ENABLE_SERIAL_FIFO
  case EMBER_SERIAL_FIFO: {
    EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialTxQueues[port];
    if(q->used)
      halInternalStartUartTx(port);
    break; }
#endif
#ifdef EM_ENABLE_SERIAL_BUFFER
  case EMBER_SERIAL_BUFFER: {
    EmSerialBufferQueue *q = (EmSerialBufferQueue *)emSerialTxQueues[port];
    if(q->used)
      halInternalStartUartTx(port);
    break; }
#endif
  }

  return EMBER_SUCCESS;
}

//------------------------------------------------------
// Serial buffer maintenance
void emberSerialFlushRx(int8u port) 
{
  EmSerialFifoQueue *q = (EmSerialFifoQueue *)emSerialRxQueues[port];
  ATOMIC_LITE(
     q->used = 0;
     q->head = 0;
     q->tail = 0;
     )
}

//------------------------------------------------------
// Serial Buffer Cleanup Tick

void emberSerialBufferTick(void)
{
#ifdef EM_ENABLE_SERIAL_BUFFER
  int8u port;
  EmSerialBufferQueue *q;
  int8u numDead, deadIndex;

  for(port=0; port<EM_NUM_SERIAL_PORTS; port++) {
    if(emSerialPortModes[port] == EMBER_SERIAL_BUFFER) {
      q = (EmSerialBufferQueue *)emSerialTxQueues[port];

      if(q->dead) {
        ATOMIC_LITE(
          numDead = q->dead;
          q->dead = 0;
          deadIndex = (q->tail - numDead) & emSerialTxQueueMasks[port];
        )
        for(;numDead;numDead--) {
          // Refcounts may be manipulated in ISR if DMA used
          ATOMIC( emberReleaseMessageBuffer(q->fifo[deadIndex].buffer); )
          deadIndex = (deadIndex + 1) & emSerialTxQueueMasks[port];
        }
      }
    }
  }
#endif
}
