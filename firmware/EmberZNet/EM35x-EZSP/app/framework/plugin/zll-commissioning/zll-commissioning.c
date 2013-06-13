// *******************************************************************
// * zll-commissioning.c
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "app/framework/util/common.h"
#include "zll-commissioning.h"
#include "zll-commissioning-callback.h"

//------------------------------------------------------------------------------
// Globals

// The bits for cluster-specific command (0) and disable default response (4)
// are always set.  The direction bit (3) is only set for server-to-client
// commands (i.e., DeviceInformationResponse).  Some Philips devices still use
// the old frame format and set the frame control to zero.
#define ZLL_FRAME_CONTROL_LEGACY           0x00
#define ZLL_FRAME_CONTROL_CLIENT_TO_SERVER 0x11
#define ZLL_FRAME_CONTROL_SERVER_TO_CLIENT 0x19

#define ZLL_HEADER_FRAME_CONTROL_OFFSET   0 // one byte
#define ZLL_HEADER_SEQUENCE_NUMBER_OFFSET 1 // one byte
#define ZLL_HEADER_COMMAND_ID_OFFSET      2 // one byte
#define ZLL_HEADER_TRANSACTION_ID_OFFSET  3 // four bytes
#define ZLL_HEADER_OVERHEAD               7

#define ZLL_DEVICE_INFORMATION_RECORD_SIZE 16

#define isFactoryNew(state)         ((state) & EMBER_ZLL_STATE_FACTORY_NEW)
#define isRequestingPriority(state) ((state) & EMBER_ZLL_STATE_LINK_PRIORITY_REQUEST)

EmberEventControl emberAfPluginZllCommissioningTouchLinkEventControl;
static EmberZllNetwork network;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static EmberZllDeviceInfoRecord subDevices[EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SUB_DEVICE_TABLE_SIZE];
static int8u subDeviceCount = 0;
static int8u channel;
static int8s rssi;
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

enum {
  INITIAL                     = 0x00,
  SCAN_FOR_TOUCH_LINK         = 0x01,
  SCAN_FOR_DEVICE_INFORMATION = 0x02,
  SCAN_FOR_IDENTIFY           = 0x04,
  SCAN_FOR_RESET              = 0x08,
  TARGET_NETWORK_FOUND        = 0x10,
  ABORTING_TOUCH_LINK         = 0x20,
  SCAN_COMPLETE               = 0x40,
  TOUCH_LINK_TARGET           = 0x80,
};
static int8u flags = INITIAL;
#define touchLinkInProgress()      (flags                            \
                                    & (SCAN_FOR_TOUCH_LINK           \
                                       | SCAN_FOR_DEVICE_INFORMATION \
                                       | SCAN_FOR_IDENTIFY           \
                                       | SCAN_FOR_RESET              \
                                       | TOUCH_LINK_TARGET))
#define scanForTouchLink()         (flags & SCAN_FOR_TOUCH_LINK)
#define scanForDeviceInformation() (flags & SCAN_FOR_DEVICE_INFORMATION)
#define scanForIdentify()          (flags & SCAN_FOR_IDENTIFY)
#define scanForReset()             (flags & SCAN_FOR_RESET)
#define targetNetworkFound()       (flags & TARGET_NETWORK_FOUND)
#define abortingTouchLink()        (flags & ABORTING_TOUCH_LINK)
#define scanComplete()             (flags & SCAN_COMPLETE)
#define touchLinkTarget()          (flags & TOUCH_LINK_TARGET)

int32u emAfZllPrimaryChannelMask = EMBER_AF_PLUGIN_ZLL_COMMISSIONING_PRIMARY_CHANNEL_MASK;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_SECONDARY_CHANNELS
int32u emAfZllSecondaryChannelMask = EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SECONDARY_CHANNEL_MASK;
#endif
int8u emAfZllExtendedPanId[EXTENDED_PAN_ID_SIZE] = EMBER_AF_PLUGIN_ZLL_COMMISSIONING_EXTENDED_PAN_ID;

extern EmberStatus emSetLogicalAndRadioChannel(int8u channel);
extern int8u emGetLogicalChannel(void);

//------------------------------------------------------------------------------
// Forward Declarations

static void touchLinkComplete(void);
static void abortTouchLink(EmberAfZllCommissioningStatus reason);
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static EmberStatus sendDeviceInformationRequest(int8u startIndex);
static EmberStatus sendIdentifyRequest(int16u identifyDuration);
static EmberStatus sendResetToFactoryNewRequest(void);
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static void deviceInformationRequestHandler(const EmberEUI64 source,
                                            int32u transaction,
                                            int8u startIndex);
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static void deviceInformationResponseHandler(const EmberEUI64 source,
                                             int32u transaction,
                                             int8u numberOfSubDevices,
                                             int8u startIndex,
                                             int8u deviceInformationRecordCount,
                                             int8u *deviceInformationRecordList);
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static void identifyRequestHandler(const EmberEUI64 source,
                                   int32u transaction,
                                   int16u identifyDuration);
