// *****************************************************************************
// * fragmentation.c
// *
// * Splits long messages into smaller fragments for transmission and
// * reassembles received fragments into full messages.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#ifdef EZSP_HOST
#include "app/framework/util/af-main.h"
#include "app/util/source-route-host.h"
#endif //EZSP_HOST
#include "fragmentation.h"

// For CLI
#include "app/util/serial/command-interpreter2.h"

EmberEventControl emAfFragmentationEvents[10];

#ifdef EZSP_HOST
static int16u emberApsAckTimeoutMs    = 0;
static int8u  emberFragmentWindowSize =
    EMBER_AF_PLUGIN_FRAGMENTATION_RX_WINDOW_SIZE;
#else //EZSP_HOST
extern int8u  emberFragmentWindowSize;
#endif //EZSP_HOST

//------------------------------------------------------------------------------
// Sending

static EmberStatus sendNextFragments(txFragmentedPacket* txPacket);
static void abortTransmission(txFragmentedPacket *txPacket, EmberStatus status);
static txFragmentedPacket* getFreeTxPacketEntry(void);
static txFragmentedPacket* txPacketLookUp(EmberApsFrame *apsFrame);

static txFragmentedPacket txPackets[EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS];

EmberStatus emAfFragmentationSendUnicast(EmberOutgoingMessageType type,
                                         int16u indexOrDestination,
                                         EmberApsFrame *apsFrame,
                                         int8u *buffer,
                                         int16u bufLen)
{
  EmberStatus status;
  int16u fragments;
  txFragmentedPacket* txPacket;

  if (emberFragmentWindowSize == 0) {
    return EMBER_INVALID_CALL;
  }

  txPacket = getFreeTxPacketEntry();
  if (txPacket == NULL) {
    return EMBER_MAX_MESSAGE_LIMIT_REACHED;
  }

  txPacket->indexOrDestination = indexOrDestination;
  MEMCOPY(&txPacket->apsFrame, apsFrame, sizeof(EmberApsFrame));
  txPacket->apsFrame.options |=
      (EMBER_APS_OPTION_FRAGMENT | EMBER_APS_OPTION_RETRY);
#ifdef EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  txPacket->sourceRoute =
      emberFindSourceRoute(indexOrDestination,
                           &txPacket->relayCount,
                           txPacket->relayList);
#endif //EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  MEMCOPY(txPacket->buffer, buffer, bufLen);
  txPacket->bufLen = bufLen;
  txPacket->fragmentLen = emberAfMaximumApsPayloadLength(type,
                                                         indexOrDestination,
                                                         &txPacket->apsFrame);
  fragments = ((bufLen + txPacket->fragmentLen - 1) / txPacket->fragmentLen);
  if (fragments > MAX_INT8U_VALUE
      || bufLen > EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE) {
    return EMBER_MESSAGE_TOO_LONG;
  }
  txPacket->messageType = type;
  txPacket->fragmentCount = (int8u)fragments;
  txPacket->fragmentBase = 0;
  txPacket->fragmentsInTransit = 0;

  status = sendNextFragments(txPacket);
  if (status != EMBER_SUCCESS) {
    txPacket->messageType = 0xFF;
  }
  return status;
}

boolean emAfFragmentationMessageSent(EmberApsFrame *apsFrame,
                                     EmberStatus status)
{
  if (apsFrame->options & EMBER_APS_OPTION_FRAGMENT) {
    // If the outgoing APS frame is fragmented, we should always have a
    // a corresponding record in the txFragmentedPacket array.
    txFragmentedPacket *txPacket = txPacketLookUp(apsFrame);
    if (txPacket == NULL)
      return TRUE;

    if (status == EMBER_SUCCESS) {
      txPacket->fragmentsInTransit--;
      if (txPacket->fragmentsInTransit == 0) {
        txPacket->fragmentBase += emberFragmentWindowSize;
        abortTransmission(txPacket, sendNextFragments(txPacket));
      }
    } else {
      abortTransmission(txPacket, status);
    }
    return TRUE;
  } else {
    return FALSE;
  }
}

