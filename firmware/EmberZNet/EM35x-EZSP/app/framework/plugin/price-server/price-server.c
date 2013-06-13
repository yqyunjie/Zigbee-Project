// *******************************************************************
// * price-server.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "price-server.h"

#include "app/framework/plugin/test-harness/test-harness.h"

static EmberAfScheduledPrice priceTable[EMBER_AF_PRICE_CLUSTER_SERVER_ENDPOINT_COUNT][EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE];

// Bits 1 through 7 are reserved in the price control field.  These are used
// internally to represent whether the message is valid, active, or is a "start
// now" price.
#define VALID  BIT(1)
#define ACTIVE BIT(2)
#define NOW    BIT(3)

#define priceIsValid(price)   ((price)->priceControl & VALID)
#define priceIsActive(price)  ((price)->priceControl & ACTIVE)
#define priceIsNow(price)     ((price)->priceControl & NOW)
#define priceIsForever(price) ((price)->duration == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED)

// Returns TRUE if the price will be current or scheduled at the given time.
static boolean priceIsCurrentOrScheduled(const EmberAfScheduledPrice *price,
                                         int32u time)
{
  return (priceIsValid(price)
          && priceIsActive(price)
          && (priceIsForever(price)
              || time < price->startTime + (int32u)price->duration * 60));
}

// Returns the number of all current or scheduled prices.
static int8u scheduledPriceCount(int8u endpoint, int32u startTime)
{
  int8u i, count = 0;
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return 0;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE; i++) {
    if (priceIsCurrentOrScheduled(&priceTable[ep][i], startTime)) {
      count++;
    }
  }
  return count;
}

typedef struct {
  boolean isIntraPan;
  union {
    struct {
      EmberNodeId nodeId;
      int8u       clientEndpoint;
      int8u       serverEndpoint;
    } intra;
    struct {
      EmberEUI64 eui64;
      EmberPanId panId;
    } inter;
  } pan;
  int8u  sequence;
  int8u  index;
  int32u startTime;
  int8u  numberOfEvents;
} GetScheduledPricesPartner;
static GetScheduledPricesPartner partner;

void emberAfPriceClearPriceTable(int8u endpoint)
{
  int8u i;
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE; i++) {
    priceTable[ep][i].priceControl &= ~VALID;
  }
}

// Retrieves the price at the index.  Returns FALSE if the index is invalid.
boolean emberAfPriceGetPriceTableEntry(int8u endpoint,
                                       int8u index,
                                       EmberAfScheduledPrice *price)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF || index == 0xFF) {
    return FALSE;
  }

  if (index < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE) {
    MEMCOPY(price, &priceTable[ep][index], sizeof(EmberAfScheduledPrice));

    // Clear out our internal bits from the price control.
    price->priceControl &= ~ZCL_PRICE_CLUSTER_RESERVED_MASK;

    // If the price is expired or it has an absolute time, set the start time
    // and duration to the original start time and duration.  For "start now"
    // prices that are current or scheduled, set the start time to the special
    // value for "now" and set the duration to the remaining time, if it is not
    // already the special value for "until changed."
    if (priceIsCurrentOrScheduled(&priceTable[ep][index], emberAfGetCurrentTime())
        && priceIsNow(&priceTable[ep][index])) {
      price->startTime = ZCL_PRICE_CLUSTER_START_TIME_NOW;
      if (!priceIsForever(&priceTable[ep][index])) {
        price->duration -= ((emberAfGetCurrentTime()
                             - priceTable[ep][index].startTime)
                            / 60);
      }
    }
    return TRUE;
  }

  return FALSE;
}

// Sets the price at the index.  Returns FALSE if the index is invalid.
boolean emberAfPriceSetPriceTableEntry(int8u endpoint, 
                                       int8u index,
                                       const EmberAfScheduledPrice *price)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return FALSE;
  }

  if (index < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE) {
    if (price == NULL) {
      priceTable[ep][index].priceControl &= ~ACTIVE;
      return TRUE;
    }

    MEMCOPY(&priceTable[ep][index], price, sizeof(EmberAfScheduledPrice));

    // Rember if this is a "start now" price, but store the start time as the
    // current time so the duration can be adjusted.
    if (priceTable[ep][index].startTime == ZCL_PRICE_CLUSTER_START_TIME_NOW) {
      priceTable[ep][index].priceControl |= NOW;
      priceTable[ep][index].startTime = emberAfGetCurrentTime();
    } else {
      priceTable[ep][index].priceControl &= ~NOW;
    }

    priceTable[ep][index].priceControl |= (VALID | ACTIVE);
    return TRUE;
  }
  return FALSE;
}

