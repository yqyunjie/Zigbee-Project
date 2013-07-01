/**
 * @file network-formation.h
 * See @ref network_formation for documentation.
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */

/**
 * @addtogroup network_formation
 * @brief EmberZNet API for finding, forming, joining, and leaving 
 * ZigBee networks.
 *
 * See network-formation.h for source code.
 * @{
 */

/** @brief Initializes the radio and the EmberZNet stack. The 
 * value of \c reset is returned by the HAL function ::halGetResetInfo(). 
 * (See the <i>Hardware Abstraction Layer (HAL) API Reference</i>.) 
 *
 * Device configuration functions must be called before ::emberInit() 
 * is called.
 *
 * @note The application must check the return value of this function. If the
 * initialization fails, normal messaging functions will not be available.
 * Some failure modes are not
 * fatal, but the application must follow certain procedures to permit recovery.
 * Ignoring the return code will result in unpredictable radio and API behavior.
 * (In particular, problems with association will occur.)
 *
 * @param reset  Cause of the microprocessor reset.
 *
 * @return An ::EmberStatus value indicating successful initialization or the
 * reason for failure.
 */
EmberStatus emberInit(EmberResetType reset);

/** @brief A periodic tick routine that should be called:
 * - in the application's main event loop,
 * - as soon as possible after any radio interrupts, and 
 * - after ::emberInit().
 */
void emberTick(void);

/** @brief Resume network operation after a reboot.  
 * 
 *   The node retains its original type.  
 *   This should be called on startup whether or not the node
 *   was previously part of a network.  ::EMBER_NOT_JOINED is returned
 *   if the node is not part of a network.
 *
 * @return An ::EmberStatus value that indicates one of the following:
 * - successful initialization,
 * - ::EMBER_NOT_JOINED if the node is not part of a network, or
 * - the reason for failure.
 */
EmberStatus emberNetworkInit(void);

/** @brief Forms a new network by becoming the coordinator.
 *
 * @note If using security, the application must call
 *   ::emberSetInitialSecurityState() prior to joining the network.  This also 
 *   applies when a device leaves a network and wants to form another one.

 * @param parameters  Specification of the new network.
 *
 * @return An ::EmberStatus value that indicates either the successful formation
 * of the new network, or the reason that the network formation failed.
 */
EmberStatus emberFormNetwork(EmberNetworkParameters *parameters);

/** @brief Tells the stack to allow other nodes to join the network
  * with this node as their parent.  Joining is initially disabled by default.
  * This funcion may only be called after the node is part of a network
  * and the stack is up.
  *
  * @param duration  A value of 0x00 disables joining. A value of 0xFF
  *  enables joining.  Any other value enables joining for that number of
  *  seconds.
  */
EmberStatus emberPermitJoining(int8u duration);

/** @brief Causes the stack to associate with the network using the
 * specified network parameters. It can take several seconds for the stack to
 * associate with the local network. Do not send messages until a call to the 
 * ::emberStackStatusHandler() callback informs you that the stack is up.
 *
 * @note If ::emberInit() returns an error, your application fails to handle
 * the error, and your application still calls ::emberAssociate(), 
 * ::emberAssociate() might block forever.
 *
 * @note If using security, the application must call
 *   ::emberSetInitialSecurityState() prior to joining the network.  This also 
 *   applies when a device leaves a network and wants to join another one.
 *
 * @param nodeType    Specification of the role that this node will have in
 *   the network.  This role must not be ::EMBER_COORDINATOR.  To be a coordinator,
 *   call ::emberFormNetwork().
 *
 * @param parameters  Specification of the network with which the node
 *   should associate.
 *
 * @return An ::EmberStatus value that indicates either:
 * - that the association process began successfully, or
 * - the reason for failure. 
 */
EmberStatus emberJoinNetwork(EmberNodeType nodeType,
                             EmberNetworkParameters *parameters);

/** @brief Causes the stack to leave the current network. 
 * This generates a call to the ::emberStackStatusHandler() callback to indicate
 * that the network is down. The radio will not be used until after a later call
 * to ::emberFormNetwork() or ::emberJoinNetwork().
 *
 * @return An ::EmberStatus value indicating success or reason for failure. 
 * A status of ::EMBER_INVALID_CALL indicates that the node is either not
 * joined to a network or is already in the process of leaving.
 */
EmberStatus emberLeaveNetwork(void);

/** @brief The application may call this function when contact with the
 * network has been lost.  The most common usage case is when an end device 
 * can no longer communicate with its parent and wishes to find a new one.
 * Another case is when a device has missed a Network Key update and no 
 * longer has the current Network Key.  
 *
 * Note that a call to ::emberPollForData() on an end device that has lost
 * contact with its parent will automatically call ::emberRejoinNetwork(TRUE).
 *
 * The stack will call ::emberStackStatusHandler() to indicate that the network
 * is down, then try to re-establish contact with the network by performing
 * an active scan, choosing a network with matching extended pan id, and 
 * sending a ZigBee network rejoin request.  A second call to the 
 * ::emberStackStatusHandler() callback indicates either the success 
 * or the failure of the attempt.  The process takes
 * approximately 150 milliseconds per channel to complete.
 *
 * This call replaces the ::emberMobileNodeHasMoved() API from EmberZNet 2.x,
 * which used MAC association and consequently took half a second longer
 * to complete.
 *
 * @param haveCurrentNetworkKey  This parameter determines whether the request
 * to rejoin the Network is sent encrypted (TRUE) or unencrypted (FALSE).  The
 * application should first try to use encryption.  If that fails,
 * the application should call this function again and use no encryption.  
 * If the unencrypted rejoin is successful then device will be in the joined but 
 * unauthenticated state.  The Trust Center will be notified of the rejoin and
 * send an updated Network encrypted using the device's Link Key.  Sending the
 * rejoin unencrypted is only supported on networks using Standard Security
 * with link keys (i.e. ZigBee 2006 networks do not support it).
 *
 * @param channelMask A mask indicating the channels to be scanned.  
 * See ::emberStartScan() for format details.
 *
 * @return An ::EmberStatus value indicating success or reason for failure. 
 */
