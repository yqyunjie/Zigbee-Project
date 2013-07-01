/** @file ezsp-callbacks.c
 * @brief Convenience stubs for little-used EZSP callbacks. 
 *
 * <!--Copyright 2006 by Ember Corporation. All rights reserved.         *80*-->
 */

#include PLATFORM_HEADER 
#include "stack/include/ember-types.h"
#include "stack/include/error.h"

// *****************************************
// Convenience Stubs
// *****************************************

#ifndef EZSP_APPLICATION_HAS_WAITING_FOR_RESPONSE
void ezspWaitingForResponse(void)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_NO_CALLBACKS
void ezspNoCallbacks(void)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_TIMER_HANDLER
void ezspTimerHandler(int8u timerId)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_DEBUG_HANDLER
void ezspDebugHandler(int8u messageLength, 
                      int8u *messageContents)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_CHILD_JOIN_HANDLER
void ezspChildJoinHandler(int8u index,
                          boolean joining,
                          EmberNodeId childId,
                          EmberEUI64 childEui64,
                          EmberNodeType childType)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
void ezspTrustCenterJoinHandler(EmberNodeId newNodeId,
                                EmberEUI64 newNodeEui64,
                                EmberDeviceUpdate status,
                                EmberJoinDecision policyDecision,
                                EmberNodeId parentOfNewNode)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_ZIGBEE_KEY_ESTABLISHMENT_HANDLER
void ezspZigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_SWITCH_NETWORK_KEY_HANDLER
void ezspSwitchNetworkKeyHandler(int8u sequenceNumber)
{
}
#endif

#ifndef EZSP_APPLICATION_HAS_REMOTE_BINDING_HANDLER
void ezspRemoteSetBindingHandler(EmberBindingTableEntry *entry,
                                 int8u index,
                                 EmberStatus policyDecision)
{}

void ezspRemoteDeleteBindingHandler(int8u index,
                                    EmberStatus policyDecision)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_POLL_COMPLETE_HANDLER
void ezspPollCompleteHandler(EmberStatus status) {}
#endif

#ifndef EZSP_APPLICATION_HAS_POLL_HANDLER
void ezspPollHandler(EmberNodeId childId) {}
#endif

#ifndef EZSP_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
void ezspEnergyScanResultHandler(int8u channel, int8u maxRssiValue)
{
}
#endif

#ifndef EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
void ezspIncomingRouteRecordHandler(EmberNodeId source,
                                    EmberEUI64 sourceEui,
                                    int8u lastHopLqi,
                                    int8s lastHopRssi,
                                    int8u relayCount,
                                    int16u *relayList)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_BUTTON_HANDLER
void halButtonIsr(int8u button, int8u state)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_INCOMING_SENDER_EUI64_HANDLER
void ezspIncomingSenderEui64Handler(EmberEUI64 senderEui64)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_ID_CONFLICT_HANDLER
void ezspIdConflictHandler(EmberNodeId id)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_INCOMING_MANY_TO_ONE_ROUTE_REQUEST_HANDLER
void ezspIncomingManyToOneRouteRequestHandler(EmberNodeId source,
                                              EmberEUI64 longId,
                                              int8u cost)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_INCOMING_ROUTE_ERROR_HANDLER
void ezspIncomingRouteErrorHandler(EmberStatus status, EmberNodeId target)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_BOOTLOADER_HANDLER
void ezspIncomingBootloadMessageHandler(EmberEUI64 longId,
                                        int8u lastHopLqi,
                                        int8s lastHopRssi,
                                        int8u messageLength,
                                        int8u *messageContents)
{}

void ezspBootloadTransmitCompleteHandler(EmberStatus status,
                                         int8u messageLength,
                                         int8u *messageContents)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_MAC_PASSTHROUGH_HANDLER
void ezspMacPassthroughMessageHandler(int8u messageType,
                                      int8u lastHopLqi,
                                      int8s lastHopRssi,
                                      int8u messageLength,
                                      int8u *messageContents)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_MFGLIB_HANDLER
void ezspMfglibRxHandler(
      int8u linkQuality,
      int8s rssi,
      int8u packetLength,
      int8u *packetContents)
{}
#endif

#ifndef EZSP_APPLICATION_HAS_RAW_HANDLER
void ezspRawTransmitCompleteHandler(EmberStatus status)
{}
#endif

// Certificate Based Key Exchange (CBKE)
#ifndef EZSP_APPLICATION_HAS_CBKE_HANDLERS
void ezspGenerateCbkeKeysHandler(EmberStatus status,
                                 EmberPublicKeyData* ephemeralPublicKey)
{
}

void ezspCalculateSmacsHandler(EmberStatus status,
                               EmberSmacData* initiatorSmac,
                               EmberSmacData* responderSmac)
{
}
#endif

// Elliptical Cryptography Digital Signature Algorithm (ECDSA)
#ifndef EZSP_APPLICATION_HAS_DSA_SIGN_HANDLER
void ezspDsaSignHandler(EmberStatus status,
                        int8u messageLength,
                        int8u* messageContents)
{
}
#endif

#ifndef EZSP_APPLICATION_HAS_UNUSED_PANID_FOUND_HANDLER
void ezspUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
}
#endif

#ifndef EZSP_APPLICATION_HAS_FRAGMENT_SOURCE_ROUTE_HANDLER
EmberStatus ezspFragmentSourceRouteHandler(void)
{
  return EMBER_SUCCESS;
}
#endif
