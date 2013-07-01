// File: ezsp-enum.h
//
// *** Generated file. Do not edit! ***
// 
// Description: Enumerations for EZSP.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_ENUM_H__
#define __EZSP_ENUM_H__

//------------------------------------------------------------------------------
// Identifies a configuration value.
typedef int8u EzspConfigId;

enum {
  // The number of packet buffers available to the stack.
  EZSP_CONFIG_PACKET_BUFFER_COUNT               = 0x01,
  // The maximum number of router neighbors the stack can keep track of. A
  // neighbor is a node within radio range.
  EZSP_CONFIG_NEIGHBOR_TABLE_SIZE               = 0x02,
  // The maximum number of APS retried messages the stack can be transmitting at
  // any time.
  EZSP_CONFIG_APS_UNICAST_MESSAGE_COUNT         = 0x03,
  // The maximum number of non-volatile bindings supported by the stack.
  EZSP_CONFIG_BINDING_TABLE_SIZE                = 0x04,
  // The maximum number of EUI64 to network address associations that the stack
  // can maintain.
  EZSP_CONFIG_ADDRESS_TABLE_SIZE                = 0x05,
  // The maximum number of multicast groups that the device may be a member of.
  EZSP_CONFIG_MULTICAST_TABLE_SIZE              = 0x06,
  // The maximum number of destinations to which a node can route messages. This
  // includes both messages originating at this node and those relayed for
  // others.
  EZSP_CONFIG_ROUTE_TABLE_SIZE                  = 0x07,
  // The number of simultaneous route discoveries that a node will support.
  EZSP_CONFIG_DISCOVERY_TABLE_SIZE              = 0x08,
  // The size of the alarm broadcast buffer.
  EZSP_CONFIG_BROADCAST_ALARM_DATA_SIZE         = 0x09,
  // The size of the unicast alarm buffers allocated for end device children.
  EZSP_CONFIG_UNICAST_ALARM_DATA_SIZE           = 0x0A,
  // Specifies the stack profile.
  EZSP_CONFIG_STACK_PROFILE                     = 0x0C,
  // The security level used for security at the MAC and network layers. The
  // supported values are 0 (no security) and 5 (payload is encrypted and a
  // four-byte MIC is used for authentication).
  EZSP_CONFIG_SECURITY_LEVEL                    = 0x0D,
  // The maximum number of hops for a message.
  EZSP_CONFIG_MAX_HOPS                          = 0x10,
  // The maximum number of end device children that a router will support.
  EZSP_CONFIG_MAX_END_DEVICE_CHILDREN           = 0x11,
  // The maximum amount of time that the MAC will hold a message for indirect
  // transmission to a child.
  EZSP_CONFIG_INDIRECT_TRANSMISSION_TIMEOUT     = 0x12,
  // The maximum amount of time that an end device child can wait between polls.
  // If no poll is heard within this timeout, then the parent removes the end
  // device from its tables.
  EZSP_CONFIG_END_DEVICE_POLL_TIMEOUT           = 0x13,
  // The maximum amount of time that a mobile node can wait between polls. If no
  // poll is heard within this timeout, then the parent removes the mobile node
  // from its tables.
  EZSP_CONFIG_MOBILE_NODE_POLL_TIMEOUT          = 0x14,
  // The number of child table entries reserved for use only by mobile nodes.
  EZSP_CONFIG_RESERVED_MOBILE_CHILD_ENTRIES     = 0x15,
  // The amount of RAM available for use by the Host.
  EZSP_CONFIG_HOST_RAM                          = 0x16,
  // Enables boost power mode and/or the alternate transmitter output.
  EZSP_CONFIG_TX_POWER_MODE                     = 0x17,
  // 0: Allow this node to relay messages. 1: Prevent this node from relaying
  // messages.
  EZSP_CONFIG_DISABLE_RELAY                     = 0x18,
  // The maximum number of EUI64 to network address associations that the Trust
  // Center can maintain.
  EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE   = 0x19,
  // The size of the source route table.
  EZSP_CONFIG_SOURCE_ROUTE_TABLE_SIZE           = 0x1A,
  // The units used for timing out end devices on their parents.
  EZSP_CONFIG_END_DEVICE_POLL_TIMEOUT_SHIFT     = 0x1B,
  // The number of blocks of a fragmented message that can be sent in a single
  // window.
  EZSP_CONFIG_FRAGMENT_WINDOW_SIZE              = 0x1C,
  // The time the stack will wait (in milliseconds) between sending blocks of a
  // fragmented message.
  EZSP_CONFIG_FRAGMENT_DELAY_MS                 = 0x1D,
  // The size of the Key Table used for storing individual link keys (if the
  // device is a Trust Center) or Application Link Keys (if the device is a
  // normal node).
  EZSP_CONFIG_KEY_TABLE_SIZE                    = 0x1E,
  // The APS ACK timeout value. The stack waits this amount of time between
  // resends of APS retried messages.
  EZSP_CONFIG_APS_ACK_TIMEOUT                   = 0x1F,
  // The duration of an active scan, in the units used by the 15.4 scan
  // parameter (((1 << duration) + 1) * 15ms). This also controls the jitter
  // used when responding to a beacon request.
  EZSP_CONFIG_ACTIVE_SCAN_DURATION              = 0x20,
  // The time the coordinator will wait (in seconds) for a second end device
  // bind request to arrive.
  EZSP_CONFIG_END_DEVICE_BIND_TIMEOUT           = 0x21,
  // The number of PAN id conflict reports that must be received by the network
  // manager within one minute to trigger a PAN id change.
  EZSP_CONFIG_PAN_ID_CONFLICT_REPORT_THRESHOLD  = 0x22,
  // The timeout value in minutes for how long the Trust Center or a normal node
  // waits for the Zigbee Request Key to complete. On the Trust Center this
  // controls whether or not the device buffers the request, waiting for a
  // matching pair of Zigbee Request Key. If the value is non-zero, the Trust
  // Center buffers and waits for that amount of time. If the value is zero, the
  // Trust Center does not buffer the request and immediately responds to the
  // request. Zero is the most compliant behavior.
  EZSP_CONFIG_REQUEST_KEY_TIMEOUT               = 0x24,
  // With some board layouts, the EM250 and EM260 are susceptible to a dual
  // channel issue in which packets from 12 channels above or below can
  // sometimes be heard faintly. This affects channels 11 - 14 and 23 - 26.
  // Hardware reference designs EM260_REF_DES_CER, version C1 and
  // EM260_REF_DES_LAT, version A1 solve the problem.  Setting this
  // configuration value variable to TRUE enables a software workaround to the
  // dual channel issue which can be used with vulnerable boards. After
  // scanForJoinableNetwork discovers a network on one of the susceptible
  // channels, the channel number that differs by 12 is also scanned. If the
  // same network can be heard there, the true channel is determined by
  // comparing the link quality of the received beacons. The default value of
  // the configuratin value is TRUE for the EM250 and EM260. It is not used on
  // other platforms.
  EZSP_CONFIG_ENABLE_DUAL_CHANNEL_SCAN          = 0x25,
  // Set this value to 1 in order to receive supported ZDO request messages via
  // the incomingMessageHandler callback. A supported ZDO request is one that is
  // handled by the EmberZNet stack. The stack will continue to handle the
  // request and send the appropriate ZDO response even if this configuration
  // option is enabled.
  EZSP_CONFIG_APPLICATION_RECEIVES_SUPPORTED_ZDO_REQUESTS = 0x26,
  // Set this value to 1 in order to receive unsupported ZDO request messages
  // via the incomingMessageHandler callback. An unsupported ZDO request is one
  // that is not handled by the EmberZNet stack, other than to send a 'not
  // supported' ZDO response. If this configuration option is enabled, the stack
  // will no longer send any ZDO response, and it is the application's
  // responsibility to do so. To see if a response is required, the application
  // must check the APS options bitfield within the incomingMessageHandler
  // callback to see if the EMBER_APS_OPTION_ZDO_RESPONSE_REQUIRED flag is set.
  EZSP_CONFIG_APPLICATION_HANDLES_UNSUPPORTED_ZDO_REQUESTS = 0x27,
  // Set this value to 1 in order to receive the following ZDO request messages
  // via the incomingMessageHandler callback: SIMPLE_DESCRIPTOR_REQUEST,
  // MATCH_DESCRIPTORS_REQUEST, and ACTIVE_ENDPOINTS_REQUEST. If this
  // configuration option is enabled, the stack will no longer send any ZDO
  // response, and it is the application's responsibility to do so. To see if a
  // response is required, the application must check the APS options bitfield
  // within the incomingMessageHandler callback to see if the
  // EMBER_APS_OPTION_ZDO_RESPONSE_REQUIRED flag is set.
  EZSP_CONFIG_APPLICATION_HANDLES_ENDPOINT_ZDO_REQUESTS = 0x28,
  // This value indicates the size of the runtime modifiable certificate table.
  // Normally certificates are stored in MFG tokens but this table can be used
  // to field upgrade devices with new Smart Energy certificates. This value
  // cannot be set, it can only be queried.
  EZSP_CONFIG_CERTIFICATE_TABLE_SIZE            = 0x29
};

