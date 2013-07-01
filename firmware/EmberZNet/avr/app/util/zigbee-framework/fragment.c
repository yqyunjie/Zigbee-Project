// File: fragment.c
//
// Description: Breaks long messages into smaller blocks for transmission and
// reassembles received blocks.
//
//
// Copyright 2007 by Ember Corporation.  All rights reserved.               *80*

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"

#include "fragment.h"


// This is defined in hal/ember-configuration.c.
extern int8u emberFragmentWindowSize;

// DEBUG - Needed for Golden Unit Compliance
// This bitmask controls how the node will respond to a set of
// fragmented data.  It can be used to artifically set a number
// of missed blocks in a fragmented transmission so that the sender
// will retransmit.  It directly relates to the ACK bitmask field
// sent by the receiver.  0xFF means that no blocks will be 
// artifically specified as missed.  If set to something other than 0xFF
// it will send back that bitmask for the first APS Ack and then reset
// the bitmask to 0xFF.
int8u emMissedBlocks = 0xFF;

#define FRAGMENT_DEBUG(x)    do { } while (0)

// A message can be broken into at most this many pieces.
#define MAX_TOTAL_BLOCKS 10

#define ZIGBEE_APSC_MAX_TRANSMIT_RETRIES 3

//------------------------------------------------------------------------------
// Forward declarations.

static EmberStatus sendNextFragments(void);
static void abortTransmission(EmberStatus status);
static void releaseFragments(EmberMessageBuffer *fragments);
static void abortReception(void);

//------------------------------------------------------------------------------
// Sending

// Information needed to actually send the messages.
static EmberOutgoingMessageType fragmentOutgoingType = 0xFF;
static int16u fragmentIndexOrDestination;
static EmberApsFrame fragmentApsFrame;
static int8u totalTxBlocks;
static int8u txWindowBase;
static EmberMessageBuffer txFragments[MAX_TOTAL_BLOCKS];
static int8u blocksInTransit;              // How many payloads are in the air.

EmberStatus emberSendFragmentedMessage(EmberOutgoingMessageType type,
                                       int16u indexOrDestination,
                                       EmberApsFrame *apsFrame,
                                       EmberMessageBuffer payload,
                                       int8u maxFragmentSize)
{
  int8u length = emberMessageBufferLength(payload);
  if (length == 0 || emberFragmentWindowSize == 0) {
    return EMBER_INVALID_CALL;
  }
  totalTxBlocks = (length + maxFragmentSize - 1) / maxFragmentSize;

  FRAGMENT_DEBUG(simPrint("sending %d fragments to %04X",
                          totalTxBlocks, indexOrDestination););

  abortTransmission(EMBER_ERR_FATAL);   // Clear out any existing traffic.
  MEMSET(txFragments,
         EMBER_NULL_MESSAGE_BUFFER,
         MAX_TOTAL_BLOCKS);
  if (totalTxBlocks > MAX_TOTAL_BLOCKS) {
    return EMBER_MESSAGE_TOO_LONG;
  }
  fragmentOutgoingType = type;
  fragmentIndexOrDestination = indexOrDestination;
  MEMCOPY(&fragmentApsFrame, apsFrame, sizeof(EmberApsFrame));

  {
    int8u i = totalTxBlocks - 1;
    int8u fragmentSize = length - ((totalTxBlocks - 1) * maxFragmentSize);
    int8u index = length;

    do {
      EmberMessageBuffer temp = emberFillLinkedBuffers(NULL, fragmentSize);
      if (temp == EMBER_NULL_MESSAGE_BUFFER) {
        releaseFragments(txFragments);
        return EMBER_NO_BUFFERS;
      }
      txFragments[i] = temp;
      index -= fragmentSize;
      emberCopyBufferBytes(temp, 0, payload, index, fragmentSize);
      emberSetLinkedBuffersLength(payload, index);
      fragmentSize = maxFragmentSize;
      i--;
    } while (0 < index);
  }
  fragmentApsFrame.options |= (EMBER_APS_OPTION_FRAGMENT
                               | EMBER_APS_OPTION_RETRY);
  txWindowBase = 0;
  blocksInTransit = 0;
  {
    EmberStatus status = sendNextFragments();
    if (status != EMBER_SUCCESS)
      releaseFragments(txFragments);
    return status;
  }
}

