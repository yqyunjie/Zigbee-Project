// File: form-and-join-node-adpater.c
//
// Description: this file adapts the form-and-join library to work
// directly on the Zigbee processor (eg, EM250).

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

#include "stack/include/ember.h"
#include "form-and-join-adapter.h"

// We use message buffers for caching energy scan results,
// pan id candidates, and joinable beacons.
static EmberMessageBuffer dataCache = EMBER_NULL_MESSAGE_BUFFER;

static EmberEventControl cleanupEvent = {0, 0};
#define CLEANUP_TIMEOUT_QS 120

int8u formAndJoinStackProfile(void)
{
  return emberStackProfile();
}

// We're relying on the fact that message buffers are a multiple of 16 bytes 
// in size, so that NetworkInfo records do not cross buffer boundaries.

NetworkInfo *formAndJoinGetNetworkPointer(int8u index)
{
  return (NetworkInfo *) emberGetLinkedBuffersPointer(dataCache, index << 4);
}

void formAndJoinSetCleanupTimeout(void)
{
  emberEventControlSetDelayQS(cleanupEvent, CLEANUP_TIMEOUT_QS);
}

int8u *formAndJoinAllocateBuffer(void)
{
  dataCache = emberAllocateStackBuffer();
  return (dataCache == EMBER_NULL_MESSAGE_BUFFER
          ? NULL
          : emberMessageBufferContents(dataCache));
}

// Set the dataCache length in terms of the number of NetworkInfo entries.

EmberStatus formAndJoinSetBufferLength(int8u entryCount)
{
  return emberSetLinkedBuffersLength(dataCache, entryCount << 4);
}

void formAndJoinReleaseBuffer(void)
{
  if (dataCache != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(dataCache);
    dataCache = EMBER_NULL_MESSAGE_BUFFER;
  }
}

static void cleanupEventHandler(void)
{
  emberEventControlSetInactive(cleanupEvent);
  emberFormAndJoinCleanup(EMBER_SUCCESS);
}

static EmberEventData formAndJoinEvents[] =
  {
    { &cleanupEvent, cleanupEventHandler },
    { NULL, NULL }       // terminator
  };

void emberFormAndJoinTick(void)
{
  emberRunEvents(formAndJoinEvents);
}

