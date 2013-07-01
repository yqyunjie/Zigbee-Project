// *******************************************************************
//  common.c
//
//  common functions for sensor sample app
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor/common.h"
#include "app/util/security/security.h"

#ifdef SINK_APP
extern int16u ticksSinceLastHeard[];
#endif

#ifdef SENSOR_APP
void printDataMode(void);
#endif



// *********************************
// utility function, used below in printing the security keys that are set
// *********************************
void sensorPrintKeyOptions(EmberKeyStruct* key)
{
  
  // sequence number
  emberSerialPrintf(APP_SERIAL, "\r\n    seqNum [");
  if ((key->bitmask) & EMBER_KEY_HAS_SEQUENCE_NUMBER) {
    emberSerialPrintf(APP_SERIAL, "%x] ", key->sequenceNumber);
  } else {
    emberSerialPrintf(APP_SERIAL, "none] ");
  }
  emberSerialWaitSend(APP_SERIAL);

  // outgoing frame counter
  emberSerialPrintf(APP_SERIAL, "\r\n    outFC [");
  if (key->bitmask & EMBER_KEY_HAS_OUTGOING_FRAME_COUNTER ) {
    emberSerialPrintf(APP_SERIAL, "%4x] ", key->outgoingFrameCounter);
  } else {
    emberSerialPrintf(APP_SERIAL, "none] ");
  }
  emberSerialWaitSend(APP_SERIAL);

  // incoming frame counter
  emberSerialPrintf(APP_SERIAL, "\r\n    inFC [");
  if (key->bitmask & EMBER_KEY_HAS_INCOMING_FRAME_COUNTER ) {
    emberSerialPrintf(APP_SERIAL, "%4x] ",
                      key->incomingFrameCounter);
  } else {
    emberSerialPrintf(APP_SERIAL, "none] ");
  }
  emberSerialWaitSend(APP_SERIAL);

  // partner EUI
  emberSerialPrintf(APP_SERIAL, "\r\n    partner [");
  if (key->bitmask & EMBER_KEY_HAS_PARTNER_EUI64) {
    printEUI64(APP_SERIAL, (EmberEUI64*) key->partnerEUI64);
    emberSerialPrintf(APP_SERIAL, "]");
  } else {
    emberSerialPrintf(APP_SERIAL, "None] ");
  }
  emberSerialWaitSend(APP_SERIAL);

  // is key authorized
  emberSerialPrintf(APP_SERIAL, "\r\n    auth [");
  if (key->bitmask & EMBER_KEY_IS_AUTHORIZED) {
    emberSerialPrintf(APP_SERIAL, "Y] ");
  } else {
    emberSerialPrintf(APP_SERIAL, "N] ");
  }
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "\r\n");
  
  
}

// *********************************
// utility function, used below in printing the security keys that are set
// *********************************
void sensorCommonPrint16ByteKey(int8u* key) 
{
  int8u i;
  for (i=0; i<EMBER_ENCRYPTION_KEY_SIZE; i+=8) {
    // To save on buffers, we reduce the number of calls to 
    // emberSerialPrintf().
    emberSerialPrintf(APP_SERIAL, "%X %X %X %X %X %X %X %X ", 
                      key[i+0], key[i+1], key[i+2], key[i+3],
                      key[i+4], key[i+5], key[i+6], key[i+7]);
    emberSerialWaitSend(APP_SERIAL);
  }
  emberSerialWaitSend(APP_SERIAL);
  
}



