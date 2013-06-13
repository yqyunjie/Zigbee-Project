// *******************************************************************
// Stack Profile Parameters


// *******************************************************************
// Stack Memory Requirements

#define EMBER_CHILD_TABLE_TOKEN_SIZE                32

#ifdef EMBER_USING_TREE_STACK
  #define EMBER_APS_INDIRECT_BINDING_TABLE_TOKEN_SIZE 32
#else
  #define EMBER_BINDING_TABLE_TOKEN_SIZE              32
#endif

#define EMBER_TRANSPORT_CONNECTION_COUNT            0

// *******************************************************************
// Application Handlers

#define EZSP_APPLICATION_HAS_TIMER_HANDLER

// *******************************************************************
// Application Configuration

#define EZSP_TOKEN_SIZE    8
#define EZSP_TOKEN_ENTRIES 8


#define EMBER_ASSERT_SERIAL_PORT 0
