// *****************************************************************************
// * service-discovery-common.c
// *
// * Service discovery code that is common to different types of service
// * discovery, e.g. match descriptor, NWK address lookup, and IEEE address
// * lookup.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/zigbee-framework/zigbee-device-common.h"
#ifdef EZSP_HOST
  #include "app/util/zigbee-framework/zigbee-device-host.h"
#endif
#include "service-discovery.h"

#if EMBER_SUPPORTED_NETWORKS > 4
  #error "Service discovery is limited to four networks."
#endif

//==============================================================================
// Service discovery state machine
//
//   This code handles initiating a limited set of ZDO, receiving
//   the response and sending it back to the cluster or code element that
//   requested it.  Unfortunately the ZDO message does not have any distinct
//   identifiers that would allow us to determine what cluster/endpoint on our
//   local device initiated the request.  Therefore we can only allow one
//    outstanding request at a time.

EmberEventControl emAfServiceDiscoveryEventControls[EMBER_SUPPORTED_NETWORKS];

typedef struct {
  boolean active;
  EmberAfServiceDiscoveryCallback *callback;
  // This will contain the target type: broadcast or unicast (high bit)
  // and the ZDO cluster ID of the request.  Since ZDO requests
  // clear the high bit (only repsonses use it), we can use that leftover bit
  // for something else.
  int16u requestData;
} State;
static State states[EMBER_SUPPORTED_NETWORKS];

#define UNICAST_QUERY_BIT (0x8000)
#define isUnicastQuery(state) (UNICAST_QUERY_BIT == (state->requestData & UNICAST_QUERY_BIT))
#define setUnicastQuery(state) (state->requestData |= UNICAST_QUERY_BIT)
#define getRequestCluster(state) (state->requestData & ~UNICAST_QUERY_BIT)
#define serviceDiscoveryInProgress(state) (state->active)

#define DISCOVERY_TIMEOUT_QS (2 * 4)

// seq. number (1), status (1), address (2), length (1)
#define MATCH_DESCRIPTOR_OVERHEAD               5
#define MINIMUM_MATCH_DESCRIPTOR_SUCCESS_LENGTH MATCH_DESCRIPTOR_OVERHEAD

// seq. number (1), status (1)
#define ZDO_OVERHEAD 2
// EUI64 (8), node ID (2),
#define MINIMUM_ADDRESS_REQEUST_SUCCESS (ZDO_OVERHEAD + 10)
#define ADDRESS_RESPONSE_NODE_ID_OFFSET (ZDO_OVERHEAD + EUI64_SIZE)

#define PREFIX "Svc Disc: "

//==============================================================================
// Forward Declarations

static void setupDiscoveryData(State *state,
                               EmberNodeId messageDest,
                               EmberAfServiceDiscoveryCallback *callback,
                               int16u zdoClusterId);

//==============================================================================

EmberStatus emberAfFindDevicesByProfileAndCluster(EmberNodeId target,
                                                  EmberAfProfileId profileId,
                                                  EmberAfClusterId clusterId,
                                                  boolean serverCluster,
                                                  EmberAfServiceDiscoveryCallback *callback)
{
  State *state = &states[emberGetCurrentNetwork()];
  EmberStatus status;

  if (serviceDiscoveryInProgress(state)) {
    emberAfServiceDiscoveryPrintln("%pDiscovery already in progress", PREFIX);
    return EMBER_INVALID_CALL;
  }

  if (EMBER_BROADCAST_ADDRESS <= target
      && target != EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS) {
    // Note:  The core spec. only allows a Match Descriptor broadcast to
    // the 'rx on when idle' address.  No other broadcast addresses are allowed.
    // The Ember stack will silently discard broadcast match descriptors
    // to invalid broadcast addresses.
    emberAfServiceDiscoveryPrintln("%pIllegal broadcast address, remapping to valid one.",
                                   PREFIX);
    target = EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS;
  }

  status = emAfSendMatchDescriptor(target, profileId, clusterId, serverCluster);
  if (status != EMBER_SUCCESS) {
    emberAfServiceDiscoveryPrintln("%pFailed to send match discovery: 0x%x",
                                   PREFIX,
                                   status);
    return status;
  }

  emberAfServiceDiscoveryPrintln("%pStarting discovery for cluster 0x%2x",
                                 PREFIX,
                                 clusterId);

  setupDiscoveryData(state, target, callback, MATCH_DESCRIPTORS_REQUEST);

  return EMBER_SUCCESS;
}

EmberStatus emberAfFindClustersByDeviceAndEndpoint(EmberNodeId target,
                                                   int8u targetEndpoint,
                                                   EmberAfServiceDiscoveryCallback *callback) {
  
  State *state = &states[emberGetCurrentNetwork()];
  EmberStatus status;
  
  if (serviceDiscoveryInProgress(state)) {
    return EMBER_INVALID_CALL;
  }

  status = emberSimpleDescriptorRequest(target,
                                        targetEndpoint,
                                        EMBER_AF_DEFAULT_APS_OPTIONS);
  
  if (status != EMBER_SUCCESS) {
    emberAfServiceDiscoveryPrintln("%pFailed to send simple descriptor request: 0x%x",
                                   PREFIX,
                                   status);
    return status;
  }
  
  setupDiscoveryData(state, target, callback, SIMPLE_DESCRIPTOR_REQUEST);
  
  return status;
}

