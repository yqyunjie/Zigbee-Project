// *******************************************************************
// * af-main-host.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"

// CLI - command line interface
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/cli/core-cli.h"
#include "app/framework/cli/custom-cli.h"
#include "app/framework/cli/network-cli.h"
#include "app/framework/cli/plugin-cli.h"
#include "app/framework/cli/security-cli.h"
#include "app/framework/cli/zcl-cli.h"
#include "app/framework/cli/zdo-cli.h"
#include "app/framework/plugin/test-harness/test-harness-cli.h"

// ZCL - ZigBee Cluster Library
#include "app/framework/util/attribute-storage.h"
#include "app/framework/util/util.h"
#include "app/framework/util/af-event.h"

// ZDO - ZigBee Device Object
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "app/util/zigbee-framework/zigbee-device-host.h"

// Service discovery library
#include "service-discovery.h"

// Fragmentation
#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
#include "app/framework/plugin/fragmentation/fragmentation.h"
#endif
#include "app/util/source-route-host.h"

// determines the number of in-clusters and out-clusters based on defines
// in config.h
#include "app/framework/util/af-main.h"

// Needed for zaTrustCenterSecurityPolicyInit()
#include "app/framework/security/af-security.h"

#ifdef GATEWAY_APP
  #define COMMAND_INTERPRETER_SUPPORT
  #include "app/util/gateway/gateway.h"
#endif

#include "app/util/security/security.h"  // Trust Center Address Cache
#include "app/util/common/form-and-join.h"

#include "app/framework/plugin/partner-link-key-exchange/partner-link-key-exchange.h"
#include "app/util/common/library.h"
#include "app/framework/security/crypto-state.h"

// This is used to store the local EUI of the NCP when using
// fake certificates.
// Fake certificates are constructed by setting the data to all F's
// but using the device's real IEEE in the cert.  The Key establishment
// code requires access to the local IEEE to do this.
EmberEUI64 emLocalEui64;

// APP_SERIAL is set in the project files
int8u serialPort = APP_SERIAL;

typedef struct {
  EmberNodeId nodeId;
  EmberPanId  panId;
} NetworkCache;
static NetworkCache networkCache[EMBER_SUPPORTED_NETWORKS];

// the stack version that the NCP is running
static int16u ncpStackVer;

#if defined(EMBER_TEST)
  #define EMBER_TEST_ASSERT(x) assert(x)
#else
  #define EMBER_TEST_ASSERT(x)
#endif

#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
  #define SOURCE_ROUTE_TABLE_SIZE EMBER_SOURCE_ROUTE_TABLE_SIZE
#else
  // Define a small size that will not consume much memory on NCP
  #define SOURCE_ROUTE_TABLE_SIZE 2
#endif

// ******************************************************************
// Globals 

// when this is set to TRUE it means the NCP has reported a serious error
// and the host needs to reset and re-init the NCP
static boolean ncpNeedsResetAndInit = FALSE;

// Declarations related to idling
#ifndef EMBER_TASK_COUNT
  #define EMBER_TASK_COUNT (3)
#endif
EmberTaskControl emTasks[EMBER_TASK_COUNT];
PGM int8u emTaskCount = EMBER_TASK_COUNT;
static int8u emActiveTaskCount = 0;


#if defined (GATEWAY_APP)
  // Baud rate on the gateway application is meaningless, but we must
  // define it satisfy compilation.
  #define BAUD_RATE BAUD_115200

  // Port 1 on the gateway application is used for CLI, while port 0
  // is used for "raw" binary.  We ignore what was set via App. Builder
  // since only port 1 is used by the application.
  #undef APP_SERIAL
  #define APP_SERIAL 1

#elif (EMBER_AF_BAUD_RATE == 300)
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

#define MAX_CLUSTER 58

// We only get the sender EUI callback when the sender EUI is in the incoming
// message. This keeps track of if the value in the variable is valid or not.
// This is set to VALID (TRUE) when the callback happens and set to INVALID
// (FALSE) at the end of IncomingMessageHandler.
static boolean currentSenderEui64IsValid;
static EmberEUI64 currentSenderEui64;
static EmberNodeId currentSender = EMBER_NULL_NODE_ID;
static int8u currentBindingIndex = EMBER_NULL_BINDING;
static int8u currentAddressIndex = EMBER_NULL_ADDRESS_TABLE_INDEX;

#if defined(EMBER_TEST) && defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM)
void emAfSetupFakeEepromForSimulation(void);
#endif

#if defined(MAIN_FUNCTION_HAS_STANDARD_ARGUMENTS)
  #define APP_FRAMEWORK_MAIN_ARGUMENTS argc, argv
#else
  #define APP_FRAMEWORK_MAIN_ARGUMENTS 0, NULL
#endif

static int16u cachedConfigIdValues[EZSP_CONFIG_ID_MAX + 1];
static boolean cacheConfigIdValuesAllowed = FALSE;