// *********************************
// Set the security keys and the security state - specific to this 
// application, all variants of this application (sink, sensor, 
// sleepy-sensor, mobile-sensor) need to use the same security setup.
// *********************************
void sensorCommonSetupSecurity(void)
{
  boolean status;

  // this application chooses to use a preconfigured link key. This means
  // the link key is set on each device.
  EmberKeyData preconfiguredLinkKey =
    {'Z','i','g','b','e','e',' ','S','e','c','u','r','i','t','y','!'};

#ifdef SINK_APP
  // the sink is the security trust center. The trust center needs to
  // set the network security key. Other devices besides the trust center 
  // do not need to set the network security key.
  EmberKeyData networkKey =
    {'e','m','b','e','r',' ','E','M','2','5','0',' ','c','h','i','p'};

  // this call is located in app/util/security/trust-center.c. This 
  // call sets the preconfigured link key, the network key, sets up 
  // the correct EmberInitialSecurityState bitmask and sets the 
  // networkKeySequenceNumber to 0. If the preconfiguredLinkKey passed
  // in is NULL, this picks a random one and assumes joining devices 
  // dont have the key. If the preconfiguredLinkKey passed in is a 
  // correct key, then it assumes joining devices do have the link
  // key preconfigured.
  status = trustCenterInit(&preconfiguredLinkKey, &networkKey);
 
#else // SINK_APP

  // setup the secutity on non-trust center devices. 
  // this call is located in app/util/security/node.c
  status = nodeSecurityInit(&preconfiguredLinkKey);
#endif // SINK_APP


  // print error if necessary
  if (!status) {
    emberSerialPrintf(APP_SERIAL, "ERROR: initializing security\r\n", 
                      status);
  } else {
    // if no error, print the keys that were set. Here we print the keys
    // that we passed in, we don't use emberGetKey since this only
    // returns the correct key if a device is joined.
    emberSerialPrintf(APP_SERIAL, "INFO : link key set to: ");
    sensorCommonPrint16ByteKey(preconfiguredLinkKey.contents);
#ifdef SINK_APP
    emberSerialPrintf(APP_SERIAL, "INFO : nwk key set to:  ");
    sensorCommonPrint16ByteKey(networkKey.contents);
#endif // SINK_APP
  }

}


// *********************************
// print the extendedPanId
// *********************************
void printExtendedPanId(int8u port, int8u *extendedPanId) {
  int8u i;
  emberSerialPrintf(port, "ExtendedPanId: ");
  for (i = 0 ; i < EXTENDED_PAN_ID_SIZE ; i++) {
    emberSerialPrintf(port, "%x", extendedPanId[i]);
  }
}


// *********************************
// print info about this node
// *********************************
void printNodeInfo(void) {
  int8u channel;
  int8u power;
  int16u panId;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE];

  channel = emberGetRadioChannel();
  power = emberGetRadioPower();
  panId = emberGetPanId();
  emberGetExtendedPanId(extendedPanId);

#ifdef SINK_APP
  emberSerialPrintf(APP_SERIAL, "sink ");
#else
  emberSerialPrintf(APP_SERIAL, "sensor ");
#endif
  emberSerialPrintf(APP_SERIAL, "eui [");
  printEUI64(APP_SERIAL, (EmberEUI64*)emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "] short ID [%2x]\r\n", emberGetNodeId());
  emberSerialWaitSend(APP_SERIAL);

  if (emberNetworkState() == EMBER_JOINED_NETWORK) {
    emberSerialPrintf(APP_SERIAL,
                    "channel [0x%x], power [0x%x], panId [0x%2x]\r\n",
                    channel, power, panId);
    printExtendedPanId(APP_SERIAL, extendedPanId);
  } else {
    emberSerialPrintf(APP_SERIAL,
                    "channel [N/A], power [N/A], panId [N/A] \r\n");
    emberSerialPrintf(APP_SERIAL, "ExtendedPanId [N/A]");
  }
  emberSerialPrintf(APP_SERIAL, ", stack [%2x], app [2.60]\r\n",
                    SOFTWARE_VERSION);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "security level [%x]\r\n",
                    EMBER_SECURITY_LEVEL);
  emberSerialWaitSend(APP_SERIAL);
#ifdef SENSOR_APP
  printDataMode();
#endif

#ifdef DEBUG
  emberSerialPrintf(APP_SERIAL, "DEBUG IS ON\r\n");
  emberSerialWaitSend(APP_SERIAL);
#endif

}

