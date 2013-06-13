// *******************************************************************
//  SP-configuration.h
//
//  Configuration Parameters
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************

// *******************************************************************
// Stack Profile Parameters
#define EMBER_STACK_PROFILE            0

// *******************************************************************
// Security
//
// Standard security with a preconfigured link key (both devices must have the 
// key) is used.  When using a preconfigured Link key, the Network Key will be 
// sent from the Trust Center to the joining device using APS Encryption (using 
// the Link Key).  See app/util/security/trust-center.c
//
// The Network Key is randomly generated to prevent a device that obtains the 
// key in one network from being able to gain access to another.   It also helps 
// to prevent identical deployments (on the same channel and panId) from being 
// able to read each other's packets.
//
// The link key is preconfigured on each device with a call to setupSecurity()
// in app/super-parent/sp-common.c.
//
// EMBER_SECURITY_LEVEL=5 turns security on.
// EMBER_SECURITY_LEVEL=0 turns security off.
//

#define EMBER_SECURITY_LEVEL  5

// *******************************************************************
// Ember static memory requirements
//
// There are constants that define the amount of static memory
// required by the stack. If the application does not set them then
// the default values from stack/config/ember-configuration-defaults.h are used.
//
// for example, this changes the default number of buffers to 8
// #define EMBER_PACKET_BUFFER_COUNT 8


#ifdef GATEWAY_APP
  // parent nodes have 33 address table entries plus 5 for keeping track
  // of the EUI64 addresses of parents of joining devices. This is used
  // for APS Encryption.  End device nodes do not need address table.
  #define GW_ADDRESS_TABLE_SIZE              33
  #define GW_TRUST_CENTER_ADDRESS_CACHE_SIZE  5
  #define EMBER_ADDRESS_TABLE_SIZE \
    (GW_ADDRESS_TABLE_SIZE + GW_TRUST_CENTER_ADDRESS_CACHE_SIZE)
  // do not allow end device to join to gateway since it cannot be super parent
  #define EMBER_MAX_END_DEVICE_CHILDREN 	0
#else
  #define EMBER_ADDRESS_TABLE_SIZE 1
#endif

#ifdef PARENT_APP
  // Need to be set to 0xFF to indicate that it is a super parent, hence,
  // the application will manage the child table itself.
  #define EMBER_RESERVED_MOBILE_CHILD_ENTRIES 0xFF
  // Indicate how many end devices are allowed to join simultaneously
  #define EMBER_MAX_END_DEVICE_CHILDREN 	32
#endif


// Super arent should hold the message long enough for the child to poll for it. 
// The value is derived from (SP_PERIODIC_POLL_INTERVAL_SEC + 1) * 1000 where
// SP_PERIODIC_POLL_INTERVAL_SEC is 20 seconds.
#define EMBER_INDIRECT_TRANSMISSION_TIMEOUT   21000 // ms

// Need to be set long enough so end device has the chance to get all its 
// retried APS messages; (2 * EMBER_INDIRECT_TRANSMISSION_TIMEOUT) + 6.  
// Value is (6<<3) which is 48 seconds.
#define EMBER_END_DEVICE_POLL_TIMEOUT 	6
#define EMBER_END_DEVICE_POLL_TIMEOUT_SHIFT 	3 // 8 seconds

// *******************************************************************
// Application Handlers
//
// By default, a number of stub handlers are automatically provided
// that have no effect.  If the application would like to implement any
// of these handlers itself, it needs to define the appropriate macro


#ifdef GATEWAY_APP
  #define EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  #define EZSP_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
  #define EMBER_SOURCE_ROUTE_TABLE_SIZE 33
#else
  #define EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
  #define EMBER_APPLICATION_HAS_BUTTON_HANDLER
  #define EMBER_APPLICATION_HAS_SWITCH_KEY_HANDLER
#endif

#ifdef PARENT_APP
  #define EMBER_APPLICATION_HAS_CHILD_JOIN_HANDLER
  #define EMBER_APPLICATION_HAS_POLL_HANDLER
  #define EMBER_APPLICATION_HAS_INCOMING_MANY_TO_ONE_ROUTE_REQUEST_HANDLER
#endif

#ifdef ED_APP
  #define EMBER_APPLICATION_HAS_POLL_COMPLETE_HANDLER
#endif