static EmberStatus sendNextFragments(txFragmentedPacket* txPacket)
{
  int8u i;
  int16u offset;

  offset = txPacket->fragmentBase * txPacket->fragmentLen;

  // Send fragments until the window is full.
  for (i = txPacket->fragmentBase;
       i < txPacket->fragmentBase + emberFragmentWindowSize
       && i < txPacket->fragmentCount;
       i++) {
    EmberStatus status;

    // For a message requiring n fragments, the length of each of the first
    // n - 1 fragments is the maximum fragment size.  The length of the last
    // fragment is whatever is leftover.
    int8u fragmentLen = (offset + txPacket->fragmentLen < txPacket->bufLen
                         ? txPacket->fragmentLen
                         : txPacket->bufLen - offset);

    txPacket->apsFrame.groupId = HIGH_LOW_TO_INT(txPacket->fragmentCount, i);

#ifdef EZSP_HOST
#ifdef EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
    if (txPacket->sourceRoute) {
      status = ezspSetSourceRoute(txPacket->indexOrDestination,
                                  txPacket->relayCount,
                                  txPacket->relayList);
      if (status != EMBER_SUCCESS) {
        return status;
      }
    }
#endif //EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
    status = ezspSendUnicast(txPacket->messageType,
                             txPacket->indexOrDestination,
                             &txPacket->apsFrame,
                             i,
                             fragmentLen,
                             txPacket->buffer + offset,
                             &txPacket->apsFrame.sequence);
#else //EZSP_HOST
    {
      EmberMessageBuffer message;
      message = emberFillLinkedBuffers(txPacket->buffer + offset,
                                       fragmentLen);
      if (message == EMBER_NULL_MESSAGE_BUFFER) {
        return EMBER_NO_BUFFERS;
      }
      status = emberSendUnicast(txPacket->messageType,
                                txPacket->indexOrDestination,
                                &txPacket->apsFrame,
                                message);

      emberReleaseMessageBuffer(message);
    }
#endif //EZSP_HOST

    if (status != EMBER_SUCCESS) {
      return status;
    }

    txPacket->fragmentsInTransit++;
    offset += fragmentLen;
  } // close inner for

  if (txPacket->fragmentsInTransit == 0) {
    txPacket->messageType = 0xFF;
    emAfFragmentationMessageSentHandler(txPacket->messageType,
                                        txPacket->indexOrDestination,
                                        &txPacket->apsFrame,
                                        txPacket->buffer,
                                        txPacket->bufLen,
                                        EMBER_SUCCESS);
  }

  return EMBER_SUCCESS;
}

static void abortTransmission(txFragmentedPacket *txPacket,
                              EmberStatus status)
{
  if (status != EMBER_SUCCESS && txPacket->messageType != 0xFF) {
    emAfFragmentationMessageSentHandler(txPacket->messageType,
                                        txPacket->indexOrDestination,
                                        &txPacket->apsFrame,
                                        txPacket->buffer,
                                        txPacket->bufLen,
                                        status);
    txPacket->messageType = 0xFF;
  }
}

static txFragmentedPacket* getFreeTxPacketEntry(void)
{
  int8u i;
  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS; i++) {
    txFragmentedPacket *txPacket = &(txPackets[i]);
    if (txPacket->messageType == 0xFF) {
      return txPacket;
    }
  }
  return NULL;
}

static txFragmentedPacket* txPacketLookUp(EmberApsFrame *apsFrame)
{
  int8u i;
  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS; i++) {
    txFragmentedPacket *txPacket = &(txPackets[i]);
    if (txPacket->messageType == 0xFF) {
      continue;
    }

    // Each node has a single source APS counter.
    if (apsFrame->sequence == txPacket->apsFrame.sequence) {
      return txPacket;
    }
  }
  return NULL;
}


//------------------------------------------------------------------------------
// Receiving.

