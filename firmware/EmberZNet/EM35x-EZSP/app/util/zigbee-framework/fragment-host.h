/**
 * @file fragment-host.h
 * @brief Fragmented message support for EZSP Hosts. Splits long messages into
 * smaller blocks for transmission and reassembles received blocks.
 * See @ref fragment for documentation.
 *
 * @deprecated The fragment library is deprecated and will be removed in a
 * future release.  Similar functionality is available in the Fragmentation
 * plugin in Application Framework.
 *
 * <!--Copyright 2007 by Ember Corporation. All rights reserved.         *80*-->
 */

/** @addtogroup fragment
 * Fragmented message support for EZSP Hosts. Splits long messages into smaller
 * blocks for transmission and reassembles received blocks. See fragment-host.c
 * for source code.
 *
 * ::EZSP_CONFIG_FRAGMENT_WINDOW_SIZE controls how many blocks are sent at a
 * time. ::EZSP_CONFIG_FRAGMENT_DELAY_MS controls the spacing between blocks.
 *
 * Before calling any of the other functions listed here, the application must
 * call ezspFragmentInit().
 *
 * To send a long message, the application calls ezspFragmentSendUnicast().
 * The application must add a call to ezspFragmentMessageSent() at the start of
 * its ezspMessageSentHandler(). If ezspFragmentMessageSent() returns TRUE,
 * the fragmentation code has handled the event and the application must not
 * process it further. The fragmentation code calls the application-defined
 * ezspFragmentMessageSentHandler() when it has finished sending the long
 * message.
 *
 * To receive a long message, the application must add a call to
 * ezspFragmentIncomingMessage() at the start of its
 * ezspIncomingMessageHandler(). If ezspFragmentIncomingMessage() returns
 * TRUE, the fragmentation code has handled the message and the application must
 * not process it further. The application must also call ezspFragmentTick()
 * regularly.
 * @{
 */

/** @name Initialization
 * @{
 */

/** 
 * @brief Initialize variables and buffers used for sending and receiving long
 * messages. This functions reads the values of ::EZSP_CONFIG_MAX_HOPS and
 * ::EZSP_CONFIG_FRAGMENT_WINDOW_SIZE. The application must set these values
 * before calling this function.
 *
 * @param receiveBufferLength The length of receiveBuffer. Incoming messages
 *                            longer than this will be dropped.
 * @param receiveBuffer       The buffer used to reassemble incoming long
 *                            messages. Once the message is complete, this
 *                            buffer will be passed back to the application by
 *                            ezspFragmentIncomingMessage().
 */
void ezspFragmentInit(int16u receiveBufferLength, int8u *receiveBuffer);

/** @} END name group */

/** @name Transmitting
 * @{
 */

/** 
 * @brief Sends a long message by splitting it into blocks. Only one long message can
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
 * @param maxFragmentSize    The message will be broken into blocks no larger
 *                           than this.
 * @param messageLength      The length of the messageContents parameter in
 *                           bytes.
 * @param messageContents    The long message to be sent.
 *
 * @return An EmberStatus value.
 * - ::EMBER_SUCCESS
 * - ::EMBER_MESSAGE_TOO_LONG
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_NETWORK_BUSY
 * - ::EMBER_INVALID_CALL is returned if messageLength is
 *     zero or if the window size (::EZSP_CONFIG_FRAGMENT_WINDOW_SIZE) is zero.
 */
EmberStatus ezspFragmentSendUnicast(EmberOutgoingMessageType type,
                                    int16u indexOrDestination,
                                    EmberApsFrame *apsFrame,
                                    int8u maxFragmentSize,
                                    int16u messageLength,
                                    int8u *messageContents);

/**
 * @brief A callback invoked just before each block of the current long message is
 * sent. If the message is to be source routed, the application must define this
 * callback and call ezspSetSourceRoute() in it.
 *
 * The application must define
 * EZSP_APPLICATION_HAS_FRAGMENT_SOURCE_ROUTE_HANDLER
 * in its configuration header if it defines this callback.
 *
 * @return ::EMBER_SUCCESS if the source route has been set. Any other value
 * will abort transmission of the current long message.
 */
EmberStatus ezspFragmentSourceRouteHandler(void);

/** 
 * @brief The application must call this function at the start of
 * its ezspMessageSentHandler(). If it returns TRUE, the fragmentation code has
 * handled the event and the application must not process it further.
 *
 * @param apsFrame The APS frame passed to ezspMessageSentHandler().
 * @param status   The status passed to ezspMessageSentHandler().
 *
 * @return TRUE if the sent message was a block of a long message. The
 * fragmentation code has handled the event so the application must return
 * immediately from its ezspMessageSentHandler(). Returns FALSE otherwise. The
 * fragmentation code has not handled the event so the application must
 * continue to process it.
 */
boolean ezspFragmentMessageSent(EmberApsFrame *apsFrame, EmberStatus status);

/** 
 * @brief The fragmentation code calls this application-defined handler when it
 * finishes sending a long message.
 *
 * @param status ::EMBER_SUCCESS if all the blocks of the long message were
 * delivered to the destination, otherwise ::EMBER_DELIVERY_FAILED,
 * ::EMBER_NETWORK_DOWN or ::EMBER_NETWORK_BUSY.
 */
void ezspFragmentMessageSentHandler(EmberStatus status);

/** @} END name group */

/** @name Receiving
 * @{
 */

/** 
 * @brief The application must call this function at the start of its
 * ezspIncomingMessageHandler(). If it returns TRUE, the fragmentation code has
 * handled the message and the application must not process it further. When the
 * final block of a long message is received, this function replaces the message
 * with the reassembled long message and returns FALSE so that the application
 * processes it.
 *
 * @param apsFrame        The APS frame passed to ezspIncomingMessageHandler().
 * @param sender          The sender passed to ezspIncomingMessageHandler().
 * @param messageLength   A pointer to the message length passed to
 *                        ezspIncomingMessageHandler().
 * @param messageContents A pointer to the message contents passed to
 *                        ezspIncomingMessageHandler().
 *
 * @return TRUE if the incoming message was a block of an incomplete long
 * message. The fragmentation code has handled the message so the application
 * must return immediately from its ezspIncomingMessageHandler(). Returns
 * FALSE if the incoming message was not part of a long message. The
 * fragmentation code has not handled the message so the application must
 * continue to process it. Returns FALSE if the incoming message was a block
 * that completed a long message. The fragmentation code replaces the message
 * with the reassembled long message so the application must continue to process
 * it.
 */
boolean ezspFragmentIncomingMessage(EmberApsFrame *apsFrame,
                                    EmberNodeId sender,
                                    int16u *messageLength,
                                    int8u **messageContents);

/** 
 * @brief Used by the fragmentation code to time incoming blocks. The
 * application must call this function regularly.
 */
void ezspFragmentTick(void);

/** @} END name group */

/** @} END addtogroup */
