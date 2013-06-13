//----------------------------------------------------------------
// bootloader-demo-v2.h
// 
// Common include file for the standalone bootloader demonstration
// for em35x and em250.
//
// Copyright 2006-2010 by Ember Corporation. All rights reserved.           *80*
//
// Sample application for interacting with em357, em250 standalone bootloader
// via provided bootload utilities library (located at app/util/bootload).
//
// There are many ways to put new image onto Ember nodes.  The software that 
// allows uploading of new image is called 'bootloader'.  Standalone bootloader
// is a type of bootloader that independently lives in flash.  It has 
// minimum interaction with the application, hence, the word 'standalone'.
//
// In order to upload new image onto the node, it has to have bootloader and
// application that understand bootloader protocol.  If your nodes are
// already in the field without this support, you will need to bootload this
// application via some other means the first time to get this functionality,
// for example, via serial (RS-232) port.
//
// The application supports the following bootloader features:
//
//  - serial pass-thru bootloading of the gateway node: PC -> gateway node.
//    bootload mode = BOOTLOAD_MODE_NONE.
//  - serial pass-thru bootloading of a node in the mesh one hop away from
//    the gateway: PC -> gateway -> node A. bootload mode = BOOTLOAD_MODE_PASSTHRU.
//  - recovery function via serial passthru, in case
//    the bootload process fails. Note that if the failure is not a power
//    failure on the target node, the recover is done on the same channel.
//    However, if it's the power failure on the target node, then the recover
//    is done on channel 13.  Any nodes in the network that is one hop away
//    from the failed node can perform recovery.  
//    
// For bootloading passthru mode, the XModem communication between the PC host
// and the source node is done over uart port 1.  To minimize the risk of 
// interrupting the XModem communication, the application should check 
// IS_BOOTLOADING value.  If TRUE, that means bootloading is currently taking
// place (the node is the source node bootloading a remote target node).
// In such a situation, the node should avoiding using uart port 1 and also
// should limit the radio activities in order to avoid any disruption and
// to enhance performance.
//
//
//----------------------------------------------------------------

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

// Our configuration parameters
#include "bootloader-demo-v2-configuration.h"

// Ember stack and related utilities
#include "stack/include/ember.h"         // Main stack definitions
#include "stack/include/packet-buffer.h" // Linked message buffers
#include "stack/include/error.h"         // Status codes
#include "stack/include/event.h"         // Events

// HAL
#include "hal/hal.h" 
#include "hal/micro/token.h" 

// Application utilities
#include "app/util/serial/serial.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/util/common/common.h"
#include "app/util/common/form-and-join.h"
#include "app/util/bootload/bootload-utils.h"
#include "app/util/bootload/bootload-utils-internal.h"


// ****************************************************************
// This setting controls whether we use a fixed set of network parameters 
// to aid in development/debugging (when this constant is defined), or 
// whether we use standard ZigBee methods for selecting these parameters
// (not defined).
#define USE_HARDCODED_NETWORK_SETTINGS
#define BOOTLOADER_DEMO_POWER   (-1)
#define BOOTLOADER_DEMO_CHANNEL (11)
#define BOOTLOADER_DEMO_PAN_ID  (0x0113)
int8u BOOTLOADER_DEMO_EXT_PAN_ID[8] = {
  0xff, 0xff, 0xff, 0xff,  
  0xff, 0xff, 0xff, 0xff
};

// The application profile ID we use.  This profile ID is assigned to
// Ember Corp. and can only be used during development.
#define PROFILE_ID 0xC00F

// A generic endpoint.
#define ENDPOINT 1

// We always want to talk to the entire network.
#define BROADCAST_RADIUS EMBER_MAX_HOPS

// The number of seconds we allow for a node to join after the user presses
// the 'allow joining' command.
#define JOIN_TIMEOUT 60

// The number of seconds the user has to push the provisioning button on
// the second device when doing pushbutton provisioning.
#define CONNECT_TIMEOUT 60

// How long we wait in between broadcasts.  A typical ZigBee network
// has a sustained broadcast bandwidth of about one per second (the
// broadcast table holds eight messages and each stays in the table
// for nine seconds).  We require that a device wait 3 seconds between
// broadcasts.
#define INTERMESSAGE_DELAY    12        // quarter seconds

// The serial ports we use.
#define APP_SERIAL          0
#define BOOTLOAD_SERIAL     1

