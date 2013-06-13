// *****************************************************************************
// * form-and-join.c
// * 
// * Utilities for forming and joining networks.
// * See form-and-join.h for a description of the exported interface.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              
// **************************************************************************80*

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#define __FORM_AND_JOIN_C__

#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h" 
#include "app/util/serial/serial.h"
#include "form-and-join.h"
#include "form-and-join-adapter.h"

// We can't include ember.h or ezsp.h from this file since it is used
// for both host and node-size libraries.  However the emberStartScan()
// API is identical in both.
extern EmberStatus emberStartScan(int8u scanType, 
                                  int32u channelMask, 
                                  int8u duraiton);

enum {
  FORM_AND_JOIN_NOT_SCANNING,
  FORM_AND_JOIN_NEXT_NETWORK,
  FORM_AND_JOIN_ENERGY_SCAN,      
  FORM_AND_JOIN_PAN_ID_SCAN,      
  FORM_AND_JOIN_JOINABLE_SCAN,    
  FORM_AND_JOIN_DUAL_CHANNEL_SCAN 
};

static int8u formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;

static int8u *dataContents;
#define panIdCandidates ((int16u *) dataContents)
#define channelEnergies (dataContents)

static int8u extendedPanIdCache[8];
static int32u channelMaskCache;
static int8u channelCache;
static boolean ignoreExtendedPanId;
static int8u networkCount;  // The number of NetworkInfo records.

// The dual channel code is only present on the EM250 and EM260.
boolean emberEnableDualChannelScan = TRUE;

// The minimum significant difference between energy scan results.
// Results that differ by less than this are treated as identical.
#define ENERGY_SCAN_FUZZ 25

#define NUM_PAN_ID_CANDIDATES 16  // Must fit into one packet buffer on the node

#define INVALID_PAN_ID 0xFFFF

// ZigBee specifies that active scans have a duration of 3 (138 msec).
// See documentation for emberStartScan in include/network-formation.h
// for more info on duration values.
#define ACTIVE_SCAN_DURATION 3

//------------------------------------------------------------------------------
// Macros for enabling or disabling debug print messages.
// #define SCAN_DEBUG_PRINT_ENABLE

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

#elif defined(EMBER_SCRIPTED_TEST) 
  void debug(const char* format, ...);
  boolean isTestFrameworkDebugOn(void);
  #define SCAN_DEBUG_MSG(x,y) debug(x,y)
  #define SCAN_DEBUG(x) debug(x )
  #define SCAN_DEBUG_XPAN_PRINT(xpan) \
      do {                                     \
             debug("%x%x%x%x%x%x%x%x", \
                xpan[0], xpan[1], xpan[2], xpan[3], \
                xpan[4], xpan[5], xpan[6], xpan[7]); \
         } while (FALSE)

#else
   // define this to turn off printing messages
   #define SCAN_DEBUG_MSG(x, ...) 
   #define SCAN_DEBUG(x) 
   #define SCAN_DEBUG_XPAN_PRINT(x) 
#endif

//------------------------------------------------------------------------------
// Forward declarations for the benefit of the compiler.

static void saveNetwork(EmberZigbeeNetwork *network, int8u lqi, int8s rssi);
static boolean setup(int8u scanType);
static EmberStatus startScan(EmberNetworkScanType type, int32u mask, int8u duration);
static void startPanIdScan(void);
static boolean isArrayZero(int8u* array, int8u size);

//------------------------------------------------------------------------------
// Finding unused PAN ids.

EmberStatus emberScanForUnusedPanId(int32u channelMask, int8u duration)
{
  EmberStatus status = EMBER_ERR_FATAL;
  if (setup(FORM_AND_JOIN_ENERGY_SCAN)) {
    MEMSET(channelEnergies, 0xFF, EMBER_NUM_802_15_4_CHANNELS);
    status = startScan(EMBER_ENERGY_SCAN, channelMask, duration);
  }
  return status;
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

  // There must be at least one channel,
  // so there will be at least one candidate.
  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    if (channelEnergies[i] < cutoff) {
      candidateCount += 1;
    }
  }

  // If for some reason we never got any energy scan results
  // then our candidateCount will be 0.  We want to avoid that case and
  // bail out (since we will do a divide by 0 below)
  if (candidateCount == 0) {
    emberFormAndJoinCleanup(EMBER_ERR_FATAL);
    return;
  }

  channelIndex = halCommonGetRandom() % candidateCount;

  for (i = 0; i < EMBER_NUM_802_15_4_CHANNELS; i++) {
    if (channelEnergies[i] < cutoff) {
      if (channelIndex == 0) {
        channelCache = EMBER_MIN_802_15_4_CHANNEL_NUMBER + i;
        break;
      }
      channelIndex -= 1;
    }
  }

  startPanIdScan();
}

