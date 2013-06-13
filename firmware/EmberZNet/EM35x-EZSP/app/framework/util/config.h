// *******************************************************************
  // * config.h
// *
// * This file is the main configuration settings for the Zigbee app.
// * The zigbee app can become a Home Automation (HA) device, a Smart
// * Energy (SE) device, or a Custom Zigbee device. 
// *
// * This application can be configured using AppBuilder. AppBuilder
// * generates a file containing defines that setup what pieces of the
// * code is used (which clusters, security settings, zigbee device type,
// * serial port, etc). These defines are added to a new file and included
// * by setting ZA_GENERATED_HEADER to the new filename so these defines are
// * sourced first.
// *
// * This file also contains default values for the defines so some can
// * be set by the user but defaults are always available.
// *******************************************************************

#ifndef __EMBER_AF_CONFIG_H__
#define __EMBER_AF_CONFIG_H__

// include generated configuration information from AppBuilder. 
// ZA_GENERATED_HEADER is defined in the project file
#ifdef ZA_GENERATED_HEADER
  #include ZA_GENERATED_HEADER
#endif


// *******************************************************************
// pre-defined Devices
// 
// use these to determine which type of device the current application is.
// do not use the EMBER_* versions from ember-types.h as these are in an
// enum and are not available at preprocessor time. These need to be set 
// before the devices are loaded from ha-devices.h and se-devices.h
#define ZA_COORDINATOR 1
#define ZA_ROUTER 2
#define ZA_END_DEVICE 3
#define ZA_SLEEPY_END_DEVICE 4
#define ZA_MOBILE_END_DEVICE 5

#define SE_PROFILE_ID (0x0109)
#define CBA_PROFILE_ID (0x0105)

// Define Ember's manufacturer code and use it for the application if a real
// one was not set.
#define EM_AF_MANUFACTURER_CODE 0x1002
#ifndef EMBER_AF_MANUFACTURER_CODE
  #define EMBER_AF_MANUFACTURER_CODE EM_AF_MANUFACTURER_CODE
#endif

// This file determines the security profile used, and from that various
// other security parameters.
#include "app/framework/security/security-config.h"

// *******************************************************************
// Application configuration of RAM for cluster usage
// 
// This section defines constants that size the tables used by the cluster
// code. 


// This is the max hops that the network can support - used to determine
// the max source route overhead and broadcast radius
// if we havent defined MAX_HOPS then define based on profile ID
#ifndef ZA_MAX_HOPS
  #ifdef EMBER_AF_HAS_SECURITY_PROFILE_SE
    #define ZA_MAX_HOPS 6
  #else
    #define ZA_MAX_HOPS 12
  #endif
#endif

#ifndef EMBER_AF_SOURCE_ROUTING_RESERVED_PAYLOAD_LENGTH
#define EMBER_AF_SOURCE_ROUTING_RESERVED_PAYLOAD_LENGTH 0
#endif

// The maximum APS payload, not including any APS options.  This value is also
// available from emberMaximumApsPayloadLength() or ezspMaximumPayloadLength().
// See http://portal.ember.com/faq/payload for more information.
#ifdef EMBER_AF_HAS_SECURITY_PROFILE_NONE
  #define EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH \
          100 - EMBER_AF_SOURCE_ROUTING_RESERVED_PAYLOAD_LENGTH
#else
  #define EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH \
          82 - EMBER_AF_SOURCE_ROUTING_RESERVED_PAYLOAD_LENGTH
#endif

// Max PHY size = 128
//   -1 byte for PHY length
//   -2 bytes for MAC CRC
#define EMBER_AF_MAXIMUM_INTERPAN_LENGTH 125

// The additional overhead required for APS encryption (security = 5, MIC = 4).
#define EMBER_AF_APS_ENCRYPTION_OVERHEAD 9

// The additional overhead required for APS fragmentation.
#define EMBER_AF_APS_FRAGMENTATION_OVERHEAD 2

// The additional overhead required for network source routing (relay count = 1,
// relay index = 1).  This does not include the size of the relay list itself.
#define EMBER_AF_NWK_SOURCE_ROUTE_OVERHEAD 2

// The additional overhead required per relay address for network source
// routing.  This is in addition to the overhead defined above.
#define EMBER_AF_NWK_SOURCE_ROUTE_PER_RELAY_ADDRESS_OVERHEAD 2

// defines the largest size payload allowed to send and receive. This 
// affects the payloads generated from the CLI and the payloads generated 
// as responses.
// Maximum payload length.
// If fragmenation is enabled, and fragmentation length is bigger than default, then use that
#if defined(EMBER_AF_PLUGIN_FRAGMENTATION) \
    && (EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE > EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH)
  #define EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE
  #define EMBER_AF_INCOMING_BUFFER_LENGTH      EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE
#else
  #define EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH
  #define EMBER_AF_INCOMING_BUFFER_LENGTH      EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH
#endif


// *******************************************************************
// Application configuration of Flash
//
// This section gives the application options for turning on or off
// features that affect the amount of flash used.

