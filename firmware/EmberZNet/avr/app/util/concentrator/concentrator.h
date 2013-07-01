/**
 * @file: concentrator.h
 *
 * Description: In ZigBee, a concentrator is a router or coordinator that
 * uses many-to-one routes and source routing to communicate with a large
 * number of other ZigBee nodes.  This library provides support for standard
 * route maintenance activities that must be performed by a concentrator.
 * (For managing source routes, see app/util/source-route*.)
 * This library can be used both on an EZSP host processor as well as on
 * an EmberZNet node (eg, EM250).
 *
 * Summary of application requirements:
 * - Call ::emberConcentratorTick() regularly in the main loop.
 * - Call ::emberConcentratorNoteRouteError() from the route error handler.
 * - Call ::emberConcentratorNoteDeliveryFailure() from the message sent handler.
 * - Implement ::emberConcentratorRouteDiscoverySentHandler().
 * - Include ::halCommonGetInt16uQuarterSecondTick() in the build.
 *
 * This library manages sending of many-to-one route requests periodically.
 * A many-to-one route request, which we will also call a "discovery",
 * is a broadcast ZigBee network command which establishes routes back
 * to the concentrator on all routers in the network that hear it.
 * The radius of the discovery broadcast can be limited by defining
 * EMBER_CONCENTRATOR_RADIUS in the CONFIGURATION_HEADER. 
 * It is set to ::EMBER_MAX_HOPS by default.
 *
 * Discoveries must be sent periodically because wireless network topologies 
 * change over time even in networks without mobility, so that routes must be
 * repaired and reestablished.  However, because a discovery consumes a large 
 * amount of network bandwidth, it is important to limit the rate at which they 
 * are sent.  A minimum and maximum rate of discovery can be defined by the 
 * application in the CONFIGURATION_HEADER by defining
 * ::EMBER_CONCENTRATOR_MIN_SECONDS_BETWEEN_DISCOVERIES
 * and ::EMBER_CONCENTRATOR_MAX_SECONDS_BETWEEN_DISCOVERIES
 * The appropriate settings depend on the number of nodes in the network,
 * and the bandwidth and latency requirements of the application.
 * Since a ZigBee network can support approximately one broadcast per second,
 * Ember recommends a minimum of 10 seconds between discoveries, which 
 * is the default value.
 * 
 * The application can request that a discovery be triggered immediately by
 * calling ::emberConcentratorQueueRouteDiscovery().  A new discovery will
 * be sent as soon as the minimum time since the previous discovery has
 * passed.  
 *
 * When an APS unicast encounters a broken link en route to its destination,
 * the ZigBee network layer generates a route error message.  If the source
 * or destination of the unicast is a concentrator, the network layer attempts
 * to route the error back to the concentrator.  This is because it is the
 * concentrator's responsibility to the repair the route by issuing a
 * many-to-one route request.  
 * 
 * Thus, the application on the concentrator should inform this library of incoming 
 * route errors received via the ::emberIncomingRouteErrorHandler(), by calling 
 * ::emberConcentratorNoteRouteError().  (On an EZSP host processor the callback
 * is named ::ezspIncomingRouteErrorHandler().)  Once the number of route errors
 * reaches ::EMBER_CONCENTRATOR_ROUTE_ERROR_THRESHOLD, a new discovery is 
 * queued for sending, subject to the minimum delay as usual.  Note that an
 * APS unicast is retried up to three times, and the network layer may generate
 * a separate route error for each end-to-end retry that encounters a broken
 * link.  Also note that it is possible for a route error to fail to reach 
 * the concentrator.  Thus route errors are not guaranteed to alert the 
 * concentrator of a routing problem.
 *
 * A more deterministic indication of a routing problem is the failure of
 * a retried APS unicast sent by the concentrator.  The application is always 
 * notified of such a failure via the ::emberMessageSent() callback, or the 
 * ::ezspMessageSent() callback on an EZSP host, with a status parameter
 * of ::EMBER_DELIVERY_FAILED.  The application should then notify the
 * concentrator library of the failure by calling 
 * ::emberConcentratorNoteDeliveryFailure().  Once the number of delivery
 * failures reaches ::EMBER_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD, a new
 * discovery is queued for sending.  The default threshold is a single
 * failure.  This could be increased if a single failure is not problematic,
 * or if the application performs its own application level retries.
 *
 * There are two types of concentrators: ::EMBER_HIGH_RAM_CONCENTRATOR
 * and ::EMBER_LOW_RAM_CONCENTRATOR.  High ram is used by default, and
 * the application can override it by defining EMBER_CONCENTRATOR_TYPE
 * in the CONFIGURATION_HEADER.  
 *
 * The application may wish to be informed by the library whenever
 * a discovery is actually sent.  This is accomplished by implementing
 * the ::emberConcentratorRouteDiscoverySentHandler() callback.
 *
 * Copyright 2008 by Ember Corporation.  All rights reserved.               *80*
 */