//------------------------------------------------------------------------------
// Identifies a value.
typedef int8u EzspValueId;

enum {
  // The contents of the node data stack token.
  EZSP_VALUE_TOKEN_STACK_NODE_DATA              = 0x00,
  // The types of MAC passthrough messages that the host wishes to receive.
  EZSP_VALUE_MAC_PASSTHROUGH_FLAGS              = 0x01,
  // The source address used to filter legacy EmberNet messages when the
  // EMBER_MAC_PASSTHROUGH_EMBERNET_SOURCE flag is set in
  // EZSP_VALUE_MAC_PASSTHROUGH_FLAGS.
  EZSP_VALUE_EMBERNET_PASSTHROUGH_SOURCE_ADDRESS = 0x02
};

//------------------------------------------------------------------------------
// Identifies a policy.
typedef int8u EzspPolicyId;

enum {
  // Controls trust center behavior.
  EZSP_TRUST_CENTER_POLICY                      = 0x00,
  // Controls how external binding modification requests are handled.
  EZSP_BINDING_MODIFICATION_POLICY              = 0x01,
  // Controls whether the Host supplies unicast replies.
  EZSP_UNICAST_REPLIES_POLICY                   = 0x02,
  // Controls whether pollHandler callbacks are generated.
  EZSP_POLL_HANDLER_POLICY                      = 0x03,
  // Controls whether the message contents are included in the
  // messageSentHandler callback.
  EZSP_MESSAGE_CONTENTS_IN_CALLBACK_POLICY      = 0x04,
  // Controls whether the Trust Center will respond to Trust Center link key
  // requests.
  EZSP_TC_KEY_REQUEST_POLICY                    = 0x05,
  // Controls whether the Trust Center will respond to application link key
  // requests.
  EZSP_APP_KEY_REQUEST_POLICY                   = 0x06
};