// Returns the index in the price table of the current price.  The first price
// in the table that starts in the past and ends in the future in considered
// the current price.
int8u emberAfGetCurrentPriceIndex(int8u endpoint)
{
  int32u now = emberAfGetCurrentTime();
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);
  int8u i;

  if (ep == 0xFF) {
    return 0xFF;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE; i++) {
    if (priceIsValid(&priceTable[ep][i])) {
      int32u endTime = ((priceTable[ep][i].duration
                         == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED)
                        ? ZCL_PRICE_CLUSTER_END_TIME_NEVER
                        : priceTable[ep][i].startTime + priceTable[ep][i].duration * 60);

      emberAfPriceClusterPrint("checking price %x, currTime %4x, start %4x, end %4x ",
                               i,
                               now,
                               priceTable[ep][i].startTime,
                               endTime);

      if (priceTable[ep][i].startTime <= now && now < endTime) {
        emberAfPriceClusterPrintln("valid");
        emberAfPriceClusterFlush();
        return i;
      } else {
        emberAfPriceClusterPrintln("no");
        emberAfPriceClusterFlush();
      }
    }
  }

  return ZCL_PRICE_INVALID_INDEX;
}

// Retrieves the current price.  Returns FALSE is there is no current price.
boolean emberAfGetCurrentPrice(int8u endpoint, EmberAfScheduledPrice *price)
{
  return emberAfPriceGetPriceTableEntry(endpoint, emberAfGetCurrentPriceIndex(endpoint), price);
}

void emberAfPricePrint(const EmberAfScheduledPrice *price)
{
  emberAfPriceClusterPrint("  label: ");
  emberAfPriceClusterPrintString(price->rateLabel);

  emberAfPriceClusterPrint("(%x)\r\n  uom/cur: %x/%2x"
                           "\r\n  pid/eid: %4x/%4x"
                           "\r\n  ct/st/dur: %4x/%4x/",
                           emberAfStringLength(price->rateLabel),
                           price->unitOfMeasure,
                           price->currency,
                           price->providerId,
                           price->issuerEventID,
                           emberAfGetCurrentTime(),
                           price->startTime);
  if (price->duration == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED) {
    emberAfPriceClusterPrint("INF");
  } else {
    emberAfPriceClusterPrint("%2x", price->duration);
  }
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("\r\n  ptdt/ptrt: %x/%x"
                             "\r\n  p/pr: %4x/%x"
                             "\r\n  gp/gpr: %4x/%x"
                             "\r\n  acd/acu/actd: %4x/%x/%x",
                             price->priceTrailingDigitAndTier,
                             price->numberOfPriceTiersAndTier,
                             price->price,
                             price->priceRatio,
                             price->generationPrice,
                             price->generationPriceRatio,
                             price->alternateCostDelivered,
                             price->alternateCostUnit,
                             price->alternateCostTrailingDigit);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("  nobt: %x", price->numberOfBlockThresholds);
  emberAfPriceClusterPrintln("  pc: %x",
                             (price->priceControl
                              & ZCL_PRICE_CLUSTER_RESERVED_MASK));
  emberAfPriceClusterPrint("  price is valid from time %4x until ",
                           price->startTime);
  if (price->duration == ZCL_PRICE_CLUSTER_DURATION_UNTIL_CHANGED) {
    emberAfPriceClusterPrintln("eternity");
  } else {
    emberAfPriceClusterPrintln("%4x",
                               (price->startTime + (price->duration * 60)));
  }
  emberAfPriceClusterFlush();
}

void emberAfPricePrintTable(int8u endpoint)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)
  int8u i;
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);
  int8u currPriceIndex = emberAfGetCurrentPriceIndex(endpoint);

  if (ep == 0xFF || currPriceIndex == 0xFF) {
    return;
  }

  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("Configured Prices: (total %x, curr index %x)",
                             scheduledPriceCount(endpoint, 0),
                             currPriceIndex);
  emberAfPriceClusterFlush();
  emberAfPriceClusterPrintln("  Note: ALL values given in HEX\r\n");
  emberAfPriceClusterFlush();
  for (i = 0; i < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE; i++) {
    if (!priceIsValid(&priceTable[ep][i])) {
      continue;
    }
    emberAfPriceClusterPrintln("= PRICE %x =%p",
                               i,
                               (i == currPriceIndex ? " (Current Price)" : ""));
    emberAfPricePrint(&priceTable[ep][i]);
  }
#endif // defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_PRICE_CLUSTER)
}

