// Copyright 2007 - 2012 by Ember Corporation. All rights reserved.
// 
//

#include "app/framework/include/af.h"
#include "app/util/counters/counters.h"
#include "diagnostic-server.h"

boolean emberAfReadDiagnosticAttribute(
                    EmberAfAttributeMetadata *attributeMetadata, 
                    int8u *buffer) {
  int8u emberCounter = EMBER_COUNTER_TYPE_COUNT;
  switch (attributeMetadata->attributeId) {
    case ZCL_MAC_RX_BCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_RX_BROADCAST;
      break;
    case ZCL_MAC_TX_BCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_TX_BROADCAST;
      break;
    case ZCL_MAC_RX_UCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_RX_UNICAST;
      break;
    case ZCL_MAC_TX_UCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_TX_UNICAST_SUCCESS;
      break;
    case ZCL_MAC_TX_UCAST_RETRY_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_TX_UNICAST_RETRY;
      break;
    case ZCL_MAC_TX_UCAST_FAIL_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_MAC_TX_UNICAST_FAILED;
      break;
    case ZCL_APS_RX_BCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_RX_BROADCAST;
      break;
    case ZCL_APS_TX_BCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_TX_BROADCAST;
      break;
    case ZCL_APS_RX_UCAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_RX_UNICAST;
      break;
    case ZCL_APS_UCAST_SUCCESS_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_TX_UNICAST_SUCCESS;
      break;
    case ZCL_APS_TX_UCAST_RETRY_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_TX_UNICAST_RETRY;
      break;
    case ZCL_APS_TX_UCAST_FAIL_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DATA_TX_UNICAST_FAILED;
      break;
    case ZCL_ROUTE_DISC_INITIATED_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_ROUTE_DISCOVERY_INITIATED;
      break;
    case ZCL_NEIGHBOR_ADDED_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_NEIGHBOR_ADDED;
      break;
    case ZCL_NEIGHBOR_REMOVED_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_NEIGHBOR_REMOVED;
      break;
    case ZCL_NEIGHBOR_STALE_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_NEIGHBOR_STALE;
      break;
    case ZCL_JOIN_INDICATION_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_JOIN_INDICATION;
      break;
    case ZCL_CHILD_MOVED_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_CHILD_REMOVED;
      break;
    case ZCL_NWK_FC_FAILURE_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_NWK_FRAME_COUNTER_FAILURE;
      break;
    case ZCL_APS_FC_FAILURE_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_FRAME_COUNTER_FAILURE;
      break;
    case ZCL_APS_UNAUTHORIZED_KEY_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_LINK_KEY_NOT_AUTHORIZED;
      break;
    case ZCL_NWK_DECRYPT_FAILURE_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_NWK_DECRYPTION_FAILURE;
      break;
    case ZCL_APS_DECRYPT_FAILURE_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_APS_DECRYPTION_FAILURE;
      break;
    case ZCL_PACKET_BUFFER_ALLOC_FAILURES_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_ALLOCATE_PACKET_BUFFER_FAILURE;
      break;
    case ZCL_RELAYED_UNICAST_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_RELAYED_UNICAST;
      break;
    case ZCL_PHY_TO_MAC_QUEUE_LIMIT_REACHED_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_PHY_TO_MAC_QUEUE_LIMIT_REACHED;
      break;
    case ZCL_PACKET_VALIDATE_DROP_COUNT_ATTRIBUTE_ID:
      emberCounter = EMBER_COUNTER_PACKET_VALIDATE_LIBRARY_DROPPED_COUNT;
      break;  
    default:
      break;
  }
  if (emberCounter < EMBER_COUNTER_TYPE_COUNT) {
    MEMCOPY(buffer, &emberCounters[emberCounter], 2);
    return TRUE;
  }
  return FALSE;
}