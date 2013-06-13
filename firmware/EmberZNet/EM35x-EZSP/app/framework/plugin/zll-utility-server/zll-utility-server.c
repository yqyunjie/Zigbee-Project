// *******************************************************************
// * zll-utility-server.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "../zll-commissioning/zll-commissioning.h"
#include "zll-commissioning-callback.h"

boolean emberAfZllCommissioningClusterGetGroupIdentifiersRequestCallback(int8u startIndex)
{
  int8u endpoint = emberAfCurrentEndpoint();
  int8u total = emberAfPluginZllCommissioningGroupIdentifierCountCallback(endpoint);
  int8u i, *count;

  emberAfZllCommissioningClusterPrintln("RX: GetGroupIdentifiersRequest 0x%x",
                                        startIndex);

  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                             | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_GET_GROUP_IDENTIFIERS_RESPONSE_COMMAND_ID,
                            "uu",
                            total,
                            startIndex);

  count = &appResponseData[appResponseLength];
  emberAfPutInt8uInResp(0); // temporary count

  for (i = startIndex; i < total; i++) {
    EmberAfPluginZllCommissioningGroupInformationRecord record;
    if (emberAfPluginZllCommissioningGroupIdentifierCallback(endpoint,
                                                             i,
                                                             &record)) {
      emberAfPutInt16uInResp(record.groupId);
      emberAfPutInt8uInResp(record.groupType);
      (*count)++;
    }
  }

  emberAfSendResponse();
  return TRUE;
}

boolean emberAfZllCommissioningClusterGetEndpointListRequestCallback(int8u startIndex)
{
  int8u endpoint = emberAfCurrentEndpoint();
  int8u total = emberAfPluginZllCommissioningEndpointInformationCountCallback(endpoint);
  int8u i, *count;

  emberAfZllCommissioningClusterPrintln("RX: GetEndpointListRequest 0x%x",
                                        startIndex);

  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                             | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_GET_ENDPOINT_LIST_RESPONSE_COMMAND_ID,
                            "uu",
                            total,
                            startIndex);

  count = &appResponseData[appResponseLength];
  emberAfPutInt8uInResp(0); // temporary count

  for (i = startIndex; i < total; i++) {
    EmberAfPluginZllCommissioningEndpointInformationRecord record;
    if (emberAfPluginZllCommissioningEndpointInformationCallback(endpoint,
                                                                 i,
                                                                 &record)) {
      emberAfPutInt16uInResp(record.networkAddress);
      emberAfPutInt8uInResp(record.endpointId);
      emberAfPutInt16uInResp(record.profileId);
      emberAfPutInt16uInResp(record.deviceId);
      emberAfPutInt8uInResp(record.version);
      (*count)++;
    }
  }

  emberAfSendResponse();
  return TRUE;
}
