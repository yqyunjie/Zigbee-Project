// *****************************************************************************
// * trust-center-keepalive.c
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/plugin/smart-energy-registration/smart-energy-registration.h"
#include "trust-center-keepalive.h"

// NOTE: This code is intended only for routers, so we do not consider sleepy
// devices and their hibernating/napping issues.  As per the spec, only routers
// have to keep track of the TC.  Sleepies just keep track of their parent.

//------------------------------------------------------------------------------
// Globals

typedef enum {
  STATE_INITIAL,
  STATE_SEND_KEEPALIVE_SIGNAL,
  STATE_INITIATE_TRUST_CENTER_SEARCH,
} TrustCenterKeepaliveState;
static TrustCenterKeepaliveState states[EMBER_SUPPORTED_NETWORKS];

extern EmberEventControl emberAfPluginTrustCenterKeepaliveTickNetworkEventControls[];

static int8u failures[EMBER_SUPPORTED_NETWORKS];

static boolean waitingForResponse[EMBER_SUPPORTED_NETWORKS];

#define KEEPALIVE_WAIT_TIME_MS (5 * MILLISECOND_TICKS_PER_SECOND)

//------------------------------------------------------------------------------
// Forward Declarations

static void initiateTrustCenterSearch(void);
static void delayUntilNextKeepalive(void);
static void trustCenterKeepaliveStart(void);

//------------------------------------------------------------------------------

void emberAfPluginTrustCenterKeepaliveInitCallback(void)
{
  int8u i;

  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    states[i] = STATE_INITIAL;
    failures[i] = 0;
    waitingForResponse[i] = FALSE;
  }
}
void emberAfTrustCenterKeepaliveAbortCallback(void)
{
  emberAfCorePrintln("Setting trust center keepalive inactive.");
  states[emberGetCurrentNetwork()] = STATE_INITIAL;
  emberAfNetworkEventControlSetInactive(emberAfPluginTrustCenterKeepaliveTickNetworkEventControls);
}

void emberAfTrustCenterKeepaliveUpdateCallback(boolean registrationComplete)
{
  int8u nwkIndex = emberGetCurrentNetwork();

  // Note:  registartion complete does NOT necessarily mean registration went
  // through and completed successfully.  We may have been called after a rejoin
  // failed and the router just came back up on the network.

  if (!registrationComplete) {
    return;
  }

  if (states[nwkIndex] == STATE_INITIAL) {
    trustCenterKeepaliveStart();
    return;

  } else if (states[nwkIndex] == STATE_INITIATE_TRUST_CENTER_SEARCH) {
    EmberKeyStruct keyStruct;
    EmberStatus status = emberGetKey(EMBER_TRUST_CENTER_LINK_KEY,
                                     &keyStruct);
    if (status == EMBER_SUCCESS
        && !(keyStruct.bitmask & EMBER_KEY_IS_AUTHORIZED)) {
      // If the TC link key is not authorized, that means we rejoined
      // to a new TC and key establishment did not complete successfully.
      emberAfCorePrintln("Registration failed after TC change.");
      emberAfCorePrintln("Forcing reboot to forget old network.");
      emberAfCoreFlush();
      halReboot();
    }
    states[nwkIndex] = STATE_SEND_KEEPALIVE_SIGNAL;
    delayUntilNextKeepalive();
  }

  // If the Trust Center's EUI changed and we completed registration,
  // this means that the we have found a new network and must
  // start persistently storing our tokens again.  Everything is a-okay.

  // If this callback was executed without a Trust Center search in progress
  // or after a failed trust center search, the call below has no effect, 
  // because we didn't call emberStopWritingStackTokens() previously.
  emberStartWritingStackTokens();
} 

static void trustCenterKeepaliveStart(void)
{
  if (emberAfGetNodeId() == EMBER_TRUST_CENTER_NODE_ID) {
    // If this code is executing, then the trust center must be alive.
    return;
  }

  failures[emberGetCurrentNetwork()] = 0;
  emAfSendKeepaliveSignal();
}

static void delayUntilNextKeepalive(void)
{
  emberAfNetworkEventControlSetDelay(emberAfPluginTrustCenterKeepaliveTickNetworkEventControls,
                                     EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE_DELAY_INTERVAL);
}

static void messageTimeout(void)
{
  int8u nwkIndex = emberGetCurrentNetwork();

  waitingForResponse[nwkIndex] = FALSE;
  if (failures[nwkIndex] != 255) {
    failures[nwkIndex]++;
  }

  emberAfSecurityPrintln("ERR: Trust center did not acknowledge"
                         " previous keepalive signal");

  if (failures[nwkIndex] >= EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE_FAILURE_LIMIT) {
    emberAfSecurityPrintln("ERR: Keepalive failure limit reached (%d)",
                           EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE_FAILURE_LIMIT);
    initiateTrustCenterSearch();
  } else {
    delayUntilNextKeepalive();
  }
}

