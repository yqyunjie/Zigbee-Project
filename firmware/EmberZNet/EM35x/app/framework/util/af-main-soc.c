// *******************************************************************
// * af-main-soc.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/util/serial/command-interpreter2.h"

// ZDO
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "app/util/zigbee-framework/zigbee-device-library.h"

#include "app/util/counters/counters.h"
#include "app/util/security/security.h"
#include "app/util/common/form-and-join.h"

#include "app/framework/util/service-discovery.h"
#include "app/framework/util/af-main.h"
#include "app/framework/util/util.h"

#include "app/framework/security/af-security.h"

#include "app/framework/plugin/test-harness/test-harness-cli.h"
#include "app/framework/plugin/partner-link-key-exchange/partner-link-key-exchange.h"
#include "app/framework/plugin/fragmentation/fragmentation.h"

#if defined(__ICCARM__)
  #define EM35X_SERIES
#endif

#if defined(EM35X_SERIES)
#include "hal/micro/cortexm3/diagnostic.h"
#endif

// *****************************************************************************
// Globals

// APP_SERIAL is set in the project files
int8u serialPort = APP_SERIAL;

#if (EMBER_AF_BAUD_RATE == 300)
  #define BAUD_RATE BAUD_300
#elif (EMBER_AF_BAUD_RATE == 600)
  #define BAUD_RATE BAUD_600
#elif (EMBER_AF_BAUD_RATE == 900)
  #define BAUD_RATE BAUD_900
#elif (EMBER_AF_BAUD_RATE == 1200)
  #define BAUD_RATE BAUD_1200
#elif (EMBER_AF_BAUD_RATE == 2400)
  #define BAUD_RATE BAUD_2400
#elif (EMBER_AF_BAUD_RATE == 4800)
  #define BAUD_RATE BAUD_4800
#elif (EMBER_AF_BAUD_RATE == 9600)
  #define BAUD_RATE BAUD_9600
#elif (EMBER_AF_BAUD_RATE == 14400)
  #define BAUD_RATE BAUD_14400
#elif (EMBER_AF_BAUD_RATE == 19200)
  #define BAUD_RATE BAUD_19200
#elif (EMBER_AF_BAUD_RATE == 28800)
  #define BAUD_RATE BAUD_28800
#elif (EMBER_AF_BAUD_RATE == 38400)
  #define BAUD_RATE BAUD_38400
#elif (EMBER_AF_BAUD_RATE == 50000)
  #define BAUD_RATE BAUD_50000
#elif (EMBER_AF_BAUD_RATE == 57600)
  #define BAUD_RATE BAUD_57600
#elif (EMBER_AF_BAUD_RATE == 76800)
  #define BAUD_RATE BAUD_76800
#elif (EMBER_AF_BAUD_RATE == 100000)
  #define BAUD_RATE BAUD_100000
#elif (EMBER_AF_BAUD_RATE == 115200)
  #define BAUD_RATE BAUD_115200
#elif (EMBER_AF_BAUD_RATE == 230400)
  #define BAUD_RATE BAUD_230400
#elif (EMBER_AF_BAUD_RATE == 460800)
  #define BAUD_RATE BAUD_460800
#else
  #error EMBER_AF_BAUD_RATE set to an invalid baud rate
#endif

#if defined(MAIN_FUNCTION_HAS_STANDARD_ARGUMENTS)
  #define APP_FRAMEWORK_MAIN_ARGUMENTS argc, argv
#else
  #define APP_FRAMEWORK_MAIN_ARGUMENTS 0, NULL
#endif

// *****************************************************************************
// Forward declarations.

#if defined(EMBER_TEST) && defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM)
void emAfSetupFakeEepromForSimulation(void);
#endif

#if defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)
  #define COMMAND_READER_INIT() emberCommandReaderInit()
#else
  #define COMMAND_READER_INIT()
#endif

#ifdef EMBER_AF_DISABLE_FORM_AND_JOIN_TICK
  #define FORM_AND_JOIN_TICK()
#else
  #define FORM_AND_JOIN_TICK() emberFormAndJoinTick()
#endif

#ifdef ZA_TINY_SERIAL_INPUT
  /** @deprecated */
  #define TINY_SERIAL_INPUT_TICK() emAfTinySerialInputTick()
#else
  /** @deprecated */
  #define TINY_SERIAL_INPUT_TICK()
#endif

#ifdef DEBUG
  #define DEBUG_INIT(port) emberDebugInit(port)
  #define SERIAL_BUFFER_TICK() emberSerialBufferTick()