EmberStatus emberFindAndRejoinNetwork(boolean haveCurrentNetworkKey,
                                      int32u channelMask);

/** @brief A convenience function which calls ::emberFindAndRejoinNetwork()
 * with a channel mask value for scanning only the current channel.  
 * Included for back-compatibility.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberRejoinNetwork(boolean haveCurrentNetworkKey);
#else
#define emberRejoinNetwork(haveKey) emberFindAndRejoinNetwork((haveKey), 0)
#endif

/** @brief This function will start a scan. ::EMBER_SUCCESS signals that
  * the scan successfully started.  
  *
  * Possible error responses and their meanings:
  * - ::EMBER_MAC_SCANNING, we are already scanning.
  * - ::EMBER_MAC_JOINED_NETWORK, we are currently joined to a network and 
  *   can not begin a scan.
  * - ::EMBER_MAC_BAD_SCAN_DURATION, we have set a duration value that is 
  *   not 0..14 inclusive.
  * - ::EMBER_MAC_INCORRECT_SCAN_TYPE, we have requested an undefined 
  *   scanning type; 
  * - ::EMBER_MAC_INVALID_CHANNEL_MASK, our channel mask did not specify any
  *   valid channels on the current platform.
  * 
  * @param scanType     Indicates the type of scan to be performed.  
  *  Possible values:  ::EMBER_ENERGY_SCAN, ::EMBER_ACTIVE_SCAN.
  * 
  * @param channelMask  Bits set as 1 indicate that this particular channel
  * should be scanned. Bits set to 0 indicate that this particular channel
  * should not be scanned.  For example, a channelMask value of 0x00000001
  * would indicate that only channel 0 should be scanned.  Valid channels range
  * from 11 to 26 inclusive.  This translates to a channel mask value of 0x07
  * FF F8 00.  As a convenience, a channelMask of 0 is reinterpreted as the
  * mask for the current channel.
  * 
  * @param duration     Sets the exponent of the number of scan periods,
  * where a scan period is 960 symbols, and a symbol is 16 miscroseconds. 
  * The scan will occur for ((2^duration) + 1) scan periods.  The value
  * of duration must be less than 15.  The time corresponding to the first
  * few values are as follows: 0 = 31 msec, 1 = 46 msec, 2 = 77 msec,
  * 3 = 138 msec, 4 = 261 msec, 5 = 507 msec, 6 = 998 msec.
  */
EmberStatus emberStartScan(EmberNetworkScanType scanType,
                           int32u channelMask,
                           int8u duration);

/** @brief Terminates a scan in progress.
  *
  * @return Returns ::EMBER_SUCCESS if successful.
  */
EmberStatus emberStopScan(void);

/** @brief Returns the status of the current scan.  ::EMBER_SUCCESS
  * signals that the scan has completed.  Other error conditions  signify
  * a failure to scan on the channel specified.
  *
  * @param channel  The channel on which the current error occurred.  
  * Undefined for the case of ::EMBER_SUCCESS.
  *
  * @param status   The error condition that occurred on the current channel.
  * Value will be ::EMBER_SUCCESS when the scan has completed.
  */
void emberScanCompleteHandler( int8u channel, EmberStatus status );

/** @brief Reports the maximum RSSI value measured on the channel.
  *
  * @param channel       The 802.15.4 channel number on which the RSSI value
  *   was measured.
  *
  * @param maxRssiValue  The maximum RSSI value measured.
  *
  */
void emberEnergyScanResultHandler(int8u channel, int8u maxRssiValue);

/** @brief Reports that a network was found, and gives the network 
  * parameters useful for deciding which network to join.
  *
  * @param networkFound A pointer to a ::EmberZigbeeNetwork structure 
  *   that contains the discovered network and its associated parameters.
  *
  */
void emberNetworkFoundHandler(EmberZigbeeNetwork *networkFound);

/** @} END addtogroup */

/**
 * <!-- HIDDEN
 * @page 2p5_to_3p0
 * <hr>
 * The file network-formation.h is new and is described in @ref network_formation.
 * <ul>
 * <li> <b>New items</b>
 *   - emberFindAndRejoinNetwork() - replaced emberMobileNodeHasMoved()
 *   - emberRejoinNetwork()
 *   .
 * <li> <b>Items moved from ember.h</b>
 *   - emberEnergyScanResultHandler()
 *   - emberFormNetwork()
 *   - emberInit()
 *   - emberJoinNetwork()
 *   - emberLeaveNetwork()
 *   - emberNetworkFoundHandler()
 *   - emberNetworkInit()
 *   - emberPermitJoining()
 *   - emberScanCompleteHandler()
 *   - emberStartScan()
 *   - emberStopScan()
 *   - emberTick()
 * </ul>
 * HIDDEN -->
 */
