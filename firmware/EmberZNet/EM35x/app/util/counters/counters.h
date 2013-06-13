/**
 * @file: counters.h
 *
 * A library to tally up Ember stack counter events.  
 *
 * The Ember stack tracks a number of events defined by ::EmberCounterType
 * and reports them to the app via the ::emberCounterHandler() callback.
 * This library simply keeps a tally of the number of times each type of
 * counter event occurred.  The application must define 
 * ::EMBER_APPLICATION_HAS_COUNTER_HANDLER in its CONFIGURATION_HEADER
 * in order to use this library.
 *
 * See counters-ota.h for the ability to retrieve stack counters from a 
 * remote node over the air.
 *
 * Copyright 2007 by Ember Corporation. All rights reserved.                *80*
 */

/** 
 * The ith entry in this array is the count of events of EmberCounterType i. 
 */
extern int16u emberCounters[EMBER_COUNTER_TYPE_COUNT];


#if !defined(EMBER_MULTI_NETWORK_STRIPPED)
#define MULTI_NETWORK_COUNTER_TYPE_COUNT 17
/**
 * The value at the position [n,i] in this matrix is the count of events of
 * per-network EmberCounterType i for network n.
 */
extern int16u emberMultiNetworkCounters[EMBER_SUPPORTED_NETWORKS]
                                       [MULTI_NETWORK_COUNTER_TYPE_COUNT];
#endif // EMBER_MULTI_NETWORK_STRIPPED

/** Reset the counters to zero. */
void emberClearCounters(void);
