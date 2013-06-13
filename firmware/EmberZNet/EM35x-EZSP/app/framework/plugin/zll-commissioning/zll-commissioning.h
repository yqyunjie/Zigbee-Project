// *******************************************************************
// * zll-commissioning.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

/** @brief Generate a random network key and initialize the security state of
 * the device.
 *
 * This function is a convenience wrapper for ::emberZllSetInitialSecurityState,
 * which must be called before starting or joining a network.  The plugin will
 * initialize the security state for the initiator during touch linking.  The
 * target must initialize its own security state prior to forming a network
 * either by using this function or by calling ::emberZllSetInitialSecurityState
 * directly.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
EmberStatus emberAfZllSetInitialSecurityState(void);

/** @brief Initiate the touch link procedure.
 *
 * This function will cause the stack to broadcast a series of ScanRequest
 * commands via inter-PAN messaging.  The plugin will select the target that
 * sent a ScanResponse command with the strongest RSSI and attempt to link with
 * it.  If touch linking completes successfully, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkCompleteCallback with information
 * about the network and the target.  If touch linking fails, the plugin will
 * call ::emberAfPluginZllCommissioningTouchLinkFailedCallback.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
EmberStatus emberAfZllInitiateTouchLink(void);

/** @brief Initiates a touch link for the purpose of retrieving information
 * about a target device.
 *
 * As with a traditional touch link, this function will cause the stack to
 * broadcast messages to discover a target device.  When the target is selected
 * (based on RSSI), the plugin will retrieve information about it by unicasting
 * a series of DeviceInformationRequest commands via inter-PAN messaging.  If
 * the process completes successfully, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkCompleteCallback with information
 * about the target.  If touch linking fails, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkFailedCallback.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
EmberStatus emberAfZllDeviceInformationRequest(void);

/** @brief Initiates a touch link for the purpose of causing a target device to
 * identify itself.
 *
 * As with a traditional touch link, this function will cause the stack to
 * broadcast messages to discover a target device.  When the target is selected
 * (based on RSSI), the plugin will cause it to identify itself by unicasting
 * an IdentifyRequest command via inter-PAN messaging.  If the process
 * completes successfully, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkCompleteCallback with information
 * about the target.  If touch linking fails, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkFailedCallback.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
EmberStatus emberAfZllIdentifyRequest(void);

/** @brief Initiates a touch link for the purpose of resetting a target device.
 *
 * As with a traditional touch link, this function will cause the stack to
 * broadcast messages to discover a target device.  When the target is selected
 * (based on RSSI), the plugin will reset it by unicasting a
 * ResetToFactoryNewRequest command via inter-PAN messaging.  If the process
 * completes successfully, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkCompleteCallback with information
 * about the target.  If touch linking fails, the plugin will call
 * ::emberAfPluginZllCommissioningTouchLinkFailedCallback.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
EmberStatus emberAfZllResetToFactoryNewRequest(void);

/** @brief Abort the touch link procedure.
 *
 * This function can be called to cancel the touch link procedure.  This can be
 * useful, for example, if the touch link target is incorrect.
 */
void emberAfZllAbortTouchLink(void);

/** @brief Indicates if a touch link procedure is currently in progress.
 *
 * @return TRUE if a touch link is in progress or FALSE otherwise.
 */
boolean emberAfZllTouchLinkInProgress(void);

/** @brief Reset the local device to a factory new state.
 *
 * This function will cause the device to leave the network and clear its
 * network parameters, reset its attributes to their default values, and clear
 * the group and scene tables.
 */
void emberAfZllResetToFactoryNew(void);

/** @brief Scan for joinable networks.
 *
 * This function will scan the primary channel set for joinable networks.  If a
 * joinable network is found, the plugin will attempt to join to it.  If no
 * joinable networks are found or if joining is not successful, the plugin will
 * scan the secondary channel set for joinable networks.  If a joinable network
 * is found, the plugin will attempt to join to it.  The plugin will only scan
 * the secondary channel set if
 * ::EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_SECONDARY_CHANNELS is defined.
 * Otherwise, scanning stops after the initial scan of the primary channel set.
 *
 * Routers and end devices should scan for joinable networks when directed by
 * the application.  Scanning for joinable networks enables classical ZigBee
 * commissioning with non-ZLL devices.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
  EmberStatus emberAfZllScanForJoinableNetwork(void);
#else
  #define emberAfZllScanForJoinableNetwork emberAfStartSearchForJoinableNetworkCallback
#endif

/** @brief Scan for an unused PAN id.
 *
 * This function will scan the primary channel set for a channel with low
 * average energy and then select a PAN id that is not in use on that channel.
 * The plugin will then form a ZLL network on that channel with the chosen PAN
 * id.
 *
 * Factory new routers should form a new ZLL network at startup.  All routers
 * should form a new ZLL network if classical ZigBee commissioning has failed.
 * End devices should not use this API and should instead form ZLL networks via
 * touch linking.
 *
 * @return An ::EmberStatus value that indicates the success or failure of the
 * command.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
  EmberStatus emberAfZllScanForUnusedPanId(void);
#else
  #define emberAfZllScanForUnusedPanId emberAfFindUnusedPanIdAndFormCallback
#endif

// The exponent of the number of scan periods, where a scan period is 960
// symbols, and a symbol is 16 miscroseconds.  The scan will occur for
// ((2^duration) + 1) scan periods.  The ZLL specification requires routers to
// scan for joinable networks using a duration of 4.
#define EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_DURATION 4

#define EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_MILLISECONDS_DELAY \
  (EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_SECONDS_DELAY * MILLISECOND_TICKS_PER_SECOND)

#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  #define EMBER_AF_PLUGIN_ZLL_COMMISSIONING_ADDITIONAL_STATE EMBER_ZLL_STATE_ADDRESS_ASSIGNMENT_CAPABLE
#else
  #define EMBER_AF_PLUGIN_ZLL_COMMISSIONING_ADDITIONAL_STATE EMBER_ZLL_STATE_NONE
#endif

// Internal APIs.
EmberStatus emAfZllFormNetwork(int8u channel, int8s power, EmberPanId panId);
void emAfZllStackStatus(EmberStatus status);

extern int32u emAfZllPrimaryChannelMask;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_SECONDARY_CHANNELS
extern int32u emAfZllSecondaryChannelMask;
#endif
extern int8u emAfZllExtendedPanId[];