//------------------------------------------------------------------------------
// Identifies a policy decision.
typedef int8u EzspDecisionId;

enum {
  // Send the network key in the clear to all joining and rejoining devices.
  EZSP_ALLOW_JOINS                              = 0x00,
  // Send the network key in the clear to all joining devices. Rejoining devices
  // are sent the network key encrypted with their trust center link key. The
  // trust center and any rejoining device are assumed to share a link key,
  // either preconfigured or obtained under a previous policy.
  EZSP_ALLOW_JOINS_REJOINS_HAVE_LINK_KEY        = 0x04,
  // Send the network key encrypted with the joining or rejoining device's trust
  // center link key. The trust center and any joining or rejoining device are
  // assumed to share a link key, either preconfigured or obtained under a
  // previous policy. This is the default value for the
  // EZSP_TRUST_CENTER_POLICY.
  EZSP_ALLOW_PRECONFIGURED_KEY_JOINS            = 0x01,
  // Send the network key encrypted with the rejoining device's trust center
  // link key. The trust center and any rejoining device are assumed to share a
  // link key, either preconfigured or obtained under a previous policy. No new
  // devices are allowed to join.
  EZSP_ALLOW_REJOINS_ONLY                       = 0x02,
  // Reject all unsecured join and rejoin attempts.
  EZSP_DISALLOW_ALL_JOINS_AND_REJOINS           = 0x03,
  // EZSP_BINDING_MODIFICATION_POLICY default decision. Do not allow the local
  // binding table to be changed by remote nodes.
  EZSP_DISALLOW_BINDING_MODIFICATION            = 0x10,
  // EZSP_BINDING_MODIFICATION_POLICY decision. Allow remote nodes to change the
  // local binding table.
  EZSP_ALLOW_BINDING_MODIFICATION               = 0x11,
  // EZSP_UNICAST_REPLIES_POLICY default decision. The EM260 will automatically
  // send an empty reply (containing no payload) for every unicast received.
  EZSP_HOST_WILL_NOT_SUPPLY_REPLY               = 0x20,
  // EZSP_UNICAST_REPLIES_POLICY decision. The EM260 will only send a reply if
  // it receives a sendReply command from the Host.
  EZSP_HOST_WILL_SUPPLY_REPLY                   = 0x21,
  // EZSP_POLL_HANDLER_POLICY default decision. Do not inform the Host when a
  // child polls.
  EZSP_POLL_HANDLER_IGNORE                      = 0x30,
  // EZSP_POLL_HANDLER_POLICY decision. Generate a pollHandler callback when a
  // child polls.
  EZSP_POLL_HANDLER_CALLBACK                    = 0x31,
  // EZSP_MESSAGE_CONTENTS_IN_CALLBACK_POLICY default decision. Include only the
  // message tag in the messageSentHandler callback.
  EZSP_MESSAGE_TAG_ONLY_IN_CALLBACK             = 0x40,
  // EZSP_MESSAGE_CONTENTS_IN_CALLBACK_POLICY decision. Include both the message
  // tag and the message contents in the messageSentHandler callback.
  EZSP_MESSAGE_TAG_AND_CONTENTS_IN_CALLBACK     = 0x41,
  // EZSP_TC_KEY_REQUEST_POLICY decision. When the Trust Center receives a
  // request for a Trust Center link key, it will be ignored.
  EZSP_DENY_TC_KEY_REQUESTS                     = 0x50,
  // EZSP_TC_KEY_REQUEST_POLICY decision. When the Trust Center receives a
  // request for a Trust Center link key, it will reply to it with the
  // corresponding key.
  EZSP_ALLOW_TC_KEY_REQUESTS                    = 0x51,
  // EZSP_APP_KEY_REQUEST_POLICY decision. When the Trust Center receives a
  // request for an application link key, it will be ignored.
  EZSP_DENY_APP_KEY_REQUESTS                    = 0x60,
  // EZSP_APP_KEY_REQUEST_POLICY decision. When the Trust Center receives a
  // request for an application link key, it will randomly generate a key and
  // send it to both partners.
  EZSP_ALLOW_APP_KEY_REQUESTS                   = 0x61
};

