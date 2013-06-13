// *******************************************************************
// * util.c
// *
// * This file contains all of the common ZCL command and attribute handling 
// * code for Ember's ZCL implementation
// *
// * The actual compiled version of this code will vary depending on
// * #defines for clusters included in the built application.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


#include "../include/af.h"
#include "af-main.h"
#include "common.h"
#include "../security/crypto-state.h"
#include "../plugin/time-server/time-server.h"
#include "../../util/source-route-common.h"

#include "app/framework/util/af-event.h"

//------------------------------------------------------------------------------
// Forward Declarations

static void platformTick(void);

#ifdef EMBER_AF_HEARTBEAT_ENABLE
  #define blinkHeartbeat() halToggleLed(EMBER_AF_HEARTBEAT_LED)
#else
  #define blinkHeartbeat()
#endif 

//------------------------------------------------------------------------------
// Globals

// Storage and functions for turning on and off devices
boolean afDeviceEnabled[MAX_ENDPOINT_COUNT];

#ifdef EMBER_AF_ENABLE_STATISTICS
// a variable containing the number of messages send from the utilities
// since emberAfInit was called.
int32u afNumPktsSent;
#endif

PGM EmberAfClusterName zclClusterNames[] = {
  CLUSTER_IDS_TO_NAMES                 // defined in print-cluster.h
  { ZCL_NULL_CLUSTER_ID, NULL },  // terminator
};

// A pointer to the current command being processed
// This struct is allocated on the stack inside
// emberAfProcessMessage. The pointer below is set
// to NULL when the function exits.
EmberAfClusterCommand *emAfCurrentCommand;

// variable used for turning off Aps Link security. Set by the CLI 
boolean emAfApsSecurityOff = FALSE;

// DEPRECATED.
int8u emberAfIncomingZclSequenceNumber = 0xFF;

static boolean afNoSecurityForDefaultResponse = FALSE;

// Sequence used for outgoing messages if they are
// not responses.
int8u emberAfSequenceNumber = 0xFF;

// A boolean value so we know when the device is performing
// key establishment.
boolean emAfDeviceIsPerformingKeyEstablishment = FALSE;

int8u emberAfApsRetryOverride = EMBER_AF_RETRY_OVERRIDE_NONE;

// Holds the response type
int8u emberAfResponseType = ZCL_UTIL_RESP_NORMAL;

static EmberAfInterpanHeader interpanResponseHeader;

#if EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE != 0
  static int8u addressTableReferenceCounts[EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE];
#endif

#define SECONDS_IN_DAY 86400L
#define SECONDS_IN_HOUR 3600
PGM int8u emberAfDaysInMonth[] =
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static PGM int8u emberAfAnalogDiscreteThresholds[] = {
  0x07, EMBER_AF_DATA_TYPE_NONE,
  0x1F, EMBER_AF_DATA_TYPE_DISCRETE,
  0x2F, EMBER_AF_DATA_TYPE_ANALOG,
  0x37, EMBER_AF_DATA_TYPE_DISCRETE,
  0x3F, EMBER_AF_DATA_TYPE_ANALOG,
  0x57, EMBER_AF_DATA_TYPE_DISCRETE,
  0xDF, EMBER_AF_DATA_TYPE_NONE,
  0xE7, EMBER_AF_DATA_TYPE_ANALOG,
  0xFF, EMBER_AF_DATA_TYPE_NONE 
};

typedef void (TickFunction)(void);

static TickFunction* PGM internalTickFunctions[] = {
  // This is the main customer tick
  emberAfMainTickCallback,

  platformTick,

  NULL            // terminator, must be last
};

#ifdef EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_DECLARATIONS
  EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_DECLARATIONS
#endif

//------------------------------------------------------------------------------

static void platformTick(void)
{
  static int16u lastBlinkTime = 0;
  int16u time;

  emberAfSchedulePollEventCallback();
  emberAfCheckForSleepCallback();

  time = halCommonGetInt16uMillisecondTick();

  if (elapsedTimeInt16u(lastBlinkTime, time) > 200 /*ms*/) {
    blinkHeartbeat();
    lastBlinkTime = time;
  }
}

// Device enabled/disabled functions
boolean emberAfIsDeviceEnabled(int8u endpoint)
{
  int8u index;
#ifdef ZCL_USING_BASIC_CLUSTER_DEVICE_ENABLED_ATTRIBUTE
  boolean deviceEnabled;
  if (emberAfReadServerAttribute(endpoint,
                                 ZCL_BASIC_CLUSTER_ID,
                                 ZCL_DEVICE_ENABLED_ATTRIBUTE_ID,
                                 (int8u *)&deviceEnabled,
                                 sizeof(deviceEnabled))
      == EMBER_ZCL_STATUS_SUCCESS) {
    return deviceEnabled;
  }
#endif
  index = emberAfIndexFromEndpoint(endpoint);
  if (index != 0xFF && index < sizeof(afDeviceEnabled)) {
    return afDeviceEnabled[index];
  }
  return FALSE;
}

