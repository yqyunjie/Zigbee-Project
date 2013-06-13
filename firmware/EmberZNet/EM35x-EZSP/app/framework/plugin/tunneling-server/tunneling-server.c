// *******************************************************************
// * tunneling-server.c
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "tunneling-server.h"
#include "tunneling-server-callback.h"

#define UNUSED_ENDPOINT_ID 0xFF

// If addressIndex is EMBER_NULL_ADDRESS_TABLE_INDEX and clientEndpoint is
// UNUSED_ENDPOINT_ID, then the entry is unused and available for use by a new
// tunnel.  Occasionally, clientEndpoint will be UNUSED_ENDPOINT_ID, but
// addressIndex will contain a valid index.  This happens after a tunnel is
// removed but before the address table entry has been cleaned up.  There is a
// delay between closure and cleanup to allow the stack to continue using the
// address table entry to send messages to the client.
typedef struct {
  int8u   addressIndex;
  int8u   clientEndpoint;
  int8u   serverEndpoint;
  int8u   protocolId;
  int16u  manufacturerCode;
  boolean flowControlSupport;
  int32u  lastActive;
} EmAfTunnelingServerTunnel;

// this tells you both if the test protocol IS SUPPORTED and if
// the current protocol requested IS the test protocol
#ifdef EMBER_AF_PLUGIN_TUNNELING_SERVER_TEST_PROTOCOL_SUPPORT
  #define emAfIsTestProtocol(protocolId, manufacturerCode) \
    ((protocolId) == EMBER_ZCL_TUNNELING_PROTOCOL_ID_TEST  \
     && (manufacturerCode) == ZCL_TUNNELING_CLUSTER_UNUSED_MANUFACTURER_CODE)
  #define emAfTunnelIsTestProtocol(tunnel) \
    (emAfIsTestProtocol((tunnel)->protocolId, (tunnel)->manufacturerCode))
#else
  #define emAfIsTestProtocol(protocolId, manufacturerCode) (FALSE)
  #define emAfTunnelIsTestProtocol(tunnel) (FALSE)
#endif

// global for keeping track of test-harness behavior "busy status"
static boolean emberAfPluginTunnelingServerBusyStatus = FALSE;

static EmAfTunnelingServerTunnel tunnels[EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT];

static EmberAfStatus serverFindTunnel(int16u tunnelId,
                                      int8u addressIndex,
                                      int8u clientEndpoint,
                                      int8u serverEndpoint,
                                      EmAfTunnelingServerTunnel **tunnel);
static void closeInactiveTunnels(int8u endpoint);

void emberAfTunnelingClusterServerInitCallback(int8u endpoint)
{
  EmberAfStatus status;
  int16u closeTunnelTimeout = EMBER_AF_PLUGIN_TUNNELING_SERVER_CLOSE_TUNNEL_TIMEOUT;
  int8u i;

  for (i = 0; i < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT; i++) {
    tunnels[i].addressIndex = EMBER_NULL_ADDRESS_TABLE_INDEX;
    tunnels[i].clientEndpoint = UNUSED_ENDPOINT_ID;
  }

  status = emberAfWriteServerAttribute(endpoint,
                                       ZCL_TUNNELING_CLUSTER_ID,
                                       ZCL_CLOSE_TUNNEL_TIMEOUT_ATTRIBUTE_ID,
                                       (int8u *)&closeTunnelTimeout,
                                       ZCL_INT16U_ATTRIBUTE_TYPE);
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TUNNELING_CLUSTER)
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfTunnelingClusterPrintln("ERR: writing close tunnel timeout 0x%x",
                                   status);
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TUNNELING_CLUSTER)
}

void emberAfTunnelingClusterServerTickCallback(int8u endpoint)
{
  closeInactiveTunnels(endpoint);
}

void emberAfTunnelingClusterServerAttributeChangedCallback(int8u endpoint,
                                                           EmberAfAttributeId attributeId)
{
  if (attributeId == ZCL_CLOSE_TUNNEL_TIMEOUT_ATTRIBUTE_ID) {
    closeInactiveTunnels(endpoint);
  }
}

