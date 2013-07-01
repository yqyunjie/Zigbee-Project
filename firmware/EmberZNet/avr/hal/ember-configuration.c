/** @file ember-configuration.c
 * @brief User-configurable stack memory allocation and convenience stubs 
 * for little-used callbacks. 
 *
 * \b Note: Application developers should \b not modify any portion
 * of this file. Doing so may lead to mysterious bugs. Allocations should be 
 * adjusted only with macros in a custom CONFIGURATION_HEADER. 
 *
 * <!-- Author(s): Lee Taylor, lee@ember.com -->
 *
 * <!--Copyright 2005 by Ember Corporation. All rights reserved.         *80*-->
 */
#include PLATFORM_HEADER 
#include "stack/include/ember.h"
#include "stack/include/error.h"
#include "stack/include/ember-static-struct.h" // Required typedefs

// *****************************************
// Memory Allocations & declarations
// *****************************************

extern int8u emAvailableMemory[];
#ifdef XAP2B
  #define align(value) ((value) + ((value) & 1))
#else
  #define align(value) (value)
#endif

//------------------------------------------------------------------------------
// API Version

const int8u emApiVersion 
  = (EMBER_API_MAJOR_VERSION << 4) + EMBER_API_MINOR_VERSION;

//------------------------------------------------------------------------------
// Packet Buffers

int8u emPacketBufferCount = EMBER_PACKET_BUFFER_COUNT;
int8u emPacketBufferFreeCount = EMBER_PACKET_BUFFER_COUNT;

// The actual memory for buffers.
int8u *emPacketBufferData = &emAvailableMemory[0];
#define END_emPacketBufferData          \
  (align(EMBER_PACKET_BUFFER_COUNT * 32))

int8u *emMessageBufferLengths = &emAvailableMemory[END_emPacketBufferData];
#define END_emMessageBufferLengths      \
  (END_emPacketBufferData + align(EMBER_PACKET_BUFFER_COUNT))

int8u *emMessageBufferReferenceCounts = &emAvailableMemory[END_emMessageBufferLengths];
#define END_emMessageBufferReferenceCounts      \
  (END_emMessageBufferLengths + align(EMBER_PACKET_BUFFER_COUNT))

int8u *emPacketBufferLinks = &emAvailableMemory[END_emMessageBufferReferenceCounts];
#define END_emPacketBufferLinks      \
  (END_emMessageBufferReferenceCounts + align(EMBER_PACKET_BUFFER_COUNT))

int8u *emPacketBufferQueueLinks = &emAvailableMemory[END_emPacketBufferLinks];
#define END_emPacketBufferQueueLinks      \
  (END_emPacketBufferLinks + align(EMBER_PACKET_BUFFER_COUNT))

//------------------------------------------------------------------------------
// NWK Layer

#ifdef EMBER_DISABLE_RELAY
int8u emAllowRelay = FALSE;
#else
int8u emAllowRelay = TRUE;
#endif

// emChildIdTable must be sized one element larger than EMBER_CHILD_TABLE_SIZE
// to allow emberChildIndex() to perform an optimized search when setting the
// frame pending bit.  emberChildTableSize and EMBER_CHILD_TABLE_SIZE still
// correspond to the number of children, not the number of child table elements.
EmberNodeId *emChildIdTable = (EmberNodeId *) &emAvailableMemory[END_emPacketBufferQueueLinks];
int8u emberChildTableSize = EMBER_CHILD_TABLE_SIZE;
#define END_emChildIdTable              \
 (END_emPacketBufferQueueLinks + align( (EMBER_CHILD_TABLE_SIZE+1) * sizeof(EmberNodeId)))

int16u *emChildStatus = (int16u *) &emAvailableMemory[END_emChildIdTable];
#define END_emChildStatus               \
 (END_emChildIdTable + align(EMBER_CHILD_TABLE_SIZE * sizeof(int16u)))

int8u *emChildTimers = (int8u *) &emAvailableMemory[END_emChildStatus];
#define END_emChildTimers               \
 (END_emChildStatus + align(EMBER_CHILD_TABLE_SIZE * sizeof(int8u)))

