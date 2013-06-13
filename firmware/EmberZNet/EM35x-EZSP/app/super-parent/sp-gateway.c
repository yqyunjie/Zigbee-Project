// *******************************************************************
//  sp-gateway.c
//
//  sample app for Ember Stack API
//
//  This is an example of a super parent's gateway application.
//  
//  In this application, gateway contains information regarding all the end
//  devices in the network.  In order for each super parent to support more than
//  its normal maximum of 32 end devices, it offloads the storage burden to the
//  gateway node.  Super parent will have the ability to add and remove end
//  device from its local child table as needed.  While the gateway node will
//  maintain all the information regarding all the end devices in the network.
//
//  Note that gateway node cannot be super parent since it runs on EM260 
//  platform and hence lacks the ability to send Just In Time (JIT) message.  
//  Gateway is also a concentrator and a trust center.  Communication between 
//  gateway and other routers are done via source routing.
//
//  Parent node is used as a communication proxy between gateway and device.
//  Gateway sends messages directly to parent node when it needs to communicate
//  with the end device, and vice versa.  This is because in order to support
//  large number of end devices in the network (for example, 200 end devices
//  per each parent), end device's (or children's) information cannot be stored
//  on the parent node since it simply does not have the resource.  The 
//  information is in fact stored on the gateway node instead.  Gateway node is
//  assumed to have storage space large enough to contain information of all
//  the end devices in the network.
//
//  Every messages sent from end devices is forwarded by the parent to the 
//  gateway node.  Upon receiving these messages, the database module on the
//  gateway node will update its database accordingly.
//
//  This example also provides some simple commands that can be sent to the
//  node via the serial port. A brief explanation of the commands is
//  provided below:
//  Common serial commands:
//  help: Prints all commands with short description
//  info: Prints EUI, shortID, channel, radio power, short PAN ID, 
//  extended PAN ID, stack version, app version, and security level
//  security: Prints security parameters including preconfigured link key, 
//  network key and their corresponding frame counters.
//  reboot: Resets device
//  leave: Leave network
//  init: Initialize network stack after a reset
//
//  Serial commands on SP-GW module:
//  form: Perform energy and active scans, and form the network
//  query: Issue on-demand query command for certain end device
//  database: Print database on the gateway node.
//  key: Generate new random network key and cause key switching.
//  broadcast: Send broadcast to all devices in the network (broadcast is
//  forwarded to end devices by super parents)
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************

#include "app/super-parent/sp-common.h"

#include "app/util/gateway/gateway.h"

// *******************************************************************
// Define the concentrator type for use when sending "many-to-one"
// route requests.
//
// EMBER_HIGH_RAM_CONCENTRATOR is used when the concentrator has enough
// memory to store source routes for the whole network.

int16u concentratorType = EMBER_HIGH_RAM_CONCENTRATOR;

// *******************************************************************
// Ember endpoint and interface configuration

int8u emberEndpointCount = 1;
EmberEndpointDescription PGM endpointDescription = { PROFILE_ID, 0, };
EmberEndpoint emberEndpoints[] = {
  { ENDPOINT, &endpointDescription },
};

// End Ember endpoint and interface configuration
// *******************************************************************

// *******************************************************************

// buffer for organizing data before we send a message
EmberStatus status;
// global array stored outgoing and incoming messages
int8u globalBuffer[100];
int8u globalBufferLength = 0;
// child table entry used in the database
spChildTableEntry globalEntry;
  
// Application timing parameters.  They are converted to quarter second unit
// since that is the unit applicationTick() is running at.

// wait 30 seconds before starting periodic query process.
int32u timeBeforeQueryProcess = 30 * 4; 
// time before requesting next end device's information to be queried.
int32u timeBeforeNextQuery = 0;
// total number of query sent in the current process.
int32u querySent = 0;
// indicate whether it is time to send MTORR
boolean sendMTORR = TRUE;
// time before broadcasting network key switch message.
int32u timeBeforeSwitchKey = 0xFFFFFFFF;
// structure to store necessary network parameters of the node
// (which are panId, enableRelay, radioTxPower, and radioChannel)
EmberNetworkParameters networkParams;
// indicate network state.  The value is set inside ezspStackStatusHandler()
EmberNetworkStatus networkState = EMBER_NO_NETWORK;
// Sequence number for broadcast message
int8u broadcastSequence = 0;
// Keep track of broadcast to end devices period.  We halt the periodic query
// process while broadcasting to end devices.
int32u broadcastTimeout = 0;

