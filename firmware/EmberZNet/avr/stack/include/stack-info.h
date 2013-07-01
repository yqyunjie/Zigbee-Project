/**
 * @file stack-info.h
 * @brief EmberZNet API for accessing and setting stack information.
 * See @ref stack_info for documentation.
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */

/**
 * @addtogroup stack_info
 *
 * See stack-info.h for source code.
 * @{
 */


/** @brief A callback invoked when the status of the stack changes. 
 * If the
 * status parameter equals ::EMBER_NETWORK_UP, then the 
 * ::emberGetNetworkParameters() 
 * function can be called to obtain the new network parameters. If any of the 
 * parameters are being stored in nonvolatile memory by the application, the 
 * stored values should be updated.
 *
 * The application is free to begin messaging once it receives the
 * ::EMBER_NETWORK_UP status.  However, routes discovered immediately after
 * the stack comes up may be suboptimal.  This is because the routes are based
 * on the neighbor table's information about two-way links with neighboring nodes,
 * which is obtained from periodic ZigBee Link Status messages.  It can take
 * two or three link status exchange periods (of 16 seconds each) before the
 * neighbor table has a good estimate of link quality to neighboring nodes.
 * Therefore, the application may improve the quality of initially discovered
 * routes by waiting after startup to give the neighbor table time 
 * to be populated.
 *
 * @param status  Stack status. One of the following:
 * - ::EMBER_NETWORK_UP
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_JOIN_FAILED
 * - ::EMBER_MOVE_FAILED
 * - ::EMBER_CANNOT_JOIN_AS_ROUTER
 * - ::EMBER_NODE_ID_CHANGED
 * - ::EMBER_PAN_ID_CHANGED
 * - ::EMBER_CHANNEL_CHANGED
 * - ::EMBER_NO_BEACONS
 * - ::EMBER_RECEIVED_KEY_IN_THE_CLEAR
 * - ::EMBER_NO_NETWORK_KEY_RECEIVED
 * - ::EMBER_NO_LINK_KEY_RECEIVED
 * - ::EMBER_PRECONFIGURED_KEY_REQUIRED
 */
void emberStackStatusHandler(EmberStatus status);

/** @brief Returns the current join status.
 *
 *   Returns a value indicating whether the node is joining,
 *   joined to, or leaving a network.
 *
 * @return An ::EmberNetworkStatus value indicating the current join status.
 */
EmberNetworkStatus emberNetworkState(void);

/** @brief Indicates whether the stack is currently up.
 *
 *   Returns true if the stack is joined to a network and
 *   ready to send and receive messages.  This reflects only the state
 *   of the local node; it does not indicate whether or not other nodes are
 *   able to communicate with this node.
 *
 * @return True if the stack is up, false otherwise.
 */
boolean emberStackIsUp(void);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Returns the EUI64 ID of the local node.
 *
 * @return The 64-bit ID.
 */
EmberEUI64 emberGetEui64(void);

/** @brief Determines whether \c eui64 is the local node's EUI64 ID.
 *
 * @param eui64  An EUI64 ID.
 *
 * @return TRUE if \c eui64 is the local node's ID, otherwise FALSE.
 */
boolean emberIsLocalEui64(EmberEUI64 eui64);

/** @brief Returns the 16-bit node ID of the local node.
 *
 * @return The 16-bit ID.
 */
EmberNodeId emberGetNodeId(void);

/** @brief Sets the manufacturer code to the specified value. The
 * manufacturer code is one of the fields of the node descriptor.
 * 
 * @param code  The manufacturer code for the local node.
 */
void emberSetManufacturerCode(int16u code);

/** @brief Sets the power descriptor to the specified value. The power
 * descriptor is a dynamic value, therefore you should call this function
 * whenever the value changes.
 * 
 * @param descriptor  The new power descriptor for the local node.
 */
void emberSetPowerDescriptor(int16u descriptor);