#if EMBER_SECURITY_LEVEL == 5
/******************************************************************************
// Print the Keys
*******************************************************************************/
void sensorCommonPrintKeys(void) {
  EmberKeyStruct keyStruct;
  EmberKeyType keyType = EMBER_TRUST_CENTER_LINK_KEY;
  EmberNetworkStatus networkStatus; 
  int8u keyTypesMax = 2;
  int8u i;
  EmberStatus status;

  networkStatus = emberNetworkState();
  
  if (networkStatus == EMBER_JOINED_NETWORK ||
      networkStatus == EMBER_JOINED_NETWORK_NO_PARENT) {

    for (i=1; i<=keyTypesMax; i++) {
      //Get the key data
      emberGetKey(keyType, &keyStruct);
      
      if (keyType == EMBER_TRUST_CENTER_LINK_KEY) {
        emberSerialPrintf(APP_SERIAL,"EMBER_TRUST_CENTER_LINK_KEY: ");
      } else {
        emberSerialPrintf(APP_SERIAL,"EMBER_CURRENT_NETWORK_KEY:  ");
      }

      //print the key
      sensorCommonPrint16ByteKey(emberKeyContents(&keyStruct.key));
      sensorPrintKeyOptions(&keyStruct);

      //change key type
      keyType = EMBER_CURRENT_NETWORK_KEY;
    }

    // print the link key table - sensor app doesnt use link keys
    // so comment this code out
    /*
    emberSerialPrintf(APP_SERIAL, "Link Key Table, size = 0x%x", 
                      EMBER_KEY_TABLE_SIZE);
    for (i=0; i<EMBER_KEY_TABLE_SIZE; i++) {
      status = emberGetKeyTableEntry(i, &keyStruct);
      emberSerialPrintf(APP_SERIAL, "\r\n%x:", i);

      switch (status) {
      case EMBER_KEY_INVALID:
        emberSerialPrintf(APP_SERIAL, "invalid key");
        break;
      case EMBER_INDEX_OUT_OF_RANGE:
        emberSerialPrintf(APP_SERIAL, "index out of range");
        break;
      case EMBER_SUCCESS:
        sensorCommonPrint16ByteKey(emberKeyContents(&keyStruct.key));
        sensorPrintKeyOptions(&keyStruct);
        break;
      case EMBER_LIBRARY_NOT_PRESENT:
        emberSerialPrintf(APP_SERIAL, "link key library not present");
        break;
      default:
        emberSerialPrintf(APP_SERIAL, "unknown status 0x%x", status);
      }
    }
    */
  } else {
    emberSerialPrintf(APP_SERIAL,
      "INFO : not a valid network state to query the keys \r\n ");
  }
}
#endif //EMBER_SECURITY_LEVEL == 5


// ********************************
// to print address table
// ********************************
void printAddressTable(int8u tableSize) {
    int8u i;
    EmberEUI64 remoteEui64;
    EmberNodeId remoteNodeId;
    int8u indexLow;
    int8u indexHigh;

    emberSerialPrintf(APP_SERIAL, "Index  Inuse  NodeId   Eui64");
#ifdef SINK_APP
    emberSerialPrintf(APP_SERIAL, "             Age");
#endif
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);

    for (i=0; i<tableSize; i++) {
      indexLow = (i % 10) + 48;
      indexHigh = ((i - (i % 10))/10) + 48;

      emberSerialPrintf(APP_SERIAL, " %c%c: ",
                        indexHigh, indexLow);

      remoteNodeId = emberGetAddressTableRemoteNodeId(i);
      if (remoteNodeId == EMBER_TABLE_ENTRY_UNUSED_NODE_ID)
        emberSerialPrintf(APP_SERIAL, "  FALSE");
      else
        emberSerialPrintf(APP_SERIAL, "  TRUE ");

      switch (remoteNodeId) {
      case EMBER_TABLE_ENTRY_UNUSED_NODE_ID:
        emberSerialPrintf(APP_SERIAL, "  UNUSED   ", remoteNodeId);
        break;
      case EMBER_UNKNOWN_NODE_ID:
        emberSerialPrintf(APP_SERIAL, "  UNKNOWN  ", remoteNodeId);
        break;
      case EMBER_DISCOVERY_ACTIVE_NODE_ID:
        emberSerialPrintf(APP_SERIAL, "  DISC ACT ", remoteNodeId);
        break;
      default:
        emberSerialPrintf(APP_SERIAL, "  0x%2x   ", remoteNodeId);
        break;
      }

      emberGetAddressTableRemoteEui64(i, remoteEui64);
      printEUI64(APP_SERIAL, &remoteEui64);

#ifdef SINK_APP
      emberSerialPrintf(APP_SERIAL, "  0x%2x", ticksSinceLastHeard[i]);
#endif
      emberSerialPrintf(APP_SERIAL, "\r\n");
      emberSerialWaitSend(APP_SERIAL);
    }
}

