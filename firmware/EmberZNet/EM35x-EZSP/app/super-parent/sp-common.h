// *******************************************************************
//  SP-common.h
//
//  Common code for super-parent application
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************

#include PLATFORM_HEADER //compiler/micro specifics, types
#ifdef GATEWAY_APP
#include "stack/include/ember-types.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include <unistd.h>
#include "app/util/source-route-host.h"
#else
#include "stack/include/ember.h"
#include "stack/include/packet-buffer.h"
#include "app/util/common/print-stack-tables.h"
#endif
#include "app/util/common/form-and-join.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "app/util/serial/cli.h"
#include "app/util/security/security.h"
#include "app/util/super-parent-util/sp-util.h"

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

// The super parent nodes will store the gateway address, and only the gateway
// address, in the address table. The gateway nodes will add entries to
// the address table for all super parent nodes.
#define GW_ADDRESS_TABLE_INDEX             0

// application timers are based on quarter second intervals, each
// quarter second is measured in millisecond ticks. This value defines
// the approximate number of millisecond ticks in a quarter second.
// Account for variations in system timers.
#define TICKS_PER_QUARTER_SECOND 250

// End application specific constants and globals
// *******************************************************************
#define USE_HARDCODED_NETWORK_SETTINGS
#ifdef USE_HARDCODED_NETWORK_SETTINGS
  // when the network settings are hardcoded - set the channel, panid, and
  // power. 
  #define APP_CHANNEL (23)
  #define APP_PANID   (0xFACE)
  #define APP_EXTENDED_PANID {'s','u','p','e','r',0,0,0}
  #define APP_POWER   (-1)
#else
  // when not using hardcoded settings, it is possible to ensure that
  // devices join to each other by setting an extended PAN ID below. 
  // Leaving the extended PAN ID as "0" means "use any"
  #define APP_EXTENDED_PANID {'s','u','p','e','r',0,0,0}
#endif //USE_HARDCODED_NETWORK_SETTINGS

// Helpful macro
#define myNodeId emberGetNodeId()

// Convenient definition used to set time remaining of action that needs to be
// performed now.
#define NOW 0

// To help with development we make the commands available.

#define SP_COMMON_COMMANDS            \
  {"info",     infoCB},               \
  {"reboot",   rebootCB},             \
  {"init",     initCB},               \
  {"security", printSecurityCB},      \
  {"leave",    leaveCB},              \
  {"set_channel",   setChannelCB},    \
  {"set_pan_id",    setPanIDCB},      \
  {"set_power",     setPowerCB},      \
  {"address",    addrTableCB},        \
  {"help",          getHelpCB}

#define SP_APP_COMMANDS         \
  {"join",       joinCB},       \
  {"neighbor",   neighborCB},   \

#define SP_PARENT_COMMANDS      \
  {"child",      childCB},      \
  {"jitQ",       jitQCB},       \
  
#define SP_GW_COMMANDS          \
  {"form",     formCB},         \
  {"query",    queryCB},        \
  {"database", databaseCB},     \
  {"key",      keyCB},          \
  {"broadcast",broadcastCB},    \
  
#define SP_ED_COMMANDS     \
  {"join_ed",   joinEDCB}, \
  {"rejoin",    rejoinCB}, \

// *******************************************************************
// Ember Command line interface parameters

// Application network commands.
void formCB(void);
void joinCB(void);
void joinEDCB(void);
void initCB(void);
void leaveCB(void);
void rejoinCB(void);

// Application utility commands
void setChannelCB(void);
void setPanIDCB(void);
void setPowerCB(void);
void getHelpCB(void);
void infoCB(void);
void printSecurityCB(void);
void rebootCB(void);

// Application Specific commands
void queryCB(void);
void databaseCB(void);
void neighborCB(void);
void addrTableCB(void);
void childCB(void);
void jitQCB(void);
void keyCB(void);
void broadcastCB(void);

// End command line parameters
// *******************************************************************

// the flag is defined in app/super-parent-util/sp-parent-util.c
extern int32u removeChildFlag;

// common utility functions
void setupSecurity(void);
void printEUI64(int8u port, EmberEUI64* eui);
void printExtendedPanId(int8u port, int8u *extendedPanId);
void quit(void);


