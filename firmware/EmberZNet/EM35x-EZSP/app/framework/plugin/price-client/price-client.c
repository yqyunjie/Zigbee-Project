// *******************************************************************
// * price-client.c
// *
// * The Price client plugin is responsible for keeping track of the current
// * and future prices.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "price-client.h"
#include "price-client-callback.h"

static void initPrice(EmberAfPluginPriceClientPrice *price);
static void printPrice(EmberAfPluginPriceClientPrice *price);
static void scheduleTick(int8u endpoint);

static EmberAfPluginPriceClientPrice priceTable[EMBER_AF_PRICE_CLUSTER_CLIENT_ENDPOINT_COUNT][EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE];

void emberAfPriceClusterClientInitCallback(int8u endpoint)
{
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint,
                                                   ZCL_PRICE_CLUSTER_ID);
  if (ep == 0xFF) {
    return;
  }
  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE; i++) {
    initPrice(&priceTable[ep][i]);
  }
}

void emberAfPriceClusterClientTickCallback(int8u endpoint)
{
  scheduleTick(endpoint);
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
  int8u endpoint = emberAfCurrentEndpoint();
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint,
                                                   ZCL_PRICE_CLUSTER_ID);
  EmberAfPluginPriceClientPrice *price = NULL, *last;
  EmberAfStatus status;
  int32u endTime, now = emberAfGetCurrentTime();
  int8u i;

  if (ep == 0xFF) {
    return FALSE;
  }

  last = &priceTable[ep][0];

  emberAfPriceClusterPrint("RX: PublishPrice 0x%4x, \"", providerId);
  emberAfPriceClusterPrintString(rateLabel);
  emberAfPriceClusterPrint("\"");
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrint(", 0x%4x, 0x%4x, 0x%x, 0x%2x, 0x%x, 0x%x, 0x%4x",
                           issuerEventId,
                           currentTime,
                           unitOfMeasure,
                           currency,
                           priceTrailingDigitAndPriceTier,
                           numberOfPriceTiersAndRegisterTier,
                           startTime);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrint(", 0x%2x, 0x%4x, 0x%x, 0x%4x, 0x%x, 0x%4x, 0x%x",
                           durationInMinutes,
                           prc,
                           priceRatio,
                           generationPrice,
                           generationPriceRatio,
                           alternateCostDelivered,
                           alternateCostUnit);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln(", 0x%x, 0x%x, 0x%x",
                             alternateCostTrailingDigit,
                             numberOfBlockThresholds,
                             priceControl);
  emberAfPriceClusterFlush();

  if (startTime == ZCL_PRICE_CLUSTER_START_TIME_NOW) {
    startTime = now;
  }
  endTime = (durationInMinutes == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED
             ? ZCL_PRICE_CLUSTER_END_TIME_NEVER
             : startTime + durationInMinutes * 60);

  // If the price has already expired, don't bother with it.
  if (endTime <= now) {
    status = EMBER_ZCL_STATUS_FAILURE;
    goto kickout;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE; i++) {
    // Ignore invalid prices, but remember the empty slot for later.
    if (!priceTable[ep][i].valid) {
      if (price == NULL) {
        price = &priceTable[ep][i];
      }
      continue;
    }

    // Reject duplicate prices based on the issuer event id.  This assumes that
    // issuer event ids are unique and that pricing information associated with
    // an issuer event id never changes.
    if (priceTable[ep][i].issuerEventId == issuerEventId) {
      status = EMBER_ZCL_STATUS_DUPLICATE_EXISTS;
      goto kickout;
    }

    // Nested and overlapping prices are not allowed.  Prices with the newer
    // issuer event ids takes priority over all nested and overlapping prices.
    // The only exception is when a price with a newer issuer event id overlaps
    // with the end of the current active price.  In this case, the duration of
    // the current active price is changed to "until changed" and it will expire
    // when the new price starts.
    if (priceTable[ep][i].startTime < endTime
        && priceTable[ep][i].endTime > startTime) {
      if (priceTable[ep][i].issuerEventId < issuerEventId) {
        if (priceTable[ep][i].active && now < startTime) {
          priceTable[ep][i].endTime = startTime;
          priceTable[ep][i].durationInMinutes = ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED;
        } else {
          if (priceTable[ep][i].active) {
            priceTable[ep][i].active = FALSE;
            emberAfPluginPriceClientPriceExpiredCallback(&priceTable[ep][i]);
          }
          initPrice(&priceTable[ep][i]);
        }
      } else {
        status = EMBER_ZCL_STATUS_FAILURE;
        goto kickout;
      }
    }

    // Along the way, search for an empty slot for this new price and find the
    // price in the table with the latest start time.  If there are no empty
    // slots, we will either have to drop this price or the last one, depending
    // on the start times.
    if (price == NULL) {
      if (!priceTable[ep][i].valid) {
        price = &priceTable[ep][i];
      } else if (last->startTime < priceTable[ep][i].startTime) {
        last = &priceTable[ep][i];
      }
    }
  }

  // If there were no empty slots and this price starts after all of the other
  // prices in the table, drop this price.  Otherwise, drop the price with the
  // latest start time and replace it with this one.
  if (price == NULL) {
    if (last->startTime < startTime) {
      status = EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
      goto kickout;
    } else {
      price = last;
    }
  }

  price->valid                             = TRUE;
  price->active                            = FALSE;
  price->clientEndpoint                    = endpoint;
  price->providerId                        = providerId;
  emberAfCopyString(price->rateLabel, rateLabel, ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH);
  price->issuerEventId                     = issuerEventId;
  price->currentTime                       = currentTime;
  price->unitOfMeasure                     = unitOfMeasure;
  price->currency                          = currency;
  price->priceTrailingDigitAndPriceTier    = priceTrailingDigitAndPriceTier;
  price->numberOfPriceTiersAndRegisterTier = numberOfPriceTiersAndRegisterTier;
  price->startTime                         = startTime;
  price->endTime                           = endTime;
  price->durationInMinutes                 = durationInMinutes;
  price->price                             = prc;
  price->priceRatio                        = priceRatio;
  price->generationPrice                   = generationPrice;
  price->generationPriceRatio              = generationPriceRatio;
  price->alternateCostDelivered            = alternateCostDelivered;
  price->alternateCostUnit                 = alternateCostUnit;
  price->alternateCostTrailingDigit        = alternateCostTrailingDigit;
  price->numberOfBlockThresholds           = numberOfBlockThresholds;
  price->priceControl                      = priceControl;

  // Now that we have saved the price in our table, we may have to reschedule
  // our tick to activate or expire prices.
  scheduleTick(endpoint);

  // If the acknowledgement is required, send it immediately.  Otherwise, a
  // default response is sufficient.
  if (priceControl & ZCL_PRICE_CLUSTER_PRICE_ACKNOWLEDGEMENT_MASK) {
    emberAfFillCommandPriceClusterPriceAcknowledgement(providerId,
                                                       issuerEventId,
                                                       now,
                                                       priceControl);
    emberAfSendResponse();
    return TRUE;
  } else {
    status = EMBER_ZCL_STATUS_SUCCESS;
  }