//------------------------------------------------------------------------------
// Manufacturing token IDs used by ezspGetMfgToken().
typedef int8u EzspMfgTokenId;

enum {
  // Custom version (2 bytes).
  EZSP_MFG_CUSTOM_VERSION                       = 0x00,
  // Manufacturing string (16 bytes).
  EZSP_MFG_STRING                               = 0x01,
  // Board name (16 bytes).
  EZSP_MFG_BOARD_NAME                           = 0x02,
  // Manufacturing ID (2 bytes).
  EZSP_MFG_MANUF_ID                             = 0x03,
  // Radio configuration (2 bytes).
  EZSP_MFG_PHY_CONFIG                           = 0x04,
  // Bootload AES key (16 bytes).
  EZSP_MFG_BOOTLOAD_AES_KEY                     = 0x05,
  // ASH configuration (40 bytes).
  EZSP_MFG_ASH_CONFIG                           = 0x06,
  // EZSP storage (8 bytes).
  EZSP_MFG_EZSP_STORAGE                         = 0x07,
  // Radio calibration data (64 bytes). 4 bytes (VCO at LNA, mod DAC, filter,
  // LNA) are stored for each of the 16 channels. This token is not stored in
  // the Flash Information Area. It is updated by the stack each time a
  // calibration is performed.
  EZSP_STACK_CAL_DATA                           = 0x08,
  // Certificate Based Key Exchange (CBKE) data (92 bytes).
  EZSP_MFG_CBKE_DATA                            = 0x09,
  // Installation code (20 bytes).
  EZSP_MFG_INSTALLATION_CODE                    = 0x0A
};

//------------------------------------------------------------------------------
// Status values used by EZSP.
typedef int8u EzspStatus;