#else   // Doxgyen ignores the following
extern EmberEUI64 emLocalEui64;
#define emberGetEui64() (emLocalEui64)
#define emberIsLocalEui64(eui64)                           \
(MEMCOMPARE((eui64), emLocalEui64, EUI64_SIZE) == 0)

#pragma pagezero_on  // place this function in zero-page memory for xap
EmberNodeId emberGetNodeId(void);
#pragma pagezero_off

extern int16u emManufacturerCode;
extern int16u emPowerDescriptor;
#define emberSetManufacturerCode(code)                     \
(emManufacturerCode = (code))
#define emberSetPowerDescriptor(descriptor)                \
(emPowerDescriptor = (descriptor))
#endif

/** @brief Copies the current network parameters into the structure
 * provided by the caller.
 *
 * @param parameters  A pointer to an EmberNetworkParameters value
 *  into which the current network parameters will be copied.
 *
 * @return An ::EmberStatus value indicating the success or
 *  failure of the command. 
 */
EmberStatus emberGetNetworkParameters(EmberNetworkParameters *parameters);

/** @brief Copies the current node type into the location provided
 * by the caller.
 *
 * @param resultLocation  A pointer to an EmberNodeType value
 *  into which the current node type will be copied.
 *
 * @return An ::EmberStatus value that indicates the success or failure
 *  of the command. 
 */
EmberStatus emberGetNodeType(EmberNodeType *resultLocation);

/** @brief Sets the channel to use for sending and receiving messages.
 * For a list of 
 * available radio channels, see the technical specification for 
 * the RF communication module in your Developer Kit.
 *
 * Note: Care should be taken when using this API,
 * as all devices on a network must use the same channel.
 *
 * @param channel  Desired radio channel.
 *
 * @return An ::EmberStatus value indicating the success or
 *  failure of the command. 
 */
EmberStatus emberSetRadioChannel(int8u channel);

/** @brief Gets the radio channel to which a node is set. The 
 * possible return values depend on the radio in use. For a list of 
 * available radio channels, see the technical specification for 
 * the RF communication module in your Developer Kit.
 *
 * @return Current radio channel.
 */
int8u emberGetRadioChannel(void);

/** @brief Sets the radio output power at which a node is operating. Ember 
 * radios have discrete power settings. For a list of available power settings,
 * see the technical specification for the RF communication module in 
 * your Developer Kit. 
 * Note: Care should be taken when using this api on a running network,
 * as it will directly impact the established link qualities neighboring nodes
 * have with the node on which it is called.  This can lead to disruption of
 * existing routes and erratic network behavior.
 *
 * @param power  Desired radio output power, in dBm.
 *
 * @return An ::EmberStatus value indicating the success or
 *  failure of the command. 
 */
EmberStatus emberSetRadioPower(int8s power);

/** @brief Gets the radio output power at which a node is operating. Ember 
 * radios have discrete power settings. For a list of available power settings,
 * see the technical specification for the RF communication module in 
 * your Developer Kit. 
 *
 * @return Current radio output power, in dBm.
 */
int8s emberGetRadioPower(void);

/** @brief Returns a node's 802.15.4 PAN identifier.
 *
 * @return A PAN ID.
 */
EmberPanId emberGetPanId(void);

/** @brief Fetches a node's 8 byte Extended PAN identifier. */
void emberGetExtendedPanId(int8u *resultLocation);

/** @brief The application must provide a definition for this variable. */
extern PGM int8u emberStackProfileId[];

/** @brief Endpoint information (a ZigBee Simple Descriptor).
 *
 * This is a ZigBee Simple Descriptor and contains information
 * about an endpoint.  This information is shared with other nodes in the
 * network by the ZDO.
 */

typedef struct {
  /** Identifies the endpoint's application profile. */
  int16u profileId;
  /** The endpoint's device ID within the application profile. */
  int16u deviceId;
  /** The endpoint's device version. */
  int8u deviceVersion;
  /** The number of input clusters. */
  int8u inputClusterCount;
  /** The number of output clusters. */
  int8u outputClusterCount;
} EmberEndpointDescription;

/** @brief Gives the endpoint information for a particular endpoint.
 */