// End application specific constants and globals
// *******************************************************************

// *******************************************************************
// Forward declarations
static void applicationTick(void);
void spGatewaySendMessage(int8u msgType);
void spGatewaySendBroadcastToEnddevices(void);

// these are defines used for the variable networkFormMethod. This variable
// affects the serial output when a network is formed. 
#define GW_FORM_NEW_NETWORK 1
#define GW_USE_NETWORK_INIT 2
#define GW_USE_SCAN_UTILS   3
int8u networkFormMethod = GW_FORM_NEW_NETWORK;

//
// *******************************************************************
// End device application specific command
// Provide an error handler for the command interpreter
// See app/util/serial/cli.h for documentation.
// storage for command arguments
cliSerialCmdEntry cliCmdList[] = {
  SP_GW_COMMANDS
  SP_APP_COMMANDS
  SP_COMMON_COMMANDS
};

int8u cliCmdListLen = sizeof(cliCmdList)/sizeof(cliSerialCmdEntry);
PGM_P cliPrompt = "SP-GW";

void getHelpCB(void)
{
  int8u i;
  for (i=0; i<cliCmdListLen; i++) {
    emberSerialPrintf(APP_SERIAL, "%p\r\n", cliCmdList[i].cmd);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

void formCB(void) 
{
  // Set the security keys and the security state - specific to this 
  // application.  This function is in app/super-parent/sp-common.c. 
  // This function should only be called when a network is formed as the 
  // act of setting the key sets the frame counters to 0. On reset and 
  // networkInit this should not be called.
  setupSecurity();

  // Bring the network up.
  #ifdef USE_HARDCODED_NETWORK_SETTINGS
    // set the mode we are using
    networkFormMethod = GW_FORM_NEW_NETWORK;
    // tell the user what is going on
    emberSerialPrintf(APP_SERIAL,
         "FORM : attempting to form network\r\n");
    emberSerialPrintf(APP_SERIAL, "     : using channel 0x%x, panid 0x%2x, ",
         networkParams.radioChannel, networkParams.panId);
    printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);

    // attempt to form the network
    status = emberFormNetwork(&networkParams);
    if (status != EMBER_SUCCESS) {
      emberSerialGuaranteedPrintf(APP_SERIAL,
        "ERROR: from emberFormNetwork: 0x%x\r\n",
        status);
      quit();
    }
  #else
  {
    int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
    // set the mode we are using
    networkFormMethod = GW_USE_SCAN_UTILS;
    // tell the user what is going on
    emberSerialPrintf(APP_SERIAL,
         "FORM : scanning for an available channel and panid\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberScanForUnusedPanId(EMBER_ALL_802_15_4_CHANNELS_MASK, 5); // 507 ms duration
  }
  #endif
}

// Send on-demand query for end device using EUI64.
void queryCB(void)
{
  EmberEUI64 targetEui;
  int8u i, index = 7;
  for(i=0; i<EUI64_SIZE; ++i) {
    targetEui[index-i] = cliGetHexByteFromArgument(i, 1);
  }
  if(spDatabaseUtilgetEnddeviceInfoViaEui64(&globalEntry, targetEui) == TRUE) {
    spGatewaySendMessage(SP_QUERY_MSG);     
  } else {
    emberSerialPrintf(APP_SERIAL, "ERROR: cannot find node ");
    printEUI64(APP_SERIAL, (EmberEUI64*)targetEui);
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
}

void databaseCB(void)
{
  // display database (end device table)
  spDatabaseUtilPrint();
}

// Send a broadcast message containing newly generated network key to nodes
// in the network.  Then wait SP_SWITCH_NETWORK_KEY_SEC to send out another 
// message to notify the nodes to switch to using the new network key.
void keyCB(void)
{
  EmberKeyData key;
  int8u i;
  // generate new random network key
  for(i=0; i<EMBER_ENCRYPTION_KEY_SIZE; ++i) {
    key.contents[i] = halCommonGetRandom()&0xFF; 
  }
  // broadcast new network key to the network
  status = emberBroadcastNextNetworkKey(&key);
  if(status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
      "Error: cannot send new network key, status 0x%x\r\n", status); 
  }
  timeBeforeSwitchKey = SP_SWITCH_NETWORK_KEY_SEC*4; // in qs
  emberSerialPrintf(APP_SERIAL, "[EVENT]: update network key\r\n");
}

// Send application broadcast message to end devices in the network.  The 
// message consists of one byte message type, one byte sequence number and a
// payload.
void broadcastCB(void)
{
  // send broadcast message to end devices
  spGatewaySendBroadcastToEnddevices();
  // start broadcast timeout.  we halt periodic query process while broadcasting
  // to end devices.
  broadcastTimeout = (SP_BCAST_DURATION_SEC + 10) * 4; // qs
}

// *******************************************************************
// Begin main application loop
int main(int argc, char *argv[])
{
  EmberNodeType nodeType;
  EzspStatus ezspStat;
  
  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  if (EMBER_SUCCESS != gatewayInit(argc, argv)) {
    return -1;
  }

  gatewayCliInit("SP-GW",   // prompt
                 cliCmdList,
                 cliCmdListLen);
  
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1) 
     != EMBER_SUCCESS) {
    return -1;
  }
  
  // print the reason for the reset
  emberSerialPrintf(APP_SERIAL, "RESET: %p\r\n", 
                              halGetResetString());
  //reset the EM260 and wait for it to come back
  //this will guarantee that the two MCUs start in the same state
  {
    EzspStatus stat = ezspInit();
    if(stat != EZSP_SUCCESS) {
      // report status here
      emberSerialPrintf(APP_SERIAL,
                "ERROR: ezspInit 0x%x\r\n", stat);
      // the app can choose what to do here, if the app is running
      // another device then it could stay running and report the
      // error visually for example. This app asserts.
      quit();
    }
  }
  
  // ezspUtilInit must be called before other EmberNet stack functions
  status = ezspUtilInit(APP_SERIAL);
  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialPrintf(APP_SERIAL,
              "ERROR: ezspUtilInit 0x%x\r\n", status);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
    quit();
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: ezspUtilInit passed\r\n");
  }

  // This configuration parameter sizes the number of address table 
  // entries that contain short to long address mappings needed during 
  // the key exchanges that happen immediately after a join.  It should 
  // be set to the maximum expected simultaneous joins.
  ezspStat = 
    ezspSetConfigurationValue(EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE, 
                              GW_TRUST_CENTER_ADDRESS_CACHE_SIZE);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
                      "ERROR: ezspSetConfigurationValue "
                      "EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE: 0x%x\r\n", 
                      ezspStat);
  }

  // This configuration parameter sizes the source route table that is 
  // kept on the ncp.  Its value must be big enough to hold a route that 
  // is equal to the maximum number of hops in the network.  
  ezspStat = ezspSetConfigurationValue(EZSP_CONFIG_SOURCE_ROUTE_TABLE_SIZE, 
                                       EMBER_SOURCE_ROUTE_TABLE_SIZE);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
                       "ERROR: ezspSetConfigurationValue "
                       "EZSP_CONFIG_SOURCE_ROUTE_TABLE_SIZE: 0x%x\r\n", 
                        ezspStat);
  }                         
 
  // print a startup message
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT : gateway app ");

  printEUI64(APP_SERIAL, (EmberEUI64*) emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "\r\n");
  
  // host needs to supply ack so that there is an opportunity to add 
  // the source route - set the policy before forming  
  ezspStat = ezspSetPolicy(EZSP_UNICAST_REPLIES_POLICY, 
                           EZSP_HOST_WILL_SUPPLY_REPLY);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
                 "ERROR: ezspSetPolicy EZSP_UNICAST_REPLIES_POLICY: 0x%x\r\n", 
                                ezspStat);
  }
  
  // set trust center policy to allow joining with preconfigured link key.  
  ezspStat = ezspSetPolicy(EZSP_TRUST_CENTER_POLICY, 
                           EZSP_ALLOW_PRECONFIGURED_KEY_JOINS);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
                 "ERROR: ezspSetPolicy EZSP_TRUST_CENTER_POLICY: 0x%x\r\n", 
                                ezspStat);
  }
  
  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  // gw node need to be coordinators, so ensure we are a coordinator
  status = ezspGetNetworkParameters(&nodeType, &networkParams);
  if ((status != EMBER_SUCCESS) ||
      (nodeType != EMBER_COORDINATOR) ||
      (emberNetworkInit() != EMBER_SUCCESS))
  {
    #ifdef USE_HARDCODED_NETWORK_SETTINGS
      // use the settings from app/super-parent/sp-common.h
      int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
      networkParams.panId = APP_PANID;
      networkParams.radioTxPower = APP_POWER;
      networkParams.radioChannel = APP_CHANNEL;
      MEMCOPY(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);
    #endif
    emberSerialPrintf(APP_SERIAL, "Issue 'form' cmd to form network\r\n");
  } 
  // initialize the cli library to use the correct serial port. APP_SERIAL 
  // is defined in the makefile or project-file depending on the platform.
  cliInit(APP_SERIAL);
  
  // event loop
  while(TRUE) {

    halResetWatchdog();
    applicationTick(); // check timeouts
    emberTick();
    cliProcessSerialInput();
  }
}

