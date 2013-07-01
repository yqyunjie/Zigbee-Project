/** @file ash-host.h
 * @brief Header for ASH Host functions
 *
 * See @ref ash_util for documentation.
 *
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup ash_util
 *
 * See ash-host.h.
 *
 *@{
 */
 
#ifndef __ASH_HOST_H__
#define __ASH_HOST_H__

#define ASH_MAX_TIMEOUTS        6   /*!< timeouts before link is judged down */

#define ASH_PORT_LEN            40  /*!< length of serial port name string */

// Bits in traceFlags to enable various host trace outputs
#define TRACE_FRAMES_BASIC      1   /*!< frames sent and received */
#define TRACE_FRAMES_VERBOSE    2   /*!< basic frames + internal variables */
#define TRACE_EVENTS            4   /*!< events */
#define TRACE_EZSP              8   /*!< EZSP commands, responses and callbacks */
#define TRACE_EZSP_VERBOSE     16   /*!< additional EZSP information */

// resetMethod values
#define ASH_RESET_METHOD_RST    0   /*!< send RST frame */
#define ASH_RESET_METHOD_DTR    1   /*!< reset using DTR */
#define ASH_RESET_METHOD_CUSTOM 2   /*!< hook for user-defined reset */
#define ASH_RESET_METHOD_NONE   3   /*!< no reset - for testing */

// ncpType values
#define ASH_NCP_TYPE_EM2XX      0   /*!< EM250 or EM260 */
#define ASH_NCP_TYPE_AVR        1   /*!< AVR ATmega-128 with EM2420 */

// ashSelectHostConfig() values
#define ASH_HOST_CONFIG_EM2XX_115200_RTSCTS   0
#define ASH_HOST_CONFIG_EM2XX_57600_XONXOFF   1
#define ASH_HOST_CONFIG_AVR_38400_XONXOFF     2

/** @brief Configuration parameters: values must be defined before calling ashResetNcp() 
 * or ashStart(). Note that all times are in milliseconds.
 */
typedef struct
{
  char serialPort[ASH_PORT_LEN];  /*!< serial port name */
  int32u baudRate;      /*!< baud rate (bits/second) */
  int8u  stopBits;      /*!< stop bits */
  int8u  rtsCts;        /*!< TRUE enables RTS/CTS flow control, FALSE XON/XOFF */
  int16u outBlockLen;   /*!< max bytes to buffer before writing to serial port */
  int16u inBlockLen;    /*!< max bytes to read ahead from serial port */
  int8u  traceFlags;    /*!< trace output control bit flags */
  int8u  txK;           /*!< max frames sent without being ACKed (1-7) */
  int8u  randomize;     /*!< enables randomizing DATA frame payloads */
  int16u ackTimeInit;   /*!< adaptive rec'd ACK timeout initial value */
  int16u ackTimeMin;    /*!< adaptive rec'd ACK timeout minimum value */
  int16u ackTimeMax;    /*!< adaptive rec'd ACK timeout maximum value */
  int16u timeRst;       /*!< time allowed to receive RSTACK after ncp is reset */
  int8u  nrLowLimit;    /*!< if free buffers < limit, host receiver isn't ready */
  int8u  nrHighLimit;   /*!< if free buffers > limit, host receiver is ready */
  int16u nrTime;        /*!< time after which a set nFlag must be resent */
  int8u  resetMethod;   /*!< method used to reset ncp */
  int8u  ncpType;       /*!< type of ncp processor */
} AshHostConfig;

#define ashReadConfig(member) \
  (ashHostConfig.member)

#define ashReadConfigOrDefault(member, defval) \
  (ashHostConfig.member)

#define ashWriteConfig(member, value) \
  do { ashHostConfig.member = value; } while (0)

#define BUMP_HOST_COUNTER(mbr) do {ashCount.mbr++;} while (0)
#define ADD_HOST_COUNTER(op, mbr) do {ashCount.mbr += op;}  while(0)

typedef struct 
{
  int32u txBytes;             /*!< total bytes transmitted */
  int32u txBlocks;            /*!< blocks transmitted */
  int32u txData;              /*!< DATA frame data fields bytes transmitted */
  int32u txAllFrames;         /*!< frames of all types transmitted */
  int32u txDataFrames;        /*!< DATA frames transmitted */
  int32u txAckFrames;         /*!< ACK frames transmitted */
  int32u txNakFrames;         /*!< NAK frames transmitted */
  int32u txReDataFrames;      /*!< DATA frames retransmitted */
  int32u txN0Frames;          /*!< ACK and NAK frames with nFlag 0 transmitted */
  int32u txN1Frames;          /*!< ACK and NAK frames with nFlag 1 transmitted */
  int32u txCancelled;         /*!< frames cancelled (with ASH_CAN byte) */

  int32u rxBytes;             /*!< total bytes received */
  int32u rxBlocks;            /*!< blocks received         */
  int32u rxData;              /*!< DATA frame data fields bytes received */
  int32u rxAllFrames;         /*!< frames of all types received         */
  int32u rxDataFrames;        /*!< DATA frames received                    */
  int32u rxAckFrames;         /*!< ACK frames received                       */
  int32u rxNakFrames;         /*!< NAK frames received                       */
  int32u rxReDataFrames;      /*!< retransmitted DATA frames received        */            
  int32u rxN0Frames;          /*!< ACK and NAK frames with nFlag 0 received  */
  int32u rxN1Frames;          /*!< ACK and NAK frames with nFlag 1 received  */
  int32u rxCancelled;         /*!< frames cancelled (with ASH_CAN byte) */

  int32u rxCrcErrors;         /*!< frames with CRC errors */
  int32u rxCommErrors;        /*!< frames with comm errors (with ASH_SUB byte) */
  int32u rxTooShort;          /*!< frames shorter than minimum */
  int32u rxTooLong;           /*!< frames longer than maximum */
  int32u rxBadControl;        /*!< frames with illegal control byte */
  int32u rxBadLength;         /*!< frames with illegal length for type of frame */
  int32u rxBadAckNumber;      /*!< frames with bad ACK numbers */
  int32u rxNoBuffer;          /*!< DATA frames discarded due to lack of buffers */
  int32u rxDuplicates;        /*!< duplicate retransmitted DATA frames */
  int32u rxOutOfSequence;     /*!< DATA frames received out of sequence */
  int32u rxAckTimeouts;       /*!< received ACK timeouts */
} AshCount;

