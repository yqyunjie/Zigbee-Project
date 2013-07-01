// *******************************************************************
// * form-and-join3_2.c
// * 
// * Utilities for forming and joining networks.
// * See form-and-join3_2.h for a description of the exported interface.
// *
// * Deprecated and will disappear in a future release.
// * Use form-and-join.c instead.
// *
// * Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#define __FORM_AND_JOIN_C__

#include "stack/include/ember.h"
#include "hal/hal.h" 
#include "app/util/serial/serial.h" // Serial utility APIs
#include "form-and-join3_2.h"

//----------------------------------------------------------------
// NOTE: applications that use the scan utilities need to 
// define EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
// within their CONFIGURATION_HEADER
//----------------------------------------------------------------

#define INVALID_CHANNEL 0xFF

// The minimum significant difference between energy scan results.
// Results that differ by less than this are treated as identical.
#define ENERGY_SCAN_FUZZ 25

// Our current reason for scanning.
int8u formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;

// During energy scans we need to keep track of the energy found on each
// channel.  During PAN ID scans we need to keep a list of candidate PAN IDs.
// To save on flash we use the same array for both.  It is sized by the 
// number of channels.  Channels are one byte and PAN IDs two, so we get
// half as many PAN_IDs as channels.  We round up to ensure that we get at
// least one.

#define NUM_PAN_ID_CANDIDATES ((EMBER_NUM_802_15_4_CHANNELS + 1) / 2)

static int16u panIdCandidates[NUM_PAN_ID_CANDIDATES];

#define channelEnergies ((int8u *) panIdCandidates)

// Used for caching network parameters while doing a scan.

static EmberNetworkParameters networkParameters;

// We need to save the node type parameter while scanning
// for a network to join.

static EmberNodeType nodeTypeCache;

// Used for the channel crosstalk workaround.
static int8u linkQualityCache;
static int32u channelMaskCache;

//----------------------------------------------------------------
// macros for enabling or disabling debug print messages for the 
// form-and-join code
//#define SCAN_DEBUG_PRINT_ENABLE

#ifdef SCAN_DEBUG_PRINT_ENABLE
   // define this if printing messages -- note serial port used is 1
   #define SCAN_DEBUG_MSG(x,y) emberSerialPrintf(1,x,y); emberSerialWaitSend(1)
   #define SCAN_DEBUG(x) emberSerialPrintf(1,x); emberSerialWaitSend(1)

   #define SCAN_DEBUG_XPAN_PRINT(xpan)  \
      do {                                     \
             emberSerialPrintf(1, "%x%x%x%x%x%x%x%x", \
                xpan[0], xpan[1], xpan[2], xpan[3], \
                xpan[4], xpan[5], xpan[6], xpan[7]); \
             emberSerialWaitSend(1);  \
         } while (FALSE)
#else
   // define this to turn off printing messages
   #define SCAN_DEBUG_MSG(x, ...) do { ; } while(FALSE)
   #define SCAN_DEBUG(x) do { ; } while(FALSE)
   #define SCAN_DEBUG_XPAN_PRINT(x) do { ; } while(FALSE)
#endif

//----------------------------------------------------------------
// Forward declarations for the benefit of the compiler.

static void startPanIdScan(void);

//----------------------------------------------------------------
// Form a network.  This performs the following actions:
//   1. Do an energy scan on the indicated channels and randomly choose
//      one from amongst those with the least average energy.
//   2. Randomly pick a PAN ID that does not appear during an active scan
//      on the chosen channel.
//   3. Randomly pick an Extended PAN ID if the Extended PAN ID passed in
//      is "0" or null.
//   4. Form a network using the chosen channel, PAN ID, and Extended PAN ID.
// If any errors occur they status code is passed to scanError() and no
// network is formed.  Success is indicated by calling emberStackStatusHandler()
// with the EMBER_NETWORK_UP status value.
//
// This returns after starting the first scan.  Later scans and the actual
// emberFormNetwork() call are initiated by emberScanCompleteHandler() as
// each scan finishes.