int8u *emUnicastAlarmData = (int8u *) &emAvailableMemory[END_emChildTimers];
int8u emUnicastAlarmDataSize = EMBER_UNICAST_ALARM_DATA_SIZE;
#define END_emUnicastAlarmData          \
 (END_emChildTimers+ align(EMBER_CHILD_TABLE_SIZE * EMBER_UNICAST_ALARM_DATA_SIZE))

int8u *emBroadcastAlarmData = (int8u *) &emAvailableMemory[END_emUnicastAlarmData];
int8u emBroadcastAlarmDataSize = EMBER_BROADCAST_ALARM_DATA_SIZE;
#define END_emBroadcastAlarmData        \
 (END_emUnicastAlarmData + align(EMBER_BROADCAST_ALARM_DATA_SIZE))

EmRouteTableEntry *emRouteData = (EmRouteTableEntry *) &emAvailableMemory[END_emBroadcastAlarmData];
int8u emRouteTableSize = EMBER_ROUTE_TABLE_SIZE;
#define END_emRouteData        \
 (END_emBroadcastAlarmData + align(EMBER_ROUTE_TABLE_SIZE * sizeof(EmRouteTableEntry)))

EmDiscoveryTableEntry *emDiscoveryTable = (EmDiscoveryTableEntry *) &emAvailableMemory[END_emRouteData];
int8u emDiscoveryTableSize = EMBER_DISCOVERY_TABLE_SIZE;
#define END_emDiscoveryTable        \
 (END_emRouteData + align(EMBER_DISCOVERY_TABLE_SIZE * sizeof(EmDiscoveryTableEntry)))

EmberMulticastTableEntry *emberMulticastTable = (EmberMulticastTableEntry *) &emAvailableMemory[END_emDiscoveryTable];
int8u emberMulticastTableSize = EMBER_MULTICAST_TABLE_SIZE;
#define END_emberMulticastTable        \
 (END_emDiscoveryTable + align(EMBER_MULTICAST_TABLE_SIZE * sizeof(EmberMulticastTableEntry)))

//------------------------------------------------------------------------------
// Neighbor Table

EmNeighborTableEntry *emNeighborData = (EmNeighborTableEntry *) &emAvailableMemory[END_emberMulticastTable];
int8u emNeighborTableSize = EMBER_NEIGHBOR_TABLE_SIZE;
#define END_emNeighborData        \
 (END_emberMulticastTable + align(EMBER_NEIGHBOR_TABLE_SIZE * sizeof(EmNeighborTableEntry)))

int32u *emFrameCounters = (int32u *) &emAvailableMemory[END_emNeighborData];
#define END_emFrameCounters        \
 (END_emNeighborData + align((EMBER_NEIGHBOR_TABLE_SIZE + EMBER_CHILD_TABLE_SIZE) * sizeof(int32u)))

//------------------------------------------------------------------------------
// Binding Table

int8u emberBindingTableSize = EMBER_BINDING_TABLE_SIZE;

int16u *emBindingRemoteNode = (int16u *) &emAvailableMemory[END_emFrameCounters];
#define END_emBindingRemoteNode        \
 (END_emFrameCounters + align(EMBER_BINDING_TABLE_SIZE * sizeof(int16u)))

int8u *emBindingFlags = &emAvailableMemory[END_emBindingRemoteNode];
#define END_emBindingFlags        \
 (END_emBindingRemoteNode + align(EMBER_BINDING_TABLE_SIZE))

//------------------------------------------------------------------------------
// APS Layer

int8u emAddressTableSize = EMBER_ADDRESS_TABLE_SIZE;
EmAddressTableEntry *emAddressTable = (EmAddressTableEntry *) &emAvailableMemory[END_emBindingFlags];
#define END_emAddressTable        \
 (END_emBindingFlags + align(EMBER_ADDRESS_TABLE_SIZE * sizeof(EmAddressTableEntry)))