// ********************************
// to print multicast table
// ********************************
void printMulticastTable(int8u tableSize) {
    int8u i;
    EmberMulticastTableEntry *multicastTableEntry;
    int8u indexLow;
    int8u indexHigh;

    emberSerialPrintf(APP_SERIAL, "Index  MulticastId  Endpoint\r\n");
    emberSerialWaitSend(APP_SERIAL);

    for (i=0, multicastTableEntry = emberMulticastTable;
         i<tableSize; 
         i++, multicastTableEntry++) {
      indexLow = (i % 10) + 48;
      indexHigh = ((i - (i % 10))/10) + 48;
      emberSerialPrintf(APP_SERIAL, " %c%c:   ",
                        indexHigh, indexLow);
      emberSerialPrintf(APP_SERIAL, "  0x%2x     ", 
                        multicastTableEntry->multicastId);
      emberSerialPrintf(APP_SERIAL, "  0x%x\r\n", 
                        multicastTableEntry->endpoint);
      emberSerialWaitSend(APP_SERIAL);
    }
}

// *********************************
// utility for printing EUI64 addresses
// *********************************
void printEUI64(int8u port, EmberEUI64* eui) {
  int8u i;
  int8u* p = (int8u*)eui;
  for (i=8; i>0; i--) {
    emberSerialPrintf(port, "%x", p[i-1]);
  }
}

// *********************************
// print the node's tokens
// *********************************
void printTokens() {
  tokTypeStackNodeData tokNodeData;
  tokTypeStackKeys tokKeys;
  int i;

  halCommonGetToken(&tokNodeData, TOKEN_STACK_NODE_DATA);
  halCommonGetToken(&tokKeys, TOKEN_STACK_KEYS);

  emberSerialPrintf(APP_SERIAL, "PAN_ID: 0x%2x\r\n",
                    tokNodeData.panId);
  emberSerialPrintf(APP_SERIAL, "RADIO_TX_POWER: 0x%x\r\n",
                    tokNodeData.radioTxPower);
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "RADIO_FREQ_CHANNEL: 0x%x\r\n",
                    tokNodeData.radioFreqChannel);
  emberSerialPrintf(APP_SERIAL, "STACK_PROFILE: 0x%x\r\n",
                    tokNodeData.stackProfile);
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "NODE_TYPE: 0x%x\r\n",
                    tokNodeData.nodeType);
  emberSerialPrintf(APP_SERIAL, "NETWORK_KEY: 0x");
  for (i=0;i<16;i++) {
    emberSerialPrintf(APP_SERIAL, "%x", tokKeys.networkKey[i]);
    emberSerialWaitSend(APP_SERIAL);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "ACTIVE_KEY_SEQ_NUM: 0x%x\r\n",
                    tokKeys.activeKeySeqNum);
  emberSerialPrintf(APP_SERIAL, "ZIGBEE_NODE_ID: 0x%2x\r\n",
                    tokNodeData.zigbeeNodeId);
  emberSerialWaitSend(APP_SERIAL);
}


// *********************************************************************
// the next set of variables are needed to support EM250 bootloader.
#ifdef USE_BOOTLOADER_LIB

EmberNodeId bootloadInProgressChildId;
EmberEUI64  bootloadInProgressEui;
void sendBootloaderLaunchMessage(EmberEUI64 targetEui);
boolean parentLaunchBootload = FALSE;

#endif // USE_BOOTLOADER_LIB

#if defined(SENSOR_APP) || defined(SINK_APP)
// *********************************************************************
// The follwing section has to do with sending APS messages to sleeping
// children. This code is needed by devices that act as parents

// Called when the parent receives a MSG_SINK_QUERY. The parent will
// respond with a MSG_SINK_ADVERTISE
void handleSinkQuery(EmberNodeId childId)
{
  EmberMessageBuffer buffer;
  EmberApsFrame apsFrame;

  if (mainSinkFound == TRUE) {
    EmberStatus status;

    // the data - sink long address (EUI), sink short address
    MEMCOPY(&(globalBuffer[0]), sinkEUI, EUI64_SIZE);
    emberStoreLowHighInt16u(&(globalBuffer[EUI64_SIZE]), sinkShortAddress);

    // copy the data into a packet buffer
    buffer = emberFillLinkedBuffers((int8u*)globalBuffer, EUI64_SIZE + 2);

    // check to make sure a buffer is available
    if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
      emberSerialPrintf(APP_SERIAL,
                        "TX ERROR [sink advertise], OUT OF BUFFERS\r\n");
      return;
    }

    // all of the defined values below are from app/sensor-host/common.h
    // with the exception of the options from stack/include/ember.h
    apsFrame.profileId = PROFILE_ID;          // profile unique to this app
    apsFrame.clusterId = MSG_SINK_ADVERTISE;
    apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
    apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
    apsFrame.options = EMBER_APS_OPTION_RETRY;

    // send the message
    status = emberSendUnicast(EMBER_OUTGOING_DIRECT,
                              childId, 
                              &apsFrame,
                              buffer);

    // done with the packet buffer
    emberReleaseMessageBuffer(buffer);

    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, 
                        "ERROR: send unicast, child %2x, status %x\r\n",
                        childId, status);
      emberSerialWaitSend(APP_SERIAL);
    }
  }
}

