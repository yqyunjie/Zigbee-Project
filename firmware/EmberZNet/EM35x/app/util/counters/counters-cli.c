// File: counters-cli.c
//
// Used for testing the counters library via a command line interface.
// For documentation on the counters library see counters.h.
//
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#if !defined(EZSP_HOST)
#include "stack/include/ember.h"
#else
#include "stack/include/ember-types.h"
#endif

#include "hal/hal.h" 
#include "app/util/serial/serial.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/util/common/common.h"
#include "app/util/counters/counters.h"
#include "app/util/counters/counters-ota.h"

#if defined(EZSP_HOST)
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#endif

void clearCountersCommand(void)
{
#if !defined(EZSP)
  emberClearCounters();
#else
  emberSerialPrintLine("Not supported.  Counters cleared automatically when retrieved by host.");
#endif
}

PGM_NO_CONST PGM_P titleStrings[] = {
  EMBER_COUNTER_STRINGS
};

PGM_NO_CONST PGM_P unknownCounter = "???";

void printCountersCommand(void)
{ 
  int8u i=0;

#if defined(EZSP_HOST)
  ezspReadAndClearCounters(emberCounters);
#endif

  while ( i < EMBER_COUNTER_TYPE_COUNT ) {
    int16u data = emberCounters[i];
    emberSerialPrintfLine(serialPort, "%d) %p: %d", 
                          i,
                          (titleStrings[i] == NULL
                           ? unknownCounter
                           : titleStrings[i]),
                          data);
    emberSerialWaitSend(serialPort);
    i++;
  }
}

// For applications short on const space.
void simplePrintCountersCommand(void)
{
  int8u i;
  for ( i = 0; i < EMBER_COUNTER_TYPE_COUNT; i++ ) {
    emberSerialPrintfLine(serialPort, "%d: %d",
                          i,
                          emberCounters[i]);
    emberSerialWaitSend(serialPort);
  }
}

void sendCountersRequestCommand(void)
{
  emberSendCountersRequest(emberUnsignedCommandArgument(0),
                           emberUnsignedCommandArgument(1));
}

#if !defined(EZSP_HOST)

void printCountersResponse(EmberMessageBuffer message)
{
  int8u i;
  int8u length = emberMessageBufferLength(message);
  for (i = 0; i < length; i += 3) {
    emberSerialPrintfLine(serialPort, "%d: %d",
                          emberGetLinkedBuffersByte(message, i),
                          emberGetLinkedBuffersLowHighInt16u(message, i + 1));
    emberSerialWaitSend(serialPort);
  }
}

#endif
