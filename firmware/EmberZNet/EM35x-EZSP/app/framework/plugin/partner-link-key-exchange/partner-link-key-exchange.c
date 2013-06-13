// *******************************************************************
// * partner-link-key-exchange.c
// *
// * This file handles the functionality to establish a link key
// * between two devices, neither of which are the TC.   This is called
// * a "Partner Link Key Request" and is done as follows:
// *   1) Device A sends Binding Request to Device B to bind the Key
// *      Establishment Cluster.
// *   2) If Device B has the capacity to support another link key, and allows
// *      the Partner Link Key Request it returns a Binding result of
// *      ZDO Success.
// *   3) Device A receives the binding response and sends an APS Command
// *      of Request Key, type Application Link Key, to the Trust Center.
// *   4) Trust Center checks that both devices have Authorized keys 
// *      (meaning they have gone through Key Establishment successfully).
// *      If they do, then it randomly generates a new key and sends it back
// *      using an APS Command Transport Key.
// *
// * This file implements the binding functionality to initiate,
// * and handle the request/response, plus the functionality to indicate
// * when new key is received from the Trust Center by the stack.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/zigbee-framework/zigbee-device-common.h" //emberBindRequest
#ifdef EZSP_HOST
  #include "app/util/zigbee-framework/zigbee-device-host.h" //emberNetworkAddressRequest
#endif
#include "partner-link-key-exchange.h"

#include "app/framework/util/af-main.h"

extern EmberEventControl emberAfPluginPartnerLinkKeyExchangeTimeoutNetworkEventControls[];

typedef struct {
  boolean active;
  EmberNodeId target;
  EmberAfPartnerLinkKeyExchangeCallback *callback;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

static void partnerLinkKeyExchangeComplete(boolean success);
static EmberStatus validateKeyRequest(EmberEUI64 partnerEui64);
boolean emAfAllowPartnerLinkKey = TRUE;

EmberStatus emberAfInitiatePartnerLinkKeyExchangeCallback(EmberNodeId target,
                                                          int8u endpoint,
                                                          EmberAfPartnerLinkKeyExchangeCallback *callback)
{
  State *state = &states[emberGetCurrentNetwork()];
  EmberEUI64 source, destination;
  EmberStatus status;
  int8u destinationEndpoint;

  if (state->active) {
    emberAfKeyEstablishmentClusterPrintln("%pPartner link key exchange in progress",
                                          "Error: ");
    return EMBER_INVALID_CALL;
  }

  destinationEndpoint = emberAfPrimaryEndpointForCurrentNetworkIndex();
  if (destinationEndpoint == 0xFF) {
    return EMBER_INVALID_CALL;
  }

  status = emberLookupEui64ByNodeId(target, source);
  if (status != EMBER_SUCCESS) {
    emberAfKeyEstablishmentClusterPrintln("%pIEEE address of node 0x%2x is unknown",
                                          "Error: ",
                                          target);
    return status;
  }

  status = validateKeyRequest(source);
  if (status != EMBER_SUCCESS) {
    emberAfKeyEstablishmentClusterPrintln("%p%p: 0x%x",
                                          "Error: ",
                                          "Cannot perform partner link key exchange",
                                          status);
    return status;
  }

  emberAfGetEui64(destination);
  status = emberBindRequest(target,
                            source,
                            endpoint,
                            ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                            UNICAST_BINDING,
                            destination,
                            0, // multicast group identifier - ignored
                            destinationEndpoint,
                            EMBER_APS_OPTION_NONE);
  if (status != EMBER_SUCCESS) {
    emberAfKeyEstablishmentClusterPrintln("%p%p: 0x%x",
                                          "Error: ",
                                          "Failed to send bind request",
                                          status);
  } else {
    state->active = TRUE;
    state->target = target;
    state->callback = callback;
    emberAfAddToCurrentAppTasks(EMBER_AF_WAITING_FOR_PARTNER_LINK_KEY_EXCHANGE);
    emberAfNetworkEventControlSetDelay(emberAfPluginPartnerLinkKeyExchangeTimeoutNetworkEventControls,
                                       EMBER_AF_PLUGIN_PARTNER_LINK_KEY_EXCHANGE_TIMEOUT_MILLISECONDS);

  }
  return status;
}

EmberStatus emberAfPartnerLinkKeyExchangeRequestCallback(EmberEUI64 partner)
{
  EmberStatus status = validateKeyRequest(partner);
  if (status != EMBER_SUCCESS) {
    emberAfKeyEstablishmentClusterPrint("%pRejected parter link key request from ",
                                        "Error: ");
    emberAfKeyEstablishmentClusterDebugExec(emberAfPrintBigEndianEui64(partner));
    emberAfKeyEstablishmentClusterPrintln(": 0x%x", status);
    return status;
  }

  // Manually create an address table entry for the remote node so that the
  // stack will track its network address.  We can't send a network address
  // request here because we are not in the right stack context.
  if (emberAfAddAddressTableEntry(partner, EMBER_UNKNOWN_NODE_ID)
      == EMBER_NULL_ADDRESS_TABLE_INDEX) {
    emberAfKeyEstablishmentClusterPrint("WARN: Could not create address table entry for ");
    emberAfKeyEstablishmentClusterDebugExec(emberAfPrintBigEndianEui64(partner));
    emberAfKeyEstablishmentClusterPrintln("");
  }

  if (!emAfAllowPartnerLinkKey) {
    emberAfKeyEstablishmentClusterPrintln("Partner link key not allowed.");
    return EMBER_SECURITY_CONFIGURATION_INVALID;
  }
  return EMBER_SUCCESS;
}

void emberAfPartnerLinkKeyExchangeResponseCallback(EmberNodeId sender,
                                                   EmberZdoStatus status)
{
  State *state = &states[emberGetCurrentNetwork()];
  if (state->active && state->target == sender) {
    EmberEUI64 partner;
    if (status != EMBER_ZDP_SUCCESS) {
      emberAfKeyEstablishmentClusterPrintln("%pNode 0x%2x rejected partner link key request: 0x%x",
                                            "Error: ",
                                            sender,
                                            status);
      partnerLinkKeyExchangeComplete(FALSE); // failure
      return;
    }
    if (emberLookupEui64ByNodeId(sender, partner) != EMBER_SUCCESS) {
      emberAfKeyEstablishmentClusterPrintln("%pIEEE address of node 0x%2x is unknown",
                                            "Error: ",
                                            sender);
      partnerLinkKeyExchangeComplete(FALSE); // failure
      return;
    }
    {
      EmberStatus status = emberRequestLinkKey(partner);
      if (status != EMBER_SUCCESS) {
        emberAfKeyEstablishmentClusterPrintln("%p%p: 0x%x",
                                              "Error: ",
                                              "Failed to request link key",
                                              status);
        partnerLinkKeyExchangeComplete(FALSE); // failure
        return;
      }
    }
  }
}

static void zigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status)
{
  State *state;
  EmberNodeId nodeId;

  emberAfPushCallbackNetworkIndex();

  state = &states[emberGetCurrentNetwork()];

  // If we can't look up the node id using the long address, we need to send a
  // network address request.  This should only occur when we did not initiate
  // the partner link key exchange, so we don't need to worry about the node id
  // check below that fires the callback.  The stack will update the address
  // table entry that we created above when the response comes back.
  nodeId = emberLookupNodeIdByEui64(partner);
  if (nodeId == EMBER_NULL_NODE_ID) {
    emberNetworkAddressRequest(partner, FALSE, 0); // no children
  }

  emberAfKeyEstablishmentClusterPrintln((status <= EMBER_TRUST_CENTER_LINK_KEY_ESTABLISHED
                                         ? "Key established: %d"
                                         : ((status >= EMBER_TC_RESPONDED_TO_KEY_REQUEST 
                                             && status <= EMBER_TC_APP_KEY_SENT_TO_REQUESTER)
                                            ? "TC answered key request: %d"
                                            : "Failed to establish key: %d")),
                                        status);

  if (state->active && state->target == nodeId) {
    partnerLinkKeyExchangeComplete(status == EMBER_APP_LINK_KEY_ESTABLISHED);
  }

  emberAfPopNetworkIndex();
}

void emberZigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status)
{
  zigbeeKeyEstablishmentHandler(partner, status);
}

void ezspZigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status)
{
  zigbeeKeyEstablishmentHandler(partner, status);
}

void emberAfPluginPartnerLinkKeyExchangeTimeoutNetworkEventHandler(void)
{
  partnerLinkKeyExchangeComplete(FALSE); // failure
}

static void partnerLinkKeyExchangeComplete(boolean success)
{
  State *state = &states[emberGetCurrentNetwork()];

  state->active = FALSE;
  emberAfNetworkEventControlSetInactive(emberAfPluginPartnerLinkKeyExchangeTimeoutNetworkEventControls);
  emberAfRemoveFromCurrentAppTasks(EMBER_AF_WAITING_FOR_PARTNER_LINK_KEY_EXCHANGE);

  if (state->callback != NULL) {
    (*state->callback)(success);
  }
}

static EmberStatus validateKeyRequest(EmberEUI64 partner)
{
  EmberKeyStruct keyStruct;

  // Partner link key requests are not valid for the trust center because it
  // already has a link key with all nodes on the network.
  if (emberAfGetNodeId() == EMBER_TRUST_CENTER_NODE_ID) {
    return EMBER_INVALID_CALL;
  }

  // We must have an authorized trust center link key before we are able to
  // process link key exchanges with other nodes.
  if (emberGetKey(EMBER_TRUST_CENTER_LINK_KEY, &keyStruct) != EMBER_SUCCESS
      || !(keyStruct.bitmask & EMBER_KEY_IS_AUTHORIZED)) {
    return EMBER_KEY_NOT_AUTHORIZED;
  }

  // We need an existing entry or an empty entry in the key table to process a
  // partner link key exchange.
  if (emberFindKeyTableEntry(partner, TRUE) == 0xFF
      && emberFindKeyTableEntry((int8u*)emberAfNullEui64, TRUE) == 0xFF) {
    return EMBER_TABLE_FULL;
  }

  return EMBER_SUCCESS;
}
