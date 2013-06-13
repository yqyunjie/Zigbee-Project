// *******************************************************************
// Stack Profile Parameters

#define EMBER_STACK_PROFILE            0


// *******************************************************************
// Security
//
// This application uses the security utilities in "app/util/security"
// to configure security. Calls are made to the security library from 
// this application in "app/sensor/common.c". This application chooses
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

#define EMBER_SECURITY_LEVEL  5


// *******************************************************************
// Ember static memory requirements
//
// There are constants that define the amount of static memory
// required by the stack. If the application does not set them then
// the default values from ember-configuration-defaults.h are used.
//
// for example, this changes the default number of buffers to 8
// #define EMBER_PACKET_BUFFER_COUNT 8

// sink nodes need 15 address table entries plus 5 for keeping track
// of the EUI64 addresses of parents of joining devices. This is used
// for APS Encryption.  Non-sinks only need one table entry.

#ifdef SINK_APP
  #define SINK_ADDRESS_TABLE_SIZE              15
  #define SINK_TRUST_CENTER_ADDRESS_CACHE_SIZE  5

  #define EMBER_ADDRESS_TABLE_SIZE \
    (SINK_ADDRESS_TABLE_SIZE + SINK_TRUST_CENTER_ADDRESS_CACHE_SIZE)
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

// need to define this to use the form-and-join utilities
#define EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER

#define EMBER_APPLICATION_HAS_BUTTON_HANDLER

#if defined(SINK_APP)
  #define EMBER_APPLICATION_HAS_SOURCE_ROUTING
  #define EMBER_SOURCE_ROUTE_TABLE_SIZE 15
#else
  #define EMBER_APPLICATION_HAS_SWITCH_KEY_HANDLER
#endif

#if (defined(SINK_APP)) || (defined(SENSOR_APP))
  #define EMBER_APPLICATION_HAS_CHILD_JOIN_HANDLER

  #define EMBER_APPLICATION_HAS_POLL_HANDLER
#endif


#if (defined(SLEEPY_SENSOR_APP)) || (defined(MOBILE_SENSOR_APP))
  #define EMBER_APPLICATION_HAS_POLL_COMPLETE_HANDLER
#endif

// To add bootloading capability to the application.
// #define USE_BOOTLOADER_LIB
#ifdef USE_BOOTLOADER_LIB
  #define EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS
#endif

#if defined(SINK_APP)
  #define EMBER_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
#else
  #define EMBER_USE_TRUST_CENTER
#endif
