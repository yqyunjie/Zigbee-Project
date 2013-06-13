// *****************************************************************************
// * zdo-cli.c
// *
// * CLI commands for sending ZDO messages.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

// common include file
#include "app/framework/util/common.h"
#include "app/framework/util/af-main.h"

#include "app/util/serial/command-interpreter2.h"

#include "app/util/zigbee-framework/zigbee-device-common.h"

#if defined(EZSP_HOST)
  #include "app/util/zigbee-framework/zigbee-device-host.h"
#else // SOC
  #include "app/util/zigbee-framework/zigbee-device-library.h"
#endif

#include "app/framework/cli/option-cli.h"

//------------------------------------------------------------------------------
// Forward declarations

static void zdoNwkAddressRequestCommand(void);
static void zdoIeeeAddressRequestCommand(void);
static void zdoSimpleCommand(void);
static void zdoNodeCommand(void);
static void zdoMatchCommand(void);
static void zdoBindCommand(void);
static void zdoAddClusterCommand(void);
static void zdoClearClusterCommand(void);
static void zdoNetworkUpdateChannelCommand(void);
static void zdoNetworkUpdateScanCommand(void);
static void zdoNetworkUpdateSetCommand(void);
static void zdoActiveEpCommand(void);
static void zdoMgmtLqiCommand(void);

static EmberStatus matchDescriptorsRequest(EmberNodeId target,
                                           int16u profile,
                                           int8u inCount,
                                           int8u outCount,
                                           int16u *inClusters,
                                           int16u *outClusters,
                                           EmberApsOption options);

//------------------------------------------------------------------------------
// Globals 

static PGM_P addClusterArguments[] = {
  "ZCL Cluster to add.",
  NULL,
};

static EmberCommandEntry zdoClusterCommands[] = {
  emberCommandEntryActionWithDetails("add", 
                                     zdoAddClusterCommand, 
                                     "v",
                                     "Add a ZCL cluster to the CLI's list.",
                                     addClusterArguments),
  emberCommandEntryAction("clear", 
                          zdoClearClusterCommand, 
                          "",
                          "Remove all ZCL clusters from the CLI's list"),
  emberCommandEntryTerminator(),
};

static PGM_P channelChangeArguments[] = {
  "Channel to change to.",
  NULL,
};

static PGM_P channelScanArguments[] = {
  "Target Node ID",
  "Scan Duration",  // see stack/include/zigbee-device-stack.h for enumeration
  "Scan count",
  NULL,
};

static PGM_P channelManagerArguments[] = {
  "NWK manager node ID",
  "Channel mask",
  NULL,
};

static EmberCommandEntry zdoNetworkUpdateCommands[] = {
  emberCommandEntryActionWithDetails("chan", 
                                     zdoNetworkUpdateChannelCommand, 
                                     "u",
                                     "Send a channel change command.",
                                     channelChangeArguments),
  emberCommandEntryActionWithDetails("scan", 
                                     zdoNetworkUpdateScanCommand,    
                                     "vuv",
                                     "Tell a remote node to perform a channel scan",
                                     channelScanArguments),
  emberCommandEntryActionWithDetails("set",  
                                     zdoNetworkUpdateSetCommand,     
                                     "vw",
                                     "Broadcast a new NWK manager Node ID and channel list.",
                                     channelManagerArguments),
  emberCommandEntryTerminator(),
};


static PGM_P simpleDescriptorArguments[] = {
  "The target node ID",
  "The target endpoint",
  NULL,
};

static PGM_P zdoMatchCommandArguments[] = {
  "Target node ID",
  "Profile ID",
  NULL,
};

static PGM_P zdoBindCommandArguments[] = {
  "Dest node ID",
  "local EP",
  "remote EP",
  "cluster ID",
  "THEIR EUI",
  "binding dest EUI",
  NULL,
};

static PGM_P zdoMgmtLqiCommandArguments[] = {
  "Dest node ID",
  "start index",
  NULL,
};