void emberAfSetDeviceEnabled(int8u endpoint, boolean enabled)
{
  int8u index = emberAfIndexFromEndpoint(endpoint);
  if (index != 0xFF && index < sizeof(afDeviceEnabled)) {
    afDeviceEnabled[index] = enabled;
  }
#ifdef ZCL_USING_BASIC_CLUSTER_DEVICE_ENABLED_ATTRIBUTE
  emberAfWriteServerAttribute(endpoint,
                              ZCL_BASIC_CLUSTER_ID,
                              ZCL_DEVICE_ENABLED_ATTRIBUTE_ID,
                              (int8u *)&enabled,
                              ZCL_BOOLEAN_ATTRIBUTE_TYPE);
#endif
}

// Is the device identifying?
boolean emberAfIsDeviceIdentifying(int8u endpoint)
{
#ifdef ZCL_USING_IDENTIFY_CLUSTER_SERVER
  int16u identifyTime;
  EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                                    ZCL_IDENTIFY_CLUSTER_ID,
                                                    ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                                    (int8u *)&identifyTime,
                                                    sizeof(identifyTime));
  return (status == EMBER_ZCL_STATUS_SUCCESS && 0 < identifyTime);
#else
  return FALSE;
#endif
}


// Calculates difference. Works only for dataSizes of 4 or less.
int32u emberAfGetDifference(int8u *pData, int32u value, int8u dataSize) 
{
  int32u value2 = 0, diff;
  int8u i;

  // only support data types up to 4 bytes
  if (dataSize > 4) {
    return 0;
  }

  // get the 32-bit value
  for (i = 0; i < dataSize; i++) {
    value2 = value2 << 8;
#if (BIGENDIAN_CPU)
    value2 += pData[i];
#else //BIGENDIAN
    value2 += pData[dataSize - i - 1];
#endif //BIGENDIAN
  }

  if (value > value2) {
    diff = value - value2; 
  } else {
    diff = value2 - value; 
  }

  //emberAfDebugPrintln("comparing 0x%4x and 0x%4x, diff is 0x%4x",
  //                    value, value2, diff);
  
  return diff;
}

// --------------------------------------------------

static void prepareForResponse(const EmberAfClusterCommand *cmd)
{
  emberAfResponseApsFrame.profileId           = cmd->apsFrame->profileId;
  emberAfResponseApsFrame.clusterId           = cmd->apsFrame->clusterId;
  emberAfResponseApsFrame.sourceEndpoint      = cmd->apsFrame->destinationEndpoint;
  emberAfResponseApsFrame.destinationEndpoint = cmd->apsFrame->sourceEndpoint;

  // Use the default APS options for the response, but also use encryption and
  // retries if the incoming message used them.  The rationale is that the
  // sender of the request cares about some aspects of the delivery, so we as
  // the receiver should make equal effort for the response.
  emberAfResponseApsFrame.options = EMBER_AF_DEFAULT_APS_OPTIONS;
  if (cmd->apsFrame->options & EMBER_APS_OPTION_ENCRYPTION) {
    emberAfResponseApsFrame.options |= EMBER_APS_OPTION_ENCRYPTION;
  }
  if (cmd->apsFrame->options & EMBER_APS_OPTION_RETRY) {
    emberAfResponseApsFrame.options |= EMBER_APS_OPTION_RETRY;
  }

  if (cmd->interPanHeader == NULL) {
    emberAfResponseDestination = cmd->source;
    emberAfResponseType &= ~ZCL_UTIL_RESP_INTERPAN;
  } else {
    emberAfResponseType |= ZCL_UTIL_RESP_INTERPAN;
    MEMCOPY(&interpanResponseHeader, 
            cmd->interPanHeader, 
            sizeof(EmberAfInterpanHeader));
    // Always send responses as unicast
    interpanResponseHeader.messageType = EMBER_AF_INTER_PAN_UNICAST;
  }
}

// ****************************************
// Initialize Clusters
// ****************************************
void emberAfInit(void) 
{
  int8u i;
#ifdef EMBER_AF_ENABLE_STATISTICS  
  afNumPktsSent = 0;
#endif

  for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
    emberAfPushNetworkIndex(i);
    emberAfLoadAttributesFromDefaults(EMBER_BROADCAST_ENDPOINT);
    emberAfLoadAttributesFromTokens(EMBER_BROADCAST_ENDPOINT);
    emberAfPopNetworkIndex();
  }

  MEMSET(afDeviceEnabled, TRUE, emberAfEndpointCount());

  // Set up client API buffer.
  emberAfSetExternalBuffer(appResponseData,
                           EMBER_AF_RESPONSE_BUFFER_LEN,
                           &appResponseLength,
                           &emberAfResponseApsFrame);

 // initialize event management system
 emAfInitEvents();

#ifdef EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_CALLS
  EMBER_AF_GENERATED_PLUGIN_INIT_FUNCTION_CALLS
#endif

  emAfCallInits();
}

void emberAfTick(void) 
{
  int8u i = 0;

  while (internalTickFunctions[i] != NULL) {
    if (emAfIsCryptoOperationInProgress()) {
      // Wait until ECC operations are done.  Don't allow
      // any of the clusters to send messages.  This is necessary
      // on host or SOC application.
      return;
    }

    (internalTickFunctions[i])();
    i++;
  }
}

