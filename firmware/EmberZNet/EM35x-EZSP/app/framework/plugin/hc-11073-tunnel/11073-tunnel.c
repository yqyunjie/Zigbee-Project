// Copyright 2007 - 2012 by Ember Corporation. All rights reserved.
// 
//

#include "../../include/af.h"
#include "../../util/common.h"
#include "11073-tunnel.h"


/** @brief Transfer A P D U
 *
 *
 * @param apdu   Ver.: always
 */
boolean emberAf11073ProtocolTunnelClusterTransferAPDUCallback(int8u* apdu) {
  return FALSE;
}

/** @brief Connect Request
 *
 *
 * @param connectControl   Ver.: always
 * @param idleTimeout   Ver.: always
 * @param managerTarget   Ver.: always
 * @param managerEndpoint   Ver.: always
 */
boolean emberAf11073ProtocolTunnelClusterConnectRequestCallback(int8u connectControl, 
                                                                int16u idleTimeout, 
                                                                int8u* managerTarget, 
                                                                int8u managerEndpoint) {
  boolean connected = FALSE;
  boolean preemptible = FALSE;
  EmberAfStatus status;
  
  // Check to see if we are already connected by looking at connected attribute
  status = emberAfReadServerAttribute(HC_11073_TUNNEL_ENDPOINT,
                            CLUSTER_ID_11073_TUNNEL,
                            ATTRIBUTE_11073_TUNNEL_CONNECTED,
                            &connected,
                            1);
 
  // if we are already connected send back connection status ALREADY_CONNECTED
  if (connected) {
    emberAfFillCommand11073ProtocolTunnelClusterConnectStatusNotification(
      EMBER_ZCL_11073_TUNNEL_CONNECTION_STATUS_ALREADY_CONNECTED);
    emberAfSendResponse();
    return TRUE;
  }
  
  // if not already connected copy attributes
  connected = TRUE;
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
    CLUSTER_ID_11073_TUNNEL,
    ATTRIBUTE_11073_TUNNEL_CONNECTED,
    &connected,
    ZCL_BOOLEAN_ATTRIBUTE_TYPE);

  preemptible = connectControl & 
    EMBER_ZCL_11073_CONNECT_REQUEST_CONNECT_CONTROL_PREEMPTIBLE;
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
    CLUSTER_ID_11073_TUNNEL,
    ATTRIBUTE_11073_TUNNEL_PREEMPTIBLE,
    &preemptible,
    ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
    CLUSTER_ID_11073_TUNNEL,
    ATTRIBUTE_11073_TUNNEL_IDLE_TIMEOUT,
    (int8u*)&idleTimeout,
    ZCL_INT16U_ATTRIBUTE_TYPE);
  
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
    CLUSTER_ID_11073_TUNNEL,
    ATTRIBUTE_11073_TUNNEL_MANAGER_TARGET,
    (int8u*)managerTarget,
    ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE);
    
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
      CLUSTER_ID_11073_TUNNEL,
      ATTRIBUTE_11073_TUNNEL_MANAGER_ENDPOINT,
      &managerEndpoint,
      ZCL_INT8U_ATTRIBUTE_TYPE);
    
  
  // if idle timer other than 0xffff, set timer to disconnect, reset timer when 
  // rx data
  
  // Generate conection status connected back to manager
  emberAfFillCommand11073ProtocolTunnelClusterConnectStatusNotification(
      EMBER_ZCL_11073_TUNNEL_CONNECTION_STATUS_CONNECTED);
  emberAfSendResponse();
  
  return TRUE;
}

/** @brief Disconnect Request
 *
 *
 * @param managerIEEEAddress   Ver.: always
 */
boolean emberAf11073ProtocolTunnelClusterDisconnectRequestCallback(int8u* managerIEEEAddress) {
  boolean connected = FALSE;
  EmberEUI64 currentManager;
  boolean preemptible;
  EmberAfStatus status;
  
  // check to see if already connected
  status = emberAfReadServerAttribute(HC_11073_TUNNEL_ENDPOINT,
                            CLUSTER_ID_11073_TUNNEL,
                            ATTRIBUTE_11073_TUNNEL_CONNECTED,
                            &connected,
                            1);
  
  // if not currently connected, generate connection status DISCONNECTED
  if (!connected) {
    emberAfFillCommand11073ProtocolTunnelClusterConnectStatusNotification(
      EMBER_ZCL_11073_TUNNEL_CONNECTION_STATUS_DISCONNECTED);
    emberAfSendResponse();
    return TRUE;
  }
  
  // if is connected, is ieee address same or is pre-emptible set to true? 
  status = emberAfReadServerAttribute(HC_11073_TUNNEL_ENDPOINT,
                            CLUSTER_ID_11073_TUNNEL,
                            ATTRIBUTE_11073_TUNNEL_PREEMPTIBLE,
                            &preemptible,
                            1);
  
  if(!preemptible) {
    status = emberAfReadServerAttribute(HC_11073_TUNNEL_ENDPOINT,
                              CLUSTER_ID_11073_TUNNEL,
                              ATTRIBUTE_11073_TUNNEL_MANAGER_TARGET,
                              (int8u*)&currentManager,
                              EUI64_SIZE);
    if (MEMCOMPARE(&currentManager, managerIEEEAddress, EUI64_SIZE) != 0) {
      emberAfFillCommand11073ProtocolTunnelClusterConnectStatusNotification(
        EMBER_ZCL_11073_TUNNEL_CONNECTION_STATUS_NOT_AUTHORIZED);
      emberAfSendResponse();
      return TRUE;
    }
  }
  
  // Set attribute to disconnected
  connected = FALSE;
  status = emberAfWriteServerAttribute(HC_11073_TUNNEL_ENDPOINT,
    CLUSTER_ID_11073_TUNNEL,
    ATTRIBUTE_11073_TUNNEL_CONNECTED,
    &connected,
    ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  
  
  // If it is authorized, then we can disconnect.Within 12 seconds device must send 
  // DISCONNECTED notification to the manager device. Connected attribute set to 
  // FALSE to manager.
  emberAfFillCommand11073ProtocolTunnelClusterConnectStatusNotification(
    EMBER_ZCL_11073_TUNNEL_CONNECTION_STATUS_DISCONNECTED);
  emberAfSendResponse();
  return TRUE;
  
  // Send another DISCONNECTED connection event to sender of message. (may be same
  // as manager, may be some other device).
  
  return FALSE;
}

/** @brief Connect Status Notification
 *
 *
 * @param connectStatus   Ver.: always
 */
boolean emberAf11073ProtocolTunnelClusterConnectStatusNotificationCallback(int8u connectStatus) {
  
  return FALSE;
}