#define lowBitMask(n) ((1 << (n)) - 1)
static void setFragmentMask(rxFragmentedPacket *rxPacket);
static boolean storeRxFragment(rxFragmentedPacket *rxPacket,
                               int8u fragment,
                               int8u *buffer,
                               int16u bufLen);
static void moveRxWindow(rxFragmentedPacket *rxPacket);
static rxFragmentedPacket* getFreeRxPacketEntry(void);
static rxFragmentedPacket* rxPacketLookUp(EmberApsFrame *apsFrame,
                                          EmberNodeId sender);

static rxFragmentedPacket rxPackets[EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS];

static void ageAllAckedRxPackets(void)
{
  int8u i;
  for(i=0; i<EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    if (rxPackets[i].status == EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_ACKED) {
      rxPackets[i].ackedPacketAge++;
    }
  }
}

boolean emAfFragmentationIncomingMessage(EmberApsFrame *apsFrame,
                                         EmberNodeId sender,
                                         int8u **buffer,
                                         int16u *bufLen)
{
  static boolean rxWindowMoved = FALSE;
  boolean newFragment;
  int8u fragment;
  int8u mask;
  rxFragmentedPacket *rxPacket;

  if (!(apsFrame->options & EMBER_APS_OPTION_FRAGMENT)) {
    return FALSE;
  }

  assert(*bufLen <= MAX_INT8U_VALUE);

  rxPacket = rxPacketLookUp(apsFrame, sender);
  fragment = LOW_BYTE(apsFrame->groupId);

  // First fragment for this packet, we need to set up a new entry.
  if (rxPacket == NULL) {
    rxPacket = getFreeRxPacketEntry();
    if (rxPacket == NULL || fragment >= emberFragmentWindowSize)
      return TRUE;

    rxPacket->status = EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_IN_USE;
    rxPacket->fragmentSource = sender;
    rxPacket->fragmentSequenceNumber = apsFrame->sequence;
    rxPacket->fragmentBase = 0;
    rxPacket->windowFinger = 0;
    rxPacket->fragmentsReceived = 0;
    rxPacket->fragmentsExpected = 0xFF;
    rxPacket->fragmentLen = (int8u)(*bufLen);
    setFragmentMask(rxPacket);

    emberEventControlSetDelayMS(*(rxPacket->fragmentEventControl),
                                (emberApsAckTimeoutMs
                                 * ZIGBEE_APSC_MAX_TRANSMIT_RETRIES));
  }

  // All fragments inside the rx window have been received and the incoming
  // fragment is outside the receiving window: let's move the rx window.
  if (rxPacket->fragmentMask == 0xFF
      && rxPacket->fragmentBase + emberFragmentWindowSize <= fragment) {
    moveRxWindow(rxPacket);
    setFragmentMask(rxPacket);
    rxWindowMoved = TRUE;

    emberEventControlSetDelayMS(*(rxPacket->fragmentEventControl),
                                (emberApsAckTimeoutMs
                                 * ZIGBEE_APSC_MAX_TRANSMIT_RETRIES));
  }

  // Fragment outside the rx window.
  if (rxPacket->fragmentBase + emberFragmentWindowSize <= fragment) {
    return TRUE;
  } else { // Fragment inside the rx window.
    if (rxWindowMoved){
      // We assume that the fragment length for the new rx window is the length
      // of the first fragment received inside the window. However, if the first
      // fragment received is the last fragment of the packet, we do not
      // consider it for setting the fragment length.
      if (fragment < rxPacket->fragmentsExpected - 1) {
        rxPacket->fragmentLen = (int8u)(*bufLen);
        rxWindowMoved = FALSE;
      }
    } else {
      // We enforce that all the subsequent fragments (except for the last
      // fragment) inside the rx window have the same length as the first one.
      if (fragment < rxPacket->fragmentsExpected - 1
          && rxPacket->fragmentLen != (int8u)(*bufLen)) {
        goto kickout;
      }
    }
  }

  mask = 1 << (fragment % emberFragmentWindowSize);
  newFragment = !(mask & rxPacket->fragmentMask);

  // First fragment, setting the total number of expected fragments.
  if (fragment == 0) {
    rxPacket->fragmentsExpected = HIGH_BYTE(apsFrame->groupId);
    if (rxPacket->fragmentsExpected < emberFragmentWindowSize) {
      setFragmentMask(rxPacket);
    }
  }

  rxPacket->fragmentMask |= mask;
  if (newFragment) {
    rxPacket->fragmentsReceived++;
    if (!storeRxFragment(rxPacket, fragment, *buffer, *bufLen)) {
      goto kickout;
    }
  }

  if (fragment == rxPacket->fragmentsExpected - 1
      || (rxPacket->fragmentMask
          | lowBitMask(fragment % emberFragmentWindowSize)) == 0xFF) {
#ifdef EZSP_HOST
    apsFrame->groupId =
        HIGH_LOW_TO_INT(rxPacket->fragmentMask, rxPacket->fragmentBase);
    ezspSendReply(sender, apsFrame, 0, NULL);
#else //EZSP_HOST
    emberSetReplyFragmentData(HIGH_LOW_TO_INT(rxPacket->fragmentMask,
                                              rxPacket->fragmentBase));
    emberSendReply(apsFrame->clusterId, EMBER_NULL_MESSAGE_BUFFER);
#endif //EZSP_HOST
  }

  // Received all the expected fragments.
  if (rxPacket->fragmentsReceived == rxPacket->fragmentsExpected) {
    int8u fragmentsInLastWindow =
        rxPacket->fragmentsExpected % emberFragmentWindowSize;
    if (fragmentsInLastWindow == 0)
      fragmentsInLastWindow = emberFragmentWindowSize;

    // Pass the reassembled packet only once to the application.
    if (rxPacket->status == EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_IN_USE) {
      //Age all acked packets first
      ageAllAckedRxPackets();
      // Mark the packet entry as acked.
      rxPacket->status = EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_ACKED;
      // Set the age of the new acked packet as the youngest one.
      rxPacket->ackedPacketAge = 0;
      // This library sends replies for all fragments, so, before passing on the
      // reassembled message, clear the retry bit to prevent the application
      // from sending a duplicate reply.
      apsFrame->options &= ~EMBER_APS_OPTION_RETRY;

      // The total size is the window finger + (n-1) full fragments + the last
      // fragment.
      *bufLen = rxPacket->windowFinger + rxPacket->lastfragmentLen
                 + (fragmentsInLastWindow - 1)*rxPacket->fragmentLen;
      *buffer = rxPacket->buffer;
      return FALSE;
    }
  }
  return TRUE;

kickout:
  emAfFragmentationAbortReception(rxPacket->fragmentEventControl);
  return TRUE;
}