#ifndef EMBER_PACKET_BUFFER_COUNT
// This applies only to the SOC.
  #if defined (CORTEXM3) || defined (EMBER_TEST)
    // Much of the memory we use in App. framework is global and not
    // in packet buffers.  We size this to a large enough size to
    // insure the stack has enough to process messages, but leave
    // most of the memory for the application.
    #define EMBER_PACKET_BUFFER_COUNT 75
  #else // Assume XAP
    // Empirically, 24 is the minimum amount needed for key establishment 
    // operations during joining.
    #define EMBER_PACKET_BUFFER_COUNT 24
  #endif
#endif //EMBER_PACKET_BUFFER_COUNT

// *******************************************************************
// Defines needed for enabling security
//

// Unless we are not using security, our stack profile is 2 (ZigBee Pro).  The
// stack will set up other configuration values based on profile.
#ifndef EMBER_AF_HAS_SECURITY_PROFILE_NONE
  #define EMBER_STACK_PROFILE 2
#else
  #ifndef EMBER_STACK_PROFILE
    #define EMBER_STACK_PROFILE 0
  #endif
  #ifndef EMBER_SECURITY_LEVEL
    #define EMBER_SECURITY_LEVEL 0 
  #endif
#endif


// *******************************************************************
// Application Handlers
//
// By default, a number of stub handlers are automatically provided
// that have no effect.  If the application would like to implement any
// of these handlers itself, it needs to define the appropriate macro

#include "stack-handlers.h"

#define EMBER_APPLICATION_HAS_REMOTE_BINDING_HANDLER
#define EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
#define EMBER_APPLICATION_HAS_GET_ENDPOINT
#define EMBER_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
#define EMBER_APPLICATION_HAS_BUTTON_HANDLER

#define EZSP_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
#define EZSP_APPLICATION_HAS_INCOMING_SENDER_EUI64_HANDLER
#define EZSP_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
#define EZSP_APPLICATION_HAS_BUTTON_HANDLER

#if defined(EMBER_AF_PLUGIN_OTA_CLIENT_SIGNATURE_VERIFICATION_SUPPORT)
  #define EZSP_APPLICATION_HAS_DSA_VERIFY_HANDLER
#endif

#define EMBER_APPLICATION_HAS_COUNTER_HANDLER
#define EMBER_APPLICATION_HAS_COMMAND_ACTION_HANDLER

// This will enable/disable the source routing code which lives
// outside the framework in app/util/source-route*
#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
 #if defined(EZSP_HOST)
   #define EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
 #else
   #define EMBER_APPLICATION_HAS_SOURCE_ROUTING
 #endif
#else
  // We don't need this anymore since the source route code is dynamically
  // included by AppBuilder based on whether the Concentrator Plugin
  // is enabled or disabled.  However for customers that may be upgrading
  // or manually including the code in their project file, we want to disable
  // it to prevent problems.
  #define ZA_NO_SOURCE_ROUTING
#endif

// *******************************************************************
// Default values for required defines
//

// define the serial port that the application uses to be 1 if this is not set
#ifndef APP_SERIAL
  #define APP_SERIAL 1
#endif


// The address table plugin is enabled by default. If it gets disabled for some
// reason, we still need to define these #defines to some default value.
#ifndef EMBER_AF_PLUGIN_ADDRESS_TABLE
  #define EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE 2
  #define EMBER_AF_PLUGIN_ADDRESS_TABLE_TRUST_CENTER_CACHE_SIZE 2
#endif

// The total size of the address table is the size of the section used by the
// application plus the size of section used for the trust center address cache.
// The NCP allows each section to be sized independently, but the SOC requires
// a single configuration for the whole table.
#ifndef EMBER_ADDRESS_TABLE_SIZE
  #define EMBER_ADDRESS_TABLE_SIZE                            \
  (EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE                         \
   + EMBER_AF_PLUGIN_ADDRESS_TABLE_TRUST_CENTER_CACHE_SIZE)
#endif

#ifndef EMBER_AF_DEFAULT_APS_OPTIONS
  // BUGZID 12261: Concentrators use MTORRs for route discovery and should not
  // enable route discovery in the APS options.
  #ifdef EMBER_AF_PLUGIN_CONCENTRATOR
    #define EMBER_AF_DEFAULT_APS_OPTIONS            \
      (EMBER_APS_OPTION_RETRY                       \
       | EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY)
  #else
    #define EMBER_AF_DEFAULT_APS_OPTIONS            \
      (EMBER_APS_OPTION_RETRY                       \
       | EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY    \
       | EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY)
  #endif
#endif

// *******************************************************************
// // Default values for required defines
// //

#ifdef EMBER_AF_DEFAULT_RESPONSE_POLICY_NEVER
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS BIT(4)
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES BIT(4)
#elif defined(EMBER_AF_DEFAULT_RESPONSE_POLICY_CONDITIONAL)
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS 0
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES BIT(4)
#else
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS 0
  #define EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES 0
#endif // EMBER_AF_DEFAULT_RESPONSE_POLICY_NEVER
#endif // __EMBER_AF_CONFIG_H__

