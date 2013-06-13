// File: form-and-join-host-adapter.c
//
// Description: this file adapts the form-and-join library to work on
// an EZSP host processor.  

#include PLATFORM_HEADER     
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-host-configuration-defaults.h"
#include "form-and-join.h"
#include "form-and-join-adapter.h"

// Flat buffer for caching matched networks, channel energies, and random PAN
// ids.  The unused pan id code assumes a size of 32 bytes.  Each matching
// network consumes 16 to 20 bytes, depending on struct padding.
// The default size is set in ezsp-host-configuration-defaults.h and can 
// be adjusted within the configuration header.  
// The buffer is an int16u[] instead of an int8u[] in
// order to avoid alignment issues on the host.
static int16u data[EZSP_HOST_FORM_AND_JOIN_BUFFER_SIZE];

int8u formAndJoinStackProfile(void)
{
  int16u stackProfile = 2;  // Assume ZigBee Pro profile if the following call fails.
  ezspGetConfigurationValue(EZSP_CONFIG_STACK_PROFILE, &stackProfile);
  return stackProfile;
}

NetworkInfo *formAndJoinGetNetworkPointer(int8u index)
{
  return ((NetworkInfo *) data) + index;
}

EmberStatus formAndJoinSetBufferLength(int8u entryCount)
{
  return (sizeof(data) < entryCount * sizeof(NetworkInfo) 
          ? EMBER_NO_BUFFERS
          : EMBER_SUCCESS);
}

void formAndJoinSetCleanupTimeout(void)
{
}

int8u *formAndJoinAllocateBuffer(void)
{
  return (int8u *) data;
}

void formAndJoinReleaseBuffer(void)
{
}