boolean emberIsOutgoingFragment(EmberApsFrame *apsFrame,
                                EmberMessageBuffer buffer,
                                EmberStatus status)
{
  if (apsFrame->options & EMBER_APS_OPTION_FRAGMENT) {
    int8u i;
    FRAGMENT_DEBUG(simPrint("fragment %d sent", buffer););
    for (i = 0; i < MAX_TOTAL_BLOCKS; i++)
      if (buffer == txFragments[i]) {
        FRAGMENT_DEBUG(simPrint("have status %02X for fragment %d",
                                status, i););
        if (status == EMBER_SUCCESS) {
           txFragments[i] = EMBER_NULL_MESSAGE_BUFFER;
           emberReleaseMessageBuffer(buffer);
           blocksInTransit -= 1;
           if (blocksInTransit == 0) {
             txWindowBase += emberFragmentWindowSize;
             abortTransmission(sendNextFragments());
           }
        } else {
          abortTransmission(status);
        }
      }
    return TRUE;
  } else
    return FALSE;
}

static EmberStatus sendNextFragments(void)
{
  int8u i;

  for (i = txWindowBase;
       (i - txWindowBase < emberFragmentWindowSize
        && i < totalTxBlocks);
       i++) {
    EmberStatus status;
    fragmentApsFrame.groupId = HIGH_LOW_TO_INT(totalTxBlocks, i);
    status = emberSendUnicast(fragmentOutgoingType,
                              fragmentIndexOrDestination,
                              &fragmentApsFrame,
                              txFragments[i]);
    if (status == EMBER_SUCCESS)
      blocksInTransit += 1;
    else
      return status;
  }
  if (blocksInTransit == 0) {
    FRAGMENT_DEBUG(simPrint("successfully sent %d fragments to %04X",
                            totalTxBlocks,
                            fragmentIndexOrDestination););
    fragmentOutgoingType = 0xFF;
    emberFragmentedMessageSentHandler(EMBER_SUCCESS);
  }
  return EMBER_SUCCESS;
}

// Release the current window's payloads.
static void releaseFragments(EmberMessageBuffer *fragments)
{
  int8u i;

  for (i = 0; i < MAX_TOTAL_BLOCKS; i++) {
    EmberMessageBuffer fragment = fragments[i];
    if (fragment != EMBER_NULL_MESSAGE_BUFFER) {
      fragments[i] = EMBER_NULL_MESSAGE_BUFFER;
      emberReleaseMessageBuffer(fragment);
    }
  }
}

