/**
 * @file fragment-host.h
 * @brief Fragmented message support for EZSP Hosts. Breaks long messages into
 * smaller blocks for transmission and reassembles received blocks.
 * See @ref fragment for documentation.
 *
 * <!--Copyright 2007 by Ember Corporation. All rights reserved.         *80*-->
 */

/** @addtogroup fragment 
 * Fragmented message support for EZSP Hosts. Breaks long messages into smaller
 * blocks for transmission and reassembles received blocks. See fragment-host.h
 * for source code.
 *
 * Long messages are broken down into blocks. All blocks from a single
 * fragmented message use the same APS sequence number.
 * ::EZSP_CONFIG_FRAGMENT_WINDOW_SIZE controls how many blocks are sent at a time.
 * Each block is sent using ezspSendUnicast() with the type set to
 * ::EMBER_OUTGOING_DIRECT. ::EZSP_CONFIG_FRAGMENT_DELAY_MS controls the spacing
 * between blocks. Once all the blocks in the window have been sent, no new
 * blocks are sent until the receiver has acknowledged all the blocks in the
 * window.
 *
 * Before calling any of the other functions listed here, the application must
 * call ezspFragmentInit().
 *
 * To send a long message, the application calls ezspSendFragmentedMessage().
 * The application must then call ezspIsOutgoingFragment() from within
 * ezspMessageSentHandler() until ezspFragmentedMessageSentHandler() is
 * called.
 *
 * To receive a long message, the application must call
 * ezspIsIncomingFragment() from within ezspIncomingMessageHandler(). The
 * application must also call ezspFragmentTick() regularly.
 * @{
 */

/** @name Initialization
 * @{
 */

/** Initialize variables and buffers used for sending and receiving fragmented
 * messages. This functions reads the values of ::EZSP_CONFIG_MAX_HOPS and
 * ::EZSP_CONFIG_FRAGMENT_WINDOW_SIZE. The application must set these values
 * before calling this function.
 * 
 * @param receiveBufferLength The length of receiveBuffer. Incoming fragmented
 *                            messages longer than this will be dropped.
 * @param receiveBuffer       The buffer used to assemble incoming fragmented
 *                            messages. Once the message is complete, this
 *                            buffer will be passed back to the application by
 *                            ezspIsIncomingFragment().
 */
void ezspFragmentInit(int8u receiveBufferLength, int8u *receiveBuffer);

/** @} END name group */

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
 * @param maxFragmentSize    The message will be broken into blocks no larger
 *                           than this.
 * @param messageLength      The length of the messageContents parameter in
 *                           bytes.
 * @param messageContents    The message to be fragmented and sent.
 * 
 * @return An EmberStatus value. 
 * - ::EMBER_SUCCESS
 * - ::EMBER_MESSAGE_TOO_LONG
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_NETWORK_BUSY
 * - ::EMBER_INVALID_CALL is returned if messageLength is
 *     zero or if the window size (EZSP_CONFIG_FRAGMENT_WINDOW_SIZE) is zero.
 */
EmberStatus ezspSendFragmentedMessage(EmberOutgoingMessageType type,
                                      int16u indexOrDestination,
                                      EmberApsFrame *apsFrame,
                                      int8u maxFragmentSize,
                                      int8u messageLength,
                                      int8u *messageContents);

/**
 * A callback invoked just before each fragment of the current fragmented
 * message is sent. If the fragment should be source routed, the application
 * must define this callback and call ezspSetSourceRoute() in it.
 *
 * The application must define 
 * EZSP_APPLICATION_HAS_FRAGMENT_SOURCE_ROUTE_HANDLER
 * in its configuration header if it defines this callback.
 *
 * @return ::EMBER_SUCCESS if the source route has been set. Any other value
 * will abort transmission of the fragmented message.
 */
EmberStatus ezspFragmentSourceRouteHandler(void);

/** This needs to be called first thing within ezspMessageSentHandler(). It
 * returns TRUE if the sent buffer was a fragment and should be ignored by the
 * rest of ezspMessageSentHandler().
 * 
 * @param apsFrame The APS frame passed to ezspMessageSentHandler().
 * @param status   The status passed to ezspMessageSentHandler().
 * 
 * @return TRUE if the buffer was a fragment and should be ignored by the rest
 * of ezspMessageSentHandler().
 */
boolean ezspIsOutgoingFragment(EmberApsFrame *apsFrame,
                               EmberStatus status);

/** The fragmentation code calls this application-defined handler upon
 * completion or failure of the current fragmented message.
 * 
 * @param status EMBER_SUCCESS if all the blocks of the current message were
 * delivered to the destination, otherwise EMBER_DELIVERY_FAILED,
 * EMBER_NETWORK_DOWN or EMBER_NETWORK_BUSY.
 */
void ezspFragmentedMessageSentHandler(EmberStatus status);

/** @} END name group */

/** @name Receiving
 * @{
 */

/** This needs to be called first thing within ezspIncomingMessageHandler() for
 * unicasts. It returns TRUE if the message was a fragment and should be ignored
 * by the rest of ezspIncomingMessageHandler(). When the final fragment is
 * received, this function will return FALSE, messageLength will be increased to
 * the length of the complete message and messageContents will be changed to
 * point to the buffer set by ezspFragmentInit().
 * 
 * @param apsFrame        The APS frame passed to ezspIncomingMessageHandler().
 * @param sender          The sender passed to ezspIncomingMessageHandler().
 * @param messageLength   A pointer to the message length passed to
 *                        emberIncomingMessageHandler().
 * @param messageContents A pointer to the message contents passed to
 *                        emberIncomingMessageHandler().
 * 
 * @return TRUE if the buffer was a fragment and should be ignored by the rest
 * of emberIncomingMessageHandler().
 */
boolean ezspIsIncomingFragment(EmberApsFrame *apsFrame,
                               EmberNodeId sender,
                               int8u *messageLength,
                               int8u **messageContents);

/** Used by the fragmentation code to time incoming blocks. The
 * application must call ezspFragmentTick() regularly.
 */
void ezspFragmentTick(void);

/** @} END name group */

/** @} END addtogroup */