int8u emMaxApsUnicastMessages = EMBER_APS_UNICAST_MESSAGE_COUNT;
EmApsUnicastMessageData *emApsUnicastMessageData = (EmApsUnicastMessageData *) &emAvailableMemory[END_emAddressTable];
#define END_emApsUnicastMessageData        \
 (END_emAddressTable + align(EMBER_APS_UNICAST_MESSAGE_COUNT * sizeof(EmApsUnicastMessageData)))

int16u emberApsAckTimeoutMs = 
 ((EMBER_APSC_MAX_ACK_WAIT_HOPS_MULTIPLIER_MS
   * EMBER_MAX_HOPS)
  + EMBER_APSC_MAX_ACK_WAIT_TERMINAL_SECURITY_MS);

int8u emFragmentDelayMs = EMBER_FRAGMENT_DELAY_MS;
int8u emberFragmentWindowSize = EMBER_FRAGMENT_WINDOW_SIZE;

int8u emberKeyTableSize = EMBER_KEY_TABLE_SIZE;
int32u* emIncomingApsFrameCounters = (int32u*)&emAvailableMemory[END_emApsUnicastMessageData];
#define END_emIncomingApsFrameCounters \
  (END_emApsUnicastMessageData + align(EMBER_KEY_TABLE_SIZE * sizeof(int32u)))

EmberLinkKeyRequestPolicy emberTrustCenterLinkKeyRequestPolicy = 
  EMBER_ALLOW_KEY_REQUESTS;
EmberLinkKeyRequestPolicy emberAppLinkKeyRequestPolicy = 
  EMBER_ALLOW_KEY_REQUESTS;

int8u emCertificateTableSize = EMBER_CERTIFICATE_TABLE_SIZE;

// Define this in order to receive supported ZDO request messages via
// the incomingMessageHandler callback.  A supported ZDO request is one that
// is handled by the EmberZNet stack.  The stack will continue to handle the
// request and send the appropriate ZDO response even if this configuration
// option is enabled.
#ifdef EMBER_APPLICATION_RECEIVES_SUPPORTED_ZDO_REQUESTS
  boolean emAppReceivesSupportedZdoRequests = TRUE;
#else
  boolean emAppReceivesSupportedZdoRequests = FALSE;
#endif

// Define this in order to receive unsupported ZDO request messages via
// the incomingMessageHandler callback.  An unsupported ZDO request is one that
// is not handled by the EmberZNet stack, other than to send a 'not supported'
// ZDO response.  If this configuration option is enabled, the stack will no
// longer send any ZDO response, and it is the application's responsibility
// to do so.  To see if a response is required, the application must check
// the APS options bitfield within the emberIncomingMessageHandler callback to see
// if the EMBER_APS_OPTION_ZDO_RESPONSE_REQUIRED flag is set.
#ifdef EMBER_APPLICATION_HANDLES_UNSUPPORTED_ZDO_REQUESTS
  boolean emAppHandlesUnsupportedZdoRequests = TRUE;
#else
  boolean emAppHandlesUnsupportedZdoRequests = FALSE;
#endif

// Define this in order to receive the following ZDO request 
// messages via the emberIncomingMessageHandler callback: SIMPLE_DESCRIPTOR_REQUEST,
// MATCH_DESCRIPTORS_REQUEST, and ACTIVE_ENDPOINTS_REQUEST.  If this 
// configuration option is enabled, the stack will no longer send any ZDO
// response, and it is the application's responsibility to do so.
// To see if a response is required, the application must check
// the APS options bitfield within the emberIncomingMessageHandler callback to see
// if the EMBER_APS_OPTION_ZDO_RESPONSE_REQUIRED flag is set.
#ifdef EMBER_APPLICATION_HANDLES_ENDPOINT_ZDO_REQUESTS
  boolean emAppHandlesEndpointZdoRequests = TRUE;
#else
  boolean emAppHandlesEndpointZdoRequests = FALSE;
#endif

//------------------------------------------------------------------------------
// Memory Allocation

#ifndef RESERVED_AVAILABLE_MEMORY
  #define RESERVED_AVAILABLE_MEMORY 0
#endif
#define END_stackMemory  END_emIncomingApsFrameCounters + RESERVED_AVAILABLE_MEMORY

