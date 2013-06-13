// *****************************************************************************
// * network-find.c
// *
// * Routines for finding and joining any viable network via scanning, rather
// * than joining a specific network. 
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/common/form-and-join.h"
#include "network-find-callback.h"

//------------------------------------------------------------------------------
// Forward Declaration

//------------------------------------------------------------------------------
// Globals

enum {
  NETWORK_FIND_NONE,
  NETWORK_FIND_FORM,
  NETWORK_FIND_JOIN,
  NETWORK_FIND_WAIT,
};
#if defined(EMBER_SCRIPTED_TEST)
  #define HIDDEN 
#else
  #define HIDDEN static
#endif

HIDDEN int8u state = NETWORK_FIND_NONE;

#if defined(EMBER_SCRIPTED_TEST)
  #define EMBER_AF_PLUGIN_NETWORK_FIND_DURATION 5
  extern int32u testFrameworkChannelMask;
  #define CHANNEL_MASK testFrameworkChannelMask

#elif defined(EMBER_AF_PLUGIN_TEST_HARNESS)
  const int32u testHarnessOriginalChannelMask = EMBER_AF_PLUGIN_NETWORK_FIND_CHANNEL_MASK;
  int32u testHarnessChannelMask = EMBER_AF_PLUGIN_NETWORK_FIND_CHANNEL_MASK;
  #define CHANNEL_MASK testHarnessChannelMask

#else
  #define CHANNEL_MASK EMBER_AF_PLUGIN_NETWORK_FIND_CHANNEL_MASK

#endif

#ifdef  EMBER_AF_PLUGIN_NETWORK_FIND_RADIO_TX_CALLBACK
  #define GET_RADIO_TX_POWER(channel) emberAfPluginNetworkFindGetRadioPowerForChannelCallback(channel)
#else
  #define GET_RADIO_TX_POWER(channel) EMBER_AF_PLUGIN_NETWORK_FIND_RADIO_TX_POWER
#endif

static int8u extendedPanIds[EMBER_SUPPORTED_NETWORKS][EXTENDED_PAN_ID_SIZE];

EmberEventControl emberAfPluginNetworkFindTickEventControl;

//------------------------------------------------------------------------------

void emberAfPluginNetworkFindInitCallback(void)
{
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = EMBER_AF_PLUGIN_NETWORK_FIND_EXTENDED_PAN_ID;
  int8u i;
  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    MEMCOPY(extendedPanIds[i], extendedPanId, EXTENDED_PAN_ID_SIZE);
  }
}

void emberAfUnusedPanIdFoundCallback(EmberPanId panId, int8u channel)
{
  EmberNetworkParameters networkParams;
  EmberStatus status;

  MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
  emberAfGetFormAndJoinExtendedPanIdCallback(networkParams.extendedPanId);
  networkParams.panId = panId;
  networkParams.radioChannel = channel;
  networkParams.radioTxPower = GET_RADIO_TX_POWER(channel);

  status = emberAfFormNetwork(&networkParams);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p error 0x%x %p", "Form", 
                      status,
                      "aborting");
    emberAfAppFlush();
    emberAfScanErrorCallback(status);
  }
}

void emberAfJoinableNetworkFoundCallback(EmberZigbeeNetwork *networkFound,
                                         int8u lqi,
                                         int8s rssi)
{
  EmberStatus status = EMBER_ERR_FATAL;

  // NOTE: It's not necessary to check the networkFound->extendedPanId here
  // because the form-and-join utilities ensure this handler is only called
  // when the beacon in the found network has the same EPID as what we asked
  // for when we initiated the scan.  If the scan was requested for the EPID of
  // all zeroes, that's a special case that means any network is OK.  Either
  // way we can trust that it's OK to try joining the network params found in
  // the beacon.

  if (emberAfPluginNetworkFindJoinCallback(networkFound, lqi, rssi)) {
    // Now construct the network parameters to join
    EmberNetworkParameters networkParams;
    MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
    MEMCOPY(networkParams.extendedPanId,
            networkFound->extendedPanId,
            EXTENDED_PAN_ID_SIZE);
    networkParams.panId = networkFound->panId;
    networkParams.radioChannel = networkFound->channel;
    networkParams.radioTxPower = GET_RADIO_TX_POWER(networkFound->channel);

    emberAfAppPrintln("Nwk found, ch %d, panId 0x%2X, joining",
                      networkFound->channel,
                      networkFound->panId);

    status = emberAfJoinNetwork(&networkParams);
  }

  // Note that if the application wants to skip this network or if the join
  // fails, we can't continue the scan from here (by calling
  // emberScanForNextJoinableNetwork) because that's the function that called
  // this handler in the first place, and we don't want to recurse.
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p error 0x%x, %p", 
                      "Join", 
                      status,
                      "aborting");
    emberAfAppFlush();
    emberEventControlSetActive(emberAfPluginNetworkFindTickEventControl);
  }
}

void emberAfPluginNetworkFindTickEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginNetworkFindTickEventControl);
  if (state == NETWORK_FIND_JOIN) {
    // If the tick fires while we're searching for a joinable network, it means
    // we need to keep searching.  This can happen if the join fails or if the
    // application decided to leave the network because it was the wrong
    // network.
    emberAfAppPrintln("Continue %p search", "joinable network");
    emberScanForNextJoinableNetwork();
  } else {
    // In all other cases, we're done and can clean up.
    state = NETWORK_FIND_NONE;
    emberAfAppPrintln("Network find complete");
    emberFormAndJoinCleanup(EMBER_SUCCESS);
    emberAfPluginNetworkFindFinishedCallback(EMBER_SUCCESS);
  }
}

void emberAfScanErrorCallback(EmberStatus status)
{
  if (status == EMBER_NO_BEACONS) {
    emberAfCorePrintln("%p and join scan done", "Form");
  } else {
    emberAfCorePrintln("%p error 0x%x", "Scan", status);
  }
  emberAfCoreFlush();
  state = NETWORK_FIND_NONE;
  emberAfAppPrintln("%p (scan error).",
                    "Network find complete");
  emberAfPluginNetworkFindFinishedCallback(status);
}

EmberStatus emberAfFindUnusedPanIdAndFormCallback(void)
{
  EmberStatus status = EMBER_INVALID_CALL;
#ifdef EMBER_AF_HAS_COORDINATOR_NETWORK
  if (state != NETWORK_FIND_NONE) {
    emberAfAppPrintln("%pForm and join ongoing", "Error: ");
    return EMBER_INVALID_CALL;
  }

  status = emberScanForUnusedPanId(CHANNEL_MASK,
                                   EMBER_AF_PLUGIN_NETWORK_FIND_DURATION);
  if (status == EMBER_SUCCESS) {
    state = NETWORK_FIND_FORM;
  }
#endif
  return status;
}

EmberStatus emberAfStartSearchForJoinableNetworkCallback(void)
{
  EmberStatus status;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE];

  if (state != NETWORK_FIND_NONE) {
    emberAfAppPrintln("%pForm and join ongoing", "Error: ");
    return EMBER_INVALID_CALL;
  }

  emberAfAppPrint("Search for %p\nChannels: ", "joinable network");
  emberAfAppDebugExec(emberAfPrintChannelListFromMask(CHANNEL_MASK));
  emberAfAppPrintln("");

  emberAfGetFormAndJoinExtendedPanIdCallback(extendedPanId);
  status = emberScanForJoinableNetwork(CHANNEL_MASK, extendedPanId);
  if (status == EMBER_SUCCESS) {
    state = NETWORK_FIND_JOIN;
  }
  return status;
}

void emberAfPluginNetworkFindStackStatusCallback(EmberStatus status)
{
  int8u delayMinutes = MAX_INT8U_VALUE;
  if (status == EMBER_NETWORK_UP) {
    // If we had been searching for an unused network so that we could form,
    // we're done.  If we were searching for a joinable network and have
    // successfully joined, we give the application some time to determine if
    // this is the correct network.  If so, we'll eventually time out and clean
    // up the state machine.  If not, the application will tell the stack to
    // leave, we'll get an EMBER_NETWORK_DOWN, and we'll continue searching.
    if (state == NETWORK_FIND_FORM) {
      delayMinutes = 0;
    } else if (state == NETWORK_FIND_JOIN) {
      state = NETWORK_FIND_WAIT;
      delayMinutes = EMBER_AF_PLUGIN_NETWORK_FIND_JOINABLE_SCAN_TIMEOUT_MINUTES;
    }
  } else if (NETWORK_FIND_JOIN <= state) {
    // If we get something other than EMBER_NETWORK_UP while trying to join or
    // while waiting for the application to determine if this is the right
    // network, we need to continue searching for a joinable network.
    state = NETWORK_FIND_JOIN;
    delayMinutes = 0;
  }

  if (delayMinutes == 0) {
    emberAfPluginNetworkFindTickEventHandler();
  } else if (delayMinutes != MAX_INT8U_VALUE) {
    emberEventControlSetDelayMinutes(emberAfPluginNetworkFindTickEventControl,
                                     delayMinutes);
  }
}

void emberAfGetFormAndJoinExtendedPanIdCallback(int8u *resultLocation)
{
  int8u networkIndex = emberGetCurrentNetwork();
  MEMCOPY(resultLocation, extendedPanIds[networkIndex], EXTENDED_PAN_ID_SIZE);
}

void emberAfSetFormAndJoinExtendedPanIdCallback(const int8u *extendedPanId)
{
  int8u networkIndex = emberGetCurrentNetwork();
  MEMCOPY(extendedPanIds[networkIndex], extendedPanId, EXTENDED_PAN_ID_SIZE);
}
