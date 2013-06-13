// *******************************************************************
//  zdo-host-config.h
//
//  host sample app sending and receiving ZDO messages
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// *******************************************************************
// Ember static memory requirements
//
// There are constants that define the amount of static memory
// required by the stack. If the application does not set them then
// the default values from ember-configuration-defaults.h are used.
//
// for example, this changes the default number of buffers to 8
// Size the binding table at 15
#define EMBER_BINDING_TABLE_SIZE 15 

//security if off by default
#if defined(SECURE)
  #define EMBER_SECURITY_LEVEL        5    
#else
  #define EMBER_SECURITY_LEVEL        0    
#endif
#define APP_EXTENDED_PANID {0x01,0x22,0x03,0x44,0x55,0x66,0x77,0x88}
#define APP_VERSION "1.0.0.0"

// *******************************************************************
// Application Handlers
//
// By default, a number of stub handlers are automatically provided
// that have no effect.  If the application would like to implement any
// of these handlers itself, it needs to define the appropriate macro
#define EZSP_APPLICATION_HAS_REMOTE_BINDING_HANDLER
#define EZSP_APPLICATION_HAS_COUNTER_HANDLER

// *******************************************************************
//the max # of clusters used by the endpoints
#define MAX_NUM_CLUSTERS 3 