static void resetToFactoryNewRequestHandler(const EmberEUI64 source,
                                            int32u transaction);
static boolean amFactoryNew(void);
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
static boolean isSameNetwork(const EmberZllNetwork *network);
static EmberStatus setRadioChannel(int8u newChannel);
static int32u getChannelMask(int8u purpose);
static int8s targetCompare(const EmberZllNetwork *t1,
                           int8s r1,
                           const EmberZllNetwork *t2,
                           int8s r2);
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

//------------------------------------------------------------------------------

void emberAfPluginZllCommissioningNcpInitCallback(boolean memoryAllocation)
{
#ifdef EZSP_HOST
  if (!memoryAllocation) {
    emberAfSetEzspConfigValue(EZSP_CONFIG_ZLL_GROUP_ADDRESSES, 
                              EMBER_ZLL_GROUP_ADDRESSES,
                              "group addresses");
    emberAfSetEzspConfigValue(EZSP_CONFIG_ZLL_RSSI_THRESHOLD, 
                              EMBER_ZLL_RSSI_THRESHOLD,
                              "rssi threshold");
  }
#endif // EZSP_HOST
}

boolean emberAfPluginInterpanPreMessageReceivedCallback(const EmberAfInterpanHeader *header,
                                                        int8u msgLen,
                                                        int8u *message)
{
  int32u transaction;
  int8u frameControl, commandId, msgIndex;

  // If the message isn't for the ZLL Commissioning cluster, drop it with an
  // indication that we didn't handle it.  After this, everything else will be
  // considered handled so that the message doesn't end up going through the
  // normal ZCL processing.
  if (header->profileId != EMBER_ZLL_PROFILE_ID
      || header->clusterId != ZCL_ZLL_COMMISSIONING_CLUSTER_ID) {
    return FALSE;
  }

  if (header->messageType != EMBER_AF_INTER_PAN_UNICAST
      || !(header->options & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS)
      || msgLen < ZLL_HEADER_OVERHEAD) {
    return TRUE;
  }

  // Verify that the frame control byte makes sense.  Accept only the legacy
  // format or simple client-to-server or server-to-client messages (i.e., no
  // manufacturer-specific commands, etc.)  For non-legacy messages, check that
  // the frame control is correct for the command.  The check is based on
  // DeviceInformationResponse because it is the only server-to-client command
  // we care about.
  frameControl = message[ZLL_HEADER_FRAME_CONTROL_OFFSET];
  commandId = message[ZLL_HEADER_COMMAND_ID_OFFSET];
  if (frameControl != ZLL_FRAME_CONTROL_LEGACY
      && (frameControl != ZLL_FRAME_CONTROL_CLIENT_TO_SERVER
          || commandId == ZCL_DEVICE_INFORMATION_RESPONSE_COMMAND_ID)
      && (frameControl != ZLL_FRAME_CONTROL_SERVER_TO_CLIENT
          || commandId != ZCL_DEVICE_INFORMATION_RESPONSE_COMMAND_ID)) {
    return TRUE;
  }

  msgIndex = ZLL_HEADER_TRANSACTION_ID_OFFSET;
  transaction = emberAfGetInt32u(message, msgIndex, msgLen);
  msgIndex += 4;

  switch (commandId) {
  case ZCL_DEVICE_INFORMATION_REQUEST_COMMAND_ID:
    if (msgIndex + 1 <= msgLen) {
      int8u startIndex = emberAfGetInt8u(message, msgIndex, msgLen);
      deviceInformationRequestHandler(header->longAddress,
                                      transaction,
                                      startIndex);
    }
    break;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  case ZCL_DEVICE_INFORMATION_RESPONSE_COMMAND_ID:
    if (msgIndex + 3 <= msgLen) {
      int8u numberOfSubDevices, startIndex, deviceInformationRecordCount;
      numberOfSubDevices = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      startIndex = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      deviceInformationRecordCount = emberAfGetInt8u(message, msgIndex, msgLen);
      msgIndex++;
      if ((msgIndex
           + deviceInformationRecordCount * ZLL_DEVICE_INFORMATION_RECORD_SIZE)
          <= msgLen) {
        int8u *deviceInformationRecordList = message + msgIndex;
        deviceInformationResponseHandler(header->longAddress,
                                         transaction,
                                         numberOfSubDevices,
                                         startIndex,
                                         deviceInformationRecordCount,
                                         deviceInformationRecordList);
      }
    }
    break;
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  case ZCL_IDENTIFY_REQUEST_COMMAND_ID:
    if (msgIndex + 2 <= msgLen) {
      int16u identifyDuration = emberAfGetInt16u(message, msgIndex, msgLen);
      identifyRequestHandler(header->longAddress,
                             transaction,
                             identifyDuration);
    }
    break;
  case ZCL_RESET_TO_FACTORY_NEW_REQUEST_COMMAND_ID:
    resetToFactoryNewRequestHandler(header->longAddress, transaction);
    break;
  }

  // All ZLL Commissioning cluster messages are considered handled, even if we
  // ended up dropping them because they were malformed, etc.
  return TRUE;
}

