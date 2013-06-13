// *******************************************************************
// Stack profile parameters

#define EMBER_USING_MESH_STACK

#define EMBER_STACK_PROFILE 0
#define EMBER_MAX_END_DEVICE_CHILDREN 8

#define EMBER_SECURITY_LEVEL 5
// This applications runs on small networks, but there is no particular
// benefit to keeping the radius small.  We always want to reach the
// entire network.
#define EMBER_MAX_HOPS 10

// *******************************************************************
// Ember static memory requirements
//
// There are constants that define the amount of static memory 
// required by the stack. If the application does not set them then
// the default values from ember-configuration-defaults.h are used. 
// 
// for example, this changes the default number of buffers to 8
// #define EMBER_PACKET_BUFFER_COUNT 8

// We do not use the binding table.
#define EMBER_BINDING_TABLE_SIZE 0

// The maximum number of characters in a command.
#define EMBER_MAX_COMMAND_LENGTH    15

// *******************************************************************
// Application Handlers
//
// By default, a number of stub handlers are automatically provided
// that have no effect.  If the application would like to implement any
// of these handlers itself, it needs to define the appropriate macro

// We define an optional handler for use in doing energy scan.
#define EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
#define EMBER_APPLICATION_HAS_REMOTE_BINDING_HANDLER
#define EMBER_APPLICATION_HAS_BUTTON_HANDLER
#define EMBER_APPLICATION_HAS_POLL_COMPLETE_HANDLER

// *******************************************************************
// Bootloader Library
//
// By default, this application always use bootload library
//
// Modify the selections below to customize the library
#define USE_BOOTLOADER_LIB

#ifdef USE_BOOTLOADER_LIB

  // bootload message handlers
  #define EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS  // not def'd adds message and transmit complete stubs

  // target only mode
  //#define SBL_LIB_TARGET             // build reduced size target only image


  // don't modify
  #ifdef SBL_LIB_TARGET
    #define SBL_LIB_SRC_NO_PASSTHRU    // remove passthru code
  #endif // SBL_LIB_TARGET
  
#endif // USE_BOOTLOADER_LIB

// *******************************************************************
// Misc. Defines

// Add description fields to all CLI commands
#define EMBER_COMMAND_INTEPRETER_HAS_DESCRIPTION_FIELD
