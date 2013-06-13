// *******************************************************************
// * price-server-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/plugin/price-server/price-server.h"

static void clear(void);
static void who(void);
static void what(void);
static void when(void);
static void prce(void);
static void alternate(void);
static void ack(void);
static void valid(void);
static void get(void);
static void print(void);
static void sprint(void);
static void publish(void);

static EmberAfScheduledPrice price;

EmberCommandEntry emberAfPluginPriceServerCommands[] = {
  emberCommandEntryAction("clear",  clear, "u", ""),
  emberCommandEntryAction("who",  who, "wbw", ""),
  emberCommandEntryAction("what",  what, "uvuu", ""),
  emberCommandEntryAction("when",  when, "wv", ""),
  emberCommandEntryAction("price",  prce, "wuwu", ""),
  emberCommandEntryAction("alternate",  alternate, "wuu", ""),
  emberCommandEntryAction("ack",  ack, "u", ""),
  emberCommandEntryAction("valid",  valid, "uu", ""),
  emberCommandEntryAction("get",  get, "uu", ""),
  emberCommandEntryAction("print",  print, "u", ""),
  emberCommandEntryAction("sprint",  sprint, "u", ""),
  emberCommandEntryAction("publish", publish, "vuuu", ""),
  emberCommandEntryTerminator(),
};

// plugin price-server clear <endpoint:1>
static void clear(void)
{
  emberAfPriceClearPriceTable(emberUnsignedCommandArgument(0));
}

// plugin price-server who <provId:4> <label:1-13> <eventId:4>
static void who(void)
{
  int8u length;
  price.providerId = emberUnsignedCommandArgument(0);
  length = emberCopyStringArgument(1,
                                   price.rateLabel + 1,
                                   ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH,
                                   FALSE);
  price.rateLabel[0] = length;
  price.issuerEventID = emberUnsignedCommandArgument(2);
}

// plugin price-server what <unitOfMeas:1> <curr:2> <ptd:1> <PTRT:1>
static void what(void)
{
  price.unitOfMeasure = (int8u)emberUnsignedCommandArgument(0);
  price.currency = (int16u)emberUnsignedCommandArgument(1);
  price.priceTrailingDigitAndTier = (int8u)emberUnsignedCommandArgument(2);
  price.numberOfPriceTiersAndTier = (int8u)emberUnsignedCommandArgument(3);
}

// plugin price-server when <startTime:4> <duration:2>
static void when(void)
{
  price.startTime = emberUnsignedCommandArgument(0);
  price.duration = (int16u)emberUnsignedCommandArgument(1);
}

// plugin price-server price <price:4> <ratio:1> <genPrice:4> <genRatio:1>
static void prce(void)
{
  price.price = emberUnsignedCommandArgument(0);
  price.priceRatio = (int8u)emberUnsignedCommandArgument(1);
  price.generationPrice = emberUnsignedCommandArgument(2);
  price.generationPriceRatio = (int8u)emberUnsignedCommandArgument(3);
}

// plugin price-server alternate <alternateCostDelivered:4> <alternateCostUnit:1> <alternateCostTrailingDigit:1>
static void alternate(void)
{
  price.alternateCostDelivered = emberUnsignedCommandArgument(0);
  price.alternateCostUnit = (int8u)emberUnsignedCommandArgument(1);
  price.alternateCostTrailingDigit = (int8u)emberUnsignedCommandArgument(2);
}

// plugin price-server ack <req:1>
static void ack(void)
{
  price.priceControl &= ~ZCL_PRICE_CLUSTER_PRICE_ACKNOWLEDGEMENT_MASK;
  if (emberUnsignedCommandArgument(0) == 1) {
    price.priceControl |= EMBER_ZCL_PRICE_CONTROL_ACKNOWLEDGEMENT_REQUIRED;
  }
}

// pllugin price-server <valid | invalid> <endpoint:1> <index:1>
static void valid(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  int8u index = (int8u)emberUnsignedCommandArgument(1);
  if (!emberAfPriceSetPriceTableEntry(endpoint,
                                      index,
                                      (emberCurrentCommand->name[0] == 'v'
                                       ? &price
                                       : NULL))) {
    emberAfPriceClusterPrintln("price entry %d not present", index);;
  }
}

// plugin price-server get <endpoint:1> <index:1>
static void get(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  int8u index = (int8u)emberUnsignedCommandArgument(1);
  if (!emberAfPriceGetPriceTableEntry(endpoint, index, &price)) {
    emberAfPriceClusterPrintln("price entry %d not present", index);;
  }
}

// plugin price-server print <endpoint:1>
static void print(void) 
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emberAfPricePrintTable(endpoint);
}

// plugin price-server sprint
static void sprint(void)
{
  emberAfPricePrint(&price);
}

// plugin price-server publish <nodeId:2> <srcEndpoint:1> <dstEndpoint:1> <priceIndex:1>
static void publish(void)
{
  emberAfPluginPriceServerPublishPriceMessage((EmberNodeId)emberUnsignedCommandArgument(0),
                                              (int8u)emberUnsignedCommandArgument(1),
                                              (int8u)emberUnsignedCommandArgument(2),
                                              (int8u)emberUnsignedCommandArgument(3));
}
