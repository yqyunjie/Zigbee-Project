/*
 * @file zll-api.h
 *
 * @brief AI for sending and receiving ZigBee Light Link (ZLL) messages.
 *
 * <!--Copyright 2010 by Ember Corporation. All rights reserved.         *80*-->
 */

/**
 * @addtogroup zll
 * See zll-api.h for source code.
 * @{
 */

/** @brief This will set the device type as a router or end device 
 * (depending on the passed nodeType) and setup a ZLL 
 * commissioning network with the passed parameters.  If panId is 0xFFFF,
 * a random PAN ID will be generated.  If extendedPanId is set to all F's, 
 * then a random extended pan ID will be generated.  If channel is 0xFF,
 * then channel 11 will be used.
 * If all F values are passed for PAN Id or Extended PAN ID then the
 * randomly generated values will be returned in the passed structure.
 * 
 * @param networkInfo A pointer to an ::EmberZllNetwork struct indicating the
 *   network parameters to use when forming the network.  If random values are
 *   requested, the stack's randomly generated values will be returned in the
 *   structure.
 * @param radioTxPower the radio output power at which a node is to operate.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllFormNetwork(EmberZllNetwork* networkInfo,
                                int8s radioTxPower);

/** @brief This call will cause the device to send a NWK start or join to the
 *  target device and cause the remote AND local device to start operating
 *  on a network together.  If the local device is a factory new device
 *  then it will send a ZLL NWK start to the target requesting that the
 *  target generate new network parameters.  If the device is 
 *  not factory new then the local device will send a NWK join request
 *  using the current network parameters. 
 *
 * @param targetNetworkInfo A pointer to an ::EmberZllNetwork structure that
 *   indicates the info about what device to send the NWK start/join
 *   request to.  This information must have previously been returned
 *   from a ZLL scan.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllJoinTarget(const EmberZllNetwork* targetNetworkInfo);


/** @brief This call will cause the device to setup the security information
 *    used in its network.  It must be called prior to forming, starting, or
 *    joining a network.  
 *    
 * @param networkKey is a pointer to an ::EmberKeyData structure containing the
 *   value for the network key.  If the value is set to all F's, then a random
 *   network key will be generated.
 * @param securityState The security configuration to be set.
 *  
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllSetInitialSecurityState(const EmberKeyData *networkKey,
                                            const EmberZllInitialSecurityState *securityState);


/**
 * @brief This call will initiate a ZLL network scan on all the specified 
 *   channels.  Results will be returned in ::emberZllNetworkFoundHandler().
 *
 * @param channelMask indicating the range of channels to scan.
 * @param radioPowerForScan the radio output power used for the scan requests.
 * @param nodeType the the node type of the local device.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */ 
EmberStatus emberZllStartScan(int32u channelMask,
                              int8s radioPowerForScan,
                              EmberNodeType nodeType);

/**
 * @brief This call will change the mode of the radio so that the receiver is
 * on when the device is idle.  Changing the idle mode permits the application
 * to communicate with the target of a touch link before the network is
 * established.  The receiver will remain on until the duration elapses, the
 * duration is set to zero, or ::emberZllJoinTarget is called.
 * 
 * @param durationMs The duration in milliseconds to leave the radio on.
 *
 * @return An ::EmberStatus value indicating whether the operation succeeded or
 * why it failed.
 */
EmberStatus emberZllSetRxOnWhenIdle(int16u durationMs);

/**
 * @brief This call is fired when a ZLL network scan finds a ZLL network.
 *   The network information will be returned to the application for
 *   processing.
 * 
 * @param networkInfo is a pointer to an ::EmberZllNetwork struct containing
 *   the Zigbee and ZLL specific information about the discovered network.
 * @param deviceInfo is a pointer to an ::EmberZllDeviceInfoRecord struct
 *   containing the device specific info.  This pointer may be NULL,
 *   indicating the device has either 0 sub-devices, or more than 1
 *   sub-devices.
 */
void emberZllNetworkFoundHandler(const EmberZllNetwork* networkInfo,
                                 const EmberZllDeviceInfoRecord* deviceInfo);

/**
 * @brief This call is fired when a ZLL network scan is complete.
 *
 * @param status An ::EmberStatus value indicating whether the operation
 * succeeded, or why it failed.  If the status is not ::EMBER_SUCCESS, the
 * application should not attempt to start or join a network returned via
 * ::emberZllNetworkFoundHandler.
 */
void emberZllScanCompleteHandler(EmberStatus status);

/**
 * @brief This call is fired when network and group addresses are assigned to
 * a remote mode in a network start or network join request.
 *
 * @param addressInfo is a pointer to an ::EmberZllAddressAssignment struct
 *   containing the address assignment information.
 */
void emberZllAddressAssignmentHandler(const EmberZllAddressAssignment* addressInfo);

/**
 * @brief This call is fired when the device is a target of a touch link.
 *
 * @param networkInfo is a pointer to an ::EmberZllNetwork struct containing
 *   the Zigbee and ZLL specific information about the initiator.
 */
void emberZllTouchLinkTargetHandler(const EmberZllNetwork *networkInfo);

/**
 * @brief This call reads the Zll Stack data token.
 */
void emberZllGetTokenStackZllData(EmberTokTypeStackZllData *token);

/**
 * @brief This call reads the Zll Stack security token.
 */
void emberZllGetTokenStackZllSecurity(EmberTokTypeStackZllSecurity *token);

/**
 * @brief This call reads both the Zll Stack data and security tokens.
 */
void emberZllGetTokensStackZll(EmberTokTypeStackZllData *data,
                               EmberTokTypeStackZllSecurity *security);

/**
 * @brief This call sets the Zll Stack data token.
 */
void emberZllSetTokenStackZllData(EmberTokTypeStackZllData *token);

/**
 * @brief This call returns whether or not the network is a ZLL network.
 */
boolean emberIsZllNetwork(void);

/**
 * @brief This call will alter the ZLL data token to reflect the fact that the
 * network is non-ZLL.
 */
void emberZllSetNonZllNetwork(void);

/** @} // END addtogroup
 */
