// *****************************************************************************
// * fragmentation.h
// *
// * Splits long messages into smaller blocks for transmission and reassembles
// * received blocks.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************
#ifndef ZIGBEE_APSC_MAX_TRANSMIT_RETRIES
#define ZIGBEE_APSC_MAX_TRANSMIT_RETRIES 3
#endif //ZIGBEE_APSC_MAX_TRANSMIT_RETRIES

#ifndef EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS
#define EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS 2
#endif //EMBER_AF_PLUGIN_FRAGMENTATION_MAX_INCOMING_PACKETS

#ifndef EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS
#define EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS 2
#endif //EMBER_AF_PLUGIN_FRAGMENTATION_MAX_OUTGOING_PACKETS

#ifndef EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE
#define EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE 1500
#endif //EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE

#ifndef EMBER_AF_PLUGIN_FRAGMENTATION_RX_WINDOW_SIZE
#define EMBER_AF_PLUGIN_FRAGMENTATION_RX_WINDOW_SIZE 1
#endif //EMBER_AF_PLUGIN_FRAGMENTATION_RX_WINDOW_SIZE

// TODO: We should have the App Builder generating these events. For now, I
// manually added 10 events which means we will be able to set and accept up to
// 10 incoming distinct fragmented packets. In AppBuilder the max incoming
// packets number is capped to 10, therefore we will never run out of events.
#define EMBER_AF_FRAGMENTATION_EVENTS \
  {&(emAfFragmentationEvents[0]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[1]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[3]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[4]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[5]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[6]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[7]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[8]), (void (*)(void))emAfFragmentationAbortReception}, \
  {&(emAfFragmentationEvents[9]), (void (*)(void))emAfFragmentationAbortReception},

#define EMBER_AF_FRAGMENTATION_EVENT_STRINGS \
  "Frag 0", \
  "Frag 1", \
  "Frag 2", \
  "Frag 3", \
  "Frag 4", \
  "Frag 5", \
  "Frag 6", \
  "Frag 7", \
  "Frag 8", \
  "Frag 9",

extern EmberEventControl emAfFragmentationEvents[10];

//------------------------------------------------------------------------------
// Sending

typedef struct {
  EmberOutgoingMessageType  messageType;
  int16u                    indexOrDestination;
  int8u                     sequence;
  EmberApsFrame             apsFrame;
#ifdef EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  boolean                   sourceRoute;
  int8u                     relayCount;
  int16u                    relayList[ZA_MAX_HOPS];
#endif //EZSP_APPLICATION_HAS_ROUTE_RECORD_HANDLER
  int8u                     buffer[EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE];
  int16u                    bufLen;
  int8u                     fragmentLen;
  int8u                     fragmentCount;
  int8u                     fragmentBase;
  int8u                     fragmentsInTransit;
}txFragmentedPacket;

EmberStatus emAfFragmentationSendUnicast(EmberOutgoingMessageType type,
                                         int16u indexOrDestination,
                                         EmberApsFrame *apsFrame,
                                         int8u *buffer,
                                         int16u bufLen);

boolean emAfFragmentationMessageSent(EmberApsFrame *apsFrame,
                                     EmberStatus status);

void emAfFragmentationMessageSentHandler(EmberOutgoingMessageType type,
                                         int16u indexOrDestination,
                                         EmberApsFrame *apsFrame,
                                         int8u *buffer,
                                         int16u bufLen,
                                         EmberStatus status);

//------------------------------------------------------------------------------
// Receiving.

typedef enum {
  EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_AVAILABLE        = 0,
  EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_ACKED            = 1,
  EMBER_AF_PLUGIN_FRAGMENTATION_RX_PACKET_IN_USE           = 2
}rxPacketStatus;

typedef struct {
  rxPacketStatus status;
  int8u       ackedPacketAge;
  int8u       buffer[EMBER_AF_PLUGIN_FRAGMENTATION_BUFFER_SIZE];
  EmberNodeId fragmentSource;
  int8u       fragmentSequenceNumber;
  int8u       fragmentBase; // first fragment inside the rx window.
  int16u      windowFinger; //points to the first byte inside the rx window.
  int8u       fragmentsExpected; // total number of fragments expected.
  int8u       fragmentsReceived; // fragments received so far.
  int8u       fragmentMask; // bitmask of received fragments inside the rx window.
  int8u       lastfragmentLen; // Length of the last fragment.
  int8u       fragmentLen; // Length of the fragment inside the rx window.
                           // All the fragments inside the rx window should have
                           // the same length.
  EmberEventControl *fragmentEventControl;
}rxFragmentedPacket;

boolean emAfFragmentationIncomingMessage(EmberApsFrame *apsFrame,
                                         EmberNodeId sender,
                                         int8u **buffer,
                                         int16u *bufLen);

void emAfFragmentationAbortReception(EmberEventControl* control);
