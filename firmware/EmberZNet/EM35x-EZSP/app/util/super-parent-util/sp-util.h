// *******************************************************************
//  sp-util.h
//
//  Utility file for super parent application.
//
//  The file contains application's defines and parameters, as well as
//  function prototypes of utility functions.
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************

// -----------------------------------------------------------------
// Application specfic parameters
// The frequency that end devices poll their parents.
#define SP_PERIODIC_POLL_INTERVAL_SEC 20

// The frequency that end devices poll their parents for APS ack. 
#define SP_FAST_POLL_INTERVAL_SEC 1

// How often the gateway queries each end device. Value is every ~ 11 minutes.
#define SP_GW_QUERY_INTERVAL_SEC  700

// Maximum number of end devices supported in the network.
#define SP_MAX_END_DEVICES  200

// The rate at which the gateway sends out query message.  The rate is constant  
// and is roughly calculated from SP_GW_QUERY_INTERVAL_SEC divided by 
// SP_MAX_END_DEVICE. 
#define SP_GW_QUERY_RATE_SEC  3

// The period that end device waits after responding to the last query message 
// before sending report message.  Value is 1.5 times SP_GW_QUERY_INTERVAL_SEC. 
#define SP_ZED_REPORT_INTERVAL_SEC    ((3*SP_GW_QUERY_INTERVAL_SEC)/2)

// We tie when many-to-one-route-discovery (MTORR) is sent to when periodic 
// query process will begin.  MTORR is performed 
// SP_MTORR_INTERVAL_BEFORE_QUERY_SEC before the query process begins.  Value
// is 5 seconds.
#define SP_MTORR_INTERVAL_BEFORE_QUERY_SEC  5

// How long to turn permit join on for.  Value is 60 seconds.
#define SP_PERMIT_JOIN_SEC  60

// Total number of messages missed or 802.15.4 ack missed (in response to 
// data request) before rejoining to network
#define SP_END_DEVICE_MISS_PACKET_TOLERANCE 3

// How long for the trust center to wait after sending out new network key
// via broadcast until telling nodes in the network to switch to use the
// new network key. This should be long enough to ensure that all routers in
// the network has received the new network key.  
#define SP_SWITCH_NETWORK_KEY_SEC   30

// How long parent will keep sending broadcast message as unicast to its
// children.
#define SP_BCAST_DURATION_SEC ((2*SP_PERIODIC_POLL_INTERVAL_SEC)+5)

// Define maximum size of broadcast message to end devices
#define SP_BCAST_TO_CHILDREN_MAX_SIZE 20

// Application cluster id
#define END_DEVICE_AND_PARENT_CLUSTER   0x0000
#define PARENT_AND_GW_CLUSTER           0x0001

// Application status byte (for each child entry in the database).  Status
// byte has eight bits.
// BIT[0-3]: indicates missed message
#define SP_STATUS_MISSED_MASK   0x7

// Application child entry
typedef struct {
  EmberNodeId childId;
  EmberEUI64 childEui;
  EmberNodeId parentId;
  int8u statusByte;
} spChildTableEntry;

// Application Message Types
enum {
  SP_JOIN_MSG,
  SP_QUERY_MSG,
  SP_REPORT_MSG,
  SP_TIMEOUT_MSG,
  SP_MTORR_RESP,
  SP_BCAST_MSG,
};

// global array and its length defined in sp-parent.c and sp-gateway.c
extern int8u globalBuffer[100];
extern int8u globalBufferLength;
extern boolean sendMessageToGateway;
extern int8u broadcastBuffer[SP_BCAST_TO_CHILDREN_MAX_SIZE+2];
extern int8u broadcastLength;

// -----------------------------------------------------------------
// EmberZNet functions and variables
extern EmberStatus emberAddChild(EmberNodeId shortId,
                          EmberEUI64 longId,
                          EmberNodeType nodeType);
extern EmberStatus emberRemoveChild(EmberEUI64 childEui64);
extern int8u emberReservedMobileChildEntries;
#define APP_MANAGES_CHILD_TABLE           0xFF
#define APP_MANAGES_CHILD_TABLE_BROADCAST 0xFE

// -----------------------------------------------------------------
// Database Utility Function Prototypes
void spDatabaseUtilForwardMessage(int8u *data, int8u length);
void spDatabaseUtilPrint(void);
boolean spDatabaseUtilgetEnddeviceInfo(spChildTableEntry *entry);
boolean spDatabaseUtilgetEnddeviceInfoViaEui64(
                                   spChildTableEntry *entry,
                                   EmberEUI64 childEui);

// -----------------------------------------------------------------
// Super Parent Utilit Function Prototypes
void spParentUtilTick(void);
void sendUnicastInResponseToMTORR(void);
void spParentUtilSendJITMessage(int8u msgType, 
                                EmberNodeId childId,
                                EmberEUI64 childEui);
void spParentUtilPrintJITQ(void);