#else
  #define DEBUG_INIT(port)
  #define SERIAL_BUFFER_TICK()
#endif

// *****************************************************************************
// Functions

void main(void)
{
  EmberStatus status;
  int8u reset = halGetResetInfo();
  int16u extendedResetInfo = 0;

#if defined(EM35X_SERIES)
  // Assume we are on the 35x SOC
  extendedResetInfo = halGetExtendedResetInfo();
#endif

#if defined(EMBER_TEST) && defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM)
  emAfSetupFakeEepromForSimulation();
#endif

  //Initialize the hal
  halInit();
  INTERRUPTS_ON();  // Safe to enable interrupts at this point

  {
    // The SOC does not support a return code from main().  But for consistency
    // and to avoid the callback referencing a NULL pointer, we pass in a valid 
    // pointer.
    int returnCode;
    if (emberAfMainStartCallback(&returnCode, APP_FRAMEWORK_MAIN_ARGUMENTS)) {
      return;
    }
  }

  // Initialize the Ember Stack.
  status = emberInit();

  DEBUG_INIT(0); // Use serial port 0 for debug output

  emberSerialInit(APP_SERIAL, BAUD_RATE, PARITY_NONE, 1);

  emberAfCorePrintln("Reset info: 0x%x (%p)", 
                     reset,
                     halGetResetString());

#if defined(EM35X_SERIES)
  emberAfCorePrintln("Extended Reset info: 0x%2X (%p)",
                     extendedResetInfo,
                     halGetExtendedResetString());

  if (halResetWasCrash()) {
    halPrintCrashSummary(serialPort);
    halPrintCrashDetails(serialPort);
    halPrintCrashData(serialPort);
  }

#endif

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("%pemberInit 0x%x", "ERROR: ", status);

    // The app can choose what to do here.  If the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
    assert(FALSE);
  } else {
    emberAfDebugPrintln("init pass");
  }

  // This will initialize the stack of networks maintained by the framework,
  // including setting the default network.
  emAfInitializeNetworkIndexStack();

  emberAfEndpointConfigure();
  emberAfMainInitCallback();

  emberAfInit();

  // The address cache needs to be initialized and used with the source routing
  // code for the trust center to operate properly.
  securityAddressCacheInit(EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE,                     // offset
                           EMBER_AF_PLUGIN_ADDRESS_TABLE_TRUST_CENTER_CACHE_SIZE); // size

  // network init if possible - the node type this device was previously
  // needs to match or the device can be a ZC and joined a network as a ZR.
  {
    int8u i;
    for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
      EmberNodeType nodeType;
      emberAfPushNetworkIndex(i);
      if (emAfNetworks[i].nodeType == EMBER_COORDINATOR) {
        zaTrustCenterSecurityPolicyInit();
      }
      if (emberGetNodeType(&nodeType) == EMBER_SUCCESS
          && (nodeType == emAfNetworks[i].nodeType
              || (nodeType == EMBER_ROUTER
                  && emAfNetworks[i].nodeType == EMBER_COORDINATOR))) {
        emAfNetworkInit();
      }
      emberAfPopNetworkIndex();
    }
  }

  COMMAND_READER_INIT();

  // Set the manufacturing code. This is defined by ZigBee document 053874r10
  // Ember's ID is 0x1002 and is the default, but this can be overridden in App Builder.
  emberSetManufacturerCode(EMBER_AF_MANUFACTURER_CODE);

  emberSetMaximumIncomingTransferSize(EMBER_AF_INCOMING_BUFFER_LENGTH);
  emberSetMaximumOutgoingTransferSize(EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH);
  emberSetTxPowerMode(EMBER_AF_TX_POWER_MODE);

  while(TRUE) {
    halResetWatchdog();   // Periodically reset the watchdog.
    emberTick();          // Allow the stack to run.

    // Allow the ZCL clusters to run. This should go immediately after emberTick
    emberAfTick();

    FORM_AND_JOIN_TICK();
    /** @deprecated */
    TINY_SERIAL_INPUT_TICK();
    SERIAL_BUFFER_TICK();

    emberAfRunEvents();

#if defined(ZA_CLI_MINIMAL) || defined(ZA_CLI_FULL)
    if (emberProcessCommandInput(APP_SERIAL)) {
      emberAfGuaranteedPrint("%p>", ZA_PROMPT);
    }
#endif

#if defined(EMBER_TEST)
    if (1) {
      // Simulation only
      int32u timeToNextEventMax = emberMsToNextStackEvent();
      timeToNextEventMax = emberAfMsToNextEvent(timeToNextEventMax);
      simulatedTimePassesMs(timeToNextEventMax);
    }
#endif

    // After each interation through the main loop, our network index stack
    // should be empty and we should be on the default network index again.
    emAfAssertNetworkIndexStackIsEmpty();
  }
}

