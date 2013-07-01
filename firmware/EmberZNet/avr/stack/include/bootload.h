/**
 * @file bootload.h
 * See @ref znet_bootload for documentation.
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */

/**
 * @addtogroup znet_bootload
 * @brief EmberZNet bootload API.
 *
 * See bootload.h for source code.
 * @{
 */

/** @brief Transmits the given bootload message to a neighboring
 * node using a specific 802.15.4 header that allows the EmberZNet stack 
 * as well as the bootloader to recognize the message, but will not 
 * interfere with other ZigBee stacks.
 *
 * @param broadcast  If TRUE, the destination address and pan id are
 *     both set to the broadcast address.  
 * @param destEui64  The EUI64 of the target node.  Ignored if
 *     the broadcast field is set to TRUE.
 * @param message    The bootloader message to send.
 *
 * @return ::EMBER_SUCCESS if the message was successfully submitted to
 * the transmit queue, and ::EMBER_ERR_FATAL otherwise.
 */
EmberStatus emberSendBootloadMessage(boolean broadcast,
                                     EmberEUI64 destEui64,
                                     EmberMessageBuffer message);

/**@brief A callback invoked by the EmberZNet stack when a 
 * bootload message is received.  If the application includes
 * ::emberIncomingBootloadMessageHandler(),
 * it must define EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS in its
 * CONFIGURATION_HEADER.
 *
 * @param longId   The EUI64 of the sending node.
 * @param message  The bootload message that was sent.
 */
void emberIncomingBootloadMessageHandler(EmberEUI64 longId, 
                                         EmberMessageBuffer message);

/**@brief A callback invoked by the EmberZNet stack when the
 * MAC has finished transmitting a bootload message.  If the 
 * application includes this callback, it must define
 * EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS in its CONFIGURATION_HEADER.
 *
 * @param message  The message that was sent.
 * @param status   ::EMBER_SUCCESS if the transmission was successful,
 *    or ::EMBER_DELIVERY_FAILED if not.
 */
void emberBootloadTransmitCompleteHandler(EmberMessageBuffer message,
                                          EmberStatus status);

/** @} END addtogroup */
                                         
/**
 * <!-- HIDDEN
 * @page 2p5_to_3p0
 * <hr>
 * The file bootload.h is new and is described in @ref znet_bootload.
 * 
 * - <b>Items moved from ember.h</b>
 *   - emberBootloadTransmitCompleteHandler() 
 *   - emberIncomingBootloadMessageHandler() 
 *   - emberSendBootloadMessage() 
 *   .
 * .
 * HIDDEN -->
 */