void emberAfPluginZllCommissioningStackStatusCallback(EmberStatus status)
{
  // During touch linking, EMBER_NETWORK_UP means the process is complete.  Any
  // other status, unless we're busy joining or rejoining, means that the touch
  // link failed.
  if (touchLinkInProgress()) {
    if (status == EMBER_NETWORK_UP) {
      touchLinkComplete();
    } else if (emberNetworkState() != EMBER_JOINING_NETWORK
               && !emberStackIsPerformingRejoin()) {
      emberAfAppPrintln("%p%p%p",
                        "Error: ",
                        "Touch linking failed: ",
                        "joining failed");
      abortTouchLink(EMBER_AF_ZLL_JOINING_FAILED);
    }
  } else {
    emAfZllStackStatus(status);
  }
}

static void touchLinkComplete(void)
{
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;
  flags = INITIAL;
  emberAfGetNetworkParameters(&nodeType, &parameters);
  network.zigbeeNetwork.channel = parameters.radioChannel;
  network.zigbeeNetwork.panId = parameters.panId;
  MEMCOPY(network.zigbeeNetwork.extendedPanId,
          parameters.extendedPanId,
          EXTENDED_PAN_ID_SIZE);
  network.zigbeeNetwork.nwkUpdateId = parameters.nwkUpdateId;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  emberAfPluginZllCommissioningTouchLinkCompleteCallback(&network,
                                                         subDeviceCount,
                                                         (subDeviceCount == 0
                                                          ? NULL
                                                          : subDevices));
#else
  emberAfPluginZllCommissioningTouchLinkCompleteCallback(&network, 0, NULL);
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

EmberStatus emberAfZllSetInitialSecurityState(void)
{
  EmberKeyData networkKey;
  EmberZllInitialSecurityState securityState = {
    0, // bitmask - unused
    EMBER_ZLL_KEY_INDEX_CERTIFICATION,
    EMBER_ZLL_CERTIFICATION_ENCRYPTION_KEY,
    EMBER_ZLL_CERTIFICATION_PRECONFIGURED_LINK_KEY,
  };
  EmberStatus status;

  // We can only initialize security information while not on a network.
  if (emberNetworkState() != EMBER_NO_NETWORK) {
    return EMBER_SUCCESS;
  }

  status = emberAfGenerateRandomKey(&networkKey);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Generating random key",
                      status);
    return status;
  }

  emberAfPluginZllCommissioningInitialSecurityStateCallback(&securityState);
  status = emberZllSetInitialSecurityState(&networkKey, &securityState);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Initializing security",
                      status);
  }
  return status;
}

EmberStatus emAfZllFormNetwork(int8u channel, int8s power, EmberPanId panId)
{
  EmberZllNetwork network;
  MEMSET(&network, 0, sizeof(EmberZllNetwork));
  network.zigbeeNetwork.channel = channel;
  network.zigbeeNetwork.panId = panId;
  emberAfGetFormAndJoinExtendedPanIdCallback(network.zigbeeNetwork.extendedPanId);
  network.state = EMBER_AF_PLUGIN_ZLL_COMMISSIONING_ADDITIONAL_STATE;
  network.nodeType = emAfCurrentNetwork->nodeType;
  emberAfZllSetInitialSecurityState();
  return emberZllFormNetwork(&network, power);
}

