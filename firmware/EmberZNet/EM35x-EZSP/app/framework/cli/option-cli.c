// *****************************************************************************
// * option-cli.c
// *
// * All CLI commands in the 'option' command-tree.
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// common include file
#include "app/framework/util/common.h"

#include "app/framework/util/af-main.h"
#include "app/framework/util/service-discovery.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/util/concentrator/concentrator.h"


// *****************************************************************************
// Forward Declarations
// *****************************************************************************

static void optionPrintRxCommand(void);
static void optionRegisterCommand(void);
static void optionDiscoverCommand(void);
static void optionBindingTablePrintCommand(void);
static void optionBindingTableClearCommand(void);
static void optionAddressTablePrintCommand(void);
static void optionAddressTableSetCommand(void);
static void optionEndDeviceBindCommand(void);
static void optionApsRetryCommand(void);
static void optionApsSecurityCommand(void);
static void optionLinkCommand(void);
static void optionBindingTableSetCommand(void);
static void optionPrintRouteCommand(void);
static void optionInstallCodeCommand(void);
static void optionDiscoveryTargetCommand(void);

// *****************************************************************************
// Globals
// *****************************************************************************

// option print-rx-msgs enable
// option print-rx-msgs disable
static EmberCommandEntry optionPrintRxCommands[] = {
  {"enable", optionPrintRxCommand, ""},
  {"disable", optionPrintRxCommand, ""},
  { NULL }
};

// option binding-table print
// option binding-table clear
// option binding-table set <index> <cluster> <local ep> <remote ep> <EUI>
static EmberCommandEntry optionBindingCommands[] = {
  {"print", optionBindingTablePrintCommand, ""},
  {"clear", optionBindingTableClearCommand, ""},
  {"set", optionBindingTableSetCommand, "uvuub"},
  { NULL }
};

// option address-table print
// option address-table set <index> <eui64> <node id>
static EmberCommandEntry optionAddressCommands[] = {
  {"print", optionAddressTablePrintCommand, ""},
  {"set", optionAddressTableSetCommand, "ubv"},
  { NULL }
};

// option security aps [off|on]
static EmberCommandEntry optionApsSecurityCommands[] = {
  {"on", optionApsSecurityCommand, ""},
  {"off", optionApsSecurityCommand, ""},
  { NULL }
};

static EmberCommandEntry apsRetryCommands[] = {
  {"on", optionApsRetryCommand, ""},
  {"off", optionApsRetryCommand, ""},
  {"def", optionApsRetryCommand, ""},
  { NULL }
};

static EmberCommandEntry optionSecurityCommands[] = {
  {"aps", NULL, (PGM_P)optionApsSecurityCommands},
  { NULL }
};

EmberCommandEntry emAfOptionCommands[] = {
  {"print-rx-msgs", NULL, (PGM_P)optionPrintRxCommands},
  {"register", optionRegisterCommand, ""},

  emberCommandEntryAction("disc", optionDiscoverCommand, "vv", 
                          "Perform a match descriptor request"),
  emberCommandEntryAction("target", optionDiscoveryTargetCommand, "v", 
                          "Set the target address of the CLI discovery"),

  {"binding-table", NULL, (PGM_P)optionBindingCommands},
  {"address-table", NULL, (PGM_P)optionAddressCommands},

  {"edb", optionEndDeviceBindCommand, "u"},
  {"security", NULL, (PGM_P)optionSecurityCommands},
  {"apsretry", NULL, (PGM_P)apsRetryCommands},

#ifdef DEBUG_PRINT_FOR_ROUTING_TABLE
  {"route", optionPrintRouteCommand, ""},
#endif

#if EMBER_KEY_TABLE_SIZE > 0
  {"link",         optionLinkCommand, "ubb"},
  {"install-code", optionInstallCodeCommand, "ubb"},
#endif
  { NULL }
};

static EmberNodeId discoveryTargetNodeId = EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS;


