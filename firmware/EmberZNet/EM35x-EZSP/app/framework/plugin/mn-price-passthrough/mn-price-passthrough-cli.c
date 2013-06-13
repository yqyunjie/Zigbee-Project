// *******************************************************************
// * mn-price-passthrough-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/mn-price-passthrough/mn-price-passthrough.h"

static void start(void);
static void stop(void);
static void setRouting(void);
static void print(void);

EmberCommandEntry emberAfPluginMnPricePassthroughCommands[] = {
  emberCommandEntryAction("start", start, "", ""),
  emberCommandEntryAction("stop", stop, "", ""),
  emberCommandEntryAction("set-routing", setRouting, "vuu", ""),
  emberCommandEntryAction("print", print, "", ""),
  emberCommandEntryTerminator(),
};

// plugin mn-price-passthrough start
static void start(void)
{
  emAfPluginMnPricePassthroughStartPollAndForward();
}

// plugin mn-price-passthrough start
static void stop(void)
{
  emAfPluginMnPricePassthroughStopPollAndForward();
}

// plugin mn-price-passthrough setRouting <forwardingId:2> <forwardingEndpoint:1> <proxyEsiEndpoint:1>
static void setRouting(void)
{
  EmberNodeId fwdId = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u fwdEndpoint = (int8u)emberUnsignedCommandArgument(1);
  int8u esiEndpoint = (int8u)emberUnsignedCommandArgument(2);
  emAfPluginMnPricePassthroughRoutingSetup(fwdId,
                                           fwdEndpoint,
                                           esiEndpoint);
}

// plugin mn-price-passthrough print
static void print(void)
{
  emAfPluginMnPricePassthroughPrintCurrentPrice();
}
