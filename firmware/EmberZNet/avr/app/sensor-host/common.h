// *******************************************************************
//  common.h
//
//  sample app for Ember Stack API
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"

#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/serial/serial.h"
#include "app/util/source-route-host.h"

// *******************************************************************
// XXX: HW Specific Support
#define NO_BOOTLOADER
// *******************************************************************

// *******************************************************************
// Ember endpoint and interface configuration

// The application profile ID we use.  This profile ID is assigned to
// Ember Corp. and can only be used during development.
#define PROFILE_ID 0xC00F

// Numeric index for the first endpoint. Applications can use any
// endpoints between 1 and 238.
// 0 = ZDO, 239 = SPDO, 240 = EDO, 241-255 reserved
// This application uses only one endpoint. This constant makes the code
// easier to read.
#define ENDPOINT 1

// End Ember endpoint and interface configuration
// *******************************************************************

// *******************************************************************
// Application specific constants and globals

// All nodes running the sensor or sink application will belong to the
// SENSOR MULTICAST group and will use the same location in the 
// multicast table.

#define MULTICAST_ID            0x1111
#define MULTICAST_TABLE_INDEX   0

// The sensor nodes will store the sink address, and only the sink
// address, in the address table. The sink nodes will add entries to
// the address table for all sensor nodes.

#define SINK_ADDRESS_TABLE_INDEX             0

#define HELLO_MSG_SIZE 5

// **********************************
// define the message types

#define MSG_NONE              0

// protocol for hooking up a sensor to a sink
#define MSG_SINK_ADVERTISE       1 // sink: advertise service
#define MSG_SENSOR_SELECT_SINK   2 // sensor: selects one sink
#define MSG_SINK_READY           3 // sink: tells sensor it is ready to rx data
#define MSG_SINK_QUERY           4 // sleepy end devices: ask who the sink is

// data transmission from [sensor to sink]
#define MSG_DATA             10 // sensor: tx data

// multicast hello
#define MSG_MULTICAST_HELLO 100

//
// **********************************

// serial ports
#ifdef GATEWAY_APP
// for uart
#define APP_SERIAL   1
#else
// for spi
#define APP_SERIAL   0
#endif

// application timers are based on quarter second intervals, each
// quarter second is measured in millisecond ticks. This value defines
// the approximate number of millisecond ticks in a quarter second.
// Account for variations in system timers.
#ifdef AVR_ATMEGA_32
#define TICKS_PER_QUARTER_SECOND 225
#else
#define TICKS_PER_QUARTER_SECOND 250
#endif

// the time a sink waits before advertising.
// This is in quarter seconds.
#define TIME_BEFORE_SINK_ADVERTISE 240 // ~60 seconds

// the next two defines determine how fast to send data and 
// how much data to send. SEND_DATA_SIZE must be multiple of 2 
// (since we're sending two-byte integers). The maximum value for 
// SEND_DATA_SIZE is determined by the maximum transport payload 
// size, which varies if security is on or off. The sendData
// function in sensor.c checks this value and changes it if necessary
#define SEND_DATA_SIZE  40 // default 40 bytes
#define SEND_DATA_RATE  80 // ~20 seconds (min 2)

// num of pkts to miss before deciding that other node is gone
#define MISS_PACKET_TOLERANCE    3

// buffer used to build up a message before being transmitted
extern int8u globalBuffer[];

// End application specific constants and globals
// *******************************************************************

// common utility functions
void sensorCommonSetupSecurity(void);
void printAddressTable(int8u tableSize);
void printMulticastTable(int8u tableSize);
void printEUI64(int8u port, EmberEUI64* eui);
void printExtendedPanId(int8u port, int8u *extendedPanId);
void printNodeInfo(void);
void printChildTable(void);
void sensorCommonPrint16ByteKey(int8u* key);
EmberStatus emberGenerateRandomKey(EmberKeyData* keyAddress);
void sensorCommonPrintKeys(void);

// it is not recommended to ever hard code the network settings
// applications should always scan to determine the channel and panid
// to use. However, in some instances, this can aid in debugging
#define USE_HARDCODED_NETWORK_SETTINGS

#ifdef USE_HARDCODED_NETWORK_SETTINGS
  #define APP_CHANNEL (26)
  #define APP_PANID   (0x01ff)
  #define APP_EXTENDED_PANID {'s','e','n','s','o','r',0,0}
#else
  // when not using hardcoded settings, it is possible to ensure that
  // devices join to each other by setting an extended PAN ID below. 
  // Leaving the extended PAN ID as "0" means "use any"
  #define APP_EXTENDED_PANID {0,0,0,0,0,0,0,0}
#endif //USE_HARDCODED_NETWORK_SETTINGS

// define the power level to use
#define APP_POWER   (-1)

#if defined(SENSOR_APP) || defined(SINK_APP)
// ****************************************************************
// The following function are to support sleepy end devices

// Handle queries from end devices for the sink by sending a 
// sink advertise in response
void handleSinkQuery(EmberNodeId childId);

#if defined(SINK_APP)

#define mainSinkFound TRUE
#define sinkEUI emberGetEui64()
#define sinkShortAddress emberGetNodeId()

#else // must be SENSOR_APP

extern boolean mainSinkFound;
extern EmberEUI64 sinkEUI;
extern EmberNodeId sinkShortAddress;

#endif // defined(SINK_APP)

#endif // defined(SENSOR_APP) || defined(SINK_APP)

