// *******************************************************************
//  SP-common.c
//
//  common functions for super parent sample app
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************
#include "app/super-parent/sp-common.h"

// structure to store necessary network parameters of the node
// (which are panId, enableRelay, radioTxPower, and radioChannel)
extern EmberNetworkParameters networkParams;

// *********************************
// utility function, used below in printing the security keys that are set
// *********************************
void commonPrint16ByteKey(int8u* key) 
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
// application, all variants of this application (SP-GW, SP-APP and SP-ED) 
// need to use the same security setup.
// *********************************
void setupSecurity(void)
{
  EmberStatus status;

  // this application chooses to use a preconfigured link key. This means
  // the link key is set on each device.
  EmberKeyData preconfiguredLinkKey =
    {'S','u','p','e','r','P','a','r','e','n','t','L','i','n','k','K'};

#ifdef GATEWAY_APP
  // the gateway is the security trust center. The trust center needs to
  // set the network security key. Other devices besides the trust center 
  // do not need to set the network security key.
  EmberKeyData networkKey =
    {'S','u','p','e','r','P','a','r','e','n','t','N','e','t','w','K'};

  // this call is located in app/util/security/trust-center.c. This 
  // call sets the preconfigured link key, the network key, sets up 
  // the correct EmberInitialSecurityState bitmask and sets the 
  // networkKeySequenceNumber to 0.
  status = trustCenterInit(&preconfiguredLinkKey, &networkKey);
 
#else 
  // setup the secutity on non-trust center devices. 
  // this call is located in app/util/security/node.c
  status = nodeSecurityInit(&preconfiguredLinkKey);
#endif // GATEWAY_APP

  // print error if necessary
  if (!status) {
    emberSerialPrintf(APP_SERIAL, "ERROR: initializing security\r\n", 
                      status);
  } else {
    // if no error, print the keys that were set. Here we print the keys
    // that we passed in, we don't use emberGetKey since this only
    // returns the correct key if a device is joined.
    emberSerialPrintf(APP_SERIAL, "INFO : link key set to: ");
    commonPrint16ByteKey(preconfiguredLinkKey.contents);
#ifdef GATEWAY_APP
    emberSerialPrintf(APP_SERIAL, "INFO : nwk key set to:  ");
    commonPrint16ByteKey(networkKey.contents);
#endif // GATEWAY_APP
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
// print info about this node
// *********************************
void infoCB(void) {
  EmberNetworkParameters nwkParams;
  
#ifdef GATEWAY_APP
  emberSerialPrintf(APP_SERIAL, "SP-GW ");
#endif
#ifdef ED_APP
  emberSerialPrintf(APP_SERIAL, "SP-ED ");
#endif
#ifdef PARENT_APP
  emberSerialPrintf(APP_SERIAL, "SP-PARENT ");
#endif

emberSerialPrintf(APP_SERIAL, "eui [");
#ifdef GATEWAY_APP
  EmberEUI64 eui64;
  EmberNodeType nodeType;
  ezspGetNetworkParameters(&nodeType, &nwkParams);
  ezspGetEui64(eui64);
  printEUI64(APP_SERIAL, (EmberEUI64*)eui64);
#else
  emberGetNetworkParameters(&nwkParams);
  printEUI64(APP_SERIAL, (EmberEUI64*)emberGetEui64());
#endif

  emberSerialPrintf(APP_SERIAL, "] short ID [%2x]\r\n", emberGetNodeId());
  emberSerialWaitSend(APP_SERIAL);

  if (emberNetworkState() == EMBER_JOINED_NETWORK) {
    emberSerialPrintf(APP_SERIAL,
                    "channel [0x%x], power [0x%x], panId [0x%2x]\r\n",
                    nwkParams.radioChannel, nwkParams.radioTxPower,
                    nwkParams.panId);
    printExtendedPanId(APP_SERIAL, nwkParams.extendedPanId);
  } else {
    emberSerialPrintf(APP_SERIAL,
                    "channel [N/A], power [N/A], panId [N/A] \r\n");
    emberSerialPrintf(APP_SERIAL, "ExtendedPanId [N/A]");
  }
#ifdef GATEWAY_APP  
  emberSerialPrintf(APP_SERIAL, ", stack [%2x], app [1.0]\r\n",
                    ezspUtilStackVersion);
#else
  emberSerialPrintf(APP_SERIAL, ", stack [%2x], app [1.0]\r\n",
                    SOFTWARE_VERSION);
#endif
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "security level [%x]\r\n",
                    EMBER_SECURITY_LEVEL);
  emberSerialWaitSend(APP_SERIAL);
}

/******************************************************************************
// Print the Keys
*******************************************************************************/
void printSecurityCB(void) {
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
      commonPrint16ByteKey(emberKeyContents(&keyStruct.key));
      emberSerialPrintf(APP_SERIAL, "sequenceNumber: %d\r\n",keyStruct.sequenceNumber);
      emberSerialWaitSend(APP_SERIAL);
      
      emberSerialPrintf(APP_SERIAL, "incomingFrameCounter: 0x%4x\r\n",keyStruct.incomingFrameCounter);
      emberSerialWaitSend(APP_SERIAL);
      
      emberSerialPrintf(APP_SERIAL, "outgoingFrameCounter: 0x%4x\r\n",keyStruct.outgoingFrameCounter);
      emberSerialWaitSend(APP_SERIAL);
                  
      //change key type
      keyType = EMBER_CURRENT_NETWORK_KEY;
    } 
  } else {
    emberSerialPrintf(APP_SERIAL,
      "INFO : not a valid network state to query the keys \r\n ");
  }
}