// end main application loop
// *******************************************************************

// *******************************************************************
// Begin Ember callback handlers
// Populate as needed
//

// Called when a message has completed transmission --
// status indicates whether the message was successfully
// transmitted or not.
void ezspMessageSentHandler(EmberOutgoingMessageType type,
                     int16u indexOrDestination,
                     EmberApsFrame *apsFrame,
                     int8u messageTag,
                     EmberStatus status,
                     int8u messageLength,
                     int8u *messageContents)
{

}


void ezspIncomingMessageHandler(EmberIncomingMessageType type,
                                EmberApsFrame *apsFrame,
                                int8u lastHopLqi,
                                int8s lastHopRssi,
                                EmberNodeId sender,
                                int8u bindingTableIndex,
                                int8u addressTableIndex,
                                int8u length,
                                int8u *message)
{
  int8u msgType;
  EmberStatus status;
  int8u relayCount;
  
  if(length == 0) {
    return;
  }
  // retrieve message type
  msgType = message[0];

  if(apsFrame->clusterId == PARENT_AND_GW_CLUSTER) {
    switch(msgType) {
      // messages received from parent node are already in the correct format.
      // Gateway node just have to forward it to the database.
      case SP_JOIN_MSG:
      case SP_REPORT_MSG:
      case SP_TIMEOUT_MSG:
        // forward message to database
        spDatabaseUtilForwardMessage(message, length);
        break;

      case SP_MTORR_RESP:
        //do nothing.  Just need to send APS ACK.
        break;
        
      case SP_BCAST_MSG:
        // receiving broadcast loopback, do nothing.
        return;   
        
      default:
        emberSerialPrintf(APP_SERIAL, 
          "[GW] RX unknown msg [%x] with PARENT_AND_GW_CLUSTER\r\n", 
          msgType);
    } // end switch
    //find the sender in the source route table in order to send APS ACK.
    if (emberFindSourceRoute(sender, &relayCount, (int16u *)globalBuffer)) {
      // if found, send the source route to the ncp
      status = ezspSetSourceRoute(sender, relayCount, (int16u *)globalBuffer);
      if (status != EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL, 
                          "ERROR: ezspSetSourceRoute: 0x%x\r\n", status); 
      }
    }
    
    // send the ack using the source route that was previously sent to 
    // the ncp - no payload
    status = ezspSendReply(sender, apsFrame, 0, NULL);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, 
                        "ERROR: ezspSendReply: 0x%x\r\n", status);
    }
  } else if(apsFrame->clusterId == 0x8038) {
    emberSerialPrintf(APP_SERIAL, 
          "[GW] RX Nwk_Update_Msg from %2x\r\n", sender);
  } else {
    emberSerialPrintf(APP_SERIAL, 
          "[GW] RX msg with unknown cluster id [%2x] from %2x\r\n", 
          apsFrame->clusterId, sender);
    emberSerialPrintf(APP_SERIAL, "profile %2x srcEnd %x destEnd %x options %2x\r\n", apsFrame->profileId,apsFrame->sourceEndpoint,apsFrame->destinationEndpoint,apsFrame->options);    
    
  }// end cluster check
}

