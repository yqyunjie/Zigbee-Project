/** @file ash-host-io.h
 * @brief Header for ASH host I/O functions
 *
 * See @ref ash_util for documentation.
 *
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.-->   
 */

#ifndef __ASH_HOST_IO_H__
#define __ASH_HOST_IO_H__

/** @addtogroup ash_util
 *
 * See ash-host-io.h.
 *
 *@{
 */
 
/** @brief Initializes the serial port for use by ASH. The port number,
 *  baud rate, stop bits, and flow control method are specifed by the
 *  by the ashHostConfig structure.
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_HOST_FATAL_ERROR
 */
EzspStatus ashSerialInit(void);

/** @brief If the serial port is open, discards all I/O data
 *  and closes the port.
 */
void ashSerialClose(void);

/** @brief Resets the ncp by deasserting and asserting DTR.
 *  This requires a conenction between DTR and nRESET, as there is on the 
 *  EM260 breakout board when the on-board USB interface is used.
 */
void ashResetDtr(void);

/** @brief Custom method for resetting the ncp which must be defined by
 *  the user for their specific hardware and interconect. As shipped, this
 *  function does nothing.
 */
void ashResetCustom(void);

/** @brief Checks to see if there is space available in the serial
 *  write buffer. If the buffer is full, it is output to the serial port
 *  and it return a "no space indication".
 *
 * @return  
 * - ::EZSP_SUCCESS
 * _ ::EZSP_ASH_NO_TX_SPACE
 */
EzspStatus ashSerialWriteAvailable(void);

/** @brief Writes a byte to the serial output buffer.
 *
 * @param byte byte to write
 */
void ashSerialWriteByte(int8u byte);

/** @brief Writes all data the write output buffer to the serial port
 *  and calls fsync(). This is called when a complete frame to be sent to
 *  the ncp has been created.
 */
void ashSerialWriteFlush(void);

/** @brief Reads a byte from the serial port, if one is available.
 *
 * @param byte pointer to a variable where the byte read will be output
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_NO_RX_DATA
 */
EzspStatus ashSerialReadByte(int8u *byte);

/** @brief Discards input data from the serial port until there
 *  is none left.
 */
void ashSerialReadFlush(void);

/** @brief Flushes the ASH ASCII trace output stream.
 */
void ashDebugFlush(void);

/** @brief Prints ASH ACSII trace information.
 */
#define DEBUG_STREAM  stdout

#ifdef WIN32
  #define ashDebugPrintf printf
#else
  #define ashDebugPrintf(...) fprintf(DEBUG_STREAM, __VA_ARGS__)
#endif

#define ashDebugVfprintf(format, argPointer) \
          vfprintf(DEBUG_STREAM, format, argPointer)

/** @brief Returns the file descriptor associated with the serial port. 
 */

int ashSerialGetFd(void);

#endif //__ASH_HOST_H___

/** @} // END addtogroup
 */