//------------------------------------------------------------------------------
// Forward declarations

#ifdef EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_DECLARATIONS
  EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_DECLARATIONS
#endif


static EzspStatus createEndpoint(int8u endpoint);

//------------------------------------------------------------------------------
// Functions

boolean emberAfNcpNeedsReset(void)
{
  return ncpNeedsResetAndInit;
}

static void storeLowHighInt16u(int8u* contents, int16u value)
{
  contents[0] = LOW_BYTE(value);
  contents[1] = HIGH_BYTE(value);
}

// Because an EZSP call can be expensive in terms of bandwidth,
// we cache the node ID so it can be quickly retrieved by the host.
EmberNodeId emberAfGetNodeId(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  if (networkCache[networkIndex].nodeId == EMBER_NULL_NODE_ID) {
    networkCache[networkIndex].nodeId = emberGetNodeId();
  }
  return networkCache[networkIndex].nodeId;
}

EmberPanId emberAfGetPanId(void)
{
  int8u networkIndex = emberGetCurrentNetwork();
  if (networkCache[networkIndex].panId == 0xFFFF) {
    EmberNodeType nodeType;
    EmberNetworkParameters parameters;
    emberAfGetNetworkParameters(&nodeType, &parameters);
    networkCache[networkIndex].panId = parameters.panId;
  }
  return networkCache[networkIndex].panId;
}

void emberAfGetMfgString(int8u* returnData)
{
  static int8u mfgString[MFG_STRING_MAX_LENGTH];
  static boolean mfgStringRetrieved = FALSE;

  if (mfgStringRetrieved == FALSE) {
    ezspGetMfgToken(EZSP_MFG_STRING, mfgString);
    mfgStringRetrieved = TRUE;
  }
  // NOTE:  The MFG string is not NULL terminated.
  MEMCOPY(returnData, mfgString, MFG_STRING_MAX_LENGTH);
}

void emAfClearNetworkCache(int8u networkIndex)
{
  networkCache[networkIndex].nodeId = EMBER_NULL_NODE_ID;
  networkCache[networkIndex].panId = 0xFFFF;
}

// This is an ineffecient way to generate a random key for the host.
// If there is a pseudo random number generator available on the host,
// that may be a better mechanism.

EmberStatus emberAfGenerateRandomKey(EmberKeyData* result)
{
  int16u data;
  int8u* keyPtr = emberKeyContents(result);
  int8u i;

  // Since our EZSP command only generates a random 16-bit number,
  // we must call it repeatedly to get a 128-bit random number.

  for ( i = 0; i < 8; i++ ) {
    EmberStatus status = ezspGetRandomNumber(&data);

    if ( status != EMBER_SUCCESS ) {
      return status;
    }

    storeLowHighInt16u(keyPtr, data);
    keyPtr+=2;
  }

  return EMBER_SUCCESS;
}

// Some NCP's support a 'maximize packet buffer' call.  If that doesn't
// work, slowly ratchet up the packet buffer count.
static void setPacketBufferCount(void)
{
  int16u value;
  EzspStatus ezspStatus;
  EzspStatus maxOutBufferStatus;

  maxOutBufferStatus
    = ezspSetConfigurationValue(EZSP_CONFIG_PACKET_BUFFER_COUNT,
                                EZSP_MAXIMIZE_PACKET_BUFFER_COUNT);
  
  // We start from the default used by the NCP and increase up from there
  // rather than use a hard coded default in the code.
  // This is more portable to different NCP hardware (i.e. 357 vs. 260).
  ezspStatus = ezspGetConfigurationValue(EZSP_CONFIG_PACKET_BUFFER_COUNT,
                                         &value);

  if (maxOutBufferStatus == EZSP_SUCCESS) {
    emberAfAppPrintln("NCP supports maxing out packet buffers");
    goto setPacketBufferCountDone;
  }

  while (ezspStatus == EZSP_SUCCESS) {
    value++;
    ezspStatus = ezspSetConfigurationValue(EZSP_CONFIG_PACKET_BUFFER_COUNT,
                                           value);
  }

 setPacketBufferCountDone:
  emberAfAppPrintln("Ezsp Config: set packet buffers to %d",
                    (maxOutBufferStatus == EZSP_SUCCESS
                     ? value
                     : value - 1));
  emberAfAppFlush();
}

