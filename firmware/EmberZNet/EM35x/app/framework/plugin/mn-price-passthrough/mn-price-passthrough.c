// *******************************************************************
// * mn-price-passthrough.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/plugin/mn-price-passthrough/mn-price-passthrough.h"
#include "app/framework/plugin/test-harness/test-harness.h"

#define VALID  BIT(1)

static EmberNodeId forwardingId;
static int8u forwardingEndpoint, proxyEsiEndpoint;
static EmberAfScheduledPrice currentPrice;

EmberEventControl emberAfPluginMnPricePassthroughPollAndForwardEsiEventControl;

void emberAfPluginMnPricePassthroughPollAndForwardEsiEventHandler(void)
{
  EmberStatus status;
  int8u i;

  // Ensure that the endpoint for the proxy ESI has been set
  if (forwardingId == EMBER_NULL_NODE_ID
      || forwardingEndpoint == 0xFF
      || proxyEsiEndpoint == 0xFF) {
    emberAfPriceClusterPrintln("Routing parameters not properly established: node %x forwarding %x proxy %x",
                               forwardingId,
                               forwardingEndpoint,
                               proxyEsiEndpoint);
    goto reschedule;
  }

  // Poll the real ESI
  emberAfFillCommandPriceClusterGetCurrentPrice(0x00);
  emberAfSetCommandEndpoints(forwardingEndpoint, proxyEsiEndpoint);
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, forwardingId);
  if (status != EMBER_SUCCESS) {
    emberAfPriceClusterPrintln("Error in poll and forward event handler %x", status);
  }

reschedule:
  // Reschedule the event
  emberEventControlSetDelayMinutes(emberAfPluginMnPricePassthroughPollAndForwardEsiEventControl,
                                   EMBER_AF_PLUGIN_MN_PRICE_PASSTHROUGH_POLL_RATE);
}

void emberAfPluginMnPricePassthroughInitCallback(void)
{
  // Initialize the proxy ESI endpoint
  emAfPluginMnPricePassthroughRoutingSetup(EMBER_NULL_NODE_ID, 0xFF, 0xFF);
}

void emAfPluginMnPricePassthroughRoutingSetup(EmberNodeId fwdId,
                                              int8u fwdEndpoint,
                                              int8u esiEndpoint)
{
  forwardingId = fwdId;
  forwardingEndpoint = fwdEndpoint;
  proxyEsiEndpoint = esiEndpoint;
}

void emAfPluginMnPricePassthroughStartPollAndForward(void)
{
  emberAfPriceClusterPrintln("Starting %p", "poll and forward");
  emberEventControlSetDelayMS(emberAfPluginMnPricePassthroughPollAndForwardEsiEventControl,
                              MILLISECOND_TICKS_PER_SECOND);
}

void emAfPluginMnPricePassthroughStopPollAndForward(void)
{
  emberAfPriceClusterPrintln("Stopping %p", "poll and forward");
  emberEventControlSetInactive(emberAfPluginMnPricePassthroughPollAndForwardEsiEventControl);
}

boolean emberAfPriceClusterGetCurrentPriceCallback(int8u commandOptions)
{
  emberAfPriceClusterPrintln("RX: GetCurrentPrice 0x%x", commandOptions);
  if (currentPrice.priceControl & VALID) {
    emberAfPriceClusterPrintln("no valid price to return!");
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);
  } else {
    emberAfFillExternalBuffer(ZCL_CLUSTER_SPECIFIC_COMMAND|ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                              | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES,
                              ZCL_PRICE_CLUSTER_ID,
                              ZCL_PUBLISH_PRICE_COMMAND_ID,
                              (sendSE11PublishPriceCommand
                               ? "wswwuvuuwvwuwuwuuuu"
                               : "wswwuvuuwvwuwuwuu" ),
                              currentPrice.providerId,
                              currentPrice.rateLabel,
                              currentPrice.issuerEventID,
                              emberAfGetCurrentTime(),
                              currentPrice.unitOfMeasure,
                              currentPrice.currency,
                              currentPrice.priceTrailingDigitAndTier,
                              currentPrice.numberOfPriceTiersAndTier,
                              currentPrice.startTime,
                              currentPrice.duration,
                              currentPrice.price,
                              currentPrice.priceRatio,
                              currentPrice.generationPrice,
                              currentPrice.generationPriceRatio,
                              currentPrice.alternateCostDelivered,
                              currentPrice.alternateCostUnit,
                              currentPrice.alternateCostTrailingDigit,
                              currentPrice.numberOfBlockThresholds,
                              currentPrice.priceControl);
    emberAfSendResponse();
  }
  return TRUE;
}

