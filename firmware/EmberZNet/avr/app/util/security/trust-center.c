// *******************************************************************
//  trust-center.c
//
//  functions for manipulating security for Trust Center nodes using
//  the commercial security library.
//
//  The Trust Center operates in two basic modes:  allowing joins or  
//  allowing rejoins.  A Trust Center cannot know whether the device
//  is joining insecurily or rejoining insecurely, so it is up to the Trust 
//  Center to decide out what to do based on its internal state.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

//------------------------------------------------------------------------------
// INCLUDES

#include PLATFORM_HEADER //compiler/micro specifics, types

#if defined EZSP_HOST
  #include "stack/include/ember-types.h"
  #include "stack/include/error.h"

  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/ezsp-utils.h"
  #include "app/util/ezsp/serial-interface.h"

#else // Stack App
  #include "stack/include/ember.h"
#endif

#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "app/util/security/security.h"

#if !defined APP_SERIAL
  #define APP_SERIAL 1
#endif

//------------------------------------------------------------------------------
// GLOBALS

static boolean trustCenterAllowJoins = FALSE;
static boolean trustCenterUsePreconfiguredKey = TRUE;

#if defined EZSP_HOST
  static boolean generateRandomKey(EmberKeyData* result);
#else
  #define generateRandomKey(result) \
    (EMBER_SUCCESS == emberGenerateRandomKey(result))
#endif

//------------------------------------------------------------------------------
// FUNCTIONS

static EmberStatus permitRequestingTrustCenterLinkKey(boolean allow);

//------------------------------------------------------------------------------

boolean trustCenterInit(EmberKeyData* preconfiguredKey, 
                        EmberKeyData* networkKey)
{
  EmberInitialSecurityState state;

  trustCenterUsePreconfiguredKey = ( preconfiguredKey != NULL );

  if ( trustCenterUsePreconfiguredKey ) {
    MEMCOPY(emberKeyContents(&state.preconfiguredKey), 
            preconfiguredKey,
            EMBER_ENCRYPTION_KEY_SIZE);
    // When using pre-configured TC link keys, devices are not allowed to
    // request TC link keys.  Otherwise it exposes a security hole.
    if ( EMBER_SUCCESS != permitRequestingTrustCenterLinkKey(FALSE)) {
      emberSerialPrintfLine(APP_SERIAL, 
                            "Failed to set policy for requesting TC link keys.");
      return FALSE;
    }
  } else {
    if ( ! generateRandomKey(&(state.preconfiguredKey) )) {
      emberSerialPrintf(APP_SERIAL, "Failed to generate random link key.\r\n");
      return FALSE;
    }
  }

  // The network key should be randomly generated to minimize the risk
  // where a network key obtained from one network can be used in another.
  // This library supports setting a particular (not random) network key.
  if ( networkKey == NULL) {
    if ( ! generateRandomKey(&(state.networkKey)) ) {
      emberSerialPrintf(APP_SERIAL, 
                        "Failed to generate random NWK key.\r\n");
      return FALSE;
    }
  } else {
    MEMCOPY(emberKeyContents(&state.networkKey), 
            networkKey,EMBER_ENCRYPTION_KEY_SIZE);
  }
  // EMBER_HAVE_PRECONFIGURED_KEY is always set on the TC regardless of whether
  // the Trust Center is expecting the device to have a preconfigured key.
  // This is the value for the Trust Center Link Key.
  state.bitmask = ( EMBER_HAVE_PRECONFIGURED_KEY  
                    | EMBER_STANDARD_SECURITY_MODE
                    | EMBER_GLOBAL_LINK_KEY
                    | EMBER_HAVE_NETWORK_KEY );
  state.networkKeySequenceNumber = 0;

  return (EMBER_SUCCESS == emberSetInitialSecurityState(&state));
}

//------------------------------------------------------------------------------

void trustCenterPermitJoins(boolean allow)
{
  trustCenterAllowJoins = allow;

#if defined EZSP_HOST
  ezspSetPolicy(EZSP_TRUST_CENTER_POLICY,
                (allow
                 ? ( trustCenterUsePreconfiguredKey
                     ? EZSP_ALLOW_PRECONFIGURED_KEY_JOINS
                     : EZSP_ALLOW_JOINS )
                 : EZSP_ALLOW_REJOINS_ONLY));
#endif

  if (!trustCenterUsePreconfiguredKey) {
    permitRequestingTrustCenterLinkKey(allow);
  }

  if ( ! trustCenterAllowJoins )
    emberSerialPrintf(APP_SERIAL, "Trust Center no longer allowing joins.\r\n");
}

//------------------------------------------------------------------------------

boolean trustCenterIsPermittingJoins(void)
{
  return trustCenterAllowJoins;
}

//------------------------------------------------------------------------------

#if !defined EZSP_HOST
EmberJoinDecision emberTrustCenterJoinHandler(EmberNodeId newNodeId,
                                              EmberEUI64 newNodeEui64,
                                              EmberDeviceUpdate status,
                                              EmberNodeId parentOfNewNode)
{
  if ( status == EMBER_DEVICE_LEFT ) {
    return EMBER_NO_ACTION;

  } else if ( status == EMBER_STANDARD_SECURITY_SECURED_REJOIN ) {
    // MAC Encryption is no longer supported by Zigbee.  Therefore this means
    // the device rejoined securely and has the Network Key.  
    
    return EMBER_NO_ACTION;

  } else if ( trustCenterAllowJoins ) {
    // If we are using a preconfigured Link Key the Network Key is sent
    // APS encrypted using the Link Key.
    // If we are not using a preconfigured link key, then both
    // the Link and the Network Key are sent in the clear to the joining device.
    return (trustCenterUsePreconfiguredKey 
            ? EMBER_USE_PRECONFIGURED_KEY
            : EMBER_SEND_KEY_IN_THE_CLEAR );
  }

  // Device rejoined insecurely.  Send it the updated Network Key
  // encrypted with the Link Key.
  return EMBER_USE_PRECONFIGURED_KEY;
}
#endif // !defined EZSP_HOST

//------------------------------------------------------------------------------

#if defined EZSP_HOST
static EmberStatus permitRequestingTrustCenterLinkKey(boolean allow)
{
  return ezspSetPolicy(EZSP_TC_KEY_REQUEST_POLICY,
                       (allow
                        ? EZSP_ALLOW_TC_KEY_REQUESTS
                        : EZSP_DENY_TC_KEY_REQUESTS));
}

#else // EM250

static EmberStatus permitRequestingTrustCenterLinkKey(boolean allow)
{
  emberTrustCenterLinkKeyRequestPolicy = (allow
                                          ? EMBER_ALLOW_KEY_REQUESTS
                                          : EMBER_DENY_KEY_REQUESTS);
  return EMBER_SUCCESS;
}

#endif

//------------------------------------------------------------------------------

#if defined EZSP_HOST
static boolean generateRandomKey(EmberKeyData* result)
{
  int16u data;
  int8u* keyPtr = emberKeyContents(result);
  int8u i;

  // Since our EZSP command only generates a random 16-bit number,
  // we must call it repeatedly to get a 128-bit random number.

  for ( i = 0; i < 8; i++ ) {
    EmberStatus status = ezspGetRandomNumber(&data);

    if ( status != EMBER_SUCCESS ) {
      return FALSE;
    }

    emberStoreLowHighInt16u(keyPtr, data);
    keyPtr+=2;
  }

  return TRUE;
}
#endif // defined EZSP_HOST