// initialize the network co-processor (NCP)
void emAfResetAndInitNCP(void)
{
  int8u ep;
  EmberStatus status;
  EzspStatus ezspStatus;
  boolean memoryAllocation;

  emberAfPreNcpResetCallback();

  // ezspInit resets the NCP by calling halNcpHardReset on a SPI host or
  // ashResetNcp on a UART host
  ezspStatus = ezspInit();

  if (ezspStatus != EZSP_SUCCESS) {
    emberAfCorePrintln("ERROR: ezspForceReset 0x%x", ezspStatus);
    emberAfCoreFlush();
    assert(FALSE);
  }

  // send the version command before any other commands
  emAfCliVersionCommand();

#ifdef EMBER_MAX_END_DEVICE_CHILDREN
  // BUG 14223: If EMBER_MAX_END_DEVICE_CHILDREN is defined, the AF's NCP init
  // code should set the NCP's config value accordingly to match this.
  emberAfSetEzspConfigValue(EZSP_CONFIG_MAX_END_DEVICE_CHILDREN,
                            EMBER_MAX_END_DEVICE_CHILDREN,
                            "max end device children");
#endif // EMBER_MAX_END_DEVICE_CHILDREN

  // set the binding table size
  emberAfSetEzspConfigValue(EZSP_CONFIG_BINDING_TABLE_SIZE,
                            EMBER_BINDING_TABLE_SIZE,
                            "binding table size");

  // set the source route table size
  emberAfSetEzspConfigValue(EZSP_CONFIG_SOURCE_ROUTE_TABLE_SIZE,
                            SOURCE_ROUTE_TABLE_SIZE,
                            "source route table size");

  emberAfSetEzspConfigValue(EZSP_CONFIG_SECURITY_LEVEL,
                            EMBER_SECURITY_LEVEL,
                            "security level");

  // set the address table size
  emberAfSetEzspConfigValue(EZSP_CONFIG_ADDRESS_TABLE_SIZE,
                            EMBER_AF_PLUGIN_ADDRESS_TABLE_SIZE,
                            "address table size");

  // set the trust center address cache size
  emberAfSetEzspConfigValue(EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE,
                            EMBER_AF_PLUGIN_ADDRESS_TABLE_TRUST_CENTER_CACHE_SIZE,
                            "TC addr cache");

  // the stack profile is defined in the config file
  emberAfSetEzspConfigValue(EZSP_CONFIG_STACK_PROFILE,
                            EMBER_STACK_PROFILE,
                            "stack profile");

  // BUG 14222: If stack profile is 2 (ZigBee Pro), we need to enforce
  // the standard stack configuration values for that feature set.
  if( EMBER_STACK_PROFILE == 2 )
  {
    // MAC indirect timeout should be 7.68 secs
    emberAfSetEzspConfigValue(EZSP_CONFIG_INDIRECT_TRANSMISSION_TIMEOUT,
                              7680,
                              "MAC indirect TX timeout");

    // Max hops should be 2 * nwkMaxDepth, where nwkMaxDepth is 15
    emberAfSetEzspConfigValue(EZSP_CONFIG_MAX_HOPS,
                              30,
                              "max hops");
  }

  emberAfSetEzspConfigValue(EZSP_CONFIG_KEY_TABLE_SIZE,
                            EMBER_KEY_TABLE_SIZE,
                            "key table size");

  emberAfSetEzspConfigValue(EZSP_CONFIG_TX_POWER_MODE,
                            EMBER_AF_TX_POWER_MODE,
                            "tx power mode");

  emberAfSetEzspConfigValue(EZSP_CONFIG_SUPPORTED_NETWORKS,
                            EMBER_SUPPORTED_NETWORKS,
                            "supported networks");

  // allow other devices to modify the binding table
  emberAfSetEzspPolicy(EZSP_BINDING_MODIFICATION_POLICY,
                       EZSP_ALLOW_BINDING_MODIFICATION,
                       "binding modify",
                       "allow");

  // return message tag and message contents in ezspMessageSentHandler()
  emberAfSetEzspPolicy(EZSP_MESSAGE_CONTENTS_IN_CALLBACK_POLICY,
                       EZSP_MESSAGE_TAG_AND_CONTENTS_IN_CALLBACK,
                       "message content in msgSent",
                       "return");

  {
    int8u value[2];
    value[0] = LOW_BYTE(EMBER_AF_INCOMING_BUFFER_LENGTH);
    value[1] = HIGH_BYTE(EMBER_AF_INCOMING_BUFFER_LENGTH);
    emberAfSetEzspValue(EZSP_VALUE_MAXIMUM_INCOMING_TRANSFER_SIZE,
                        2,     // value length
                        value,
                        "maximum incoming transfer size");
    value[0] = LOW_BYTE(EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH);
    value[1] = HIGH_BYTE(EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH);
    emberAfSetEzspValue(EZSP_VALUE_MAXIMUM_OUTGOING_TRANSFER_SIZE,
                        2,     // value length
                        value,
                        "maximum outgoing transfer size");
  }

  // Set the manufacturing code. This is defined by ZigBee document 053874r10
  // Ember's ID is 0x1002 and is the default, but this can be overridden in App Builder.
  emberSetManufacturerCode(EMBER_AF_MANUFACTURER_CODE);

  // Call the plugin and user-specific NCP inits.  This is when configuration
  // that affects table sizes should occur, which means it must happen before
  // setPacketBufferCount.
  memoryAllocation = TRUE;
#ifdef EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_CALLS
  EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_CALLS
#endif
  emberAfNcpInitCallback(memoryAllocation);

  setPacketBufferCount();

  // Call the plugin and user-specific NCP inits again.  This is where non-
  // sizing configuration should occur.
  memoryAllocation = FALSE;
#ifdef EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_CALLS
    EMBER_AF_GENERATED_PLUGIN_NCP_INIT_FUNCTION_CALLS
#endif
  emberAfNcpInitCallback(memoryAllocation);

  // create endpoints
  for ( ep = 0; ep < emberAfEndpointCount(); ep++ ) {
    createEndpoint(emberAfEndpointFromIndex(ep));
  }

  // network init if possible - the node type this device was previously
  // needs to match or the device can be a ZC and joined a network as a ZR.
  {
    int8u i;
    for (i = 0; i < EMBER_SUPPORTED_NETWORKS; i++) {
      EmberNodeType nodeType;
      EmberNetworkParameters parameters;
      emberAfPushNetworkIndex(i);
      emAfClearNetworkCache(i);
      if (emAfNetworks[i].nodeType == EMBER_COORDINATOR) {
        zaTrustCenterSecurityPolicyInit();
      }
      if (ezspGetNetworkParameters(&nodeType, &parameters) == EMBER_SUCCESS
          && (nodeType == emAfNetworks[i].nodeType
              || (nodeType == EMBER_ROUTER
                  && emAfNetworks[i].nodeType == EMBER_COORDINATOR))) {
        emAfNetworkInit();
      }
      emberAfPopNetworkIndex();
    }
  }

  MEMSET(cachedConfigIdValues, 0xFF, ((EZSP_CONFIG_ID_MAX + 1) * sizeof(int16u)));
  cacheConfigIdValuesAllowed = TRUE;
  emberAfGetEui64(emLocalEui64);
}

