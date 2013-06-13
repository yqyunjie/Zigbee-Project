/**
 * @file client-api.h
 * @brief API for generating command buffer.
 */

#ifndef __CLIENT_API__
#define __CLIENT_API__

/** @name Client API functions */
// @{

/** 
 * @brief Function that fills in the buffer with command.
 *
 * Fills the outgoing zcl buffer and returns the number
 * of bytes used in a buffer. The buffers used are the
 * ones that were previously set with emberAfSetExternalBuffer.
 *
 * @param frameControl byte used for frame control
 * @param clusterId cluster id of message
 * @param commandId command id of message
 * @param format String format for further arguments to the function.
 *   Format values are: 
 *     - '0' -- '9' and 'A' -- 'G': Pointer to a buffer contain 0--16 raw
 *            bytes.  The data is copied as is to the destination buffer.
 *     - 'u': int8u.
 *     - 'v': int16u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'x': int24u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'w': int32u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'l': Pointer to a buffer containing a ZigBee long string, with the
 *            first two bytes of the buffer specifying the length of the string
 *            data in little-endian format.  The length bytes and the string
 *            itself are both included as is in the destination buffer.
 *     - 's': Pointer to a buffer containing a ZigBee string, with the first
 *            byte of the buffer specifying the length of the string data.  The
 *            length byte and the string itself are both included as is in the
 *            destination buffer.
 *     - 'L': Pointer to a buffer containing a long string followed by an
 *            int16u specifying the length of the string data.  The length
 *            bytes in little-endian format will precede the string itself in
 *            the destination buffer.
 *     - 'S': Pointer to a buffer containing a string followed by an int8u
 *            specifying the length of the string data.  The length byte will
 *            precede the string itself in the destination buffer.
 *     - 'b': Pointer to a buffer containing a sequence of raw bytes followed
 *            by an int16u specifying the length of the data.  The data is
 *            copied as is to the destination buffer.  The length is not
 *            included.
 */
int16u emberAfFillExternalBuffer(int8u frameControl,
                                 EmberAfClusterId clusterId,
                                 int8u commandId,
                                 PGM_P format,
                                 ...);

/** 
 * @brief Function that fills in the buffer with manufacturer-specifc command.
 *
 * Fills the outgoing zcl buffer and returns the number
 * of bytes used in a buffer. The buffers used are the
 * ones that were previously set with emberAfSetExternalBuffer.
 *
 * @param frameControl byte used for frame control
 * @param clusterId cluster id of message
 * @param manufacturerCode manufacturer code of message
 * @param commandId command id of message
 * @param format String format for further arguments to the function.
 *   Format values are: 
 *     - '0' -- '9' and 'A' -- 'G': Pointer to a buffer contain 0--16 raw
 *            bytes.  The data is copied as is to the destination buffer.
 *     - 'u': int8u.
 *     - 'v': int16u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'x': int24u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'w': int32u.  The bytes are copied in little-endian format to the
 *            destination buffer.
 *     - 'l': Pointer to a buffer containing a ZigBee long string, with the
 *            first two bytes of the buffer specifying the length of the string
 *            data in little-endian format.  The length bytes and the string
 *            itself are both included as is in the destination buffer.
 *     - 's': Pointer to a buffer containing a ZigBee string, with the first
 *            byte of the buffer specifying the length of the string data.  The
 *            length byte and the string itself are both included as is in the
 *            destination buffer.
 *     - 'L': Pointer to a buffer containing a long string followed by an
 *            int16u specifying the length of the string data.  The length
 *            bytes in little-endian format will precede the string itself in
 *            the destination buffer.
 *     - 'S': Pointer to a buffer containing a string followed by an int8u
 *            specifying the length of the string data.  The length byte will
 *            precede the string itself in the destination buffer.
 *     - 'b': Pointer to a buffer containing a sequence of raw bytes followed
 *            by an int16u specifying the length of the data.  The data is
 *            copied as is to the destination buffer.  The length is not
 *            included.
 */
int16u emberAfFillExternalManufacturerSpecificBuffer(int8u frameControl,
                                                     EmberAfClusterId clusterId,
                                                     int16u manufacturerCode,
                                                     int8u commandId,
                                                     PGM_P format,
                                                     ...);

/** 
 * @brief Function  that registers the buffer to use with the fill function.
 * Registers the buffer for use with the emberAfFillExternalBuffer function.
 *
 * @param buffer Main buffer for constructing message.
 * @param bufferLen Available length of buffer.
 * @param responseLengthPtr location where length of message will be written into.
 * @param apsFramePtr location where APS frame data will be written.
 */
void emberAfSetExternalBuffer(int8u *buffer, 
                              int16u bufferLen,
                              int16u *responseLengthPtr,
                              EmberApsFrame *apsFramePtr);

/** 
 * @brief Stateless function that fills the passed buffer with command data.
 *
 * Stateless method, that fill the passed buffer.
 * This method is used internally by emberAfFillExternalBuffer, but can be used
 * for generic buffer filling.
 */
int16u emberAfFillBuffer(int8u *buffer,
                         int16u bufferLen,
                         int8u frameControl,
                         int8u commandId,
                         PGM_P format,
                         ...);

// Generated macros
#include "client-command-macro.h"

/** @} END Client API functions */

#endif // __CLIENT_API__
