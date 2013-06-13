// File: library.c
// 
// Description: Code to display or retrieve the presence or absence of
//   Ember stack libraries on the device.
//
// Copyright 2009 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#if defined EZSP_HOST
  #include "stack/include/ember-types.h"
  #include "stack/include/error.h"
  #include "hal/hal.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "stack/include/library.h"
#else
  #include "stack/include/ember.h"
  #include "hal/hal.h"
#endif

#include "app/util/serial/serial.h"
#include "app/util/common/common.h"

static PGM_P libraryNames[] = {
  EMBER_LIBRARY_NAMES
};

void printAllLibraryStatus(void)
{
  int8u i = EMBER_FIRST_LIBRARY_ID;
  while (i < EMBER_NUMBER_OF_LIBRARIES) {
    EmberLibraryStatus status = emberGetLibraryStatus(i);
    if (status == EMBER_LIBRARY_ERROR) {
      emberSerialPrintfLine(serialPort, "Error retrieving info for library ID %d",
                            i);
    } else {
      emberSerialPrintfLine(serialPort,
                            "%p library%p present",
                            libraryNames[i],
                            ((status & EMBER_LIBRARY_PRESENT_MASK)
                             ? ""
                             : " NOT"));
      if (status & EMBER_LIBRARY_PRESENT_MASK) {
        if (i == EMBER_ZIGBEE_PRO_LIBRARY_ID
            || i == EMBER_SECURITY_CORE_LIBRARY_ID) {
          emberSerialPrintfLine(serialPort,
                                ((status 
                                  & EMBER_ZIGBEE_PRO_LIBRARY_HAVE_ROUTER_CAPABILITY)
                                 ? "  Have Router Support"
                                 : "  End Device Only"));
        }
        if (i == EMBER_ZIGBEE_PRO_LIBRARY_ID
            && (status
                & EMBER_ZIGBEE_PRO_LIBRARY_ZLL_SUPPORT)) {
          emberSerialPrintfLine(serialPort, "  Have ZLL Support");
        }
        if (i == EMBER_PACKET_VALIDATE_LIBRARY_ID
            && (status
                && EMBER_LIBRARY_PRESENT_MASK)) {
          emberSerialPrintfLine(serialPort,
                                ((status & EMBER_PACKET_VALIDATE_LIBRARY_ENABLED)
                                 ? "  Enabled"
                                 : "  Disabled"));
        }
      }
    }
    emberSerialWaitSend(serialPort);
    i++;
  }
}

boolean isLibraryPresent(int8u libraryId)
{
  EmberLibraryStatus status = emberGetLibraryStatus(libraryId);
  return (status != EMBER_LIBRARY_ERROR
          && (status & EMBER_LIBRARY_PRESENT_MASK));
}