// ****************************************
// This function is called by the application when the stack goes down,
// such as after a leave network. This allows zcl utils to clear state 
// that should not be kept when changing networks
// ****************************************
void emberAfStackDown(void)
{
  // (Case 14696) Clearing the report table is only necessary if the stack is
  // going down for good; if we're rejoining, leave the table intact since we'll
  // be right back, hopefully.
  if (emberStackIsPerformingRejoin() == FALSE) {
    // the report table should be cleared when the stack comes down. 
    // going to a new network means new report devices should be discovered.
    // if the table isnt cleared the device keeps trying to send messages.
    emberAfClearReportTableCallback();
  }

  emberAfRegistrationAbortCallback();
  emberAfTrustCenterKeepaliveAbortCallback();
}

// ****************************************
// Print out information about each cluster
// ****************************************

int16u emberAfFindClusterNameIndex(int16u cluster)
{
  int16u index = 0;
  while (zclClusterNames[index].id != ZCL_NULL_CLUSTER_ID) {
    if (zclClusterNames[index].id == cluster) {
      return index;
    }
    index++;
  }  
  return 0xFFFF;
}

void emberAfDecodeAndPrintCluster(int16u cluster)
{
  int16u index = emberAfFindClusterNameIndex(cluster);
  if (index == 0xFFFF) { 
    emberAfPrint(emberAfPrintActiveArea,
                 "(Unknown clus. [0x%2x])", 
                 cluster);
  } else {
    emberAfPrint(emberAfPrintActiveArea, 
                 "(%p)", 
                 zclClusterNames[index].name);
  }
  emberAfFlush(emberAfPrintActiveArea);
}

static void printIncomingZclMessage(const EmberAfClusterCommand *cmd)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_APP)
  if (emberAfPrintReceivedMessages) {
    emberAfAppPrint("\r\nT%4x:", emberAfGetCurrentTime());
    emberAfAppPrint("RX len %d, ep %x, clus 0x%2x ",
                    cmd->bufLen,
                    cmd->apsFrame->destinationEndpoint,
                    cmd->apsFrame->clusterId);
    emberAfAppDebugExec(emberAfDecodeAndPrintCluster(cmd->apsFrame->clusterId));
    if (cmd->mfgSpecific) {
      emberAfAppPrint(" mfgId %2x", cmd->mfgCode);
    }
    emberAfAppPrint(" FC %x seq %x cmd %x payload[",
                    cmd->buffer[0], // frame control
                    cmd->seqNum,
                    cmd->commandId);
    emberAfAppFlush();
    emberAfAppPrintBuffer(cmd->buffer + cmd->payloadStartIndex, // message
                          cmd->bufLen - cmd->payloadStartIndex, // length
                          TRUE);                                // spaces?
    emberAfAppFlush();
    emberAfAppPrintln("]");
  }
#endif
}

static boolean dispatchZclMessage(EmberAfClusterCommand *cmd)
{
  int8u index = emberAfIndexFromEndpoint(cmd->apsFrame->destinationEndpoint);
  if (index == 0xFF) {
    emberAfDebugPrint("Drop cluster 0x%2x command 0x%x",
                      cmd->apsFrame->clusterId,
                      cmd->commandId);
    emberAfDebugPrint(" due to invalid endpoint: ");
    emberAfDebugPrintln("0x%x", cmd->apsFrame->destinationEndpoint);
    return FALSE;
  } else if (emberAfNetworkIndexFromEndpointIndex(index) != cmd->networkIndex) {
    emberAfDebugPrint("Drop cluster 0x%2x command 0x%x",
                      cmd->apsFrame->clusterId,
                      cmd->commandId);
    emberAfDebugPrint(" for endpoint 0x%x due to wrong %p: ",
                      cmd->apsFrame->destinationEndpoint,
                      "network");
    emberAfDebugPrintln("%d", cmd->networkIndex);
    return FALSE;
  } else if (emberAfProfileIdFromIndex(index) != cmd->apsFrame->profileId
             && (cmd->apsFrame->profileId != EMBER_WILDCARD_PROFILE_ID
                 || (EMBER_MAXIMUM_STANDARD_PROFILE_ID
                     < emberAfProfileIdFromIndex(index)))) {
    emberAfDebugPrint("Drop cluster 0x%2x command 0x%x",
                      cmd->apsFrame->clusterId,
                      cmd->commandId);
    emberAfDebugPrint(" for endpoint 0x%x due to wrong %p: ",
                      cmd->apsFrame->destinationEndpoint,
                      "profile");
    emberAfDebugPrintln("0x%2x", cmd->apsFrame->profileId);
    return FALSE;
  } else if ((cmd->type == EMBER_INCOMING_MULTICAST
              || cmd->type == EMBER_INCOMING_MULTICAST_LOOPBACK)
             && !emberAfGroupsClusterEndpointInGroupCallback(cmd->apsFrame->destinationEndpoint,
                                                             cmd->apsFrame->groupId)) {
    emberAfDebugPrint("Drop cluster 0x%2x command 0x%x",
                      cmd->apsFrame->clusterId,
                      cmd->commandId);
    emberAfDebugPrint(" for endpoint 0x%x due to wrong %p: ",
                      cmd->apsFrame->destinationEndpoint,
                      "group");
    emberAfDebugPrintln("0x%2x", cmd->apsFrame->groupId);
    return FALSE;
  } else {
    return (cmd->clusterSpecific
            ? emAfProcessClusterSpecificCommand(cmd)
            : emAfProcessGlobalCommand(cmd));
  }
}