boolean emberAfTunnelingClusterRequestTunnelCallback(int8u protocolId,
                                                     int16u manufacturerCode,
                                                     int8u flowControlSupport,
                                                     int16u maximumIncomingTransferSize)
{
  int16u tunnelId = ZCL_TUNNELING_CLUSTER_INVALID_TUNNEL_ID;
  EmberAfTunnelingTunnelStatus status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_NO_MORE_TUNNEL_IDS;

  emberAfTunnelingClusterPrintln("RX: RequestTunnel 0x%x, 0x%2x, 0x%x, 0x%2x",
                                 protocolId,
                                 manufacturerCode,
                                 flowControlSupport,
                                 maximumIncomingTransferSize);

  if (!emAfIsTestProtocol(protocolId, manufacturerCode)
      && !emberAfPluginTunnelingServerIsProtocolSupportedCallback(protocolId,
                                                                  manufacturerCode)) {
    status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_PROTOCOL_NOT_SUPPORTED;
  } else if (flowControlSupport) {
    // TODO: Implement support for flow control.
    status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_FLOW_CONTROL_NOT_SUPPORTED;
  } else if (emberAfPluginTunnelingServerBusyStatus) {
    status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_BUSY;
  } else {
    int8u i;
    for (i = 0; i < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT; i++) {
      if (tunnels[i].addressIndex == EMBER_NULL_ADDRESS_TABLE_INDEX
          && tunnels[i].clientEndpoint == UNUSED_ENDPOINT_ID) {
        EmberEUI64 eui64;
        EmberNodeId client = emberAfCurrentCommand()->source;
        status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_BUSY;
        if (emberLookupEui64ByNodeId(client, eui64) == EMBER_SUCCESS) {
          tunnels[i].addressIndex = emberAfAddAddressTableEntry(eui64, client);
          if (tunnels[i].addressIndex != EMBER_NULL_ADDRESS_TABLE_INDEX) {
            tunnelId = i;
            tunnels[i].clientEndpoint = emberAfCurrentCommand()->apsFrame->sourceEndpoint;
            tunnels[i].serverEndpoint = emberAfCurrentCommand()->apsFrame->destinationEndpoint;
            tunnels[i].protocolId = protocolId;
            tunnels[i].manufacturerCode = manufacturerCode;
            tunnels[i].flowControlSupport = flowControlSupport;
            tunnels[i].lastActive = emberAfGetCurrentTime();
            status = EMBER_ZCL_TUNNELING_TUNNEL_STATUS_SUCCESS;
            // This will reschedule the tick that will timeout tunnels.
            closeInactiveTunnels(emberAfCurrentCommand()->apsFrame->destinationEndpoint);
          } else {
            emberAfTunnelingClusterPrintln("ERR: Could not create address"
                                           " table entry for node 0x%2x",
                                           client);
          }
        } else {
          emberAfTunnelingClusterPrintln("ERR: EUI64 for node 0x%2x"
                                         " is unknown",
                                         client);
        }
        break;
      }
    }
  }

  if (status == EMBER_ZCL_TUNNELING_TUNNEL_STATUS_SUCCESS) {
    emberAfPluginTunnelingServerTunnelOpenedCallback(tunnelId,
                                                     protocolId,
                                                     manufacturerCode,
                                                     flowControlSupport,
                                                     maximumIncomingTransferSize);
  }

  emberAfFillCommandTunnelingClusterRequestTunnelResponse(tunnelId,
                                                          status,
                                                          EMBER_AF_PLUGIN_TUNNELING_SERVER_MAXIMUM_INCOMING_TRANSFER_SIZE);
  emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_SOURCE_EUI64;
  emberAfSendResponse();
  return TRUE;
}