static EmberStatus startScan(int8u purpose)
{
  EmberStatus status = EMBER_INVALID_CALL;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    emberAfAppPrintln("%pTouch linking in progress", "Error: ");
  } else {
    emberAfZllSetInitialSecurityState();
    channel = emGetLogicalChannel();
    status = emberZllStartScan(getChannelMask(purpose),
                               EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_POWER_LEVEL,
                               emAfCurrentNetwork->nodeType);
    if (status == EMBER_SUCCESS) {
      flags = purpose;
    } else {
      emberAfAppPrintln("%p%p%p0x%x",
                        "Error: ",
                        "Touch linking failed: ",
                        "could not start scan: ",
                        status);
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  return status;
}

EmberStatus emberAfZllInitiateTouchLink(void)
{
  return startScan(SCAN_FOR_TOUCH_LINK);
}

EmberStatus emberAfZllDeviceInformationRequest(void)
{
  return startScan(SCAN_FOR_DEVICE_INFORMATION);
}

EmberStatus emberAfZllIdentifyRequest(void)
{
  return startScan(SCAN_FOR_IDENTIFY);
}

EmberStatus emberAfZllResetToFactoryNewRequest(void)
{
  return startScan(SCAN_FOR_RESET);
}

void emberAfZllAbortTouchLink(void)
{
  if (touchLinkInProgress()) {
    // If the scanning portion of touch linking is already finished, we can
    // abort right away.  If not, we need to stop the scan and wait for the
    // stack to inform us when the scan is done.
    emberAfAppPrintln("%p%p%p",
                      "Error: ",
                      "Touch linking failed: ",
                      "aborted by application");
    if (scanComplete()) {
      abortTouchLink(EMBER_AF_ZLL_ABORTED_BY_APPLICATION);
    } else {
      flags |= ABORTING_TOUCH_LINK;
      emberStopScan();
    }
  }
}

static void abortTouchLink(EmberAfZllCommissioningStatus reason)
{
  flags = INITIAL;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (emberEventControlGetActive(emberAfPluginZllCommissioningTouchLinkEventControl)) {
    emberEventControlSetInactive(emberAfPluginZllCommissioningTouchLinkEventControl);
    if (network.numberSubDevices != 1) {
      emberZllSetRxOnWhenIdle(0); // restore original idle mode
    }
    sendIdentifyRequest(0x0000); // exit identify mode
  }
  {
    EmberStatus status = emberSetRadioChannel(channel);
    if (status != EMBER_SUCCESS) {
      emberAfAppPrintln("%p%p0x%x",
                        "Error: ",
                        "could not restore channel: ",
                        status);
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  emberAfPluginZllCommissioningTouchLinkFailedCallback(reason);
}

boolean emberAfZllTouchLinkInProgress(void)
{
  return touchLinkInProgress();
}

void emberAfZllResetToFactoryNew(void)
{
  emberLeaveNetwork();
  emberAfLoadAttributesFromDefaults(EMBER_BROADCAST_ENDPOINT);
  emberAfLoadAttributesFromTokens(EMBER_BROADCAST_ENDPOINT);
  emberAfGroupsClusterClearGroupTableCallback(EMBER_BROADCAST_ENDPOINT);
  emberAfScenesClusterClearSceneTableCallback(EMBER_BROADCAST_ENDPOINT);
  emberAfPluginZllCommissioningResetToFactoryNewCallback();
}

void ezspZllNetworkFoundHandler(EmberZllNetwork *networkInfo,
                                boolean isDeviceInfoNull,
                                EmberZllDeviceInfoRecord *deviceInfo,
                                int8u lastHopLqi,
                                int8s lastHopRssi)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {

    if (networkInfo->nodeType < EMBER_ROUTER
        || EMBER_SLEEPY_END_DEVICE < networkInfo->nodeType) {
      // In any situation, the target must be a router or end device.  ZLL does
      // not use coordinators and any other types must have wandered into our
      // network in error.
      return;
    } else if (scanForTouchLink()) {
      // When scanning for the purposes of touch linking (as opposed to, for
      // example, resetting to factory new), additional restrictions apply, as
      // described below.
      if (!emberIsZllNetwork()
          && (isFactoryNew(networkInfo->state)
              || !isSameNetwork(networkInfo))) {
        // If the initiator device is on a non-ZLL network, the target cannot
        // be factory new or on a different network.  Those devices can only
        // join the network using classical ZigBee commissioning.
        return;
      } else if (networkInfo->nodeType == EMBER_END_DEVICE
                 || networkInfo->nodeType == EMBER_SLEEPY_END_DEVICE) {
        // When the initiator is factory new, the target cannot be an end
        // device.  If the target is not factory new and is also trying to
        // touch link, the stack will cancel our touch link and wait for a join
        // request.  When the initiator is not factory new, the target must
        // either be factory new or must be on the same network.
        if (amFactoryNew()
            || (!isFactoryNew(networkInfo->state)
                && !isSameNetwork(networkInfo))) {
          return;
        }
      }
    }

    if (!targetNetworkFound()
        || 0 < targetCompare(networkInfo, lastHopRssi, &network, rssi)) {
      MEMCOPY(&network, networkInfo, sizeof(EmberZllNetwork));
      subDeviceCount = 0;
      if (!isDeviceInfoNull) {
        MEMCOPY(&subDevices[0], deviceInfo, sizeof(EmberZllDeviceInfoRecord));
        subDeviceCount++;
      }
      rssi = lastHopRssi;
      flags |= TARGET_NETWORK_FOUND;
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

#ifndef EZSP_HOST
void emberZllNetworkFoundHandler(const EmberZllNetwork *networkInfo,
                                 const EmberZllDeviceInfoRecord *deviceInfo)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    int8s lastHopRssi;

    if (networkInfo->nodeType < EMBER_ROUTER
        || EMBER_SLEEPY_END_DEVICE < networkInfo->nodeType) {
      // In any situation, the target must be a router or end device.  ZLL does
      // not use coordinators and any other types must have wandered into our
      // network in error.
      return;
    } else if (scanForTouchLink()) {
      // When scanning for the purposes of touch linking (as opposed to, for
      // example, resetting to factory new), additional restrictions apply, as
      // described below.
      if (!emberIsZllNetwork()
          && (isFactoryNew(networkInfo->state)
              || !isSameNetwork(networkInfo))) {
        // If the initiator device is on a non-ZLL network, the target cannot
        // be factory new or on a different network.  Those devices can only
        // join the network using classical ZigBee commissioning.
        return;
      } else if (networkInfo->nodeType == EMBER_END_DEVICE
                 || networkInfo->nodeType == EMBER_SLEEPY_END_DEVICE) {
        // When the initiator is factory new, the target cannot be an end
        // device.  If the target is not factory new and is also trying to
        // touch link, the stack will cancel our touch link and wait for a join
        // request.  When the initiator is not factory new, the target must
        // either be factory new or must be on the same network.
        if (amFactoryNew()
            || (!isFactoryNew(networkInfo->state)
                && !isSameNetwork(networkInfo))) {
          return;
        }
      }
    }

    emberGetLastHopRssi(&lastHopRssi);
    if (!targetNetworkFound()
        || 0 < targetCompare(networkInfo, lastHopRssi, &network, rssi)) {
      MEMCOPY(&network, networkInfo, sizeof(EmberZllNetwork));
      subDeviceCount = 0;
      if (deviceInfo != NULL) {
        MEMCOPY(&subDevices[0], deviceInfo, sizeof(EmberZllDeviceInfoRecord));
        subDeviceCount++;
      }
      rssi = lastHopRssi;
      flags |= TARGET_NETWORK_FOUND;
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}
#endif // EZSP_HOST

void ezspZllScanCompleteHandler(EmberStatus status)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    flags |= SCAN_COMPLETE;
    if (status != EMBER_SUCCESS) {
      emberAfAppPrintln("%p%p0x%x",
                        "Error: ",
                        "Touch linking failed: ",
                        status);
      abortTouchLink(EMBER_AF_ZLL_PREEMPTED_BY_STACK);
    } else if (abortingTouchLink()) {
      abortTouchLink(EMBER_AF_ZLL_ABORTED_BY_APPLICATION);
    } else if (targetNetworkFound()) {
      EmberStatus status = setRadioChannel(network.zigbeeNetwork.channel);
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("%p%p%p0x%x",
                          "Error: ",
                          "Touch linking failed: ",
                          "could not change channel: ",
                          status);
        abortTouchLink(EMBER_AF_ZLL_CHANNEL_CHANGE_FAILED);
        return;
      }

      // When scanning for the purposes of touch linking or requesting device
      // information and the target has more than one sub device, turn the
      // receiver on (so we can actually receive the response) and send out the
      // first request.  If the target only has one sub device, its data will
      // have already been received in the ScanRequest.
      if ((scanForTouchLink() || scanForDeviceInformation())
          && network.numberSubDevices != 1) {
        emberZllSetRxOnWhenIdle(EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_MILLISECONDS_DELAY);
        status = sendDeviceInformationRequest(0x00); // start index
        if (status != EMBER_SUCCESS) {
          emberAfAppPrintln("%p%p%p0x%x",
                            "Error: ",
                            "Touch linking failed: ",
                            "could not send device information request: ",
                            status);
          if (scanForDeviceInformation()) {
            abortTouchLink(EMBER_AF_ZLL_SENDING_DEVICE_INFORMATION_REQUEST_FAILED);
            return;
          }
        }
      }

      status = sendIdentifyRequest(0xFFFF); // identify for a default time
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("%p%p%p0x%x",
                          "Error: ",
                          "Touch linking failed: ",
                          "could not send identify request: ",
                          status);
        if (scanForIdentify()) {
          abortTouchLink(EMBER_AF_ZLL_SENDING_IDENTIFY_REQUEST_FAILED);
          return;
        }
      }
      emberEventControlSetDelayMS(emberAfPluginZllCommissioningTouchLinkEventControl,
                                  EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_MILLISECONDS_DELAY);
    } else {
      emberAfAppPrintln("%p%p%p",
                        "Error: ",
                        "Touch linking failed: ",
                        "no networks were found");
      abortTouchLink(EMBER_AF_ZLL_NO_NETWORKS_FOUND);
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

void emberZllScanCompleteHandler(EmberStatus status)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    flags |= SCAN_COMPLETE;
    if (status != EMBER_SUCCESS) {
      emberAfAppPrintln("%p%p0x%x",
                        "Error: ",
                        "Touch linking failed: ",
                        status);
      abortTouchLink(EMBER_AF_ZLL_PREEMPTED_BY_STACK);
    } else if (abortingTouchLink()) {
      abortTouchLink(EMBER_AF_ZLL_ABORTED_BY_APPLICATION);
    } else if (targetNetworkFound()) {
      EmberStatus status = setRadioChannel(network.zigbeeNetwork.channel);
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("%p%p%p0x%x",
                          "Error: ",
                          "Touch linking failed: ",
                          "could not change channel: ",
                          status);
        abortTouchLink(EMBER_AF_ZLL_CHANNEL_CHANGE_FAILED);
        return;
      }

      // When scanning for the purposes of touch linking or requesting device
      // information and the target has more than one sub device, turn the
      // receiver on (so we can actually receive the response) and send out the
      // first request.  If the target only has one sub device, its data will
      // have already been received in the ScanRequest.
      if ((scanForTouchLink() || scanForDeviceInformation())
          && network.numberSubDevices != 1) {
        emberZllSetRxOnWhenIdle(EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_MILLISECONDS_DELAY);
        status = sendDeviceInformationRequest(0x00); // start index
        if (status != EMBER_SUCCESS) {
          emberAfAppPrintln("%p%p%p0x%x",
                            "Error: ",
                            "Touch linking failed: ",
                            "could not send device information request: ",
                            status);
          if (scanForDeviceInformation()) {
            abortTouchLink(EMBER_AF_ZLL_SENDING_DEVICE_INFORMATION_REQUEST_FAILED);
            return;
          }
        }
      }

      status = sendIdentifyRequest(0xFFFF); // identify for a default time
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("%p%p%p0x%x",
                          "Error: ",
                          "Touch linking failed: ",
                          "could not send identify request: ",
                          status);
        if (scanForIdentify()) {
          abortTouchLink(EMBER_AF_ZLL_SENDING_IDENTIFY_REQUEST_FAILED);
          return;
        }
      }
      emberEventControlSetDelayMS(emberAfPluginZllCommissioningTouchLinkEventControl,
                                  EMBER_AF_PLUGIN_ZLL_COMMISSIONING_TOUCH_LINK_MILLISECONDS_DELAY);
    } else {
      emberAfAppPrintln("%p%p%p",
                        "Error: ",
                        "Touch linking failed: ",
                        "no networks were found");
      abortTouchLink(EMBER_AF_ZLL_NO_NETWORKS_FOUND);
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

void ezspZllAddressAssignmentHandler(EmberZllAddressAssignment *addressInfo,
                                     int8u lastHopLqi,
                                     int8s lastHopRssi)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    network.nodeId = addressInfo->nodeId;
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

void emberZllAddressAssignmentHandler(const EmberZllAddressAssignment *addressInfo)
{
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    network.nodeId = addressInfo->nodeId;
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

void ezspZllTouchLinkTargetHandler(EmberZllNetwork *networkInfo)
{
  MEMCOPY(&network, networkInfo, sizeof(EmberZllNetwork));
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  subDeviceCount = 0;
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  flags = TOUCH_LINK_TARGET;
}

void emberZllTouchLinkTargetHandler(const EmberZllNetwork *networkInfo)
{
  MEMCOPY(&network, networkInfo, sizeof(EmberZllNetwork));
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  subDeviceCount = 0;
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  flags = TOUCH_LINK_TARGET;
}

void emberAfPluginZllCommissioningTouchLinkEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginZllCommissioningTouchLinkEventControl);
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
  if (touchLinkInProgress()) {
    EmberStatus status;

    sendIdentifyRequest(0x0000); // exit identify mode

    if (scanForTouchLink()) {
      // If we are not factory new, we want to bring the target into our
      // existing network, so we set the channel to our original channel.
      // Otherwise, we'll use whatever channel the target is on presently.
      if (!amFactoryNew()) {
        network.zigbeeNetwork.channel = channel;
      }
      emberAfZllSetInitialSecurityState();
      status = emberZllJoinTarget(&network);
      if (status != EMBER_SUCCESS) {
        emberAfAppPrintln("%p%p%p0x%x",
                          "Error: ",
                          "Touch linking failed: ",
                          "could not send start/join: ",
                          status);
        abortTouchLink(EMBER_AF_ZLL_SENDING_START_JOIN_FAILED);
        return;
      }
    } else {
      if (scanForReset()) {
        status = sendResetToFactoryNewRequest();
        if (status != EMBER_SUCCESS) {
          emberAfAppPrintln("%p%p%p0x%x",
                            "Error: ",
                            "Touch linking failed: ",
                            "could not send reset: ",
                            status);
          abortTouchLink(EMBER_AF_ZLL_SENDING_RESET_TO_FACTORY_NEW_REQUEST_FAILED);
          return;
        }
      }
      emberSetRadioChannel(channel);
      touchLinkComplete();
    }
  }
#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
}

#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

static EmberStatus sendDeviceInformationRequest(int8u startIndex)
{
  EmberStatus status;
  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                             | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_DEVICE_INFORMATION_REQUEST_COMMAND_ID,
                            "wu",
                            network.securityAlgorithm.transactionId,
                            startIndex);
  status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                      network.eui64,
                                      EMBER_NULL_NODE_ID,    // node id - ignored
                                      0x0000,                // group id - ignored
                                      EMBER_ZLL_PROFILE_ID);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Device information request",
                      status);
  }
  return status;
}