void formZigbeeNetwork3_2(int32u channelMask, 
                          int8s radioTxPower,
                          int8u* extendedPanIdDesired)
{
  EmberStatus status;
  int8u i;

  networkParameters.radioTxPower = radioTxPower;
  networkParameters.radioChannel = INVALID_CHANNEL;

  // null extended PAN ID means use random
  if (extendedPanIdDesired == NULL) {
    MEMSET(networkParameters.extendedPanId, 0, EXTENDED_PAN_ID_SIZE);
  } else {
    MEMCOPY(networkParameters.extendedPanId, extendedPanIdDesired, 
           EXTENDED_PAN_ID_SIZE);
  }
  
  assert(channelMask != 0);

  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    channelEnergies[i] = 0xFF;
  }

  formAndJoinScanType = FORM_AND_JOIN_ENERGY_SCAN;
  status = emberStartScan(EMBER_ENERGY_SCAN,
                          channelMask,
                          5);             // about half a second per channel
  SCAN_DEBUG_MSG("SCAN: start energy scan, status 0x%x\r\n", status);
 
  if (status != EMBER_SUCCESS) {
    formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
    scanError(status);
    return;
  }
}

// Pick a channel from among those with the lowest energy and then look for
// a PAN ID not in use on that channel.
// 
// The energy scans are not particularly accurate, especially as we don't run
// them for very long, so we add in some slop to the measurements and then pick
// a random channel from the least noisy ones.  This avoids having several
// coordinators pick the same slightly quieter channel.

static void energyScanComplete(void)
{
  int8u cutoff = 0xFF;
  int8u candidateCount = 0;
  int8u channelIndex;
  int8u i;

  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    if (channelEnergies[i] < cutoff - ENERGY_SCAN_FUZZ) {
      cutoff = channelEnergies[i] + ENERGY_SCAN_FUZZ;
    }
  }

  // There must be at least one channel, as checked by the assert() above,
  // so there will be at least one candidate.
  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    if (channelEnergies[i] < cutoff) {
      candidateCount += 1;
    }
  }

  channelIndex = halCommonGetRandom() % candidateCount;

  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    if (channelEnergies[i] < cutoff) {
      if (channelIndex == 0) {
        networkParameters.radioChannel = EMBER_MIN_802_15_4_CHANNEL_NUMBER + i;
        break;
      }
      channelIndex -= 1;
    }
  }
  assert(networkParameters.radioChannel != INVALID_CHANNEL);  
  startPanIdScan();
}

// PAN IDs can be 0..0xFFFE.  We pick some trial candidates and then do a scan
// to find one that is not in use.

static void startPanIdScan(void)
{
  int8u i;
  EmberStatus status;

  for (i = 0; i < NUM_PAN_ID_CANDIDATES;) {
    int16u panId = halCommonGetRandom() & 0xFFFF;
    if (panId != 0xFFFF) {
      panIdCandidates[i] = panId;
      i++;
    }
  }

  formAndJoinScanType = FORM_AND_JOIN_PAN_ID_SCAN;
  status = emberStartScan(EMBER_ACTIVE_SCAN,
                          BIT32(networkParameters.radioChannel),
                          9);           // about eight seconds
  
  SCAN_DEBUG_MSG("SCAN: start active scan, status 0x%x\r\n", status);
  if (status != EMBER_SUCCESS) {
    formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
    scanError(status);
    return;
  }
}

// Form a network using one of the unused PAN IDs.  If we got unlucky we
// pick some more and try again.

static void panIdScanComplete(void)
{
  int8u i;

  for (i = 0; i < NUM_PAN_ID_CANDIDATES; i++) {
    if (panIdCandidates[i] != 0xFFFF) {
      EmberStatus status;

      networkParameters.panId = panIdCandidates[i];
      status = emberFormNetwork(&networkParameters);
      SCAN_DEBUG_MSG("SCAN: attempt to form network, status 0x%x", status);
      if (status != EMBER_SUCCESS)
        scanError(status);
      return;
    }
  }
  startPanIdScan();     // Start over with new candidates.
}

