/**
 * @file fragment.h
 * @brief Splits long messages into smaller blocks for transmission and
 * reassembles received blocks.
 * See @ref fragment for documentation.
 *
 * @deprecated The fragment library is deprecated and will be removed in a
 * future release.  Similar functionality is available in the Fragmentation
 * plugin in Application Framework.
 *
 * <!--Copyright 2007 by Ember Corporation. All rights reserved.         *80*-->
 */

/** @addtogroup fragment 
 * Splits long messages into smaller blocks for transmission and reassembles
 * received blocks. See fragment.h for source code.
 *
 * ::EMBER_FRAGMENT_WINDOW_SIZE controls how many blocks are sent at a time.
 * ::EMBER_FRAGMENT_DELAY_MS controls the spacing between blocks.
 *
 * To send a long message, the application calls emberFragmentSendUnicast().
 * The application must add a call to emberFragmentMessageSent() at the start of
 * its emberMessageSentHandler(). If emberFragmentMessageSent() returns TRUE,
 * the fragmentation code has handled the event and the application must not
 * process it further. The fragmentation code calls the application-defined
 * emberFragmentMessageSentHandler() when it has finished sending the long
 * message.
 *
 * To receive a long message, the application must add a call to
 * emberFragmentIncomingMessage() at the start of its
 * emberIncomingMessageHandler(). If emberFragmentIncomingMessage() returns
 * TRUE, the fragmentation code has handled the message and the application must
 * not process it further. The application must also call emberFragmentTick()
 * regularly.
 * @{
 */

/** @name Transmitting
 * @{
 */

/** Sends a long message by splitting it into blocks. Only one long message can
 * be sent at a time. Calling this function a second time aborts the first
 * message.
 * 
 * @param type               Specifies the outgoing message type.  Must be one
 *                           of ::EMBER_OUTGOING_DIRECT,
 *                           ::EMBER_OUTGOING_VIA_ADDRESS_TABLE, or
 *                           ::EMBER_OUTGOING_VIA_BINDING.
 * @param indexOrDestination Depending on the type of addressing used, this
 *                           is either the EmberNodeId of the destination, an
 *                           index into the address table, or an index into the
 *                           binding table.
 * @param apsFrame           The APS frame for the message.
 * @param payload            The long message to be sent.
 * @param maxFragmentSize    The message will be broken into blocks no larger
 *                           than this.
 * 
 * @return An EmberStatus value. 
 * - ::EMBER_SUCCESS
 * - ::EMBER_MESSAGE_TOO_LONG
 * - ::EMBER_NO_BUFFERS
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_NETWORK_BUSY
 * - ::EMBER_INVALID_CALL is returned if the payload length is
 *     zero or if the window size (::EMBER_FRAGMENT_WINDOW_SIZE) is zero.
 */
EmberStatus emberFragmentSendUnicast(EmberOutgoingMessageType type,
                                     int16u indexOrDestination,
                                     EmberApsFrame *apsFrame,
                                     EmberMessageBuffer payload,
                                     int8u maxFragmentSize);

/** The application must call this function at the start of
 * its emberMessageSentHandler(). If it returns TRUE, the fragmentation code has
 * handled the event and the application must not process it further.
 * 
 * @param apsFrame The APS frame passed to emberMessageSentHandler().
 * @param buffer   The buffer passed to emberMessageSentHandler().
 * @param status   The status passed to emberMessageSentHandler().
 * 
 * @return TRUE if the sent message was a block of a long message. The
 * fragmentation code has handled the event so the application must return
 * immediately from its emberMessageSentHandler(). Returns FALSE otherwise. The
 * fragmentation code has not handled the event so the application must
 * continue to process it.
 */
boolean emberFragmentMessageSent(EmberApsFrame *apsFrame,
                                 EmberMessageBuffer buffer,
                                 EmberStatus status);

/** The fragmentation code calls this application-defined handler when it
 * finishes sending a long message.
 * 
 * @param status ::EMBER_SUCCESS if all the blocks of the long message were
 * delivered to the destination, otherwise ::EMBER_DELIVERY_FAILED,
 * ::EMBER_NO_BUFFERS, ::EMBER_NETWORK_DOWN or ::EMBER_NETWORK_BUSY.
 */
void emberFragmentMessageSentHandler(EmberStatus status);

/** @} END name group */

/** @name Receiving
 * @{
 */

/** The application must call this function at the start of its
 * emberIncomingMessageHandler(). If it returns TRUE, the fragmentation code has
 * handled the message and the application must not process it further. When the
 * final block of a long message is received, this function replaces the message
 * with the reassembled long message and returns FALSE so that the application
 * processes it.
 * 
 * @param apsFrame  The APS frame passed to emberIncomingMessageHandler().
 * @param payload   The payload passed to emberIncomingMessageHandler().
 * 
 * @return TRUE if the incoming message was a block of an incomplete long
 * message. The fragmentation code has handled the message so the application
 * must return immediately from its emberIncomingMessageHandler(). Returns
 * FALSE if the incoming message was not part of a long message. The
 * fragmentation code has not handled the message so the application must
 * continue to process it. Returns FALSE if the incoming message was a block
 * that completed a long message. The fragmentation code replaces the message
 * with the reassembled long message so the application must continue to process
 * it.
 */
boolean emberFragmentIncomingMessage(EmberApsFrame *apsFrame,
                                     EmberMessageBuffer payload);

/** Used by the fragmentation code to time incoming blocks. The
 * application must call this function regularly.
 */
void emberFragmentTick(void);

/** @} // END name group
 */

/** @} END addtogroup */
