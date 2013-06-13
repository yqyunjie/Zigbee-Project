// *******************************************************************
// * tunneling-client.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#define EMBER_AF_PLUGIN_TUNNELING_CLIENT_NULL_INDEX 0xFF

/**
 * @brief Request a Tunneling cluster tunnel with a server.
 *
 * This function can be used to request a tunnel with a server.  The Tunneling
 * client plugin will look up the long address of the server (using discovery,
 * if necessary), establish a link key with the server, and create an address
 * table entry for the server before sending the request.  All future
 * communication using the tunnel will be sent using the address table entry.
 * The plugin will call ::emberAfPluginTunnelingClientTunnelOpenedCallback with
 * the status of the request.
 *
 * @param server The network address of the server to which the request will be
 * sent.
 * @param clientEndpoint The local endpoint from which the request will be
 * sent.
 * @param serverEndpoint The remote endpoint to which the request will be sent.
 * @param protocolId The protocol id of the requested tunnel.
 * @param manufacturerCode The manufacturer code of the requested tunnel.
 * @param flowControlSupport TRUE if flow control support is requested or FALSE
 * if not.  Note: flow control is not currently supported by the Tunneling
 * client or server plugins.
 * @return ::EMBER_AF_PLUGIN_TUNNELING_CLIENT_SUCCESS if the request is in
 * process or another ::EmberAfPluginTunnelingClientStatus otherwise.
 */
EmberAfPluginTunnelingClientStatus emberAfPluginTunnelingClientRequestTunnel(EmberNodeId server,
                                                                             int8u clientEndpoint,
                                                                             int8u serverEndpoint,
                                                                             int8u protocolId,
                                                                             int16u manufacturerCode,
                                                                             boolean flowControlSupport);

/**
 * @brief Transfer data to a server through a Tunneling cluster tunnel.
 *
 * This function can be used to transfer data to a server through a tunnel. The
 * Tunneling client plugin will send the data to the endpoint on the node that
 * is managing the given tunnel.
 *
 * @param tunnelIndex The index of the tunnel through which to send the data.
 * @param data Buffer containing the raw octets of the data.
 * @param dataLen The length in octets of the data.
 * @return ::EMBER_ZCL_STATUS_SUCCESS if the data was sent,
 * ::EMBER_ZCL_STATUS_FAILURE if an error occurred, or
 * ::EMBER_ZCL_STATUS_NOT_FOUND if the tunnel does not exist.
 */
EmberAfStatus emberAfPluginTunnelingClientTransferData(int8u tunnelIndex,
                                                       int8u *data,
                                                       int16u dataLen);

/**
 * @brief Close a Tunneling cluster tunnel.
 *
 * This function can be used to close a tunnel.  The Tunneling client plugin
 * will send the close command to the endpoint on the node that is managing the
 * given tunnel.
 *
 * @param tunnelIndex The index of the tunnel to close.
 * @return ::EMBER_ZCL_STATUS_SUCCESS if the close request was sent,
 * ::EMBER_ZCL_STATUS_FAILURE if an error occurred, or
 * ::EMBER_ZCL_STATUS_NOT_FOUND if the tunnel does not exist.
 */
EmberAfStatus emberAfPluginTunnelingClientCloseTunnel(int8u tunnelIndex);

void emAfPluginTunnelingClientPrint(void);