void emberAfGetMfgString(int8u* returnData)
{
  halCommonGetMfgToken(returnData, TOKEN_MFG_STRING);
}

EmberNodeId emberAfGetNodeId(void)
{
  return emberGetNodeId();
}

EmberPanId emberAfGetPanId(void)
{
  return emberGetPanId();
}

int8u emberAfGetBindingIndex(void)
{
  return emberGetBindingIndex();
}

int8u emberAfGetStackProfile(void)
{
  return EMBER_STACK_PROFILE;
}

int8u emberAfGetAddressIndex(void)
{
  EmberNodeId nodeId = emberGetSender();
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE; i++) {
    if (emberGetAddressTableRemoteNodeId(i) == nodeId) {
      return i;
    }
  }
  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

// ******************************************************************
// binding
// ******************************************************************
EmberStatus emberAfSendEndDeviceBind(int8u endpoint)
{
  EmberStatus status;
  EmberApsOption options = ((EMBER_AF_DEFAULT_APS_OPTIONS
                             | EMBER_APS_OPTION_SOURCE_EUI64)
                            & ~EMBER_APS_OPTION_RETRY);

  status = emberAfPushEndpointNetworkIndex(endpoint);
  if (status != EMBER_SUCCESS) {
    return status;
  }

  emberAfZdoPrintln("send %x %2x", endpoint, options);
  status = emberEndDeviceBindRequest(endpoint, options);
  emberAfZdoPrintln("done: %x.", status);
  emberAfZdoFlush();

  emberAfPopNetworkIndex();
  return status;
}

EmberStatus emberRemoteSetBindingHandler(EmberBindingTableEntry *entry)
{
  EmberStatus status = EMBER_TABLE_FULL;
  EmberBindingTableEntry candidate;
  int8u i;

  emberAfPushCallbackNetworkIndex();

  // If we receive a bind request for the Key Establishment cluster and we are
  // not the trust center, then we are doing partner link key exchange.  We
  // don't actually have to create a binding.
  if (emberAfGetNodeId() != EMBER_TRUST_CENTER_NODE_ID
      && entry->clusterId == ZCL_KEY_ESTABLISHMENT_CLUSTER_ID) {
    status = emberAfPartnerLinkKeyExchangeRequestCallback(entry->identifier);
    goto kickout;
  }

  // For all other requests, we search the binding table for an unused entry
  // and store the new entry there if we find one.
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    if (emberGetBinding(i, &candidate) == EMBER_SUCCESS
        && candidate.type == EMBER_UNUSED_BINDING) {
      status = emberSetBinding(i, entry);
      goto kickout;
    }
  }

kickout:
  emberAfPopNetworkIndex();
  return status;
}

EmberStatus emberRemoteDeleteBindingHandler(int8u index)
{
  EmberStatus status;
  emberAfPushCallbackNetworkIndex();
  status = emberDeleteBinding(index);
  emberAfZdoPrintln("delete binding: %x %x", index, status);
  emberAfPopNetworkIndex();
  return status;
}

// ******************************************************************
// setup endpoints and clusters for responding to ZDO requests
// ******************************************************************
int8u emberGetEndpoint(int8u index)
{
  return (((emberAfNetworkIndexFromEndpointIndex(index)
           == emberGetCallbackNetwork())
           && emberAfEndpointIndexIsEnabled(index))
          ? emberAfEndpointFromIndex(index)
          : 0xFF);
}

// must return the endpoint desc of the endpoint specified
boolean emberGetEndpointDescription(int8u endpoint,
                                    EmberEndpointDescription *result)
{
  int8u endpointIndex = emberAfIndexFromEndpoint(endpoint);
  if (endpointIndex == 0xFF
      || (emberAfNetworkIndexFromEndpointIndex(endpointIndex)
          != emberGetCallbackNetwork())) {
    return FALSE;
  }
  result->profileId          = emberAfProfileIdFromIndex(endpointIndex);
  result->deviceId           = emberAfDeviceIdFromIndex(endpointIndex);
  result->deviceVersion      = emberAfDeviceVersionFromIndex(endpointIndex);
  result->inputClusterCount  = emberAfClusterCount(endpoint, TRUE);
  result->outputClusterCount = emberAfClusterCount(endpoint, FALSE);
  return TRUE;
}