// a single call to process global and cluster-specific messages and callbacks.
boolean emberAfProcessMessage(EmberApsFrame *apsFrame,
                              EmberIncomingMessageType type,
                              int8u *message,
                              int16u msgLen,
                              EmberNodeId source,
                              InterPanHeader *interPanHeader)
{
  EmberAfClusterCommand cmd;
  boolean msgHandled = FALSE;

  // This function handles ZCL messages, so the message must have the header.
  if (msgLen < EMBER_AF_ZCL_OVERHEAD
      || (message[0] & ZCL_MANUFACTURER_SPECIFIC_MASK
          && msgLen < EMBER_AF_ZCL_MANUFACTURER_SPECIFIC_OVERHEAD)) {
    emberAfAppPrintln("%pRX pkt too short!", "ERROR: ");
    goto kickout;
  }

  // Populate the cluster command struct for processing.
  cmd.apsFrame        = apsFrame;
  cmd.type            = type;
  cmd.source          = source;
  cmd.buffer          = message;
  cmd.bufLen          = msgLen;
  cmd.clusterSpecific = (message[0] & ZCL_CLUSTER_SPECIFIC_COMMAND);
  cmd.mfgSpecific     = (message[0] & ZCL_MANUFACTURER_SPECIFIC_MASK);
  cmd.direction       = ((message[0] & ZCL_FRAME_CONTROL_DIRECTION_MASK)
                          ? ZCL_DIRECTION_SERVER_TO_CLIENT
                          : ZCL_DIRECTION_CLIENT_TO_SERVER);
  cmd.payloadStartIndex = 1;
  if (cmd.mfgSpecific) {
    cmd.mfgCode = emberAfGetInt16u(message, cmd.payloadStartIndex, msgLen);
    cmd.payloadStartIndex += 2;
  } else {
    cmd.mfgCode = EMBER_AF_NULL_MANUFACTURER_CODE;
  }
  cmd.seqNum         = message[cmd.payloadStartIndex++];
  cmd.commandId      = message[cmd.payloadStartIndex++];
  cmd.interPanHeader = interPanHeader;
  cmd.networkIndex   = emberGetCurrentNetwork();
  emAfCurrentCommand = &cmd;

  // All of these should be covered by the EmberAfClusterCommand but are
  // still here until all the code is moved over to use the cmd. -WEH
  emberAfIncomingZclSequenceNumber = cmd.seqNum;

  printIncomingZclMessage(&cmd);
  prepareForResponse(&cmd);

  if (emberAfPreCommandReceivedCallback(&cmd)) {
    msgHandled = TRUE;
    goto kickout;
  }

  if (interPanHeader == NULL) {
    boolean broadcast = (type == EMBER_INCOMING_BROADCAST
                         || type == EMBER_INCOMING_BROADCAST_LOOPBACK
                         || type == EMBER_INCOMING_MULTICAST
                         || type == EMBER_INCOMING_MULTICAST_LOOPBACK);

    // if the cluster for the incoming message requires security and
    // doesnt have it return default response STATUS_FAILURE
    if (emberAfDetermineIfLinkSecurityIsRequired(cmd.commandId,
                                                 TRUE, // incoming
                                                 broadcast,
                                                 cmd.apsFrame->profileId,
                                                 cmd.apsFrame->clusterId)
        && (!(cmd.apsFrame->options & EMBER_APS_OPTION_ENCRYPTION))) {
      emberAfDebugPrintln("Drop clus %2x due to no aps security",
                          cmd.apsFrame->clusterId);
      afNoSecurityForDefaultResponse = TRUE;
      emberAfSendDefaultResponse(&cmd, EMBER_ZCL_STATUS_FAILURE);
      afNoSecurityForDefaultResponse = FALSE;

      // Mark the message as processed.  It failed security processing, so no
      // other parts of the code act should upon it.
      msgHandled = TRUE;
      goto kickout;
    }
  } else if (!(interPanHeader->options
               & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS)) {
    // For safety, dump all interpan messages that don't have a long
    // source in the MAC layer.  In theory they should not get past
    // the MAC filters but this is insures they will not get processed.
    goto kickout;
  }

  if (cmd.apsFrame->destinationEndpoint == EMBER_BROADCAST_ENDPOINT) {
    int8u i;
    for (i = 0; i < emberAfEndpointCount(); i++) {
      if (!emberAfEndpointIndexIsEnabled(i)) {
        continue;
      }
      // Since the APS frame is cleared after each sending, 
      // we must reinitialize it.  It is cleared to prevent
      // data from leaking out and being sent inadvertently.
      prepareForResponse(&cmd);

      // Change the destination endpoint of the incoming command and the source
      // source endpoint of the response so they both reflect the endpoint the
      // message is actually being passed to in this iteration of the loop.
      cmd.apsFrame->destinationEndpoint      = emberAfEndpointFromIndex(i);
      emberAfResponseApsFrame.sourceEndpoint = emberAfEndpointFromIndex(i);
      if (dispatchZclMessage(&cmd)) {
        msgHandled = TRUE;
      }
    }
  } else {
    msgHandled = dispatchZclMessage(&cmd);
  }

kickout:
  emberAfClearResponseData();
  MEMSET(&interpanResponseHeader,
         0,
         sizeof(EmberAfInterpanHeader));
  emAfCurrentCommand = NULL;
  return msgHandled;
}