// this is called when the stack status changes
void ezspStackStatusHandler(EmberStatus status)
{
  EmberNodeType nodeType;
  
  networkState = emberNetworkState();
  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");

    {
      status = ezspGetNetworkParameters(&nodeType, &networkParams);
      if (status == EMBER_SUCCESS) {
        switch (networkFormMethod) {
        case GW_USE_NETWORK_INIT:
          emberSerialPrintf(APP_SERIAL,
                            "FORM : network started using network init\r\n");
          break;
        case GW_FORM_NEW_NETWORK:
          emberSerialPrintf(APP_SERIAL,
                            "FORM : new network formed\r\n");
          break;
        case GW_USE_SCAN_UTILS:
          emberSerialPrintf(APP_SERIAL,
                            "FORM : new network formed by scanning\r\n");
          break;
        }
        emberSerialPrintf(APP_SERIAL,
             "     : channel 0x%x, panid 0x%2x, ",
             networkParams.radioChannel, networkParams.panId);
        printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
        emberSerialPrintf(APP_SERIAL, "\r\n");
        emberSerialWaitSend(APP_SERIAL);
      }
    }
    break;

  case EMBER_NETWORK_DOWN:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_DOWN\r\n");
    break;

  case EMBER_JOIN_FAILED:
    emberSerialPrintf(APP_SERIAL,
           "EVENT: stackStatus now EMBER_JOIN_FAILED\r\n");
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "EVENT: stackStatus now 0x%x\r\n", status);
  }
}

