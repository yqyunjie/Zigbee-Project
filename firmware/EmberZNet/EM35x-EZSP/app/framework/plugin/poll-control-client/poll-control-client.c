// *******************************************************************
// * poll-control-client.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "poll-control-client.h"

static boolean fastPolling = FALSE;
static boolean respondToCheckIn = TRUE;
static int16u fastPollingTimeout = EMBER_AF_PLUGIN_POLL_CONTROL_CLIENT_DEFAULT_FAST_POLL_TIMEOUT;

void emAfSetFastPollingMode(boolean mode)
{
  fastPolling = mode;
}

void emAfSetFastPollingTimeout(int16u timeout)
{
  fastPollingTimeout = timeout;
}

void emAfSetResponseMode(boolean mode)
{
  respondToCheckIn = mode;
}

boolean emberAfPollControlClusterCheckInCallback(void)
{
  if (respondToCheckIn) {
    emberAfFillCommandPollControlClusterCheckInResponse(fastPolling, fastPollingTimeout);
    emberAfSendResponse();
  }

  return TRUE;
}

void emAfPollControlClientPrint(void)
{
  emberAfPollControlClusterPrintln("Poll Control Client:\n%p %p\n%p 0x%x", 
                                   "fast polling: ",
                                   fastPolling ? "on" : "off",
                                   "fast polling timeout: ",
                                   fastPollingTimeout);
}