// *******************************************************************
// *******************************************************************
// The main() loop and the application's contribution.

int main( MAIN_FUNCTION_PARAMETERS )
{
#if defined(EMBER_TEST) && defined(EMBER_AF_PLUGIN_OTA_STORAGE_SIMPLE_EEPROM)
  emAfSetupFakeEepromForSimulation();
#endif

  //Initialize the hal
  halInit();
  INTERRUPTS_ON();  // Safe to enable interrupts at this point

  {
    int returnCode;
    if (emberAfMainStartCallback(&returnCode,
                                 APP_FRAMEWORK_MAIN_ARGUMENTS)) {
      return returnCode;
    }
  }

  emberSerialInit(APP_SERIAL, BAUD_RATE, PARITY_NONE, 1);

  emberAfAppPrintln("Reset info: %d (%p)",
                    halGetResetInfo(),
                    halGetResetString());
  emberAfCoreFlush();

  // This will initialize the stack of networks maintained by the framework,
  // including setting the default network.
  emAfInitializeNetworkIndexStack();

  // We must initialize the endpoint information first so
  // that they are correctly added by emAfResetAndInitNCP()
  emberAfEndpointConfigure();

  // initialize the network co-processor (NCP)
  emAfResetAndInitNCP();

  // Main init callback
  emberAfMainInitCallback();

  // initialize the ZCL Utilities
  emberAfInit();

  emberCommandReaderInit();

  // main loop
  while(TRUE) {
    halResetWatchdog();   // Periodically reset the watchdog.

    // see if the NCP has anything waiting to send us
    ezspTick();

    while (ezspCallbackPending()) {
      ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;
      ezspCallback();
    }

    // check if we have hit an EZSP Error and need to reset and init the NCP
    if (ncpNeedsResetAndInit) {
      ncpNeedsResetAndInit = FALSE;
      // re-initialize the NCP
      emAfResetAndInitNCP();
    }

    // Wait until ECC operations are done.  Don't allow any of the clusters
    // to send messages as the NCP is busy doing ECC
    if (emAfIsCryptoOperationInProgress()) {
      continue;
    }

    // let the ZCL Utils run - this should go after ezspTick
    emberAfTick();

#ifdef ZA_TINY_SERIAL_INPUT
    /** @deprecated */
    emAfTinySerialInputTick();
#endif

    emberAfRunEvents();

    if (emberProcessCommandInput(APP_SERIAL)) {
#if !defined GATEWAY_APP
      // Gateway app. has its own way of handling the command-line prompt.
      emberAfGuaranteedPrint("%p>", ZA_PROMPT);
#endif
    }

#if defined(EMBER_TEST)
    if (1) {
      int32u timeToNextEventMax = emberMsToNextStackEvent();
      timeToNextEventMax = emberAfMsToNextEvent(timeToNextEventMax);
      simulatedTimePassesMs(timeToNextEventMax);
    }
#endif


#ifdef DEBUG
    // Needed for buffered serial, which debug uses
    emberSerialBufferTick();
#endif //DEBUG

    // After each interation through the main loop, our network index stack
    // should be empty and we should be on the default network index again.
    emAfAssertNetworkIndexStackIsEmpty();
  }
  return 0;
}