// bitmasks to tell which children have or have not been sent a
// partcular JIT message - one bitmask per message. This can support
// up to 16 children
int16u jitMaskMulticastHello;

// Message Buffers are allocated for each message type
EmberMessageBuffer jitMessageMulticastHello = EMBER_NULL_MESSAGE_BUFFER;

// all JIT messages in this app use the same APS frame setup except
// for the clusterID. The APS header is setup before it is needed to
// save time. Only the clusterID is modified before a message is sent
EmberApsFrame jitMessageApsFrame;


// Called from appAddJitForAllChildren which is called when the parent
// receives a message that it must turn into a JIT message.
//
// This fills in the Message Buffer for the JIT message type passed in
// if it has not already been filled in. It also updates the data in the
// MessageBuffer to the data passed in.
void createAndStoreJitMessage(int8u messageType,
                              EmberMessageBuffer* globalJitBuffer,
                              int8u* data,
                              int8u dataLength)
{
  // make sure we don't put in too much data and overrun the packet buffer
  if (dataLength  > PACKET_BUFFER_SIZE)
  {
    assert(FALSE);
  }

  // if this is the first time we are creating the JIT message then
  // setup the aps header. Also allocate a buffer
  if ((*globalJitBuffer) == EMBER_NULL_MESSAGE_BUFFER)
  {
    emberSerialPrintf(APP_SERIAL, "EVENT: creating JIT msg 0x%x\r\n", 
                      messageType);
    emberSerialWaitSend(APP_SERIAL);

    jitMessageApsFrame.sourceEndpoint = ENDPOINT;
    jitMessageApsFrame.destinationEndpoint = ENDPOINT;
    // the cluster ID will change based on the message
    //jitMessageApsFrame.clusterId      = clusterId;
    jitMessageApsFrame.profileId      = PROFILE_ID;
    jitMessageApsFrame.options        = EMBER_APS_OPTION_POLL_RESPONSE;

    (*globalJitBuffer) = emberFillLinkedBuffers(data,
                                             dataLength);
    if ((*globalJitBuffer) == EMBER_NULL_MESSAGE_BUFFER) {
      emberSerialPrintf(APP_SERIAL, "ERROR: no buffers to construct JIT message!\r\n");
      return;
    }
  }

}

// Called when the parent receives a message that it must turn into
// a JIT message.
//
// this sets the JIT message flag for all children, and sets
// the bitmask that the application uses to make sure it doesn't
// send the same JIT message to a child more than once.
void appAddJitForAllChildren(int8u msgType, 
                             int8u* data, 
                             int8u dataLength)
{
  int8u i;
  EmberStatus status;

  if (msgType == MSG_MULTICAST_HELLO) {
    jitMaskMulticastHello = 0xFFFF;
    createAndStoreJitMessage(MSG_MULTICAST_HELLO,
                             &jitMessageMulticastHello,
                             data, dataLength);
  }

  // set the message flag for all children
  emberSerialPrintf(APP_SERIAL, "JIT: setting flag, for all children\r\n");

  for (i=0; i<emberMaxChildCount(); i++)
  {
    EmberNodeId childId = emberChildId(i);
    if (childId != EMBER_NULL_NODE_ID) {
      status = emberSetMessageFlag(childId);
      if (status != EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL,
                          "ERROR: set flag, child %2x, status %x\r\n",
                          emberChildId(i), status);
        emberSerialWaitSend(APP_SERIAL);
      }
    }
  }
}