static EmberStatus sendIdentifyRequest(int16u identifyDuration)
{
  EmberStatus status;
  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                             | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_IDENTIFY_REQUEST_COMMAND_ID,
                            "wv",
                            network.securityAlgorithm.transactionId,
                            identifyDuration);
  status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                      network.eui64,
                                      EMBER_NULL_NODE_ID,    // node id - ignored
                                      0x0000,                // group id - ignored
                                      EMBER_ZLL_PROFILE_ID);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Identify request",
                      status);
  }
  return status;
}

static EmberStatus sendResetToFactoryNewRequest(void)
{
  EmberStatus status;
  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                             | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_RESET_TO_FACTORY_NEW_REQUEST_COMMAND_ID,
                            "w",
                            network.securityAlgorithm.transactionId);
  status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                      network.eui64,
                                      EMBER_NULL_NODE_ID,    // node id - ignored
                                      0x0000,                // group id - ignored
                                      EMBER_ZLL_PROFILE_ID);
  if (status != EMBER_SUCCESS) {
    emberAfAppPrintln("%p%p failed 0x%x",
                      "Error: ",
                      "Reset to factory new request",
                      status);
  }
  return status;
}

#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

static void deviceInformationRequestHandler(const EmberEUI64 source,
                                            int32u transaction,
                                            int8u startIndex)
{
  EmberEUI64 eui64;
  EmberStatus status;
  int8u total = emberAfEndpointCount();
  int8u i, *count;

  emberAfZllCommissioningClusterPrintln("RX: DeviceInformationRequest 0x%4x, 0x%x",
                                        transaction,
                                        startIndex);

  emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                             | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                             | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
                            ZCL_ZLL_COMMISSIONING_CLUSTER_ID,
                            ZCL_DEVICE_INFORMATION_RESPONSE_COMMAND_ID,
                            "wuu",
                            transaction,
                            total,
                            startIndex);

  emberAfGetEui64(eui64);
  count = &appResponseData[appResponseLength];
  emberAfPutInt8uInResp(0); // temporary count
  for (i = startIndex; i < total; i++) {
    int8u endpoint = emberAfEndpointFromIndex(i);
    emberAfPutBlockInResp(eui64, EUI64_SIZE);
    emberAfPutInt8uInResp(endpoint);
    emberAfPutInt16uInResp(EMBER_ZLL_PROFILE_ID);
    emberAfPutInt16uInResp(emberAfDeviceIdFromIndex(i));
    emberAfPutInt8uInResp(emberAfDeviceVersionFromIndex(i));
    emberAfPutInt8uInResp(emberAfPluginZllCommissioningGroupIdentifierCountCallback(endpoint));
    emberAfPutInt8uInResp(i); // sort order
    (*count)++;
  }

  status = emberAfSendCommandInterPan(0xFFFF,                // destination pan id
                                      source,
                                      EMBER_NULL_NODE_ID,    // node id - ignored
                                      0x0000,                // group id - ignored
                                      EMBER_ZLL_PROFILE_ID);
  if (status != EMBER_SUCCESS) {
    emberAfZllCommissioningClusterPrintln("%p%p failed 0x%x",
                                          "Error: ",
                                          "Device information response",
                                          status);
  }
}

