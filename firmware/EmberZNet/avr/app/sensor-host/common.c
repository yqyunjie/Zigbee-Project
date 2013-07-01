// *******************************************************************
//  common.c
//
//  common functions for sensor sample app
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor-host/common.h"
#include "app/util/security/security.h"

#if defined(SINK_APP)
extern int16u ticksSinceLastHeard[];
#endif


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
  emberSerialPrintf(APP_SERIAL, "\r\n");
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

#if defined(SINK_APP)

  // the sink is the security trust center. The trust center needs to
  // set the network security key. Other devices besides the trust center 
  // do not need to set the network security key.
  EmberKeyData networkKey =
    {'e','m','b','e','r',' ','E','M','2','5','0',' ','c','h','i','p'};

  // this call is located in app/util/security/trust-center.c. This 
  // call sets the preconfigured link key, the network key, sets up 
  // the correct EmberInitialSecurityState bitmask and sets the 
  // networkKeySequenceNumber to 0.
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
#if defined(SINK_APP)
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
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;
  ezspGetNetworkParameters(&nodeType, &parameters);

#if defined(GATEWAY_APP)
  emberSerialPrintf(APP_SERIAL, "gateway ");
#elif defined(SINK_APP)
  emberSerialPrintf(APP_SERIAL, "sink ");
#else
  emberSerialPrintf(APP_SERIAL, "sensor ");
#endif
  emberSerialPrintf(APP_SERIAL, "eui [");
  printEUI64(APP_SERIAL, (EmberEUI64*)emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "] short ID [%2x]\r\n", emberGetNodeId());
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL,
                    "channel [0x%x], power [0x%x], panId [0x%2x]\r\n",
                    parameters.radioChannel,
                    parameters.radioTxPower,
                    parameters.panId);
  printExtendedPanId(APP_SERIAL, parameters.extendedPanId);

  emberSerialPrintf(APP_SERIAL, ", stack [%2x], app [2.60]\r\n",
                    ezspUtilStackVersion);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "security level [%x]\r\n",
                    EMBER_SECURITY_LEVEL);
  emberSerialWaitSend(APP_SERIAL);
}


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
#if defined(SINK_APP)
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

#if defined(SINK_APP)
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
    EmberStatus status;
    EmberMulticastTableEntry multicastTableEntry;
    int8u indexLow;
    int8u indexHigh;

    emberSerialPrintf(APP_SERIAL, "Index  MulticastId  Endpoint\r\n");
    emberSerialWaitSend(APP_SERIAL);

    for (i=0; i<tableSize; i++) {
      indexLow = (i % 10) + 48;
      indexHigh = ((i - (i % 10))/10) + 48;
      emberSerialPrintf(APP_SERIAL, " %c%c:   ",
                        indexHigh, indexLow);

      status = ezspGetMulticastTableEntry(i, &multicastTableEntry);
      if (EMBER_SUCCESS == status) {
        emberSerialPrintf(APP_SERIAL, "  0x%2x     ", 
                          multicastTableEntry.multicastId);
        emberSerialPrintf(APP_SERIAL, "  0x%x\r\n", 
                          multicastTableEntry.endpoint);
      } else {
        emberSerialPrintf(APP_SERIAL, " --- ERROR ---\r\n ");
      }
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

#if defined(SENSOR_APP) || defined(SINK_APP)

// *********************************************************************
// The following section has to do with sending APS messages to sleeping
// children. This code is needed by devices that act as parents

// Called when the parent receives a MSG_SINK_QUERY. The parent will
// respond with a MSG_SINK_ADVERTISE
void handleSinkQuery(EmberNodeId childId)
{
  EmberApsFrame apsFrame;

  if (mainSinkFound == TRUE) {
    EmberStatus status;

    // the data - sink long address (EUI), sink short address
    MEMCOPY(&(globalBuffer[0]), sinkEUI, EUI64_SIZE);
    emberStoreLowHighInt16u(&(globalBuffer[EUI64_SIZE]), sinkShortAddress);

    // all of the defined values below are from app/sensor-host/common.h
    // with the exception of the options from stack/include/ember.h
    apsFrame.profileId = PROFILE_ID;          // profile unique to this app
    apsFrame.clusterId = MSG_SINK_ADVERTISE;
    apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
    apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
    apsFrame.options = EMBER_APS_OPTION_RETRY;

    // send the message
    status = ezspSendUnicast(EMBER_OUTGOING_DIRECT,
                             childId, 
                             &apsFrame,
                             MSG_SINK_ADVERTISE,
                             (EUI64_SIZE + 2),
                             globalBuffer,
                             &apsFrame.sequence);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, 
                        "ERROR: send unicast, child %2x, status %x\r\n",
                        childId, status);
      emberSerialWaitSend(APP_SERIAL);
    }
  }
}

// this is called to print the child table
void printChildTable(void)
{
  int8u i;
  EmberStatus status;
  EmberNodeId id;
  EmberEUI64 eui;
  EmberNodeType type;

  emberSerialPrintf(APP_SERIAL, "index eui              id   type\r\n");
  for (i=0; i<EMBER_MAX_END_DEVICE_CHILDREN; i++)
  {
    status = ezspGetChildData(i, &id, eui, &type);
    emberSerialPrintf(APP_SERIAL, " %x:  ", i);
    if (status == EMBER_SUCCESS)
    {
      printEUI64(APP_SERIAL, (EmberEUI64*)eui);
      emberSerialPrintf(APP_SERIAL, " %2x", id);
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

#endif // defined(SENSOR_APP) || defined(SINK_APP)

/******************************************************************************
// Print the Keys
*******************************************************************************/
void sensorCommonPrintKeys(void) {
  EmberKeyStruct keyStruct;
  EmberKeyType keyType = EMBER_TRUST_CENTER_LINK_KEY;
  EmberNetworkStatus networkStatus; 
  int8u keyTypesMax = 2;
  int8u i;
  
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

      //change key type
      keyType = EMBER_CURRENT_NETWORK_KEY;
    } 
  } else {
    emberSerialPrintf(APP_SERIAL,
      "INFO : not a valid network state to query the keys \r\n ");
  }
}


EmberStatus emberGenerateRandomKey(EmberKeyData* keyAddress)
{
  int8u i;
  int8u* keyBytes = keyAddress->contents;

  for (i=0; i<4; i++) {
    int16u randomNum = halCommonGetRandom();
    keyBytes[i*2] = HIGH_BYTE(randomNum);
    keyBytes[(i*2)+1] = LOW_BYTE(randomNum);
  }

  emberSerialPrintf(APP_SERIAL, "NEW KEY: ");
  sensorCommonPrint16ByteKey(keyBytes);

  return EMBER_SUCCESS;
}