// This is called from emberPollHandler when the polling node has it's
// message flag set.
//
// This sends the JIT message to the child using an APS message. The
// child should be awake because it has just polled and the parent should
// have sent a mac ack with frame pending set to true
void appSendJitToChild(EmberNodeId childId)
{
  EmberStatus status;
  // used for debugging (see below)
  //   boolean sentMcastHello = FALSE;
  //   EmberStatus mcastHelloStatus;

  // get the child index from child ID. The index is used by the
  // application to determine if the child has already polled for
  // that message
  int8u childIndex = emberChildIndex(childId);

  // This application only sends 1 message per poll. If the sleepy child gets
  // data from a poll it will nap and poll again (not hibernate) until it
  // doesn't get any data from a poll. In order to send more than one message
  // in response to a single poll, the parent needs to be sure to clear the
  // message flag (emberClearMessageFlag) after the first message has been
  // put into the message queue. If the flag is cleared before that, then
  // the first message will have framePending=false and the child will not
  // be awake to hear the second message. The best way to handle this is
  // to keep track of the number of messages sent to each child, decrement
  // this for each emberMessageSentHandler callback, and then clear the message
  // flag (in emberMessageSentHandler) when the number of outstanding messages go
  // to zero. This application doesn't do this and instead sends one
  // JIT message at a time. If there is a second JIT message on the parent
  // it will be picked up on the next nap cycle, set for 1 second.

  // check if this child has already polled for the message
  if (BIT(childIndex) & jitMaskMulticastHello) {

    jitMessageApsFrame.clusterId = MSG_MULTICAST_HELLO;
    /*mcastHelloStatus = */ emberSendUnicast(EMBER_OUTGOING_DIRECT,
                                             childId,
                                             &jitMessageApsFrame,
                                             jitMessageMulticastHello);
    // sentMcastHello = TRUE;
    // clear the bit - invert the number that has the child index'th bit set (xor)
    // and then bitwise AND to clear from the jitMask
    jitMaskMulticastHello = jitMaskMulticastHello & (BIT(childIndex) ^ 0xFFFF);
  }

  // The next section of code is used for debugging and is commented out.
  // Serial characters are dropped when there are a bunch of these messages
  // going out at the same time so it's not very useful with more than one
  // or two children, but can be helpful when initially testing modifications
  // to this code (for instance, adding another JIT type)
  // 
  //  if (sentMcastHello) {
  //    emberSerialPrintf(APP_SERIAL, "TX JIT [hello]:%2x(%x)\r\n", 
  //                      childId, mcastHelloStatus);
  //  }


  // clear the message flag if there are no msgs waiting for this node
  if (!(BIT(childIndex) & jitMaskMulticastHello))
  {
    status = emberClearMessageFlag(childId);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, "ERROR: %x, clear flag %2x\r\n", 
                        status, childId);
    }
    // The next three lines are commented out but may be useful when
    // modifying and then debugging this code.
    //else {
    //  emberSerialPrintf(APP_SERIAL, "JIT: clear flag %2x\r\n", childId);
    //}
  }
}

// This is called by the stack when a device polls. When
// transmitExpected is true, this means the message flag is set for
// this child.
//
void emberPollHandler(EmberNodeId childId, boolean transmitExpected)
{
  // functions called from within this should not contain actions that
  // could take a long time. The messages sent in response to the poll
  // have to be within 15ms. Doing a emberSerialWaitSend in here, for
  // example would cause the sleepy end device to go back to sleep
  // before the parent was able to send any messages at all.

  if (transmitExpected)
  {
	  // Check if we need to send the initial bootload launch message.
    // In addition don't call appSendJitToChild if we have bootload process
    // pending since it could confuse the bootload by sending other JIT message 
    // and also could clear the message flag
    #ifdef USE_BOOTLOADER_LIB
      // Check if the child that polls is the node to be bootloaded.
      if(bootloadInProgressChildId == childId) {
        if (parentLaunchBootload) {
          sendBootloaderLaunchMessage(bootloadInProgressEui);
          parentLaunchBootload = FALSE;
          // Do not try to send any other JIT message when starting 
          // bootload process.
          return;
        } else if (blState == BOOTLOAD_STATE_DELAY_BEFORE_START) {
          // We have received the authentication challenge.
          bootloadUtilSendAuthResponse(bootloadInProgressEui);
          emberClearMessageFlag(childId);
          // Do not try to send any other JIT message when starting 
          // bootload process.
          return;
        }
      }
      if(!IS_BOOTLOADING) {
        // take care of all the sending of JIT messages to the child
        appSendJitToChild(childId);	
      }
    #else
      appSendJitToChild(childId);	
    #endif
  }
}