// called by form-and-join library
void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
  EmberNetworkParameters networkParams;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
  MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
  networkParams.radioChannel = channel;
  networkParams.panId = panId;
  networkParams.radioTxPower = APP_POWER;
  MEMCOPY(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);

  EmberStatus status = emberFormNetwork(&networkParams);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Form error 0x%x", status);
  }
}

// called by form-and-join library
void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{}

// called by form-and-join library
void emberScanErrorHandler(EmberStatus status)
{
  emberSerialPrintf(APP_SERIAL, "Scan error 0x%x", status);
}

void halNcpIsAwakeIsr(boolean isAwake)
{}

void ezspErrorHandler(EzspStatus status)
{
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              "ERROR: ezspErrorHandler 0x%x\r\n",
                              status);
  quit();
}

// end Ember callback handlers
// *******************************************************************


// *******************************************************************
// Functions that use EmberNet

// applicationTick - called to check application timeouts
static void applicationTick(void) {
  static int16u lastBlinkTime = 0;
  int16u time;
  
  time = halCommonGetInt16uMillisecondTick();

  
  // Application timers are based on quarter second intervals, where each 
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks. 
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in 
  // app/super-parent/sp-common.h.
  if ( (int16u)(time - lastBlinkTime) > TICKS_PER_QUARTER_SECOND ) {
    lastBlinkTime = time;

    // Perform following actions only if we are part of the network.
    if (networkState == EMBER_JOINED_NETWORK)
    {
      // ******************************************
      // MTORR: see if it is time to send many-to-one-route-discovery.  MTORR
      // is sent SP_MTORR_INTERVAL_BEFORE_QUERY_SEC seconds before query process 
      // start to allow each parent to send its route record to the gateway. 
      // ******************************************
      if ((sendMTORR) && 
        (timeBeforeQueryProcess < SP_MTORR_INTERVAL_BEFORE_QUERY_SEC*4)) {
        status = emberSendManyToOneRouteRequest(concentratorType,
                                                10);        // radius
        emberSerialPrintf(APP_SERIAL,
                          "[EVENT] GW send many-to-one route request,"
                          " status 0x%x\r\n", status);
        sendMTORR = FALSE;
      } 
      
      // ******************************************
      // Switch Key: switch to new network key
      // ******************************************
      // timeBeforeSwitchKey is valid when it is less than or equal to
      // SP_SWITCH_NETWORK_KEY_SEC, otherwise, the value is set to 0xFFFFFFFF
      // initially.
      if(timeBeforeSwitchKey <= (SP_SWITCH_NETWORK_KEY_SEC*4)) {
        if(timeBeforeSwitchKey == 0) {
          status = emberBroadcastNetworkKeySwitch();
          if(status != EMBER_SUCCESS) {
            emberSerialPrintf(APP_SERIAL, 
              "Error: cannot send switch key message, status 0x%x\r\n", status); 
          }
          timeBeforeSwitchKey = 0xFFFFFFFF;  
        } else {
          timeBeforeSwitchKey = timeBeforeSwitchKey - 1;
        }
      }

      // ******************************************
      // QUERY: perform periodic query of end devices
      // ******************************************
      if(timeBeforeQueryProcess == 0) {
        // start the query by retrieving end device's information from the
        // database.
        if(timeBeforeNextQuery == 0) {
          if(spDatabaseUtilgetEnddeviceInfo(&globalEntry) == TRUE) {
            spGatewaySendMessage(SP_QUERY_MSG);
            ++querySent;
            timeBeforeNextQuery = SP_GW_QUERY_RATE_SEC * 4;
          } else {
            // There is no more end device to be queried, so gateway waits until
            // query timeout.
            timeBeforeQueryProcess = 
              (SP_GW_QUERY_INTERVAL_SEC - (querySent*SP_GW_QUERY_RATE_SEC)) * 4;
            querySent = 0;
            sendMTORR = TRUE;
          }
        } else {
          // only decrementing the query time if we are not doing broadcast
          // to end devices.
          if(broadcastTimeout == 0) {
            timeBeforeNextQuery = timeBeforeNextQuery - 1;
          }
        }
      } else {
        // only decrementing the query time if we are not doing broadcast
        // to end devices.
        if(broadcastTimeout == 0) {
          timeBeforeQueryProcess = timeBeforeQueryProcess - 1;
        }
      }
      
      // ******************************************
      // Broadcast period: decrement the broadcast timeout
      // ******************************************
      if(broadcastTimeout > 0 ) {
        broadcastTimeout = broadcastTimeout - 1; 
      }
      
    } // end joined network check
  } // end quarter second event
} // end application tick