/** The type of the concentrator, either
 * ::EMBER_HIGH_RAM_CONCENTRATOR or ::EMBER_LOW_RAM_CONCENTRATOR.
 * The default value is EMBER_HIGH_RAM_CONCENTRATOR.
 */
#ifndef EMBER_CONCENTRATOR_TYPE
  #define EMBER_CONCENTRATOR_TYPE EMBER_HIGH_RAM_CONCENTRATOR
#endif

/** The maximum number of hops many-to-one route requests
 * will be relayed.  A radius of zero is converted to ::EMBER_MAX_HOPS.
 * The default value is 0.
 */
#ifndef EMBER_CONCENTRATOR_RADIUS
  #define EMBER_CONCENTRATOR_RADIUS EMBER_MAX_HOPS
#endif

/** Many-to-one route discoveries
 * will be sent at least this frequently by the library.  Ember recommends that 
 * this value be set to 10 minutes or less.
 * The default value is 30 seconds.
 */
#ifndef EMBER_CONCENTRATOR_MAX_SECONDS_BETWEEN_DISCOVERIES
  #define EMBER_CONCENTRATOR_MAX_SECONDS_BETWEEN_DISCOVERIES 30
#endif

/** Many-to-one route discoveries will not be sent more often than this value.  
 * The default value is 10 seconds.
 */
#ifndef EMBER_CONCENTRATOR_MIN_SECONDS_BETWEEN_DISCOVERIES
  #define EMBER_CONCENTRATOR_MIN_SECONDS_BETWEEN_DISCOVERIES 10
#endif

/** The number of route error messages which
 * will trigger a many-to-one route discovery.  
 * The default value is 3.
 */
#ifndef EMBER_CONCENTRATOR_ROUTE_ERROR_THRESHOLD
  #define EMBER_CONCENTRATOR_ROUTE_ERROR_THRESHOLD 3
#endif

/** The number of failed APS unicast messages which
 * will trigger a many-to-one-route discovery.
 * The default value is 1.
 */
#ifndef EMBER_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD
  #define EMBER_CONCENTRATOR_DELIVERY_FAILURE_THRESHOLD 1
#endif

/** @brief Starts periodic many-to-one route discovery.
 * Periodic discovery is started by default on bootup,
 * but this function may be used if discovery has been
 * stopped by a call to ::emberConcentratorStopDiscovery().
 */
void emberConcentratorStartDiscovery(void);

/** @brief Stops periodic many-to-one route discovery. */
void emberConcentratorStopDiscovery(void);

/**
 * @brief The application must notify the library of incoming route
 * errors by calling this function.
 *
 * @param status The status of the route error as reported by
 * ::emberIncomingRouteErrorHandler() or ::ezspIncomingRouteErrorHandler().
 */
void emberConcentratorNoteRouteError(EmberStatus status);

/**
 * @brief The application must notify the library of failed outgoing APS
 * Unicast messages by calling this function.
 *
 * @param status The status of the failed APS unicast message as
 * reported by ::emberMessageSent() or ::ezspMessageSent().
 */
void emberConcentratorNoteDeliveryFailure(EmberOutgoingMessageType type,
                                          EmberStatus status);

/**
 * @brief Tells the library to send a many-to-one route discovery as soon
 * as ::emberConcentratorMinSecondsBetweenDiscoveries seconds have past
 * since the last discovery.
 */
void emberConcentratorQueueRouteDiscovery(void);

/**
 * @brief A callback informing the application that a many-to-one 
 * route discovery has just been sent.
 */
void emberConcentratorRouteDiscoverySentHandler(void);

/**
 * @brief The application must call this function in its main loop
 * so that the library can perform any necessary tasks.
 */
void emberConcentratorTick(void);