kickout:
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

void emAfPluginPriceClientPrintInfo(int8u endpoint)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)
  int8u i;                                                                                                              
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);                                     

  if (ep == 0xFF) {
    return;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE; i++) {                                                       
    emberAfPriceClusterFlush();                                                                                         
    emberAfPriceClusterPrintln("= Price %d =", i);                                                                      
    printPrice(&priceTable[ep][i]);                                                                
    emberAfPriceClusterFlush();                                                                                         
  }  
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)
}

static void initPrice(EmberAfPluginPriceClientPrice *price)
{
  price->valid                             = FALSE;
  price->active                            = FALSE;
  price->clientEndpoint                    = 0xFF;
  price->providerId                        = 0x00000000UL;
  price->rateLabel[0]                      = 0;
  price->issuerEventId                     = 0x00000000UL;
  price->currentTime                       = 0x00000000UL;
  price->unitOfMeasure                     = 0x00;
  price->currency                          = 0x0000;
  price->priceTrailingDigitAndPriceTier    = 0x00;
  price->numberOfPriceTiersAndRegisterTier = 0x00;
  price->startTime                         = 0x00000000UL;
  price->endTime                           = 0x00000000UL;
  price->durationInMinutes                 = 0x0000;
  price->price                             = 0x00000000UL;
  price->priceRatio                        = ZCL_PRICE_CLUSTER_PRICE_RATIO_NOT_USED;
  price->generationPrice                   = ZCL_PRICE_CLUSTER_GENERATION_PRICE_NOT_USED;
  price->generationPriceRatio              = ZCL_PRICE_CLUSTER_GENERATION_PRICE_RATIO_NOT_USED;
  price->alternateCostDelivered            = ZCL_PRICE_CLUSTER_ALTERNATE_COST_DELIVERED_NOT_USED;
  price->alternateCostUnit                 = ZCL_PRICE_CLUSTER_ALTERNATE_COST_UNIT_NOT_USED;
  price->alternateCostTrailingDigit        = ZCL_PRICE_CLUSTER_ALTERNATE_COST_TRAILING_DIGIT_NOT_USED;
  price->numberOfBlockThresholds           = ZCL_PRICE_CLUSTER_NUMBER_OF_BLOCK_THRESHOLDS_NOT_USED;
  price->priceControl                      = ZCL_PRICE_CLUSTER_PRICE_CONTROL_NOT_USED;
}

