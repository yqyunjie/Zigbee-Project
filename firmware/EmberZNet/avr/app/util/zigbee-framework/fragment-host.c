// File: fragment-host.c
//
// Description: Breaks long messages into smaller blocks for transmission and
// reassembles received blocks.
//
//
// Copyright 2007 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"

#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"

#include "fragment-host.h"

// A message can be broken into at most this many pieces.
#define MAX_TOTAL_BLOCKS 10

#define ZIGBEE_APSC_MAX_TRANSMIT_RETRIES 3
     
//------------------------------------------------------------------------------
// Forward declarations.

static EmberStatus sendNextFragments(void);
static void abortTransmission(EmberStatus status);
static void abortReception(void);

//------------------------------------------------------------------------------
// Initialization

static int16u receptionTimeout;
static int8u windowSize;
static int8u *receiveMessage;
static int8u receiveMessageMaxLength = 0;

void ezspFragmentInit(int8u receiveBufferLength, int8u *receiveBuffer)
{
  int16u temp;
  receiveMessageMaxLength = receiveBufferLength;
  receiveMessage = receiveBuffer;
  ezspGetConfigurationValue(EZSP_CONFIG_APS_ACK_TIMEOUT, &receptionTimeout);
  ezspGetConfigurationValue(EZSP_CONFIG_FRAGMENT_WINDOW_SIZE, &temp);
  windowSize = temp;
}

//------------------------------------------------------------------------------
// Sending

// Information needed to actually send the messages.
static EmberOutgoingMessageType fragmentOutgoingType = 0xFF;
static int16u fragmentIndexOrDestination;
static EmberApsFrame fragmentApsFrame;
static int8u totalTxBlocks;
static int8u txWindowBase;
static int8u txFragments[MAX_TOTAL_BLOCKS];
static int8u *sendMessage;
static int8u blocksInTransit;              // How many payloads are in the air.

EmberStatus ezspSendFragmentedMessage(EmberOutgoingMessageType type,
                                      int16u indexOrDestination,
                                      EmberApsFrame *apsFrame,
                                      int8u maxFragmentSize,
                                      int8u messageLength,
                                      int8u *messageContents)
{
  EmberStatus status;
  int8u remainingLength = messageLength;
  if (messageLength == 0 || windowSize == 0) {
    return EMBER_INVALID_CALL;
  }
  abortTransmission(EMBER_ERR_FATAL);   // Clear out any existing traffic.
  fragmentOutgoingType = type;
  fragmentIndexOrDestination = indexOrDestination;
  MEMCOPY(&fragmentApsFrame, apsFrame, sizeof(EmberApsFrame));
  sendMessage = messageContents;
  totalTxBlocks = 0;
  while (remainingLength > 0 && totalTxBlocks < MAX_TOTAL_BLOCKS) {
    if (remainingLength > maxFragmentSize) {
      txFragments[totalTxBlocks] = maxFragmentSize;
    } else {
      txFragments[totalTxBlocks] = remainingLength;
    }
    remainingLength -= txFragments[totalTxBlocks];
    totalTxBlocks++;
  }
  if (remainingLength > 0) {
    fragmentOutgoingType = 0xFF;
    return EMBER_MESSAGE_TOO_LONG;
  }
  fragmentApsFrame.options |= (EMBER_APS_OPTION_FRAGMENT
                               | EMBER_APS_OPTION_RETRY);
  txWindowBase = 0;
  blocksInTransit = 0;
  status = sendNextFragments();
  if (status != EMBER_SUCCESS) {
    fragmentOutgoingType = 0xFF;
  }
  return status;
}

boolean ezspIsOutgoingFragment(EmberApsFrame *apsFrame,
                               EmberStatus status)
{
  if (apsFrame->options & EMBER_APS_OPTION_FRAGMENT) {
    if (status == EMBER_SUCCESS) {
       blocksInTransit -= 1;
       if (blocksInTransit == 0) {
         txWindowBase += windowSize;
         abortTransmission(sendNextFragments());
       }
    } else {
      abortTransmission(status);
    }
    return TRUE;
  } else
    return FALSE;
}

static EmberStatus sendNextFragments(void)
{
  int8u i;
  int8u index = 0;
  // Move the message index to the start of the first fragment to be sent.
  for (i = 0; i < txWindowBase; i++) {
    index += txFragments[i];
  }
  // Send fragments until the window is full.
  for (i = txWindowBase;
       i - txWindowBase < windowSize && i < totalTxBlocks;
       i++) {
    EmberStatus status = ezspFragmentSourceRouteHandler();
    fragmentApsFrame.groupId = HIGH_LOW_TO_INT(totalTxBlocks, i);
    if (status == EMBER_SUCCESS) {
      status = ezspSendUnicast(fragmentOutgoingType,
                               fragmentIndexOrDestination,
                               &fragmentApsFrame,
                               i,
                               txFragments[i],
                               sendMessage + index,
                               &fragmentApsFrame.sequence);
    }
    if (status == EMBER_SUCCESS) {
      blocksInTransit += 1;
    } else {
      return status;
    }
    index += txFragments[i];
  }
  if (blocksInTransit == 0) {
    fragmentOutgoingType = 0xFF;
    ezspFragmentedMessageSentHandler(EMBER_SUCCESS);
  }
  return EMBER_SUCCESS;
}

static void abortTransmission(EmberStatus status)
{
  if (status != EMBER_SUCCESS
      && fragmentOutgoingType != 0xFF) {
    fragmentOutgoingType = 0xFF;
    ezspFragmentedMessageSentHandler(status);
  }
}