extern EzspStatus ashError;
extern EzspStatus ncpError;
extern AshHostConfig ashHostConfig;
extern AshCount ashCount;

/** @brief Selects a set of host configuration parameters. To select
 * a configuration other than the default, must be called before ashStart().
 *
 * @param config  one of the following:
 *  -            ::ASH_HOST_CONFIG_EM2XX_115200_RTSCTS (default)
 *  -            ::ASH_HOST_CONFIG_EM2XX_57600_RTSCTS
 *  -            ::ASH_HOST_CONFIG_EM2XX_57600_XONXOFF
 *  -            ::ASH_HOST_CONFIG_AVR_38400_XONXOFF 
 *
 *
 * @return  
 * - ::EZSP_SUCCESS
 * _ ::EZSP_ASH_HOST_FATAL_ERROR
 */
EzspStatus ashSelectHostConfig(int8u config);

/** @brief Initializes the ASH protocol, and waits until the NCP
 * finishes rebooting, or a non-recoverable error occurs.
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_HOST_FATAL_ERROR
 * - ::EZSP_ASH_NCP_FATAL_ERROR
 */
EzspStatus ashStart(void);

/** @brief Stops the ASH protocol - flushes and closes the serial port,
 *  clears all queues, stops timers, etc. Does not affect any host configuration
 *  parameters.
 */
void ashStop(void);

/** @brief Adds a DATA frame to the transmit queue to send to the NCP.
 *  Frames that are too long or too short will not be sent, and frames
 *  will not be added to the queue if the host is not in the Connected
 *  state, or the NCP is not ready to receive a DATA frame or if there
 *  is no room in the queue;
 *
 * @param len    length of data field
 *
 * @param inptr  pointer to array containing the data to be sent
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_NO_TX_SPACE
 * - ::EZSP_ASH_DATA_FRAME_TOO_SHORT
 * - ::EZSP_ASH_DATA_FRAME_TOO_LONG
 * - ::EZSP_ASH_NOT_CONNECTED
 */
EzspStatus ashSend(int8u len, const int8u *inptr);

/** @brief Initializes the ASH serial port and (if enabled)
 *  resets the NCP. The method used to do the reset is specified by the
 *  the host configuration parameter resetMethod.
 *
 *  When the reset method is sending a RST frame, the caller should 
 *  retry NCP resets a few times if it fails.
 *
 * @return
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_HOST_FATAL_ERROR
 * - ::EZSP_ASH_NCP_FATAL_ERROR
 */
EzspStatus ashResetNcp(void);

/** @brief Indicates if the host is in the Connected state.
 *  If not, the host and NCP cannot exchange DATA frames. Note that
 *  this function does not actively confirm that communication with
 *  NCP is healthy, but simply returns its last known status.
 *
 * @return  
 * - TRUE      host and NCP can exchange DATA frames
 * - FALSE     host and NCP cannot now exchange DATA frames
*/
boolean ashIsConnected(void);

/** @brief Manages outgoing communication to the NCP, including
 *  DATA frames as well as the frames used for initialization and
 *  error detection and recovery.
 */
void ashSendExec(void);

/** @brief Processes all received frames.
 *  Received DATA frames are appended to the receive queue if there is room.
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_IN_PROGRESS
 * - ::EZSP_ASH_NO_RX_DATA
 * - ::EZSP_ASH_NO_RX_SPACE
 * - ::EZSP_ASH_HOST_FATAL_ERROR
 * - ::EZSP_ASH_NCP_FATAL_ERROR
 */
EzspStatus ashReceiveExec(void);

/** @brief Returns the next DATA frame received, if there is one.
 *  To be more precise, the head of the receive queue is copied into the
 *  specified buffer and then freed.
 *
 * @param len     length of the DATA frame if one was returnnd
 *
 * @param buffer  array into which the DATA frame should be copied
 *
 * @return  
 * - ::EZSP_SUCCESS
 * - ::EZSP_ASH_NO_RX_DATA
 * - ::EZSP_ASH_NOT_CONNECTED
 */
EzspStatus ashReceive(int8u *len, int8u *buffer);

#endif //__ASH_HOST_H___

/** @} // END addtogroup
 */