static void abortTransmission(EmberStatus status)
{
  if (status != EMBER_SUCCESS
      && fragmentOutgoingType != 0xFF) {
    FRAGMENT_DEBUG(simPrint("aborting %d fragments to %04X",
                            totalTxBlocks,
                            fragmentIndexOrDestination););
    releaseFragments(txFragments);
    fragmentOutgoingType = 0xFF;
    emberFragmentedMessageSentHandler(status);
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
static EmberMessageBuffer rxFragments[MAX_TOTAL_BLOCKS];

// An event for timing out failed incoming packets.
static EmberEventControl abortReceptionEvent = {0, 0};

// A mask with the low n bits set.
#define lowBitMask(n) ((1 << (n)) - 1)

static void setBlockMask(void)
{
  // Unused bits must be 1.
  int8u highestZeroBit = emberFragmentWindowSize;
  // If we are in the final window, there may be additional unused bits.
  if (rxWindowBase + emberFragmentWindowSize > expectedRxBlocks) {
    highestZeroBit = (expectedRxBlocks % emberFragmentWindowSize);
  }
  blockMask = ~ lowBitMask(highestZeroBit);
}

boolean emberIsIncomingFragment(EmberApsFrame *apsFrame,
                                EmberMessageBuffer payload)
{
  if (! (apsFrame->options & EMBER_APS_OPTION_FRAGMENT))
    return FALSE;        // Not a fragment, process as usual.
  else {
    int8u blockNumber = LOW_BYTE(apsFrame->groupId);

    if (fragmentSource == EMBER_NULL_NODE_ID
        && blockNumber < emberFragmentWindowSize) {
      fragmentSource = emberGetSender();
      fragmentApsSequenceNumber = apsFrame->sequence;
      rxWindowBase = 0;
      blocksReceived = 0;
      expectedRxBlocks = 0xFF;
      setBlockMask();
      MEMSET(rxFragments,
             EMBER_NULL_MESSAGE_BUFFER,
             MAX_TOTAL_BLOCKS);
      FRAGMENT_DEBUG(simPrint("receiving fragments from %04X",
                              fragmentSource););
      emberEventControlSetDelayMS(abortReceptionEvent,
                                  emberApsAckTimeoutMs
                                  * ZIGBEE_APSC_MAX_TRANSMIT_RETRIES);
    }

    if (! (emberGetSender() == fragmentSource
           && apsFrame->sequence == fragmentApsSequenceNumber))
      return TRUE;      // Drop unexpected fragments.
    else {
      if (blockMask == 0xFF
          && rxWindowBase + emberFragmentWindowSize <= blockNumber) {
        rxWindowBase += emberFragmentWindowSize;
        setBlockMask();
        emberEventControlSetDelayMS(abortReceptionEvent,
                                    emberApsAckTimeoutMs
                                    * ZIGBEE_APSC_MAX_TRANSMIT_RETRIES);
      }

      if (rxWindowBase + emberFragmentWindowSize <= blockNumber)
        return TRUE;    // Drop unexpected fragments.
      else {
        int8u mask = 1 << (blockNumber % emberFragmentWindowSize);
        boolean isNew = ! (mask & blockMask);

        // Golden Unit Compliance
        if ( !(mask & emMissedBlocks) ) {
          FRAGMENT_DEBUG(simPrint("pretending to miss fragment %d (%04X)",
                                  blockNumber,
                                  apsFrame->groupId););
          emMissedBlocks |= mask;  // clear the bit so a retransmission will
                                   // succeed.
          return TRUE;
        }
        // End Compliance

        FRAGMENT_DEBUG(simPrint("have fragment %d (%04X)", blockNumber,
                                apsFrame->groupId););

        if (blockNumber == 0) {
          expectedRxBlocks = HIGH_BYTE(apsFrame->groupId);
          // Need to set unused bits in the window to 1.
          // Previously a full window was assumed.
          if ( expectedRxBlocks < emberFragmentWindowSize )
            setBlockMask();
          FRAGMENT_DEBUG(simPrint("%d fragments from %04X",
                                  expectedRxBlocks, fragmentSource););
          if (MAX_TOTAL_BLOCKS < expectedRxBlocks)
            goto kickout;
        }

        blockMask |= mask;
        if (isNew) {
          blocksReceived += 1;
          rxFragments[blockNumber] = payload;
          emberHoldMessageBuffer(payload);
        }
        
        // ACK if it is the last block in the fragmented message,
        // or it is a new message and we have all fragments in the window,
        // or it is the last block in the window.
        if (blockNumber == expectedRxBlocks - 1
           || (blockMask | lowBitMask(blockNumber % emberFragmentWindowSize))
               == 0xFF) {
          FRAGMENT_DEBUG(simPrint("sending ack base %d mask %02X",
                                  rxWindowBase, blockMask););
          emberSetReplyFragmentData(HIGH_LOW_TO_INT(blockMask, rxWindowBase));
          emberSendReply(apsFrame->clusterId,
                         EMBER_NULL_MESSAGE_BUFFER);
        }
        
        if (blocksReceived == expectedRxBlocks) {
          // To avoid the uncertain effects of copying from a buffer onto
          // itself, we copy the payload into another buffer and then copy
          // everything back into the original payload.
          EmberMessageBuffer copy = emberCopyLinkedBuffers(payload);
          int8u i;
          int8u length = 0;
          
          if (copy == EMBER_NULL_MESSAGE_BUFFER)
            goto kickout;
          
          rxFragments[blockNumber] = copy;
          emberReleaseMessageBuffer(payload);
          emberSetLinkedBuffersLength(payload, 0);

          for (i = 0; i < expectedRxBlocks; i++) {
            EmberMessageBuffer temp = rxFragments[i];
            int8u tempLength = emberMessageBufferLength(temp);

            if (emberSetLinkedBuffersLength(payload, length + tempLength)
                != EMBER_SUCCESS)
              goto kickout;

            emberCopyBufferBytes(payload, length, temp, 0, tempLength);
            length += tempLength;

            emberReleaseMessageBuffer(temp);
            rxFragments[i] = EMBER_NULL_MESSAGE_BUFFER;
          }
          FRAGMENT_DEBUG(simPrint("received %d fragments from %04X",
                                  expectedRxBlocks,
                                  fragmentSource););
          FRAGMENT_DEBUG(printPacketBuffers(payload););
          fragmentSource = EMBER_NULL_NODE_ID;
          return FALSE;
        kickout:
          abortReception();
          return TRUE;
        } else
          return TRUE;
      }
    }
  }
}

// Flush the current message if blocks stop arriving.
static void abortReception(void)
{
  emberEventControlSetInactive(abortReceptionEvent);

  if (fragmentSource != EMBER_NULL_NODE_ID) {
    FRAGMENT_DEBUG(simPrint("aborting reception of %d fragments from %04X",
                            expectedRxBlocks,
                            fragmentSource););
    releaseFragments(rxFragments);
    fragmentSource = EMBER_NULL_NODE_ID;
  }
}

static EmberEventData fragmentationEvents[] =
  {
    { &abortReceptionEvent, abortReception },
    { NULL, NULL }       // terminator
  };

void emberFragmentTick(void)
{
  emberRunEvents(fragmentationEvents);
}