// On the XAP2B platform, emAvailableMemory is allocated automatically to fill
// the available space. On other platforms, we must allocate it here.
#ifdef XAP2B
  extern int8u emAvailableMemoryTop[];
  const int16u emMinAvailableMemorySize = END_stackMemory;
#else
  int8u emAvailableMemory[END_stackMemory];
  const int16u emAvailableMemorySize = END_stackMemory;
#endif

void emCheckAvailableMemory(void)
{
#ifdef XAP2B
  int16u emAvailableMemorySize = emAvailableMemoryTop - emAvailableMemory;
#endif
  assert(END_stackMemory <= emAvailableMemorySize);
}

// *****************************************
// Stack Profile Parameters
// *****************************************

PGM int8u emberStackProfileId[8] = { 0, };

int8u emStackProfile = EMBER_STACK_PROFILE;
int8u emZigbeeNetworkSecurityLevel = EMBER_SECURITY_LEVEL;
int8u emMaxEndDeviceChildren = EMBER_MAX_END_DEVICE_CHILDREN;
int8u emMaxHops = EMBER_MAX_HOPS;
int16u emberMacIndirectTimeout = EMBER_INDIRECT_TRANSMISSION_TIMEOUT;
int8u emberReservedMobileChildEntries = EMBER_RESERVED_MOBILE_CHILD_ENTRIES;
int8u emberMobileNodePollTimeout = EMBER_MOBILE_NODE_POLL_TIMEOUT;
int8u emberEndDevicePollTimeout = EMBER_END_DEVICE_POLL_TIMEOUT;
int8u emberEndDevicePollTimeoutShift = EMBER_END_DEVICE_POLL_TIMEOUT_SHIFT;
int8u emEndDeviceBindTimeout = EMBER_END_DEVICE_BIND_TIMEOUT;
int8u emRequestKeyTimeout = EMBER_REQUEST_KEY_TIMEOUT;
int8u emPanIdConflictReportThreshold = EMBER_PAN_ID_CONFLICT_REPORT_THRESHOLD;

// *****************************************
// Convenience Stubs
// *****************************************

#ifndef EMBER_APPLICATION_HAS_TRUST_CENTER_JOIN_HANDLER
EmberJoinDecision emberDefaultTrustCenterDecision = EMBER_USE_PRECONFIGURED_KEY;

EmberJoinDecision emberTrustCenterJoinHandler(EmberNodeId newNodeId,
                                              EmberEUI64 newNodeEui64,
                                              EmberDeviceUpdate status,
                                              EmberNodeId parentOfNewNode)
{
  if (status == EMBER_STANDARD_SECURITY_SECURED_REJOIN
      || status == EMBER_DEVICE_LEFT
      || status == EMBER_HIGH_SECURITY_SECURED_REJOIN)
    return EMBER_NO_ACTION;

  return emberDefaultTrustCenterDecision;
}
#endif

#ifndef EMBER_APPLICATION_HAS_SWITCH_KEY_HANDLER
void emberSwitchNetworkKeyHandler(int8u sequenceNumber)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_ZIGBEE_KEY_ESTABLISHMENT_HANDLER
void emberZigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_CHILD_JOIN_HANDLER
void emberChildJoinHandler(int8u index, boolean joining)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_POLL_COMPLETE_HANDLER
void emberPollCompleteHandler(EmberStatus status)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS
void emberIncomingBootloadMessageHandler(EmberEUI64 longId,
                                         EmberMessageBuffer message)
{
}
void emberBootloadTransmitCompleteHandler(EmberMessageBuffer message,
                                          EmberStatus status)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_MAC_PASSTHROUGH_HANDLER
void emberMacPassthroughMessageHandler(EmberMacPassthroughType messageType,
                                       EmberMessageBuffer message)
{
}
#endif
#ifndef EMBER_APPLICATION_HAS_RAW_HANDLER
void emberRawTransmitCompleteHandler(EmberMessageBuffer message,
                                     EmberStatus status)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_INCOMING_MFG_TEST_MESSAGE_HANDLER
void emberIncomingMfgTestMessageHandler(int8u messageType, 
                                        int8u dataLength, 
                                        int8u *data) {}