int8u emberAfNextSequence(void)
{
  return ((++emberAfSequenceNumber) & EMBER_AF_ZCL_SEQUENCE_MASK);
}

int8u emberAfGetLastSequenceNumber(void)
{
  return (emberAfSequenceNumber & EMBER_AF_ZCL_SEQUENCE_MASK);
}

// the caller to the library can set a flag to say do not respond to the
// next ZCL message passed in to the library. Passing TRUE means disable
// the reply for the next ZCL message. Setting to FALSE re-enables the
// reply (in the case where the app disables it and then doesnt send a 
// message that gets parsed).
void emberAfSetNoReplyForNextMessage(boolean set)
{
  if (set) {
    emberAfResponseType |= ZCL_UTIL_RESP_NONE;
  } else {
    emberAfResponseType &= ~ZCL_UTIL_RESP_NONE;
  }
}

EmberStatus emberAfSendResponse(void)
{
  EmberStatus status;
  int8u label;

  // If the no-response flag is set, dont send anything.
  if (emberAfResponseType & ZCL_UTIL_RESP_NONE) {
    emberAfDebugPrintln("ZCL Util: no response at user request");
    return EMBER_SUCCESS;
  }

  if (emberAfApsRetryOverride == EMBER_AF_RETRY_OVERRIDE_SET) {
    emberAfResponseApsFrame.options |= EMBER_APS_OPTION_RETRY;
  } else if (emberAfApsRetryOverride == EMBER_AF_RETRY_OVERRIDE_UNSET) {
    emberAfResponseApsFrame.options &= ~EMBER_APS_OPTION_RETRY;
  }

  // Fill commands may increase the sequence.  For responses, we want to make
  // sure the sequence is reset to that of the request.
  if (appResponseData[0] & ZCL_MANUFACTURER_SPECIFIC_MASK) {
    appResponseData[3] = emberAfIncomingZclSequenceNumber;
  } else {
    appResponseData[1] = emberAfIncomingZclSequenceNumber;
  }

  // The manner in which the message is sent depends on the response flags and
  // the destination of the message.
  if (emberAfResponseType & ZCL_UTIL_RESP_INTERPAN) {
    label = 'I';
    status = emberAfInterpanSendMessageCallback(&interpanResponseHeader,
                                                appResponseLength,
                                                appResponseData);
    emberAfResponseType &= ~ZCL_UTIL_RESP_INTERPAN;
  } else if (emberAfResponseDestination < EMBER_BROADCAST_ADDRESS) {
    label = 'U';
    status = emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
                                emberAfResponseDestination,
                                &emberAfResponseApsFrame,
                                appResponseLength,
                                appResponseData);
  } else {
    label = 'B';
    status = emberAfSendBroadcast(emberAfResponseDestination,
                                  &emberAfResponseApsFrame,
                                  appResponseLength,
                                  appResponseData);
  }

  emberAfDebugPrintln("T%4x:TX (%p) %ccast 0x%x%p",
                      emberAfGetCurrentTime(),
                      "resp",
                      label,
                      status,
                      ((emberAfResponseApsFrame.options
                        & EMBER_APS_OPTION_ENCRYPTION)
                       ? " w/ link key" : ""));
  emberAfDebugPrint("TX buffer: [");
  emberAfDebugFlush();
  emberAfDebugPrintBuffer(appResponseData, appResponseLength, TRUE);
  emberAfDebugPrintln("]");
  emberAfDebugFlush();

#ifdef EMBER_AF_ENABLE_STATISTICS
  if (status == EMBER_SUCCESS) {
    afNumPktsSent++;
  }
#endif

  return status;
}

EmberStatus emberAfSendImmediateDefaultResponse(EmberAfStatus status)
{
  return emberAfSendDefaultResponse(emberAfCurrentCommand(), status);
}

EmberStatus emberAfSendDefaultResponse(const EmberAfClusterCommand *cmd,
                                       EmberAfStatus status)
{
  int8u frameControl;

  // Default Response commands are only sent in response to unicast commands.
  if (cmd->type != EMBER_INCOMING_UNICAST
      && cmd->type != EMBER_INCOMING_UNICAST_REPLY) {
    return EMBER_SUCCESS;
  }

  // If the Disable Default Response sub-field is set, Default Response commands
  // are only sent if there was an error.
  if ((cmd->buffer[0] & ZCL_DISABLE_DEFAULT_RESPONSE_MASK)
      && status == EMBER_ZCL_STATUS_SUCCESS) {
    return EMBER_SUCCESS;
  }

  // Default Response commands are never sent in response to other Default
  // Response commands.
  if (!cmd->clusterSpecific
      && cmd->commandId == ZCL_DEFAULT_RESPONSE_COMMAND_ID) {
    return EMBER_SUCCESS;
  }

  appResponseLength = 0;
  frameControl = (ZCL_PROFILE_WIDE_COMMAND
                  | (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER
                     ? ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                     : ZCL_FRAME_CONTROL_CLIENT_TO_SERVER));
  if (!cmd->mfgSpecific) {
    emberAfPutInt8uInResp(frameControl & ~ZCL_MANUFACTURER_SPECIFIC_MASK);
  } else {
    emberAfPutInt8uInResp(frameControl | ZCL_MANUFACTURER_SPECIFIC_MASK);
    emberAfPutInt16uInResp(cmd->mfgCode);
  }
  emberAfPutInt8uInResp(cmd->seqNum);
  emberAfPutInt8uInResp(ZCL_DEFAULT_RESPONSE_COMMAND_ID);
  emberAfPutInt8uInResp(cmd->commandId);
  emberAfPutInt8uInResp(status);

  prepareForResponse(cmd);
  return emberAfSendResponse();
}

