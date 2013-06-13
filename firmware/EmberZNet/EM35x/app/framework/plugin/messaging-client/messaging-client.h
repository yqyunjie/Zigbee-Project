// *******************************************************************
// * messaging-client.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// ----------------------------------------------------------------------------
// Message Control byte
// ----------------------------------------------------------------------------

#define ZCL_MESSAGING_CLUSTER_TRANSMISSION_MASK (BIT(1) | BIT(0))
#define ZCL_MESSAGING_CLUSTER_IMPORTANCE_MASK   (BIT(3) | BIT(2))
#define ZCL_MESSAGING_CLUSTER_CONFIRMATION_MASK BIT(7)

#define ZCL_MESSAGING_CLUSTER_START_TIME_NOW         0x00000000UL
#define ZCL_MESSAGING_CLUSTER_END_TIME_NEVER         0xFFFFFFFFUL
#define ZCL_MESSAGING_CLUSTER_DURATION_UNTIL_CHANGED 0xFFFF

void emAfPluginMessagingClientPrintInfo(int8u endpoint);

/**
 * @brief Confirm a message.
 *
 * This function can be used to confirm a message.  The Messaging client plugin
 * will send the confirmation command to the endpoint on the node that sent the
 * message.
 *
 * @return ::EMBER_ZCL_STATUS_SUCCESS if the confirmation was sent,
 * ::EMBER_ZCL_STATUS_FAILURE if an error occurred, or
 * ::EMBER_ZCL_STATUS_NOT_FOUND if the message does not exist.
 */
EmberAfStatus emberAfPluginMessagingClientConfirmMessage(int8u endpoint);