typedef struct {
  /** An endpoint of the application on this node. */
  int8u endpoint;
  /** The endpoint's description. */
  EmberEndpointDescription PGM * description;
  /** Input clusters the endpoint will accept. */
  int16u PGM * inputClusterList;
  /** Output clusters the endpoint may send. */
  int16u PGM * outputClusterList;
} EmberEndpoint;

/** @brief The application must provide a definition for this variable. */
extern int8u emberEndpointCount;

/** @brief If emberEndpointCount is nonzero, the application must
 * provide descriptions for each endpoint.  
 *
 * This can be done either
 * by providing a definition of emberEndpoints or by providing definitions
 * of ::emberGetEndpoint(), ::emberGetEndpointDescription() and
 * ::emberGetEndpointCluster().  Using the array is often simpler, but consumes
 * large amounts of memory if emberEndpointCount is large.
 *
 * If the application provides definitions for the three functions, it must
 * define EMBER_APPLICATION_HAS_GET_ENDPOINT in its
 * CONFIGURATION_HEADER.
 */
extern EmberEndpoint emberEndpoints[];

/** @brief Retrieves the endpoint number for
 * the index'th endpoint.  <code> index </code> must be less than
 * the value of emberEndpointCount.
 * 
 * This function is provided by the stack, using the data from
 * emberEndpoints, unless the application defines
 * EMBER_APPLICATION_HAS_GET_ENDPOINT in its CONFIGURATION_HEADER.
 * 
 * @param index  The index of an endpoint (as distinct from its endpoint
 * number).  This must be less than the value of emberEndpointCount.
 * 
 * @return The endpoint number for the index'th endpoint.
 */
int8u emberGetEndpoint(int8u index);

/** @brief Retrieves the endpoint description for the
 * given endpoint.
 * 
 * This function is provided by the stack, using the data from
 * emberEndpoints, unless the application defines
 * ::EMBER_APPLICATION_HAS_GET_ENDPOINT in its ::CONFIGURATION_HEADER.
 * 
 * @param endpoint  The endpoint whose description is to be returned.
 * 
 * @param result    A pointer to the location to which to copy the endpoint
 *    description.
 * 
 * @return TRUE if the description was copied to result or FALSE if the
 * endpoint is not active.
 */
boolean emberGetEndpointDescription(int8u endpoint,
                                    EmberEndpointDescription *result);

/** @brief Retrieves a cluster ID from one of the cluster lists associated
 * with the given endpoint.
 * 
 * This function is provided by the stack, using the data from
 * emberEndpoints, unless the application defines
 * ::EMBER_APPLICATION_HAS_GET_ENDPOINT in its CONFIGURATION_HEADER.
 * 
 * @param endpoint   The endpoint from which the cluster ID is to be read.
 * 
 * @param listId     The list from which the cluster ID is to be read.
 * 
 * @param listIndex  The index of the desired cluster ID in the list. This value
 * must be less than the length of the list. The length can be found in the
 * EmberEndpointDescription for this endpoint.
 * 
 * @return The cluster ID at position listIndex in the specified endpoint
 * cluster list.
 */
int16u emberGetEndpointCluster(int8u endpoint,
                               EmberClusterListId listId,
                               int8u listIndex);

/** @brief Determines whether \c nodeId is valid.
 *
 * @param nodeId  A node ID.
 *
 * @return TRUE if \c nodeId is valid, FALSE otherwise.
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
boolean emberIsNodeIdValid(EmberNodeId nodeId);
#else
#define emberIsNodeIdValid(nodeId) ((nodeId) < EMBER_DISCOVERY_ACTIVE_NODE_ID)
#endif

/** @brief Returns the node ID that corresponds to the specified EUI64.
 * The node ID is found by searching through all stack tables for the specified
 * EUI64.
 *
 * @param eui64  The EUI64 of the node to look up.
 *
 * @return The short ID of the node or ::EMBER_NULL_NODE_ID if the short ID 
 * is not known.
 */
