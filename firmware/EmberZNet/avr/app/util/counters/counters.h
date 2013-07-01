/**
 * @file: counters.h
 *
 * A library to tally up Ember stack counter events.  
 *
 * The Ember stack tracks a number of events defined by ::EmberCountersType
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

/** Reset the counters to zero. */
void emberClearCounters(void);
