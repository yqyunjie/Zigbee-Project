/** @file serial/serial.h
 * See @ref serial_comm for documentation.
 * 
 * @brief  High-level serial communication functions
 *  
 * 
 * <!--Culprit(s):  Lee Taylor (lee@ember.com)-->
 * <!-- Copyright 2004 by Ember Corporation. All rights reserved.-->   
 */


#ifndef __SERIAL_H__
#define __SERIAL_H__

#ifndef __HAL_H__
  #error hal/hal.h should be included first
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#include <stdarg.h>

//Rx FIFO Full indicator
#define RX_FIFO_FULL (0xFFFF)

#endif // DOXYGEN_SHOULD_SKIP_THIS

 /** @addtogroup serial_comm
 * Unless otherwise noted, the EmberNet stack does not use these functions, 
 * and therefore the HAL is not required to implement them. However, many of
 * the supplied example applications do use them.  On some platforms, they are
 * also required by DEBUG builds of the stack
 * 
 * Many of these functions return an ::EmberStatus value. See 
 * stack/include/error-defs.h for definitions of all ::EmberStatus return values.
 * See app/util/serial/serial.h for source code.
 *  To use these serial routines, they must be properly configured. 
 * 
 * If the Ember serial library is built using EMBER_SERIAL_USE_STDIO,
 * then the Ember serial code will redirect to stdio.h.  EMBER_SERIAL_USE_STDIO
 * will not consume any of the usual Ember serial library buffers and does
 * not require use of any of the other EMBER_SERIALx definitions described
 * here. In this mode, the only required lower layers are:
 * - putchar()
 * - getchar()
 * - fflush(stdout)
 * - halInternalUartInit()
 * - halInternalPrintfWriteAvailable()
 * - halInternalPrintfReadAvailable()
 * - halInternalForcePrintf()
 *
 *  The functions can work in two ways, depending on how messages waiting for 
 *  transmission are stored:
 *  - Buffered mode: Uses stack linked buffers. This method can be more
 *  efficient if many messages received over the air also need to be
 *  transmitted over the serial interface.
 *  - FIFO mode: Uses a statically allocated queue of bytes, and data
 *  to be transmitted is copied into the queue.
 * 
 * (These modes deal only with data transmission. Data \b reception 
 *  always occurs in a FIFO mode.)
 * 
 *  The current version of these sources provides support for as many as two
 *  serial ports, but it can be easily extended. The ports are numbered 0
 *  and 1 and should be accessed using those numbers. The ports can be set up 
 *  independently of each other.
 * 
 *  To enable a port, a Use mode (buffered or FIFO) and a Queue Size must be 
 *  declared on the port. In FIFO mode, the Queue Size is the size of the FIFO and 
 *  represents the number of bytes that can be waiting for transmission at any 
 *  given time. In buffered mode, the Queue Size represents the number of whole
 *  messages that can be waiting for transmission at any given time. A single 
 *  message is created for each call to any of the serial APIs.
 *  
 *  To specify a Use mode and Queue Size, place declarations in the compiler 
 *  preprocessor options when building your application:
 *  - <b>Use Mode:</b>
 *    -  ::EMBER_SERIAL0_MODE=::EMBER_SERIAL_BUFFER or ::EMBER_SERIAL_FIFO
 *    -  ::EMBER_SERIAL1_MODE=::EMBER_SERIAL_BUFFER or ::EMBER_SERIAL_FIFO
 *  - <b>Queue Size:</b>
 *    -  ::EMBER_SERIAL0_TX_QUEUE_SIZE=2
 *    -  ::EMBER_SERIAL0_RX_QUEUE_SIZE=4
 *    -  ::EMBER_SERIAL1_TX_QUEUE_SIZE=8
 *    -  ::EMBER_SERIAL1_RX_QUEUE_SIZE=16
 *
 *  Note the following: 
 *  - If buffered mode is declared, emberSerialBufferTick() should be 
 *  called in the application's main event loop.
 *  - If buffered mode is declared, the Tx queue size \b MUST be <= 255
 *  - On the AVR platform, Rx & Tx queue sizes are limited to powers of 2 <= 128
 *  - By default, both ports are unused.
 *
 *  You can also use declarations to specify what should be done if an attempt
 *  is made to send more data than the queue can accommodate:
 *  - ::EMBER_SERIAL0_BLOCKING
 *  - ::EMBER_SERIAL1_BLOCKING
 *
 *  Be aware that since blocking spins in a loop, doing nothing until space 
 *  is available, it can adversely affect any code that has tight timing 
 *  requirements.
 *
 *  If ::EMBER_SERIAL0_BLOCKING or ::EMBER_SERIAL1_BLOCKING is defined,
 *  then the call to the port will block until space
 *  is available, guaranteeing that the entire message is sent. Note that 
 *  in buffered mode, even if blocking mode is in effect entire messages
 *  may be dropped if insufficient stack buffers are available to hold them.
 *  When this happens, ::EMBER_NO_BUFFERS is returned.
 *  
 *  If no blocking mode is defined, the serial code defaults to non-blocking
 *  mode. In this event, when the queue is too short, the data that don't fit 
 *  are dropped. In FIFO mode, this may result bytes being dropped,
 *  starting in the middle of message. In buffered mode, the entire
 *  message is dropped. When data is dropped, ::EMBER_SERIALTX_OVERFLOW 
 *  is returned. 
 *
 *  To minimize code size, very little error checking is done on
 *  the given parameters. Specifying an invalid or unused serial port may
 *  result in unexplained behavior. In some cases ::EMBER_ERR_FATAL may be 
 *  returned.
 *@{
 */