// *****************************************************************************
// Functions
// *****************************************************************************

#if defined(__MCCXAP2B__)
  // The XAP is very strict about type conversions with regard to EmberEUI64.
  // In order to cast "const int8u*" to "const EmberEUI64" 
  // we must first cast away the const of the int8u* type and then cast the
  // data to a 'const EmberEUI64'.
  #define INT8U_TO_EUI64_CAST(x) ((const EmberEUI64)((int8u*)x)) 
#else
  #define INT8U_TO_EUI64_CAST(x) (x)
#endif

void emAfCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  if (!emberAfHaveDiscoveryResponseStatus(result->status)) {
    // Do nothing
  } else if (result->zdoRequestClusterId == MATCH_DESCRIPTORS_REQUEST) {
    const EmberAfEndpointList* epList = (const EmberAfEndpointList*)result->responseData;
    emberAfAppPrintln("Match %py from 0x%2X, ep %d",
                      "discover",
                      result->matchAddress,
                      epList->list[0]);
  } else if (result->zdoRequestClusterId == NETWORK_ADDRESS_REQUEST) {
    emberAfAppPrintln("NWK Address response: 0x%2X", result->matchAddress);
  } else if (result->zdoRequestClusterId == IEEE_ADDRESS_REQUEST) {
    const int8u* eui64ptr = (int8u*)(result->responseData);
    emberAfAppPrint("IEEE Address response: ");
    emberAfPrintBigEndianEui64(INT8U_TO_EUI64_CAST(eui64ptr));
    emberAfAppPrintln("");
  }

  if (result->status != EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED) {
    emberAfAppPrintln("Service %py done.", 
                      "discover");
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}



// option print-rx-msgs [enable | disable]
static void optionPrintRxCommand(void) 
{
  if (emberCurrentCommand->name[0] == 'e') {
    emberAfPrintReceivedMessages = TRUE;
  } else {
    emberAfPrintReceivedMessages = FALSE;
  }
  emberAfAppPrintln("%pd print", 
                      (emberAfPrintReceivedMessages
                       ? "enable"
                       : "disable"));
}

// option register
static void optionRegisterCommand(void) 
{
  emberAfRegistrationStartCallback();
}

static void optionDiscoveryTargetCommand(void)
{
  discoveryTargetNodeId = (int16u)emberUnsignedCommandArgument(0);
}

// option disc <profileId> <clusterId>
static void optionDiscoverCommand(void)
{
  EmberAfProfileId profileId = (int16u)emberUnsignedCommandArgument(0);
  EmberAfClusterId clusterId = (int16u)emberUnsignedCommandArgument(1);
  emberAfFindDevicesByProfileAndCluster(discoveryTargetNodeId,
                                        profileId,
                                        clusterId, 
                                        EMBER_AF_SERVER_CLUSTER_DISCOVERY,
                                        emAfCliServiceDiscoveryCallback);
}

// option binding-table print
static void optionBindingTablePrintCommand(void)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  int8u i;
  EmberBindingTableEntry result;

  PGM_P typeStrings[] = {
    "EMPTY",
    "UNICA",
    "M2ONE",
    "MULTI",
    "?    ",
  };
  int8u bindings = 0;

  emberAfAppPrintln("#  type   nwk  loc   rem   clus   eui");
  for (i = 0; i < emberAfGetBindingTableSize(); i++) {
    EmberStatus status = emberGetBinding(i, &result);
    if (status == EMBER_SUCCESS) {
      if (result.type > EMBER_MULTICAST_BINDING) {
        result.type = 4;  // last entry in the string list above
      }
      if (result.type != EMBER_UNUSED_BINDING) {
        bindings++;
        emberAfAppPrint("%d: ", i);
        emberAfAppPrint("%p", typeStrings[result.type]);
        emberAfAppPrint("  %d    0x%x  0x%x  0x%2x ",
                        result.networkIndex,
                        result.local,
                        result.remote,
                        result.clusterId);
        emberAfAppDebugExec(emberAfPrintBigEndianEui64(result.identifier));
        emberAfAppPrintln("");
      }
    } else {
      emberAfAppPrintln("0x%x: emberGetBinding Error: %x", status);
      emberAfAppFlush();
    }
    emberAfAppFlush();
  }
  emberAfAppPrintln("%d of %d bindings used",
                    bindings,
                    emberAfGetBindingTableSize());
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
}

