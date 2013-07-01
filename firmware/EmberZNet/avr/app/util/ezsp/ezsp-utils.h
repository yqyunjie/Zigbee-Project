// File: ezsp-utils.h
// 
// Description: EZSP utility library. These functions are provided to
// make it easier to port applications from the environment where the
// Ember stack and application where on a single processor to an
// environment where the stack is running on an Ember radio chip and
// the application is running on a separte host processor and
// utilizing the EZSP library.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_UTILS_H__
#define __EZSP_UTILS_H__

//----------------------------------------------------------------
// Zigbee Simple Descriptor:

/** @brief Endpoint information (a ZigBee Simple Descriptor)
 * @description This is a ZigBee Simple Descriptor and contains information
 * about an endpoint.  This information is shared with other nodes in the
 * network by the ZDO.
 */

typedef struct {
  /** Identifies the endpoint's application profile. */
  int16u profileId;
  /** The endpoint's device ID within the application profile. */
  int16u deviceId;
  /** The endpoint's device version. */
  int8u deviceVersion;
  /** The number of input clusters. */
  int8u inputClusterCount;
  /** The number of output clusters. */
  int8u outputClusterCount;
} EmberEndpointDescription;

/** @brief Gives the endpoint information for a particular endpoint.
 * @description Gives the endpoint information for a particular endpoint.
 */

typedef struct {
  /** An endpoint of the application on this node. */
  int8u endpoint;
  /** The endpoint's description. */
  PGM EmberEndpointDescription *description;
  /** Input clusters the endpoint will accept. */
  PGM int16u* inputClusterList;
  /** Output clusters the endpoint may send. */
  PGM int16u* outputClusterList;
} EmberEndpoint;

extern int8u emberEndpointCount;
extern EmberEndpoint emberEndpoints[];

/** @description Defines config parameter incompatibilities between the
 *   host and node
 */
enum
{
  EZSP_UTIL_INCOMPATIBLE_PROTOCOL_VERSION                   = 0x00000001,
  EZSP_UTIL_INCOMPATIBLE_STACK_ID                           = 0x00000002,
  EZSP_UTIL_INCOMPATIBLE_PACKET_BUFFER_COUNT                = 0x00000008,
  EZSP_UTIL_INCOMPATIBLE_NEIGHBOR_TABLE_SIZE                = 0x00000010,
  EZSP_UTIL_INCOMPATIBLE_APS_UNICAST_MESSAGE_COUNT          = 0x00000020,
  EZSP_UTIL_INCOMPATIBLE_BINDING_TABLE_SIZE                 = 0x00000040,
  EZSP_UTIL_INCOMPATIBLE_ADDRESS_TABLE_SIZE                 = 0x00000080,
  EZSP_UTIL_INCOMPATIBLE_MULTICAST_TABLE_SIZE               = 0x00000100,
  EZSP_UTIL_INCOMPATIBLE_ROUTE_TABLE_SIZE                   = 0x00000200,
  EZSP_UTIL_INCOMPATIBLE_DISCOVERY_TABLE_SIZE               = 0x00000400,
  EZSP_UTIL_INCOMPATIBLE_BROADCAST_ALARM_DATA_SIZE          = 0x00000800,
  EZSP_UTIL_INCOMPATIBLE_UNICAST_ALARM_DATA_SIZE            = 0x00001000,
  EZSP_UTIL_INCOMPATIBLE_STACK_PROFILE                      = 0x00004000,
  EZSP_UTIL_INCOMPATIBLE_SECURITY_LEVEL                     = 0x00008000,
  EZSP_UTIL_INCOMPATIBLE_MAX_HOPS                           = 0x00040000,
  EZSP_UTIL_INCOMPATIBLE_MAX_END_DEVICE_CHILDREN            = 0x00080000,
  EZSP_UTIL_INCOMPATIBLE_INDIRECT_TRANSMISSION_TIMEOUT      = 0x00100000,
  EZSP_UTIL_INCOMPATIBLE_END_DEVICE_POLL_TIMEOUT            = 0x00200000,
  EZSP_UTIL_INCOMPATIBLE_MOBILE_NODE_POLL_TIMEOUT           = 0x00400000,
  EZSP_UTIL_INCOMPATIBLE_RESERVED_MOBILE_CHILD_ENTRIES      = 0x00800000,
  EZSP_UTIL_INCOMPATIBLE_END_DEVICE_POLL_TIMEOUT_SHIFT      = 0x01000000,
  EZSP_UTIL_INCOMPATIBLE_SOURCE_ROUTE_TABLE_SIZE            = 0x02000000,
  EZSP_UTIL_INCOMPATIBLE_FRAGMENT_WINDOW_SIZE               = 0x04000000,
  EZSP_UTIL_INCOMPATIBLE_FRAGMENT_DELAY_MS                  = 0x08000000,
  EZSP_UTIL_INCOMPATIBLE_KEY_TABLE_SIZE                     = 0x10000000,
};