EmberNodeId emberLookupNodeIdByEui64(EmberEUI64 eui64);

/** @brief Returns the EUI64 that corresponds to the specified node ID.
 * The EUI64 is found by searching through all stack tables for the specified
 * node ID.
 *
 * @param nodeId       The short ID of the node to look up.
 *
 * @param eui64Return  The EUI64 of the node is copied here if it is known.
 * 
 * @return An ::EmberStatus value:\n\n
 * - ::EMBER_SUCCESS - eui64Return has been set to the EUI64 of the node.
 * - ::EMBER_ERR_FATAL - The EUI64 of the node is not known.
 */
EmberStatus emberLookupEui64ByNodeId(EmberNodeId nodeId,
                                     EmberEUI64 eui64Return);

/** @brief A callback invoked to inform the application of the 
 * occurrence of an event defined by ::EmberCountersType, for example,
 * transmissions and receptions at different layers of the stack.
 *
 * The application must define ::EMBER_APPLICATION_HAS_COUNTER_HANDLER
 * in its CONFIGURATION_HEADER to use this.
 * This function may be called in ISR context, so processing should
 * be kept to a minimum.
 * 
 * @param type  The type of the event.
 * @param data  For transmission events, the number of retries used.
 * For other events, this parameter is unused and is set to zero.
 */
void emberCounterHandler(EmberCounterType type, int8u data);

/** @brief Copies a neighbor table entry to the structure that 
 * \c result points to.  Neighbor table entries are stored in 
 * ascending order by node id, with all unused entries at the end
 * of the table.  The number of active neighbors can be obtained
 * using ::emberNeighborCount().
 *
 * @param index   The index of a neighbor table entry.
 *
 * @param result  A pointer to the location to which to copy the neighbor
 * table entry.
 *
 * @return ::EMBER_ERR_FATAL if the index is greater or equal to the
 * number of active neighbors, or if the device is an end device.  
 * Returns ::EMBER_SUCCESS otherwise.
 */
EmberStatus emberGetNeighbor(int8u index, EmberNeighborTableEntry *result);

/** @brief Copies a route table entry to the structure that
 * \c result points to. Unused route table entries have destination
 * 0xFFFF.  The route table size
 * can be obtained via ::emberRouteTableSize().
 *
 * @param index   The index of a route table entry.
 * 
 * @param result  A pointer to the location to which to copy the route
 * table entry.
 *
 * @return ::EMBER_ERR_FATAL if the index is out of range or the device
 * is an end device, and ::EMBER_SUCCESS otherwise.
 */
EmberStatus emberGetRouteTableEntry(int8u index, EmberRouteTableEntry *result);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Returns the stack profile of the network which the
  * node has joined.
  *
  * @return  stack profile
  */
int8u emberStackProfile(void);

/** @brief Returns the depth of the node in the network.
  *
  * @return current depth
  */
int8u emberTreeDepth(void);
#endif
/** @brief Returns the number of active entries in the neighbor table.
 *
 * @return number of active entries in the neighbor table
 */
int8u emberNeighborCount(void);
#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Returns the size of the route table.
 *
 * @return the size of the route table
 */
int8u emberRouteTableSize(void);

#else   // Doxgyen ignores the following
// The '+ 0' prevents anyone from accidentally assigning to these.
#define emberStackProfile()        (emStackProfile      + 0)
#define emberTreeDepth()           (emTreeDepth         + 0)
#define emberMaxDepth()            (emMaxDepth          + 0)
#define emberSecurityLevel()       (emZigbeeNetworkSecurityLevel + 0)
#define emberRouteTableSize()      (emRouteTableSize    + 0)

extern int8u emStackProfile;
extern int8u emZigbeeNetworkSecurityLevel;
extern int8u emMaxHops;
extern int8u emNeighborCount;
extern int8u emRouteTableSize;
extern int8u emMaxDepth;                // maximum tree depth
extern int8u emTreeDepth;               // our depth
#endif

/** @name Radio-specific Functions*/
//@{