// option binding-table clear
static void optionBindingTableClearCommand(void)
{
  emberClearBindingTable();
}

// option address-table print
static void optionAddressTablePrintCommand(void)
{
  int8u i;
  int8u used = 0;
  emberAfAppPrintln("#  node   eui");
  for (i = 0; i < emberAfGetAddressTableSize(); i++) {
    EmberNodeId nodeId = emberGetAddressTableRemoteNodeId(i);
    if (nodeId != EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      EmberEUI64 eui64;
      used++;
      emberAfAppPrint("%d: 0x%2x ", i, nodeId);
      emberGetAddressTableRemoteEui64(i, eui64);
      emberAfAppDebugExec(emberAfPrintBigEndianEui64(eui64));
      emberAfAppPrintln("");
      emberAfAppFlush();
    }
  }
  emberAfAppPrintln("%d of %d entries used.", 
                    used, 
                    emberAfGetAddressTableSize());
}

// option address-table set <index> <eui64> <node id>
static void optionAddressTableSetCommand(void)
{
  EmberEUI64 eui64;
  EmberStatus status;
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  EmberNodeId nodeId = (EmberNodeId)emberUnsignedCommandArgument(2);
  emberAfCopyBigEndianEui64Argument(1, eui64);
  status = emberAfSetAddressTableEntry(index, eui64, nodeId);
  emberAfAppPrintln("set address %d: 0x%x", index, status);
}

// option edb <endpoint>
static void optionEndDeviceBindCommand(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  emberAfSendEndDeviceBind(endpoint);
}

static void optionApsRetryCommand(void) 
{
  if ( emberCurrentCommand->name[1] == 'e' ) {
    emberAfApsRetryOverride = EMBER_AF_RETRY_OVERRIDE_NONE;
  } else if ( emberCurrentCommand->name[1] == 'n' ) {
    emberAfApsRetryOverride = EMBER_AF_RETRY_OVERRIDE_SET;
  } else if ( emberCurrentCommand->name[1] == 'f' ) {
    emberAfApsRetryOverride = EMBER_AF_RETRY_OVERRIDE_UNSET;
  }
}

// option security aps <off | on>
static void optionApsSecurityCommand(void)
{
  emAfApsSecurityOff = (emberCurrentCommand->name[1] == 'f'
                        ? TRUE
                        : FALSE);
}

#if EMBER_KEY_TABLE_SIZE > 0

// option link <index> <eui64 in big endian format> <key in big endian format>
static void optionLinkCommand(void) 
{  
  EmberEUI64 partnerEUI64;
  EmberKeyData newKey;
  EmberStatus status;
  //int8u i;
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  emberAfCopyBigEndianEui64Argument(1, partnerEUI64);
  emberCopyKeyArgument(2, &newKey);
  status = emberSetKeyTableEntry(index, 
                                 partnerEUI64,
                                 TRUE,
                                 &newKey);
  emberAfAppDebugExec(emAfPrintStatus("add link key", status));
  emberAfAppPrintln("");
  emberAfCoreFlush();
}

// Reverse the bits in a byte.
// http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith32Bits
static int8u reverse(int8u b)
{
  return ((b * 0x0802UL & 0x22110UL) | (b * 0x8020UL & 0x88440UL)) * 0x10101UL >> 16;
}

