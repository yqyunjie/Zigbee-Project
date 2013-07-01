/**
 * @file form-and-join.h
 * @brief Utilities for forming and joining networks.
 *
 * See @ref networks for documentation.
 *
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.       *80* -->
 *******************************************************************************
 */

/**
 * @addtogroup networks
 * Functions for finding an existing network to join and for
 * finding an unused PAN id with which to form a network.
 *
 * Summary of application requirements:
 *
 * For the EM250:
 * - Define ::EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
 *   in the configuration header.
 * - Call ::emberFormAndJoinTick() regularly in the main loop.
 * - Include form-and-join.c and form-and-join-node-adapter.c in the build.
 * - Optionally include form-and-join-node-callbacks.c in the build.
 *
 * For an EZSP Host:
 * - Define ::EZSP_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
 *   in the configuration header. 
 * - Include form-and-join.c and form-and-join-host-adapter.c in the build.
 * - Optionally include form-and-join-host-callbacks.c in the build.
 *
 * For either platform, the application can omit the
 * form-and-join-*-callback.c file from the build and implement
 * the callbacks itself if necessary.  In this case the appropriate
 * form-and-join callback function must be called from within each callback,
 * as is done within the form-and-join-*-callback.c files.
 * 
 * This library improves upon the form-and-join library from 
 * EmberZNet 3.2 and prior.  The old library is still included in this
 * release as form-and-join3_2.h for back-compatibility. The improved library
 * is able to resume scanning for joinable networks from where it left
 * off, via a call to emberScanForNextJoinableNetwork().  Thus if the first
 * joinable network found is not the correct one, the application can continue
 * scanning without starting from the beginning and without finding the same
 * network that it has already rejected.  The new library can also be used
 * on the host processor.
 *
 * For EZSP-based applications, this library is only needed if the image
 * being used on the network coprocessor (eg EM260) does not implement the
 * form-and-join EZSP commands for flash reasons.  If implemented, the EZSP
 * frames for the equivalent form-and-join functionality are as follows:
 *
 * For joining networks:
 * - scanForJoinableNetwork
 * - startScan with scanType EZSP_NEXT_JOINABLE_NETWORK_SCAN
 * - results via networkFoundHandler callback
 * - errors via scanErrorHandler callback
 *
 * For forming networks:
 * - startScan with scanType EZSP_UNUSED_PAN_ID_SCAN
 * - results via unusedPanIdFoundHandler
 * - errors via scanErrorHandler callback
 *
 * @{
 */

/** @brief Find an unused PAN id.
 *  
 * Does an energy scan on the indicated channels and randomly chooses
 * one from amongst those with the least average energy. Then
 * picks a short PAN id that does not appear during an active
 * scan on the chosen channel.  The chosen PAN id and channel are returned
 * via the ::emberUnusedPanIdFoundHandler() callback.  If an error occurs, the
 * application is informed via the ::emberScanErrorHandler().
 *
 * @param channelMask
 * @param duration  The duration of the energy scan.  See the documentation
 * for ::emberStartScan() in stack/include/network-formation.h for information
 * on duration values.  
 */
void emberScanForUnusedPanId(int32u channelMask, int8u duration);

/** @brief Finds a joinable network.
 *  
 * Performs an active scan on the specified channels looking for networks that:
 * -# currently permit joining,
 * -# match the stack profile of the application,
 * -# match the extended PAN id argument if it is not NULL.
 * 
 * Upon finding a matching network, the application is notified via the
 * emberJoinableNetworkFoundHandler() callback, and scanning stops.
 * If an error occurs during the scanning process, the application is
 * informed via the emberScanErrorHandler(), and scanning stops.
 *
 * If the application determines that the discovered network is not the correct
 * one, it may call emberScanForNextJoinableNetwork() to continue the scanning
 * process where it was left off and find a different joinable network.  If the
 * next network is not the correct one, the application can continue to call 
 * emberScanForNextJoinableNetwork().  Each call must
 * occur within 30 seconds of the previous one, otherwise the state of the scan
 * process is deleted to free up memory.  Calling emberScanForJoinableNetwork()
 * causes any old state to be forgotten and starts scanning from the beginning.
 *
 * @param channelMask  
 * @param extendedPanId
 */
void emberScanForJoinableNetwork(int32u channelMask, int8u* extendedPanId);