// this function is called from jitMessageStatus to print the contents 
// of a JIT buffer. Useful in making sure the correct data is being
// copied into the JIT buffer.
void jitMessageStatusPrintMessageData(EmberMessageBuffer bufferToPrint)
{
  int8u i;
  if (bufferToPrint != EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL, "   data: ");
    for (i=0; i< emberMessageBufferLength(bufferToPrint); i++) {
      emberSerialPrintf(APP_SERIAL, "%x ",
                        emberGetLinkedBuffersByte(bufferToPrint, i));
    }
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }

}

// This is called to print the status of the JIT storage on a parent node
// (sink or line powered sensor)
void jitMessageStatus(void)
{
  halResetWatchdog();

  // bitmasks
  emberSerialPrintf(APP_SERIAL, "bitmask for Multicast_Hello message: %2x\r\n",
                    jitMaskMulticastHello);
  emberSerialWaitSend(APP_SERIAL);

  // mcast hello packet buffer
  emberSerialPrintf(APP_SERIAL, "packet buffer for Multicast_Hello: %x\r\n",
                    jitMessageMulticastHello);
  emberSerialWaitSend(APP_SERIAL);
  jitMessageStatusPrintMessageData(jitMessageMulticastHello);
}


// this is called to print the child table
void printChildTable(void)
{
  int8u i;
  EmberStatus status;
  EmberEUI64 eui;
  EmberNodeType type;

  for (i=0; i<emberMaxChildCount(); i++)
  {
    status = emberGetChildData(i, eui, &type);
    emberSerialPrintf(APP_SERIAL, "%x:", i);
    if (status == EMBER_SUCCESS)
    {
      printEUI64(APP_SERIAL, (EmberEUI64*)eui);
      switch (type){
      case EMBER_SLEEPY_END_DEVICE:
        emberSerialPrintf(APP_SERIAL, " sleepy");
        break;
      case EMBER_MOBILE_END_DEVICE:
        emberSerialPrintf(APP_SERIAL, " mobile");
        break;
      case EMBER_END_DEVICE:
        emberSerialPrintf(APP_SERIAL, " non-sleepy");
        break;
      default:
        emberSerialPrintf(APP_SERIAL, " ??????");
        break;
      }
    }
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
}


void emberChildJoinHandler(int8u index, boolean joining)
{
  EmberStatus status;
  EmberEUI64 eui;
  EmberNodeType type;

  // if the node is joining...
  if (joining == TRUE)
  {
    status = emberGetChildData(index, eui, &type);
    // ...and the type is mobile...
    if ((status == EMBER_SUCCESS) &&
        (type == EMBER_MOBILE_END_DEVICE))
    {
      // ...set the message flag so it gets any messages right
      // away, since the node will not be in the child table
      // (and not have it's message flag set) whenever it
      // hibernates
      emberSetMessageFlag(emberChildId(index));
    }
  }
}
#endif // defined(SENSOR_APP) || defined(SINK_APP)


// **************************************************
// utility calls to support the EM250 Bootloader
//
// **************************************************
#ifdef USE_BOOTLOADER_LIB

// this is called by the sensor or sink -- applications that act
// as parents. This is called with the EUI of the child that the
// parents wants to bootload. Since the sleepy device may not be
// awake (likely not), this function sets the message flag of the
// child and remembers which child needs to be bootloaded. When the
// child wakes up and polls, the initial bootloader launch message 
// is sent -- see emberPollHandler
void bootloadMySleepyChild(EmberEUI64 targetEui)
{
  int8u index;
  
  // message for the user
  emberSerialPrintf(APP_SERIAL, "INFO: attempt child bootload, eui ");
  printEUI64(APP_SERIAL, (EmberEUI64*)targetEui);
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);
 
  // make sure the EUI is a child of this parent
  if (!(isMyChild(targetEui, &index))) {
	return;
  }
  
  // set the message flag for the child
  emberSetMessageFlag(emberChildId(index));

  // remember the EUI and shortId for the child we are bootloading
  bootloadInProgressChildId = emberChildId(index);
  MEMCOPY(bootloadInProgressEui, targetEui, EUI64_SIZE);
  
  // the bootload util library tells us if a bootload is in progres
  // with the IS_BOOTLOADING macro. We need to set a flag to send the
  // bootload launch message. Only want to do this once.
  parentLaunchBootload = TRUE;

  // print a message explaining that passthru is initiating 
  // and there is a few seconds before the xmodem ('C' characters
  // printing) starts.
  emberSerialPrintf(APP_SERIAL, 
                   "If a bootload response is seen, xmodem ");
  emberSerialPrintf(APP_SERIAL, "will start in 30 seconds\r\n");
  emberSerialPrintf(APP_SERIAL, 
                    "If xmodem does not start, the bootload failed\r\n");
}