/** @brief Enables boost power mode and/or the alternate transmitter
  * output on the em250.  
  *
  * Boost power mode is a high performance radio mode
  * which offers increased receive sensitivity and transmit power at the cost of
  * an increase in power consumption.  The alternate transmitter output allows
  * for simplified connection to an external power amplifier via the
  * RF_TX_ALT_P and RF_TX_ALT_N pins on the em250.  ::emberInit() calls this
  * function using the power mode and transmitter output settings as specified
  * in the MFG_PHY_CONFIG token (with each bit inverted so that the default
  * token value of 0xffff corresponds to normal power mode and bi-directional RF
  * transmitter output).  The application only needs to call
  * ::emberSetTxPowerMode() if it wishes to use a power mode or transmitter output
  * setting different from that specified in the MFG_PHY_CONFIG token.  If the
  * application does call ::emberSetTxPowerMode() it must do so after it calls
  * ::emberInit(), otherwise the default settings will be reapplied by
  * ::emberInit().  After this initial call to ::emberSetTxPowerMode(), the stack
  * will automatically maintain the specified power mode configuration across
  * sleep/wake cycles.
  *
  * @note This function is only available when running on the em250.
  *
  * @note This function does not alter the MFG_PHY_CONFIG token.  The
  * MFG_PHY_CONFIG token must be properly configured to ensure optimal radio
  * performance when the standalone bootloader runs in recovery mode.  The
  * MFG_PHY_CONFIG can only be set using external tools.  If your product uses
  * boost mode or the alternate transmitter output and the standalone bootloader
  * contact support@ember.com for instructions on how to set the MFG_PHY_CONFIG
  * token appropriately.
  *
  * @param txPowerMode  Specifies which of the transmit power mode options are
  * to be activated.  This parameter should be set one of the literal values
  * described in stack/include/ember-types.h.  Any power option not specified 
  * in the txPowerMode parameter will be deactivated.
  * 
  * @return ::EMBER_SUCCESS if successful; an error code otherwise.
  */
EmberStatus emberSetTxPowerMode(int16u txPowerMode);


/** @brief The radio calibration callback function.
 *
 * The Voltage Controlled Oscillator (VCO) on the em250 can drift with
 * temperature changes.  During every call to ::emberTick(), the stack will check
 * to see if the VCO has drifted.  If the VCO has drifted, the stack will call
 * ::emberRadioNeedsCalibratingHandler() to inform the application that it should perform
 * calibration of the current channel as soon as possible.  Calibration can take
 * up to 150ms.  The default callback function implementation provided here
 * performs calibration immediately.  If the application wishes, it can define
 * its own callback by defining ::EMBER_APPLICATION_HAS_CUSTOM_RADIO_CALIBRATION_CALLBACK
 * in its CONFIGURATION_HEADER.  It can then 
 * failsafe any critical processes or peripherals before calling
 * ::emberCalibrateCurrentChannel().  This callback function will be called once
 * during every call to ::emberTick() until the radio has been sucessfully
 * calibrated.  
 */
void emberRadioNeedsCalibratingHandler(void);

/** @brief Calibrates the current channel.  The stack will notify the
  * application of the need for channel calibration via the
  * ::emberRadioNeedsCalibratingHandler() callback function during ::emberTick().  This
  * function should only be called from within the context of the
  * ::emberRadioNeedsCalibratingHandler() callback function.  Calibration can take up
  * to 150ms.
  * 
  * @note This function is only available when running on the em250.
  */
void emberCalibrateCurrentChannel(void);
//@}//END Radio-specific functions


/** @name General Functions */
//@{

/** @brief This function copies an array of bytes and reverses the order
 *  before writing the data to the destination.
 *
 * @param dest A pointer to the location where the data will be copied to.
 * @param src A pointer to the location where the data will be copied from.
 * @param length The length (in bytes) of the data to be copied.
 */
void emberReverseMemCopy(int8u* dest, const int8u* src, int8u length);


/** @brief Returns the value built from the two \c int8u values
 *  \c contents[0] and \c contents[1]. \c contents[0] is the low byte.
 */
