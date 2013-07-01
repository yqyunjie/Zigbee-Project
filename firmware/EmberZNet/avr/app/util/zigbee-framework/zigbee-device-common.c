// File: zigbee-device-common.c
//
// Description: ZigBee Device Object (ZDO) functions available on all platforms.
//
//
// Copyright 2007 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "zigbee-device-common.h"

static int8u zigDevRequestSequence = 0;
int8u zigDevRequestRadius = 255;

int8u emberGetLastZigDevRequestSequence(void)
{
  return zigDevRequestSequence;
}

int8u emberNextZigDevRequestSequence(void)
{
  return ((++zigDevRequestSequence) & 0x7F);
}

EmberStatus emberSendZigDevRequestTarget(EmberNodeId target,
                                         int16u clusterId,
                                         EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 2];
  int8u *payload = contents + ZDO_MESSAGE_OVERHEAD;
  payload[0] = LOW_BYTE(target);
  payload[1] = HIGH_BYTE(target);
  return emberSendZigDevRequest(target,
                                clusterId,
                                options,
                                contents,
                                sizeof(contents));
}

EmberStatus emberSimpleDescriptorRequest(EmberNodeId target,
                                         int8u targetEndpoint,
                                         EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 3];
  int8u *payload = contents + ZDO_MESSAGE_OVERHEAD;
  payload[0] = LOW_BYTE(target);
  payload[1] = HIGH_BYTE(target);
  payload[2] = targetEndpoint;
  return emberSendZigDevRequest(target,
                                SIMPLE_DESCRIPTOR_REQUEST,
                                options,
                                contents,
                                sizeof(contents));
}

EmberStatus emberSendZigDevBindRequest(EmberNodeId target,
                                       int16u bindClusterId,
                                       EmberEUI64 source,
                                       int8u sourceEndpoint,
                                       int16u clusterId,
                                       int8u type,
                                       EmberEUI64 destination,
                                       EmberMulticastId groupAddress,
                                       int8u destinationEndpoint,
                                       EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 21];
  int8u *payload = contents + ZDO_MESSAGE_OVERHEAD;
  int8u length;
  MEMCOPY(payload, source, 8);
  payload[8] = sourceEndpoint;
  payload[9] = LOW_BYTE(clusterId);
  payload[10] = HIGH_BYTE(clusterId);
  payload[11] = type;
  switch (type) {
  case UNICAST_BINDING:
    MEMCOPY(payload + 12, destination, 8);
    payload[20] = destinationEndpoint;
    length = ZDO_MESSAGE_OVERHEAD + 21;
    break;
  case MULTICAST_BINDING:
    payload[12] = LOW_BYTE(groupAddress);
    payload[13] = HIGH_BYTE(groupAddress);
    length = ZDO_MESSAGE_OVERHEAD + 14;
    break;
  default:
    return EMBER_ERR_FATAL;
  }
  return emberSendZigDevRequest(target,
                                bindClusterId,
                                options,
                                contents,
                                length);
}

#define emberLqiTableRequest(target, startIndex, options) \
  (emberTableRequest(LQI_TABLE_REQUEST, (target), (startIndex), (options)))
#define emberRoutingTableRequest(target, startIndex, options) \
  (emberTableRequest(ROUTING_TABLE_REQUEST, (target), (startIndex), (options)))
#define emberBindingTableRequest(target, startIndex, options) \
  (emberTableRequest(BINDING_TABLE_REQUEST, (target), (startIndex), (options)))

EmberStatus emberTableRequest(int16u clusterId,
                              EmberNodeId target,
                              int8u startIndex,
                              EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 1];
  contents[ZDO_MESSAGE_OVERHEAD] = startIndex;
  return emberSendZigDevRequest(target,
                                clusterId,
                                options,
                                contents,
                                sizeof(contents));
}

EmberStatus emberLeaveRequest(EmberNodeId target,
                              EmberEUI64 deviceAddress,
                              int8u leaveRequestFlags,
                              EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 9];
  MEMCOPY(contents + ZDO_MESSAGE_OVERHEAD, deviceAddress, 8);
  contents[ZDO_MESSAGE_OVERHEAD + 8] = leaveRequestFlags;
  return emberSendZigDevRequest(target,
                                LEAVE_REQUEST,
                                options,
                                contents,
                                sizeof(contents));
}

EmberStatus emberPermitJoiningRequest(EmberNodeId target,
                                      int8u duration,
                                      int8u authentication,
                                      EmberApsOption options)
{
  int8u contents[ZDO_MESSAGE_OVERHEAD + 2];
  contents[ZDO_MESSAGE_OVERHEAD] = duration;
  contents[ZDO_MESSAGE_OVERHEAD + 1] = authentication;
  return emberSendZigDevRequest(target,
                                PERMIT_JOINING_REQUEST,
                                options,
                                contents,
                                sizeof(contents));
}
