// File: counters.c
//
// Description: implements emberCounterHandler() and keeps a tally
// of the events reported by the stack.  The application must define
// EMBER_APPLICATION_HAS_COUNTER_HANDLER in its configuration header
// to use this file.
//
// Author(s): Matteo Paris, matteo@ember.com
//
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember.h"

int16u emberCounters[EMBER_COUNTER_TYPE_COUNT];

// Implement the stack callback by simply tallying up the counts.
void emberCounterHandler(EmberCounterType type, int8u data)
{
  if (emberCounters[type] < 0xFFFF)
    emberCounters[type] += 1;
  
  // The retry counts need to be handled as a separate case.
  if (EMBER_COUNTER_MAC_TX_UNICAST_SUCCESS == type
      || EMBER_COUNTER_MAC_TX_UNICAST_FAILED == type)
    emberCounters[EMBER_COUNTER_MAC_TX_UNICAST_RETRY] += data;
  else if (EMBER_COUNTER_APS_DATA_TX_UNICAST_SUCCESS == type
           || EMBER_COUNTER_APS_DATA_TX_UNICAST_FAILED == type)
    emberCounters[EMBER_COUNTER_APS_DATA_TX_UNICAST_RETRY] += data;    
}

void emberClearCounters(void)
{
  MEMSET(emberCounters, 0, sizeof(emberCounters));
}