EmberCommandEntry zdoCommands[] = {
  emberCommandEntryAction("active",
                          zdoActiveEpCommand,
                          "v",
                          "Send an Active EP request"),
  emberCommandEntryActionWithDetails("bind",
                                     zdoBindCommand,                          
                                     "vuuvbb",
                                     "Sends bind request",
                                     zdoBindCommandArguments),
  emberCommandEntryAction("ieee",
                          zdoIeeeAddressRequestCommand,
                          "v",
                          "Unicast an IEEE address request to the specified node."),
  emberCommandEntrySubMenu("in-cl-list",  
                           zdoClusterCommands, 
                           "Modify input cluster list"),
  emberCommandEntryActionWithDetails("match",
                                     zdoMatchCommand,
                                     "vv",
                                     "Send a match descriptor request using CLI configured in/out clusters.",
                                     zdoMatchCommandArguments),
  emberCommandEntryActionWithDetails("mgmt-lqi",
                                     zdoMgmtLqiCommand,
                                     "vu",
                                     "Send a MGMT-LQI request to target",
                                     zdoMgmtLqiCommandArguments),
  emberCommandEntryAction("node",
                          zdoNodeCommand,
                          "v",
                          "Send a node descriptor request to target"),
  emberCommandEntryAction("nwk", 
                          zdoNwkAddressRequestCommand,
                          "b",
                          "Broadcast a NWK address request for the specified IEEE."),
  emberCommandEntrySubMenu("nwk-upd",     
                           zdoNetworkUpdateCommands,
                           "Frequency Agility Commands"),
  emberCommandEntrySubMenu("out-cl-list", 
                           zdoClusterCommands,
                           "Modify output cluster list"),
  emberCommandEntryActionWithDetails("simple",      
                                     zdoSimpleCommand,                        
                                     "vu",
                                     "Send a Simple Descriptor request to the target node and endpoint",
                                     simpleDescriptorArguments),
  emberCommandEntryTerminator(),
};

#define MAX_CLUSTERS_CAN_MATCH 5
static int16u zdoInClusters[MAX_CLUSTERS_CAN_MATCH];
static int16u zdoOutClusters[MAX_CLUSTERS_CAN_MATCH];
static int8u inClCount = 0;
static int8u outClCount = 0;

//------------------------------------------------------------------------------

// *****************************************
// zdoCommand
//
// zdo nwk <eui64:big-endian>
// zdo ieee <node-id>
// zdo simple <dest> <target ep>
// zdo node <dest>
// zdo match  <dest> <profile>
// zdo bind   <dest> <local ep> <remote ep> <cluster> <THEIR eui> <dest eui>
// zdo in-cl-list add <cluster IDs>
// zdo in-cl-list clear
// zdo out-cl-list add <clusters IDs>
// zdo out-cl-list clear
// zdo nwk-upd chan  <channel>
// zdo nwk-upd set   <nwk mgr id>  <chan mask>
// zdo nwk-upd scan  <target> <duration> <count>
// zdo active <target>
// *****************************************

static void zdoNwkAddressRequestCommand(void)
{
  EmberEUI64 eui64;
  emberAfCopyBigEndianEui64Argument(0, eui64);
  emberAfFindNodeId(eui64,
                    emAfCliServiceDiscoveryCallback);
}

static void zdoIeeeAddressRequestCommand(void)
{
  EmberNodeId id = (EmberNodeId)emberUnsignedCommandArgument(0);
  emberAfFindIeeeAddress(id,
                         emAfCliServiceDiscoveryCallback);
}

static void zdoSimpleCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u targetEndpoint = (int8u)emberUnsignedCommandArgument(1);
  EmberStatus status = emberSimpleDescriptorRequest(target,
                                                    targetEndpoint,
                                                    EMBER_AF_DEFAULT_APS_OPTIONS);
  emberAfAppPrintln("ZDO simple desc req %x", status);
}

static void zdoNodeCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  EmberStatus status = emberNodeDescriptorRequest(target,
                                                  EMBER_AF_DEFAULT_APS_OPTIONS);
  emberAfAppPrintln("ZDO node desc req %x", status);
}