#endif

#ifndef EMBER_APPLICATION_HAS_ENERGY_SCAN_RESULT_HANDLER
void emberEnergyScanResultHandler(int8u channel, int8u maxRssiValue) {}
#endif

#ifndef EMBER_APPLICATION_HAS_DEBUG_HANDLER
void emberDebugHandler(EmberMessageBuffer message) {}
#endif

#ifndef EMBER_APPLICATION_HAS_POLL_HANDLER
void emberPollHandler(EmberNodeId childId, boolean transmitExpected)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_REMOTE_BINDING_HANDLER
EmberStatus emberRemoteSetBindingHandler(EmberBindingTableEntry *entry)
{
  // Don't let anyone mess with our bindings.
  return EMBER_INVALID_BINDING_INDEX;
}
EmberStatus emberRemoteDeleteBindingHandler(int8u index)
{
  // Don't let anyone mess with our bindings.
  return EMBER_INVALID_BINDING_INDEX;
}
#endif

#ifndef EMBER_APPLICATION_HAS_BUTTON_HANDLER
void halButtonIsr(int8u button, int8u state)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_SOURCE_ROUTING
void emberIncomingRouteRecordHandler(EmberNodeId source,
                                     EmberEUI64 sourceEui,
                                     int8u relayCount,
                                     EmberMessageBuffer header,
                                     int8u relayListIndex)
{
}
int8u emberAppendSourceRouteHandler(EmberNodeId destination,
                                    EmberMessageBuffer header)
{
  return 0;
}
#endif

#ifndef EMBER_APPLICATION_HAS_INCOMING_MANY_TO_ONE_ROUTE_REQUEST_HANDLER
void emberIncomingManyToOneRouteRequestHandler(EmberNodeId source,
                                               EmberEUI64 longId,
                                               int8u cost)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_INCOMING_ROUTE_ERROR_HANDLER
void emberIncomingRouteErrorHandler(EmberStatus status, 
                                    EmberNodeId target)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_GET_ENDPOINT
int8u emberGetEndpoint(int8u index)
{
  return emberEndpoints[index].endpoint;
}

boolean emberGetEndpointDescription(int8u endpoint,
                                    EmberEndpointDescription *result)
{ 
  int8u i;
  EmberEndpoint *endpoints = emberEndpoints;
  for (i = 0; i < emberEndpointCount; i++, endpoints++) {
    if (endpoints->endpoint == endpoint) {
      EmberEndpointDescription PGM * d = endpoints->description;
      result->profileId                   = d->profileId;
      result->deviceId                    = d->deviceId;
      result->deviceVersion               = d->deviceVersion;
      result->inputClusterCount           = d->inputClusterCount;
      result->outputClusterCount          = d->outputClusterCount;
      return TRUE;
    }
  }
  return FALSE;
}

int16u emberGetEndpointCluster(int8u endpoint,
                               EmberClusterListId listId,
                               int8u listIndex)
{
  int8u i;
  EmberEndpoint *endpoints = emberEndpoints;
  for (i = 0; i < emberEndpointCount; i++, endpoints++) {
    if (endpoints->endpoint == endpoint) {
      switch (listId) {
      case EMBER_INPUT_CLUSTER_LIST:
        return endpoints->inputClusterList[listIndex];
      case EMBER_OUTPUT_CLUSTER_LIST:
        return endpoints->outputClusterList[listIndex];
      }
    }
  }
  return 0;
}
#endif

// Inform the application that an orphan notification has been received.
// This is generally not useful for applications. It could be useful in
// testing and is included for this purpose.
#ifndef EMBER_APPLICATION_HAS_ORPHAN_NOTIFICATION_HANDLER
void emberOrphanNotificationHandler(EmberEUI64 longId)
{
  return;
}
#endif

#ifndef EMBER_APPLICATION_HAS_COUNTER_HANDLER
void emberCounterHandler(EmberCounterType type, int8u data)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_ID_CONFLICT_HANDLER
void emberIdConflictHandler(EmberNodeId conflictingId)
{
}
#endif

