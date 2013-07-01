/**
 * @file fragment.h
 * @brief Breaks long messages into smaller blocks for transmission and
 * reassembles received blocks.
 * See @ref fragment for documentation.
 *
 * <!--Copyright 2007 by Ember Corporation. All rights reserved.         *80*-->
 */

/** @addtogroup fragment 
 * Breaks long messages into smaller blocks for transmission and
 * reassembles received blocks. See fragment.h for source code.
 *
 * Long messages are broken down into blocks. All blocks from a single
 * fragmented message use the same APS sequence number.
 * ::EMBER_FRAGMENT_WINDOW_SIZE controls how many blocks are sent at a time. Each
 * block is sent using emberSendUnicast() with the type set to
 * ::EMBER_OUTGOING_DIRECT. EMBER_FRAGMENT_DELAY_MS controls the spacing between
 * blocks. Once all the blocks in the window have been sent, no new blocks are
 * sent until the receiver has acknowledged all the blocks in the window.
 *
 * To send a long message, the application calls emberSendFragmentedMessage().
 * The application must then call emberIsOutgoingFragment() from within
 * emberMessageSentHandler() until emberFragmentedMessageSentHandler() is
 * called.
 *
 * To receive a long message, the application must call
 * emberIsIncomingFragment() from within emberIncomingMessageHandler(). The
 * application must also call emberFragmentTick() regularly.
 * @{
 */

/** @name Transmitting
 * @{
 */

/** Initiate a new fragmented message. Only one fragmented message can be sent
 * at a time.  Calling this aborts any current message.
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
 * @param payload            The message to be fragmented and sent.
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
EmberStatus emberSendFragmentedMessage(EmberOutgoingMessageType type,
                                       int16u indexOrDestination,
                                       EmberApsFrame *apsFrame,
                                       EmberMessageBuffer payload,
                                       int8u maxFragmentSize);

/** This needs to be called first thing within emberMessageSentHandler(). It
 * returns TRUE if the sent buffer was a fragment and should be ignored by the
 * rest of emberMessageSentHandler().
 * 
 * @param apsFrame The APS frame passed to emberMessageSentHandler().
 * @param buffer   The buffer passed to emberMessageSentHandler().
 * @param status   The status passed to emberMessageSentHandler().
 * 
 * @return TRUE if the buffer was a fragment and should be ignored by the rest
 * of emberMessageSentHandler().
 */
boolean emberIsOutgoingFragment(EmberApsFrame *apsFrame,
                                EmberMessageBuffer buffer,
                                EmberStatus status);

/** The fragmentation code calls this application-defined handler upon
 * completion or failure of the current fragmented message.
 * 
 * @param status ::EMBER_SUCCESS if all the blocks of the current message were
 * delivered to the destination, otherwise ::EMBER_DELIVERY_FAILED,
 * ::EMBER_NO_BUFFERS, ::EMBER_NETWORK_DOWN or ::EMBER_NETWORK_BUSY.
 */
void emberFragmentedMessageSentHandler(EmberStatus status);

/** @} END name group */

/** @name Receiving
 * @{
 */

/** This needs to be called first thing within emberIncomingMessageHandler() for
 * unicasts. It returns TRUE if the message was a fragment and should be ignored
 * by the rest of emberIncomingMessageHandler().
 * 
 * @param apsFrame  The APS frame passed to emberIncomingMessageHandler().
 * @param payload   The payload passed to emberIncomingMessageHandler().
 * 
 * @return TRUE if the buffer was a fragment and should be ignored by the rest
 * of emberIncomingMessageHandler().
 */
boolean emberIsIncomingFragment(EmberApsFrame *apsFrame,
                                EmberMessageBuffer payload);

/** Used by the fragmentation code to time incoming blocks. The
 * application must call emberFragmentTick() regularly.
 */
void emberFragmentTick(void);

/** @} // END name group
 */

/** @} END addtogroup */
