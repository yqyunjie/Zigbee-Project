// *****************************************************************************
// * service-discovery-soc.c
// *
// * SOC specific routines for performing service discovery.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/zigbee-framework/zigbee-device-library.h"

//------------------------------------------------------------------------------

EmberStatus emAfSendMatchDescriptor(EmberNodeId target,
                                    EmberAfProfileId profileId,
                                    EmberAfClusterId clusterId,
                                    boolean serverCluster)
{
  EmberMessageBuffer clusterList = emberAllocateLinkedBuffers(1);
  EmberMessageBuffer inClusters = EMBER_NULL_MESSAGE_BUFFER;
  EmberMessageBuffer outClusters = EMBER_NULL_MESSAGE_BUFFER;
  EmberStatus status = EMBER_NO_BUFFERS;

  if (clusterList != EMBER_NULL_MESSAGE_BUFFER) {
    emberSetMessageBufferLength(clusterList, 2);
    emberSetLinkedBuffersLowHighInt16u(clusterList, 0, clusterId);

    if (serverCluster) {
      inClusters = clusterList;
    } else {
      outClusters = clusterList;
    }

    status = emberMatchDescriptorsRequest(target,
                                          profileId,
                                          inClusters,
                                          outClusters,
                                          EMBER_AF_DEFAULT_APS_OPTIONS);
    emberReleaseMessageBuffer(clusterList);
  }
  return status;
}