//------------------------------------------------------------------------------
// Receiving.

// These two are used to identify incoming blocks.
static EmberNodeId fragmentSource = EMBER_NULL_NODE_ID;
static int8u fragmentApsSequenceNumber;

static int8u rxWindowBase;
static int8u blockMask;                // Mask to be sent in the next ACK.
static int8u expectedRxBlocks;         // How many are supposed to arrive.
static int8u blocksReceived;           // How many have arrived.
static int8u rxFragments[MAX_TOTAL_BLOCKS];

static boolean runTimer = FALSE;
static int16u lastRxTime;

// A mask with the low n bits set.
#define lowBitMask(n) ((1 << (n)) - 1)

static void setBlockMask(void)
{
  // Unused bits must be 1.
  int8u highestZeroBit = windowSize;
  // If we are in the final window, there may be additional unused bits.
  if (rxWindowBase + windowSize > expectedRxBlocks) {
    highestZeroBit = (expectedRxBlocks % windowSize);
  }
  blockMask = ~ lowBitMask(highestZeroBit);
}

static void clearRxFragments(void)
{
  int8u i;
  for (i = 0; i < MAX_TOTAL_BLOCKS; i++) {
    rxFragments[i] = 0;
  }
}

static boolean storeRxFragment(int8u blockNumber,
                               int8u messageLength,
                               int8u *messageContents)
{
  int8u i;
  int8u index = 0;
  for (i = 0; i < blockNumber; i++) {
    index += rxFragments[i];
  }
  if (index + messageLength > receiveMessageMaxLength) {
    return FALSE;
  }
  MEMCOPY(receiveMessage + index + messageLength,
          receiveMessage + index,
          receiveMessageMaxLength - (index + messageLength));
  MEMCOPY(receiveMessage + index, messageContents, messageLength);
  rxFragments[blockNumber] = messageLength;
  return TRUE;
}

boolean ezspIsIncomingFragment(EmberApsFrame *apsFrame,
                               EmberNodeId sender,
                               int8u *messageLength,
                               int8u **messageContents)
{
  int8u blockNumber;
  int8u mask;
  boolean newBlock;

  if (! (apsFrame->options & EMBER_APS_OPTION_FRAGMENT)) {
    return FALSE;        // Not a fragment, process as usual.
  }
  blockNumber = LOW_BYTE(apsFrame->groupId);

  if (fragmentSource == EMBER_NULL_NODE_ID
      && blockNumber < windowSize) {
    fragmentSource = sender;
    fragmentApsSequenceNumber = apsFrame->sequence;
    rxWindowBase = 0;
    blocksReceived = 0;
    expectedRxBlocks = 0xFF;
    setBlockMask();
    clearRxFragments();
    runTimer = TRUE;
    lastRxTime = halCommonGetInt16uMillisecondTick();
  }
  
  if (! (sender == fragmentSource
         && apsFrame->sequence == fragmentApsSequenceNumber)) {
    return TRUE;      // Drop unexpected fragments.
  }
  if (blockMask == 0xFF
      && rxWindowBase + windowSize <= blockNumber) {
    rxWindowBase += windowSize;
    setBlockMask();
    runTimer = TRUE;
    lastRxTime = halCommonGetInt16uMillisecondTick();
  }
  
  if (rxWindowBase + windowSize <= blockNumber) {
    return TRUE;    // Drop unexpected fragments.
  }
  mask = 1 << (blockNumber % windowSize);
  newBlock = ! (mask & blockMask);

  if (blockNumber == 0) {
    expectedRxBlocks = HIGH_BYTE(apsFrame->groupId);
    // Need to set unused bits in the window to 1.
    // Previously a full window was assumed.
    if ( expectedRxBlocks < windowSize )
      setBlockMask();
    if (expectedRxBlocks > MAX_TOTAL_BLOCKS) {
      goto kickout;
    }
  }

  blockMask |= mask;
  if (newBlock) {
    blocksReceived += 1;
    if (!storeRxFragment(blockNumber, *messageLength, *messageContents)) {
      goto kickout;
    }
  }

  if (blockNumber == expectedRxBlocks - 1
     || (blockMask | lowBitMask(blockNumber % windowSize))
         == 0xFF) {
    apsFrame->groupId = HIGH_LOW_TO_INT(blockMask, rxWindowBase);
    ezspSendReply(sender, apsFrame, 0, NULL);
  }

  if (blocksReceived == expectedRxBlocks) {
    int8u i;
    int8u length = 0;
    for (i = 0; i < expectedRxBlocks; i++) {
      length += rxFragments[i];
    }
    fragmentSource = EMBER_NULL_NODE_ID;
    *messageLength = length;
    *messageContents = receiveMessage;
    apsFrame->options &= ~ EMBER_APS_OPTION_RETRY;
    return FALSE;
  }
  return TRUE;
kickout:
  abortReception();
  return TRUE;
}

// Flush the current message if blocks stop arriving.
static void abortReception(void)
{
  runTimer = FALSE;

  if (fragmentSource != EMBER_NULL_NODE_ID) {
    clearRxFragments();
    fragmentSource = EMBER_NULL_NODE_ID;
  }
}

void ezspFragmentTick(void)
{
  if (runTimer) {
    int16u now = halCommonGetInt16uMillisecondTick();
    if ((int16u)(now - lastRxTime)
        > receptionTimeout * ZIGBEE_APSC_MAX_TRANSMIT_RETRIES) {
      abortReception();
    }
  }
}