// Replacement for SOFTWARE_VERSION
extern int16u ezspUtilStackVersion;

// Replacement function for emberInit - checks configuration, sets the
// stack profile, and registers endpoints.
EmberStatus ezspUtilInit(int8u serialPort, EmberResetType reset);

void emberTick(void);

int8u emberGetRadioChannel(void);

extern EmberEUI64 emLocalEui64;
// The ezsp util library keeps track of the local node's EUI64 and
// short id so that there isn't the need for communication with the
// Ember radio node every time these values are queried.
#define emberGetEui64() (emLocalEui64)
#define emberIsLocalEui64(eui64) \
        (MEMCOMPARE(eui64, emLocalEui64, EUI64_SIZE) == 0)

extern int8u emberBindingTableSize;
extern int8u emberTemporaryBindingEntries;

#define emberFetchLowHighInt16u(contents)            \
        (HIGH_LOW_TO_INT((contents)[1], (contents)[0]))
#define emberStoreLowHighInt16u(contents, value)     \
        do {                                         \
             (contents)[0] = LOW_BYTE(value);        \
             (contents)[1] = HIGH_BYTE(value);       \
        } while (0)

// For back-compatibility
#define emberRejoinNetwork(haveKey) emberFindAndRejoinNetwork((haveKey), 0)

// For back-compatibility
#define ezspReset() ezspInit()

/** @brief Broadcasts a request to set the identity of the network manager and 
 * the active channel mask.  The mask is used when scanning
 *  for the network after missing a channel update.
 *
 * @param networkManager   The network address of the network manager.
 * @param activeChannels   The new active channel mask.
 * 
 * @return An ::EmberStatus value. 
 * - ::EMBER_SUCCESS
 * - ::EMBER_NO_BUFFERS
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_NETWORK_BUSY
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberSetNetworkManagerRequest(EmberNodeId networkManager,
                                          int32u activeChannels);
#else
#define emberSetNetworkManagerRequest(manager, channels)        \
(emberEnergyScanRequest(EMBER_SLEEPY_BROADCAST_ADDRESS,         \
                        (channels),                             \
                        0xFF,                                   \
                        (manager)))
#endif

/** @brief Broadcasts a request to change the channel.  This request may
 * only be sent by the current network manager.  There is a delay of
 * several seconds from receipt of the broadcast to changing the channel,
 * to allow time for the broadcast to propagate.
 *
 * @param channel  The channel to change to.
 * 
 * @return An ::EmberStatus value. 
 * - ::EMBER_SUCCESS
 * - ::EMBER_NO_BUFFERS
 * - ::EMBER_NETWORK_DOWN
 * - ::EMBER_NETWORK_BUSY
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberChannelChangeRequest(int8u channel)
#else
#define emberChannelChangeRequest(channel)                      \
(emberEnergyScanRequest(EMBER_SLEEPY_BROADCAST_ADDRESS,         \
                        BIT32(channel),                         \
                        0xFE,                                   \
                        0))
#endif

#endif // __EZSP_UTILS_H__
