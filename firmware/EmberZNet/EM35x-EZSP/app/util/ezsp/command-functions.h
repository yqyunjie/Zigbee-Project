// File: command-functions.h
// 
// *** Generated file. Do not edit! ***
// 
// Description: Functions for sending every EM260 frame and returning the result
// to the Host.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*


//------------------------------------------------------------------------------
// Configuration Frames
//------------------------------------------------------------------------------

int8u ezspVersion(
      int8u desiredProtocolVersion,
      int8u *stackType,
      int16u *stackVersion)
{
  int8u protocolVersion;
  startCommand(EZSP_VERSION);
  appendInt8u(desiredProtocolVersion);
  sendCommand();
  protocolVersion = fetchInt8u();
  *stackType = fetchInt8u();
  *stackVersion = fetchInt16u();
  return protocolVersion;
}

EzspStatus ezspGetConfigurationValue(
      EzspConfigId configId,
      int16u *value)
{
  int8u status;
  startCommand(EZSP_GET_CONFIGURATION_VALUE);
  appendInt8u(configId);
  sendCommand();
  status = fetchInt8u();
  *value = fetchInt16u();
  return status;
}

EzspStatus ezspSetConfigurationValue(
      EzspConfigId configId,
      int16u value)
{
  int8u status;
  startCommand(EZSP_SET_CONFIGURATION_VALUE);
  appendInt8u(configId);
  appendInt16u(value);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EzspStatus ezspAddEndpoint(
      int8u endpoint,
      int16u profileId,
      int16u deviceId,
      int8u deviceVersion,
      int8u inputClusterCount,
      int8u outputClusterCount,
      int16u *inputClusterList,
      int16u *outputClusterList)
{
  int8u status;
  startCommand(EZSP_ADD_ENDPOINT);
  appendInt8u(endpoint);
  appendInt16u(profileId);
  appendInt16u(deviceId);
  appendInt8u(deviceVersion);
  appendInt8u(inputClusterCount);
  appendInt8u(outputClusterCount);
  appendInt16uArray(inputClusterCount, inputClusterList);
  appendInt16uArray(outputClusterCount, outputClusterList);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EzspStatus ezspSetPolicy(
      EzspPolicyId policyId,
      EzspDecisionId decisionId)
{
  int8u status;
  startCommand(EZSP_SET_POLICY);
  appendInt8u(policyId);
  appendInt8u(decisionId);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EzspStatus ezspGetPolicy(
      EzspPolicyId policyId,
      EzspDecisionId *decisionId)
{
  int8u status;
  startCommand(EZSP_GET_POLICY);
  appendInt8u(policyId);
  sendCommand();
  status = fetchInt8u();
  *decisionId = fetchInt8u();
  return status;
}

EzspStatus ezspGetValue(
      EzspValueId valueId,
      int8u *valueLength,
      int8u *value)
{
  int8u status;
  startCommand(EZSP_GET_VALUE);
  appendInt8u(valueId);
  sendCommand();
  status = fetchInt8u();
  *valueLength = fetchInt8u();
  fetchInt8uArray(*valueLength, value);
  return status;
}

EzspStatus ezspGetExtendedValue(
      EzspExtendedValueId valueId,
      int32u characteristics,
      int8u *valueLength,
      int8u *value)
{
  int8u status;
  startCommand(EZSP_GET_EXTENDED_VALUE);
  appendInt8u(valueId);
  appendInt32u(characteristics);
  sendCommand();
  status = fetchInt8u();
  *valueLength = fetchInt8u();
  fetchInt8uArray(*valueLength, value);
  return status;
}

EzspStatus ezspSetValue(
      EzspValueId valueId,
      int8u valueLength,
      int8u *value)
{
  int8u status;
  startCommand(EZSP_SET_VALUE);
  appendInt8u(valueId);
  appendInt8u(valueLength);
  appendInt8uArray(valueLength, value);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EzspStatus ezspSetGpioCurrentConfiguration(
      int8u portPin,
      int8u cfg,
      int8u out)
{
  int8u status;
  startCommand(EZSP_SET_GPIO_CURRENT_CONFIGURATION);
  appendInt8u(portPin);
  appendInt8u(cfg);
  appendInt8u(out);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EzspStatus ezspSetGpioPowerUpDownConfiguration(
      int8u portPin,
      int8u puCfg,
      int8u puOut,
      int8u pdCfg,
      int8u pdOut)
{
  int8u status;
  startCommand(EZSP_SET_GPIO_POWER_UP_DOWN_CONFIGURATION);
  appendInt8u(portPin);
  appendInt8u(puCfg);
  appendInt8u(puOut);
  appendInt8u(pdCfg);
  appendInt8u(pdOut);
  sendCommand();
  status = fetchInt8u();
  return status;
}

void ezspSetGpioRadioPowerMask(
      int32u mask)
{
  startCommand(EZSP_SET_GPIO_RADIO_POWER_MASK);
  appendInt32u(mask);
  sendCommand();
}

//------------------------------------------------------------------------------
// Utilities Frames
//------------------------------------------------------------------------------

void ezspNop(void)
{
  startCommand(EZSP_NOP);
  sendCommand();
}

int8u ezspEcho(
      int8u dataLength,
      int8u *data,
      int8u *echo)
{
  int8u echoLength;
  startCommand(EZSP_ECHO);
  appendInt8u(dataLength);
  appendInt8uArray(dataLength, data);
  sendCommand();
  echoLength = fetchInt8u();
  fetchInt8uArray(echoLength, echo);
  return echoLength;
}

void ezspCallback(void)
{
  startCommand(EZSP_CALLBACK);
  sendCommand();
  callbackDispatch();
}

EmberStatus ezspSetToken(
      int8u tokenId,
      int8u *tokenData)
{
  int8u status;
  startCommand(EZSP_SET_TOKEN);
  appendInt8u(tokenId);
  appendInt8uArray(8, tokenData);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspGetToken(
      int8u tokenId,
      int8u *tokenData)
{
  int8u status;
  startCommand(EZSP_GET_TOKEN);
  appendInt8u(tokenId);
  sendCommand();
  status = fetchInt8u();
  fetchInt8uArray(8, tokenData);
  return status;
}

int8u ezspGetMfgToken(
      EzspMfgTokenId tokenId,
      int8u *tokenData)
{
  int8u tokenDataLength;
  startCommand(EZSP_GET_MFG_TOKEN);
  appendInt8u(tokenId);
  sendCommand();
  tokenDataLength = fetchInt8u();
  fetchInt8uArray(tokenDataLength, tokenData);
  return tokenDataLength;
}

EmberStatus ezspGetRandomNumber(
      int16u *value)
{
  int8u status;
  startCommand(EZSP_GET_RANDOM_NUMBER);
  sendCommand();
  status = fetchInt8u();
  *value = fetchInt16u();
  return status;
}

EmberStatus ezspSetTimer(
      int8u timerId,
      int16u time,
      EmberEventUnits units,
      boolean repeat)
{
  int8u status;
  startCommand(EZSP_SET_TIMER);
  appendInt8u(timerId);
  appendInt16u(time);
  appendInt8u(units);
  appendInt8u(repeat);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int16u ezspGetTimer(
      int8u timerId,
      EmberEventUnits *units,
      boolean *repeat)
{
  int16u time;
  startCommand(EZSP_GET_TIMER);
  appendInt8u(timerId);
  sendCommand();
  time = fetchInt16u();
  *units = fetchInt8u();
  *repeat = fetchInt8u();
  return time;
}

EmberStatus ezspDebugWrite(
      boolean binaryMessage,
      int8u messageLength,
      int8u *messageContents)
{
  int8u status;
  startCommand(EZSP_DEBUG_WRITE);
  appendInt8u(binaryMessage);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

void ezspReadAndClearCounters(
      int16u *values)
{
  startCommand(EZSP_READ_AND_CLEAR_COUNTERS);
  sendCommand();
  fetchInt16uArray(EMBER_COUNTER_TYPE_COUNT, values);
}

void ezspDelayTest(
      int16u delay)
{
  startCommand(EZSP_DELAY_TEST);
  appendInt16u(delay);
  sendCommand();
}

EmberLibraryStatus emberGetLibraryStatus(
      int8u libraryId)
{
  int8u status;
  startCommand(EZSP_GET_LIBRARY_STATUS);
  appendInt8u(libraryId);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspGetXncpInfo(
      int16u *manufacturerId,
      int16u *versionNumber)
{
  int8u status;
  startCommand(EZSP_GET_XNCP_INFO);
  sendCommand();
  status = fetchInt8u();
  *manufacturerId = fetchInt16u();
  *versionNumber = fetchInt16u();
  return status;
}

EmberStatus ezspCustomFrame(
      int8u payloadLength,
      int8u *payload,
      int8u *replyLength,
      int8u *reply)
{
  int8u status;
  startCommand(EZSP_CUSTOM_FRAME);
  appendInt8u(payloadLength);
  appendInt8uArray(payloadLength, payload);
  sendCommand();
  status = fetchInt8u();
  *replyLength = fetchInt8u();
  fetchInt8uArray(*replyLength, reply);
  return status;
}

//------------------------------------------------------------------------------
// Networking Frames
//------------------------------------------------------------------------------

void emberSetManufacturerCode(
      int16u code)
{
  startCommand(EZSP_SET_MANUFACTURER_CODE);
  appendInt16u(code);
  sendCommand();
}

void emberSetPowerDescriptor(
      int16u descriptor)
{
  startCommand(EZSP_SET_POWER_DESCRIPTOR);
  appendInt16u(descriptor);
  sendCommand();
}

EmberStatus emberNetworkInit(void)
{
  int8u status;
  startCommand(EZSP_NETWORK_INIT);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberNetworkInitExtended(
      EmberNetworkInitStruct *networkInitStruct)
{
  int8u status;
  startCommand(EZSP_NETWORK_INIT_EXTENDED);
  appendEmberNetworkInitStruct(networkInitStruct);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberNetworkStatus emberNetworkState(void)
{
  int8u status;
  startCommand(EZSP_NETWORK_STATE);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberStartScan(
      EzspNetworkScanType scanType,
      int32u channelMask,
      int8u duration)
{
  int8u status;
  startCommand(EZSP_START_SCAN);
  appendInt8u(scanType);
  appendInt32u(channelMask);
  appendInt8u(duration);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberStopScan(void)
{
  int8u status;
  startCommand(EZSP_STOP_SCAN);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberFormNetwork(
      EmberNetworkParameters *parameters)
{
  int8u status;
  startCommand(EZSP_FORM_NETWORK);
  appendEmberNetworkParameters(parameters);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberJoinNetwork(
      EmberNodeType nodeType,
      EmberNetworkParameters *parameters)
{
  int8u status;
  startCommand(EZSP_JOIN_NETWORK);
  appendInt8u(nodeType);
  appendEmberNetworkParameters(parameters);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberLeaveNetwork(void)
{
  int8u status;
  startCommand(EZSP_LEAVE_NETWORK);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspFindAndRejoinNetwork(
      boolean haveCurrentNetworkKey,
      int32u channelMask)
{
  int8u status;
  startCommand(EZSP_FIND_AND_REJOIN_NETWORK);
  appendInt8u(haveCurrentNetworkKey);
  appendInt32u(channelMask);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberPermitJoining(
      int8u duration)
{
  int8u status;
  startCommand(EZSP_PERMIT_JOINING);
  appendInt8u(duration);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberEnergyScanRequest(
      EmberNodeId target,
      int32u scanChannels,
      int8u scanDuration,
      int16u scanCount)
{
  int8u status;
  startCommand(EZSP_ENERGY_SCAN_REQUEST);
  appendInt16u(target);
  appendInt32u(scanChannels);
  appendInt8u(scanDuration);
  appendInt16u(scanCount);
  sendCommand();
  status = fetchInt8u();
  return status;
}

void ezspGetEui64(
      EmberEUI64 eui64)
{
  startCommand(EZSP_GET_EUI64);
  sendCommand();
  fetchInt8uArray(8, eui64);
}

EmberNodeId emberGetNodeId(void)
{
  int16u nodeId;
  startCommand(EZSP_GET_NODE_ID);
  sendCommand();
  nodeId = fetchInt16u();
  return nodeId;
}

EmberStatus ezspGetNetworkParameters(
      EmberNodeType *nodeType,
      EmberNetworkParameters *parameters)
{
  int8u status;
  startCommand(EZSP_GET_NETWORK_PARAMETERS);
  sendCommand();
  status = fetchInt8u();
  *nodeType = fetchInt8u();
  fetchEmberNetworkParameters(parameters);
  return status;
}

int8u ezspGetParentChildParameters(
      EmberEUI64 parentEui64,
      EmberNodeId *parentNodeId)
{
  int8u childCount;
  startCommand(EZSP_GET_PARENT_CHILD_PARAMETERS);
  sendCommand();
  childCount = fetchInt8u();
  fetchInt8uArray(8, parentEui64);
  *parentNodeId = fetchInt16u();
  return childCount;
}

EmberStatus ezspGetChildData(
      int8u index,
      EmberNodeId *childId,
      EmberEUI64 childEui64,
      EmberNodeType *childType)
{
  int8u status;
  startCommand(EZSP_GET_CHILD_DATA);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  *childId = fetchInt16u();
  fetchInt8uArray(8, childEui64);
  *childType = fetchInt8u();
  return status;
}

EmberStatus emberGetNeighbor(
      int8u index,
      EmberNeighborTableEntry *value)
{
  int8u status;
  startCommand(EZSP_GET_NEIGHBOR);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  fetchEmberNeighborTableEntry(value);
  return status;
}

int8u emberNeighborCount(void)
{
  int8u value;
  startCommand(EZSP_NEIGHBOR_COUNT);
  sendCommand();
  value = fetchInt8u();
  return value;
}

EmberStatus emberGetRouteTableEntry(
      int8u index,
      EmberRouteTableEntry *value)
{
  int8u status;
  startCommand(EZSP_GET_ROUTE_TABLE_ENTRY);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  fetchEmberRouteTableEntry(value);
  return status;
}

EmberStatus emberSetRadioPower(
      int8s power)
{
  int8u status;
  startCommand(EZSP_SET_RADIO_POWER);
  appendInt8u(power);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberSetRadioChannel(
      int8u channel)
{
  int8u status;
  startCommand(EZSP_SET_RADIO_CHANNEL);
  appendInt8u(channel);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspSetConcentrator(
      boolean on,
      int16u concentratorType,
      int16u minTime,
      int16u maxTime,
      int8u routeErrorThreshold,
      int8u deliveryFailureThreshold,
      int8u maxHops)
{
  int8u status;
  startCommand(EZSP_SET_CONCENTRATOR);
  appendInt8u(on);
  appendInt16u(concentratorType);
  appendInt16u(minTime);
  appendInt16u(maxTime);
  appendInt8u(routeErrorThreshold);
  appendInt8u(deliveryFailureThreshold);
  appendInt8u(maxHops);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// Binding Frames
//------------------------------------------------------------------------------

EmberStatus emberClearBindingTable(void)
{
  int8u status;
  startCommand(EZSP_CLEAR_BINDING_TABLE);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberSetBinding(
      int8u index,
      EmberBindingTableEntry *value)
{
  int8u status;
  startCommand(EZSP_SET_BINDING);
  appendInt8u(index);
  appendEmberBindingTableEntry(value);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberGetBinding(
      int8u index,
      EmberBindingTableEntry *value)
{
  int8u status;
  startCommand(EZSP_GET_BINDING);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  fetchEmberBindingTableEntry(value);
  return status;
}

EmberStatus emberDeleteBinding(
      int8u index)
{
  int8u status;
  startCommand(EZSP_DELETE_BINDING);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  return status;
}

boolean emberBindingIsActive(
      int8u index)
{
  int8u active;
  startCommand(EZSP_BINDING_IS_ACTIVE);
  appendInt8u(index);
  sendCommand();
  active = fetchInt8u();
  return active;
}

EmberNodeId emberGetBindingRemoteNodeId(
      int8u index)
{
  int16u nodeId;
  startCommand(EZSP_GET_BINDING_REMOTE_NODE_ID);
  appendInt8u(index);
  sendCommand();
  nodeId = fetchInt16u();
  return nodeId;
}

void emberSetBindingRemoteNodeId(
      int8u index,
      EmberNodeId nodeId)
{
  startCommand(EZSP_SET_BINDING_REMOTE_NODE_ID);
  appendInt8u(index);
  appendInt16u(nodeId);
  sendCommand();
}

//------------------------------------------------------------------------------
// Messaging Frames
//------------------------------------------------------------------------------

int8u ezspMaximumPayloadLength(void)
{
  int8u apsLength;
  startCommand(EZSP_MAXIMUM_PAYLOAD_LENGTH);
  sendCommand();
  apsLength = fetchInt8u();
  return apsLength;
}

EmberStatus ezspSendUnicast(
      EmberOutgoingMessageType type,
      EmberNodeId indexOrDestination,
      EmberApsFrame *apsFrame,
      int8u messageTag,
      int8u messageLength,
      int8u *messageContents,
      int8u *sequence)
{
  int8u status;
  startCommand(EZSP_SEND_UNICAST);
  appendInt8u(type);
  appendInt16u(indexOrDestination);
  appendEmberApsFrame(apsFrame);
  appendInt8u(messageTag);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  *sequence = fetchInt8u();
  return status;
}

EmberStatus ezspSendBroadcast(
      EmberNodeId destination,
      EmberApsFrame *apsFrame,
      int8u radius,
      int8u messageTag,
      int8u messageLength,
      int8u *messageContents,
      int8u *sequence)
{
  int8u status;
  startCommand(EZSP_SEND_BROADCAST);
  appendInt16u(destination);
  appendEmberApsFrame(apsFrame);
  appendInt8u(radius);
  appendInt8u(messageTag);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  *sequence = fetchInt8u();
  return status;
}

EmberStatus ezspProxyBroadcast(
      EmberNodeId source,
      EmberNodeId destination,
      int8u nwkSequence,
      EmberApsFrame *apsFrame,
      int8u radius,
      int8u messageTag,
      int8u messageLength,
      int8u *messageContents,
      int8u *apsSequence)
{
  int8u status;
  startCommand(EZSP_PROXY_BROADCAST);
  appendInt16u(source);
  appendInt16u(destination);
  appendInt8u(nwkSequence);
  appendEmberApsFrame(apsFrame);
  appendInt8u(radius);
  appendInt8u(messageTag);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  *apsSequence = fetchInt8u();
  return status;
}

EmberStatus ezspSendMulticast(
      EmberApsFrame *apsFrame,
      int8u hops,
      int8u nonmemberRadius,
      int8u messageTag,
      int8u messageLength,
      int8u *messageContents,
      int8u *sequence)
{
  int8u status;
  startCommand(EZSP_SEND_MULTICAST);
  appendEmberApsFrame(apsFrame);
  appendInt8u(hops);
  appendInt8u(nonmemberRadius);
  appendInt8u(messageTag);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  *sequence = fetchInt8u();
  return status;
}

EmberStatus ezspSendReply(
      EmberNodeId sender,
      EmberApsFrame *apsFrame,
      int8u messageLength,
      int8u *messageContents)
{
  int8u status;
  startCommand(EZSP_SEND_REPLY);
  appendInt16u(sender);
  appendEmberApsFrame(apsFrame);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberSendManyToOneRouteRequest(
      int16u concentratorType,
      int8u radius)
{
  int8u status;
  startCommand(EZSP_SEND_MANY_TO_ONE_ROUTE_REQUEST);
  appendInt16u(concentratorType);
  appendInt8u(radius);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspPollForData(
      int16u interval,
      EmberEventUnits units,
      int8u failureLimit)
{
  int8u status;
  startCommand(EZSP_POLL_FOR_DATA);
  appendInt16u(interval);
  appendInt8u(units);
  appendInt8u(failureLimit);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspSetSourceRoute(
      EmberNodeId destination,
      int8u relayCount,
      int16u *relayList)
{
  int8u status;
  startCommand(EZSP_SET_SOURCE_ROUTE);
  appendInt16u(destination);
  appendInt8u(relayCount);
  appendInt16uArray(relayCount, relayList);
  sendCommand();
  status = fetchInt8u();
  return status;
}

boolean emberAddressTableEntryIsActive(
      int8u addressTableIndex)
{
  int8u active;
  startCommand(EZSP_ADDRESS_TABLE_ENTRY_IS_ACTIVE);
  appendInt8u(addressTableIndex);
  sendCommand();
  active = fetchInt8u();
  return active;
}

EmberStatus emberSetAddressTableRemoteEui64(
      int8u addressTableIndex,
      EmberEUI64 eui64)
{
  int8u status;
  startCommand(EZSP_SET_ADDRESS_TABLE_REMOTE_EUI64);
  appendInt8u(addressTableIndex);
  appendInt8uArray(8, eui64);
  sendCommand();
  status = fetchInt8u();
  return status;
}

void emberSetAddressTableRemoteNodeId(
      int8u addressTableIndex,
      EmberNodeId id)
{
  startCommand(EZSP_SET_ADDRESS_TABLE_REMOTE_NODE_ID);
  appendInt8u(addressTableIndex);
  appendInt16u(id);
  sendCommand();
}

void emberGetAddressTableRemoteEui64(
      int8u addressTableIndex,
      EmberEUI64 eui64)
{
  startCommand(EZSP_GET_ADDRESS_TABLE_REMOTE_EUI64);
  appendInt8u(addressTableIndex);
  sendCommand();
  fetchInt8uArray(8, eui64);
}

EmberNodeId emberGetAddressTableRemoteNodeId(
      int8u addressTableIndex)
{
  int16u nodeId;
  startCommand(EZSP_GET_ADDRESS_TABLE_REMOTE_NODE_ID);
  appendInt8u(addressTableIndex);
  sendCommand();
  nodeId = fetchInt16u();
  return nodeId;
}

void emberSetExtendedTimeout(
      EmberEUI64 remoteEui64,
      boolean extendedTimeout)
{
  startCommand(EZSP_SET_EXTENDED_TIMEOUT);
  appendInt8uArray(8, remoteEui64);
  appendInt8u(extendedTimeout);
  sendCommand();
}

boolean emberGetExtendedTimeout(
      EmberEUI64 remoteEui64)
{
  int8u extendedTimeout;
  startCommand(EZSP_GET_EXTENDED_TIMEOUT);
  appendInt8uArray(8, remoteEui64);
  sendCommand();
  extendedTimeout = fetchInt8u();
  return extendedTimeout;
}

EmberStatus ezspReplaceAddressTableEntry(
      int8u addressTableIndex,
      EmberEUI64 newEui64,
      EmberNodeId newId,
      boolean newExtendedTimeout,
      EmberEUI64 oldEui64,
      EmberNodeId *oldId,
      boolean *oldExtendedTimeout)
{
  int8u status;
  startCommand(EZSP_REPLACE_ADDRESS_TABLE_ENTRY);
  appendInt8u(addressTableIndex);
  appendInt8uArray(8, newEui64);
  appendInt16u(newId);
  appendInt8u(newExtendedTimeout);
  sendCommand();
  status = fetchInt8u();
  fetchInt8uArray(8, oldEui64);
  *oldId = fetchInt16u();
  *oldExtendedTimeout = fetchInt8u();
  return status;
}

EmberNodeId emberLookupNodeIdByEui64(
      EmberEUI64 eui64)
{
  int16u nodeId;
  startCommand(EZSP_LOOKUP_NODE_ID_BY_EUI64);
  appendInt8uArray(8, eui64);
  sendCommand();
  nodeId = fetchInt16u();
  return nodeId;
}

EmberStatus emberLookupEui64ByNodeId(
      EmberNodeId nodeId,
      EmberEUI64 eui64)
{
  int8u status;
  startCommand(EZSP_LOOKUP_EUI64_BY_NODE_ID);
  appendInt16u(nodeId);
  sendCommand();
  status = fetchInt8u();
  fetchInt8uArray(8, eui64);
  return status;
}

EmberStatus ezspGetMulticastTableEntry(
      int8u index,
      EmberMulticastTableEntry *value)
{
  int8u status;
  startCommand(EZSP_GET_MULTICAST_TABLE_ENTRY);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  fetchEmberMulticastTableEntry(value);
  return status;
}

EmberStatus ezspSetMulticastTableEntry(
      int8u index,
      EmberMulticastTableEntry *value)
{
  int8u status;
  startCommand(EZSP_SET_MULTICAST_TABLE_ENTRY);
  appendInt8u(index);
  appendEmberMulticastTableEntry(value);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspSendRawMessage(
      int8u messageLength,
      int8u *messageContents)
{
  int8u status;
  startCommand(EZSP_SEND_RAW_MESSAGE);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// Security Frames
//------------------------------------------------------------------------------

EmberStatus emberSetInitialSecurityState(
      EmberInitialSecurityState *state)
{
  int8u success;
  startCommand(EZSP_SET_INITIAL_SECURITY_STATE);
  appendEmberInitialSecurityState(state);
  sendCommand();
  success = fetchInt8u();
  return success;
}

EmberStatus emberGetCurrentSecurityState(
      EmberCurrentSecurityState *state)
{
  int8u status;
  startCommand(EZSP_GET_CURRENT_SECURITY_STATE);
  sendCommand();
  status = fetchInt8u();
  fetchEmberCurrentSecurityState(state);
  return status;
}

EmberStatus emberGetKey(
      EmberKeyType keyType,
      EmberKeyStruct *keyStruct)
{
  int8u status;
  startCommand(EZSP_GET_KEY);
  appendInt8u(keyType);
  sendCommand();
  status = fetchInt8u();
  fetchEmberKeyStruct(keyStruct);
  return status;
}

EmberStatus emberGetKeyTableEntry(
      int8u index,
      EmberKeyStruct *keyStruct)
{
  int8u status;
  startCommand(EZSP_GET_KEY_TABLE_ENTRY);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  fetchEmberKeyStruct(keyStruct);
  return status;
}

EmberStatus emberSetKeyTableEntry(
      int8u index,
      EmberEUI64 address,
      boolean linkKey,
      EmberKeyData *keyData)
{
  int8u status;
  startCommand(EZSP_SET_KEY_TABLE_ENTRY);
  appendInt8u(index);
  appendInt8uArray(8, address);
  appendInt8u(linkKey);
  appendEmberKeyData(keyData);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int8u emberFindKeyTableEntry(
      EmberEUI64 address,
      boolean linkKey)
{
  int8u index;
  startCommand(EZSP_FIND_KEY_TABLE_ENTRY);
  appendInt8uArray(8, address);
  appendInt8u(linkKey);
  sendCommand();
  index = fetchInt8u();
  return index;
}

EmberStatus emberAddOrUpdateKeyTableEntry(
      EmberEUI64 address,
      boolean linkKey,
      EmberKeyData *keyData)
{
  int8u status;
  startCommand(EZSP_ADD_OR_UPDATE_KEY_TABLE_ENTRY);
  appendInt8uArray(8, address);
  appendInt8u(linkKey);
  appendEmberKeyData(keyData);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberEraseKeyTableEntry(
      int8u index)
{
  int8u status;
  startCommand(EZSP_ERASE_KEY_TABLE_ENTRY);
  appendInt8u(index);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberClearKeyTable(void)
{
  int8u status;
  startCommand(EZSP_CLEAR_KEY_TABLE);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberRequestLinkKey(
      EmberEUI64 partner)
{
  int8u status;
  startCommand(EZSP_REQUEST_LINK_KEY);
  appendInt8uArray(8, partner);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// Trust Center Frames
//------------------------------------------------------------------------------

EmberStatus emberBroadcastNextNetworkKey(
      EmberKeyData *key)
{
  int8u status;
  startCommand(EZSP_BROADCAST_NEXT_NETWORK_KEY);
  appendEmberKeyData(key);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberBroadcastNetworkKeySwitch(void)
{
  int8u status;
  startCommand(EZSP_BROADCAST_NETWORK_KEY_SWITCH);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberBecomeTrustCenter(
      EmberKeyData *newNetworkKey)
{
  int8u status;
  startCommand(EZSP_BECOME_TRUST_CENTER);
  appendEmberKeyData(newNetworkKey);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspAesMmoHash(
      EmberAesMmoHashContext *context,
      boolean finalize,
      int8u length,
      int8u *data,
      EmberAesMmoHashContext *returnContext)
{
  int8u status;
  startCommand(EZSP_AES_MMO_HASH);
  appendEmberAesMmoHashContext(context);
  appendInt8u(finalize);
  appendInt8u(length);
  appendInt8uArray(length, data);
  sendCommand();
  status = fetchInt8u();
  fetchEmberAesMmoHashContext(returnContext);
  return status;
}

EmberStatus ezspRemoveDevice(
      EmberNodeId destShort,
      EmberEUI64 destLong,
      EmberEUI64 targetLong)
{
  int8u status;
  startCommand(EZSP_REMOVE_DEVICE);
  appendInt16u(destShort);
  appendInt8uArray(8, destLong);
  appendInt8uArray(8, targetLong);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspUnicastNwkKeyUpdate(
      EmberNodeId destShort,
      EmberEUI64 destLong,
      EmberKeyData *key)
{
  int8u status;
  startCommand(EZSP_UNICAST_NWK_KEY_UPDATE);
  appendInt16u(destShort);
  appendInt8uArray(8, destLong);
  appendEmberKeyData(key);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// Certificate Based Key Exchange (CBKE)
//------------------------------------------------------------------------------

EmberStatus emberGenerateCbkeKeys(void)
{
  int8u status;
  startCommand(EZSP_GENERATE_CBKE_KEYS);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberCalculateSmacs(
      boolean amInitiator,
      EmberCertificateData *partnerCertificate,
      EmberPublicKeyData *partnerEphemeralPublicKey)
{
  int8u status;
  startCommand(EZSP_CALCULATE_SMACS);
  appendInt8u(amInitiator);
  appendEmberCertificateData(partnerCertificate);
  appendEmberPublicKeyData(partnerEphemeralPublicKey);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberClearTemporaryDataMaybeStoreLinkKey(
      boolean storeLinkKey)
{
  int8u status;
  startCommand(EZSP_CLEAR_TEMPORARY_DATA_MAYBE_STORE_LINK_KEY);
  appendInt8u(storeLinkKey);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberGetCertificate(
      EmberCertificateData *localCert)
{
  int8u status;
  startCommand(EZSP_GET_CERTIFICATE);
  sendCommand();
  status = fetchInt8u();
  fetchEmberCertificateData(localCert);
  return status;
}

EmberStatus ezspDsaSign(
      int8u messageLength,
      int8u *messageContents)
{
  int8u status;
  startCommand(EZSP_DSA_SIGN);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberDsaVerify(
      EmberMessageDigest *digest,
      EmberCertificateData *signerCertificate,
      EmberSignatureData *receivedSig)
{
  int8u status;
  startCommand(EZSP_DSA_VERIFY);
  appendEmberMessageDigest(digest);
  appendEmberCertificateData(signerCertificate);
  appendEmberSignatureData(receivedSig);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus emberSetPreinstalledCbkeData(
      EmberPublicKeyData *caPublic,
      EmberCertificateData *myCert,
      EmberPrivateKeyData *myKey)
{
  int8u status;
  startCommand(EZSP_SET_PREINSTALLED_CBKE_DATA);
  appendEmberPublicKeyData(caPublic);
  appendEmberCertificateData(myCert);
  appendEmberPrivateKeyData(myKey);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// Mfglib
//------------------------------------------------------------------------------

EmberStatus ezspMfglibStart(
      boolean rxCallback)
{
  int8u status;
  startCommand(EZSP_MFGLIB_START);
  appendInt8u(rxCallback);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibEnd(void)
{
  int8u status;
  startCommand(EZSP_MFGLIB_END);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibStartTone(void)
{
  int8u status;
  startCommand(EZSP_MFGLIB_START_TONE);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibStopTone(void)
{
  int8u status;
  startCommand(EZSP_MFGLIB_STOP_TONE);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibStartStream(void)
{
  int8u status;
  startCommand(EZSP_MFGLIB_START_STREAM);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibStopStream(void)
{
  int8u status;
  startCommand(EZSP_MFGLIB_STOP_STREAM);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibSendPacket(
      int8u packetLength,
      int8u *packetContents)
{
  int8u status;
  startCommand(EZSP_MFGLIB_SEND_PACKET);
  appendInt8u(packetLength);
  appendInt8uArray(packetLength, packetContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus mfglibSetChannel(
      int8u channel)
{
  int8u status;
  startCommand(EZSP_MFGLIB_SET_CHANNEL);
  appendInt8u(channel);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int8u mfglibGetChannel(void)
{
  int8u channel;
  startCommand(EZSP_MFGLIB_GET_CHANNEL);
  sendCommand();
  channel = fetchInt8u();
  return channel;
}

EmberStatus mfglibSetPower(
      int16u txPowerMode,
      int8s power)
{
  int8u status;
  startCommand(EZSP_MFGLIB_SET_POWER);
  appendInt16u(txPowerMode);
  appendInt8u(power);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int8s mfglibGetPower(void)
{
  int8s power;
  startCommand(EZSP_MFGLIB_GET_POWER);
  sendCommand();
  power = fetchInt8u();
  return power;
}

//------------------------------------------------------------------------------
// Bootloader
//------------------------------------------------------------------------------

EmberStatus ezspLaunchStandaloneBootloader(
      int8u mode)
{
  int8u status;
  startCommand(EZSP_LAUNCH_STANDALONE_BOOTLOADER);
  appendInt8u(mode);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspSendBootloadMessage(
      boolean broadcast,
      EmberEUI64 destEui64,
      int8u messageLength,
      int8u *messageContents)
{
  int8u status;
  startCommand(EZSP_SEND_BOOTLOAD_MESSAGE);
  appendInt8u(broadcast);
  appendInt8uArray(8, destEui64);
  appendInt8u(messageLength);
  appendInt8uArray(messageLength, messageContents);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int16u ezspGetStandaloneBootloaderVersionPlatMicroPhy(
      int8u *nodePlat,
      int8u *nodeMicro,
      int8u *nodePhy)
{
  int16u bootloader_version;
  startCommand(EZSP_GET_STANDALONE_BOOTLOADER_VERSION_PLAT_MICRO_PHY);
  sendCommand();
  bootloader_version = fetchInt16u();
  *nodePlat = fetchInt8u();
  *nodeMicro = fetchInt8u();
  *nodePhy = fetchInt8u();
  return bootloader_version;
}

void ezspAesEncrypt(
      int8u *plaintext,
      int8u *key,
      int8u *ciphertext)
{
  startCommand(EZSP_AES_ENCRYPT);
  appendInt8uArray(16, plaintext);
  appendInt8uArray(16, key);
  sendCommand();
  fetchInt8uArray(16, ciphertext);
}

EmberStatus ezspOverrideCurrentChannel(
      int8u channel)
{
  int8u status;
  startCommand(EZSP_OVERRIDE_CURRENT_CHANNEL);
  appendInt8u(channel);
  sendCommand();
  status = fetchInt8u();
  return status;
}

//------------------------------------------------------------------------------
// ZLL
//------------------------------------------------------------------------------

EmberStatus ezspZllNetworkOps(
      EmberZllNetwork *networkInfo,
      EzspZllNetworkOperation op,
      int8s radioTxPower)
{
  int8u status;
  startCommand(EZSP_ZLL_NETWORK_OPS);
  appendEmberZllNetwork(networkInfo);
  appendInt8u(op);
  appendInt8u(radioTxPower);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspZllSetInitialSecurityState(
      EmberKeyData *networkKey,
      EmberZllInitialSecurityState *securityState)
{
  int8u status;
  startCommand(EZSP_ZLL_SET_INITIAL_SECURITY_STATE);
  appendEmberKeyData(networkKey);
  appendEmberZllInitialSecurityState(securityState);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspZllStartScan(
      int32u channelMask,
      int8s radioPowerForScan,
      EmberNodeType nodeType)
{
  int8u status;
  startCommand(EZSP_ZLL_START_SCAN);
  appendInt32u(channelMask);
  appendInt8u(radioPowerForScan);
  appendInt8u(nodeType);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspZllSetRxOnWhenIdle(
      int16u durationMs)
{
  int8u status;
  startCommand(EZSP_ZLL_SET_RX_ON_WHEN_IDLE);
  appendInt16u(durationMs);
  sendCommand();
  status = fetchInt8u();
  return status;
}

EmberStatus ezspSetLogicalAndRadioChannel(
      int8u radioChannel)
{
  int8u status;
  startCommand(EZSP_SET_LOGICAL_AND_RADIO_CHANNEL);
  appendInt8u(radioChannel);
  sendCommand();
  status = fetchInt8u();
  return status;
}

int8u ezspGetLogicalChannel(void)
{
  int8u logicalChannel;
  startCommand(EZSP_GET_LOGICAL_CHANNEL);
  sendCommand();
  logicalChannel = fetchInt8u();
  return logicalChannel;
}

void ezspZllGetTokens(
      EmberTokTypeStackZllData *data,
      EmberTokTypeStackZllSecurity *security)
{
  startCommand(EZSP_ZLL_GET_TOKENS);
  sendCommand();
  fetchEmberTokTypeStackZllData(data);
  fetchEmberTokTypeStackZllSecurity(security);
}

void ezspZllSetDataToken(
      EmberTokTypeStackZllData *data)
{
  startCommand(EZSP_ZLL_SET_DATA_TOKEN);
  appendEmberTokTypeStackZllData(data);
  sendCommand();
}

void ezspZllSetNonZllNetwork(void)
{
  startCommand(EZSP_ZLL_SET_NON_ZLL_NETWORK);
  sendCommand();
}

boolean ezspIsZllNetwork(void)
{
  int8u isZllNetwork;
  startCommand(EZSP_IS_ZLL_NETWORK);
  sendCommand();
  isZllNetwork = fetchInt8u();
  return isZllNetwork;
}

static void callbackDispatch(void)
{
  callbackPointerInit();
  switch (serialGetResponseByte(EZSP_FRAME_ID_INDEX)) {

  case EZSP_NO_CALLBACKS: {
    ezspNoCallbacks();
    break;
  }

  case EZSP_STACK_TOKEN_CHANGED_HANDLER: {
    int16u tokenAddress;
    tokenAddress = fetchInt16u();
    ezspStackTokenChangedHandler(tokenAddress);
    break;
  }

  case EZSP_TIMER_HANDLER: {
    int8u timerId;
    timerId = fetchInt8u();
    ezspTimerHandler(timerId);
    break;
  }

  case EZSP_CUSTOM_FRAME_HANDLER: {
    int8u payloadLength;
    int8u *payload;
    payloadLength = fetchInt8u();
    payload = fetchInt8uPointer(payloadLength);
    ezspCustomFrameHandler(payloadLength, payload);
    break;
  }

  case EZSP_STACK_STATUS_HANDLER: {
    int8u status;
    status = fetchInt8u();
    ezspStackStatusHandler(status);
    break;
  }

  case EZSP_ENERGY_SCAN_RESULT_HANDLER: {
    int8u channel;
    int8s maxRssiValue;
    channel = fetchInt8u();
    maxRssiValue = fetchInt8u();
    ezspEnergyScanResultHandler(channel, maxRssiValue);
    break;
  }

  case EZSP_NETWORK_FOUND_HANDLER: {
    EmberZigbeeNetwork networkFound;
    int8u lastHopLqi;
    int8s lastHopRssi;
    fetchEmberZigbeeNetwork(&networkFound);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    ezspNetworkFoundHandler(&networkFound, lastHopLqi, lastHopRssi);
    break;
  }

  case EZSP_SCAN_COMPLETE_HANDLER: {
    int8u channel;
    int8u status;
    channel = fetchInt8u();
    status = fetchInt8u();
    ezspScanCompleteHandler(channel, status);
    break;
  }

  case EZSP_CHILD_JOIN_HANDLER: {
    int8u index;
    int8u joining;
    int16u childId;
    int8u childEui64[8];
    int8u childType;
    index = fetchInt8u();
    joining = fetchInt8u();
    childId = fetchInt16u();
    fetchInt8uArray(8, childEui64);
    childType = fetchInt8u();
    ezspChildJoinHandler(index, joining, childId, childEui64, childType);
    break;
  }

  case EZSP_REMOTE_SET_BINDING_HANDLER: {
    EmberBindingTableEntry entry;
    int8u index;
    int8u policyDecision;
    fetchEmberBindingTableEntry(&entry);
    index = fetchInt8u();
    policyDecision = fetchInt8u();
    ezspRemoteSetBindingHandler(&entry, index, policyDecision);
    break;
  }

  case EZSP_REMOTE_DELETE_BINDING_HANDLER: {
    int8u index;
    int8u policyDecision;
    index = fetchInt8u();
    policyDecision = fetchInt8u();
    ezspRemoteDeleteBindingHandler(index, policyDecision);
    break;
  }

  case EZSP_MESSAGE_SENT_HANDLER: {
    int8u type;
    int16u indexOrDestination;
    EmberApsFrame apsFrame;
    int8u messageTag;
    int8u status;
    int8u messageLength;
    int8u *messageContents;
    type = fetchInt8u();
    indexOrDestination = fetchInt16u();
    fetchEmberApsFrame(&apsFrame);
    messageTag = fetchInt8u();
    status = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspMessageSentHandler(type, indexOrDestination, &apsFrame, messageTag, status, messageLength, messageContents);
    break;
  }

  case EZSP_POLL_COMPLETE_HANDLER: {
    int8u status;
    status = fetchInt8u();
    ezspPollCompleteHandler(status);
    break;
  }

  case EZSP_POLL_HANDLER: {
    int16u childId;
    childId = fetchInt16u();
    ezspPollHandler(childId);
    break;
  }

  case EZSP_INCOMING_SENDER_EUI64_HANDLER: {
    int8u senderEui64[8];
    fetchInt8uArray(8, senderEui64);
    ezspIncomingSenderEui64Handler(senderEui64);
    break;
  }

  case EZSP_INCOMING_MESSAGE_HANDLER: {
    int8u type;
    EmberApsFrame apsFrame;
    int8u lastHopLqi;
    int8s lastHopRssi;
    int16u sender;
    int8u bindingIndex;
    int8u addressIndex;
    int8u messageLength;
    int8u *messageContents;
    type = fetchInt8u();
    fetchEmberApsFrame(&apsFrame);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    sender = fetchInt16u();
    bindingIndex = fetchInt8u();
    addressIndex = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspIncomingMessageHandler(type, &apsFrame, lastHopLqi, lastHopRssi, sender, bindingIndex, addressIndex, messageLength, messageContents);
    break;
  }

  case EZSP_INCOMING_ROUTE_RECORD_HANDLER: {
    int16u source;
    int8u sourceEui[8];
    int8u lastHopLqi;
    int8s lastHopRssi;
    int8u relayCount;
    int8u *relayList;
    source = fetchInt16u();
    fetchInt8uArray(8, sourceEui);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    relayCount = fetchInt8u();
    relayList = fetchInt8uPointer(relayCount*2);
    ezspIncomingRouteRecordHandler(source, sourceEui, lastHopLqi, lastHopRssi, relayCount, relayList);
    break;
  }

  case EZSP_INCOMING_MANY_TO_ONE_ROUTE_REQUEST_HANDLER: {
    int16u source;
    int8u longId[8];
    int8u cost;
    source = fetchInt16u();
    fetchInt8uArray(8, longId);
    cost = fetchInt8u();
    ezspIncomingManyToOneRouteRequestHandler(source, longId, cost);
    break;
  }

  case EZSP_INCOMING_ROUTE_ERROR_HANDLER: {
    int8u status;
    int16u target;
    status = fetchInt8u();
    target = fetchInt16u();
    ezspIncomingRouteErrorHandler(status, target);
    break;
  }

  case EZSP_ID_CONFLICT_HANDLER: {
    int16u id;
    id = fetchInt16u();
    ezspIdConflictHandler(id);
    break;
  }

  case EZSP_MAC_PASSTHROUGH_MESSAGE_HANDLER: {
    int8u messageType;
    int8u lastHopLqi;
    int8s lastHopRssi;
    int8u messageLength;
    int8u *messageContents;
    messageType = fetchInt8u();
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspMacPassthroughMessageHandler(messageType, lastHopLqi, lastHopRssi, messageLength, messageContents);
    break;
  }

  case EZSP_MAC_FILTER_MATCH_MESSAGE_HANDLER: {
    int8u filterIndexMatch;
    int8u legacyPassthroughType;
    int8u lastHopLqi;
    int8s lastHopRssi;
    int8u messageLength;
    int8u *messageContents;
    filterIndexMatch = fetchInt8u();
    legacyPassthroughType = fetchInt8u();
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspMacFilterMatchMessageHandler(filterIndexMatch, legacyPassthroughType, lastHopLqi, lastHopRssi, messageLength, messageContents);
    break;
  }

  case EZSP_RAW_TRANSMIT_COMPLETE_HANDLER: {
    int8u status;
    status = fetchInt8u();
    ezspRawTransmitCompleteHandler(status);
    break;
  }

  case EZSP_SWITCH_NETWORK_KEY_HANDLER: {
    int8u sequenceNumber;
    sequenceNumber = fetchInt8u();
    ezspSwitchNetworkKeyHandler(sequenceNumber);
    break;
  }

  case EZSP_ZIGBEE_KEY_ESTABLISHMENT_HANDLER: {
    int8u partner[8];
    int8u status;
    fetchInt8uArray(8, partner);
    status = fetchInt8u();
    ezspZigbeeKeyEstablishmentHandler(partner, status);
    break;
  }

  case EZSP_TRUST_CENTER_JOIN_HANDLER: {
    int16u newNodeId;
    int8u newNodeEui64[8];
    int8u status;
    int8u policyDecision;
    int16u parentOfNewNodeId;
    newNodeId = fetchInt16u();
    fetchInt8uArray(8, newNodeEui64);
    status = fetchInt8u();
    policyDecision = fetchInt8u();
    parentOfNewNodeId = fetchInt16u();
    ezspTrustCenterJoinHandler(newNodeId, newNodeEui64, status, policyDecision, parentOfNewNodeId);
    break;
  }

  case EZSP_GENERATE_CBKE_KEYS_HANDLER: {
    int8u status;
    EmberPublicKeyData ephemeralPublicKey;
    status = fetchInt8u();
    fetchEmberPublicKeyData(&ephemeralPublicKey);
    ezspGenerateCbkeKeysHandler(status, &ephemeralPublicKey);
    break;
  }

  case EZSP_CALCULATE_SMACS_HANDLER: {
    int8u status;
    EmberSmacData initiatorSmac;
    EmberSmacData responderSmac;
    status = fetchInt8u();
    fetchEmberSmacData(&initiatorSmac);
    fetchEmberSmacData(&responderSmac);
    ezspCalculateSmacsHandler(status, &initiatorSmac, &responderSmac);
    break;
  }

  case EZSP_DSA_SIGN_HANDLER: {
    int8u status;
    int8u messageLength;
    int8u *messageContents;
    status = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspDsaSignHandler(status, messageLength, messageContents);
    break;
  }

  case EZSP_DSA_VERIFY_HANDLER: {
    int8u status;
    status = fetchInt8u();
    ezspDsaVerifyHandler(status);
    break;
  }

  case EZSP_MFGLIB_RX_HANDLER: {
    int8u linkQuality;
    int8s rssi;
    int8u packetLength;
    int8u *packetContents;
    linkQuality = fetchInt8u();
    rssi = fetchInt8u();
    packetLength = fetchInt8u();
    packetContents = fetchInt8uPointer(packetLength);
    ezspMfglibRxHandler(linkQuality, rssi, packetLength, packetContents);
    break;
  }

  case EZSP_INCOMING_BOOTLOAD_MESSAGE_HANDLER: {
    int8u longId[8];
    int8u lastHopLqi;
    int8s lastHopRssi;
    int8u messageLength;
    int8u *messageContents;
    fetchInt8uArray(8, longId);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspIncomingBootloadMessageHandler(longId, lastHopLqi, lastHopRssi, messageLength, messageContents);
    break;
  }

  case EZSP_BOOTLOAD_TRANSMIT_COMPLETE_HANDLER: {
    int8u status;
    int8u messageLength;
    int8u *messageContents;
    status = fetchInt8u();
    messageLength = fetchInt8u();
    messageContents = fetchInt8uPointer(messageLength);
    ezspBootloadTransmitCompleteHandler(status, messageLength, messageContents);
    break;
  }

  case EZSP_ZLL_NETWORK_FOUND_HANDLER: {
    EmberZllNetwork networkInfo;
    int8u isDeviceInfoNull;
    EmberZllDeviceInfoRecord deviceInfo;
    int8u lastHopLqi;
    int8s lastHopRssi;
    fetchEmberZllNetwork(&networkInfo);
    isDeviceInfoNull = fetchInt8u();
    fetchEmberZllDeviceInfoRecord(&deviceInfo);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    ezspZllNetworkFoundHandler(&networkInfo, isDeviceInfoNull, &deviceInfo, lastHopLqi, lastHopRssi);
    break;
  }

  case EZSP_ZLL_SCAN_COMPLETE_HANDLER: {
    int8u status;
    status = fetchInt8u();
    ezspZllScanCompleteHandler(status);
    break;
  }

  case EZSP_ZLL_ADDRESS_ASSIGNMENT_HANDLER: {
    EmberZllAddressAssignment addressInfo;
    int8u lastHopLqi;
    int8s lastHopRssi;
    fetchEmberZllAddressAssignment(&addressInfo);
    lastHopLqi = fetchInt8u();
    lastHopRssi = fetchInt8u();
    ezspZllAddressAssignmentHandler(&addressInfo, lastHopLqi, lastHopRssi);
    break;
  }

  case EZSP_ZLL_TOUCH_LINK_TARGET_HANDLER: {
    EmberZllNetwork networkInfo;
    fetchEmberZllNetwork(&networkInfo);
    ezspZllTouchLinkTargetHandler(&networkInfo);
    break;
  }

  default:
    ezspErrorHandler(EZSP_ERROR_INVALID_FRAME_ID);
  }

}  