// ******************************************************************
// binding
// ******************************************************************
EmberStatus emberAfSendEndDeviceBind(int8u endpoint)
{
  EmberStatus status;
  EmberEUI64 eui;
  int8u inClusterCount, outClusterCount;
  EmberAfClusterId clusterList[MAX_CLUSTER];
  EmberAfClusterId *inClusterList;
  EmberAfClusterId *outClusterList;
  EmberAfProfileId profileId;
  EmberApsOption options = ((EMBER_AF_DEFAULT_APS_OPTIONS
                             | EMBER_APS_OPTION_SOURCE_EUI64)
                            & ~EMBER_APS_OPTION_RETRY);
  int8u index = emberAfIndexFromEndpoint(endpoint);
  if (index == 0xFF) {
    return EMBER_INVALID_ENDPOINT;
  }
  status = emberAfPushEndpointNetworkIndex(endpoint);
  if (status != EMBER_SUCCESS) {
    return status;
  }

  emberAfGetEui64(eui);

  emberAfZdoPrintln("send %x %2x ", endpoint, options);
  inClusterList = clusterList;
  inClusterCount = emberAfGetClustersFromEndpoint(endpoint,
                                                  inClusterList,
                                                  MAX_CLUSTER,
                                                  TRUE); // server?
  outClusterList = clusterList + inClusterCount;
  outClusterCount = emberAfGetClustersFromEndpoint(endpoint,
                                                   outClusterList,
                                                   (MAX_CLUSTER
                                                    - inClusterCount),
                                                   FALSE); // server?
  profileId = emberAfProfileIdFromIndex(index);

  status = ezspEndDeviceBindRequest(emberAfGetNodeId(),
                                    eui,
                                    endpoint,
                                    profileId,
                                    inClusterCount,  // cluster in count
                                    outClusterCount, // cluster out count
                                    inClusterList,   // list of input clusters
                                    outClusterList,  // list of output clusters
                                    options);
  emberAfZdoPrintln("done: %x.", status);

  emberAfPopNetworkIndex();
  return status;
}

// **********************************************************************
// this function sets an EZSP config value and prints out the results to
// the serial output
// **********************************************************************
EzspStatus emberAfSetEzspConfigValue(EzspConfigId configId,
                                     int16u value,
                                     PGM_P configIdName)
{

  EzspStatus ezspStatus = ezspSetConfigurationValue(configId, value);
  emberAfAppFlush();
  emberAfAppPrint("Ezsp Config: set %p to 0x%2x:", configIdName, value);

  emberAfAppDebugExec(emAfPrintStatus("set", ezspStatus));
  emberAfAppFlush();

  emberAfAppPrintln("");
  emberAfAppFlush();

  // If this fails, odds are the simulated NCP doesn't have enough
  // memory allocated to it.
  EMBER_TEST_ASSERT(ezspStatus == EZSP_SUCCESS);

  return ezspStatus;
}


// **********************************************************************
// this function sets an EZSP policy and prints out the results to
// the serial output
// **********************************************************************
EzspStatus emberAfSetEzspPolicy(EzspPolicyId policyId,
                                EzspDecisionId decisionId,
                                PGM_P policyName,
                                PGM_P decisionName)
{
  EzspStatus ezspStatus = ezspSetPolicy(policyId,
                                        decisionId);
  emberAfAppPrint("Ezsp Policy: set %p to \"%p\":",
                 policyName,
                 decisionName);
  emberAfAppDebugExec(emAfPrintStatus("set",
                                      ezspStatus));
  emberAfAppPrintln("");
  emberAfAppFlush();
  return ezspStatus;
}

// **********************************************************************
// this function sets an EZSP value and prints out the results to
// the serial output
// **********************************************************************
EzspStatus emberAfSetEzspValue(EzspValueId valueId,
                               int8u valueLength,
                               int8u *value,
                               PGM_P valueName)

{
  EzspStatus ezspStatus = ezspSetValue(valueId, valueLength, value);
  int32u promotedValue;
  if (valueLength <= 4) {
    promotedValue = (int32u)(*value);
  }
  emberAfAppPrint("Ezsp Value : set %p to ", valueName);

  // print the value based on the length of the value
  switch (valueLength){
  case 1:
  case 2:
  case 4:
    emberAfAppPrint("0x%4x:", promotedValue);
    break;
  default:
    emberAfAppPrint("{val of len %x}:", valueLength);
  }

  emberAfAppDebugExec(emAfPrintStatus("set", ezspStatus));
  emberAfAppPrintln("");
  emberAfAppFlush();
  return ezspStatus;
}

// ******************************************************************
// setup endpoints and clusters for responding to ZDO requests
// ******************************************************************

