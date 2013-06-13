// File: form-and-join-host-callbacks.c
// 
// Description: default implementations of three EZSP callbacks
// for use with the form-and-join library on the host.  If the
// application does not need to implement these callbacks for
// itself, it can use this file to supply them.

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#include "stack/include/ember-types.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "form-and-join.h"

void ezspNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                             int8u lqi,
                             int8s rssi)
{
  emberFormAndJoinNetworkFoundHandler(networkFound, lqi, rssi);
}

void ezspScanCompleteHandler(int8u channel, EmberStatus status)
{
  emberFormAndJoinScanCompleteHandler(channel, status);
}

void ezspEnergyScanResultHandler(int8u channel, int8s rssi)
{
  emberFormAndJoinEnergyScanResultHandler(channel, rssi);
}