// PAN IDs can be 0..0xFFFE.  We pick some trial candidates and then do a scan
// to find one that is not in use.

static void startPanIdScan(void)
{
  int8u i;

  for (i = 0; i < NUM_PAN_ID_CANDIDATES;) {
    int16u panId = halCommonGetRandom() & 0xFFFF;
    if (panId != 0xFFFF) {
      SCAN_DEBUG_MSG("panIdCandidate: 0x%2X\n", panId);
      panIdCandidates[i] = panId;
      i++;
    }
  }

  formAndJoinScanType = FORM_AND_JOIN_PAN_ID_SCAN;
  startScan(EMBER_ACTIVE_SCAN, BIT32(channelCache), ACTIVE_SCAN_DURATION);
}

// Form a network using one of the unused PAN IDs.  If we got unlucky we
// pick some more and try again.

static void panIdScanComplete(void)
{
  int8u i;

  for (i = 0; i < NUM_PAN_ID_CANDIDATES; i++) {
    EmberStatus status = EMBER_SUCCESS;
    if (panIdCandidates[i] != 0xFFFF) {
      emberUnusedPanIdFoundHandler(panIdCandidates[i], channelCache);
      emberFormAndJoinCleanup(status);
      return;
    }
  }

  // XXX: Do we care this could keep happening forever?
  // In practice there couldn't be as many PAN IDs heard that
  // conflict with ALL our randomly selected set of candidate PANs.
  // But in theory we could get the same random set of numbers
  // (more likely due to a bug) and we could hear the same set of
  // PAN IDs that conflict with our random set.

  startPanIdScan();     // Start over with new candidates.
}

//------------------------------------------------------------------------------
// Finding joinable networks

EmberStatus emberScanForJoinableNetwork(int32u channelMask, int8u* extendedPanId)
{
  if (! setup(FORM_AND_JOIN_NEXT_NETWORK)) {
    return EMBER_INVALID_CALL;
  }
  // Init the channel to 10, gets incremented in call to next joinable network.
  channelCache = EMBER_MIN_802_15_4_CHANNEL_NUMBER - 1;
  channelMaskCache = channelMask;
  if (extendedPanId == NULL
      || isArrayZero(extendedPanId, EXTENDED_PAN_ID_SIZE)) {
    ignoreExtendedPanId = TRUE;
  } else {
    ignoreExtendedPanId = FALSE;
    MEMCOPY(extendedPanIdCache, extendedPanId, EXTENDED_PAN_ID_SIZE);
  }

  return emberScanForNextJoinableNetwork();
}

EmberStatus emberScanForNextJoinableNetwork(void)
{
  if (formAndJoinScanType != FORM_AND_JOIN_NEXT_NETWORK) {
    emberScanErrorHandler(EMBER_INVALID_CALL);
    return EMBER_INVALID_CALL;
  }

  // Check for cached networks first.
  while (networkCount) {
    NetworkInfo *finger = formAndJoinGetNetworkPointer(--networkCount);
    if (finger->network.panId != 0xFFFF) {
      emberJoinableNetworkFoundHandler(&(finger->network), finger->lqi, finger->rssi);
      formAndJoinSetBufferLength(networkCount);
      formAndJoinSetCleanupTimeout();
      return EMBER_SUCCESS;
    }
    formAndJoinSetBufferLength(networkCount);
  }

  // Find the next channel in the mask and start scanning.
  channelCache += 1;
  for (; channelCache <= EMBER_MAX_802_15_4_CHANNEL_NUMBER; channelCache++) {
    int32u bitMask = BIT32(channelCache);
    if (bitMask & channelMaskCache) {
      formAndJoinScanType = FORM_AND_JOIN_JOINABLE_SCAN;
      return startScan(EMBER_ACTIVE_SCAN, bitMask, ACTIVE_SCAN_DURATION);
    }
  }

  // Notify the app we're completely out of networks.
  emberFormAndJoinCleanup(EMBER_NO_BEACONS);
  return EMBER_SUCCESS;
}