// option install-code <index> <eui64> <install code>
static void optionInstallCodeCommand(void) 
{  
  EmberEUI64 eui64;
  EmberKeyData key;
  EmberStatus status;
  int16u crc = 0xFFFF;
  int8u code[16 + 2]; // 6, 8, 12, or 16 bytes plus two-byte CRC
  int8u i, length;

  length = emberCopyStringArgument(2, code, sizeof(code), FALSE);
  if (length != 8 && length != 10 && length != 14 && length != 18) {
    emberAfAppPrintln("ERR: Install code must be 8, 10, 14, or 18 bytes in length");
    return;
  }

  // Compute the CRC and verify that it matches.  The bit reversals, byte swap,
  // and ones' complement are due to differences between halCommonCrc16 and the
  // Smart Energy version.
  for (i = 0; i < length - 2; i++) {
    crc = halCommonCrc16(reverse(code[i]), crc);
  }
  crc = ~HIGH_LOW_TO_INT(reverse(LOW_BYTE(crc)), reverse(HIGH_BYTE(crc)));
  if (code[length - 2] != LOW_BYTE(crc)
      || code[length - 1] != HIGH_BYTE(crc)) {
    emberAfAppPrintln("ERR: Calculated CRC 0x%2x does not match", crc);
    return;
  }

  // Compute the key from the install code and CRC.
  status = emberAesHashSimple(length, code, emberKeyContents(&key));
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("ERR: AES-MMO hash failed: 0x%x", status);
    return;
  }

  // Add it to the table.
  emberAfCopyBigEndianEui64Argument(1, eui64);
  status = emberSetKeyTableEntry((int8u)emberUnsignedCommandArgument(0), // index
                                 eui64,
                                 TRUE,                                   // link key 
                                &key);
  emberAfAppDebugExec(emAfPrintStatus("add link key", status));
  emberAfAppPrintln("");
  emberAfAppFlush();
}

#endif // EMBER_KEY_TABLE_SIZE > 0

// option binding-table set <index> <cluster> <local ep> <remote ep> <EUI>
static void optionBindingTableSetCommand(void)
{
  EmberBindingTableEntry entry;
  int8u index = (int8u)emberUnsignedCommandArgument(0);
  int8u endpoint = (int8u)emberUnsignedCommandArgument(2);
  EmberStatus status = emberAfPushEndpointNetworkIndex(endpoint);
  if (status == EMBER_SUCCESS) {
    entry.type = EMBER_UNICAST_BINDING;
    entry.clusterId = (EmberAfClusterId)emberUnsignedCommandArgument(1);
    entry.local = endpoint;
    entry.remote = (int8u)emberUnsignedCommandArgument(3);
    emberAfCopyBigEndianEui64Argument(4, entry.identifier);
    status = emberSetBinding(index, &entry);
    emberAfPopNetworkIndex();
  }
  emberAfAppPrintln("set bind %d: 0x%x", index, status);
}


#ifdef DEBUG_PRINT_FOR_ROUTING_TABLE

static void optionPrintRouteCommand(void)
{
  PGM_P statusText[] = {
    "active",
    "discov",
    "??    ",
    "unused",
  };
  
  PGM_P concentratorText[] = {
    "- ",
    "lo",
    "hi",
  };

  PGM_P routeRecordStateText[] = {
    "none",
    "sent",
    "need",
  };
  int8u i;

  emberAfAppPrintln("Routing Table\n-----------------");

  for (i = 0; i < EMBER_ROUTE_TABLE_SIZE; i++) {
    EmberRouteTableEntry entry;
    EmberStatus status = emberGetRouteTableEntry(i, &entry);
    emberAfAppPrintln("%d: dest:0x%2X next:0x%2X status:%p age:%d conc:%p rr-state:%p",
                      i,
                      entry.destination,
                      entry.nextHop,
                      statusText[entry.status],
                      entry.age,
                      concentratorText[entry.concentratorType],
                      routeRecordStateText[entry.routeRecordState]);
    emberAfAppFlush();
  }
}
#endif //  DEBUG_PRINT_FOR_ROUTING_TABLE
