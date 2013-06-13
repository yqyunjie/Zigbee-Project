// File: ezsp.c
// 
// Description: Host EZSP layer. Provides functions that allow the Host
// application to send every EZSP command to the EM260. The command and response
// parameters are defined in the datasheet.
// 
// Copyright 2006-2010 by Ember Corporation. All rights reserved.           *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"
#include "stack/include/error.h"

#include "hal/hal.h"

#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/ezsp/ezsp-frame-utilities.h"

#ifdef EZSP_UART
  #include "app/ezsp-uart-host/ash-host-priv.h"
  #define EZSP_UART_TRACE(...) ashTraceEzspVerbose(__VA_ARGS__)
#else
  #define EZSP_UART_TRACE(...)
#endif

#if defined(EMBER_TEST)
  #define EMBER_TEST_ASSERT(x) assert(x)
#else
  #define EMBER_TEST_ASSERT(x)
#endif

//----------------------------------------------------------------
// Forward Declarations

static void startCommand(int8u command);
static void sendCommand(void);
static void callbackDispatch(void);
static void callbackPointerInit(void);
static int8u *fetchInt8uPointer(int8u length);

//----------------------------------------------------------------
// Global Variables

#define MAX_SUPPORTED_NETWORKS 4

int8u emSupportedNetworks = MAX_SUPPORTED_NETWORKS;

int8u ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;

static boolean sendingCommand = FALSE;
static int8u ezspSequence = 0;

// Multi-network support: this variable is equivalent to the
// emApplicationNetworkIndex vaiable for SOC. It stores the ezsp network index.
// It gets included in the frame control of every EZSP message to the NCP.
// The public APIs emberGetCurrentNetwork() and emberSetCurrentNetwork() set/get
// this value.
int8u ezspApplicationNetworkIndex = 0;

// Multi-network support: this variable is set when we receive a callback-related
// EZSP message from the NCP. The emberGetCallbackNetwork() API returns this
// value.
int8u ezspCallbackNetworkIndex = 0;

// Some callbacks from EZSP to the application include a pointer parameter. For
// example, messageContents in ezspIncomingMessageHandler(). Copying the
// callback and then giving the application a pointer to this copy means it is
// safe for the application to call EZSP functions inside the callback. To save
// RAM, the application can define EZSP_DISABLE_CALLBACK_COPY. The application
// must then not read from the pointer after calling an EZSP function inside the
// callback.
#ifndef EZSP_DISABLE_CALLBACK_COPY
static int8u ezspCallbackStorage[EZSP_MAX_FRAME_LENGTH];
#endif

boolean ncpHasCallbacks;

//------------------------------------------------------------------------------
// Retrieving the new version info

EzspStatus ezspGetVersionStruct(EmberVersion* returnData)
{
  int8u data[7];  // sizeof(EmberVersion)
  int8u dataLength;
  EzspStatus status = ezspGetValue(EZSP_VALUE_VERSION_INFO,
                                   &dataLength,
                                   data);

  EMBER_TEST_ASSERT(dataLength == 7);

  if (status == EZSP_SUCCESS) {
    returnData->build   = data[0] + (((int16u)data[1]) << 8);
    returnData->major   = data[2];
    returnData->minor   = data[3];
    returnData->patch   = data[4];
    returnData->special = data[5];
    returnData->type    = data[6];
  }

  return status;
}

//------------------------------------------------------------------------------
// Functions for manipulating the endpoints flags on the NCP

EzspStatus ezspSetEndpointFlags(int8u endpoint,
                                EzspEndpointFlags flags)
{
  int8u data[3];
  data[0] = endpoint;
  data[1] = (int8u)flags;
  data[2] = (int8u)(flags >> 8);
  return ezspSetValue(EZSP_VALUE_ENDPOINT_FLAGS,
                      3,
                      data);
}

