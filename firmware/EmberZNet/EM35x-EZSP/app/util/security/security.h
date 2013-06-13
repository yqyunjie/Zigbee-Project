// *******************************************************************
//  security.h
//
//  functions for manipulating security for Trust Center and 
//  non Trust Center nodes using the commercial security library.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#if !defined emberKeyContents
  #define emberKeyContents(key) (((EmberKeyData*)(key))->contents)
#endif

// Trust Center Functions
boolean trustCenterInit(EmberKeyData* preconfiguredKey, 
                        EmberKeyData* networkKey);
void trustCenterPermitJoins(boolean allow);
boolean trustCenterIsPermittingJoins(void);

// Non Trust Center functions
boolean nodeSecurityInit(EmberKeyData* preconfiguredKey);

// Common functions
extern int8u addressCacheSize;
#define securityAddressCacheGetSize() (addressCacheSize+0)
void securityAddressCacheInit(int8u securityAddressCacheStartIndex,
                              int8u securityAddressCacheSize);
void securityAddToAddressCache(EmberNodeId nodeId, EmberEUI64 nodeEui64);