boolean emberAfPriceClusterPublishPriceCallback(int32u providerId,
                                                int8u* rateLabel,
                                                int32u issuerEventId,
                                                int32u currentTime,
                                                int8u unitOfMeasure,
                                                int16u currency,
                                                int8u priceTrailingDigitAndPriceTier,
                                                int8u numberOfPriceTiersAndRegisterTier,
                                                int32u startTime,
                                                int16u durationInMinutes,
                                                int32u prc,
                                                int8u priceRatio,
                                                int32u generationPrice,
                                                int8u generationPriceRatio,
                                                int32u alternateCostDelivered,
                                                int8u alternateCostUnit,
                                                int8u alternateCostTrailingDigit,
                                                int8u numberOfBlockThresholds,
                                                int8u priceControl)
{
  emberAfPriceClusterPrint("RX: PublishPrice 0x%4x, \"", providerId);
  emberAfPriceClusterPrintString(rateLabel);
  emberAfPriceClusterPrint("\"");
  emberAfPriceClusterPrint(", 0x%4x, 0x%4x, 0x%x, 0x%2x, 0x%x, 0x%x, 0x%4x",
                           issuerEventId,
                           currentTime,
                           unitOfMeasure,
                           currency,
                           priceTrailingDigitAndPriceTier,
                           numberOfPriceTiersAndRegisterTier,
                           startTime);
  emberAfPriceClusterPrint(", 0x%2x, 0x%4x, 0x%x, 0x%4x, 0x%x, 0x%4x, 0x%x",
                           durationInMinutes,
                           prc,
                           priceRatio,
                           generationPrice,
                           generationPriceRatio,
                           alternateCostDelivered,
                           alternateCostUnit);
  emberAfPriceClusterPrintln(", 0x%x, 0x%x, 0x%x",
                             alternateCostTrailingDigit,
                             numberOfBlockThresholds,
                             priceControl);
  currentPrice.providerId = providerId;
  emberAfCopyString(currentPrice.rateLabel, rateLabel, ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH);
  currentPrice.issuerEventID = issuerEventId;
  currentPrice.startTime = currentTime;
  currentPrice.unitOfMeasure = unitOfMeasure;
  currentPrice.currency = currency;
  currentPrice.priceTrailingDigitAndTier = priceTrailingDigitAndPriceTier;
  currentPrice.numberOfPriceTiersAndTier = numberOfPriceTiersAndRegisterTier;
  currentPrice.startTime = startTime;
  currentPrice.duration = durationInMinutes;
  currentPrice.price = prc;
  currentPrice.priceRatio = priceRatio;
  currentPrice.generationPrice = generationPrice;
  currentPrice.generationPriceRatio = generationPriceRatio;
  currentPrice.alternateCostDelivered = alternateCostDelivered;
  currentPrice.alternateCostUnit = alternateCostUnit;
  currentPrice.alternateCostTrailingDigit = alternateCostTrailingDigit;
  currentPrice.numberOfBlockThresholds = numberOfBlockThresholds;
  currentPrice.priceControl = priceControl;
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

void emAfPluginMnPricePassthroughPrintCurrentPrice(void)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)

  emberAfPriceClusterPrintln("Current configured price:");
  emberAfPriceClusterPrintln("  Note: ALL values given in HEX\r\n");

  emberAfPriceClusterPrintln("= CURRENT PRICE =");
  emberAfPriceClusterPrint("  label: ");
  emberAfPriceClusterPrintString(currentPrice.rateLabel);

  emberAfPriceClusterPrint("(%x)\r\n  uom/cur: %x/%2x"
                           "\r\n  pid/eid: %4x/%4x"
                           "\r\n  ct/st/dur: %4x/%4x/",
                           emberAfStringLength(currentPrice.rateLabel),
                           currentPrice.unitOfMeasure,
                           currentPrice.currency,
                           currentPrice.providerId,
                           currentPrice.issuerEventID,
                           emberAfGetCurrentTime(),
                           currentPrice.startTime);
  if (currentPrice.duration == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED) {
    emberAfPriceClusterPrint("INF");
  } else {
    emberAfPriceClusterPrint("%2x", currentPrice.duration);
  }
  emberAfPriceClusterPrintln("\r\n  ptdt/ptrt: %x/%x"
                             "\r\n  p/pr: %4x/%x"
                             "\r\n  gp/gpr: %4x/%x"
                             "\r\n  acd/acu/actd: %4x/%x/%x",
                             currentPrice.priceTrailingDigitAndTier,
                             currentPrice.numberOfPriceTiersAndTier,
                             currentPrice.price,
                             currentPrice.priceRatio,
                             currentPrice.generationPrice,
                             currentPrice.generationPriceRatio,
                             currentPrice.alternateCostDelivered,
                             currentPrice.alternateCostUnit,
                             currentPrice.alternateCostTrailingDigit);
  emberAfPriceClusterPrintln("  nobt: %x", currentPrice.numberOfBlockThresholds);
  emberAfPriceClusterPrintln("  pc: %x",
                             (currentPrice.priceControl
                              & ZCL_PRICE_CLUSTER_RESERVED_MASK));
  emberAfPriceClusterPrint("  price is valid from time %4x until ",
                           currentPrice.startTime);
  if (currentPrice.duration == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED) {
    emberAfPriceClusterPrintln("eternity");
  } else {
    emberAfPriceClusterPrintln("%4x",
                               (currentPrice.startTime + (currentPrice.duration * 60)));
  }
#endif // defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)
}