static void zdoMatchCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int16u profile = (int16u)emberUnsignedCommandArgument(1);
  EmberStatus status = matchDescriptorsRequest(target,
                                               profile,
                                               inClCount,
                                               outClCount,
                                               zdoInClusters, 
                                               zdoOutClusters, 
                                               EMBER_AF_DEFAULT_APS_OPTIONS);
  emberAfAppPrintln("ZDO match desc req %x", status);
}

// For simple bind requests, just put {} as the last argument
//   zdo bind <dest> <local ep> <remote ep> <cluster> <THEIR EUI> {}
//
// More complex requests, you can actually specify the dest EUI64 of,
// the binding, which is NOT the same as the EUI64 of the destination
// of the device receiving the binding.  This allows for a user
// to set multiple bindings on the destination for different devices.
//   zdo bind <dest> <local ep> <remote ep> <cluster> <THEIR EUI> <dest EUI64>
static void zdoBindCommand(void)
{
  EmberStatus status;
  EmberEUI64 sourceEui, destEui;  // names relative to binding sent over-the-air
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u sourceEndpoint = (int8u)emberUnsignedCommandArgument(1);
  int8u destinationEndpoint = (int8u)emberUnsignedCommandArgument(2);
  int16u clusterId = (int16u)emberUnsignedCommandArgument(3);

  // NOTE:  The source/dest EUI is relative to the context.
  // In the case of the syntax of the CLI, we take "THEIR EUI64" to mean
  // the recipient of the binding request message.  However, when sending
  // the bind request that EUI64 used by the ZDO command is
  // actually the source for the binding because we are telling the remote
  // device (the destination) to create a binding with a source of itself.
  // And the destination for that binding will be this local device.
  // This is also not to be confused with the (short) destination of the ZDO
  // request itself.  
  emberAfCopyBigEndianEui64Argument(4, sourceEui);  // their EUI (the destination's)

  // If the last argument is empty, assume an EUI64 of the local device.
  // This allows for the simple case.  If an EUI64 is specified, it will
  // be used instead of the local EUI.  This is used for setting
  // multiple bindings on the same remote device.
  if (0 == emberAfCopyBigEndianEui64Argument(5, destEui)) {
    emberAfAppPrintln("Using my local EUI64 for dest EUI64 in binding");
    emberAfGetEui64(destEui);
  }

  status = emberBindRequest(target,          // who gets the bind req
                            sourceEui,       // source eui IN the binding
                            sourceEndpoint,
                            clusterId,       
                            UNICAST_BINDING, // binding type
                            destEui,         // destination eui IN the binding
                            0,               // groupId for new binding
                            destinationEndpoint,
                            EMBER_AF_DEFAULT_APS_OPTIONS);
  emberAfAppPrintln("ZDO bind req %x", status);
}

static void zdoAddClusterCommand(void)
{
  int16u *clusters;
  int8u *clCount;
  if (emberStringCommandArgument(-2, NULL)[0]  == 'i') {
    clusters = zdoInClusters;
    clCount = &inClCount;
  } else {
    clusters = zdoOutClusters;
    clCount = &outClCount;
  }

  if (*clCount < MAX_CLUSTERS_CAN_MATCH) {
    clusters[*clCount] = (int16u)emberUnsignedCommandArgument(0);
    (*clCount)++;
  } else {
    emberAfAppPrintln("cluster limit reached");
  }
}

static void zdoClearClusterCommand(void)
{
  if (emberStringCommandArgument(-2, NULL)[0]  == 'i') {
    inClCount = 0;
  } else {
    outClCount = 0;
  }
}

static void zdoNetworkUpdateChannelCommand(void)
{
  int8u channel = (int8u)emberUnsignedCommandArgument(0);
  if (channel < EMBER_MIN_802_15_4_CHANNEL_NUMBER
      || channel > EMBER_MAX_802_15_4_CHANNEL_NUMBER) {
    emberAfAppPrintln("invalid channel: %d", channel);
  } else {
    EmberStatus status = emberChannelChangeRequest(channel);
    emberAfAppPrint("change channel status 0x%x", status);
  }
}

static void zdoNetworkUpdateScanCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u scanDuration = (int8u)emberUnsignedCommandArgument(1);
  int16u scanCount = (int16u)emberUnsignedCommandArgument(2);
  if (scanDuration > 5 || scanCount == 0 || scanCount > 8) {
    emberAfAppPrintln("duration must be in range 0 - 5");
    emberAfAppPrintln("count must be in range 1 - 8");
  } else {
    EmberStatus status = emberEnergyScanRequest(target,
                                                EMBER_ALL_802_15_4_CHANNELS_MASK,
                                                scanDuration,
                                                scanCount);
    emberAfAppPrint("scan status 0x%x", status);
  }
}

static void zdoNetworkUpdateSetCommand(void)
{
  EmberNodeId networkManager = (EmberNodeId)emberUnsignedCommandArgument(0);
  int32u activeChannels = emberUnsignedCommandArgument(1);
  EmberStatus status = emberSetNetworkManagerRequest(networkManager,
                                                     activeChannels);
  emberAfAppPrint("network update set status 0x%x", status);  
}

static void zdoActiveEpCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  EmberStatus status = emberActiveEndpointsRequest(target,
                                                   EMBER_APS_OPTION_RETRY);
  emberAfAppPrint("Active EP request status: 0x%X",
                  status);
}

static void zdoMgmtLqiCommand(void)
{
  EmberNodeId target = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u index = emberUnsignedCommandArgument(1);
  EmberStatus status = emberLqiTableRequest(target,
                                            index,
                                            EMBER_APS_OPTION_RETRY);
  emberAfAppPrint("LQI Table request: 0x%X", status);
}

//------------------------------------------------------------------------------
// Platform specific code

#if defined(EZSP_HOST)
EmberStatus matchDescriptorsRequest(EmberNodeId target,
                                    int16u profile,
                                    int8u inCount,
                                    int8u outCount,
                                    int16u *inClusters,
                                    int16u *outClusters,
                                    EmberApsOption options)
{
  return ezspMatchDescriptorsRequest(target, 
                                     profile,
                                     inCount,
                                     outCount,
                                     inClusters,
                                     outClusters,
                                     options);
}

#else

// Copy the list of int16u input and output cluster lists into
// message buffers.
EmberStatus matchDescriptorsRequest(EmberNodeId target,
                                    int16u profile,
                                    int8u inCount,
                                    int8u outCount,
                                    int16u *inClusters,
                                    int16u *outClusters,
                                    EmberApsOption options)
{
  int8u i;
  int8u output;
  EmberMessageBuffer inClusterBuffer = EMBER_NULL_MESSAGE_BUFFER;
  EmberMessageBuffer outClusterBuffer = EMBER_NULL_MESSAGE_BUFFER;
  EmberStatus status = EMBER_NO_BUFFERS;
  for (output = 0; output < 2; output++) {
    EmberMessageBuffer* bufferPtr;
    int8u count;
    int16u* list;
    if (output) {
      count = outCount;
      list = outClusters;
      bufferPtr = &outClusterBuffer;
    } else {  // input
      count = inCount;
      list = inClusters;
      bufferPtr = &inClusterBuffer;
    }
    if (count == 0) {
      continue;
    }

    *bufferPtr = emberAllocateStackBuffer();
    if (*bufferPtr == EMBER_NULL_MESSAGE_BUFFER) {
      goto cleanup;
    }

    for (i = 0; i < count; i++) {
      int8u cluster[2];
      cluster[0] = LOW_BYTE(list[i]);
      cluster[1] = HIGH_BYTE(list[i]);
      status = emberAppendToLinkedBuffers(*bufferPtr,
                                          cluster,
                                          2);
      if (EMBER_SUCCESS != status) {
        goto cleanup;
      }
    }
  }
  status = emberMatchDescriptorsRequest(target,
                                        profile,
                                        inClusterBuffer,
                                        outClusterBuffer,
                                        options);
 cleanup:
  if (inClusterBuffer != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(inClusterBuffer);
  }
  if (outClusterBuffer != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(outClusterBuffer);
  }
  return status;
}

#endif // !defined(EZSP_HOST)