/** @brief See emberScanForJoinableNetwork(). */
void emberScanForNextJoinableNetwork(void);

/**
 * With some board layouts, the EM250 and EM260 are susceptible to a dual channel
 * issue in which packets from 12 channels above or below can sometimes be heard 
 * faintly. This affects channels 11 - 14 and 23 - 26. 
 * Hardware reference designs EM250_REF_DES_LAT, version C0 and 
 * EM250_REF_DES_CER, version B0 solve the problem.  
 *
 * Setting the emberEnableDualChannelScan variable to TRUE enables a software
 * workaround to the dual channel issue which can be used with vulnerable boards.  
 * After emberScanForJoinableNetwork() discovers a network on one of the susceptible
 * channels, the channel number that differs by 12 is also scanned.  If the same network 
 * can be heard there, the true channel is determined by comparing the link 
 * quality of the received beacons.  The default value of emberEnableDualChannelScan
 * is TRUE for the EM250 and EM260.  It is not used on other platforms.
 */
extern boolean emberEnableDualChannelScan;

/** @brief Returns true if and only if the form and join library is in the
 * process of scanning and is therefore expecting scan results to be passed
 * to it from the application.
 */
boolean emberFormAndJoinIsScanning(void);

//------------------------------------------------------------------------------
// Callbacks the application needs to implement.

/** @brief A callback the application needs to implement.
 *
 * Notifies the application of the PAN id and channel found
 * following a call to ::emberScanForUnusedPanId().
 *
 * @param panId
 * @param channel
 */
void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel);

/** @brief A callback the application needs to implement.
 *
 * Notifies the application of the network found after a call
 * to ::emberScanForJoinableNetwork() or 
 * ::emberScanForNextJoinableNetwork().
 *
 * @param networkFound
 * @param lqi  The lqi value of the received beacon.
 * @param rssi The rssi value of the received beacon.
 */
void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi);

/** @brief A callback the application needs to implement.
 *  
 * If an error occurs while scanning,
 * this function is called and the scan effort is aborted.
 *
 * Possible return status values are:
 * <ul>
 * <li> EMBER_INVALID_CALL: if emberScanForNextJoinableNetwork() is called 
 * more than 30 seconds after a previous call to emberScanForJoinableNetwork()
 * or emberScanForNextJoinableNetwork().
 * <li> EMBER_NO_BUFFERS: if there is not enough memory to start a scan.
 * <li> EMBER_NO_BEACONS: if no joinable beacons are found.
 * <li> EMBER_MAC_SCANNING: if a scan is already in progress.
 * </ul>
 *
 * @param status  
 */
void emberScanErrorHandler(EmberStatus status);

//------------------------------------------------------------------------------
// Library functions the application must call from within the
// corresponding EmberZNet or EZSP callback.  

/** @brief The application must call this function from within its
 * emberScanCompleteHandler() (on the node) or ezspScanCompleteHandler()
 * (on an EZSP host).  Default callback implementations are provided
 * in the form-and-join-*-callbacks.c files.
 *
 * @return TRUE iff the library made use of the call.
 */
boolean emberFormAndJoinScanCompleteHandler(int8u channel, EmberStatus status);

/** @brief The application must call this function from within its
 * emberNetworkFoundHandler() (on the node) or ezspNetworkFoundHandler()
 * (on an EZSP host).  Default callback implementations are provided
 * in the form-and-join-*-callbacks.c files.
 *
 * @return TRUE iff the library made use of the call.
 */
boolean emberFormAndJoinNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                            int8u lqi,
                                            int8s rssi);

/** @brief The application must call this function from within its
 * emberEnergyScanResultHandler() (on the node) or ezspEnergyScanResultHandler()
 * (on an EZSP host).  Default callback implementations are provided
 * in the form-and-join-*-callbacks.c files.
 *
 * @return TRUE iff the library made use of the call.
 */
boolean emberFormAndJoinEnergyScanResultHandler(int8u channel, int8u maxRssiValue);

/** Used by the form and join code on the node to time out a joinable scan after
 * 30 seconds of inactivity. The application must call emberFormAndJoinTick() 
 * regularly.  This function does not exist for the EZSP host library.
 */
void emberFormAndJoinTick(void);

/** @} // END addtogroup 
 */