boolean emberAfTunnelingClusterCloseTunnelCallback(int16u tunnelId)
{
  EmAfTunnelingServerTunnel *tunnel;
  EmberAfStatus status;

  emberAfTunnelingClusterPrintln("RX: CloseTunnel 0x%2x", tunnelId);

  status = serverFindTunnel(tunnelId,
                            emberAfGetAddressIndex(),
                            emberAfCurrentCommand()->apsFrame->sourceEndpoint,
                            emberAfCurrentCommand()->apsFrame->destinationEndpoint,
                            &tunnel);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    // Mark the entry as unused and schedule a tick to clean up the address
    // table entry.  The delay before cleaning up the address table is to give
    // the stack some time to continue using it for sending the response to the
    // server.
    tunnel->clientEndpoint = UNUSED_ENDPOINT_ID;
    emberAfScheduleClusterTick(emberAfCurrentCommand()->apsFrame->destinationEndpoint,
                               ZCL_TUNNELING_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               MILLISECOND_TICKS_PER_SECOND,
                               EMBER_AF_OK_TO_HIBERNATE);
    emberAfPluginTunnelingServerTunnelClosedCallback(tunnelId,
                                                     CLOSE_INITIATED_BY_CLIENT);
  }

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfTunnelingClusterTransferDataClientToServerCallback(int16u tunnelId,
                                                                  int8u* data)
{
  EmAfTunnelingServerTunnel *tunnel;
  EmberAfStatus status;
  int16u dataLen = (emberAfCurrentCommand()->bufLen
                    - (emberAfCurrentCommand()->payloadStartIndex
                       + sizeof(tunnelId)));
  EmberAfTunnelingTransferDataStatus tunnelError = EMBER_ZCL_TUNNELING_TRANSFER_DATA_STATUS_DATA_OVERFLOW;

  emberAfTunnelingClusterPrint("RX: TransferData 0x%2x, [", tunnelId);
  emberAfTunnelingClusterPrintBuffer(data, dataLen, FALSE);
  emberAfTunnelingClusterPrintln("]");

  status = serverFindTunnel(tunnelId,
                            emberAfGetAddressIndex(),
                            emberAfCurrentCommand()->apsFrame->sourceEndpoint,
                            emberAfCurrentCommand()->apsFrame->destinationEndpoint,
                            &tunnel);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    if (dataLen <= EMBER_AF_PLUGIN_TUNNELING_SERVER_MAXIMUM_INCOMING_TRANSFER_SIZE) {
      tunnel->lastActive = emberAfGetCurrentTime();

      // If this is the test protocol (and the option for test protocol support
      // is enabled), just turn the data around without notifying the
      // application.  Otherwise, everything goes to the application via a
      // callback.
      if (emAfTunnelIsTestProtocol(tunnel)) {
        emberAfPluginTunnelingServerTransferData(tunnelId, data, dataLen);
      } else {
        emberAfPluginTunnelingServerDataReceivedCallback(tunnelId, data, dataLen);
      }
      emberAfSendImmediateDefaultResponse(status);
      return TRUE;
    }
    // else
    //  tunnelError code already set (overflow)

  } else {
    tunnelError = (status == EMBER_ZCL_STATUS_NOT_AUTHORIZED
                   ? EMBER_ZCL_TUNNELING_TRANSFER_DATA_STATUS_WRONG_DEVICE
                   : EMBER_ZCL_TUNNELING_TRANSFER_DATA_STATUS_NO_SUCH_TUNNEL);
  }

  // Error
  emberAfFillCommandTunnelingClusterTransferDataErrorServerToClient(tunnelId,
                                                                    tunnelError);
  emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_SOURCE_EUI64;
  emberAfSendResponse();
  return TRUE;
}

