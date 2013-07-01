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


// *******************************************************************
// Rename callbacks defined in form-and-join.c so that the em260
// callbacks defined in stack-callbacks.c are called prior to calling
// the form-and-join.c callbacks

#ifdef __FORM_AND_JOIN_C__
#define emberScanCompleteHandler(channel, status) \
    formAndJoinScanCompleteHandler(channel, status)
#define emberNetworkFoundHandler(channel, panId, expectingJoin, stackProfile) \
    formAndJoinNetworkFoundHandler(channel, panId, expectingJoin, stackProfile)
#define emberEnergyScanResultHandler(channel, maxRssiValue) \
    formAndJoinEnergyScanResultHandler(channel, maxRssiValue)
#endif

#define EMBER_ASSERT_SERIAL_PORT 0