//----------------------------------------------------------------
// Join a network.  This performs the following actions:
//   1. Do an active scan to find a network that:
//       -- uses our stack profile
//       -- currently allows new nodes to join
//       -- matches the Extended PAN ID passed in or allows any if the
//          Extended PAN ID passed in is all zeroes.
//   2. If the chosen network is on a channel that is susceptible to
//      crosstalk (from the channel 12 up or down), scan the other
//      channel looking for the same extended pan id.  If found, compare
//      beacon link quality to decide which channel is correct.
//   3. Join the chosen network.
// If any errors occur they status code is passed to scanError() and no
// network is formed.  Success is indicated by calling emberStackStatusHandler()
// with the EMBER_NETWORK_UP status value.
//
// This returns after starting the first scan.  The actual emberJoinNetwork()
// is initiated by emberNetworkFoundHandler().

void joinZigbeeNetwork3_2(EmberNodeType nodeType,
                          int32u channelMask,
                          int8s radioTxPower,
                          int8u* extendedPanIdDesired)
{
  EmberStatus status;

  networkParameters.radioTxPower = radioTxPower;
  networkParameters.panId = 0xFFFF;             // none found yet
  nodeTypeCache = nodeType;                     // stash these for later use
  channelMaskCache = channelMask;

  // save the extended PAN ID to compare to the beacons. An extended PAN ID
  // of all zeroes means "use any"
  if (extendedPanIdDesired == NULL) {
    MEMSET(networkParameters.extendedPanId, 0, EXTENDED_PAN_ID_SIZE);
  } else {
    MEMCOPY(networkParameters.extendedPanId, extendedPanIdDesired, 
           EXTENDED_PAN_ID_SIZE);
  }

  formAndJoinScanType = FORM_AND_JOIN_JOINABLE_SCAN;
  status = emberStartScan(EMBER_ACTIVE_SCAN,
                          channelMask,
                          3);           // 138 milliseconds

  SCAN_DEBUG_MSG("SCAN: start active scan, status 0x%x\r\n", status);
  if (status != EMBER_SUCCESS) {
    formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
    scanError(status);
    return;
  }
}

static void callJoinNetwork(void)
{
  EmberStatus status = emberJoinNetwork(nodeTypeCache,
                                        &networkParameters);
  SCAN_DEBUG_MSG("SCAN: join status 0x%x\r\n", status);
  if (status != EMBER_SUCCESS) {
    scanError(status);
  }
}

// This state is a workaround for the em250 channel crosstalk issue
// in which traffic from up or down 12 channels from ours spills over
// into our channel at certain power levels.  The workaround is to 
// check the offset channel prior to attempting a join, looking for
// the same extended pan id.  If it is found there too, we assume
// that the channel with the best link quality is the correct one.
// If the network is on the offset channel but that channel is not
// in the supplied channel mask, we don't join.

static void startCrosstalkScan(void)
{
  EmberStatus status;
  int8u crossChannel = (networkParameters.radioChannel 
                        + (networkParameters.radioChannel < 15
                           ? 12
                           : -12));
  formAndJoinScanType = FORM_AND_JOIN_CROSSTALK_SCAN;
  status = emberStartScan(EMBER_ACTIVE_SCAN,
                          BIT32(crossChannel),
                          3);        // 138 milliseconds
  SCAN_DEBUG_MSG("SCAN: start crosstalk scan, status 0x%x\r\n", status);
  if (status != EMBER_SUCCESS) {
    formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
    scanError(status);
    return;
  }
}

static void crosstalkScanComplete(void)
{
  if (BIT32(networkParameters.radioChannel) & channelMaskCache) {
    callJoinNetwork();
  } else {
    // The joinable network that was found was crosstalk from
    // a channel not listed in the supplied channel mask, so we
    // can't join to it.  (The channel is not really invalid.)
    scanError(EMBER_PHY_INVALID_CHANNEL);
  }
}

static void joinableScanComplete(void)
{
  if (networkParameters.panId != 0xFFFF) {
    if (networkParameters.radioChannel < 15
        || networkParameters.radioChannel > 22) {
      startCrosstalkScan();
    } else {
      callJoinNetwork();
    }
  } else {
    scanError(EMBER_JOIN_FAILED);
  }
}

boolean isArrayZero(int8u* array, int8u size)
{
  int8u i;
  for (i=0; i<size; i++) {
    if (array[i] != 0) {
      return FALSE;
    }
  }
  return TRUE;
}

