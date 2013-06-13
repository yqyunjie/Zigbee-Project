// Copyright 2013 Silicon Laboratories, Inc.
//
//
// This callback file is created for your convenience. You may add application code
// to this file. If you regenerate this file over a previous version, the previous
// version will be overwritten and any code you have added will be lost.

#include "app/framework/include/af.h"
#include "app/framework/plugin/price-server/price-server.h"

//------------------------------------------------------------------------------
// Globals

static const int8u normalPriceLabel[] = "Normal Price";
static const int8u salePriceLabel[]   = "SALE! Buy now to save!";

static const int32u normalPrice = 40;  // $0.40
static const int32u salePrice = 3;    // $0.13

static EmberAfScheduledPrice myPrice = {
  0xABCD,     // provider Id     (made-up)
  "",         // price label (filled in later)
  0x1234,     // issuer event ID (made-up)
  0x00,       // unit of measure (kwh in pure binary format, see Table D.22 in 07-5356-17
  840,        // US Dollar See ISO 4217: http://en.wikipedia.org/wiki/ISO_4217
  0x20,       // Price Trailing Digit and Price Tier:
              //   High nibble = digits to the right of decimal point
              //   Low nibble = Price Tier (0 = no tier)
  0,          // Number of price tiers and tier
              //   High nibble = maximum number of tiers (0 = no tiers)
              //   Low nibble = Price Tier (0 = no tier)
  0,          // Start time (0 = now)
  0xFFFF,     // Duration (0xFFFF = until changed)
  0,          // Price according to Current and trailing digit (set later)
  0xFF,       // Price ratio to the "normal" price.  0xFF = unused
  0xFFFFFFFF, // Generation price (all F's = unused)
  0xFF,       // Generation price ratio (0xFF = unused)
  0xFFFFFFFF, // Alternate cost delivered (e.g. cost in CO2 emissions)
              //   (all F's = unused)
  0xFF,       // Alternate cost unit
  0xFF,       // Alternate cost trailing digit
  0,          // Number of block threshholds
  0,          // Price control (i.e. Price Ack or Repeating Block)
};

#define NORMAL_PRICE FALSE
#define SALE_PRICE   TRUE

#define PRICE_CHANGE_DELAY_MINUTES 1

EmberEventControl priceEvent;

//------------------------------------------------------------------------------
// Forward Declarations

static void setupPrice(boolean onSale);

//------------------------------------------------------------------------------
// Functions

void priceEventChange(void)
{
  setupPrice(normalPrice == myPrice.price);
  emberEventControlSetDelayMinutes(priceEvent, PRICE_CHANGE_DELAY_MINUTES);
}

static void setupPrice(boolean onSale)
{
  const int8u* label = (onSale ? salePriceLabel : normalPriceLabel);
  int8u labelSize = (onSale ? sizeof(salePriceLabel) : sizeof(normalPriceLabel));
  MEMSET(myPrice.rateLabel, 0, ZCL_PRICE_CLUSTER_MAXIMUM_RATE_LABEL_LENGTH + 1);
  MEMCOPY(myPrice.rateLabel, label, labelSize);
  myPrice.price = (onSale ? salePrice : normalPrice);

  // One of the few times '%s' is appropriate.  We are using a RAM string
  // %p is used for all CONST strings (which is the norm)
  emberAfCorePrintln("%s", myPrice.rateLabel);

  // This print assumes 2 digit prices less than 1 dollar
  emberAfCorePrintln("Price: $0.%d/kwh\n", myPrice.price);
}

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be
 * notified of changes to the stack status and take appropriate action.  The
 * return code from this callback is ignored by the framework.  The framework
 * will always process the stack status after the callback returns.
 *
 * @param status   Ver.: always
 */
boolean emberAfStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    priceEventChange();
  }

  return FALSE;
}

/** @brief Broadcast Sent
 *
 * This function is called when a new MTORR broadcast has been successfully
 * sent by the concentrator plugin.
 *
 */
void emberAfPluginConcentratorBroadcastSentCallback(void)
{
}

/** @brief Finished
 *
 * This callback is fired when the network-find plugin is finished with the
 * forming or joining process.  The result of the operation will be returned
 * in the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginNetworkFindFinishedCallback(EmberStatus status)
{
}

/** @brief Get Radio Power For Channel
 *
 * This callback is called by the framework when it is setting the radio power
 * during the discovery process. The framework will set the radio power
 * depending on what is returned by this callback.
 *
 * @param channel   Ver.: always
 */
int8s emberAfPluginNetworkFindGetRadioPowerForChannelCallback(int8u channel)
{
  return EMBER_AF_PLUGIN_NETWORK_FIND_RADIO_TX_POWER;
}

/** @brief Join
 *
 * This callback is called by the plugin when a joinable network has been
 * found.  If the application returns TRUE, the plugin will attempt to join
 * the network.  Otherwise, the plugin will ignore the network and continue
 * searching.  Applications can use this callback to implement a network
 * blacklist.
 *
 * @param networkFound   Ver.: always
 * @param lqi   Ver.: always
 * @param rssi   Ver.: always
 */
boolean emberAfPluginNetworkFindJoinCallback(EmberZigbeeNetwork * networkFound,
                                             int8u lqi,
                                             int8s rssi)
{
  return TRUE;
}

/** @brief Button Event
 *
 * This allows another module to get notification when a button is pressed and
 * the button joining plugin does not handle it.  For example, if the device
 * is already joined to the network and button 0 is pressed.  Button 0
 * normally forms or joins a network.  This callback is NOT called in ISR
 * context so there are no restrictions on what code can execute.
 *
 * @param buttonNumber The button number that was pressed.  Ver.: always
 */
void emberAfPluginButtonJoiningButtonEventCallback(int8u buttonNumber,
                                                   int32u buttonDurationMs)
{
  if (buttonNumber == 1) {
    priceEventChange();
  }
}

/** @brief Main Tick
 *
 * Whenever main application tick is called, this callback will be called at
 * the end of the main tick execution.
 *
 */
void emberAfMainTickCallback(void)
{
  static boolean executedAlready = FALSE;
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;

  if (executedAlready) {
    return;
  }

  if (EMBER_SUCCESS != emberAfGetNetworkParameters(&nodeType,
                                                   &parameters)) {
    // Form a network for the first time on one of the preferred
    // Smart Energy Channels.  Scan for an unused short pan ID.
    // Long PAN ID is randomly generated.
    emberAfFindUnusedPanIdAndFormCallback();

    // Subsequent reboots will cause the device to re-initalize
    // the previous network parameters and start running again.
  }

  executedAlready = TRUE;
}