EmberStatus emberAfFindIeeeAddress(EmberNodeId shortAddress,
                                   EmberAfServiceDiscoveryCallback *callback)
{
  State *state = &states[emberGetCurrentNetwork()];
  EmberStatus status;

  if (serviceDiscoveryInProgress(state)) {
    return EMBER_INVALID_CALL;
  }

  status = emberIeeeAddressRequest(shortAddress,
                                   FALSE,         // report kids?
                                   0,             // child start index
                                   EMBER_APS_OPTION_RETRY);

  if (status != EMBER_SUCCESS) {
    emberAfServiceDiscoveryPrintln("%pFailed to send IEEE address request: 0x%x",
                                   PREFIX,
                                   status);
    return status;
  }

  setupDiscoveryData(state, shortAddress, callback, IEEE_ADDRESS_REQUEST);

  return status;
}

EmberStatus emberAfFindNodeId(EmberEUI64 longAddress,
                              EmberAfServiceDiscoveryCallback *callback)
{
  State *state = &states[emberGetCurrentNetwork()];
  EmberStatus status;

  if (serviceDiscoveryInProgress(state)) {
    return EMBER_INVALID_CALL;
  }

  status = emberNetworkAddressRequest(longAddress,
                                      FALSE,         // report kids?
                                      0);            // child start index

  if (status != EMBER_SUCCESS) {
    emberAfServiceDiscoveryPrintln("%pFailed to send NWK address request: 0x%x",
                                   PREFIX,
                                   status);
    return status;
  }

  setupDiscoveryData(state,
                     EMBER_BROADCAST_ADDRESS,
                     callback,
                     NETWORK_ADDRESS_REQUEST);

  return status;
}

static void setupDiscoveryData(State *state,
                               EmberNodeId messageDest,
                               EmberAfServiceDiscoveryCallback *callback,
                               int16u zdoClusterRequest)
{
  state->active = TRUE;
  state->requestData = zdoClusterRequest;
  if (messageDest < EMBER_BROADCAST_ADDRESS) {
    setUnicastQuery(state);
  }
  state->callback = callback;
  emberAfServiceDiscoveryPrintln("%pWaiting %d sec for discovery to complete",
                                 PREFIX,
                                 DISCOVERY_TIMEOUT_QS >> 2);
  emberAfNetworkEventControlSetDelayQS(emAfServiceDiscoveryEventControls,
                                       DISCOVERY_TIMEOUT_QS);

  // keep sleepy end devices out of hibernation until
  // service discovery is complete
  emberAfAddToCurrentAppTasks(EMBER_AF_WAITING_FOR_SERVICE_DISCOVERY);
}

static void serviceDiscoveryComplete(int8u networkIndex)
{
  State *state = &states[networkIndex];

  emberAfPushNetworkIndex(networkIndex);

  state->active = FALSE;
  emberAfServiceDiscoveryPrintln("%pcomplete.", PREFIX);
  emberAfNetworkEventControlSetInactive(emAfServiceDiscoveryEventControls);

  // allow sleepy end devices to go into hibernation now.
  emberAfRemoveFromCurrentAppTasks(EMBER_AF_WAITING_FOR_SERVICE_DISCOVERY);

  if (state->callback != NULL) {
    EmberAfServiceDiscoveryResult result;
    result.status = (isUnicastQuery(state)
                     ? EMBER_AF_UNICAST_SERVICE_DISCOVERY_TIMEOUT
                     : EMBER_AF_BROADCAST_SERVICE_DISCOVERY_COMPLETE);
    result.zdoRequestClusterId = getRequestCluster(state);
    result.matchAddress = EMBER_NULL_NODE_ID;
    result.responseData = NULL;
    (*state->callback)(&result);
  }

  emberAfPopNetworkIndex();
}

void emAfServiceDiscoveryComplete0(void)
{
  serviceDiscoveryComplete(0);
}

void emAfServiceDiscoveryComplete1(void)
{
  serviceDiscoveryComplete(1);
}

void emAfServiceDiscoveryComplete2(void)
{
  serviceDiscoveryComplete(2);
}

void emAfServiceDiscoveryComplete3(void)
{
  serviceDiscoveryComplete(3);
}

static void executeCallback(State *state,
                            const EmberAfServiceDiscoveryResult *result)
{
  (*state->callback)(result);
  if (isUnicastQuery(state)) {
    // If the request was unicast and we received a response then we are done.
    // No need to wait for the timer to expire.

    // We NULL the callback as a way of indicating we already fired it.
    // For timeouts, the callback will not be NULL and still fire.
    state->callback = NULL;
    serviceDiscoveryComplete(emberGetCurrentNetwork());
  }
}

