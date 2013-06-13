/**
 * @file interpan.h
 * @brief Plugin for receiving InterPAN messages.
 * See @ref message for documentation.
 *
 * <!--Copyright 2010 by Ember Corporation. All rights reserved.         *80*-->
 */

// The three types of inter-PAN messages.  The values are actually the
// corresponding APS frame controls.
//
// 0x03 is the special interPAN message type.  Unicast mode is 0x00,
// broadcast mode is 0x08, and multicast mode is 0x0C.
//

// MAC Frame Max size
// - Frame control   (2-bytes)
// - Sequence        (1-byte)
// - Dest PAN ID     (2-bytes)
// - Dest long       (8-bytes)
// - Source PAN ID   (2-bytes)
// - Source long     (8-bytes)
#define MAX_INTER_PAN_MAC_SIZE 23

// NWK stub frame has two control bytes.
#define STUB_NWK_SIZE 2
#define STUB_NWK_FRAME_CONTROL 0x000B

// Interpan APS Unicast
//  - Frame Control   (1-byte)
//  - Cluster ID      (2-bytes)
//  - Profile ID      (2-bytes)
#define INTERPAN_APS_UNICAST_SIZE 5

// Interpan APS Broadcast, same as unicast
#define INTERPAN_APS_BROADCAST_SIZE 5

// Interpan APS Multicast
//  - Frame Control   (1-byte)
//  - Group ID        (2-bytes)
//  - Cluster ID      (2-bytes)
//  - Profile ID      (2-bytes)
#define INTERPAN_APS_MULTICAST_SIZE  7

#define MAX_STUB_APS_SIZE (INTERPAN_APS_MULTICAST_SIZE)
#define MIN_STUB_APS_SIZE (INTERPAN_APS_UNICAST_SIZE)

#define MAX_INTER_PAN_HEADER_SIZE \
 (MAX_INTER_PAN_MAC_SIZE + STUB_NWK_SIZE + MAX_STUB_APS_SIZE)

#define INTERPAN_APS_FRAME_TYPE      0x03
#define INTERPAN_APS_FRAME_TYPE_MASK 0x03

// The only allowed APS FC value (without the delivery mode subfield)
#define INTERPAN_APS_FRAME_CONTROL_NO_DELIVERY_MODE (INTERPAN_APS_FRAME_TYPE)

#define INTERPAN_APS_FRAME_DELIVERY_MODE_MASK 0x0C
#define INTERPAN_APS_FRAME_SECURITY           0x20

// 5 byte AUX header + 4 byte MIC
#define INTERPAN_APS_ENCRYPTION_OVERHEAD      (5 + 4)  

#define EMBER_AF_PLUGIN_INTERPAN_FILTER_LIST       \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_BROADCAST_SHORT \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_SHORT   \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_LONG    \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_BROADCAST_SHORT \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_SHORT   \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_LOCAL       \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_LONG    \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_BROADCAST_SHORT \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_SHORT   \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_NON_LOCAL \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_LONG    \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_BROADCAST_SHORT \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_SHORT   \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \
 (EMBER_MAC_FILTER_MATCH_ENABLED                   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_DEST_BROADCAST   \
  | EMBER_MAC_FILTER_MATCH_ON_PAN_SOURCE_LOCAL     \
  | EMBER_MAC_FILTER_MATCH_ON_DEST_UNICAST_LONG    \
  | EMBER_MAC_FILTER_MATCH_ON_SOURCE_LONG),        \

boolean emAfPluginInterpanProcessMessage(int8u messageLength,
                                         int8u *messageContents);

EmberStatus emAfPluginInterpanSendRawMessage(int8u length, int8u* message);

EmberStatus emAfInterpanApsCryptMessage(boolean encrypt,
                                        int8u* apsFrame,
                                        int8u* messageLength,
                                        int8u apsHeaderLength,
                                        EmberEUI64 remoteEui64);
