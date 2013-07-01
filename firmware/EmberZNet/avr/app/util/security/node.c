// *******************************************************************
//  node.c
//
//  Functions for manipulating security on a normal (non Trust Center)
//  node.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
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

#else // Stack App
  #include "stack/include/ember.h"
#endif

#include "app/util/security/security-common.h"

//------------------------------------------------------------------------------

boolean nodeSecurityInit(EmberKeyData* preconfiguredKey)
{
  EmberInitialSecurityState state;
  MEMSET(&state, 0, sizeof(state));

  if ( preconfiguredKey ) {
    MEMCOPY(emberKeyContents(&(state.preconfiguredKey)), 
            emberKeyContents(preconfiguredKey), 
            EMBER_ENCRYPTION_KEY_SIZE);
  }
  state.bitmask = ( EMBER_STANDARD_SECURITY_MODE
                    | ( preconfiguredKey 
                        ? (EMBER_HAVE_PRECONFIGURED_KEY
                           | EMBER_REQUIRE_ENCRYPTED_KEY)
                        : EMBER_GET_LINK_KEY_WHEN_JOINING ) );

  return (EMBER_SUCCESS == emberSetInitialSecurityState(&state));
}
