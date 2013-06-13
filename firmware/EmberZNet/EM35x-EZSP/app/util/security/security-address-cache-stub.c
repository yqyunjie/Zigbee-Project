// *******************************************************************
//  trust-center-address-cache-stub.c
//
//  stub functions for the trust center address cache.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

//------------------------------------------------------------------------------
// INCLUDES

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember.h"
#include "stack/include/error.h"

int8u addressCacheSize = 0;             

//------------------------------------------------------------------------------

void securityAddressCacheInit(int8u securityAddressCacheStartIndex,
                             int8u securityAddressCacheSize)
{
}

void securityAddToAddressCache(EmberNodeId nodeId, EmberEUI64 nodeEui64)
{
}