EzspStatus ezspGetEndpointFlags(int8u endpoint,
                                EzspEndpointFlags* returnFlags)
{
  int8u status;
  int8u value[2];
  int8u valueLength;

  status = ezspGetExtendedValue(EZSP_EXTENDED_VALUE_ENDPOINT_FLAGS,
                                endpoint,
                                &valueLength,
                                value);
  *returnFlags = HIGH_LOW_TO_INT(value[1], value[0]);
  return status;
}

//----------------------------------------------------------------
// Special Handling for AES functions.

// This is a copy of the function available on the SOC.  It would be a waste
// to have this be an actual EZSP call.
void emberAesMmoHashInit(EmberAesMmoHashContext *context)
{
  MEMSET(context, 0, sizeof(EmberAesMmoHashContext));
}

// Here we convert the normal Ember AES hash call to the specialized EZSP call.
// This came about because we cannot pass a block of data that is
// both input and output into EZSP.  The block must be broken up into two
// elements.  We unify the two pieces here to make it invisible to
// the users.

static EmberStatus aesMmoHash(EmberAesMmoHashContext *context,
                              boolean finalize,
                              int32u length,
                              int8u *data)
{
  EmberAesMmoHashContext returnData;
  EmberStatus status;
  if (length > 255) {
    return EMBER_INVALID_CALL;
  }
  // In theory we could use 'context' structure as the 'returnData',
  // however that could be risky if the EZSP function tries to memset() the 
  // 'returnData' prior to storing data in it.
  status = ezspAesMmoHash(context,
                          finalize,
                          (int8u)length,
                          data,
                          &returnData);
  MEMCOPY(context, &returnData, sizeof(EmberAesMmoHashContext));
  return status;
}

EmberStatus emberAesMmoHashUpdate(EmberAesMmoHashContext *context,
                                  int32u length,
                                  int8u *data)
{
  return aesMmoHash(context,
                    FALSE,   // finalize?
                    length,
                    data);
}

EmberStatus emberAesMmoHashFinal(EmberAesMmoHashContext *context,
                                 int32u length,
                                 int8u *data)
{
  return aesMmoHash(context,
                    TRUE,    // finalize?
                    length,
                    data);
}

// This is a convenience routine for hashing short blocks of data,
// less than 255 bytes.
EmberStatus emberAesHashSimple(int8u totalLength,
                               const int8u* data,
                               int8u* result)
{
  EmberStatus status;
  EmberAesMmoHashContext context;
  emberAesMmoHashInit(&context);
  status = emberAesMmoHashFinal(&context,
                                totalLength,
                                (int8u*)data);
  MEMCOPY(result, context.result, 16);
  return status;
}

//------------------------------------------------------------------------------
// SOC function names that are available on the host in a different
// form to save code space.

EmberStatus emberSetMfgSecurityConfig(int32u magicNumber,
                                      const EmberMfgSecurityStruct* settings)
{
  int8u data[4 + 2];  // 4 bytes for magic number, 2 bytes for key settings
  data[0] = (int8u)(magicNumber         & 0xFF);
  data[1] = (int8u)((magicNumber >> 8)  & 0xFF);
  data[2] = (int8u)((magicNumber >> 16) & 0xFF);
  data[3] = (int8u)((magicNumber >> 24) & 0xFF);
  data[4] = (int8u)(settings->keySettings        & 0xFF);
  data[5] = (int8u)((settings->keySettings >> 8) & 0xFF);
  return ezspSetValue(EZSP_VALUE_MFG_SECURITY_CONFIG, 6, data);
}

EmberStatus emberGetMfgSecurityConfig(EmberMfgSecurityStruct* settings)
{
  int8u data[2];
  int8u length = 2;
  EmberStatus status = ezspGetValue(EZSP_VALUE_MFG_SECURITY_CONFIG,
                                    &length,
                                    data);
  settings->keySettings = data[0] + (data[1] << 8);
  return status;
}

EmberStatus emberStartWritingStackTokens(void)
{
  int8u i = 1;
  return ezspSetValue(EZSP_VALUE_STACK_TOKEN_WRITING, 1, &i);
}