//
// *******************************************************************
// Utility functions
// *******************************************************************
// Function used to send radio messages.
void spGatewaySendMessage(int8u msgType)
{
  EmberStatus status;
  EmberApsFrame apsFrame;
  int8u relayCount;
  int8u len=0;

  //find the destination node in the source route table
  if (emberFindSourceRoute(globalEntry.parentId, &relayCount, 
    (int16u *)globalBuffer)) {
    // if found, send the source route to the ncp
    status = ezspSetSourceRoute(globalEntry.parentId, relayCount, 
    (int16u *)globalBuffer);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, 
                        "ERROR: ezspSetSourceRoute: 0x%x\r\n", status);
    }
  } else {
    emberSerialPrintf(APP_SERIAL, "ERROR: cannot find source route to %2x\r\n",
    globalEntry.parentId);
  }
  
  // Byte 0: msg type
  globalBuffer[len++] = msgType;

  switch(msgType) {
    case SP_QUERY_MSG:
      // copy the data into a packet buffer
      globalBuffer[len++] = LOW_BYTE(globalEntry.childId);
      globalBuffer[len++] = HIGH_BYTE(globalEntry.childId);
      MEMCOPY(&(globalBuffer[len]), globalEntry.childEui, 8);
      len = len + 8;
      break;

    default:
      emberSerialPrintf(APP_SERIAL, 
        "Error: invalid msg type, 0x%x\r\n", msgType);
      return;
  }
  
  // all of the defined values below are from app/super-parent/sp-common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = PARENT_AND_GW_CLUSTER;
  apsFrame.sourceEndpoint = ENDPOINT;       
  apsFrame.destinationEndpoint = ENDPOINT;  
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = ezspSendUnicast(EMBER_OUTGOING_DIRECT,
                            globalEntry.parentId,
                            &apsFrame,
                            0, //msgTag
                            len,
                            globalBuffer,
                            &(apsFrame.sequence));
  if(status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
      "Error: failed sending unicast, error 0x%x\r\n", status);                           
  }
}

// Send broadcast messages to all devices in the network.  Super parent nodes
// will handle forwarding the message to end devices.
void spGatewaySendBroadcastToEnddevices(void)
{
  EmberStatus status;
  EmberApsFrame apsFrame;
  int8u len=0;
  
  // Byte 0: msg type
  globalBuffer[len++] = SP_BCAST_MSG;
  // Byte 1: sequence number
  globalBuffer[len++] = broadcastSequence++;
  // Byte 2-6: payload of "hello"
  globalBuffer[len++] = 'H';
  globalBuffer[len++] = 'E';
  globalBuffer[len++] = 'L';
  globalBuffer[len++] = 'L';
  globalBuffer[len++] = 'O';

  // all of the defined values below are from app/super-parent/sp-common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = PARENT_AND_GW_CLUSTER;
  apsFrame.sourceEndpoint = ENDPOINT;       
  apsFrame.destinationEndpoint = ENDPOINT;  

  status = ezspSendBroadcast(EMBER_BROADCAST_ADDRESS, 
                             &apsFrame, 
                             6, // number of hops 
                             0, // msgTag
                             len, // size
                             globalBuffer, 
                             &(apsFrame.sequence));
  if(status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
      "Error: failed sending broadcast, error 0x%x\r\n", status);                           
  }
}

// End utility functions
// *******************************************************************

