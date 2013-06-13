// *****************************************************************************
// * stack-info.c
// *
// * Prints various information abouth the stack.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "app/util/serial/command-interpreter2.h"

//------------------------------------------------------------------------------
// Forward Declarations

static void printChildTable(void);
static void printNeighborTable(void);
static void printRouteTable(void);
static void printInfo(void);

//------------------------------------------------------------------------------
// Globals

EmberCommandEntry emberAfPluginStackDiagnosticsCommands[] = {
  emberCommandEntryAction("info",           printInfo,          "", "Print general stack info"),            
  emberCommandEntryAction("child-table",    printChildTable,    "", "Print the child table"),
  emberCommandEntryAction("neighbor-table", printNeighborTable, "", "Print the neighbor table"),
  emberCommandEntryAction("route-table",    printRouteTable,    "", "Print the route table"),
  emberCommandEntryTerminator(),
};

#define UNKNOWN_LEAVE_REASON_INDEX     5
#define APP_EVENT_1_LEAVE_REASON_INDEX 6
static PGM_NO_CONST PGM_P leaveReasonStrings[] = {
  "-",                 // None
  "NWK Leave message",
  "APS Remove message",
  "ZDO Leave message",
  "ZLL Touchlink",
  "???",
  "App Event 1",
};

#define UNKNOWN_REJOIN_REASON_INDEX       5
#define APP_FRAMEWORK_REJOIN_REASON_INDEX 6
#define CUSTOM_APP_EVENTS_INDEX           9
static PGM_NO_CONST PGM_P rejoinReasonStrings[] = {
  "-",
  "NWK Key Update",
  "Leave & Rejoin Message",
  "No Parent",
  "ZLL Touchlink",
  "???",

  // General App. Framework events defined in 'af.h'
  "End Device Move",
  "TC Keepalive failure",
  "CLI Command",

  // Non App. Framework, Application specific custom events
  "App Event 5",
  "App Event 4",
  "App Event 3",
  "App Event 2",
  "App Event 1",
};

static PGM_NO_CONST PGM_P nodeTypeStrings[] = {
  "Unknown",
  "Coordinator",
  "Router",
  "End Device",
  "Sleep End Device",
  "Mobile End Device",
};

//------------------------------------------------------------------------------
// Functions

static void printChildTable(void)
{
  int8u size = emberAfGetChildTableSize();
  int8u i;
  PGM_NO_CONST PGM_P types[] = {
    "Unknown",
    "Coordin",
    "Router ",
    "RxOn   ",
    "Sleepy ",
    "Mobile ",
    "???    ",
  };
  int8u used = 0;

  emberAfAppPrintln("#  type    id     eui64");
  for (i = 0; i < size; i++) {
    EmberNodeId childId;
    EmberEUI64 childEui64;
    EmberNodeType childType;
    EmberStatus status = emberAfGetChildData(i,
                                             &childId,
                                             childEui64,
                                             &childType);
    if (status != EMBER_SUCCESS) {
      continue;
    }
    if (childType > EMBER_MOBILE_END_DEVICE) {
      childType = EMBER_MOBILE_END_DEVICE + 1;
    }
    used++;
    emberAfAppPrint("%d: %p 0x%2X ", i, types[childType], childId);
    emberAfAppDebugExec(emberAfPrintBigEndianEui64(childEui64));
    emberAfAppPrintln("");
  }
  emberAfAppPrintln("\n%d of %d entries used.", 
                    used,
                    emberAfGetChildTableSize());
}

static void printNeighborTable(void)
{
  int8u used = 0;
  int8u i;
  EmberNeighborTableEntry n;

  emberAfAppPrintln("#  id     lqi  in  out  age  eui");
  for (i = 0; i < emberAfGetNeighborTableSize(); i++) {
    EmberStatus status = emberGetNeighbor(i, &n);
    if ((status != EMBER_SUCCESS)
        || (n.shortId == EMBER_NULL_NODE_ID)) {
      continue;
    }
    used++;
    emberAfAppPrint("%d: 0x%2X %d  %d   %d    %d    ", 
                    i,
                    n.shortId,
                    n.averageLqi,
                    n.inCost,
                    n.outCost,
                    n.age);
    emberAfAppDebugExec(emberAfPrintBigEndianEui64(n.longId));
    emberAfAppPrintln("");
    emberAfAppFlush();
  }
  emberAfAppPrintln("\n%d of %d entries used.", used, emberAfGetNeighborTableSize());
}