boolean emberAfTunnelingClusterTransferDataErrorClientToServerCallback(int16u tunnelId,
                                                                       int8u transferDataStatus)
{
  EmAfTunnelingServerTunnel *tunnel;
  EmberAfStatus status;

  emberAfTunnelingClusterPrintln("RX: TransferDataError 0x%2x, 0x%x",
                                 tunnelId,
                                 transferDataStatus);

  status = serverFindTunnel(tunnelId,
                            emberAfGetAddressIndex(),
                            emberAfCurrentCommand()->apsFrame->sourceEndpoint,
                            emberAfCurrentCommand()->apsFrame->destinationEndpoint,
                            &tunnel);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPluginTunnelingServerDataErrorCallback(tunnelId,
                                                  (EmberAfTunnelingTransferDataStatus)transferDataStatus);
  }

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

EmberAfStatus emberAfPluginTunnelingServerTransferData(int16u tunnelId,
                                                       int8u *data,
                                                       int16u dataLen)
{
  if (tunnelId < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT
      && tunnels[tunnelId].clientEndpoint != UNUSED_ENDPOINT_ID) {
    EmberStatus status;
    emberAfFillCommandTunnelingClusterTransferDataServerToClient(tunnelId,
                                                                 data,
                                                                 dataLen);
    emberAfSetCommandEndpoints(tunnels[tunnelId].serverEndpoint,
                               tunnels[tunnelId].clientEndpoint);
    emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_SOURCE_EUI64;
    status = emberAfSendCommandUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                                       tunnels[tunnelId].addressIndex);
    tunnels[tunnelId].lastActive = emberAfGetCurrentTime();
    return (status == EMBER_SUCCESS
            ? EMBER_ZCL_STATUS_SUCCESS
            : EMBER_ZCL_STATUS_FAILURE);
  }
  return EMBER_ZCL_STATUS_NOT_FOUND;
}

static EmberAfStatus serverFindTunnel(int16u tunnelId,
                                      int8u addressIndex,
                                      int8u clientEndpoint,
                                      int8u serverEndpoint,
                                      EmAfTunnelingServerTunnel **tunnel)
{
  if (tunnelId < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT
      && tunnels[tunnelId].clientEndpoint != UNUSED_ENDPOINT_ID) {
    if (tunnels[tunnelId].addressIndex == addressIndex
        && tunnels[tunnelId].clientEndpoint == clientEndpoint
        && tunnels[tunnelId].serverEndpoint == serverEndpoint) {
      *tunnel = &tunnels[tunnelId];
      return EMBER_ZCL_STATUS_SUCCESS;
    } else {
      return EMBER_ZCL_STATUS_NOT_AUTHORIZED;
    }
  }
  return EMBER_ZCL_STATUS_NOT_FOUND;
}