// must return the clusterId at listIndex in the list specified for the
// endpoint specified
int16u emberGetEndpointCluster(int8u endpoint,
                               EmberClusterListId listId,
                               int8u listIndex)
{
  EmberAfCluster *cluster = NULL;
  int8u endpointIndex = emberAfIndexFromEndpoint(endpoint);
  if (endpointIndex == 0xFF
      || (emberAfNetworkIndexFromEndpointIndex(endpointIndex)
          != emberGetCallbackNetwork())) {
    return 0xFFFF;
  } else if (listId == EMBER_INPUT_CLUSTER_LIST) {
    cluster = emberAfGetNthCluster(endpoint, listIndex, TRUE);
  } else if (listId == EMBER_OUTPUT_CLUSTER_LIST) {
    cluster = emberAfGetNthCluster(endpoint, listIndex, FALSE);
  }
  return (cluster == NULL ? 0xFFFF : cluster->clusterId);
}


// *******************************************************************
// Handlers required to use the Ember Stack.

// Called when the stack status changes, usually as a result of an
// attempt to form, join, or leave a network.
void emberStackStatusHandler(EmberStatus status)
{
  emberAfPushCallbackNetworkIndex();
  emAfStackStatusHandler(status);
  emberAfPopNetworkIndex();
}

// Copy the message buffer into a RAM buffer.
//   If message is too large, 0 is returned and no copying is done.
//   Otherwise data is copied, and length of copied data is returned.
int8u emAfCopyMessageIntoRamBuffer(EmberMessageBuffer message,
                                   int8u *buffer,
                                   int16u bufLen)
{
  int8u length = emberMessageBufferLength(message);
  if (bufLen < length) {
    emberAfAppPrintln("%pmsg too big (%d > %d)", 
                      "ERROR: ", 
                      length, 
                      bufLen);
    return 0;
  }
  emberCopyFromLinkedBuffers(message, 0, buffer, length); // no offset
  return length;
}

void emberIncomingMessageHandler(EmberIncomingMessageType type,
                                 EmberApsFrame *apsFrame,
                                 EmberMessageBuffer message)
{
  int8u lastHopLqi;
  int8s lastHopRssi;
  int16u messageLength;
  int8u messageContents[EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH];

  emberAfPushCallbackNetworkIndex();

  messageLength = emAfCopyMessageIntoRamBuffer(message,
                                               messageContents,
                                               EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH);
  if (messageLength == 0) {
    goto kickout;
  }

  emberGetLastHopLqi(&lastHopLqi);
  emberGetLastHopRssi(&lastHopRssi);

  emAfIncomingMessageHandler(type,
                             apsFrame,
                             lastHopLqi,
                             lastHopRssi,
                             messageLength,
                             messageContents);

kickout:
  emberAfPopNetworkIndex();
}


// Called when a message we sent is acked by the destination or when an
// ack fails to arrive after several retransmissions.
void emberMessageSentHandler(EmberOutgoingMessageType type,
                             int16u indexOrDestination,
                             EmberApsFrame *apsFrame,
                             EmberMessageBuffer message,
                             EmberStatus status)
{
  int8u messageContents[EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH];
  int8u messageLength;

  emberAfPushCallbackNetworkIndex();

#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
  if (emAfFragmentationMessageSent(apsFrame, status)) {
    goto kickout;
  }
#endif //EMBER_AF_PLUGIN_FRAGMENTATION
  
  messageLength = emAfCopyMessageIntoRamBuffer(message,
                                               messageContents,
                                               EMBER_AF_MAXIMUM_APS_PAYLOAD_LENGTH);
  if (messageLength == 0) {
    // Message too long.  Error printed by above function.
    goto kickout;
  }

  emAfMessageSentHandler(type,
                         indexOrDestination,
                         apsFrame,
                         status,
                         messageLength,
                         messageContents);

kickout:
  emberAfPopNetworkIndex();
}