static void printRouteTable(void)
{
  PGM_NO_CONST PGM_P zigbeeRouteStatusNames[] = {
    "active",
    "discovering",
    "??",
    "unused",
  };
  PGM_NO_CONST PGM_P concentratorNames[] = {
    "-    ",
    "low  ",
    "high ",
  };
  int8u used = 0;
  int8u i;
  EmberRouteTableEntry entry;

  emberAfAppPrintln("#  id      next    age  conc    status");
  for (i = 0; i < emberAfGetRouteTableSize(); i++) {
    if (EMBER_SUCCESS !=  emberGetRouteTableEntry(i, &entry)
        || entry.destination == EMBER_NULL_NODE_ID) {
      continue;
    }
    used++;
    emberAfAppPrintln("%d: 0x%2X  0x%2X  %d   %p  %p",
                      i,
                      entry.destination,
                      entry.nextHop,
                      entry.age,
                      concentratorNames[entry.concentratorType],
                      zigbeeRouteStatusNames[entry.status]);
  }
  emberAfAppPrintln("\n%d of %d entries used.", used, emberAfGetRouteTableSize());
}

static void printInfo(void)
{
  EmberNodeId nodeThatSentLeave;
  EmberRejoinReason rejoinReason = emberGetLastRejoinReason();
  EmberLeaveReason  leaveReason =  emberGetLastLeaveReason(&nodeThatSentLeave);

  int8u index;

  EmberNodeId id = emberAfGetNodeId();
  EmberNodeType type = EMBER_UNKNOWN_DEVICE;
  EmberNetworkParameters parameters;
  emberAfGetNetworkParameters(&type,
                              &parameters);

  emberAfAppPrintln("Stack Profile: %d", emberAfGetStackProfile());
  emberAfAppPrintln("Configured Node Type (%d): %p",
                    emAfCurrentNetwork->nodeType,
                    nodeTypeStrings[emAfCurrentNetwork->nodeType]);
  emberAfAppPrintln("Running Node Type    (%d): %p\n",
                    type,
                    nodeTypeStrings[type]);
  emberAfAppPrintln("Channel:       %d", parameters.radioChannel);
  emberAfAppPrintln("Node ID:       0x%2x", id);
  emberAfAppPrintln("PAN ID:        0x%2X", parameters.panId);
  emberAfAppPrint(  "Extended PAN:  ");
  emberAfPrintBigEndianEui64(parameters.extendedPanId);
  emberAfAppPrintln("\nNWK Update ID: %d\n", parameters.nwkUpdateId);

  emberAfAppPrintln("NWK Manager ID: 0x%2X", parameters.nwkManagerId);
  emberAfAppPrint(  "NWK Manager Channels: ");
  emberAfPrintChannelListFromMask(parameters.channels);
  emberAfAppPrintln("\n");

  emberAfAppPrintln("Send Multicasts to sleepies: %p\n",
                    (emberAfGetSleepyMulticastConfig()
                     ? "yes"
                     : "no"));

  index = rejoinReason;
  if (rejoinReason >= EMBER_REJOIN_DUE_TO_APP_EVENT_5) {
    index = rejoinReason - EMBER_REJOIN_DUE_TO_APP_EVENT_5 + CUSTOM_APP_EVENTS_INDEX;
  } else if (rejoinReason >= EMBER_AF_REJOIN_FIRST_REASON
             && rejoinReason <= EMBER_AF_REJOIN_LAST_REASON) {
    index = rejoinReason - EMBER_AF_REJOIN_FIRST_REASON + APP_FRAMEWORK_REJOIN_REASON_INDEX;
  } else if (rejoinReason >= EMBER_REJOIN_DUE_TO_ZLL_TOUCHLINK) {
    index = UNKNOWN_REJOIN_REASON_INDEX;
  }
  emberAfAppPrintln("Last Rejoin Reason (%d): %p",
                    rejoinReason,
                    rejoinReasonStrings[index]);

  index = leaveReason;
  if (leaveReason == 0xFF) {
    index = APP_EVENT_1_LEAVE_REASON_INDEX;
  } else if (leaveReason > EMBER_LEAVE_DUE_TO_ZLL_TOUCHLINK) {
    index = UNKNOWN_LEAVE_REASON_INDEX;
  }
  emberAfAppPrintln("Last Leave Reason  (%d): %p",
                    leaveReason,
                    leaveReasonStrings[index]);
  emberAfAppPrintln("Node that sent leave: 0x%2X", nodeThatSentLeave);
}
