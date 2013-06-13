// *******************************************************************
//  security-common.c
//
//  Common functions for manipulating security for Trust Center and
//  non Trust Center devices.
//
//  Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

//------------------------------------------------------------------------------
// INCLUDES

#include PLATFORM_HEADER //compiler/micro specifics, types

#if defined EZSP_HOST
  #include "stack/include/ember-types.h"
  #include "stack/include/error.h"

  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/ezsp-utils.h"

  #define emberKeyContents(key) ((key)->contents)

#else // Stack App
  #include "stack/include/ember.h"
#endif

//------------------------------------------------------------------------------

#if defined EZSP_HOST
boolean emberHaveLinkKey(EmberEUI64 remoteDevice)
{
  EmberKeyStruct keyStruct;

  // Check and see if the Trust Center is the remote device first.
  if (EMBER_SUCCESS == emberGetKey(EMBER_TRUST_CENTER_LINK_KEY, &keyStruct)) {
    if (0 == MEMCOMPARE(keyStruct.partnerEUI64, remoteDevice, EUI64_SIZE)) {
      return TRUE;
    }
  }

  return (0xFF != emberFindKeyTableEntry(remoteDevice, 
                                         TRUE));        // look for link keys?
}
#endif
