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
#include "form-and-join-adapter.h"

// Flat buffer for caching matched networks, channel energies, and random
// PAN ids.  The unused pan id code assumes a size of 32.
// Each matching network consumes 14 bytes.
// The default size is set in ezsp-host-configuration-defaults.h
// and can be adjusted within the configuration header.
static int8u data[EZSP_HOST_FORM_AND_JOIN_BUFFER_SIZE];

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
  return (EZSP_HOST_FORM_AND_JOIN_BUFFER_SIZE < entryCount * sizeof(NetworkInfo)
          ? EMBER_NO_BUFFERS
          : EMBER_SUCCESS);
}

void formAndJoinSetCleanupTimeout(void)
{
}

int8u *formAndJoinAllocateBuffer(void)
{
  return data;
}

void formAndJoinReleaseBuffer(void)
{
}