boolean emberAfDetermineIfLinkSecurityIsRequired(int8u commandId,
                                                 boolean incoming,
                                                 boolean broadcast,
                                                 EmberAfProfileId profileId,
                                                 EmberAfClusterId clusterId)
{
  (void)afNoSecurityForDefaultResponse; // remove warning if not used

#ifdef EMBER_AF_HAS_SECURITY_PROFILE_SE
  if (emberAfIsCurrentSecurityProfileSmartEnergy()) {
    // If we have turned off all APS security (needed for testing), then just
    // always return false.
    if (emAfApsSecurityOff || afNoSecurityForDefaultResponse) {
      afNoSecurityForDefaultResponse = FALSE;
      return FALSE;
    }

    // NOTE: In general if it is a unicast, and one of the SE clusters, it
    // requires APS encryption.  A few special cases exists that we allow for
    // but those must be explicitly spelled out here.

    // Assume that if the local device is broadcasting, even if it is using one
    // of the SE clusters, this is okay.
    if (!incoming && broadcast) {
      return FALSE;
    }

    // Check against profile IDs that use APS security on these clusters.
    if (profileId != SE_PROFILE_ID && profileId != EMBER_WILDCARD_PROFILE_ID) {
      return FALSE;
    }

    // This list comes from Section 5.4.6 of the SE spec.
    switch (clusterId) {
    case ZCL_TIME_CLUSTER_ID:
    case ZCL_COMMISSIONING_CLUSTER_ID:
    case ZCL_PRICE_CLUSTER_ID:
    case ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID:
    case ZCL_SIMPLE_METERING_CLUSTER_ID:
    case ZCL_MESSAGING_CLUSTER_ID:
    case ZCL_TUNNELING_CLUSTER_ID:
    case ZCL_GENERIC_TUNNEL_CLUSTER_ID:
      return TRUE;
    case ZCL_OTA_BOOTLOAD_CLUSTER_ID:
      if (commandId == ZCL_IMAGE_NOTIFY_COMMAND_ID && broadcast) {
        return FALSE;
      } else {
        return TRUE;
      }
    default:
      break;
    }
  }
#endif //EMBER_AF_HAS_SECURITY_PROFILE_SE

  if (emberAfClusterSecurityCustomCallback(profileId,
                                           clusterId,
                                           incoming,
                                           commandId)) {
    return TRUE;
  }

  return FALSE;
}

int8u emberAfMaximumApsPayloadLength(EmberOutgoingMessageType type,
                                     int16u indexOrDestination,
                                     EmberApsFrame *apsFrame)
{
  EmberNodeId destination = EMBER_UNKNOWN_NODE_ID;
  int max = EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH;

  if (apsFrame->options & EMBER_APS_OPTION_ENCRYPTION) {
    max -= EMBER_AF_APS_ENCRYPTION_OVERHEAD;
  }
  if (apsFrame->options & EMBER_APS_OPTION_SOURCE_EUI64) {
    max -= EUI64_SIZE;
  }
  if (apsFrame->options & EMBER_APS_OPTION_DESTINATION_EUI64) {
    max -= EUI64_SIZE;
  }
  if (apsFrame->options & EMBER_APS_OPTION_FRAGMENT) {
    max -= EMBER_AF_APS_FRAGMENTATION_OVERHEAD;
  }

  switch (type) {
  case EMBER_OUTGOING_DIRECT:
    destination = indexOrDestination;
    break;
  case EMBER_OUTGOING_VIA_ADDRESS_TABLE:
    destination = emberGetAddressTableRemoteNodeId(indexOrDestination);
    break;
  case EMBER_OUTGOING_VIA_BINDING:
    destination = emberGetBindingRemoteNodeId(indexOrDestination);
    break;
  case EMBER_OUTGOING_MULTICAST:
    // APS multicast messages include the two-byte group id and exclude the
    // one-byte destination endpoint, for a net loss of an extra byte.
    max--;
    break;
  case EMBER_OUTGOING_BROADCAST:
    break;
  }

#if !defined(ZA_NO_SOURCE_ROUTING) && !defined(EMBER_AF_PLUGIN_CONCENTRATOR_NCP_SUPPORT)
  if (destination != EMBER_UNKNOWN_NODE_ID) {
    int8u index = sourceRouteFindIndex(destination);
    if (index != NULL_INDEX) {
      max -= EMBER_AF_NWK_SOURCE_ROUTE_OVERHEAD;
      while (sourceRouteTable[index].closerIndex != NULL_INDEX) {
        index = sourceRouteTable[index].closerIndex;
        max -= EMBER_AF_NWK_SOURCE_ROUTE_PER_RELAY_ADDRESS_OVERHEAD;
      }
    }
  }
#else
  (void)destination; // remove warning if not used.

#endif //ZA_NO_SOURCE_ROUTING && EMBER_AF_PLUGIN_CONCENTRATOR_NCP_SUPPORT

  return max;
}