void emberAfPriceClusterServerInitCallback(int8u endpoint)
{
  // set the first entry in the price table
  EmberAfScheduledPrice price;
  price.providerId = 0x00000001;

  // label of "Normal"
  price.rateLabel[0] = 6;
  price.rateLabel[1] = 'N';
  price.rateLabel[2] = 'o';
  price.rateLabel[3] = 'r';
  price.rateLabel[4] = 'm';
  price.rateLabel[5] = 'a';
  price.rateLabel[6] = 'l';

  // first event
  price.issuerEventID= 0x00000001;

  price.unitOfMeasure = EMBER_ZCL_AMI_UNIT_OF_MEASURE_KILO_WATT_HOURS;

  // this is USD = US dollars
  price.currency = 840;

  // top nibble means 2 digits to right of decimal point
  // bottom nibble the current price tier.
  // Valid values are from 1-15 (0 is not valid)
  // and correspond to the tier labels, 1-15.
  price.priceTrailingDigitAndTier = 0x21;

  // initialize the numberOfPriceTiersAndTier
  price.numberOfPriceTiersAndTier =
    (EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE << 4) + 0x00;

  // start time is 0, so it is always valid
  price.startTime = 0x00000000;

  // valid for 1 hr = 60 minutes
  price.duration = 60;

  // price is 0.09 per Kw/Hr
  // we set price as 9 and two digits to right of decimal
  price.price = 9;

  // the next fields arent used
  price.priceRatio = 0xFF;
  price.generationPrice = 0xFFFFFFFFUL;
  price.generationPriceRatio = 0xFF;
  price.alternateCostDelivered = 0xFFFFFFFFUL;
  price.alternateCostUnit = 0xFF;
  price.alternateCostTrailingDigit = 0xFF;
  price.numberOfBlockThresholds = 0xFF;
  price.priceControl = 0x00;

  emberAfPriceSetPriceTableEntry(endpoint, 0, &price);

  partner.index = ZCL_PRICE_INVALID_INDEX;
}

void emberAfPriceClusterServerTickCallback(int8u endpoint)
{
  int8u ep = emberAfFindClusterServerEndpointIndex(endpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return;
  }

  while (partner.index < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE) {
    boolean xxx = priceIsCurrentOrScheduled(&priceTable[ep][partner.index],
                                            partner.startTime);
    partner.index++;
    if (xxx) {
      EmberAfScheduledPrice price;
      emberAfPriceClusterPrintln("TX price at index %x", partner.index - 1);
      emberAfPriceGetPriceTableEntry(endpoint, partner.index - 1, &price);
      emberAfFillCommandPriceClusterPublishPrice(price.providerId,
                                                 price.rateLabel,
                                                 price.issuerEventID,
                                                 emberAfGetCurrentTime(),
                                                 price.unitOfMeasure,
                                                 price.currency,
                                                 price.priceTrailingDigitAndTier,
                                                 price.numberOfPriceTiersAndTier,
                                                 price.startTime,
                                                 price.duration,
                                                 price.price,
                                                 price.priceRatio,
                                                 price.generationPrice,
                                                 price.generationPriceRatio,
                                                 price.alternateCostDelivered,
                                                 price.alternateCostUnit,
                                                 price.alternateCostTrailingDigit,
                                                 price.numberOfBlockThresholds,
                                                 price.priceControl);
      // Rewrite the sequence number of the response so it matches the request.
      appResponseData[1] = partner.sequence;
      if (partner.isIntraPan) {
        emberAfSetCommandEndpoints(partner.pan.intra.serverEndpoint,
                                   partner.pan.intra.clientEndpoint);
        emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, partner.pan.intra.nodeId);
      } else {
        emberAfSendCommandInterPan(partner.pan.inter.panId,
                                   partner.pan.inter.eui64,
                                   EMBER_NULL_NODE_ID,
                                   0, // multicast id - unused
                                   SE_PROFILE_ID);
      }

      partner.numberOfEvents--;
      break;
    }
  }

  if (partner.numberOfEvents != 0
      && partner.index < EMBER_AF_PLUGIN_PRICE_SERVER_PRICE_TABLE_SIZE) {
    emberAfScheduleClusterTick(endpoint,
                               ZCL_PRICE_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               MILLISECOND_TICKS_PER_QUARTERSECOND,
                               EMBER_AF_OK_TO_HIBERNATE);
  } else {
    partner.index = ZCL_PRICE_INVALID_INDEX;
  }
}

