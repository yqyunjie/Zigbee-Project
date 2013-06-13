/**
 * @file: legacy.h
 * 
 * Description: old APIs to facilitate upgrading applications from EmberZNet 2.x
 * to EmberZNet 3.x.
 *
 * @deprecated The legacy library is deprecated and will be removed in a future
 * release.
 *
 * Copyright 2007 by Ember Corporation. All rights reserved.                *80*
 */

/** @description As of EmberZNet 3.0, the functionality formerly provided by
 * the Ember transport layer is now available using pure ZigBee over the air.
 * This API provides the old interface for sending Ember transport datagrams,
 * for convenience when upgrading applications. 
 *
 * Calling this function simply uses the binding table entry to construct
 * a retried APS unicast message. The retry timeout is therefore the APS 
 * timeout, which by default is shorter than the old transport layer timeout.
 *
 * As usual, the status of the delivery is reported by the
 * <code> emberMessageSentHandler() </code> callback.  The outgoing 
 * message type is EMBER_OUTGOING_VIA_BINDING.  On the recieving
 * node, the incoming message type is EMBER_INCOMING_UNICAST.
 * 
 * The name "datagram" is depecreated as of EmberZNet 3.0, in favor of
 * "retried APS unicast".
 * 
 * @param bindingTableIndex: The index of the binding table entry specifying 
 *  the node and endpoint to which a retried APS unicast is being sent.
 *
 * @param clusterId: The cluster ID to use.
 *
 * @param message: Contents of the message.
 *
 * @return An \c EmberStatus value. For any result other than EMBER_SUCCESS, the 
 * message will not be sent.:\n\n
 * - <code> EMBER_SUCCESS </code> - The message has been submitted for 
 * transmission.
 * - <code> EMBER_INVALID_BINDING_INDEX </code> - The \c bindingTableIndex 
 * refers to a non-unicast binding.
 * - <code> EMBER_NETWORK_DOWN </code> - The node is not part of a network.
 * - <code> EMBER_MESSAGE_TOO_LONG </code> - The message is too large to
 * fit in a MAC layer frame.
 * - <code> EMBER_MAX_MESSAGE_LIMIT_REACHED </code> - The 
 * \c EMBER_APS_UNICAST_MESSAGE_COUNT limit has been reached.
 */
EmberStatus emberSendDatagram(int8u bindingTableIndex,
                              int16u clusterId,
                              EmberMessageBuffer message);

/** @description In EmberZNet 3.0, <code>emberCreateAggregationRoutes()</code>
 * is replaced by <code>emberSendManyToOneRouteRequest()</code>, which
 * provides a superset of the functionality.  The over-the-air
 * behavior is unchanged.
 */
#define emberCreateAggregationRoutes() \
  emberSendManyToOneRouteRequest(EMBER_HIGH_RAM_CONCENTRATOR, 0)

/** @description In EmberZNet 3.0, <code>emberMobileNodeHasMoved()</code> 
 * is replaced by <code>emberRejoinNetwork()</code>.  The over-the-air format 
 * has been changed to use the ZigBee network rejoin command.
 */
#define emberMobileNodeHasMoved() emberRejoinNetwork(TRUE)