/** @brief Initializes a serial port to a specific baud rate, parity,
 * and number of stop bits. Eight data bits are always used.
 *   
 * @param port   A serial port number (0 or 1).
 *  
 * @param rate   The baud rate (see SerialBaudRate).
 *   
 * @param parity   The parity value (see SerialParity).
 *   
 * @param stopBits  The number of stop bits.
 *   
 * @return An error code if initialization failed (such as invalid baudrate), 
 * or ::EMBER_SUCCESS.
 */
EmberStatus emberSerialInit(int8u port, 
                            SerialBaudRate rate,
                            SerialParity parity,
                            int8u stopBits);

/** @brief Returns the number of bytes currently available for reading
 * in the specified RX queue.
 *  
 * @param port  A serial port number (0 or 1).
 *   
 * @return The number of bytes available.
 */
int16u emberSerialReadAvailable(int8u port);

/** @brief Reads a byte from the specified RX queue.  If an error
 *  is returned, the dataByte should be ignored.  For errors other than
 *  ::EMBER_SERIAL_RX_EMPTY multiple bytes of data may have been lost and
 *  serial protocols should attempt to resynchronize
 * 
 * @param port  A serial port number (0 or 1).
 *   
 * @param dataByte  A pointer to storage location for the byte.
 *  
 * @return One of the following (see the Main Page):
 *  - ::EMBER_SERIAL_RX_EMPTY if no data is available
 *  - ::EMBER_SERIAL_RX_OVERFLOW if the serial receive fifo was out of space
 *  - ::EMBER_SERIAL_RX_FRAME_ERROR if a framing error was received
 *  - ::EMBER_SERIAL_RX_PARITY_ERROR if a parity error was received
 *  - ::EMBER_SERIAL_RX_OVERRUN_ERROR if the hardware fifo was out of space
 *  - ::EMBER_SUCCESS if a data byte is returned
 */
EmberStatus emberSerialReadByte(int8u port, int8u *dataByte);

/** @brief Reads bytes from the specified RX queue.  Blocks until the full
 * length has been read or an error occurs.  In the event of an error, some
 * valid data may have already been read before the error occurred, in which
 * case that data will be in the buffer pointed to by \c data and the number of
 * bytes successfully read will be placed in \c bytesRead.
 * 
 * @param port  A serial port number (0 or 1).
 * 
 * @param data  A pointer to storage location for the data.  It must be at least
 * \c length in size.
 *
 * @param length  The number of bytes to read.
 *
 * @param bytesRead  A pointer to a location that will receive the number of
 * bytes read.  If the function returns early due to an error, this value may be
 * less than \c length.  This parameter may be NULL, in which case it is ignored.
 *
 * @return One of the following (see the Main Page):
 *  - ::EMBER_SERIAL_RX_OVERFLOW if the serial receive fifo was out of space
 *  - ::EMBER_SERIAL_RX_FRAME_ERROR if a framing error was received
 *  - ::EMBER_SERIAL_RX_PARITY_ERROR if a parity error was received
 *  - ::EMBER_SERIAL_RX_OVERRUN_ERROR if the hardware fifo was out of space
 *  - ::EMBER_SUCCESS if all the data requested is returned
 */
