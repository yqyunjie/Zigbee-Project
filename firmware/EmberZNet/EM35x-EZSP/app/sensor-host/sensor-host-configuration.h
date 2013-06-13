// *******************************************************************
// Stack Profile Parameters

#define EMBER_STACK_PROFILE            0


// *******************************************************************
// Security
//
// This application uses the security utilities in "app/util/security"
// to configure security. Calls are made to the security library from 
// this application in "app/sensor-host/common.c". This application chooses
// to use a preconfigured link key on all devices, and the trust center
// sends the network key (protected by the link key) to devices just
// after they successfully join.
// 
// *NOTE*:  MAC Security has been deprecated and therefore there is no way to
// to do a secure join (the Association Commands will be sent in the clear).
// However if using a preconfigured Link key then the Network Key will be sent
// from the Trust Center to the joining device using APS Encryption (using the
// Link Key).  If not using a preconfigured Link Key then the Network and Link 
// Key will both be sent in the clear.  
//
// setting EMBER_SECURITY_LEVEL=5 turns security on.
// setting EMBER_SECURITY_LEVEL=0 turns security off.
//

#define EMBER_SECURITY_LEVEL     5


// *******************************************************************
// Ember static memory requirements
//
// There are constants that define the amount of static memory
// required by the stack. If the application does not set them then
// the default values from ember-configuration-defaults.h are used.
//
// for example, this changes the default number of buffers to 8
// #define EMBER_PACKET_BUFFER_COUNT 8

// sink nodes need 15 address table entries - non-sinks only need one
#if defined(SINK_APP)
  #define EMBER_ADDRESS_TABLE_SIZE 15
#else
  #define EMBER_ADDRESS_TABLE_SIZE 1
#endif
#define EMBER_MULTICAST_TABLE_SIZE 1
#define EMBER_NEIGHBOR_TABLE_SIZE 8

// *******************************************************************
// Application Handlers
//
// By default, a number of stub handlers are automatically provided
// that have no effect.  If the application would like to implement any
// of these handlers itself, it needs to define the appropriate macro

#define EZSP_APPLICATION_HAS_BUTTON_HANDLER
#define EZSP_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER

#if defined(SINK_APP)
  #define EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  #define EMBER_SOURCE_ROUTE_TABLE_SIZE 15
#else
  #define EZSP_APPLICATION_HAS_SWITCH_NETWORK_KEY_HANDLER
#endif

#if (defined(SLEEPY_SENSOR_APP)) || (defined(MOBILE_SENSOR_APP))
  #define EZSP_APPLICATION_HAS_POLL_COMPLETE_HANDLER
#endif