// ********************************
// to print address table
// ********************************
void addrTableCB(void) {
    int8u i;
    EmberEUI64 remoteEui64;
    EmberNodeId remoteNodeId;
    int8u indexLow;
    int8u indexHigh;

    emberSerialPrintf(APP_SERIAL, "Index  Inuse  NodeId     Eui64");
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);

    for (i=0; i<EMBER_ADDRESS_TABLE_SIZE; i++) {
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
      emberSerialPrintf(APP_SERIAL, "  %x %x %x %x %x %x %x %x",
                        remoteEui64[0], remoteEui64[1], remoteEui64[2],
                        remoteEui64[3], remoteEui64[4], remoteEui64[5],
                        remoteEui64[6], remoteEui64[7]);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      emberSerialWaitSend(APP_SERIAL);
    }
}

void initCB(void)
{
  EmberStatus status = emberNetworkInit();
  emberSerialPrintf(APP_SERIAL, "Network init status %X\r\n", status);
  switch (status) {
  case EMBER_SUCCESS:
    break;
  case EMBER_NOT_JOINED:
    emberSerialPrintf(APP_SERIAL, "Node currently not joined\r\n");
    break;
  case EMBER_INVALID_CALL:
    // The status is returned if called when network state is not in the
    // initial state or if scan is pending
    emberSerialPrintf(APP_SERIAL, "Invalid call of NetworkInit()\r\n");
    break;
  default:
    // Something is wrong and we cannot proceed.  Indicate user and terminate.
    emberSerialPrintf(APP_SERIAL,
                                "ERROR: network initialization error 0x%x\r\n",
                                status);
    quit();
  }
  emberSerialWaitSend(APP_SERIAL);
}

void setChannelCB(void)
{
  int8u channel;
  channel = LOW_BYTE(cliGetInt16uFromArgument(1));
  networkParams.radioChannel = channel;
  emberSerialPrintf(APP_SERIAL, "Channel set to %d\r\n", channel);
}

void setPanIDCB(void)
{
  int16u panID;

  panID = (cliGetHexByteFromArgument(0, 4) * 256) + 
      (cliGetHexByteFromArgument(1, 4));
  networkParams.panId = panID;
  emberSerialPrintf(APP_SERIAL, "Pan ID set to 0x%2x\r\n", panID);
}

void setPowerCB(void)
{
  int8u power;

  power = LOW_BYTE(cliGetInt16uFromArgument(1));
  networkParams.radioTxPower = power;
  emberSerialPrintf(APP_SERIAL, "Power set to %d\r\n", power);
}

void rebootCB(void)
{
#ifdef GATEWAY_APP
  //reset em260
  ezspReset();
#else
  halReboot();
#endif
}

void leaveCB(void) 
{
  emberSerialPrintf(APP_SERIAL, "leave 0x%x\r\n\r\n", emberLeaveNetwork());
}