EmberStatus emberSerialReadData(int8u port,
                                int8u *data,
                                int16u length,
                                int16u *bytesRead);

/** @brief Reads bytes from the specified RX queue, up to a maximum of \c length
 * bytes.  The function may return before \c length bytes is read if a timeout
 * is reached or an error occurs.  Returns ::EMBER_SERIAL_RX_EMPTY if a timeout
 * occurs.
 * 
 * @param port  A serial port number (0 or 1).
 * 
 * @param data  A pointer to storage location for the data.  It must be at least
 * \c length in size.
 *
 * @param length  The maximum number of bytes to read.
 *
 * @param bytesRead  A pointer to a location that will receive the number of
 * bytes read.  If the function returns early due to an error or timeout, this
 * value may be less than \c length.  This parameter may be NULL, in which case
 * it is ignored.
 *
 * @param firstByteTimeout  The amount of time, in milliseconds, to wait
 * for the first byte to arrive (if the queue is empty when the function is
 * called).  This value must be a minimum of 2 due to the timer resolution.
 *
 * @param subsequentByteTimeout  The amount of time, in milliseconds, to
 * wait after the previous byte was received for the next byte to
 * arrive.  This value must be a minimum of 2 due to the timer resolution.
 *
 * @return One of the following (see the Main Page):
 *  - ::EMBER_SERIAL_RX_EMPTY if the timeout was exceeded before the requested
 *  amount of data was read
 *  - ::EMBER_SERIAL_RX_OVERFLOW if the serial receive fifo was out of space
 *  - ::EMBER_SERIAL_RX_FRAME_ERROR if a framing error was received
 *  - ::EMBER_SERIAL_RX_PARITY_ERROR if a parity error was received
 *  - ::EMBER_SERIAL_RX_OVERRUN_ERROR if the hardware fifo was out of space
 *  - ::EMBER_SUCCESS if all the data requested is returned
 */
EmberStatus emberSerialReadDataTimeout(int8u port,
                                       int8u *data,
                                       int16u length,
                                       int16u *bytesRead,
                                       int16u firstByteTimeout,
                                       int16u subsequentByteTimeout);

/** @brief Simulates a terminal interface, reading a line of characters
 * at a time. Supports backspace. Always converts to uppercase.
 * Blocks until a line has been read or max has been exceeded.
 * Calls on halResetWatchdog().
 *  
 * @param port  A serial port number (0 or 1).
 *   
 * @param data  A pointer to storage location for the read line. There must be
 * \c max contiguous bytes available at this location.
 *  
 * @param max  The maximum number of bytes to read.
 *  
 * @return  ::EMBER_SUCCESS
 */
EmberStatus emberSerialReadLine(int8u port, char *data, int8u max);

/** @brief Simulates a partial terminal interface, reading a line of
 * characters at a time. Supports backspace. Always converts to uppercase.
 * returns ::EMBER_SUCCESS when a line has been read or max has been exceeded.
 * Must initialize the index variable to 0 to start a line.
 *  
 * @param port  A serial port number (0 or 1).
 *   
 * @param data  A pointer to storage location for the read line. There must be
 * \c max contiguous bytes available at this location.
 *  
 * @param max  The maximum number of bytes to read.
 *
 * @param index  The address of a variable that holds the place in
 * the \c data to continue.  Set to 0 to start a line read.
 *
 * @return One of the following (see the Main Page):
 *  - ::EMBER_SERIAL_RX_EMPTY if a partial line is in progress.
 *  - ::EMBER_SERIAL_RX_OVERFLOW if the serial receive fifo was out of space.
 *  - ::EMBER_SERIAL_RX_FRAME_ERROR if a framing error was received.
 *  - ::EMBER_SERIAL_RX_PARITY_ERROR if a parity error was received.
 *  - ::EMBER_SERIAL_RX_OVERRUN_ERROR if the hardware fifo was out of space.
 *  - ::EMBER_SUCCESS if a full ine is ready.
 */
