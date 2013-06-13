// *******************************************************************
// * File: network-manager-lite.c
// *
// * See network-manager.h for an overview.
// *
// * The philosophy behind this very lightweight implementation is that
// * it is more important to stay away from known bad channels than
// * to work hard to guess which channels are good.
// *
// * This implementation simply keeps a blacklist of past channels.
// * Whenever the channel is changed, the old channel is added to the list.
// * Once all channels have been visited, the process starts over.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#if defined(XAP2B_EM250) || defined(CORTEXM3)
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
int32u nmBlacklistMask;

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
      if (nmReportCount == NM_WARNING_LIMIT) {
        nmReportCount = 0;
        nmUtilWarningHandler();
      }
    }
    return TRUE;
  }
  return FALSE;
}

// Blacklists the current channel, chooses a new channel that hasn't
// been blacklisted, and broadcasts a ZDO channel change request.
// After all channels have been blacklisted, clears the blacklist.
// Tries to choose a new channel that is at least 3 away from the
// current channel, to avoid wideband interferers.

EmberStatus nmUtilChangeChannelRequest(void)
{
  int8u channel;
  int8u pass;
  int8u currentChannel = emberGetRadioChannel();

  nmBlacklistMask |= BIT32(currentChannel);
  if (nmBlacklistMask == NM_CHANNEL_MASK) {
    nmBlacklistMask = BIT32(currentChannel);
  }

  for (pass = 0; pass < 2; pass++) {
    for (channel = (pass ? 11 : currentChannel + 3); 
         channel < 27; 
         channel++) {
      boolean inBlacklist = ((nmBlacklistMask & BIT32(channel)) != 0);
      boolean inMask = ((NM_CHANNEL_MASK & BIT32(channel)) != 0);
      if (inMask && !inBlacklist) {
        return emberChannelChangeRequest(channel);
      }
    }
  }

  // The only way to get here is if there are no channels to change
  // to, because NM_CHANNEL_MASK has fewer than 2 bits set.
  return EMBER_SUCCESS;
}