#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

static void deviceInformationResponseHandler(const EmberEUI64 source,
                                             int32u transaction,
                                             int8u numberOfSubDevices,
                                             int8u startIndex,
                                             int8u deviceInformationRecordCount,
                                             int8u *deviceInformationRecordList)
{
  int16u deviceInformationRecordListLen = (deviceInformationRecordCount
                                           * ZLL_DEVICE_INFORMATION_RECORD_SIZE);
  int16u deviceInformationRecordListIndex = 0;
  int8u i;
  boolean validResponse = (emberEventControlGetActive(emberAfPluginZllCommissioningTouchLinkEventControl)
                           && (network.securityAlgorithm.transactionId == transaction)
                           && MEMCOMPARE(network.eui64, source, EUI64_SIZE) == 0);

  emberAfZllCommissioningClusterFlush();
  emberAfZllCommissioningClusterPrint("RX: DeviceInformationResponse 0x%4x, 0x%x, 0x%x, 0x%x,",
                                      transaction,
                                      numberOfSubDevices,
                                      startIndex,
                                      deviceInformationRecordCount);
  emberAfZllCommissioningClusterFlush();
  for (i = 0; i < deviceInformationRecordCount; i++) {
    int8u *ieeeAddress;
    int8u endpointId;
    int16u profileId;
    int16u deviceId;
    int8u version;
    int8u groupIdCount;
    int8u sort;
    ieeeAddress = &deviceInformationRecordList[deviceInformationRecordListIndex];
    deviceInformationRecordListIndex += EUI64_SIZE;
    endpointId = emberAfGetInt8u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex++;
    profileId = emberAfGetInt16u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex += 2;
    deviceId = emberAfGetInt16u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex += 2;
    version = emberAfGetInt8u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex++;
    groupIdCount = emberAfGetInt8u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex++;
    sort = emberAfGetInt8u(deviceInformationRecordList, deviceInformationRecordListIndex, deviceInformationRecordListLen);
    deviceInformationRecordListIndex++;
    emberAfZllCommissioningClusterPrint(" [");
    emberAfZllCommissioningClusterDebugExec(emberAfPrintBigEndianEui64(ieeeAddress));
    emberAfZllCommissioningClusterPrint(" 0x%x 0x%2x 0x%2x 0x%x 0x%x 0x%x",
                                        endpointId,
                                        profileId,
                                        deviceId,
                                        version,
                                        groupIdCount,
                                        sort);
    emberAfZllCommissioningClusterFlush();

    if (validResponse
        && (subDeviceCount
            < EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SUB_DEVICE_TABLE_SIZE)) {
      MEMCOPY(subDevices[subDeviceCount].ieeeAddress, ieeeAddress, EUI64_SIZE);
      subDevices[subDeviceCount].endpointId = endpointId;
      subDevices[subDeviceCount].profileId = profileId;
      subDevices[subDeviceCount].deviceId = deviceId;
      subDevices[subDeviceCount].version = version;
      subDevices[subDeviceCount].groupIdCount = groupIdCount;
      subDeviceCount++;
    } else {
      emberAfZllCommissioningClusterPrint(" (ignored)");
    }
    emberAfZllCommissioningClusterPrint("]");
    emberAfZllCommissioningClusterFlush();
  }
  emberAfZllCommissioningClusterPrintln("");

  if (validResponse
      && (subDeviceCount
          < EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SUB_DEVICE_TABLE_SIZE)
      && subDeviceCount < numberOfSubDevices) {
    sendDeviceInformationRequest(startIndex + deviceInformationRecordCount);
  }
}

