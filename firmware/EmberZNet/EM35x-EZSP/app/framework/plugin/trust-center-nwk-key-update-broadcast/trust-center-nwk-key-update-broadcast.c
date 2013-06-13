// *****************************************************************************
// * trust-center-key-update-broadcast.c
// *
// * Functionality for rolling the network key by broadcasting the key update
// * and the NWK key switch messages.
// * This is the preferred mechanism for Home Automation and another networks
// * without individual link keys per device.
// *
// * The process is pretty straightforward.
// *   1. Broadcast new NWK key (randomly generated)
// *   2. Wait until the broadcast fades out of the network (~20 seconds)
// *   3. Broadcast NWK key switch message
// *
// * Note: This does not PERIODICALLY update the NWK key.  It just manages the
// * process when it is told to initate the key change.  Another software
// * module must determine the policy for periodically initiating this process
// * (such as the 'Trust Center NWK Key Update Periodic' App Framework Plugin).
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"
#include "app/util/concentrator/concentrator.h"

#include "trust-center-nwk-key-update-broadcast.h"

// *****************************************************************************
// Globals

EmberEventControl emberAfPluginTrustCenterNwkKeyUpdateBroadcastMyEventControl;

#define myEvent emberAfPluginTrustCenterNwkKeyUpdateBroadcastMyEventControl

#if defined(EMBER_AF_PLUGIN_TEST_HARNESS)
  // For testing, we need to support a single application that can do
  // unicast AND broadcast key updates.  So we re-map the function name
  // so both can be compiled in without conflict.
  #define emberAfTrustCenterStartNetworkKeyUpdate emberAfTrustCenterStartBroadcastNetworkKeyUpdate
#endif

// *****************************************************************************

void emberAfPluginTrustCenterNwkKeyUpdateBroadcastMyEventHandler(void)
{
  EmberStatus status;
  emberEventControlSetInactive(myEvent);
  status = emberBroadcastNetworkKeySwitch();
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("Failed to broadcast NWK key switch");
  }
  emberAfSecurityPrintln("Sent NWK key switch.");
  emberAfNetworkKeyUpdateCompleteCallback(status);
}

static boolean nextNetworkKeyIsNewer(EmberKeyStruct* nextNwkKey)
{
  EmberKeyStruct currentNwkKey;
  EmberStatus status;

  // It is assumed that the current nwk key has valid data.
  emberGetKey(EMBER_CURRENT_NETWORK_KEY,
              &currentNwkKey);

  status = emberGetKey(EMBER_NEXT_NETWORK_KEY,
                       nextNwkKey);
  if (status != EMBER_SUCCESS 
      || (timeGTorEqualInt8u(currentNwkKey.sequenceNumber,
                             nextNwkKey->sequenceNumber))) {
    return FALSE;
  }

  return TRUE;
}

EmberStatus emberAfTrustCenterStartNetworkKeyUpdate(void)
{
  EmberKeyStruct nextNwkKey;
  EmberStatus status;

  if (emberAfGetNodeId() != EMBER_TRUST_CENTER_NODE_ID
      || emberNetworkState() != EMBER_JOINED_NETWORK
      || myEvent.status != EMBER_EVENT_INACTIVE) {
    return EMBER_INVALID_CALL;
  }

  if (!nextNetworkKeyIsNewer(&nextNwkKey)) {
    // Setting the key to all zeroes tells the stack
    // to randomly generate a new key and use that.
    MEMSET(emberKeyContents(&(nextNwkKey.key)),
           0,
           EMBER_ENCRYPTION_KEY_SIZE);
  }
  
  status = emberBroadcastNextNetworkKey(&(nextNwkKey.key));
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("Failed to broadcast next NWK key");
  } else {
    emberAfSecurityPrintln("Broadcasting next NWK key");
    // The +2 is a fuzz factor
    emberEventControlSetDelayQS(myEvent,
                                EMBER_BROADCAST_TABLE_TIMEOUT_QS + 2); 
  }
  return status;
}