void emberAfPluginTrustCenterKeepaliveTickNetworkEventHandler(void)
{
  int8u nwkIndex = emberGetCurrentNetwork();
  emberAfNetworkEventControlSetInactive(emberAfPluginTrustCenterKeepaliveTickNetworkEventControls);

  if (waitingForResponse[nwkIndex]) {
    messageTimeout();
    return;
  }

  switch (states[nwkIndex]) {
    case STATE_SEND_KEEPALIVE_SIGNAL:
      emAfSendKeepaliveSignal();
      break;
    default:
      break;
  }
}

void emAfSendKeepaliveSignal(void)
{
  EmberStatus status;
  int8u attributeIds[] = {
    LOW_BYTE(ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID),
    HIGH_BYTE(ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID),
  };
  int8u sourceEndpoint = emberAfPrimaryEndpointForCurrentNetworkIndex();
  int8u destinationEndpoint = emAfPluginSmartEnergyRegistrationTrustCenterKeyEstablishmentEndpoint();

  // The keepalive is an attempt to read a Key Establishment attribute on the
  // trust center.  In general, APS encryption is not required for Key
  // Establishment commands, but it is required by the spec for the keepalive,
  // so the option is explicitly set here.
  emberAfSecurityPrintln("Sending keepalive signal"
                         " to trust center endpoint 0x%x",
                         destinationEndpoint);
  emberAfFillCommandGlobalClientToServerReadAttributes(ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                                       attributeIds,
                                                       sizeof(attributeIds));

  // It is possible we will retrieve an undefined endpoint (0xFF) if we rebooted
  // and the TC is not around.  Nonetheless we will still use it as the broadcast
  // endpoint in the hopes that the trust center will respond.
  emberAfSetCommandEndpoints(sourceEndpoint, destinationEndpoint);
  emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_ENCRYPTION;
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,
                                     EMBER_TRUST_CENTER_NODE_ID);
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("ERR: Failed to send keepalive signal"
                           " to trust center endpoint 0x%x (0x%x)",
                           destinationEndpoint,
                           status);
  }
  states[emberGetCurrentNetwork()] = STATE_SEND_KEEPALIVE_SIGNAL;
  waitingForResponse[emberGetCurrentNetwork()] = (status == EMBER_SUCCESS);
  emberAfNetworkEventControlSetDelay(emberAfPluginTrustCenterKeepaliveTickNetworkEventControls,
                                     (status == EMBER_SUCCESS
                                      ? KEEPALIVE_WAIT_TIME_MS
                                      : EMBER_AF_PLUGIN_TRUST_CENTER_KEEPALIVE_DELAY_INTERVAL));
}

void emAfPluginTrustCenterKeepaliveReadAttributesResponseCallback(int8u *buffer,
                                                                  int16u bufLen)
{
  int8u nwkIndex = emberGetCallbackNetwork();
  // We don't care about the contents of the response, just that we got one.
  if (emberAfCurrentCommand()->source == EMBER_TRUST_CENTER_NODE_ID
      && states[nwkIndex] == STATE_SEND_KEEPALIVE_SIGNAL) {
    emberAfSecurityPrintln("Trust center acknowledged keepalive signal");
    waitingForResponse[nwkIndex] = FALSE;
    failures[nwkIndex] = 0;
    delayUntilNextKeepalive();
  }
}

static void initiateTrustCenterSearch(void)
{
  int8u nwkIndex = emberGetCurrentNetwork();

  EmberStatus status;
  emberAfSecurityFlush();
  emberAfSecurityPrintln("Initiating trust center search");
  emberAfSecurityFlush();
  status = emberStopWritingStackTokens();
  if (status == EMBER_SUCCESS) {
    status = emberFindAndRejoinNetworkWithReason(FALSE, 
                                                 EMBER_ALL_802_15_4_CHANNELS_MASK,
                                                 EMBER_AF_REJOIN_DUE_TO_TC_KEEPALIVE_FAILURE);
  } else {
    emberAfSecurityPrintln("ERR: Failed to suspend token writing.");
  }

  if (status == EMBER_SUCCESS) {
    states[nwkIndex] = STATE_INITIATE_TRUST_CENTER_SEARCH;
  } else {
    emberAfSecurityPrintln("ERR: Could not initiate TC search (0x%x)", status);
    emberStartWritingStackTokens();
  }
}