#pragma pagezero_on  // place this function in zero-page memory for xap 
int16u emberFetchLowHighInt16u(int8u *contents);
#pragma pagezero_off

/** @brief Stores \c value in \c contents[0] and \c contents[1]. \c
 *  contents[0] is the low byte.
 */
void emberStoreLowHighInt16u(int8u *contents, int16u value);

#if !defined DOXYGEN_SHOULD_SKIP_THIS
int32u emFetchInt32u(boolean lowHigh, int8u* contents);
#endif

/** @brief Returns the value built from the four \c int8u values
 *  \c contents[0], \c contents[1], \c contents[2] and \c contents[3]. \c
 *  contents[0] is the low byte.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
int32u emberFetchLowHighInt32u(int8u *contents);
#else
#define emberFetchLowHighInt32u(contents) \
  (emFetchInt32u(TRUE, contents))
#endif

/** @description Returns the value built from the four \c int8u values
 *  \c contents[0], \c contents[1], \c contents[2] and \c contents[3]. \c
 *  contents[3] is the low byte.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
int32u emberFetchHighLowInt32u(int8u *contents);
#else
#define emberFetchHighLowInt32u(contents) \
  (emFetchInt32u(FALSE, contents))
#endif



#if !defined DOXYGEN_SHOULD_SKIP_THIS
void emStoreInt32u(boolean lowHigh, int8u* contents, int32u value);
#endif

/** @brief Stores \c value in \c contents[0], \c contents[1], \c
 *  contents[2] and \c contents[3]. \c contents[0] is the low byte.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
void emberStoreLowHighInt32u(int8u *contents, int32u value);
#else
#define emberStoreLowHighInt32u(contents, value) \
  (emStoreInt32u(TRUE, contents, value))
#endif

/** @description Stores \c value in \c contents[0], \c contents[1], \c
 *  contents[2] and \c contents[3]. \c contents[3] is the low byte.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
void emberStoreHighLowInt32u(int8u *contents, int32u value);
#else
#define emberStoreHighLowInt32u(contents, value) \
  (emStoreInt32u(FALSE, contents, value))
#endif

/** @} END GENERALLY USEFUL DEFINES */

/** @} END addtogroup */

/** 
 * <!-- HIDDEN
 * @page 2p5_to_3p0
 * <hr>
 * The file stack-info.h is new and is described in @ref stack_info.
 * <ul>
 * <li> <b>New items</b>
 * <ul>
 * <li> emberIsNodeIdValid()
 * <li> emberLookupEui64ByNodeId()
 * <li> emberLookupNodeIdByEui64()
 * <li> emberCounterHandler()
 * <li> emberGetExtendedPanId()
 * <li> emberGetRouteTableEntry()
 * <li> emberRadioNeedsCalibratingHandler()
 * <li> emberRouteTableSize()
 * </ul>
 * <li> <b>Changed items</b>
 * <ul>
 * <li> EmberEndpoint - Cluster ids changed from int8u to int16u.
 * </ul>
 * <li> <b>Functions moved from ember.h</b>
 *  - emberSetTxPowerMode()
 *  - emberCalibrateCurrentChannel()
 *  -  emberStackStatusHandler()
 *  - emberNetworkState()
 *  -  emberStackIsUp() 
 *  -  emberGetEui64()
 *  -  emberIsLocalEui64()
 *  -  emberGetNodeId()
 *  -  emberSetManufacturerCode()
 *  -  emberSetPowerDescriptor()
 *  -  emberGetNetworkParameters()
 *  -  emberGetNodeType()
 *  -  emberGetRadioChannel()
 *  -  emberGetRadioPower()
 *  -  emberGetPanId()
 *  -  emberGetEndpoint()
 *  -  emberGetEndpointDescription()
 *  -  emberGetEndpointCluster()
 *  -  emberGetNeighbor()
 *  -  emberStackProfile()
 *  -  emberTreeDepth()
 *  -  emberNeighborCount()
 * </ul>
 * HIDDEN -->
 */