// called by a sensor or sink when bootloading a neighbor device is
// desired. This function just prints a bunch of information for the
// user's benefit and calls a function that sends the initial bootload
// launch message.
void bootloadMyNeighborRouter(EmberEUI64 targetEui)
{ 
  // message for the user
  emberSerialPrintf(APP_SERIAL, "INFO: attempt neighbor bootload, eui ");	
  printEUI64(APP_SERIAL, (EmberEUI64*)targetEui);
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  // print a message explaining that passthru is initiating 
  // and there is a few seconds before the xmodem ('C' characters
  // printing) starts.
  emberSerialPrintf(APP_SERIAL, 
                   "If a bootload response is seen, xmodem ");
  emberSerialPrintf(APP_SERIAL, "will start in 10 seconds\r\n");
  emberSerialPrintf(APP_SERIAL, 
                    "If xmodem does not start, the bootload failed\r\n");
  
  sendBootloaderLaunchMessage(targetEui);
}


// does the sending of the bootloader launch message. This is really
// just setting up the parameters and making a call to bootloadUtilSendRequest
void sendBootloaderLaunchMessage(EmberEUI64 targetEui)
{
  // encryptKey has to match the token TOKEN_MFG_BOOTLOADER_AES_KEY 
  // on the target device or the bootload will not start
  int8u encryptKey[16] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};

  // mfgId and hardwareTag have to match what the target device is
  // checking for in bootloadUtilRequestHandler. These values are meant
  // to be stored in the manufacturing tokens TOKEN_MFG_MANUF_ID (mfgId)
  // and TOKEN_MFG_BOARD_NAME (hardwareTag).
  int16u mfgId = 0xE250;
  int8u hardwareTag[8] = {'D', 'E', 'V', ' ', '0', '4', '5', '5'};
  
  bootloadUtilSendRequest(targetEui,
                          mfgId,
                          hardwareTag,
                          encryptKey,
                          BOOTLOAD_MODE_PASSTHRU); 	
}

// determines if the EUI passed in is in this device's child table
// return of true means the EUI is in the child table
// return of false means the EUI is not in the child table
// if the EUI is in the child table, then childIndex is set to the
// index in the child table of the EUI.
boolean isMyChild(EmberEUI64 candidateEui, int8u* childIndex)
{
  int8u i;
  EmberEUI64 childEui;
  EmberNodeType type;
  EmberStatus status;
  
  // go through whole child table  
  for (i=0; i<emberMaxChildCount(); i++) {
    status = emberGetChildData(i, childEui, &type);
	if ((status == EMBER_SUCCESS) && 
		(MEMCOMPARE(candidateEui, childEui, EUI64_SIZE)==0)) {
      // we found a match
      (*childIndex) = i;
      return TRUE;
    }
  }
  // didn't match any entry in my child table
  return FALSE;	
}

// no check for now. If an attempt is made to bootload a device 
// that is not a neighbor the initial launch will fail.
boolean isMyNeighbor(EmberEUI64 eui)
{
  return TRUE;		
}

#endif //USE_BOOTLOADER_LIB

// Do a formatted conversion of a signed fixed point 32 bit number
// to decimal.  Make lots of assumtions.  Like minDig > dot;
// minDig < 20; etc.

void formatFixed(int8u* charBuffer, int32s value, int8u minDig, int8u dot, boolean showPlus)
{
  int8u numBuff[20];
  int8u i = 0;
  int32u calcValue = (int32u)value;

  if (value < 0)
  {
    calcValue = (int32u)-value;
    *charBuffer++ = '-';
  }
  else if (showPlus)
    *charBuffer++ = '+';

  while((i < minDig) || calcValue) {
    numBuff[i++] = (int8u)(calcValue % 10);
    calcValue /= 10;
  }
  while(i--) {
    *charBuffer++ = (numBuff[i] + '0');
    if (i == dot)
      *charBuffer++ = '.';
  }
  *charBuffer = 0;
}