static void printPrice(EmberAfPluginPriceClientPrice *price)
{
  emberAfPriceClusterPrintln("    vld: %p", (price->valid ? "YES" : "NO"));
  emberAfPriceClusterPrintln("    act: %p", (price->active ? "YES" : "NO"));
  emberAfPriceClusterPrintln("    pid: 0x%4x", price->providerId);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrint(  "     rl: \"");
  emberAfPriceClusterPrintString(price->rateLabel);
  emberAfPriceClusterPrintln("\"");
  emberAfPriceClusterPrintln("   ieid: 0x%4x", price->issuerEventId);
  emberAfPriceClusterPrintln("     ct: 0x%4x", price->currentTime);
  emberAfPriceClusterPrintln("    uom: 0x%x",  price->unitOfMeasure);
  emberAfPriceClusterPrintln("      c: 0x%2x", price->currency);
  emberAfPriceClusterPrintln(" ptdapt: 0x%x",  price->priceTrailingDigitAndPriceTier);
  emberAfPriceClusterPrintln("noptart: 0x%x",  price->numberOfPriceTiersAndRegisterTier);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("     st: 0x%4x", price->startTime);
  emberAfPriceClusterPrintln("     et: 0x%4x", price->endTime);
  emberAfPriceClusterPrintln("    dim: 0x%2x", price->durationInMinutes);
  emberAfPriceClusterPrintln("      p: 0x%4x", price->price);
  emberAfPriceClusterPrintln("     pr: 0x%x",  price->priceRatio);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("     gp: 0x%4x", price->generationPrice);
  emberAfPriceClusterPrintln("    gpr: 0x%x",  price->generationPriceRatio);
  emberAfPriceClusterPrintln("    acd: 0x%4x", price->alternateCostDelivered);
  emberAfPriceClusterPrintln("    acu: 0x%x",  price->alternateCostUnit);
  emberAfPriceClusterPrintln("   actd: 0x%x",  price->alternateCostTrailingDigit);
  emberAfPriceClusterPrintln("   nobt: 0x%x",  price->numberOfBlockThresholds);
  emberAfPriceClusterPrintln("     pc: 0x%x",  price->priceControl);
}

static void scheduleTick(int8u endpoint)
{
  int32u next = ZCL_PRICE_CLUSTER_END_TIME_NEVER;
  int32u now = emberAfGetCurrentTime();
  boolean active = FALSE;
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint,
                                                   ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_CLIENT_TABLE_SIZE; i++) {
    if (!priceTable[ep][i].valid) {
      continue;
    }

    // Remove old prices from the table.  This may result in the active price
    // being expired, which requires notifying the application.
    if (priceTable[ep][i].endTime <= now) {
      if (priceTable[ep][i].active) {
        priceTable[ep][i].active = FALSE;
        emberAfPluginPriceClientPriceExpiredCallback(&priceTable[ep][i]);
      }
      initPrice(&priceTable[ep][i]);
      continue;
    }

    // If we don't have a price that should be active right now, we will need to
    // schedule the tick to wake us up when the next active price should start,
    // so keep track of the price with the start time soonest after the current
    // time.
    if (!active && priceTable[ep][i].startTime < next) {
      next = priceTable[ep][i].startTime;
    }

    // If we have a price that should be active now, a tick is scheduled for the
    // time remaining in the duration to wake us up and expire the price.  If
    // the price is transitioning from inactive to active for the first time, we
    // also need to notify the application the application.
    if (priceTable[ep][i].startTime <= now) {
      if (!priceTable[ep][i].active) {
        priceTable[ep][i].active = TRUE;
        emberAfPluginPriceClientPriceStartedCallback(&priceTable[ep][i]);
      }
      active = TRUE;
      next = priceTable[ep][i].endTime;
    }
  }

  // We need to wake up again to activate a new price or expire the current
  // price.  Otherwise, we don't have to do anything until we receive a new
  // price from the server.
  if (next != ZCL_PRICE_CLUSTER_END_TIME_NEVER) {
    emberAfScheduleClusterTick(endpoint,
                               ZCL_PRICE_CLUSTER_ID,
                               EMBER_AF_CLIENT_CLUSTER_TICK,
                               (next - now) * MILLISECOND_TICKS_PER_SECOND,
                               EMBER_AF_OK_TO_HIBERNATE);
  } else {
    emberAfDeactivateClusterTick(endpoint,
                                 ZCL_PRICE_CLUSTER_ID,
                                 EMBER_AF_CLIENT_CLUSTER_TICK);
  }
}