EmberStatus emberSerialReadPartialLine(int8u port, char *data, int8u max, int8u *index);

/** @brief Returns the number of bytes (in FIFO mode) or messages 
 * (in buffered mode) that can currently be queued to send without blocking
 * or dropping.
 * 
 * @param port  A serial port number (0 or 1).
 *   
 * @return The number of bytes or messages available for queueing. 
 */
int16u emberSerialWriteAvailable(int8u port);

/** @brief Returns the number of bytes (in FIFO mode) or messages 
 * (in buffered mode) that are currently queued and still being sent.
 * 
 * @param port  A serial port number (0 or 1).
 *   
 * @return The number of bytes or messages available for queueing. 
 */
#define emberSerialWriteUsed(port)  \
  (emSerialTxQueueSizes[port] - emberSerialWriteAvailable(port))

/** @brief Queues a single byte of data for transmission on the 
 * specified port.
 *  
 * @param port  A serial port number (0 or 1).
 *   
 * @param dataByte  The byte to be queued.
 *  
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialWriteByte(int8u port, int8u dataByte);

/** @brief Converts a given byte of data to its two-character ASCII
 * hex representation and queues it for transmission on the specified port. 
 * Values less than 0xF are always zero padded and queued as "0F".
 *  
 * @param port  A serial port number (0 or 1).
 *
 * @param dataByte  The byte to be converted.
 *  
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialWriteHex(int8u port, int8u dataByte);

/** @brief Queues a string for transmission on the specified port.
 *  
 * @param port  A serial port number (0 or 1).
 *   
 * @param string  The string to be queued.
 *   
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialWriteString(int8u port, PGM_P string);

/** @brief Printf for printing on a specified port. Supports the 
 * following format specifiers:
 * - %% percent sign
 * - %c single-byte character
 * - %s RAM string
 * - %p flash string (nonstandard specifier)
 * - %u 2-byte unsigned decimal
 * - %d 2-byte signed decimal
 * - %l 4-byte signed decimal
 * - %x %2x %4x 1-, 2-, 4-byte hex value (always 0 padded) (nonstandard
 * specifier)
 *   
 * @param port  A serial port number (0 or 1).
 *   
 * @param formatString  The string to print.
 *  
 * @param ...  Format specifiers. 
 *   
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
XAP2B_PAGEZERO_ON
EmberStatus emberSerialPrintf(int8u port, PGM_P formatString, ...);
XAP2B_PAGEZERO_OFF

/** @brief Printf for printing on a specified port.  Same as
 *  emberSerialPrintf() except it prints a carriage return at the
 *  the end of the text.
 * @param port  A serial port number (0 or 1).
 *   
 * @param formatString  The string to print.
 *  
 * @param ...  Format specifiers. 
 *   
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
XAP2B_PAGEZERO_ON
EmberStatus emberSerialPrintfLine(int8u port, PGM_P formatString, ...);
XAP2B_PAGEZERO_OFF

/** @brief Prints "\r\n" to the specified serial port.
 *
 * @param port  A serial port number (0 or 1).

 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
XAP2B_PAGEZERO_ON
EmberStatus emberSerialPrintCarriageReturn(int8u port);
XAP2B_PAGEZERO_OFF


/** @brief Prints a format string with a variable argument list.
 *
 * @param port  A serial port number (0 or 1).
 * @param formatString A printf style format string.
 * @param ap    A variable argument list.
 *
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialPrintfVarArg(int8u port, PGM_P formatString, va_list ap);

/** @brief Queues an arbitrary chunk of data for transmission on a 
 * specified port.
 *   
 * @param port  A serial port number (0 or 1).
 *   
 * @param data  A pointer to data.
 *   
 * @param length  The number of bytes to queue.
 *    
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialWriteData(int8u port, int8u *data, int8u length);

//Host HALs do not use stack buffers.
#ifndef HAL_HOST
/** @brief Queues data contained in linked stack buffers for 
 * transmission on a specified port. Can specify an arbitrary initial 
 * offset within the linked buffer chain.
 *   
 * @param port  A serial port number (0 or 1).
 *   
 * @param buffer  The starting buffer in linked buffer chain.
 *   
 * @param start  The offset from first buffer in chain.
 *  
 * @param length  The number of bytes to queue.
 *    
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialWriteBuffer(int8u port, EmberMessageBuffer buffer, int8u start, int8u length);
#endif //HAL_HOST

/** @brief Waits for all data currently queued on the specified port to 
 * be transmitted before returning. \b Note: Call this function before serial 
 * reinitialization to ensure that transmission is complete.
 *   
 * @param port  A serial port number (0 or 1).
 * 
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
XAP2B_PAGEZERO_ON
EmberStatus emberSerialWaitSend(int8u port);
XAP2B_PAGEZERO_OFF

/** @brief A printf routine that takes over the specified serial 
 * port and immediately transmits the given data regardless of what is 
 * currently queued. Does not return until the transmission is complete. 
 *  
 * @appusage Useful for fatal situations (such as asserts) where the node will 
 * be reset, but information on the cause for the reset needs to be transmitted 
 * first.
 *   
 * @param port  A serial port number (0 or 1).
 *  
 * @param formatString  The string to print.
 *  
 * @param ...  Formatting specifiers. See emberSerialPrintf() for arguments.
 *   
 * @return One of the following (see the Main Page):
 * - ::EMBER_SERIAL_TX_OVERFLOW indicates that data was dropped. 
 * - ::EMBER_NO_BUFFERS indicates that there was an insufficient number of
 *  available stack buffers.
 * - ::EMBER_SUCCESS.
 */