//
// Creates the endpoint for 260 by calling ezspAddEndpoint()
//
static EzspStatus createEndpoint(int8u endpoint) 
{
  EzspStatus status;

  int16u clusterList[MAX_CLUSTER];
  int16u *inClusterList;
  int16u *outClusterList;

  int8u inClusterCount;
  int8u outClusterCount;
  int8u endpointIndex;

  emberAfPushEndpointNetworkIndex(endpoint);

  // Lay out clusters in the arrays.
  inClusterList = clusterList;
  inClusterCount = emberAfGetClustersFromEndpoint(endpoint, inClusterList, MAX_CLUSTER, TRUE);

  outClusterList = clusterList + inClusterCount;
  outClusterCount = emberAfGetClustersFromEndpoint(endpoint, outClusterList, (MAX_CLUSTER - inClusterCount), FALSE);
  endpointIndex = emberAfIndexFromEndpoint(endpoint);

  // Call EZSP function with data.
  status = ezspAddEndpoint(endpoint,
                           emberAfProfileIdFromIndex(endpointIndex),
                           emberAfDeviceIdFromIndex(endpointIndex),
                           emberAfDeviceVersionFromIndex(endpointIndex),
                           inClusterCount,
                           outClusterCount,
                           (int16u *)inClusterList,
                           (int16u *)outClusterList);

  if (status != EZSP_SUCCESS) {
    emberAfAppPrintln("Error in creating endpoint %d: 0x%x", endpoint, status);
  } else {
    emberAfAppPrintln("Ezsp Endpoint %d added, profile 0x%2x, in clusters: %d, out clusters %d",
                      endpoint,
                      emberAfProfileIdFromIndex(endpointIndex),
                      inClusterCount,
                      outClusterCount);
  }

  emberAfPopNetworkIndex();
  return status;
}


// *******************************************************************
// Handlers required to use the Ember Stack.

// Called when the stack status changes, usually as a result of an
// attempt to form, join, or leave a network.
void ezspStackStatusHandler(EmberStatus status)
{
  emberAfPushCallbackNetworkIndex();
  emAfStackStatusHandler(status);
  emberAfPopNetworkIndex();
}

EmberNodeId emberGetSender(void)
{
  return currentSender;
}

int8u emberAfGetBindingIndex(void)
{
  return currentBindingIndex;
}

int8u emberAfGetAddressIndex(void)
{
  return currentAddressIndex;
}

// This is not called if the incoming message did not contain the EUI64 of
// the sender.
void ezspIncomingSenderEui64Handler(EmberEUI64 senderEui64)
{
  // current sender is now valid
  MEMCOPY(currentSenderEui64, senderEui64, EUI64_SIZE);
  currentSenderEui64IsValid = TRUE;
}

EmberStatus emberGetSenderEui64(EmberEUI64 senderEui64)
{
  // if the current sender EUI is valid then copy it in and send it back
  if (currentSenderEui64IsValid) {
    MEMCOPY(senderEui64, currentSenderEui64, EUI64_SIZE);
    return EMBER_SUCCESS;
  }
  // in the not valid case just return error
  return EMBER_ERR_FATAL;
}

//
// ******************************************************************

void ezspIncomingMessageHandler(EmberIncomingMessageType type,
                                EmberApsFrame *apsFrame,
                                int8u lastHopLqi,
                                int8s lastHopRssi,
                                EmberNodeId sender,
                                int8u bindingIndex,
                                int8u addressIndex,
                                int8u messageLength,
                                int8u *messageContents)
{
  emberAfPushCallbackNetworkIndex();
  currentSender = sender;
  currentBindingIndex = bindingIndex;
  currentAddressIndex = addressIndex;
  emAfIncomingMessageHandler(type,
                             apsFrame,
                             lastHopLqi,
                             lastHopRssi,
                             messageLength,
                             messageContents);
  currentSenderEui64IsValid = FALSE;
  currentSender = EMBER_NULL_NODE_ID;
  currentBindingIndex = EMBER_NULL_BINDING;
  currentAddressIndex = EMBER_NULL_ADDRESS_TABLE_INDEX;
  emberAfPopNetworkIndex();
}

// Called when a message we sent is acked by the destination or when an
// ack fails to arrive after several retransmissions.
void ezspMessageSentHandler(EmberOutgoingMessageType type,
                            int16u indexOrDestination,
                            EmberApsFrame *apsFrame,
                            int8u messageTag,
                            EmberStatus status,
                            int8u messageLength,
                            int8u *messageContents)
{
  emberAfPushCallbackNetworkIndex();
#ifdef EMBER_AF_PLUGIN_FRAGMENTATION
  if (emAfFragmentationMessageSent(apsFrame, status)) {
    goto kickout;
  }
#endif //EMBER_AF_PLUGIN_FRAGMENTATION
  emAfMessageSentHandler(type,
                         indexOrDestination,
                         apsFrame,
                         status,
                         messageLength,
                         messageContents);
  goto kickout; // silence a warning when not using fragmentation
kickout:
  emberAfPopNetworkIndex();
}