#ifndef EMBER_APPLICATION_HAS_MAC_PASSTHROUGH_FILTER_HANDLER
boolean emberMacPassthroughFilterHandler(int8u *macHeader)
{
  return FALSE;
}
#endif

#ifdef  XAP2B
#ifndef EMBER_APPLICATION_HAS_CUSTOM_SIM_EEPROM_CALLBACK
#include "hal/micro/xap2b/em250/sim-eeprom.h"
#include "hal/micro/xap2b/em250/pcb.h"
// The Simulated EEPROM Callback function.
// The Simulated EEPROM occasionally needs to perform a page erase operation
// which disables interrupts for 20ms.
// Since this operation may impact proper application functionality, it is
// performed in a callback function which may be customized by the application.
// Applications that need to perform custom processing before and after this
// operation should define EMBER_APPLICATION_HAS_CUSTOM_SIM_EEPROM_CALLBACK.
// The default implementation provided here does not perform any special
// processing before performing the page erase operation.
// 'GREEN' means a page needs to be erased, but we have not crossed the
// threshold of how full the current page is.
// 'RED' means a page needs to be erased and we have a critically small amount
// of space left in the current page (we crossed the threshold).
void halSimEepromCallback(EmberStatus status)
{
  if(status==EMBER_SIM_EEPROM_ERASE_PAGE_GREEN) {
    // this condition is expected in normal operation.  nothing to do.
  } else if(status==EMBER_SIM_EEPROM_ERASE_PAGE_RED) {
    // this condition indicates that the page erase operation must be performed.
    halSimEepromErasePage();
  } else if(status==EMBER_SIM_EEPROM_FULL) {
    // the Simulated EEPROM is full!  we must erase a page!  we should never
    // reach this case if the PAGE_RED above calls ErasePage();
    halSimEepromErasePage();
  } else if((status==EMBER_ERR_FLASH_WRITE_INHIBITED) ||
            (status==EMBER_ERR_FLASH_VERIFY_FAILED)) {
    //Something went wrong while writing a token.  There is stale data and the
    //token the app expected to write did not get written.  Also there is
    //now "stray" data written in the flash that could inhibit future token
    //writes.  To deal with this stray data, we must repair the Simulated
    //EEPROM.  Because the expected token write failed and will not be retried,
    //it is best to reset the chip and let normal boot sequences take over.
    //Since halInternalSimEeRepair() could potentially result in another write
    //failure, we use a simple semaphore to prevent recursion.
    static boolean repairActive = FALSE;
    if(!repairActive) {
      repairActive = TRUE;
      halInternalSimEeRepair(FALSE);
      if(status==EMBER_ERR_FLASH_VERIFY_FAILED) {
        halInternalSysReset(CE_REBOOT_F_VERIFY);
      } else {
        halInternalSysReset(CE_REBOOT_F_INHIBIT);
      }
      repairActive = FALSE;
    }
  } else {
    // this condition indicates an unexpected problem.
  }
}
#endif//EMBER_APPLICATION_HAS_CUSTOM_SIM_EEPROM_CALLBACK

#ifndef EMBER_APPLICATION_HAS_CUSTOM_RADIO_CALIBRATION_CALLBACK
// See stack-info.h for more information.
void emberRadioNeedsCalibratingHandler(void)
{
  // TODO: Failsafe any critical processes or peripherals.
  emberCalibrateCurrentChannel();
}

#endif//EMBER_APPLICATION_HAS_CUSTOM_RADIO_CALIBRATION_CALLBACK


#ifndef CUSTOM_EM250_TEST_APPLICATION
/* Sample system call */
int32u emberTest ( int16u arg, ... )
{
  return arg;
}
#endif//CUSTOM_EM250_TEST_APPLICATION

#ifndef EMBER_APPLICATION_HAS_CUSTOM_ISRS
int16u microGenericIsr ( int16u interrupt, int16u pcbContext )
{
  return interrupt;
}

int16u halInternalSc2Isr(int16u interrupt, int16u pcbContext)
{
  return interrupt;
}
#endif//EMBER_APPLICATION_HAS_CUSTOM_ISRS
#endif//XAP2B
