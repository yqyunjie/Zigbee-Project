// *******************************************************************
// * mn-price-passthrough.h
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

void emAfPluginMnPricePassthroughStartPollAndForward(void);
void emAfPluginMnPricePassthroughStopPollAndForward(void);
void emAfPluginMnPricePassthroughRoutingSetup(EmberNodeId fwdId,
                                              int8u fwdEndpoint,
                                              int8u esiEndpoint);
void emAfPluginMnPricePassthroughPrintCurrentPrice(void);

#ifndef EMBER_AF_PLUGIN_PRICE_SERVER
/**
 * @brief The price and metadata used by the MnPricePassthrough plugin.
 *
 */
typedef struct {
  int32u  providerId;
  int8u   rateLabel[ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH + 1];
  int32u  issuerEventID;
  int8u   unitOfMeasure;
  int16u  currency;
  int8u   priceTrailingDigitAndTier;
  int8u   numberOfPriceTiersAndTier; // added later in errata
  int32u  startTime;
  int16u  duration; // in minutes
  int32u  price;
  int8u   priceRatio;
  int32u  generationPrice;
  int8u   generationPriceRatio;
  int32u  alternateCostDelivered;
  int8u   alternateCostUnit;
  int8u   alternateCostTrailingDigit;
  int8u   numberOfBlockThresholds;
  int8u   priceControl;
} EmberAfScheduledPrice;

#define ZCL_PRICE_CLUSTER_RESERVED_MASK              0xFE
#define ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED 0xFFFF

#endif // EMBER_AF_PLUGIN_PRICE_SERVER

