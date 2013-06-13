// *******************************************************************
// * zll-utility-client.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"

boolean emberAfZllCommissioningClusterEndpointInformationCallback(int8u *ieeeAddress,
                                                                  int16u networkAddress,
                                                                  int8u endpointId,
                                                                  int16u profileId,
                                                                  int16u deviceId,
                                                                  int8u version)
{
  emberAfZllCommissioningClusterPrint("RX: EndpointInformation ");
  emberAfZllCommissioningClusterDebugExec(emberAfPrintBigEndianEui64(ieeeAddress));
  emberAfZllCommissioningClusterPrintln(", 0x%2x, 0x%x, 0x%2x, 0x%2x, 0x%x",
                                        networkAddress,
                                        endpointId,
                                        profileId,
                                        deviceId,
                                        version);
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfZllCommissioningClusterGetGroupIdentifiersResponseCallback(int8u total,
                                                                          int8u startIndex,
                                                                          int8u count,
                                                                          int8u *groupInformationRecordList)
{
  int16u groupInformationRecordListLen = (emberAfCurrentCommand()->bufLen
                                          - (emberAfCurrentCommand()->payloadStartIndex
                                             + sizeof(total)
                                             + sizeof(startIndex)
                                             + sizeof(count)));
  int16u groupInformationRecordListIndex = 0;
  int8u i;

  emberAfZllCommissioningClusterPrint("RX: GetGroupIdentifiersResponse 0x%x, 0x%x, 0x%x,",
                                      total,
                                      startIndex,
                                      count);

  for (i = 0; i < count; i++) {
    int16u groupId;
    int8u groupType;
    groupId = emberAfGetInt16u(groupInformationRecordList, groupInformationRecordListIndex, groupInformationRecordListLen);
    groupInformationRecordListIndex += 2;
    groupType = emberAfGetInt8u(groupInformationRecordList, groupInformationRecordListIndex, groupInformationRecordListLen);
    groupInformationRecordListIndex++;
    emberAfZllCommissioningClusterPrint(" [0x%2x 0x%x]", groupId, groupType);
  }

  emberAfZllCommissioningClusterPrintln("");
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfZllCommissioningClusterGetEndpointListResponseCallback(int8u total,
                                                                      int8u startIndex,
                                                                      int8u count,
                                                                      int8u *endpointInformationRecordList)
{
  int16u endpointInformationRecordListLen = (emberAfCurrentCommand()->bufLen
                                             - (emberAfCurrentCommand()->payloadStartIndex
                                                + sizeof(total)
                                                + sizeof(startIndex)
                                                + sizeof(count)));
  int16u endpointInformationRecordListIndex = 0;
  int8u i;

  emberAfZllCommissioningClusterPrint("RX: GetEndpointListResponse 0x%x, 0x%x, 0x%x,",
                                      total,
                                      startIndex,
                                      count);

  for (i = 0; i < count; i++) {
    int16u networkAddress;
    int8u endpointId;
    int16u profileId;
    int16u deviceId;
    int8u version;
    networkAddress = emberAfGetInt16u(endpointInformationRecordList, endpointInformationRecordListIndex, endpointInformationRecordListLen);
    endpointInformationRecordListIndex += 2;
    endpointId = emberAfGetInt8u(endpointInformationRecordList, endpointInformationRecordListIndex, endpointInformationRecordListLen);
    endpointInformationRecordListIndex++;
    profileId = emberAfGetInt16u(endpointInformationRecordList, endpointInformationRecordListIndex, endpointInformationRecordListLen);
    endpointInformationRecordListIndex += 2;
    deviceId = emberAfGetInt16u(endpointInformationRecordList, endpointInformationRecordListIndex, endpointInformationRecordListLen);
    endpointInformationRecordListIndex += 2;
    version = emberAfGetInt8u(endpointInformationRecordList, endpointInformationRecordListIndex, endpointInformationRecordListLen);
    endpointInformationRecordListIndex++;
    emberAfZllCommissioningClusterPrint(" [0x%2x 0x%x 0x%2x 0x%2x 0x%x]",
                                        networkAddress,
                                        endpointId,
                                        profileId,
                                        deviceId,
                                        version);
  }

  emberAfZllCommissioningClusterPrintln("");
  emberAfSendDefaultResponse(emberAfCurrentCommand(), EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}