#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

static void identifyRequestHandler(const EmberEUI64 source,
                                   int32u transaction,
                                   int16u identifyDuration)
{
  emberAfZllCommissioningClusterPrintln("RX: IdentifyRequest 0x%4x, 0x%2x",
                                        transaction,
                                        identifyDuration);
  emberAfPluginZllCommissioningIdentifyCallback(identifyDuration);
}

static void resetToFactoryNewRequestHandler(const EmberEUI64 source,
                                            int32u transaction)
{
  emberAfZllCommissioningClusterPrintln("RX: ResetToFactoryNewRequest 0x%4x",
                                        transaction);
  if (!amFactoryNew()) {
    emberAfZllResetToFactoryNew();
  }
}

static boolean amFactoryNew(void)
{
  EmberTokTypeStackZllData token;
  emberZllGetTokenStackZllData(&token);
  return isFactoryNew(token.bitmask);
}

#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR

static boolean isSameNetwork(const EmberZllNetwork *network)
{
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;
  EmberStatus status = emberAfGetNetworkParameters(&nodeType, &parameters);
  return (status == EMBER_SUCCESS
          && (MEMCOMPARE(parameters.extendedPanId,
                         network->zigbeeNetwork.extendedPanId,
                         EXTENDED_PAN_ID_SIZE) == 0)
          && parameters.panId == network->zigbeeNetwork.panId
          && parameters.nwkUpdateId == network->zigbeeNetwork.nwkUpdateId);
}