static void closeInactiveTunnels(int8u endpoint)
{
  EmberAfStatus status;
  int32u currentTime = emberAfGetCurrentTime();
  int32u delay = MAX_INT32U_VALUE;
  int16u closeTunnelTimeout;
  int8u i;

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_TUNNELING_CLUSTER_ID,
                                      ZCL_CLOSE_TUNNEL_TIMEOUT_ATTRIBUTE_ID,
                                      (int8u *)&closeTunnelTimeout,
                                      sizeof(closeTunnelTimeout));
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfTunnelingClusterPrintln("ERR: reading close tunnel timeout 0x%x",
                                   status);
    return;
  }

  // Every time the tick fires, we search the table for inactive tunnels that
  // should be closed and unused entries that still have an address table
  // index.  The unused tunnels have been closed, but the address table entry
  // was not immediately removed so the stack could continue using it.  By this
  // point, we've given the stack a fair shot to use it, so now remove the
  // address table entry.  While looking through the tunnels, the time to next
  // tick is calculated based on how recently the tunnels were used or by the
  // need to clean up newly unused tunnels.
  for (i = 0; i < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT; i++) {
    if (tunnels[i].serverEndpoint == endpoint) {
      if (tunnels[i].clientEndpoint != UNUSED_ENDPOINT_ID) {
        int32u elapsed = currentTime - tunnels[i].lastActive;
        if (closeTunnelTimeout <= elapsed) {
          // If we are closing an inactive tunnel and will not send a closure
          // notification, we can immediately remove the address table entry
          // for the client because it will no longer be used.  Otherwise, we
          // need to schedule a tick to clean up the address table entry so we
          // give the stack a chance to continue using it for sending the
          // notification.
#ifdef EMBER_AF_PLUGIN_TUNNELING_SERVER_CLOSURE_NOTIFICATION_SUPPORT
          emberAfFillCommandTunnelingClusterTunnelClosureNotification(i);
          emberAfSetCommandEndpoints(tunnels[i].serverEndpoint,
                                     tunnels[i].clientEndpoint);
          emberAfGetCommandApsFrame()->options |= EMBER_APS_OPTION_SOURCE_EUI64;
          emberAfSendCommandUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                                    tunnels[i].addressIndex);
          delay = 1;
#else
          emberAfRemoveAddressTableEntry(tunnels[i].addressIndex);
          tunnels[i].addressIndex = EMBER_NULL_ADDRESS_TABLE_INDEX;
#endif
          tunnels[i].clientEndpoint = UNUSED_ENDPOINT_ID;
          emberAfPluginTunnelingServerTunnelClosedCallback(i,
                                                           CLOSE_INITIATED_BY_SERVER);
        } else {
          int32u remaining = closeTunnelTimeout - elapsed;
          if (remaining < delay) {
            delay = remaining;
          }
        }
      } else if (tunnels[i].addressIndex != EMBER_NULL_ADDRESS_TABLE_INDEX) {
        emberAfRemoveAddressTableEntry(tunnels[i].addressIndex);
        tunnels[i].addressIndex = EMBER_NULL_ADDRESS_TABLE_INDEX;
      }
    }
  }

  if (delay != MAX_INT32U_VALUE) {
    emberAfScheduleClusterTick(endpoint,
                               ZCL_TUNNELING_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               delay * MILLISECOND_TICKS_PER_SECOND,
                               EMBER_AF_OK_TO_HIBERNATE);
  }
}

void emberAfPluginTunnelingServerToggleBusyCommand(void)
{
  emberAfTunnelingClusterPrintln("");
  if (emberAfPluginTunnelingServerBusyStatus) {
    emberAfPluginTunnelingServerBusyStatus = FALSE;
    emberAfTunnelingClusterPrintln("  NOTE: current status is NOT BUSY (tunneling works)");
  } else {
    emberAfPluginTunnelingServerBusyStatus = TRUE;
    emberAfTunnelingClusterPrintln("  NOTE: current status is BUSY (tunneling won't work)");
  }
  emberAfTunnelingClusterPrintln("");
  emberAfTunnelingClusterFlush();
}

void emAfPluginTunnelingServerPrint(void)
{
  int32u currentTime = emberAfGetCurrentTime();
  int8u i;
  emberAfTunnelingClusterPrintln("");
  emberAfTunnelingClusterPrintln("#   client              cep  sep  tid    pid  mfg    age");
  emberAfTunnelingClusterFlush();
  for (i = 0; i < EMBER_AF_PLUGIN_TUNNELING_SERVER_TUNNEL_LIMIT; i++) {
    emberAfTunnelingClusterPrint("%x: ", i);
    if (tunnels[i].clientEndpoint != UNUSED_ENDPOINT_ID) {
      EmberEUI64 eui64;
      emberGetAddressTableRemoteEui64(tunnels[i].addressIndex, eui64);
      emberAfTunnelingClusterDebugExec(emberAfPrintBigEndianEui64(eui64));
      emberAfTunnelingClusterPrint(" 0x%x 0x%x 0x%2x",
                                   tunnels[i].clientEndpoint,
                                   tunnels[i].serverEndpoint,
                                   i);
      emberAfTunnelingClusterFlush();
      emberAfTunnelingClusterPrint(" 0x%x 0x%2x 0x%4x",
                                   tunnels[i].protocolId,
                                   tunnels[i].manufacturerCode,
                                   currentTime - tunnels[i].lastActive);
      emberAfTunnelingClusterFlush();
    }
    emberAfTunnelingClusterPrintln("");
  }
}