//----------------------------------------------------------------
// Ember callbacks

void emberScanCompleteHandler(int8u channel, EmberStatus status)
{
  int8u completedScanType = formAndJoinScanType;
  formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
  if (status != EMBER_SUCCESS)
    scanError(status);

  switch (completedScanType) {
  case FORM_AND_JOIN_ENERGY_SCAN:   energyScanComplete();   break;
  case FORM_AND_JOIN_PAN_ID_SCAN:   panIdScanComplete();    break;
  case FORM_AND_JOIN_JOINABLE_SCAN: joinableScanComplete(); break;
  case FORM_AND_JOIN_CROSSTALK_SCAN: crosstalkScanComplete(); break;
  default:       assert(FALSE);
  }
}

// We are either looking for PAN IDs or for joinable networks.  In the first
// case we just check the found PAN ID against our list of candidates.  In
// the second case we stop the scan and try to join the network.

void emberNetworkFoundHandler(EmberZigbeeNetwork *networkFound)
{
  int8u lqi;
  emberGetLastHopLqi(&lqi);
  SCAN_DEBUG_MSG("SCAN: nwk found ch %d, ", networkFound->channel);
  SCAN_DEBUG_MSG("panID 0x%2x, xpan: ", networkFound->panId);
  SCAN_DEBUG_XPAN_PRINT(networkFound->extendedPanId);
  SCAN_DEBUG_MSG(", lqi %d", lqi);
  
  switch (formAndJoinScanType) {
  case FORM_AND_JOIN_PAN_ID_SCAN: {
    int8u i;
    for (i = 0; i < NUM_PAN_ID_CANDIDATES; i++)
      if (panIdCandidates[i] == networkFound->panId)
        panIdCandidates[i] = 0xFFFF;
    break;
  }

  case FORM_AND_JOIN_JOINABLE_SCAN:

    // check for a beacon with permit join on...
    if (networkFound->allowingJoin
        // ...the same stack profile as this application...
        && networkFound->stackProfile == emberStackProfile()
        && (
            // ...and a zero Extended PAN ID, or...
            (isArrayZero(networkParameters.extendedPanId,
                         EXTENDED_PAN_ID_SIZE)) ||
            // ...a matching Extended PAN ID
            (MEMCOMPARE(networkParameters.extendedPanId, 
                        networkFound->extendedPanId,
                        EXTENDED_PAN_ID_SIZE) == 0)
            )
        ) {
      // copy in the extended PAN ID, short PAN ID, and channel we want to use
      MEMCOPY(networkParameters.extendedPanId, 
              networkFound->extendedPanId, 
              EXTENDED_PAN_ID_SIZE);
      networkParameters.panId = networkFound->panId;
      networkParameters.radioChannel = networkFound->channel;
      linkQualityCache = lqi;

      // stop the scan - the emberScanCompleteHandler will 
      // call emberJoinNetwork
      SCAN_DEBUG(": MATCH\r\n"); 
      emberStopScan();
    } else {
      SCAN_DEBUG(": NO match\r\n"); 
    }
    break;
  case FORM_AND_JOIN_CROSSTALK_SCAN:
    if (MEMCOMPARE(networkParameters.extendedPanId,
                   networkFound->extendedPanId,
                   EXTENDED_PAN_ID_SIZE) == 0) {
      SCAN_DEBUG(": MATCH\r\n"); 
      if (linkQualityCache < lqi) {
        networkParameters.radioChannel = networkFound->channel;
      }
      SCAN_DEBUG_MSG("SCAN: crosstalk detected, joining channel %d\r\n", 
                     networkParameters.radioChannel);
      emberStopScan();
    } else {
      SCAN_DEBUG(": NO match\r\n");
    }
    break;
  }
}

// Just remember the result.

void emberEnergyScanResultHandler(int8u channel, int8u maxRssiValue)
{
  SCAN_DEBUG_MSG("SCAN: found energy 0x%x on ", maxRssiValue);
  SCAN_DEBUG_MSG("channel 0x%x\r\n", channel);
  channelEnergies[channel - EMBER_MIN_802_15_4_CHANNEL_NUMBER] = maxRssiValue;
}
