// *******************************************************************
// * simple-metering-client.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "simple-metering-client-callback.h"

static void clusterRequestCommon(int8u responseCommandId)
{
  int16u endpointId;
  EmberEUI64 otaEui;

  if (emberLookupEui64ByNodeId(emberAfResponseDestination, otaEui)
      != EMBER_SUCCESS) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_FAILURE);
    return;
  }

  endpointId = (ZCL_REQUEST_MIRROR_RESPONSE_COMMAND_ID == responseCommandId
                ? emberAfPluginSimpleMeteringClientRequestMirrorCallback(otaEui)
                : emberAfPluginSimpleMeteringClientRemoveMirrorCallback(otaEui));

  emberAfFillExternalBuffer(ZCL_CLUSTER_SPECIFIC_COMMAND
                            | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                            | EMBER_AF_DEFAULT_RESPONSE_POLICY_RESPONSES,
                            ZCL_SIMPLE_METERING_CLUSTER_ID,
                            responseCommandId,
                            "v",
                            endpointId);
  emberAfSendResponse();
}

boolean emberAfSimpleMeteringClusterGetProfileResponseCallback(int32u endTime,
                                                               int8u status,
                                                               int8u profileIntervalPeriod,
                                                               int8u numberOfPeriodsDelivered,
                                                               int8u* intervals)
{
  int8u i;
  emberAfSimpleMeteringClusterPrint("RX: GetProfileResponse 0x%4x, 0x%x, 0x%x, 0x%x",
                                    endTime,
                                    status,
                                    profileIntervalPeriod,
                                    numberOfPeriodsDelivered);
  for (i = 0; i < numberOfPeriodsDelivered; i++) {
    emberAfSimpleMeteringClusterPrint(" [0x%4x]",
                                      emberAfGetInt24u(intervals + i * 3, 0, 3));
  }
  emberAfSimpleMeteringClusterPrintln("");
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfSimpleMeteringClusterRequestMirrorCallback(void)
{
  emberAfSimpleMeteringClusterPrintln("RX: RequestMirror");
  clusterRequestCommon(ZCL_REQUEST_MIRROR_RESPONSE_COMMAND_ID);
  return TRUE;
}

boolean emberAfSimpleMeteringClusterRemoveMirrorCallback(void)
{
  emberAfSimpleMeteringClusterPrintln("RX: RemoveMirror");
  clusterRequestCommon(ZCL_MIRROR_REMOVED_COMMAND_ID);
  return TRUE;
}

boolean emberAfSimpleMeteringClusterRequestFastPollModeResponseCallback(int8u appliedUpdatePeriod,
                                                                        int32u fastPollModeEndtime)
{
  emberAfSimpleMeteringClusterPrintln("RX: RequestFastPollModeResponse 0x%x, 0x%4x",
                                      appliedUpdatePeriod,
                                      fastPollModeEndtime);
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}