//------------------------------------------------------------------------------
// Callbacks

boolean emberFormAndJoinScanCompleteHandler(int8u channel, EmberStatus status)
{
  if (! emberFormAndJoinIsScanning()) {
    return FALSE;
  }

  if (FORM_AND_JOIN_ENERGY_SCAN != formAndJoinScanType) {
    // This scan is an Active Scan.
    // Active Scans potentially report transmit failures through this callback.
    if (EMBER_SUCCESS != status) {
      // The Active Scan is still in progress.  This callback is informing us
      // about a failure to transmit the beacon request on this channel.
      // If necessary we could save this failing channel number and start
      // another Active Scan on this channel later (after this current scan is
      // complete).
      return FALSE;
    }
  }

  switch (formAndJoinScanType) {

  case FORM_AND_JOIN_ENERGY_SCAN:    
    energyScanComplete();    
    break;

  case FORM_AND_JOIN_PAN_ID_SCAN:
    panIdScanComplete();
    break;

  case FORM_AND_JOIN_JOINABLE_SCAN: 
    #if defined(XAP2B) || defined(EZSP_HOST) || defined(EMBER_TEST)
    if (emberEnableDualChannelScan 
        && (channelCache < 15 || channelCache > 22)
        && networkCount) {
      int8u dualChannel = (channelCache + (channelCache < 15 ? 12 : -12));
      formAndJoinScanType = FORM_AND_JOIN_DUAL_CHANNEL_SCAN;
      startScan(EMBER_ACTIVE_SCAN, BIT32(dualChannel), ACTIVE_SCAN_DURATION);
      break;
    }
    #endif
    // If no dual scan, fall through to save a little flash.

  case FORM_AND_JOIN_DUAL_CHANNEL_SCAN:   
    formAndJoinScanType = FORM_AND_JOIN_NEXT_NETWORK;
    emberScanForNextJoinableNetwork(); 
    break;
  }
  return TRUE;
}

// We are either looking for PAN IDs or for joinable networks.  In the first
// case we just check the found PAN ID against our list of candidates. 

boolean emberFormAndJoinNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                            int8u lqi,
                                            int8s rssi)
{
  int8u i;

  SCAN_DEBUG_MSG("SCAN: nwk found ch %d, ", networkFound->channel);
  SCAN_DEBUG_MSG("panID 0x%2x, xpan: ", networkFound->panId);
  SCAN_DEBUG_XPAN_PRINT(networkFound->extendedPanId);
  SCAN_DEBUG_MSG(", lqi %d", lqi);
  SCAN_DEBUG_MSG(", profile: %d", networkFound->stackProfile);
  SCAN_DEBUG_MSG(", pjoin: %d", networkFound->allowingJoin);
  SCAN_DEBUG("\n");
  
  switch (formAndJoinScanType) {

  case FORM_AND_JOIN_PAN_ID_SCAN:
    for (i = 0; i < NUM_PAN_ID_CANDIDATES; i++)
      if (panIdCandidates[i] == networkFound->panId)
        panIdCandidates[i] = 0xFFFF;
    break;
    
  case FORM_AND_JOIN_JOINABLE_SCAN:

    // check for a beacon with permit join on...
    if (networkFound->allowingJoin
        // ...the same stack profile as this application...
        && networkFound->stackProfile == formAndJoinStackProfile()
        && (// ...and ignore the Extended PAN ID, or...
            ignoreExtendedPanId
            // ...a matching Extended PAN ID
            || (MEMCOMPARE(extendedPanIdCache,
                           networkFound->extendedPanId,
                           EXTENDED_PAN_ID_SIZE) == 0))) {
      saveNetwork(networkFound, lqi, rssi);
      SCAN_DEBUG(": MATCH\r\n"); 
    } else {
      SCAN_DEBUG(": NO match\r\n"); 
    }
    break;

#if defined(XAP2B) || defined(EZSP_HOST) || defined(EMBER_TEST)
  case FORM_AND_JOIN_DUAL_CHANNEL_SCAN: {
    NetworkInfo *finger;
    for (i = 0; i < networkCount; i++) {
      finger = formAndJoinGetNetworkPointer(i);
      if (MEMCOMPARE(finger->network.extendedPanId,
                     networkFound->extendedPanId,
                     EXTENDED_PAN_ID_SIZE) == 0) {
        if (lqi > finger->lqi) {
          finger->network.panId = 0xFFFF;  // Invalid network;
        }
      }
    }
    break;
  }
#endif
  }
  return emberFormAndJoinIsScanning();
}