#if defined(PARENT_APP) || defined(GATEWAY_APP)
void joinCB(void)
{
  // if not joined with a network, join
  switch (emberNetworkState()) {
    case EMBER_NO_NETWORK:
      emberSerialPrintf(APP_SERIAL, "Attempt to join network\r\n");
      emberSerialWaitSend(APP_SERIAL);

      // Set the security keys and the security state - specific to this 
      // application.  This function is in app/super-parent/sp-common.c. This 
      // function should only be called when a network is formed as the act of 
      // setting the key also sets the frame counters to 0. On reset and 
      // networkInit, this should not be called.
      setupSecurity();

      #ifdef USE_HARDCODED_NETWORK_SETTINGS
      {
        EmberStatus status;
        // tell the user what is going on
        emberSerialPrintf(APP_SERIAL,
               "SP-APP: joining network - channel 0x%x, panid 0x%2x, ",
               networkParams.radioChannel, networkParams.panId);
        printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
        emberSerialPrintf(APP_SERIAL, "\r\n");
        emberSerialWaitSend(APP_SERIAL);

        // attempt to join the network
        status = emberJoinNetwork(EMBER_ROUTER, 
                                    &networkParams); 
        if (status != EMBER_SUCCESS) {
          emberSerialPrintf(APP_SERIAL,
            "error returned from emberJoinNetwork: 0x%x\r\n", status);
        } else {
          emberSerialPrintf(APP_SERIAL, "waiting for stack up...\r\n");
        }
      }

      // the else case means we are NOT using hardcoded settings and are
      // picking a random PAN ID and channel and either using
      // APP_EXTENDED_PANID (from app/super-parent/sp-common.h) for the 
      // extended PAN ID or picking a random one if APP_EXTENDED_PANID
      // is "0".
      #else
      {
        int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
        // tell the user what is going on
        emberSerialPrintf(APP_SERIAL,
                            "SP-APP: scanning for channel and panid\r\n");

        // Use a function from app/util/common/form-and-join.c
        // that scans and selects a beacon that has:
        // 1) allow join=TRUE
        // 2) matches the stack profile that the app is using
        // 3) matches the extended PAN ID passed in unless "0" is passed
        // Once a beacon match is found, emberJoinableNetworkFound is called.
        emberScanForJoinableNetwork(EMBER_ALL_802_15_4_CHANNELS_MASK,
                                    (int8u*) extendedPanId);
      }
      #endif // USE_HARDCODED_NETWORK_SETTINGS
      break;

    // if in the middle of joining, do nothing
    case EMBER_JOINING_NETWORK:
      emberSerialPrintf(APP_SERIAL, 
                        "App already trying to join\r\n");
      break;

    // if already joined, do nothing
    case EMBER_JOINED_NETWORK:
      // turn allow join on
      emberPermitJoining(SP_PERMIT_JOIN_SEC);
      emberSerialPrintf(APP_SERIAL, 
                "App already joined, turn on permit join for %d seconds\r\n",
                SP_PERMIT_JOIN_SEC);
      break;

    // if leaving, do nothing
    case EMBER_LEAVING_NETWORK:
      emberSerialPrintf(APP_SERIAL, "App leaving, no action\r\n");
      break;

    case EMBER_JOINED_NETWORK_NO_PARENT:
      emberSerialPrintf(APP_SERIAL, 
      "EMBER_JOINED_NETWORK_NO_PARENT, App will leave\r\n");
      emberLeaveNetwork();
      break;
    }
}

// display neighbor table using utility function defined in 
// app/util/common/print-stack-tables.h
void neighborCB(void)
{
#ifdef GATEWAY_APP
  EmberNeighborTableEntry n;
  int8u i;

  for (i = 0; i < emberNeighborCount(); i++) {
    emberGetNeighbor(i, &n);
    emberSerialPrintf(APP_SERIAL, 
            "id:%2X lqi:%d in:%d out:%d age:%d eui:(>)%X%X%X%X%X%X%X%X\r\n",
                      n.shortId,
                      n.averageLqi,
                      n.inCost,
                      n.outCost,
                      n.age,
                      n.longId[7], n.longId[6], n.longId[5], n.longId[4], 
                      n.longId[3], n.longId[2], n.longId[1], n.longId[0]);
    emberSerialWaitSend(APP_SERIAL);
  }
  if (emberNeighborCount() == 0) {
    emberSerialPrintf(APP_SERIAL, "empty neighbor table\r\n");
  }
#else
  printNeighborTable(APP_SERIAL);
#endif
}
#endif

#ifdef PARENT_APP
// this is called to print the child table
void childCB(void) 
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
      default:
        emberSerialPrintf(APP_SERIAL, " invalidType, 0x%x", type);
        break;
      }
    }
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
}

void jitQCB(void)
{
  spParentUtilPrintJITQ();
}
#endif

// shared utility functions
// *********************************
void quit(void) {
  halReboot();
}

// Common stack callback functions
// *********************************
void emberSwitchNetworkKeyHandler(int8u sequenceNumber)
{
  emberSerialPrintf(APP_SERIAL, 
    "[EVENT] Switch to new network key, sequence 0x%x\r\n", sequenceNumber); 
}
