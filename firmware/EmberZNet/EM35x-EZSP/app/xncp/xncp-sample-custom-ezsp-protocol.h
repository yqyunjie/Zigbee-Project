/*
 * File: xncp-sample-custom-ezsp-protocol.h
 * Description: Header file of a sample of a custom EZSP protocol.
 *
 * Author(s): Maurizio Nanni, maurizio.nanni@ember.com
 *
 * Copyright 2012 by Ember Corporation. All rights reserved.
 */

// This is set according to the ZigBee specification's manufacturer ID list
// Ember's manufacturer ID value is 0x1002.
#define XNCP_MANUFACTURER_ID  EMBER_MANUFACTURER_ID
#define XNCP_VERSION_NUMBER   0xBEEF

// For this sample app all the custom EZSP messages are formatted as follows:
//   <Custom command ID:1>
//   <parameters:0-118>

// -----------------------------------------------------------------------------
// Protocol specs:

// ----------------------
// SET_POWER_MODE
// Command: Command ID (1 byte) + power mode (1 byte)
// Response: Status (1 byte)
// ----------------------
// GET_POWER_MODE
// Command: Command ID (1 byte)
// Response: Status (1 byte) + power mode (1 byte)
// ----------------------
// ADD_CLUSTER_TO_FILTERING_LIST
// Command: Command ID (1 byte) + cluster ID (2 bytes)
// Response: Status (1 byte)
// ----------------------
// REMOVE_CLUSTER_TO_FILTERING_LIST
// Command: Command ID (1 byte) + cluster ID (2 bytes)
// Response: Status (1 byte)
// ----------------------
// GET_CLUSTER_TO_FILTERING_LIST
// Command: Command ID (1 byte)
// Response: Status (1 byte)  + num. entries (1 byte) + cluster IDs (variable)
// ----------------------
// ENABLE_PERIODIC_REPORTS
// Command: Command ID (1 byte) + time interval (secs.) (2 bytes)
// Response: Status (1 byte)
// ----------------------
// DISABLE_PERIODIC_REPORTS
// Command: Command ID (1 byte)
// Response: Status (1 byte)
// ----------------------
// SET_CUSTOM_TOKEN
// Command: Command ID (1 byte) + nodeType (1 byte) + nodeID (2 bytes) +
//          + panID (2 bytes)
// Response: Status (1 byte)
// ----------------------
// GET_CUSTOM_TOKEN
// Command: Command ID (1 byte)
// Response: Status (1 byte) + nodeType (1 byte) + nodeID (2 bytes) +
//           + panID (2 bytes)
// ----------------------
// CALLBACK_REPORT
// Command: Command ID (1 byte) + report count (2 bytes)
// Response: -

// -----------------------------------------------------------------------------

// Custom command IDs
enum
{
  // HOST -> XNCP
  EMBER_CUSTOM_EZSP_COMMAND_SET_POWER_MODE                               = 0x00,
  EMBER_CUSTOM_EZSP_COMMAND_GET_POWER_MODE                               = 0x01,
  EMBER_CUSTOM_EZSP_COMMAND_ADD_CLUSTER_TO_FILTERING_LIST                = 0x02,
  EMBER_CUSTOM_EZSP_COMMAND_REMOVE_CLUSTER_TO_FILTERING_LIST             = 0x03,
  EMBER_CUSTOM_EZSP_COMMAND_GET_CLUSTER_FILTERING_LIST                   = 0x04,
  EMBER_CUSTOM_EZSP_COMMAND_ENABLE_PERIODIC_REPORTS                      = 0x05,
  EMBER_CUSTOM_EZSP_COMMAND_DISABLE_PERIODIC_REPORTS                     = 0x06,
  EMBER_CUSTOM_EZSP_COMMAND_SET_CUSTOM_TOKEN                             = 0x07,
  EMBER_CUSTOM_EZSP_COMMAND_GET_CUSTOM_TOKEN                             = 0x08,

  // XNCP -> HOST
  EMBER_CUSTOM_EZSP_CALLBACK_REPORT                                      = 0x80,
};

// Power modes
enum
{
  EMBER_XNCP_NORMAL_MODE,
  EMBER_XNCP_LOW_POWER_MODE,
  EMBER_XNCP_RESERVED // always last in the enum
};