EmberStatus emAfSend(EmberOutgoingMessageType type,
                     int16u indexOrDestination,
                     EmberApsFrame *apsFrame,
                     int8u messageLength,
                     int8u *message)
{
  EmberMessageBuffer payload = emberFillLinkedBuffers(message, messageLength);
  if (payload == EMBER_NULL_MESSAGE_BUFFER) {
    return EMBER_NO_BUFFERS;
  } else {
    EmberStatus status;
    switch (type) {
    case EMBER_OUTGOING_DIRECT:
    case EMBER_OUTGOING_VIA_ADDRESS_TABLE:
    case EMBER_OUTGOING_VIA_BINDING:
      status = emberSendUnicast(type, indexOrDestination, apsFrame, payload);
      break;
    case EMBER_OUTGOING_MULTICAST:
      status = emberSendMulticast(apsFrame,
                                  ZA_MAX_HOPS, // radius
                                  ZA_MAX_HOPS, // nonmember radius
                                  payload);
      break;
    case EMBER_OUTGOING_BROADCAST:
      status = emberSendBroadcast(indexOrDestination,
                                  apsFrame,
                                  ZA_MAX_HOPS, // radius
                                  payload);
      break;
    default:
      status = EMBER_BAD_ARGUMENT;
      break;
    }
    emberReleaseMessageBuffer(payload);
    return status;
  }
}

void emberAfGetEui64(EmberEUI64 returnEui64)
{
  MEMCOPY(returnEui64, emberGetEui64(), EUI64_SIZE);
}

EmberStatus emberAfGetNetworkParameters(EmberNodeType* nodeType, 
                                        EmberNetworkParameters* parameters)
{
  EmberStatus status;
  status = emberGetNodeType(nodeType);
  if (status != EMBER_SUCCESS) {
    goto kickout;
  }
  status = emberGetNetworkParameters(parameters);

kickout:
  return status;
}

int8u emberAfGetSecurityLevel(void)
{
  return EMBER_SECURITY_LEVEL;
}

int8u emberAfGetKeyTableSize(void)
{
  return EMBER_KEY_TABLE_SIZE;
}

int8u emberAfGetBindingTableSize(void)
{
  return EMBER_BINDING_TABLE_SIZE;
}

int8u emberAfGetAddressTableSize(void)
{
  return EMBER_ADDRESS_TABLE_SIZE;
}

int8u emberAfGetChildTableSize(void)
{
  return EMBER_CHILD_TABLE_SIZE;
}

int8u emberAfGetNeighborTableSize(void)
{
  return EMBER_NEIGHBOR_TABLE_SIZE;
}

int8u emberAfGetRouteTableSize(void)
{
  return EMBER_ROUTE_TABLE_SIZE;
}

int8u emberAfGetSleepyMulticastConfig(void)
{
  return EMBER_SEND_MULTICASTS_TO_SLEEPY_ADDRESS;
}

EmberStatus emberAfGetChildData(int8u index,
                                EmberNodeId *childId,
                                EmberEUI64 childEui64,
                                EmberNodeType *childType)
{
  *childId = emberChildId(index);
  return emberGetChildData(index,
                           childEui64,
                           childType);
}

void emAfCopyCounterValues(int16u* counters)
{
  MEMCOPY(counters, emberCounters, sizeof(emberCounters));

  // To maintain parity between the SOC and the Host code,
  // we clear the counters after reading them because that is
  // what happens with the host.
  emberClearCounters();
}

int8u emAfGetPacketBufferFreeCount(void)
{
  return emberPacketBufferFreeCount();
}

int8u emAfGetPacketBufferTotalCount(void)
{
  return EMBER_PACKET_BUFFER_COUNT;
}

void emAfCliVersionCommand(void)
{
  emAfParseAndPrintVersion(emberVersion);
}

void emberNetworkFoundHandler(EmberZigbeeNetwork *networkFound)
{
  int8u lqi;
  int8s rssi;
  emberAfPushCallbackNetworkIndex();
  emberGetLastHopLqi(&lqi);
  emberGetLastHopRssi(&rssi);
  emberFormAndJoinNetworkFoundHandler(networkFound, lqi, rssi);
  emberAfPopNetworkIndex();
}

void emberScanCompleteHandler(int8u channel, EmberStatus status)
{
  emberAfPushCallbackNetworkIndex();
  emberFormAndJoinScanCompleteHandler(channel, status);
  emberAfPopNetworkIndex();
}

void emberEnergyScanResultHandler(int8u channel, int8s rssi)
{
  emberAfPushCallbackNetworkIndex();
  emberFormAndJoinEnergyScanResultHandler(channel, rssi);
  emberAfPopNetworkIndex();
}

void emAfPrintEzspEndpointFlags(int8u endpoint)
{
  // Not applicable for SOC
}
