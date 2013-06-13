// *******************************************************************
// * File: network-manager.c
// *
// * See network-manager.h for an overview.
// *
// * This implementation keeps careful track of the mean and mean
// * deviation of the energy on each channel, as given by the 
// * incoming ZDO scan reports.  When it is time to change the
// * channel, the statistics are used to choose the current best
// * channel to change to.
// *
// * A fair amount of RAM is required to store the statistics.
// * For a lighter weight approach, see network-manager-lite.c.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#ifdef XAP2B_EM250
  #include "stack/include/ember.h"
#else //XAP2B_EM250
  #include "stack/include/ember-types.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/ezsp-utils.h"
  #include "stack/include/error.h"
#endif// XAP2B_EM250

#include "hal/hal.h" 
#include "network-manager.h"

static int8u nmReportCount = 0;
static int16u nmWindowStart = 0;

typedef struct {
  int16u mean;
  int16u deviation;
} ChannelStats;

ChannelStats watchList[NM_WATCHLIST_SIZE];

// Compute the mean and mean deviation.  The algorithm is from 
// RFC793 for estimating mean round trip time in TCP.
// The values stored in ChannelStats are actually scaled; 
// the mean is multiplied by 8 and the deviation by 4.

void computeMean(int16s m, ChannelStats *s)
{
  if (s->mean == 0) {
    s->mean = m << 3;
    return;
  }
  m -= (s->mean >> 3);
  s->mean += m;
  if (m < 0)
    m = -m;
  m -= (s->deviation >> 2);
  s->deviation += m;
  
}

// Use the scan report to update the channel stats.

static void updateWatchList(int8u count, int32u mask, int8u* energies)
{
  int8u messageIndex = 0;
  int8u watchlistIndex = 0;
  int8u channel;
  for (channel = 11; channel < 27; channel++) {
    boolean inMessage = ((mask & BIT32(channel)) != 0);
    boolean inWatchlist = ((NM_CHANNEL_MASK & BIT32(channel)) != 0);
    // Integrity check to avoid out of bounds error
    if (messageIndex == count || watchlistIndex == NM_WATCHLIST_SIZE) {
      return;
    }
    if (inMessage && inWatchlist) {
      computeMean(energies[messageIndex], watchList + watchlistIndex);
      /*
      emberSerialPrintf(1, "ch:%d m:%d sa:%d sv:%d rto:%d\r\n", 
                        channel,
                        energies[messageIndex],
                        watchList[watchlistIndex].mean,
                        watchList[watchlistIndex].deviation,
                        (watchList[watchlistIndex].mean >> 3) + watchList[watchlistIndex].deviation);
      emberSerialWaitSend();
      emberSerialBufferTick();
      */
    }
    if (inMessage) {
      messageIndex++;
    }
    if (inWatchlist) {
      watchlistIndex++;
    }
  }
}

// Called from the app in emberIncomingMessageHandler. 
// Returns TRUE if and only if the library processed the message.

boolean nmUtilProcessIncoming(EmberApsFrame *apsFrame,
                              int8u messageLength,
                              int8u* message)
{
  if (apsFrame->profileId == 0
      && apsFrame->clusterId == NWK_UPDATE_RESPONSE) {
    int8u status = message[1];
    if (status == EMBER_ZDP_SUCCESS) {
      int16u now = halCommonGetInt32uMillisecondTick() >> 16;
      int16u elapsed = (int16u)(now - nmWindowStart);
      int8u channelCount = message[10];
      int32u mask = 0;
      int8u *maskPointer = message + 1;
      int8u ii;

      // The channel mask is four bytes, stored low to high
      // starting at message + 2.
      for (ii = 0; ii < 4; ii++) {
        maskPointer++;
        mask = (mask << 8) + *maskPointer;
      }

      // If twice NM_WINDOW_SIZE has elapsed since the last
      // scan report, zero out the report count.  Otherwise,
      // if more than NM_WINDOW_SIZE, divide the report count
      // by two.  This is a cheap way to roughly count the
      // reports within the window.

      if (elapsed > NM_WINDOW_SIZE) {
        nmWindowStart = now;
        nmReportCount = (elapsed > (NM_WINDOW_SIZE << 1)
                         ? 0
                         : nmReportCount >> 1);
      }
      nmReportCount++;
      if (messageLength == 11 + channelCount) {
        updateWatchList(channelCount, mask, message + 11);
      }
      if (nmReportCount == NM_WARNING_LIMIT) {
        nmReportCount = 0;
        nmUtilWarningHandler();
      }
    }
    return TRUE;
  }
  return FALSE;
}
 

// Chooses the best channel and broadcasts a ZDO channel change request.
// Note: the value used for channel comparison is the mean energy 
// plus four times the deviation.

EmberStatus nmUtilChangeChannelRequest(void)
{
  int8u index = 0;
  int16u minEnergy = 0xFFFF;
  int8u currentChannel = emberGetRadioChannel();
  int8u bestChannel = currentChannel;
  int8u channel;

  for (channel = 11; channel < 27; channel++) {
    if (NM_CHANNEL_MASK & BIT32(channel)) {
      int16u energy = (watchList[index].mean >> 3) + watchList[index].deviation;
      if (channel != currentChannel
          && energy < minEnergy) {
        minEnergy = energy;
        bestChannel = channel;
      }
      index += 1;
    }
  }

  return emberChannelChangeRequest(bestChannel);
}