boolean emberAfPriceClusterGetCurrentPriceCallback(int8u commandOptions)
{
  EmberAfScheduledPrice price;
  emberAfPriceClusterPrintln("RX: GetCurrentPrice 0x%x", commandOptions);
  if (!emberAfGetCurrentPrice(emberAfCurrentEndpoint(), &price)) {
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
                              price.providerId,
                              price.rateLabel,
                              price.issuerEventID,
                              emberAfGetCurrentTime(),
                              price.unitOfMeasure,
                              price.currency,
                              price.priceTrailingDigitAndTier,
                              price.numberOfPriceTiersAndTier,
                              price.startTime,
                              price.duration,
                              price.price,
                              price.priceRatio,
                              price.generationPrice,
                              price.generationPriceRatio,
                              price.alternateCostDelivered,
                              price.alternateCostUnit,
                              price.alternateCostTrailingDigit,
                              price.numberOfBlockThresholds,
                              price.priceControl);
    emberAfSendResponse();
  }
  return TRUE;
}

boolean emberAfPriceClusterGetScheduledPricesCallback(int32u startTime,
                                                      int8u numberOfEvents)
{
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();
  int8u endpoint = emberAfCurrentEndpoint();

  emberAfPriceClusterPrintln("RX: GetScheduledPrices 0x%4x, 0x%x",
                             startTime,
                             numberOfEvents);

  // Only one GetScheduledPrices can be processed at a time.
  if (partner.index != ZCL_PRICE_INVALID_INDEX) {
    emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_FAILURE);
    return TRUE;
  }

  partner.startTime = (startTime == ZCL_PRICE_CLUSTER_START_TIME_NOW
                       ? emberAfGetCurrentTime()
                       : startTime);
  partner.numberOfEvents = (numberOfEvents == ZCL_PRICE_CLUSTER_NUMBER_OF_EVENTS_ALL
                            ? scheduledPriceCount(endpoint, partner.startTime)
                            : numberOfEvents);

  if (partner.numberOfEvents == 0) {
    emberAfPriceClusterPrintln("no valid price to return!");
    emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_NOT_FOUND);
  } else {
    partner.isIntraPan = (cmd->interPanHeader == NULL);
    if (partner.isIntraPan) {
      partner.pan.intra.nodeId = cmd->source;
      partner.pan.intra.clientEndpoint = cmd->apsFrame->sourceEndpoint;
      partner.pan.intra.serverEndpoint = cmd->apsFrame->destinationEndpoint;
    } else {
      partner.pan.inter.panId = cmd->interPanHeader->panId;
      MEMCOPY(partner.pan.inter.eui64, cmd->interPanHeader->longAddress, EUI64_SIZE);
    }
    partner.sequence = cmd->seqNum;
    partner.index = 0;
    emberAfScheduleClusterTick(emberAfCurrentEndpoint(),
                               ZCL_PRICE_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               MILLISECOND_TICKS_PER_QUARTERSECOND,
                               EMBER_AF_OK_TO_HIBERNATE);
  }
  return TRUE;
}

void emberAfPluginPriceServerPublishPriceMessage(EmberNodeId nodeId,
                                                 int8u srcEndpoint,
                                                 int8u dstEndpoint,
                                                 int8u priceIndex)
{
  EmberStatus status;
  EmberAfScheduledPrice price;
  int8u ep = emberAfFindClusterServerEndpointIndex(srcEndpoint, ZCL_PRICE_CLUSTER_ID);

  if (ep == 0xFF) {
    return;
  }

  if (!emberAfPriceGetPriceTableEntry(srcEndpoint, priceIndex, &price)) {
    emberAfPriceClusterPrintln("Invalid price table entry at index %x", priceIndex);
    return;
  }
  emberAfFillCommandPriceClusterPublishPrice(price.providerId,
                                             price.rateLabel,
                                             price.issuerEventID,
                                             emberAfGetCurrentTime(),
                                             price.unitOfMeasure,
                                             price.currency,
                                             price.priceTrailingDigitAndTier,
                                             price.numberOfPriceTiersAndTier,
                                             price.startTime,
                                             price.duration,
                                             price.price,
                                             price.priceRatio,
                                             price.generationPrice,
                                             price.generationPriceRatio,
                                             price.alternateCostDelivered,
                                             price.alternateCostUnit,
                                             price.alternateCostTrailingDigit,
                                             price.numberOfBlockThresholds,
                                             price.priceControl);

  emberAfSetCommandEndpoints(srcEndpoint, dstEndpoint);
  emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_SOURCE_EUI64;
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, nodeId);
  if(status != EMBER_SUCCESS) {
    emberAfMessagingClusterPrintln("Error in publish price %x", status);
  }
}