// In the new multi-network stack, setting the radio channel is no longer
// effective since the MAC will re-tune the radio at the next outgoing packet
// based on what is stored in the NetworkInfo struct.
static EmberStatus setRadioChannel(int8u newChannel)
{
  return emSetLogicalAndRadioChannel(newChannel);
}

static int32u getChannelMask(int8u purpose)
{
  int32u channelMask = emAfZllPrimaryChannelMask;
#ifdef EMBER_AF_PLUGIN_ZLL_COMMISSIONING_SCAN_SECONDARY_CHANNELS
  if (emAfZllSecondaryChannelMask != 0
      && (purpose == SCAN_FOR_RESET || !emberIsZllNetwork())) {
    channelMask |= emAfZllSecondaryChannelMask;
  }
#endif
  return channelMask;
}

// Returns an integer greater than, equal to, or less than zero, according to
// whether target t1 is better than, equal to, or worse than target t2 in terms
// of requested priority with corrected signal strength serving as tiebreaker.
static int8s targetCompare(const EmberZllNetwork *t1,
                           int8s r1,
                           const EmberZllNetwork *t2,
                           int8s r2)
{
  // When considering two targets, if only one has requested priority, that one
  // is chosen.
  if (isRequestingPriority(t1->state)
      && !isRequestingPriority(t2->state)) {
    return 1;
  } else if (!isRequestingPriority(t1->state)
               && isRequestingPriority(t2->state)) {
    return -1;
  }

  // If the priority of both targets is the same, the correct signal strength
  // is considered.  The strongest corrected signal wins.
  if (r1 + t1->rssiCorrection < r2 + t2->rssiCorrection) {
    return -1;
  } else if (r2 + t2->rssiCorrection < r1 + t1->rssiCorrection) {
    return 1;
  }

  // If we got here, both targets are considered equal.
  return 0;
}

#endif //EMBER_AF_PLUGIN_ZLL_COMMISSIONING_LINK_INITIATOR
