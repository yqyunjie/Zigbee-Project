// *******************************************************************
// * 11073-tunnel.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// These are statically defined by the spec. Defines provided here 
// to improve plugin readability.
#define CLUSTER_ID_11073_TUNNEL 0x0614
#define ATTRIBUTE_11073_TUNNEL_MANAGER_TARGET 0x0001
#define ATTRIBUTE_11073_TUNNEL_MANAGER_ENDPOINT 0x0002
#define ATTRIBUTE_11073_TUNNEL_CONNECTED 0x0003
#define ATTRIBUTE_11073_TUNNEL_PREEMPTIBLE 0x0004
#define ATTRIBUTE_11073_TUNNEL_IDLE_TIMEOUT 0x0005

// These are variable and should be defined by the application using
// this plugin.
#ifndef HC_11073_TUNNEL_ENDPOINT
  #define HC_11073_TUNNEL_ENDPOINT 1
#endif

