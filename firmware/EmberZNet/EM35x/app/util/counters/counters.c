// File: counters.c
//
// Description: implements emberCounterHandler() and keeps a tally
// of the events reported by the stack.  The application must define
// EMBER_APPLICATION_HAS_COUNTER_HANDLER in its configuration header
// to use this file.
//
// Author(s): Matteo Paris, matteo@ember.com
//
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"

#include "counters.h"

int16u emberCounters[EMBER_COUNTER_TYPE_COUNT];

#if !defined(EMBER_MULTI_NETWORK_STRIPPED)
int16u emberMultiNetworkCounters[EMBER_SUPPORTED_NETWORKS]
                                [MULTI_NETWORK_COUNTER_TYPE_COUNT];
static int8u getMultiNetworkCounterIndex(EmberCounterType type);
static void multiNetworkCounterHandler(EmberCounterType type, int8u data);
#endif // EMBER_MULTI_NETWORK_STRIPPED

// Implement the stack callback by simply tallying up the counts.
void emberCounterHandler(EmberCounterType type, int8u data)
{
  if (emberCounters[type] < 0xFFFF)
    emberCounters[type] += 1;
  
  // TODO: we should avoid wrapping around here.
  // The retry counts need to be handled as a separate case.
  if (EMBER_COUNTER_MAC_TX_UNICAST_SUCCESS == type
      || EMBER_COUNTER_MAC_TX_UNICAST_FAILED == type)
    emberCounters[EMBER_COUNTER_MAC_TX_UNICAST_RETRY] += data;
  else if (EMBER_COUNTER_APS_DATA_TX_UNICAST_SUCCESS == type
           || EMBER_COUNTER_APS_DATA_TX_UNICAST_FAILED == type)
    emberCounters[EMBER_COUNTER_APS_DATA_TX_UNICAST_RETRY] += data;    
  else if (EMBER_COUNTER_PHY_TO_MAC_QUEUE_LIMIT_REACHED == type) {
    emberCounters[type] += data;
  }

#if !defined(EMBER_MULTI_NETWORK_STRIPPED)
  multiNetworkCounterHandler(type, data);
#endif // EMBER_MULTI_NETWORK_STRIPPED
}

void emberClearCounters(void)
{
  MEMSET(emberCounters, 0, sizeof(emberCounters));
#if !defined(EMBER_MULTI_NETWORK_STRIPPED)
  MEMSET(emberMultiNetworkCounters, 0, sizeof(emberMultiNetworkCounters));
#endif // EMBER_MULTI_NETWORK_STRIPPED
}


/*******************************************************************************
 * Multi-network counters support
 *
 * Some of the counters are per-network. Some of them are implicitly not
 * per-network because of the limited multi-network support. i.e., a node can be
 * coordinator/router/end device on just one network. The per-network counters
 * are defined in a table. The per-network counters are stored in a separate
 * two-dimensional array. We keep writing the multi-network counters also in the
 * usual single-network counters array.
 ******************************************************************************/

#if !defined(EMBER_MULTI_NETWORK_STRIPPED)

extern int8u emSupportedNetworks;

static PGM EmberCounterType multiNetworkCounterTable[] = {
    EMBER_COUNTER_MAC_RX_BROADCAST,
    EMBER_COUNTER_MAC_TX_BROADCAST,
    EMBER_COUNTER_MAC_RX_UNICAST,
    EMBER_COUNTER_MAC_TX_UNICAST_SUCCESS,
    EMBER_COUNTER_MAC_TX_UNICAST_RETRY,
    EMBER_COUNTER_MAC_TX_UNICAST_FAILED,
    EMBER_COUNTER_APS_DATA_RX_BROADCAST,
    EMBER_COUNTER_APS_DATA_TX_BROADCAST,
    EMBER_COUNTER_APS_DATA_RX_UNICAST,
    EMBER_COUNTER_APS_DATA_TX_UNICAST_SUCCESS,
    EMBER_COUNTER_APS_DATA_TX_UNICAST_RETRY,
    EMBER_COUNTER_APS_DATA_TX_UNICAST_FAILED,
    EMBER_COUNTER_NWK_FRAME_COUNTER_FAILURE,
    EMBER_COUNTER_APS_FRAME_COUNTER_FAILURE,
    EMBER_COUNTER_APS_LINK_KEY_NOT_AUTHORIZED,
    EMBER_COUNTER_NWK_DECRYPTION_FAILURE,
    EMBER_COUNTER_APS_DECRYPTION_FAILURE,
};

static int8u getMultiNetworkCounterIndex(EmberCounterType type)
{
  int8u i;
  for(i=0; i<MULTI_NETWORK_COUNTER_TYPE_COUNT; i++) {
    if (multiNetworkCounterTable[i] == type)
      return i;
  }
  return 0xFF;
}

static void multiNetworkCounterHandler(EmberCounterType type, int8u data)
{
  int8u counterIndex = getMultiNetworkCounterIndex(type);
  // This function is always called in a callback context emberCounterHandler().
  int8u nwkIndex = emberGetCallbackNetwork();
  assert(nwkIndex < emSupportedNetworks);

  // Not a multi-network counter, nothing to do.
  if (counterIndex == 0xFF)
    return;

  if (emberMultiNetworkCounters[nwkIndex][counterIndex] < 0xFFFF)
    emberMultiNetworkCounters[nwkIndex][counterIndex] += 1;

  // TODO: we should avoid wrapping around here
  if (EMBER_COUNTER_MAC_TX_UNICAST_SUCCESS == type
       || EMBER_COUNTER_MAC_TX_UNICAST_FAILED == type)
    emberMultiNetworkCounters[nwkIndex][EMBER_COUNTER_MAC_TX_UNICAST_RETRY]
                                        += data;
  else if (EMBER_COUNTER_APS_DATA_TX_UNICAST_SUCCESS == type
      || EMBER_COUNTER_APS_DATA_TX_UNICAST_FAILED == type) {
    emberMultiNetworkCounters[nwkIndex][EMBER_COUNTER_APS_DATA_TX_UNICAST_RETRY]
                                        += data;
  }
}

#endif // EMBER_MULTI_NETWORK_STRIPPED