void emAfFragmentationAbortReception(EmberEventControl *control)
{
  int8u i;
  emberEventControlSetInactive(*control);

  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    rxFragmentedPacket *rxPacket = &(rxPackets[i]);
    if (rxPacket->fragmentEventControl == control) {
      rxPacket->status = EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_AVAILABLE;
    }
  }
}

static void setFragmentMask(rxFragmentedPacket *rxPacket)
{
  // Unused bits must be 1.
  int8u highestZeroBit = emberFragmentWindowSize;
  // If we are in the final window, there may be additional unused bits.
  if (rxPacket->fragmentsExpected
      < rxPacket->fragmentBase + emberFragmentWindowSize) {
    highestZeroBit = (rxPacket->fragmentsExpected % emberFragmentWindowSize);
  }
  rxPacket->fragmentMask = ~ lowBitMask(highestZeroBit);
}

static boolean storeRxFragment(rxFragmentedPacket *rxPacket,
                               int8u fragment,
                               int8u *buffer,
                               int16u bufLen)
{
  int16u index = rxPacket->windowFinger;

  if (index + bufLen > EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE) {
    return FALSE;
  }

  index += (fragment - rxPacket->fragmentBase)*rxPacket->fragmentLen;
  MEMCOPY(rxPacket->buffer + index, buffer, bufLen);

  // If this is the last fragment of the packet, store its length.
  if (fragment == rxPacket->fragmentsExpected - 1) {
    rxPacket->lastfragmentLen = (int8u)bufLen;
  }

  return TRUE;
}