void emberAfCopyInt16u(int8u *data, int16u index, int16u x) {
  data[index]   = (int8u) ( ((x)    ) & 0xFF);      
  data[index+1] = (int8u) ( ((x)>> 8) & 0xFF);
}

void emberAfCopyInt24u(int8u *data, int16u index, int32u x) {
  data[index]   = (int8u) ( ((x)    ) & 0xFF );
  data[index+1] = (int8u) ( ((x)>> 8) & 0xFF );
  data[index+2] = (int8u) ( ((x)>>16) & 0xFF );
}

void emberAfCopyInt32u(int8u *data, int16u index, int32u x) {
  data[index]   = (int8u) ( ((x)    ) & 0xFF );    
  data[index+1] = (int8u) ( ((x)>> 8) & 0xFF );  
  data[index+2] = (int8u) ( ((x)>>16) & 0xFF );  
  data[index+3] = (int8u) ( ((x)>>24) & 0xFF );
}

void emberAfCopyString(int8u *dest, int8u *src, int8u size)
{
  if ( src == NULL ) {
    dest[0] = 0; // Zero out the length of string
  } else {
    int8u length = emberAfStringLength(src);
    if (size < length) {
      length = size;
    }
    MEMCOPY(dest + 1, src + 1, length);
    dest[0] = length;
  }
}

void emberAfCopyLongString(int8u *dest, int8u *src, int16u size)
{
  if ( src == NULL ) {
    dest[0] = dest[1] = 0; // Zero out the length of string
  } else {
    int16u length = emberAfLongStringLength(src);
    if (size < length) {
      length = size;
    }
    MEMCOPY(dest + 2, src + 2, length);
    dest[0] = LOW_BYTE(length);
    dest[1] = HIGH_BYTE(length);
  }
}

#if (BIGENDIAN_CPU)
  #define EM_BIG_ENDIAN TRUE
#else
  #define EM_BIG_ENDIAN FALSE
#endif

// You can pass in val1 as NULL, which will assume that it is
// pointing to an array of all zeroes. This is used so that
// default value of NULL is treated as all zeroes.
int8s emberAfCompareValues(int8u* val1, int8u* val2, int8u len) 
{
  int8u i, j, k;

  for (i = 0; i < len; i++) {
    j = ( val1 == NULL 
          ? 0 
          : (EM_BIG_ENDIAN ? val1[i] : val1[(len-1)-i])
        );
    k = (EM_BIG_ENDIAN
         ? val2[i]
         : val2[(len-1)-i]);

    if (j > k) {
      return 1;
    } else if (k > j) {
      return -1;
    }
  }
  return 0;
}

// returns the type that the attribute is, either EMBER_AF_DATA_TYPE_ANALOG,
// EMBER_AF_DATA_TYPE_DISCRETE, or EMBER_AF_DATA_TYPE_NONE. This is based on table
// 2.15 from the ZCL spec 075123r02
int8u emberAfGetAttributeAnalogOrDiscreteType(int8u dataType)
{
  int8u index = 0;

  while ( emberAfAnalogDiscreteThresholds[index] < dataType ) {
    index += 2;
  }
  return emberAfAnalogDiscreteThresholds[index+1];
}

// Zigbee spec says types between signed 8 bit and signed 64 bit
boolean emberAfIsTypeSigned(int8u dataType) {
  return (dataType >= ZCL_INT8S_ATTRIBUTE_TYPE &&
    dataType <= ZCL_INT64S_ATTRIBUTE_TYPE);
}

int32u emberAfGetCurrentTime(void)
{
#ifdef EMBER_AF_PLUGIN_TIME_SERVER
  return emAfTimeClusterServerGetCurrentTime();
#else
  return emberAfGetCurrentTimeCallback();
#endif
}

void emberAfEndpointEventControlSetInactive(EmberEventControl *controls, int8u endpoint)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  emberEventControlSetInactive(*control);
}

boolean emberAfEndpointEventControlGetActive(EmberEventControl *controls, int8u endpoint)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  return emberEventControlGetActive(*control);
}

void emberAfEndpointEventControlSetActive(EmberEventControl *controls, int8u endpoint)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  emberEventControlSetActive(*control);
}

EmberStatus emberAfEndpointEventControlSetDelay(EmberEventControl *controls, int8u endpoint, int32u timeMs)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  return emberAfEventControlSetDelay(control, timeMs);
}

void emberAfEndpointEventControlSetDelayMS(EmberEventControl *controls, int8u endpoint, int16u delay)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  emberEventControlSetDelayMS(*control, delay);
}

void emberAfEndpointEventControlSetDelayQS(EmberEventControl *controls, int8u endpoint, int16u delay)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  emberEventControlSetDelayQS(*control, delay);
}