EmberStatus emberSerialGuaranteedPrintf(int8u port, PGM_P formatString, ...);

/** @brief When a serial port is used in buffered mode, this must be
 * called in an application's main event loop, similar to emberTick(). 
 * It frees buffers that are used to queue messages. \b Note: This function
 * has no effect if FIFO mode is being used.
 */
void emberSerialBufferTick(void);

/** @brief Flushes the receive buffer in case none of the
 * incoming serial data is wanted.
 *
 * @param port  A serial port number (0 or 1).
*/
void emberSerialFlushRx(int8u port);




/** @name Printf Prototypes
 * These prototypes are for the internal printf implementation, in case
 * it is desired to use it elsewhere. See the code for emberSerialPrintf()
 * for an example of printf usage.
 *@{
 */
 
/** @brief Typedefine to cast a function into the appropriate format
 * to be used inside the \c emPrintfInternal function below, for performing the
 * actual flushing of a formatted string to a destination such as a serial port.
 *
 * @param flushVar: The destination of the flush, most commonly a serial port
 * number (0 or 1).
 * 
 * @param contents  A pointer to the string to flush.
 *
 * @param length  The number of bytes to flush.
 *
 * @return The EmberStatus value of the typedefined function.
 */
typedef EmberStatus (emPrintfFlushHandler)(int8u flushVar, 
                                           int8u *contents, 
                                           int8u length);


/** @brief The internal printf function, which scans the string for the
 * format specifiers and appropriately implants the passed data into the string.
 *
 * @param flushHandler: The name of an internal function, which has
 * parameters matching the function \c emPrintfFlushHandler above, responsible
 * for flushing a string formatted by this function, \c emPrintfInternal, to
 * the appropriate buffer or function that performs the actual transmission.
 * 
 * @param port  The destination of the flush performed above, most commonly
 * serial port number (0 or 1).
 *
 * @param string  The string to print.
 *
 * @param args  The list of arguments for the format specifiers.
 *
 * @return The number of characters written.
 */
int8u emPrintfInternal(emPrintfFlushHandler flushHandler,
                       int8u port,
                       PGM_P string,
                       va_list args);


/** @} END Printf Prototypes */

/** @} END addtogroup */

#endif // __SERIAL_H__