static boolean processMatchDescriptorResponse(State *state,
                                              const int8u *message,
                                              int16u length)
{
  EmberNodeId matchId;
  int8u listLength;

  if (length < MINIMUM_MATCH_DESCRIPTOR_SUCCESS_LENGTH) {
    emberAfServiceDiscoveryPrintln("%pMessage too short", PREFIX);
    return TRUE;
  }

  // This will now be used as the length of the match list.
  length -= MATCH_DESCRIPTOR_OVERHEAD;

  // If the parent of a sleepy device supports caching its descriptor
  // information then the sender of the response may not be the device
  // that actually matches the request.  The device id that matches
  // is included in the message.
  matchId = message[2] + (message[3] << 8);
  listLength = message[4];

  if (listLength != length) {
    emberAfServiceDiscoveryPrintln("%pMessage too short for num. endpoints",
                                   PREFIX);
    return TRUE;
  }

  emberAfServiceDiscoveryPrintln("%pMatch%p found from 0x%2x.",
                                 PREFIX,
                                 (listLength > 0
                                  ? ""
                                  : " NOT"),
                                 matchId);

  // If we got an active response with an empty list then ignore it.
  if (listLength != 0) {
    EmberAfServiceDiscoveryResult result;
    EmberAfEndpointList endpointList;
    endpointList.count = length;
    endpointList.list = &(message[MATCH_DESCRIPTOR_OVERHEAD]);
    result.status = (isUnicastQuery(state)
                     ? EMBER_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE
                     : EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED);
    result.zdoRequestClusterId = getRequestCluster(state);
    result.matchAddress = matchId;
    result.responseData = &endpointList;
    executeCallback(state, &result);
  }
  return TRUE;
}

static boolean processSimpleDescriptorResponse(State *state,
                                               const int8u *message,
                                               int16u length) {
 EmberAfServiceDiscoveryResult result;
 EmberAfClusterList clusterList;
 EmberNodeId matchId = message[2] + (message[3] << 8);
 int8u inClusterCount = message[11];
 int8u outClusterCount = message[12+(inClusterCount*2)];
 
 clusterList.inClusterCount = inClusterCount;
 clusterList.outClusterCount = outClusterCount;
 clusterList.inClusterList = (int16u*)&message[12];
 clusterList.outClusterList = (int16u*)&message[13+(inClusterCount*2)];
 
 result.status = (isUnicastQuery(state)
                  ? EMBER_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE
                  : EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED);
 result.matchAddress = matchId;
 result.zdoRequestClusterId = getRequestCluster(state);
 result.responseData = &clusterList;

 executeCallback(state, &result);
 return TRUE;
}

// Both NWK and IEEE responses have the same exact format.
static boolean processAddressResponse(State *state,
                                      const int8u *message,
                                      int16u length)
{
  EmberAfServiceDiscoveryResult result;
  EmberEUI64 eui64LittleEndian;

  if (length < MINIMUM_ADDRESS_REQEUST_SUCCESS) {
    emberAfServiceDiscoveryPrintln("%pMessage too short", PREFIX);
    return TRUE;
  }
  MEMCOPY(eui64LittleEndian, message + ZDO_OVERHEAD, EUI64_SIZE);
  result.status = (isUnicastQuery(state)
                   ? EMBER_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE
                   : EMBER_AF_BROADCAST_SERVICE_DISCOVERY_RESPONSE_RECEIVED);
  result.matchAddress = (message[ADDRESS_RESPONSE_NODE_ID_OFFSET]
                         + (message[ADDRESS_RESPONSE_NODE_ID_OFFSET+1] << 8));
  result.zdoRequestClusterId = getRequestCluster(state);
  result.responseData = eui64LittleEndian;

  executeCallback(state, &result);
  return TRUE;
}

boolean emAfServiceDiscoveryIncoming(EmberNodeId sender,
                                     EmberApsFrame *apsFrame,
                                     const int8u *message,
                                     int16u length)
{
  State *state = &states[emberGetCurrentNetwork()];
  if (!(serviceDiscoveryInProgress(state)
        && (apsFrame->profileId == EMBER_ZDO_PROFILE_ID
            // ZDO Responses set the high bit on the request cluster ID
            && (apsFrame->clusterId == (CLUSTER_ID_RESPONSE_MINIMUM
                                        | getRequestCluster(state)))))) {
    return FALSE;
  }

  // If we hear our own request and respond we ignore it.
  if (sender == emberAfGetNodeId()) {
    return TRUE;
  }

  // The second byte is the status code
  if (message[1] != EMBER_ZDP_SUCCESS) {
    return TRUE;
  }

  switch (apsFrame->clusterId) {
  case SIMPLE_DESCRIPTOR_RESPONSE:
    return processSimpleDescriptorResponse(state, message, length);
  case MATCH_DESCRIPTORS_RESPONSE:
    return processMatchDescriptorResponse(state, message, length);

  case NETWORK_ADDRESS_RESPONSE:
  case IEEE_ADDRESS_RESPONSE:
    return processAddressResponse(state, message, length);

  default:
    // Some ZDO request we don't care about.
    break;
  }

  return FALSE;
}