static void moveRxWindow(rxFragmentedPacket *rxPacket)
{
  rxPacket->fragmentBase += emberFragmentWindowSize;
  rxPacket->windowFinger += emberFragmentWindowSize*rxPacket->fragmentLen;
}

static rxFragmentedPacket* getFreeRxPacketEntry(void)
{
  int8u i;
  rxFragmentedPacket* ackedPacket = NULL;

  // Available entries first.
  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    rxFragmentedPacket *rxPacket = &(rxPackets[i]);
    if (rxPacket->status == EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_AVAILABLE) {
      return rxPacket;
    }
  }

  // Acked packets: Look for the oldest one.
  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    rxFragmentedPacket *rxPacket = &(rxPackets[i]);
    if (rxPacket->status == EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_ACKED) {
      if (ackedPacket == NULL
          || ackedPacket->ackedPacketAge < rxPacket->ackedPacketAge) {
        ackedPacket = rxPacket;
      }
    }
  }

  return ackedPacket;
}

static rxFragmentedPacket* rxPacketLookUp(EmberApsFrame *apsFrame,
                                          EmberNodeId sender)
{
  int8u i;
  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    rxFragmentedPacket *rxPacket = &(rxPackets[i]);
    if (rxPacket->status == EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_AVAILABLE) {
      continue;
    }
    // Each packet is univocally identified by the pair (node id, seq. number).
    if (apsFrame->sequence == rxPacket->fragmentSequenceNumber
        && sender == rxPacket->fragmentSource) {
      return rxPacket;
    }
  }
  return NULL;
}

//------------------------------------------------------------------------------
// Initialization
void emberAfPluginFragmentationInitCallback(void)
{
  int8u i;
#ifndef EZSP_HOST
  emberFragmentWindowSize = EMBER_AF_PLUGIN_FRAGMENTATION_RX_WINDOW_SIZE;
#endif //EZSP_HOST

  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS; i++) {
    rxPackets[i].status = EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_AVAILABLE;
    rxPackets[i].fragmentEventControl = &(emAfFragmentationEvents[i]);
  }

  for(i = 0; i < EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS; i++) {
    txPackets[i].messageType = 0xFF;
  }
}

void emberAfPluginFragmentationNcpInitCallback(void)
{
#ifdef EZSP_HOST
  ezspGetConfigurationValue(EZSP_CONFIG_APS_ACK_TIMEOUT, &emberApsAckTimeoutMs);
  emberAfSetEzspConfigValue(EZSP_CONFIG_FRAGMENT_WINDOW_SIZE,
                            emberFragmentWindowSize,
                            "Fragmentation RX window size");
#endif
}

//------------------------------------------------------------------------------
// CLI stuff

#ifdef EMBER_TEST
static void setRxWindowSize(void);
#ifdef EZSP_HOST
void emAfResetAndInitNCP(void);
#endif //EZSP_HOST
#endif //EMBER_TEST

EmberCommandEntry emberAfPluginFragmentationCommands[] = {

#ifdef EMBER_TEST
  // Allows to set the RX window size. This command doesn't work in simulation
  // for the HOST since the NCP does not really get reset.
  {"set-rx-window-size",      setRxWindowSize, "u"},
#endif //EMBER_TEST

  {NULL},
};

#ifdef EMBER_TEST
static void setRxWindowSize(void)
{
  emberFragmentWindowSize = (int8u)emberUnsignedCommandArgument(0);
  emberAfAppPrintln("Fragmentation RX window size set to 0x%x",
      emberFragmentWindowSize);

#ifdef EZSP_HOST
  emAfResetAndInitNCP();
#endif //EZSP_HOST
}
#endif //EMBER_TEST