void emberAfEndpointEventControlSetDelayMinutes(EmberEventControl *controls, int8u endpoint, int16u delay)
{
  EmberEventControl *control = controls + emberAfIndexFromEndpoint(endpoint);
  emberEventControlSetDelayMinutes(*control, delay);
}

// *******************************************************
// emberAfPrintTime and emberAfSetTime are convienience methods for setting
// and displaying human readable times.

// Expects to be passed a ZigBee time which is the number of seconds
// since the year 2000
void emberAfSetTime(int32u utcTime) {
#ifdef EMBER_AF_PLUGIN_TIME_SERVER
  emAfTimeClusterServerSetCurrentTime(utcTime);
#endif //EMBER_AF_PLUGIN_TIME_SERVER
  emberAfSetTimeCallback(utcTime);
}

// emberAfPrintTime expects to be passed a ZigBee time which is the number
// of seconds since the year 2000, it prints out a human readable time
// from that value.
void emberAfPrintTime(int32u utcTime)
{
#ifdef EMBER_AF_PRINT_ENABLE
  int32u i;
  int32u daysSince2000 = utcTime / SECONDS_IN_DAY;
  int32u secToday = utcTime - (daysSince2000 * SECONDS_IN_DAY);
  int8u hr = (int8u)(secToday / SECONDS_IN_HOUR);
  int8u min = (int8u) ((secToday - (hr * SECONDS_IN_HOUR)) / 60);
  int8u sec = (int8u) (secToday - ((hr * SECONDS_IN_HOUR) + (min * 60)));
  int16u year = 2000;
  int8u month = 1;
  int8u day = 1;
  boolean isLeapYear = TRUE; // 2000 was a leap year
  
  // march through the calendar, counting months, days and years
  // being careful to account for leapyears.
  for (i = 0; i < daysSince2000; i++) {
    int8u daysInMonth;
    if (isLeapYear && (month == 2)) {
      daysInMonth = 29;
    } else {
      daysInMonth = emberAfDaysInMonth[month - 1];
    }
    if (daysInMonth == day) {
      month++;
      day = 1;
    } else {
      day++;
    }
    if (month == 13) {
      year++;
      month = 1;
      if (year % 4 == 0 && 
          (year % 100 != 0 || 
           (year % 400 == 0)))
        isLeapYear = TRUE;
      else
        isLeapYear = FALSE;
    }
  }
  emberAfPrintln(emberAfPrintActiveArea,
                 "UTC time: %d/%d/%d %d:%d:%d (%4x)",
                 month,
                 day,
                 year,
                 hr,
                 min,
                 sec,
                 utcTime);
#endif //EMBER_AF_PRINT_ENABLE
}

#if EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE == 0
int8u emberAfAddAddressTableEntry(EmberEUI64 longId, EmberNodeId shortId)
{
  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

EmberStatus emberAfSetAddressTableEntry(int8u index,
                                        EmberEUI64 longId,
                                        EmberNodeId shortId)
{
  return EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE;
}

EmberStatus emberAfRemoveAddressTableEntry(int8u index)
{
  return EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE;
}

#else

int8u emberAfAddAddressTableEntry(EmberEUI64 longId, EmberNodeId shortId)
{
  int8u i, index = EMBER_NULL_ADDRESS_TABLE_INDEX;
  for (i = 0; i < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; i++) {
    if (emberGetAddressTableRemoteNodeId(i)
        != EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      EmberEUI64 eui64;
      emberGetAddressTableRemoteEui64(i, eui64);
      if (MEMCOMPARE(longId, eui64, EUI64_SIZE) == 0) {
        index = i;
        goto kickout;
      }
    } else if (index == EMBER_NULL_ADDRESS_TABLE_INDEX) {
      index = i;
    }
  }
  if (index != EMBER_NULL_ADDRESS_TABLE_INDEX) {
    emberSetAddressTableRemoteEui64(index, longId);
kickout:
    if (shortId != EMBER_UNKNOWN_NODE_ID) {
      emberSetAddressTableRemoteNodeId(index, shortId);
    }
    addressTableReferenceCounts[index]++;
  }
  return index;
}

EmberStatus emberAfSetAddressTableEntry(int8u index,
                                        EmberEUI64 longId,
                                        EmberNodeId shortId)
{
  EmberStatus status = EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE;
  if (index < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE) {
    if (addressTableReferenceCounts[index] == 0) {
      status = emberSetAddressTableRemoteEui64(index, longId);
      if (status == EMBER_SUCCESS && shortId != EMBER_UNKNOWN_NODE_ID) {
        emberSetAddressTableRemoteNodeId(index, shortId);
      }
      addressTableReferenceCounts[index] = 1;
    } else {
      status = EMBER_ADDRESS_TABLE_ENTRY_IS_ACTIVE;
    }
  }
  return status;
}

EmberStatus emberAfRemoveAddressTableEntry(int8u index)
{
  EmberStatus status = EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE;
  if (index < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE) {
    if (0 < addressTableReferenceCounts[index]) {
      addressTableReferenceCounts[index]--;
    }
    if (addressTableReferenceCounts[index] == 0) {
      emberSetAddressTableRemoteNodeId(index,
                                       EMBER_TABLE_ENTRY_UNUSED_NODE_ID);
    }
    status = EMBER_SUCCESS;
  }
  return status;
}
#endif