void emberChildJoinHandler(int8u index, boolean joining)
{
}

// This is called when an EZSP error is reported
void ezspErrorHandler(EzspStatus status)
{
  emberAfCorePrintln("ERROR: ezspErrorHandler 0x%x", status);
  emberAfCoreFlush();

  // Rather than detect whether or not we can recover from the error,
  // we just flag the NCP for reboot.
  ncpNeedsResetAndInit = TRUE;
}

EmberStatus emberAfEzspSetSourceRoute(EmberNodeId id)
{
#ifdef EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  int16u relayList[ZA_MAX_HOPS];
  int8u relayCount;
  if (emberFindSourceRoute(id, &relayCount, relayList)
      && ezspSetSourceRoute(id, relayCount, relayList) != EMBER_SUCCESS) {
    return EMBER_SOURCE_ROUTE_FAILURE;
  }
#endif
  return EMBER_SUCCESS;
}

EmberStatus emAfSend(EmberOutgoingMessageType type,
                     int16u indexOrDestination,
                     EmberApsFrame *apsFrame,
                     int8u messageLength,
                     int8u *message)
{
  switch (type) {
  case EMBER_OUTGOING_DIRECT:
  case EMBER_OUTGOING_VIA_ADDRESS_TABLE:
  case EMBER_OUTGOING_VIA_BINDING:
    {
      EmberStatus status = emberAfEzspSetSourceRoute(indexOrDestination);
      if (status == EMBER_SUCCESS) {
        status = ezspSendUnicast(type,
                                 indexOrDestination,
                                 apsFrame,
                                 0, // message tag - not used
                                 (int8u)messageLength,
                                 message,
                                 &apsFrame->sequence);
      }
      return status;
    }
  case EMBER_OUTGOING_MULTICAST:
    return ezspSendMulticast(apsFrame,
                             ZA_MAX_HOPS, // hops
                             ZA_MAX_HOPS, // nonmember radius
                             0,           // message tag - not used
                             messageLength,
                             message,
                             &apsFrame->sequence);
  case EMBER_OUTGOING_BROADCAST:
    return ezspSendBroadcast(indexOrDestination,
                             apsFrame,
                             ZA_MAX_HOPS, // radius
                             0,           // message tag - not used
                             messageLength,
                             message,
                             &apsFrame->sequence);
  default:
    return EMBER_BAD_ARGUMENT;
  }
}

// Platform dependent interface to get various stack parameters.
void emberAfGetEui64(EmberEUI64 returnEui64)
{
  // We cache the EUI64 of the NCP since it never changes.
  static boolean validEui64 = FALSE;
  static EmberEUI64 myEui64;

  if (validEui64 == FALSE) {
    ezspGetEui64(myEui64);
    validEui64 = TRUE;
  }
  MEMCOPY(returnEui64, myEui64, EUI64_SIZE);
}

EmberStatus emberAfGetNetworkParameters(EmberNodeType* nodeType,
                                        EmberNetworkParameters* parameters)
{
  return ezspGetNetworkParameters(nodeType, parameters);
}

// This will cache all config items to make sure repeated calls do not
// go all the way to the NCP.
int8u emberAfGetNcpConfigItem(EzspConfigId id)
{
  // In case we can't cache config items yet, we need a temp
  // variable to store the retrieved EZSP config ID.
  int16u temp = 0xFFFF;
  int16u *configItemPtr = &temp;
  boolean cacheValid;

  EMBER_TEST_ASSERT(id <= EZSP_CONFIG_ID_MAX);

  cacheValid = (cacheConfigIdValuesAllowed
                && id <= EZSP_CONFIG_ID_MAX);

  if (cacheValid) {
    configItemPtr = &(cachedConfigIdValues[id]);
  }

  if (*configItemPtr == 0xFFFF
      && EZSP_SUCCESS != ezspGetConfigurationValue(id,
                                                   configItemPtr)) {
    // We return a 0 size (for tables) on error to prevent code from using the 
    // invalid value of 0xFFFF.  This is particularly necessary for loops that
    // iterate over all indexes.
    return 0;
  }
  
  return (int8u)(*configItemPtr);
}

EmberStatus emberAfGetChildData(int8u index,
                                EmberNodeId *childId,
                                EmberEUI64 childEui64,
                                EmberNodeType *childType)
{
  return ezspGetChildData(index,
                          childId,
                          childEui64,
                          childType);
}


int8u emberAfGetChildTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_MAX_END_DEVICE_CHILDREN);
}


int8u emberAfGetKeyTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_KEY_TABLE_SIZE);
}

int8u emberAfGetAddressTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_ADDRESS_TABLE_SIZE);
}

int8u emberAfGetBindingTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_BINDING_TABLE_SIZE);
}

int8u emberAfGetNeighborTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_NEIGHBOR_TABLE_SIZE);
}

int8u emberAfGetRouteTableSize(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_ROUTE_TABLE_SIZE);
}