EmberStatus emberStopWritingStackTokens(void)
{
  int8u i = 0;
  return ezspSetValue(EZSP_VALUE_STACK_TOKEN_WRITING, 1, &i);
}

boolean emberWritingStackTokensEnabled(void)
{
  int8u value;
  int8u valueLength = 1;
  ezspGetValue(EZSP_VALUE_STACK_TOKEN_WRITING, &valueLength, &value);
  return value;
}

boolean emberStackIsPerformingRejoin(void)
{
  int8u value = 0;
  int8u valueLength;
  ezspGetValue(EZSP_VALUE_STACK_IS_PERFORMING_REJOIN,
               &valueLength,
               &value);
  return value;
}

EmberStatus emberSendRemoveDevice(EmberNodeId destShort,
                                  EmberEUI64 destLong,
                                  EmberEUI64 deviceToRemoveLong)
{
  return ezspRemoveDevice(destShort, destLong, deviceToRemoveLong);
}

EmberStatus emberSendUnicastNetworkKeyUpdate(EmberNodeId targetShort,
                                             EmberEUI64  targetLong,
                                             EmberKeyData* newKey)
{
  return ezspUnicastNwkKeyUpdate(targetShort,
                                 targetLong,
                                 newKey);
}

EmberStatus emberSetExtendedSecurityBitmask(EmberExtendedSecurityBitmask mask)
{
  int8u value[2];
  value[0] = LOW_BYTE(mask);
  value[1] = HIGH_BYTE(mask);
  if (ezspSetValue(EZSP_VALUE_EXTENDED_SECURITY_BITMASK, 2, value)
      == EZSP_SUCCESS) {
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}

EmberStatus emberGetExtendedSecurityBitmask(EmberExtendedSecurityBitmask* mask)
{
  int8u value[2];
  int8u valueLength;
  if (ezspGetValue(EZSP_VALUE_EXTENDED_SECURITY_BITMASK, &valueLength, value)
      == EZSP_SUCCESS) {
    *mask = HIGH_LOW_TO_INT(value[1], value[0]);
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}

EmberStatus emberSetNodeId(EmberNodeId nodeId)
{
  int8u value[2];
  value[0] = LOW_BYTE(nodeId);
  value[1] = HIGH_BYTE(nodeId);
  if (ezspSetValue(EZSP_VALUE_NODE_SHORT_ID, 2, value) == EZSP_SUCCESS) {
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}

void emberSetMaximumIncomingTransferSize(int16u size)
{
  int8u value[2];
  value[0] = LOW_BYTE(size);
  value[1] = HIGH_BYTE(size);

  ezspSetValue(EZSP_VALUE_MAXIMUM_INCOMING_TRANSFER_SIZE, 2, value);
}

void emberSetMaximumOutgoingTransferSize(int16u size)
{
  int8u value[2];
  value[0] = LOW_BYTE(size);
  value[1] = HIGH_BYTE(size);

  ezspSetValue(EZSP_VALUE_MAXIMUM_OUTGOING_TRANSFER_SIZE, 2, value);
}

void emberSetDescriptorCapability(int8u capability)
{
  int8u value[1];
  value[0] = capability;

  ezspSetValue(EZSP_VALUE_DESCRIPTOR_CAPABILITY, 1, value);
}

int8u emberGetLastStackZigDevRequestSequence(void)
{
  int8u value = 0;
  int8u valueLength;
  ezspGetValue(EZSP_VALUE_STACK_DEVICE_REQUEST_SEQUENCE_NUMBER,
               &valueLength,
               &value);
  return value;
}

int8u emberGetCurrentNetwork(void)
{
  return ezspApplicationNetworkIndex;
}

int8u emberGetCallbackNetwork(void)
{
  return ezspCallbackNetworkIndex;
}

EmberStatus emberSetCurrentNetwork(int8u index)
{
  if (index < emSupportedNetworks) {
    ezspApplicationNetworkIndex = index;
    return EMBER_SUCCESS;
  } else {
    return EMBER_INVALID_CALL;
  }
}

EmberStatus emberFindAndRejoinNetworkWithReason(boolean haveCurrentNetworkKey,
                                                int32u channelMask,
                                                EmberRejoinReason reason)
{
  // If there are legacy NCP devices without the rejoin reason support we want
  // to ignore a failure to this ezspSetValue() call.
  ezspSetValue(EZSP_VALUE_NEXT_HOST_REJOIN_REASON,
               1,
               &reason);
  return ezspFindAndRejoinNetwork(haveCurrentNetworkKey,
                                  channelMask);
}

EmberStatus emberFindAndRejoinNetwork(boolean haveCurrentNetworkKey,
                                      int32u channelMask)
{
  return emberFindAndRejoinNetworkWithReason(haveCurrentNetworkKey,
                                             channelMask,
                                             EMBER_REJOIN_DUE_TO_APP_EVENT_1);
}

EmberRejoinReason emberGetLastRejoinReason(void)
{
  EmberRejoinReason reason = EMBER_REJOIN_REASON_NONE;
  int8u length;
  ezspGetValue(EZSP_VALUE_LAST_REJOIN_REASON,
               &length,
               &reason);
  return reason;
}

EmberLeaveReason emberGetLastLeaveReason(EmberNodeId* id)
{
  int8u length = 3;
  int8u data[3];
  ezspGetExtendedValue(EZSP_EXTENDED_VALUE_LAST_LEAVE_REASON,
                       0,  // characteristics
                       &length,
                       data);
  if (id != NULL) {
    *id = (((int16u)data[1])
           + ((int16u)(data[2] << 8)));
  }

  return data[0];
}

//------------------------------------------------------------------------------

#include "command-functions.h"

//----------------------------------------------------------------
// EZSP Utilities

static void startCommand(int8u command)
{
  ezspWritePointer = ezspFrameContents + EZSP_PARAMETERS_INDEX;
  serialSetCommandByte(EZSP_FRAME_ID_INDEX, command);
}

enum {
  RESPONSE_SUCCESS,
  RESPONSE_WAITING,
  RESPONSE_ERROR
};

static int8u responseReceived(void)
{
  EzspStatus status;
  int8u responseFrameControl;

  status = serialResponseReceived();

  if (status == EZSP_SPI_WAITING_FOR_RESPONSE
      || status == EZSP_ASH_NO_RX_DATA) {
    return RESPONSE_WAITING;
  }

  ezspReadPointer = ezspFrameContents + EZSP_PARAMETERS_INDEX;

  if (status == EZSP_SUCCESS) {
    responseFrameControl = serialGetResponseByte(EZSP_FRAME_CONTROL_INDEX);
    if (serialGetResponseByte(EZSP_FRAME_ID_INDEX) == EZSP_INVALID_COMMAND)
      status = serialGetResponseByte(EZSP_PARAMETERS_INDEX);
    if ((responseFrameControl & EZSP_FRAME_CONTROL_DIRECTION_MASK)
        != EZSP_FRAME_CONTROL_RESPONSE)
      status = EZSP_ERROR_WRONG_DIRECTION;
    if ((responseFrameControl & EZSP_FRAME_CONTROL_TRUNCATED_MASK)
        == EZSP_FRAME_CONTROL_TRUNCATED)
      status = EZSP_ERROR_TRUNCATED;
    if ((responseFrameControl & EZSP_FRAME_CONTROL_OVERFLOW_MASK)
        == EZSP_FRAME_CONTROL_OVERFLOW_MASK)
      status = EZSP_ERROR_OVERFLOW;
    if ((responseFrameControl & EZSP_FRAME_CONTROL_PENDING_CB_MASK)
        == EZSP_FRAME_CONTROL_PENDING_CB) {
      ncpHasCallbacks = TRUE;
    } else {
      ncpHasCallbacks = FALSE;
    }

    // Set the callback network
    ezspCallbackNetworkIndex =
        (responseFrameControl & EZSP_FRAME_CONTROL_NETWORK_INDEX_MASK)
         >> EZSP_FRAME_CONTROL_NETWORK_INDEX_OFFSET;
  }
  if (status != EZSP_SUCCESS) {
    EZSP_UART_TRACE("responseReceived(): ezspErrorHandler(): 0x%x", status);
    ezspErrorHandler(status);
    return RESPONSE_ERROR;
  } else {
    return RESPONSE_SUCCESS;
  }
}

static void sendCommand(void)
{
  EzspStatus status;
  int16u length = ezspWritePointer - ezspFrameContents;
  serialSetCommandByte(EZSP_SEQUENCE_INDEX, ezspSequence);
  ezspSequence++;
  serialSetCommandByte(EZSP_FRAME_CONTROL_INDEX,
                       (EZSP_FRAME_CONTROL_COMMAND
                        | ezspSleepMode
                        | ezspApplicationNetworkIndex // we always set the network index in the
                          << EZSP_FRAME_CONTROL_NETWORK_INDEX_OFFSET)); // ezsp frame control.
  if (length > EZSP_MAX_FRAME_LENGTH) {
    EZSP_UART_TRACE("sendCommand(): ezspErrorHandler(): EZSP_ERROR_COMMAND_TOO_LONG");
    ezspErrorHandler(EZSP_ERROR_COMMAND_TOO_LONG);
    return;
  }
  serialSetCommandLength(length);
  // Ensure that a second command is not sent before the response to the first
  // command has been processed.
  assert(!sendingCommand);
  sendingCommand = TRUE;
  status = serialSendCommand();
  if (status == EZSP_SUCCESS) {
    while (responseReceived() == RESPONSE_WAITING) {
      ezspWaitingForResponse();
    }
  } else {
    EZSP_UART_TRACE("sendCommand(): ezspErrorHandler(): 0x%x", status);
    ezspErrorHandler(status);
  }
  sendingCommand = FALSE;
}

static void callbackPointerInit(void)
{
#ifndef EZSP_DISABLE_CALLBACK_COPY
  MEMCOPY(ezspCallbackStorage, ezspFrameContents, EZSP_MAX_FRAME_LENGTH);
  ezspReadPointer = ezspCallbackStorage + EZSP_PARAMETERS_INDEX;
#endif
}

static int8u *fetchInt8uPointer(int8u length)
{
  int8u *result = ezspReadPointer;
  ezspReadPointer += length;
  return result;
}

void ezspTick(void)
{
  int8u count = serialPendingResponseCount() + 1;
  // Ensure that we are not being called from within a command.
  assert(!sendingCommand);
  while (count > 0 && responseReceived() == RESPONSE_SUCCESS) {
    callbackDispatch();
    count--;
  }
  simulatedTimePasses();
}

// ZLL methods

#ifndef XAP2B
EmberStatus zllNetworkOps(EmberZllNetwork* networkInfo,
                          EzspZllNetworkOperation op,
                          int8u radioTxPower)
{
  return ezspZllNetworkOps(networkInfo, op, radioTxPower);
}

/** @brief This will set the device type as a router or end device 
 * (depending on the passed nodeType) and setup a ZLL 
 * commissioning network with the passed parameters.  If panId is 0xFFFF,
 * a random PAN ID will be generated.  If extendedPanId is set to all F's, 
 * then a random extended pan ID will be generated.  If channel is 0xFF,
 * then channel 11 will be used.
 * If all F values are passed for PAN Id or Extended PAN ID then the
 * randomly generated values will be returned in the passed structure.
 * 
 * @param networkInfo A pointer to an ::EmberZllNetwork struct indicating the
 *   network parameters to use when forming the network.  If random values are
 *   requested, the stack's randomly generated values will be returned in the
 *   structure.
 * @param radioTxPower the radio output power at which a node is to operate.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllFormNetwork(EmberZllNetwork* networkInfo,
                                int8s radioTxPower)
{
  return zllNetworkOps(networkInfo, EZSP_ZLL_FORM_NETWORK, radioTxPower);
}

/** @brief This call will cause the device to send a NWK start or join to the
 *  target device and cause the remote AND local device to start operating
 *  on a network together.  If the local device is a factory new device
 *  then it will send a ZLL NWK start to the target requesting that the
 *  target generate new network parameters.  If the device is 
 *  not factory new then the local device will send a NWK join request
 *  using the current network parameters. 
 *
 * @param targetNetworkInfo A pointer to an ::EmberZllNetwork structure that
 *   indicates the info about what device to send the NWK start/join
 *   request to.  This information must have previously been returned
 *   from a ZLL scan.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllJoinTarget(const EmberZllNetwork* targetNetworkInfo)
{
  return zllNetworkOps((EmberZllNetwork *) targetNetworkInfo, EZSP_ZLL_JOIN_TARGET, 0);
}


/** @brief This call will cause the device to setup the security information
 *    used in its network.  It must be called prior to forming, starting, or
 *    joining a network.  
 *    
 * @param networkKey is a pointer to an ::EmberKeyData structure containing the
 *   value for the network key.  If the value is set to all F's, then a random
 *   network key will be generated.
 * @param securityState The security configuration to be set.
 *  
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */
EmberStatus emberZllSetInitialSecurityState(const EmberKeyData *networkKey,
                                            const EmberZllInitialSecurityState *securityState)
{
  return ezspZllSetInitialSecurityState((EmberKeyData *) networkKey, 
                                        (EmberZllInitialSecurityState *) securityState);
}


/**
 * @brief This call will initiate a ZLL network scan on all the specified 
 *   channels.  Results will be returned in ::emberZllNetworkFoundHandler().
 *
 * @param channelMask indicating the range of channels to scan.
 * @param radioPowerForScan the radio output power used for the scan requests.
 * @param nodeType the the node type of the local device.
 *
 * @return An ::EmberStatus value indicating whether the operation 
 *   succeeded, or why it failed.
 */ 
EmberStatus emberZllStartScan(int32u channelMask,
                              int8s radioPowerForScan,
                              EmberNodeType nodeType)
{
  return ezspZllStartScan(channelMask, radioPowerForScan, nodeType);
}

/**
 * @brief This call will change the mode of the radio so that the receiver is
 * on when the device is idle.  Changing the idle mode permits the application
 * to communicate with the target of a touch link before the network is
 * established.  The receiver will remain on until the duration elapses, the
 * duration is set to zero, or ::emberZllJoinTarget is called.
 * 
 * @param durationMs The duration in milliseconds to leave the radio on.
 *
 * @return An ::EmberStatus value indicating whether the operation succeeded or
 * why it failed.
 */
EmberStatus emberZllSetRxOnWhenIdle(int16u durationMs)
{
  return ezspZllSetRxOnWhenIdle(durationMs);
}

EmberStatus emSetLogicalAndRadioChannel(int8u channel)
{
  return ezspSetLogicalAndRadioChannel(channel);
}

int8u emGetLogicalChannel(void)
{
  return ezspGetLogicalChannel();
}

void zllGetTokens(EmberTokTypeStackZllData *data,
                  EmberTokTypeStackZllSecurity *security)
{
  ezspZllGetTokens(data, security);
}

void emberZllGetTokenStackZllData(EmberTokTypeStackZllData *token)
{
  EmberTokTypeStackZllSecurity security;
  zllGetTokens(token, &security);
}

void emberZllGetTokenStackZllSecurity(EmberTokTypeStackZllSecurity *token)
{
  EmberTokTypeStackZllData data;
  zllGetTokens(&data, token);
}

void emberZllSetTokenStackZllData(EmberTokTypeStackZllData *token)
{
  ezspZllSetDataToken(token);
}

void emberZllSetNonZllNetwork(void)
{
  ezspZllSetNonZllNetwork();
}
#endif // XAP2B

boolean emberIsZllNetwork(void)
{
  return ezspIsZllNetwork();
}