// The transmit power we use.
#define TX_POWER -1

//----------------------------------------------------------------
// Vendor Specific Identifiers
//
// Other variables that are application and vendor specific.  They are
// available for optional added on security.

int16u mfgId = 0x1234;
int8u hardwareTag[16] = {
  'E', 'M', 'B', 'E',  
  'R', ' ', 'D', 'E',
  'V', ' ', 'K', 'I',
  'T', '!', '!', '!'
};

//----------------------------------------------------------------
// Messages
//
// The cluster IDs we use.

#define ENABLE_JOINING_MESSAGE   0x11      // <duration:1>
#define VERSION_MESSAGE          0x12      // <EUI64:8> <app id:1> <version:1>
#define QUERY_MESSAGE            0x13      // no arguments

// Application commands supported by the bootloader demo.

void formAction(void);
void joinAction(void);
void leaveAction(void);
void networkAction(void);
void sendVersionAction(void);
void queryNetworkAction(void);
void queryNeighborAction(void);
void serialBootloaderAction(void);
void recoverAction(void);
void recoverDefaultAction(void);
// gw node passes the image it receives (from host pc) to someone else 
// (passthru)
void bootloadRemoteAction(void);
void getNodeIdAction(void);
void getHelpCommand(void);

// To help with development we make the commands available using the command
// interpreter as well as by pushing buttons.

#define DEMO_APPLICATION_SOURCE_NODE_COMMANDS                       \
  { "query_network",  (CommandAction)queryNetworkAction,     "",    \
    "Obtain application information on network"},                   \
  { "query_neighbor", (CommandAction)queryNeighborAction,    "",    \
    "Report on bootload-related infomation"},                       \
  { "recover",        recoverAction,                         "bu", \
    "Recover <node> <1(passthru)>"},       \
  { "default",        recoverDefaultAction,                  "u",  \
    "Recover nodes on channel 13 <1(passthru)>"},  \

#define DEMO_APPLICATION_GENERIC_COMMANDS                           \
  { "form",           (CommandAction)formAction,             "",    \
    "Form a network"},                                              \
  { "leave",          (CommandAction)leaveAction,            "",    \
    "Leave the network"},                                           \
  { "join",           (CommandAction)joinAction,             "",    \
    "Join the network as router"},                                  \
  { "serial",         (CommandAction)serialBootloaderAction, "",    \
    "Put the node in serial bootload mode"},                        \
  { "help",           getHelpCommand,                        "",    \
    "List available commands"},

#define DEMO_APPLICATION_PASSTHRU_COMMAND                           \
  { "remote",         bootloadRemoteAction,                  "b",   \
    "Upload an application to <node> via passthru"},                \

// Commands used during debugging.  These are available over the serial
// port only.

void networkInitAction(void);
void setChannelCommand(void);
void setPanIDCommand(void);
void setPowerCommand(void);
void listCommands(void);
void setTuneCommand(void);

#define DEMO_DEBUG_COMMANDS                                     \
  { "status",        statusCommand,                    "",  0}, \
  { "reboot",        rebootCommand,                    "",  0}, \
  { "network_init",  networkInitAction,                "",  0}, \
  { "set_channel",   setChannelCommand,                "u", 0}, \
  { "set_pan_id",    setPanIDCommand,                  "v", 0}, \
  { "set_power",     setPowerCommand,                  "s", 0}, \
  { "set_tune",      setTuneCommand,                   "u", 0}, \
  { "get_id",        getNodeIdAction,                  "",  0}, \
  { "list_commands", listCommands,                     "",  0},
  
//----------------------------------------------------------------
// Supplied by the task-specific code and used in the generic main loop.
void rebootHandler(void);
void appTick(void);
extern EmberEventData appEvents[];
extern EmberCommandEntry commands[];

// Helper functions to indicate activity to the user
void indicateNetworkUp(void);
void indicateNetworkDown(void);
void indicateNetworkFailure(void);
void indicateButtonAction(void);

// Utilities functions.
void buttonTick(void);
EmberStatus sendBroadcast(int8u clusterId, int8u *contents, int8u length, int8u radius);
void setSecurityState(int16u bmask);

// Application specific information.  
#define APPLICATION_ID      0xBD  // Bootloader Demo
#define APPLICATION_VERSION 0x02