int8u emberAfGetSecurityLevel(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_SECURITY_LEVEL);
}

int8u emberAfGetStackProfile(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_STACK_PROFILE);
}

int8u emberAfGetSleepyMulticastConfig(void)
{
  return emberAfGetNcpConfigItem(EZSP_CONFIG_SEND_MULTICASTS_TO_SLEEPY_ADDRESS);
}


// On the System-on-a-chip this function is provided by the stack.
// Here is a copy for the host based applications.
void emberReverseMemCopy(int8u* dest, const int8u* src, int16u length)
{
  int16u i;
  int16u j = (length - 1);

  for( i = 0; i < length; i++) {
    dest[i] = src[j];
    j--;
  }
}

// ******************************************************************
// Functions called by the Serial Command Line Interface (CLI)
// ******************************************************************

// *****************************
// emAfCliVersionCommand
//
// version <no arguments>
// *****************************
void emAfCliVersionCommand(void)
{
  // Note that NCP == Network Co-Processor

  EmberVersion versionStruct;

  // the EZSP protocol version that the NCP is using
  int8u ncpEzspProtocolVer;

  // the stackType that the NCP is running
  int8u ncpStackType;

  // the EZSP protocol version that the Host is running
  // we are the host so we set this value
  int8u hostEzspProtocolVer = EZSP_PROTOCOL_VERSION;

  // send the Host version number to the NCP. The NCP returns the EZSP
  // version that the NCP is running along with the stackType and stackVersion
  ncpEzspProtocolVer = ezspVersion(hostEzspProtocolVer,
                                   &ncpStackType,
                                   &ncpStackVer);

  // verify that the stack type is what is expected
  if (ncpStackType != EZSP_STACK_TYPE_MESH) {
    emberAfAppPrint("ERROR: stack type 0x%x is not expected!",
                   ncpStackType);
    assert(FALSE);
  }

  // verify that the NCP EZSP Protocol version is what is expected
  if (ncpEzspProtocolVer != EZSP_PROTOCOL_VERSION) {
    emberAfAppPrint("ERROR: NCP EZSP protocol version of 0x%x does not match Host version 0x%x\r\n",
                   ncpEzspProtocolVer,
                   hostEzspProtocolVer);
    assert(FALSE);
  }

  emberAfAppPrint("ezsp ver 0x%x stack type 0x%x ",
                 ncpEzspProtocolVer, ncpStackType, ncpStackVer);

  if (EZSP_SUCCESS != ezspGetVersionStruct(&versionStruct)) {
    // NCP has Old style version number
    emberAfAppPrintln("stack ver [0x%2x]", ncpStackVer);
  } else {
    // NCP has new style version number
    emAfParseAndPrintVersion(versionStruct);
  }
  emberAfAppFlush();
}

void emAfCopyCounterValues(int16u* counters)
{
  ezspReadAndClearCounters(counters);
}

int8u emAfGetPacketBufferFreeCount(void)
{
  int8u freeCount;
  int8u valueLength;
  ezspGetValue(EZSP_VALUE_FREE_BUFFERS,
               &valueLength,
               &freeCount);
  return freeCount;
}

int8u emAfGetPacketBufferTotalCount(void)
{
  int16u value;
  ezspGetConfigurationValue(EZSP_CONFIG_PACKET_BUFFER_COUNT,
                            &value);
  return (int8u)value;
}

// WARNING:  This function executes in ISR context
void halNcpIsAwakeIsr(boolean isAwake)
{
  if (isAwake) {
    emberAfNcpIsAwakeIsrCallback();
  } else {
    // If we got indication that the NCP failed to wake up
    // there is not much that can be done.  We will reset the
    // host (which in turn will reset the NCP) and that will
    // hopefully bring things back in sync.
    assert(0);
  }
}

void ezspNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                             int8u lqi,
                             int8s rssi)
{
  emberAfPushCallbackNetworkIndex();
  emberFormAndJoinNetworkFoundHandler(networkFound, lqi, rssi);
  emberAfPopNetworkIndex();
}

void ezspScanCompleteHandler(int8u channel, EmberStatus status)
{
  emberAfPushCallbackNetworkIndex();
  emberFormAndJoinScanCompleteHandler(channel, status);
  emberAfPopNetworkIndex();
}

void ezspEnergyScanResultHandler(int8u channel, int8s rssi)
{
  emberAfPushCallbackNetworkIndex();
  emberFormAndJoinEnergyScanResultHandler(channel, rssi);
  emberAfPopNetworkIndex();
}

void emAfPrintEzspEndpointFlags(int8u endpoint)
{
  EzspEndpointFlags flags;
  EzspStatus status = ezspGetEndpointFlags(endpoint,
                                           &flags);
  if (status != EZSP_SUCCESS) {
    emberAfCorePrint("Error retrieving EZSP endpoint flags.");
  } else {
    emberAfCorePrint("- EZSP Endpoint flags: 0x%2X", flags);
  }
}