enum {
  // Success.
  EZSP_SUCCESS                                  = 0x00,
  // Fatal error.
  EZSP_SPI_ERR_FATAL                            = 0x10,
  // The Response frame of the current transaction indicates the EM260 has
  // reset.
  EZSP_SPI_ERR_EM260_RESET                      = 0x11,
  // The EM260 is reporting that the Command frame of the current transaction is
  // oversized (the length byte is too large).
  EZSP_SPI_ERR_OVERSIZED_EZSP_FRAME             = 0x12,
  // The Response frame of the current transaction indicates the previous
  // transaction was aborted (nSSEL deasserted too soon).
  EZSP_SPI_ERR_ABORTED_TRANSACTION              = 0x13,
  // The Response frame of the current transaction indicates the frame
  // terminator is missing from the Command frame.
  EZSP_SPI_ERR_MISSING_FRAME_TERMINATOR         = 0x14,
  // The EM260 has not provided a Response within the time limit defined by
  // WAIT_SECTION_TIMEOUT.
  EZSP_SPI_ERR_WAIT_SECTION_TIMEOUT             = 0x15,
  // The Response frame from the EM260 is missing the frame terminator.
  EZSP_SPI_ERR_NO_FRAME_TERMINATOR              = 0x16,
  // The Host attempted to send an oversized Command (the length byte is too
  // large) and the AVR's spi-protocol.c blocked the transmission.
  EZSP_SPI_ERR_EZSP_COMMAND_OVERSIZED           = 0x17,
  // The EM260 attempted to send an oversized Response (the length byte is too
  // large) and the AVR's spi-protocol.c blocked the reception.
  EZSP_SPI_ERR_EZSP_RESPONSE_OVERSIZED          = 0x18,
  // The Host has sent the Command and is still waiting for the EM260 to send a
  // Response.
  EZSP_SPI_WAITING_FOR_RESPONSE                 = 0x19,
  // The EM260 has not asserted nHOST_INT within the time limit defined by
  // WAKE_HANDSHAKE_TIMEOUT.
  EZSP_SPI_ERR_HANDSHAKE_TIMEOUT                = 0x1A,
  // The EM260 has not asserted nHOST_INT after an EM260 reset within the time
  // limit defined by STARTUP_TIMEOUT.
  EZSP_SPI_ERR_STARTUP_TIMEOUT                  = 0x1B,
  // The Host attempted to verify the SPI Protocol activity and version number,
  // and the verification failed.
  EZSP_SPI_ERR_STARTUP_FAIL                     = 0x1C,
  // The Host has sent a command with a SPI Byte that is unsupported by the
  // current mode the EM260 is operating in.
  EZSP_SPI_ERR_UNSUPPORTED_SPI_COMMAND          = 0x1D,
  // Operation not yet complete.
  EZSP_ASH_IN_PROGRESS                          = 0x20,
  // Fatal error detected by host.
  EZSP_ASH_HOST_FATAL_ERROR                     = 0x21,
  // Fatal error detected by NCP.
  EZSP_ASH_NCP_FATAL_ERROR                      = 0x22,
  // Tried to send DATA frame too long.
  EZSP_ASH_DATA_FRAME_TOO_LONG                  = 0x23,
  // Tried to send DATA frame too short.
  EZSP_ASH_DATA_FRAME_TOO_SHORT                 = 0x24,
  // No space for tx'ed DATA frame.
  EZSP_ASH_NO_TX_SPACE                          = 0x25,
  // No space for rec'd DATA frame.
  EZSP_ASH_NO_RX_SPACE                          = 0x26,
  // No receive data available.
  EZSP_ASH_NO_RX_DATA                           = 0x27,
  // Not in Connected state.
  EZSP_ASH_NOT_CONNECTED                        = 0x28,
  // The EM260 received a command before the EZSP version had been set.
  EZSP_ERROR_VERSION_NOT_SET                    = 0x30,
  // The EM260 received a command containing an unsupported frame ID.
  EZSP_ERROR_INVALID_FRAME_ID                   = 0x31,
  // The direction flag in the frame control field was incorrect.
  EZSP_ERROR_WRONG_DIRECTION                    = 0x32,
  // The truncated flag in the frame control field was set, indicating there was
  // not enough memory available to complete the response or that the response
  // would have exceeded the maximum EZSP frame length.
  EZSP_ERROR_TRUNCATED                          = 0x33,
  // The overflow flag in the frame control field was set, indicating one or
  // more callbacks occurred since the previous response and there was not
  // enough memory available to report them to the Host.
  EZSP_ERROR_OVERFLOW                           = 0x34,
  // Insufficient memory was available.
  EZSP_ERROR_OUT_OF_MEMORY                      = 0x35,
  // The value was out of bounds.
  EZSP_ERROR_INVALID_VALUE                      = 0x36,
  // The configuration id was not recognized.
  EZSP_ERROR_INVALID_ID                         = 0x37,
  // Configuration values can no longer be modified.
  EZSP_ERROR_INVALID_CALL                       = 0x38,
  // The EM260 failed to respond to a command.
  EZSP_ERROR_NO_RESPONSE                        = 0x39,
  // The length of the command exceeded the maximum EZSP frame length.
  EZSP_ERROR_COMMAND_TOO_LONG                   = 0x40,
  // The UART receive queue was full causing a callback response to be dropped.
  EZSP_ERROR_QUEUE_FULL                         = 0x41,
  // Incompatible ASH version
  EZSP_ASH_ERROR_VERSION                        = 0x50,
  // Exceeded max ACK timeouts
  EZSP_ASH_ERROR_TIMEOUTS                       = 0x51,
  // Timed out waiting for RSTACK
  EZSP_ASH_ERROR_RESET_FAIL                     = 0x52,
  // Unexpected ncp reset
  EZSP_ASH_ERROR_NCP_RESET                      = 0x53,
  // Serial port initialization failed
  EZSP_ASH_ERROR_SERIAL_INIT                    = 0x54,
  // Invalid ncp processor type
  EZSP_ASH_ERROR_NCP_TYPE                       = 0x55,
  // Invalid ncp reset method
  EZSP_ASH_ERROR_RESET_METHOD                   = 0x56,
  // XON/XOFF not supported by host driver
  EZSP_ASH_ERROR_XON_XOFF                       = 0x57,
  // ASH protocol started
  EZSP_ASH_STARTED                              = 0x70,
  // ASH protocol connected
  EZSP_ASH_CONNECTED                            = 0x71,
  // ASH protocol disconnected
  EZSP_ASH_DISCONNECTED                         = 0x72,
  // Timer expired waiting for ack
  EZSP_ASH_ACK_TIMEOUT                          = 0x73,
  // Frame in progress cancelled
  EZSP_ASH_CANCELLED                            = 0x74,
  // Received frame out of sequence
  EZSP_ASH_OUT_OF_SEQUENCE                      = 0x75,
  // Received frame with CRC error
  EZSP_ASH_BAD_CRC                              = 0x76,
  // Received frame with comm error
  EZSP_ASH_COMM_ERROR                           = 0x77,
  // Received frame with bad ackNum
  EZSP_ASH_BAD_ACKNUM                           = 0x78,
  // Received frame shorter than minimum
  EZSP_ASH_TOO_SHORT                            = 0x79,
  // Received frame longer than maximum
  EZSP_ASH_TOO_LONG                             = 0x7A,
  // Received frame with illegal control byte
  EZSP_ASH_BAD_CONTROL                          = 0x7B,
  // Received frame with illegal length for its type
  EZSP_ASH_BAD_LENGTH                           = 0x7C,
  // No reset or error
  EZSP_ASH_NO_ERROR                             = 0xFF
};

