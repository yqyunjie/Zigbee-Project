/** @file ash-common.h
 * @brief Header for ASH common functions
 *
 * See @ref ash for documentation.
 *
 *
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.-->   
 */

#ifndef __ASH_COMMON_H__
#define __ASH_COMMON_H__

/** @addtogroup ash
 *
 * Use the Asynchronous Serial Host (ASH) Framework interfaces on a host 
 * microcontroller when it communicates with an Ember chip via EZSP-UART. 
 *
 * See ash-common.h for source code.
 *
 *@{
 */

/** @brief Builds an ASH frame. Byte stuffs the control and data fields
 * as required, computes and appends the CRC and adds the ending flag byte.
 * Called with the next byte to encode, this function may return several 
 * output bytes before it's ready for the next byte.
 *  
 * @param len     new frame flag / length of the frame to be encoded.
 * A non-zero value begins a new frame, so all subsequent calls must use zero.
 * The length includes control byte and data field, but not the flag or crc.
 * This function does not validate the length.
 *
 * @param byte    the next byte of data to add to the frame.
 * Note that in general the same byte of data may have to be passed more than
 * once as escape bytes, the CRC and the end flag byte are output.
 *
 * @param offset  pointer to the offset of the next input byte.
 * (If the frame data is the array data[], the next byte would be data[offset].)
 * Is set to 0 when starting a new frame (ie, len is non-zero).
 * Is set to 0xFF when the last byte of the frame is returned.
 *
 * @return next encoded output byte in frame.
 */
int8u ashEncodeByte(int8u len, int8u byte, int8u *offset);

/** @brief Decodes and validates an ASH frame. Data is passed to it
 * one byte at a time. Decodes byte stuffing, checks crc, finds the end flag
 * and (if enabled) terminates the frame early on CAN or SUB bytes.
 * The number of bytes output will not exceed the max valid frame length,
 * which does not include the flag or the crc.
 *  
 * @param byte   the next byte of data to add to the frame
 *
 * @param out    pointer to where to write an output byte
 *
 * @param outLen number of bytes output so far
 *
 * @return status of frame decoding
 * -     ::EZSP_SUCCESS
 * -     ::EZSP_ASH_IN_PROGRESS
 * -     ::EZSP_ASH_CANCELLED
 * -     ::EZSP_ASH_BAD_CRC
 * -     ::EZSP_ASH_COMM_ERROR
 * -     ::EZSP_ASH_TOO_SHORT
 * -     ::EZSP_ASH_TOO_LONG
*/
EzspStatus ashDecodeByte(int8u byte, int8u *out, int8u *outLen);

/** @brief Randomizes array contents by XORing with an 8-bit pseudo
 * random sequence. This reduces the likelihood that byte-stuffing will 
 * greatly increase the size of the payload. (This could happen if a DATA
 * frame contained repeated instances of the same reserved byte value.)
 *  
 * @param seed  zero initializes the random sequence
 *              a non-zero value continues from a previous invocation
 *
 * @param buf   pointer to the array whose contents will be randomized
 *
 * @param len   number of bytes in the array to modify
 *
 * @return      last value of the sequence. If a buffer is processed in
 *              two or more chunks, as with linked buffers, this value
 *              should be passed back as the value of the seed argument
 *
*/
int8u ashRandomizeArray(int8u seed, int8u *buf, int8u len);

/** @brief Sets ashAckTimer to the specified period and starts it running.
 *  
 *
*/
void ashStartAckTimer(void);

/** @brief Stops and clears ashAckTimer.
 *  
*/
void ashStopAckTimer(void);
#define ashStopAckTimer() do {ashAckTimer = 0;} while (FALSE)

/** @brief Indicates whether or not ashAckTimer is currently running.
 *  The timer may be running even if expired.
 *  
*/
#define ashAckTimerIsRunning() (ashAckTimer != 0)

/** @brief Indicates whether or not ashAckTimer is currently running.
 *  The timer may be running even if expired.
 *  
*/
#define ashAckTimerIsNotRunning() (ashAckTimer == 0)

/** @brief Indicates whether or not ashAckTimer has expired.
 *  If the timer is stopped then it is not expired.
 *  
*/
boolean ashAckTimerHasExpired(void);

/** @brief Adapts the acknowledgement timer period to the 
 *  observed ACK delay.
 *  If the timer is not running, it does nothing.
 *  If the timer has expired, the timeourt period is doubled.
 *  If the timer has not expired, the elapsed time is fed into simple
 *  IIR filter: 
 *          T[n+1] = (7*T[n] + elapsedTime) / 8
 *  The timeout period, ashAckPeriod, is limited such that:
 *  ASH_xxx_TIME_DATA_MIN <= ashAckPeriod <= ASH_xxx_TIME_DATA_MAX,
 *  where xxx is either HOST or NCP.
 *
 *  The acknowledgement timer is always stopped by this function.
 *
 * @param expired TRUE if timer has expired
 *  
*/
void ashAdjustAckPeriod(boolean expired);

/** @brief Sets the acknowledgement timer period (in msec)
 *  and stops the timer.
 *  
*/
#define ashSetAckPeriod(msec)  \
    do {ashAckPeriod = msec; ashAckTimer = 0;} while (FALSE)

/** @brief Returns the acknowledgement timer period (in msec).
 *  
*/
#define ashGetAckPeriod() (ashAckPeriod)

/** @brief Sets the acknowledgement timer period (in msec),
 *  and starts the timer running.
*/
#define ashSetAndStartAckTimer(msec) \
    do {ashSetAckPeriod(msec); ashStartAckTimer();}  while (FALSE)

// Define the units used by the Not Ready timer as 2**n msecs
#define ASH_NR_TIMER_BIT    4 // log2 of msecs per NR timer unit

/** @brief Starts the Not Ready timer
 *
 *  On the host, this times nFlag refreshing when the host doesn't have 
 *  room for callbacks for a prolonged period.
 *
 *  On the NCP, if this times out the NCP resumes sending callbacks.
*/
void ashStartNrTimer(void);

/** @brief Stops the Not Ready timer.
*/
#define ashStopNrTimer()  do {ashNrTimer = 0;} while (FALSE)

/** @brief Tests whether the Not Ready timer has expired 
 *  or has stopped. If expired, it is stopped.
 *
 * @return  TRUE if the Not Ready timer has expired or stopped 
*/
boolean ashNrTimerHasExpired(void);

extern boolean ashDecodeInProgress; // set FALSE to start decoding a new frame

// ASH timers (units)
extern int16u ashAckTimer;        // rec'd ack timer (msecs)
extern int16u ashAckPeriod;       // rec'd ack timer period (msecs)
extern int8u ashNrTimer;          // not ready timer (16 msec units)

#endif //__ASH_COMMON_H__

/** @} // END addtogroup
 */