// Just remember the result.

boolean emberFormAndJoinEnergyScanResultHandler(int8u channel, int8s maxRssiValue)
{
  if (emberFormAndJoinIsScanning()) {
    SCAN_DEBUG_MSG("SCAN: found energy %d dBm on ", maxRssiValue);
    SCAN_DEBUG_MSG("channel 0x%x\r\n", channel);
    channelEnergies[channel - EMBER_MIN_802_15_4_CHANNEL_NUMBER] = maxRssiValue;
    return TRUE;
  }
  return FALSE;
}

//------------------------------------------------------------------------------
// Helper functions

static void saveNetwork(EmberZigbeeNetwork *network, int8u lqi, int8s rssi)
{
  int8u i;
  NetworkInfo *finger;

  // See if we already have that network.
  for (i = 0; i < networkCount; i++) {
    finger = formAndJoinGetNetworkPointer(i);
    if (MEMCOMPARE(finger->network.extendedPanId,
                   network->extendedPanId,
                   EXTENDED_PAN_ID_SIZE) == 0) {
      return;
    }
  }

  if (formAndJoinSetBufferLength(networkCount + 1) != EMBER_SUCCESS) {
    return;
  }
  finger = formAndJoinGetNetworkPointer(networkCount);
  networkCount += 1;
  MEMCOPY(finger, network, sizeof(EmberZigbeeNetwork));
  finger->lqi = lqi;
  finger->rssi = rssi;
}

boolean emberFormAndJoinIsScanning(void)
{
  return (formAndJoinScanType > FORM_AND_JOIN_NEXT_NETWORK);
}

boolean emberFormAndJoinCanContinueJoinableNetworkScan(void)
{
  return (formAndJoinScanType == FORM_AND_JOIN_NEXT_NETWORK);
}

static boolean setup(int8u scanType)
{
  if (emberFormAndJoinIsScanning()) {
    emberScanErrorHandler(EMBER_MAC_SCANNING);
    return FALSE;
  }
  
  // Case 12903: Need to reset the cleanup timeout when initiating a new scan
  // since a previous scan process may have concluded before the cleanup event
  // timer ran out, and we don't want it triggering in the middle of our new
  // scan and cleaning out legitimate data.  
  // This is a special cause because emberFormAndJoinCleanup is occurring 
  // directly rather than as a result of the cleanup event firing, so the 
  // cleanup event timer isn't being deactivated like it would from the event
  // handler.  Since our only interface to manipulate the cleanup event 
  // timer is to restart it (rather than deactivate it), we do that here.
  formAndJoinSetCleanupTimeout();
  emberFormAndJoinCleanup(EMBER_SUCCESS);  // In case we were in NEXT_NETWORK mode.
  networkCount = 0;
  formAndJoinScanType = scanType;
  dataContents = formAndJoinAllocateBuffer();
  if (dataContents == NULL) {
    emberFormAndJoinCleanup(EMBER_NO_BUFFERS);
    return FALSE;
  }
  return TRUE;
}

void emberFormAndJoinCleanup(EmberStatus status)
{
  formAndJoinScanType = FORM_AND_JOIN_NOT_SCANNING;
  SCAN_DEBUG("formAndJoinReleaseBuffer()\n");
  formAndJoinReleaseBuffer();
  if (status != EMBER_SUCCESS) {
    emberScanErrorHandler(status);
  }
}

static EmberStatus startScan(EmberNetworkScanType type, int32u mask, int8u duration)
{
  EmberStatus status = emberStartScan(type, mask, duration);
  SCAN_DEBUG_MSG("SCAN: start scan, status 0x%x\r\n", status);
  if (status != EMBER_SUCCESS) {
    emberFormAndJoinCleanup(status);
  }
  return status;
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