//------------------------------------------------------------------------------
// Network scan types.
typedef int8u EzspNetworkScanType;

enum {
  // An energy scan scans each channel for its RSSI value.
  EZSP_ENERGY_SCAN                              = 0x00,
  // An active scan scans each channel for available networks.
  EZSP_ACTIVE_SCAN                              = 0x01,
  // First performs an energy scan to find the quietest channel, then performs
  // an active scan on that channel to find an unused PAN id.
  EZSP_UNUSED_PAN_ID_SCAN                       = 0x02,
  // Continues a scan for joinable networks initiated by a prior call to
  // scanForJoinableNetwork. See scanForJoinableNetwork for details.
  EZSP_NEXT_JOINABLE_NETWORK_SCAN               = 0x03
};

//------------------------------------------------------------------------------
// Frame IDs

enum {

// Configuration Frames
  EZSP_VERSION                                  = 0x00,
  EZSP_GET_CONFIGURATION_VALUE                  = 0x52,
  EZSP_SET_CONFIGURATION_VALUE                  = 0x53,
  EZSP_ADD_ENDPOINT                             = 0x02,
  EZSP_SET_POLICY                               = 0x55,
  EZSP_GET_POLICY                               = 0x56,
  EZSP_GET_VALUE                                = 0xAA,
  EZSP_SET_VALUE                                = 0xAB,

// Utilities Frames
  EZSP_NOP                                      = 0x05,
  EZSP_ECHO                                     = 0x81,
  EZSP_INVALID_COMMAND                          = 0x58,
  EZSP_CALLBACK                                 = 0x06,
  EZSP_NO_CALLBACKS                             = 0x07,
  EZSP_SET_TOKEN                                = 0x09,
  EZSP_GET_TOKEN                                = 0x0A,
  EZSP_GET_MFG_TOKEN                            = 0x0B,
  EZSP_SET_RAM                                  = 0x46,
  EZSP_GET_RAM                                  = 0x47,
  EZSP_GET_RANDOM_NUMBER                        = 0x49,
  EZSP_GET_MILLISECOND_TIME                     = 0x0D,
  EZSP_SET_TIMER                                = 0x0E,
  EZSP_GET_TIMER                                = 0x4E,
  EZSP_TIMER_HANDLER                            = 0x0F,
  EZSP_SERIAL_WRITE                             = 0x10,
  EZSP_SERIAL_READ                              = 0x11,
  EZSP_DEBUG_WRITE                              = 0x12,
  EZSP_DEBUG_HANDLER                            = 0x13,
  EZSP_READ_AND_CLEAR_COUNTERS                  = 0x65,
  EZSP_DELAY_TEST                               = 0x9D,
  EZSP_GET_LIBRARY_STATUS                       = 0x01,

// Networking Frames
  EZSP_SET_MANUFACTURER_CODE                    = 0x15,
  EZSP_SET_POWER_DESCRIPTOR                     = 0x16,
  EZSP_NETWORK_INIT                             = 0x17,
  EZSP_NETWORK_STATE                            = 0x18,
  EZSP_STACK_STATUS_HANDLER                     = 0x19,
  EZSP_START_SCAN                               = 0x1A,
  EZSP_ENERGY_SCAN_RESULT_HANDLER               = 0x48,
  EZSP_NETWORK_FOUND_HANDLER                    = 0x1B,
  EZSP_SCAN_COMPLETE_HANDLER                    = 0x1C,
  EZSP_STOP_SCAN                                = 0x1D,
  EZSP_FORM_NETWORK                             = 0x1E,
  EZSP_JOIN_NETWORK                             = 0x1F,
  EZSP_SCAN_AND_FORM_NETWORK                    = 0x4F,
  EZSP_SCAN_AND_JOIN_NETWORK                    = 0x50,
  EZSP_SCAN_ERROR_HANDLER                       = 0x51,
  EZSP_SCAN_FOR_JOINABLE_NETWORK                = 0xA8,
  EZSP_UNUSED_PAN_ID_FOUND_HANDLER              = 0xA9,
  EZSP_LEAVE_NETWORK                            = 0x20,
  EZSP_FIND_AND_REJOIN_NETWORK                  = 0x21,
  EZSP_PERMIT_JOINING                           = 0x22,
  EZSP_CHILD_JOIN_HANDLER                       = 0x23,
  EZSP_ENERGY_SCAN_REQUEST                      = 0x9C,
  EZSP_GET_EUI64                                = 0x26,
  EZSP_GET_NODE_ID                              = 0x27,
  EZSP_GET_NETWORK_PARAMETERS                   = 0x28,
  EZSP_GET_PARENT_CHILD_PARAMETERS              = 0x29,
  EZSP_GET_CHILD_DATA                           = 0x4A,
  EZSP_GET_NEIGHBOR                             = 0x79,
  EZSP_NEIGHBOR_COUNT                           = 0x7A,
  EZSP_GET_ROUTE_TABLE_ENTRY                    = 0x7B,
  EZSP_SET_RADIO_POWER                          = 0x99,
  EZSP_SET_RADIO_CHANNEL                        = 0x9A,

// Binding Frames
  EZSP_CLEAR_BINDING_TABLE                      = 0x2A,
  EZSP_SET_BINDING                              = 0x2B,
  EZSP_GET_BINDING                              = 0x2C,
  EZSP_DELETE_BINDING                           = 0x2D,
  EZSP_BINDING_IS_ACTIVE                        = 0x2E,
  EZSP_GET_BINDING_REMOTE_NODE_ID               = 0x2F,
  EZSP_SET_BINDING_REMOTE_NODE_ID               = 0x30,
  EZSP_REMOTE_SET_BINDING_HANDLER               = 0x31,
  EZSP_REMOTE_DELETE_BINDING_HANDLER            = 0x32,

// Messaging Frames
  EZSP_MAXIMUM_PAYLOAD_LENGTH                   = 0x33,
  EZSP_SEND_UNICAST                             = 0x34,
  EZSP_SEND_BROADCAST                           = 0x36,
  EZSP_SEND_MULTICAST                           = 0x38,
  EZSP_SEND_REPLY                               = 0x39,
  EZSP_MESSAGE_SENT_HANDLER                     = 0x3F,
  EZSP_SEND_MANY_TO_ONE_ROUTE_REQUEST           = 0x41,
  EZSP_POLL_FOR_DATA                            = 0x42,
  EZSP_POLL_COMPLETE_HANDLER                    = 0x43,
  EZSP_POLL_HANDLER                             = 0x44,
  EZSP_INCOMING_SENDER_EUI64_HANDLER            = 0x62,
  EZSP_INCOMING_MESSAGE_HANDLER                 = 0x45,
  EZSP_INCOMING_ROUTE_RECORD_HANDLER            = 0x59,
  EZSP_SET_SOURCE_ROUTE                         = 0x5A,
  EZSP_INCOMING_MANY_TO_ONE_ROUTE_REQUEST_HANDLER = 0x7D,
  EZSP_INCOMING_ROUTE_ERROR_HANDLER             = 0x80,
  EZSP_ADDRESS_TABLE_ENTRY_IS_ACTIVE            = 0x5B,
  EZSP_SET_ADDRESS_TABLE_REMOTE_EUI64           = 0x5C,
  EZSP_SET_ADDRESS_TABLE_REMOTE_NODE_ID         = 0x5D,
  EZSP_GET_ADDRESS_TABLE_REMOTE_EUI64           = 0x5E,
  EZSP_GET_ADDRESS_TABLE_REMOTE_NODE_ID         = 0x5F,
  EZSP_SET_EXTENDED_TIMEOUT                     = 0x7E,
  EZSP_GET_EXTENDED_TIMEOUT                     = 0x7F,
  EZSP_REPLACE_ADDRESS_TABLE_ENTRY              = 0x82,
  EZSP_LOOKUP_NODE_ID_BY_EUI64                  = 0x60,
  EZSP_LOOKUP_EUI64_BY_NODE_ID                  = 0x61,
  EZSP_GET_MULTICAST_TABLE_ENTRY                = 0x63,
  EZSP_SET_MULTICAST_TABLE_ENTRY                = 0x64,
  EZSP_ID_CONFLICT_HANDLER                      = 0x7C,
  EZSP_SEND_RAW_MESSAGE                         = 0x96,
  EZSP_MAC_PASSTHROUGH_MESSAGE_HANDLER          = 0x97,
  EZSP_RAW_TRANSMIT_COMPLETE_HANDLER            = 0x98,

// Security Frames
  EZSP_SET_INITIAL_SECURITY_STATE               = 0x68,
  EZSP_GET_CURRENT_SECURITY_STATE               = 0x69,
  EZSP_GET_KEY                                  = 0x6a,
  EZSP_SWITCH_NETWORK_KEY_HANDLER               = 0x6e,
  EZSP_GET_KEY_TABLE_ENTRY                      = 0x71,
  EZSP_SET_KEY_TABLE_ENTRY                      = 0x72,
  EZSP_FIND_KEY_TABLE_ENTRY                     = 0x75,
  EZSP_ADD_OR_UPDATE_KEY_TABLE_ENTRY            = 0x66,
  EZSP_ERASE_KEY_TABLE_ENTRY                    = 0x76,
  EZSP_REQUEST_LINK_KEY                         = 0x14,
  EZSP_ZIGBEE_KEY_ESTABLISHMENT_HANDLER         = 0x9B,

// Trust Center Frames
  EZSP_TRUST_CENTER_JOIN_HANDLER                = 0x24,
  EZSP_BROADCAST_NEXT_NETWORK_KEY               = 0x73,
  EZSP_BROADCAST_NETWORK_KEY_SWITCH             = 0x74,
  EZSP_BECOME_TRUST_CENTER                      = 0x77,

// Certificate Based Key Exchange (CBKE)
  EZSP_GENERATE_CBKE_KEYS                       = 0xA4,
  EZSP_GENERATE_CBKE_KEYS_HANDLER               = 0x9E,
  EZSP_CALCULATE_SMACS                          = 0x9F,
  EZSP_CALCULATE_SMACS_HANDLER                  = 0xA0,
  EZSP_CLEAR_TEMPORARY_DATA_MAYBE_STORE_LINK_KEY = 0xA1,
  EZSP_GET_CERTIFICATE                          = 0xA5,
  EZSP_DSA_SIGN                                 = 0xA6,
  EZSP_DSA_SIGN_HANDLER                         = 0xA7,
  EZSP_SET_PREINSTALLED_CBKE_DATA               = 0xA2,

// Mfglib
  EZSP_MFGLIB_START                             = 0x83,
  EZSP_MFGLIB_END                               = 0x84,
  EZSP_MFGLIB_START_TONE                        = 0x85,
  EZSP_MFGLIB_STOP_TONE                         = 0x86,
  EZSP_MFGLIB_START_STREAM                      = 0x87,
  EZSP_MFGLIB_STOP_STREAM                       = 0x88,
  EZSP_MFGLIB_SEND_PACKET                       = 0x89,
  EZSP_MFGLIB_SET_CHANNEL                       = 0x8a,
  EZSP_MFGLIB_GET_CHANNEL                       = 0x8b,
  EZSP_MFGLIB_SET_POWER                         = 0x8c,
  EZSP_MFGLIB_GET_POWER                         = 0x8d,
  EZSP_MFGLIB_RX_HANDLER                        = 0x8e,

// Bootloader
  EZSP_LAUNCH_STANDALONE_BOOTLOADER             = 0x8f,
  EZSP_SEND_BOOTLOAD_MESSAGE                    = 0x90,
  EZSP_GET_STANDALONE_BOOTLOADER_VERSION_PLAT_MICRO_PHY = 0x91,
  EZSP_INCOMING_BOOTLOAD_MESSAGE_HANDLER        = 0x92,
  EZSP_BOOTLOAD_TRANSMIT_COMPLETE_HANDLER       = 0x93,
  EZSP_AES_ENCRYPT                              = 0x94,
  EZSP_OVERRIDE_CURRENT_CHANNEL                 = 0x95
};

#endif // __EZSP_ENUM_H__

