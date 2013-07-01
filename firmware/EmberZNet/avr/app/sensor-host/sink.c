// *******************************************************************
//  sink.c
//
//  sample app for Ember Stack API
//
//  This is an example of a sensor application using the Ember ZigBee stack.
//  There are two types of nodes: sensor nodes, which collect data and
//  attempt to send it to a central point, and sink nodes, which are the
//  central collection point. In a real environment the sensor nodes
//  would collect their data from the environment around them: an electric
//  meter, a light panel, or a temperature sensor. In this example the
//  sensor nodes create random data.
//
//  There are three types of sensor nodes: line powered sensors, sleepy
//  sensors, and mobile sensors. Line powered sensors route and are always
//  awake. They can also serve as parents to sleepy or mobile sensors.
//  Sleepy sensors sleep whenever they can to conserve power but they do
//  not route and rely on their parent when communicating with the network.
//  Mobile sensors are like sleepy sensors but are meant to move. mobile
//  sensors will wait a smaller amount of time (5 seconds) to switch parents
//  than sleepy sensors will (60 seconds) if their parent does not respond to
//  their polls.
//
//  A sink node sends out advertisements at regular intervals using a
//  multicast. When a sensor without a sink hears this multicast, it
//  sends a message requesting to use this sink. If the sink has a free
//  address table entry it responds with a sink ready message and at that
//  point the sensor is free to send data.
//  If N data packets (N is the error tolerance) cannot be successfully
//  sent from the sensor to the sink, then the sensor node will invalidate
//  its tie to the sink and wait for another advertisement.
//
//  Sleepy and mobile sensors query their parent for the sink upon
//  joining the network. The parent responds to the query with a sink
//  advertise message indicating the network sink.
//
//  Note that the serial input is not responsive when the sleepy and mobile
//  nodes are sleeping, which is whenever they are joined to the network.
//
//  This example also provides some simple commands that can be sent to the
//  node via the serial port. A brief explanation of the commands is
//  provided below:
//  ('f') force the sink to advertise
//  ('t') makes the node play a tune. Useful in identifying a node.
//  ('a') prints the address table of the node
//  ('m') prints the multicast table of the node
//  ('l') tells the node to send a multicast hello packet.
//  ('i') prints info about this node including channel, power, and app
//  ('b') puts the node into the bootloader menu (as an example).
//  ('c') prints the child table
//  ('0') simulate button 0 press - turns permit join on for 60 seconds
//        this allows other nodes to join to this node
//  ('1') simulate button 1 press - leave the ZigBee network so on node 
//            reset (command 'e') the sink can form a new network
//  ('e') resets this node
//  ('k') print the security keys
//  ('*') switch the network key: send the key followed by a switch key
//        command 30 seconds later
//  ('&') send a switch key command. This is needed only if the device 
//        sent a new key and then reset before it was able to send the
//        switch key command
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor-host/common.h"
#include "app/util/security/security.h"

#ifdef GATEWAY_APP
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#endif

// *******************************************************************
// Define the concentrator type for use when sending "many-to-one"
// route requests.
//
// In EmberZNet 2.x and earlier a node sending this type of request
// was referred to as an "aggregator". In the ZigBee specification and
// EmberZNet 3 it is referred to as a "concentrator".
// 
// EMBER_HIGH_RAM_CONCENTRATOR is used when the caller has enough
// memory to store source routes for the whole network.
// EMBER_LOW_RAM_CONCENTRATOR is used when the concentrator has
// insufficient RAM to store all outbound source routes.

#if defined(GATEWAY_APP)
  int16u concentratorType = EMBER_HIGH_RAM_CONCENTRATOR;
#else
  int16u concentratorType = EMBER_LOW_RAM_CONCENTRATOR;
#endif

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

// Keep track of the current network state
static EmberNetworkStatus networkState = EMBER_NO_NETWORK;

// the time a sink waits before advertising
int16u timeBeforeSinkAdvertise = TIME_BEFORE_SINK_ADVERTISE;

// buffer for organizing data before we send a message
int8u globalBuffer[128];

// keeps track of the last time a sink node heard from a sensor. If it doesn't 
// hear from a sensor in (MISS_PACKET_TOLERANCE * SEND_DATA_RATE) - meaning
// it missed MISS_PACKET_TOLERANCE data packets, it deletes the address
// table entry.
int16u ticksSinceLastHeard[EMBER_ADDRESS_TABLE_SIZE];

boolean buttonZeroPress = FALSE;  // for button push (see halButtonIsr)
boolean buttonOnePress = FALSE;  // for button push (see halButtonIsr)

// to play a tune - used to identify a node
int8u PGM tune[] = {
  NOTE_D4,  2,
  0,        2,
  NOTE_B4,  1,
  0,        1,
  NOTE_C4,  1,
  0,        1,
  0,        0
};

// a timer to remind us to send the network key update after we have sent
// out the network key. We must wait a period equal to the broadcast 
// timeout so that all routers have a chance to receive the broadcast 
// of the new network key
int8u sendNetworkKeyUpdateTimer;
#define SENSORAPP_NETWORK_KEY_UPDATE_TIME 120; // 30 seconds


// End application specific constants and globals
// *******************************************************************

// *******************************************************************
// Forward declarations
void printHelp(void);
void addMulticastGroup(void);
void processSerialInput(void);
static void applicationTick(void);
void sinkAdvertise(void);
void handleSinkAdvertise(int8u* data);
void handleSensorSelectSink(EmberEUI64 eui,
                            EmberNodeId sender);
int8u findFreeAddressTableLocation(void);
int8u findAddressTableLocation(EmberEUI64 eui64);
#define checkForAddressDuplicates(eui64) \
  (findAddressTableLocation(eui64) != EMBER_NULL_ADDRESS_TABLE_INDEX)
void quit(void);
void sinkInit(void);

//
// *******************************************************************

// *******************************************************************
// Begin main application loop

int main( MAIN_FUNCTION_PARAMETERS )
{
  // structure to store necessary network parameters of the node
  // (which are panId, enableRelay, radioTxPower, and radioChannel)
  EmberNetworkParameters networkParams;
  EmberResetType reset;
  EmberStatus status;
  EmberNodeType nodeType;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
  EzspStatus ezspStat;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  reset = halGetResetInfo();

  // inititialize the serial port
  // good to do this before ezspUtilInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }

  emberSerialPrintf(APP_SERIAL, "Reset is 0x%x:%p\r\n", reset,
                    halGetResetString());

#ifdef GATEWAY_APP
  if (!ashProcessCommandOptions(argc, argv))
    return 1;
#endif

  //reset the EM260 and wait for it to come back
  //this will guarantee that the two MCUs start in the same state
  {
    EzspStatus stat = ezspInit();
    if(stat != EZSP_SUCCESS) {
      // report status here
      emberSerialGuaranteedPrintf(APP_SERIAL,
                "ERROR: ezspInit 0x%x\r\n", stat);
      // the app can choose what to do here, if the app is running
      // another device then it could stay running and report the
      // error visually for example. This app asserts.
      quit();
    }
  }
  
  {
    // call version - must be first command. We configure one EZSP
    // configuration parameter below since it isnt configured via
    // ezsp utils
    int8u stackType;
    int16u ezspUtilStackVersion;
    
    int8u protocolVersion = ezspVersion(EZSP_PROTOCOL_VERSION,
                                        &stackType,
                                        &ezspUtilStackVersion);
    if ( protocolVersion != EZSP_PROTOCOL_VERSION ) {
      emberSerialGuaranteedPrintf(APP_SERIAL, 
        "Expected NCP EZSP version %d, but read %d.\n", 
        EZSP_PROTOCOL_VERSION, protocolVersion);
      quit();
    }
  }

  // This configuration parameter sizes the number of address table 
  // entries that contain short to long address mappings needed during 
  // the key exchanges that happen immediately after a join.  It should 
  // be set to the maximum expected simultaneous joins.
  ezspStat = 
    ezspSetConfigurationValue(EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE, 3);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialGuaranteedPrintf(APP_SERIAL, 
                      "ERROR: ezspSetConfigurationValue "
                      "EZSP_CONFIG_TRUST_CENTER_ADDRESS_CACHE_SIZE: 0x%x\r\n", 
                                ezspStat);
  }

  // ezspUtilInit must be called before other EmberNet stack functions
  status = ezspUtilInit(APP_SERIAL, reset);
  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
              "ERROR: ezspUtilInit 0x%x\r\n", status);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
    quit();
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: ezspUtilInit passed\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }


  // init application state
  sinkInit();


  // print a startup message
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT: sink app ");

  printEUI64(APP_SERIAL, (EmberEUI64*) emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  // host needs to supply ack so that there is an opportunity to add 
  // the source route - set the policy before forming  
  ezspStat = ezspSetPolicy(EZSP_UNICAST_REPLIES_POLICY, 
                           EZSP_HOST_WILL_SUPPLY_REPLY);
  if (ezspStat != EMBER_SUCCESS) {
    emberSerialGuaranteedPrintf(APP_SERIAL, 
                 "ERROR: ezspSetPolicy EZSP_UNICAST_REPLIES_POLICY: 0x%x\r\n", 
                                ezspStat);
  }

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  // sink nodes need to be coordinators, so ensure we are a coordinator
  status = ezspGetNetworkParameters(&nodeType, &networkParams);
  if ((status != EMBER_SUCCESS) ||
      (nodeType != EMBER_COORDINATOR) ||
      (emberNetworkInit() != EMBER_SUCCESS))
  {
    // Set the security keys and the security state - specific to this 
    // application, all variants of this application (sink, sensor, 
    // sleepy-sensor, mobile-sensor) need to use the same security setup.
    // This function is in app/sensor/common.c. This function should only
    // be called when a network is formed as the act of setting the key
    // sets the frame counters to 0. On reset and networkInit this should
    // not be called.
    #if EMBER_SECURITY_LEVEL == 5
      sensorCommonSetupSecurity();
    #endif //EMBER_SECURITY_LEVEL == 5
   

    // Bring the network up.
    #ifdef USE_HARDCODED_NETWORK_SETTINGS
      // use hardcoded settings
      networkParams.panId = APP_PANID;
      networkParams.radioTxPower = APP_POWER;
      networkParams.radioChannel = APP_CHANNEL;
      MEMCOPY(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);
      emberSerialPrintf(APP_SERIAL,
           "SINK APP: forming network - channel 0x%x, panid 0x%2x, ",
           networkParams.radioChannel, networkParams.panId);
      printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      emberSerialWaitSend(APP_SERIAL);
      status = emberFormNetwork(&networkParams);
      if (status != EMBER_SUCCESS) {
        emberSerialGuaranteedPrintf(APP_SERIAL,
          "ERROR: from emberFormNetwork: 0x%x\r\n",
          status);
        quit();
      }
    #else
      // Find a free channel and PAN ID then form the network
      emberSerialPrintf(APP_SERIAL,
           "SINK APP: scanning for an available channel and panid\r\n");
      emberSerialWaitSend(APP_SERIAL);
      ezspScanAndFormNetwork(EMBER_ALL_802_15_4_CHANNELS_MASK,
                             APP_POWER,
                             extendedPanId);
    #endif
  } else {
    emberSerialPrintf(APP_SERIAL,
         "SINK APP: forming network - channel 0x%x, panid 0x%2x, ",
         networkParams.radioChannel, networkParams.panId);
    printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }
  
  // NOTE: at this point the sink network is UP. The user needs to
  // press BUTTON0 to turn allow join on so sensors can join to this
  // sink's network

  // say hello with LEDs
  halSetLed(BOARDLED0);
  halClearLed(BOARDLED1);

  // event loop
  while(TRUE) {

    halResetWatchdog();

    emberTick();

    processSerialInput();

    applicationTick(); // check timeouts, buttons, flash LEDs
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
  // print an error if a message can't be sent
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "ERROR 0x%x: no ack for message 0x%2x\r\n", 
                      status, 
                      apsFrame->clusterId);
  }
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
  int8u relayCount;
  EmberStatus status;
  boolean needSendAck = FALSE;

  // Called with an incoming message
  EmberEUI64 eui;

  // make sure this is a valid packet of sensor/sink app
  // it must have a EUI64 address (8 bytes minimum)
  if (length < 8) {
    emberSerialPrintf(APP_SERIAL, 
                      "RX [bad packet] cluster 0x%2x of length %x\r\n",
                      apsFrame->clusterId,
                      length);
    return;
  }
  MEMCOPY((int8u*) &eui, message, EUI64_SIZE);

  // ********************************************
  // handle the incoming message
  // ********************************************
  switch (apsFrame->clusterId) {
  case MSG_SINK_ADVERTISE:
    // Sink advertise should be 10 bytes: EUI(8) and shortID(2)
    // This handles packets that are too short.
    if (length < 10) {
      emberSerialPrintf(APP_SERIAL, "RX too short [sink advertise] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      return;
    } 

    // ignore own multicast advertisements
    if (!(emberIsLocalEui64(eui))) {
      emberSerialPrintf(APP_SERIAL, "RX [sink advertise] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "; ignoring\r\n");
    }
    break;

  case MSG_SENSOR_SELECT_SINK:
    if (type == EMBER_INCOMING_UNICAST) {
      emberSerialPrintf(APP_SERIAL, "RX [sensor select sink] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
      handleSensorSelectSink(eui, sender);
	  needSendAck = TRUE;
    }
    // if type is EMBER_INCOMING_DATAGRAM_REPLY then this is an ack
    // for MSG_SENSOR_SELECT_SINK, which is really a MSG_SINK_READY
    else if (type == EMBER_INCOMING_UNICAST_REPLY) {
      emberSerialPrintf(APP_SERIAL, "RX [sink ready] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "; this is an error]\r\n");
    }
    break;

  case MSG_SINK_READY:
    emberSerialPrintf(APP_SERIAL, "RX [sink ready] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; this is an error]\r\n");
    break;

  case MSG_SINK_QUERY:
    emberSerialPrintf(APP_SERIAL, "RX [sink query] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
    handleSinkQuery(sender);
	needSendAck = TRUE;
    break;
    
  case MSG_DATA:
    // we just heard from this remote node so clear ticks since last heard
    if (addressTableIndex != EMBER_NULL_ADDRESS_TABLE_INDEX)
      ticksSinceLastHeard[addressTableIndex] = 0;
    emberSerialPrintf(APP_SERIAL, "RX [DATA] from: ");
    printEUI64(APP_SERIAL, &eui);
    if (length < 10) {
      emberSerialPrintf(APP_SERIAL, "; len 0x%x / data NO DATA!\r\n");
    } else {
      emberSerialPrintf(APP_SERIAL, "; len 0x%x / data 0x%x%x\r\n", length,
                        message[EUI64_SIZE + 0],
                        message[EUI64_SIZE + 1]);
    }
	needSendAck = TRUE;
    break;

  case MSG_MULTICAST_HELLO:
    // ignore own multicast hello's
    if (!(emberIsLocalEui64(eui))) {
      emberSerialPrintf(APP_SERIAL, "RX [multicast hello] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
    }
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "RX [unknown (%2x)] from: ",
                      apsFrame->clusterId);
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; ignoring\r\n");
  }

  if (needSendAck) {
    //find the sender in the source route table
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
  }
}

// this is called when the stack status changes
void ezspStackStatusHandler(EmberStatus status)
{
  networkState = emberNetworkState();

  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");
#ifndef USE_HARDCODED_NETWORK_SETTINGS
    {
      EmberStatus status;
      EmberNodeType nodeType;
      EmberNetworkParameters networkParams;
      status = ezspGetNetworkParameters(&nodeType, &networkParams);
      if (status == EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL,
             "SINK APP: network formed - channel 0x%x, panid 0x%2x, ",
             networkParams.radioChannel, networkParams.panId);
        printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
        emberSerialPrintf(APP_SERIAL, "\r\n");
        emberSerialWaitSend(APP_SERIAL);
      }
    }
#endif

    // Add the multicast group to the multicast table - this is done 
    // after the stack comes up
    addMulticastGroup();
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

// this is called when a scan is complete
void ezspScanCompleteHandler( int8u channel, EmberStatus status )
{}

// this is called when a network is found when app is performing scan
void ezspNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                             int8u lastHopLqi,
                             int8s lastHopRssi)
{}

// this is called when an error occurs while performing a
// scanAndFormNetwork or a scanAndJoinNetwork
void ezspScanErrorHandler(EmberStatus status)
{
  emberSerialGuaranteedPrintf(APP_SERIAL,
    "EVENT: could not find an available channel and panid - status: 0x%x\r\n", 
    status);
}

void ezspUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{}

void hal260IsAwakeIsr(boolean isAwake)
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

// applicationTick - called to check application timeouts, button events,
// and periodically flash LEDs
static void applicationTick(void) {
  static int16u lastBlinkTime = 0;
  int16u time;
  int8u i;
  EmberStatus status;

  static int16u permitJoinsTimer;      // quarter second timer
  int16u joinTimeout = 60;  // 60 seconds

  time = halCommonGetInt16uMillisecondTick();

  // Application timers are based on quarter second intervals, where each 
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks. 
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in 
  // app/sensor-host/common.h.
  if ( (int16u)(time - lastBlinkTime) > TICKS_PER_QUARTER_SECOND ) {
    lastBlinkTime = time;

    // **************************************
    // there are some events we only do when we are joined to the network
    // **************************************
    if (networkState == EMBER_JOINED_NETWORK)
    {
      // *******************
      // blink the LEDs
      // *******************
      halToggleLed(BOARDLED0);
      halToggleLed(BOARDLED1);

      // *******************
      // Increment our timer for joining.  Turn off joining when it
      // has reached the join timeout.
      // *******************
      if ( trustCenterIsPermittingJoins() ) {
        permitJoinsTimer++;
        
        // permitJoinsTimer is in quarter-seconds, joinTimeout is in seconds
        if ( permitJoinsTimer > (int32u)(joinTimeout * 4) ) {
          trustCenterPermitJoins(FALSE);
          permitJoinsTimer = 0;
        }
      }

      // ******************************************
      // see if it is time to advertise
      // ******************************************
      timeBeforeSinkAdvertise = timeBeforeSinkAdvertise - 1;

      // at 0.5 seconds to go, send the "many-to-one" route request
      if (timeBeforeSinkAdvertise == 2) {
        status = emberSendManyToOneRouteRequest(concentratorType,
                                               10); // radius
        emberSerialPrintf(APP_SERIAL,
                          "EVENT: sink send many-to-one route request,"
                          " status 0x%x\r\n", status);
      }

      // do the sink advertise (multicast)
      if (timeBeforeSinkAdvertise == 0) {
        emberSerialPrintf(APP_SERIAL,
            "EVENT: sink automatically advertising to find sensors\r\n");
        sinkAdvertise();
        timeBeforeSinkAdvertise = TIME_BEFORE_SINK_ADVERTISE;
      }

      // ******************************************
      // see if it is time to change the network key
      // ******************************************
      if (sendNetworkKeyUpdateTimer > 0) {
        sendNetworkKeyUpdateTimer--;
        if (sendNetworkKeyUpdateTimer == 0) {
          status = emberBroadcastNetworkKeySwitch();
          emberSerialPrintf(APP_SERIAL,
              "keyswitch: sending switch network key command, status %x\r\n",
                            status);
          emberSerialWaitSend(APP_SERIAL);
        
        }
      }

    }
    // ***************************
    // the next set of events are done even if the device is not part of
    // a network
    // ***************************


    // ***************************
    // check for address table entries that are no longer
    // active (no data messages from them)
    // ***************************

    // increment the counters for invalidating entries in the address table
    for (i=0; i<EMBER_ADDRESS_TABLE_SIZE; i++) {
      if (ticksSinceLastHeard[i] != 0xFFFF) {
        if (++ticksSinceLastHeard[i] >
            (MISS_PACKET_TOLERANCE * SEND_DATA_RATE)) {
          emberSetAddressTableRemoteNodeId(i, EMBER_TABLE_ENTRY_UNUSED_NODE_ID);
          emberSerialPrintf(APP_SERIAL,
              "EVENT: too long since last heard, ");
          emberSerialPrintf(APP_SERIAL,
              "deleting address table index %x, status %x\r\n", i, status);
          ticksSinceLastHeard[i] = 0xFFFF;
        }
      }
    }

    // ***************************
    // check for button events
    // ***************************

    // **** check for BUTTON0 press
    if (buttonZeroPress) {
      buttonZeroPress = FALSE;
      emberSerialPrintf(APP_SERIAL, 
                        "BUTTON0: turn permit join ON for 60 seconds\r\n");

      // turn allow join on
      emberPermitJoining(joinTimeout);
      permitJoinsTimer = 0;
      trustCenterPermitJoins(TRUE);
    }

    // **** check for BUTTON1 press
    if (buttonOnePress) {
      buttonOnePress = FALSE;
      emberSerialPrintf(APP_SERIAL, "EVENT: leave network status 0x%x\r\n",
                        emberLeaveNetwork());
    }


  }
}

// add the multicast group
void addMulticastGroup(void) {
  EmberMulticastTableEntry entry;
  EmberStatus status;

  entry.multicastId = MULTICAST_ID;
  entry.endpoint = ENDPOINT;
  status = ezspSetMulticastTableEntry(MULTICAST_TABLE_INDEX, &entry);

  emberSerialPrintf(APP_SERIAL,
           "EVENT: setting multicast table entry, status is 0x%x\r\n", status);
}

// sendMulticastHello
void sendMulticastHello(void) {
  EmberStatus status;
  EmberApsFrame apsFrame;
  int8u data[HELLO_MSG_SIZE] = {'h','e','l','l','o'};

  // the data - my long address and the string "hello"
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);
  MEMCOPY(&(globalBuffer[EUI64_SIZE]), data, HELLO_MSG_SIZE);

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of EMBER_APS_OPTION_NONE from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_MULTICAST_HELLO; // message type
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_NONE; // none for multicast
  apsFrame.groupId = MULTICAST_ID;          // multicast ID unique to this app 
  apsFrame.sequence = 0;                    // use seq of 0

  // send the message
  status = ezspSendMulticast(&apsFrame, // multicast ID & cluster
                             10,        // radius
                             6,         // non-member radius
                             MSG_MULTICAST_HELLO, /* message tag */
                             EUI64_SIZE + HELLO_MSG_SIZE, /* msg length */
                             globalBuffer, /* message to send */
                             &apsFrame.sequence);

  emberSerialPrintf(APP_SERIAL,
       "TX [multicast hello], status 0x%x\r\n", status);
}

// sinkAdvertise
void sinkAdvertise(void) {
  EmberStatus status;
  EmberApsFrame apsFrame;

  // the data - sink long address (EUI), sink short address
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);
  emberStoreLowHighInt16u(&(globalBuffer[EUI64_SIZE]), emberGetNodeId());

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of EMBER_APS_OPTION_NONE from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SINK_ADVERTISE; // message type
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_NONE; // none for multicast
  apsFrame.groupId = MULTICAST_ID;          // multicast ID unique to this app 
  apsFrame.sequence = 0;                    // use seq of 0

  // send the message
  status = ezspSendMulticast(&apsFrame, // multicast ID & cluster
                             10,        // radius
                             6,         // non-member radius
                             MSG_SINK_ADVERTISE, /* message tag */
                             EUI64_SIZE + 2, /* msg length */
                             globalBuffer, /* message to send */
                             &apsFrame.sequence);

  emberSerialPrintf(APP_SERIAL,
       "TX [sink advertise], status 0x%x\r\n", status);
}


// this handles the message from the sensor to the sink to select the
// sink as a destination for the data generated by the sensor
void handleSensorSelectSink(EmberEUI64 eui,
                            EmberNodeId sender) {
  int8u addressTableIndex;
  EmberStatus status;
  EmberApsFrame apsFrame;
  int8u relayCount;

  // check for a duplicate message, if duplicate then 
  // we've already added the sender's addresses to the
  // address table so just skip over that logic.  We
  // still need to do the sendReply because the sender 
  // never received to previous reply.
  // unicast messages do not guarantee no duplicates
  // because of this, the node must check to make sure it
  // hasn't already received this message.
  if (checkForAddressDuplicates(eui) == TRUE) {
    emberSerialPrintf(APP_SERIAL,
                      "receive duplicate message from ");
    printEUI64(APP_SERIAL, (EmberEUI64 *)eui);
    emberSerialPrintf(APP_SERIAL, "\r\n");
  } else {

    // find a free address table location to put this address
    addressTableIndex = findFreeAddressTableLocation();
    if (addressTableIndex == EMBER_NULL_ADDRESS_TABLE_INDEX) {
      emberSerialPrintf(APP_SERIAL,
                        "WARNING: no more free address table entries\r\n");
      return;
    }

    // add an address table entry
    status = emberSetAddressTableRemoteEui64(addressTableIndex, eui);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL,
                        "TX ERROR [sink ready], set remote EUI64 failure,"
                        " status 0x%x\r\n",
                        status);
      return;
    }
    emberSetAddressTableRemoteNodeId(addressTableIndex, sender);

    emberSerialPrintf(APP_SERIAL,
                      "EVENT: sink set address table entry %x to node [",
                      addressTableIndex );
    printEUI64(APP_SERIAL, (EmberEUI64 *)eui);
    emberSerialPrintf(APP_SERIAL, "]\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }

  ticksSinceLastHeard[addressTableIndex] = 0;

  //find the sender in the source route table
  if (emberFindSourceRoute(sender, &relayCount, (int16u *)globalBuffer)) {
    // if found, send the source route to the ncp
    status = ezspSetSourceRoute(sender, relayCount, (int16u *)globalBuffer);
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, 
                        "ERROR: ezspSetSourceRoute: 0x%x\r\n", 
                        status); 
    }
  }
  
  // send a message indicating success
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SINK_READY;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = ezspSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                           addressTableIndex,
                           &apsFrame,
                           MSG_SINK_READY,
                           EUI64_SIZE,
                           globalBuffer,
                           &apsFrame.sequence);

  // status message
  emberSerialPrintf(APP_SERIAL,
                    "TX [sink ready], status:0x%x\r\n",
                    status);
}

// find a free entry in the address table
int8u findFreeAddressTableLocation(void) {
  int8u i;

  // look through address table
  for (i=0; i<EMBER_ADDRESS_TABLE_SIZE; i++)
    if (emberGetAddressTableRemoteNodeId(i) 
        == EMBER_TABLE_ENTRY_UNUSED_NODE_ID)
      return i;

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

// find the location where the given EUI is stored in the address table
int8u findAddressTableLocation(EmberEUI64 eui64) {
  int8u i;

  // look through address table
  for (i=0; i<EMBER_ADDRESS_TABLE_SIZE; i++)
    if (emberGetAddressTableRemoteNodeId(i) 
        != EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      EmberEUI64 remoteEui64;
      emberGetAddressTableRemoteEui64(i, remoteEui64);
      if (MEMCOMPARE(eui64, remoteEui64, EUI64_SIZE) == 0)
        return i;
    }

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

//
// *******************************************************************

// *******************************************************************
// Callback from the HAL when a button state changes
// WARNING: this callback is an ISR so the best approach is to set a
// flag here when an action should be taken and then perform the action
// somewhere else. In this case the actions are serviced in the 
// applicationTick function
void halButtonIsr(int8u button, int8u state)
{
  // button 0 (button 0 on the dev board) was pushed down
  if (button == BUTTON0 && state == BUTTON_PRESSED) {
    buttonZeroPress = TRUE;
  }

  // button 1 (button 1 on the dev board) was pushed down
  if (button == BUTTON1 && state == BUTTON_PRESSED) {
    buttonOnePress = TRUE;
  }
}

//
// *******************************************************************


// *******************************************************************
// Utility functions

#ifdef GATEWAY_APP
void quit(void) {
  ezspClose();
  exit(-1);
}
#else // GATEWAY_APP
void quit(void) {
  assert(FALSE);
}
#endif // GATEWAY_APP

// **********************************************
// This function:
// - generates a new random key
// - broadcasts this new network key via a Transport Key message to the network
// - sets a flag so that 30 seconds later a Switch Key message is sent out
//
// if the switch key message doesnt go out for some reason attempts to send
// out another Transport Key message will fail. The application needs to send
// out the Switch Key message before sending out another new key. This can be
// done with the "&" command
// **********************************************
void sinkAppSwitchNetworkKey(void)
{
  EmberStatus status;
  EmberKeyData newKey;

  // generate a random key  
  emberSerialPrintf(APP_SERIAL, "keyswitch: generating new key\r\n");
  emberSerialWaitSend(APP_SERIAL);
  status = emberGenerateRandomKey(&newKey);

  // abort on an error
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "keyswitch: key gen FAILED!\r\n");
    emberSerialWaitSend(APP_SERIAL);
    return;
  }

  // print out the key
  emberSerialPrintf(APP_SERIAL, "keyswitch: new key: ");
  sensorCommonPrint16ByteKey(newKey.contents);
  emberSerialWaitSend(APP_SERIAL);

  // broadcast the key out
  status = emberBroadcastNextNetworkKey(&newKey);
  emberSerialPrintf(APP_SERIAL, "keyswitch: bcast new key status 0x%x\r\n",
                    status);
  emberSerialWaitSend(APP_SERIAL);

  // if the broadcast key succeeded, set the flag for sending the key
  // switch command
  if (status == EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
         "keyswitch: waiting 30 seconds to send key switch command\r\n");
    emberSerialWaitSend(APP_SERIAL);
    // set a flag so we know to send the key switch at a later time
    sendNetworkKeyUpdateTimer = SENSORAPP_NETWORK_KEY_UPDATE_TIME;
  } 
  // if the broadcast key failed with INVALID CALL that could mean we
  // have sent a key update but not a switch key. Inform the user
  else if (status == EMBER_INVALID_CALL) {
    emberSerialPrintf(APP_SERIAL, 
             "ERROR: this status could mean that the key update was sent\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, 
             "       out but that the switch key wasnt sent out. Use the\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, 
             "       command \"&\" to send the switch key command\r\n");
    emberSerialWaitSend(APP_SERIAL);
  } 

}


// init common state for sensor and sink nodes
void sinkInit(void) {
  int8u i;

  // init the table that keeps track of when we last heard from an
  // address in the address table
  for (i=0; i<EMBER_ADDRESS_TABLE_SIZE; i++) {
    ticksSinceLastHeard[i] = 0xFFFF;
  }
}


// *****************************
// for processing serial cmds
// *****************************
void processSerialInput(void) {
  int8u cmd = '\0';

  if(emberSerialReadByte(APP_SERIAL, &cmd) == 0) {
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\n");

    switch(cmd) {

      // info
    case 'i':
      printNodeInfo();
      break;

      // force sink advertisement
    case 'f':
      emberSerialPrintf(APP_SERIAL,
       "EVENT: serial input, force sink advertise\r\n");
      // setting timer to 3 means next time it is checked, it will
      // go to 2 and send a many-to-one route request, then two ticks later
      // it will send the sink advertise
      timeBeforeSinkAdvertise = 3;
      break;

      // help
    case '?':
      printHelp();
      break;

      // identify tune
    case 't':
      halPlayTune_P(tune, 0);
      halPlayTune_P(tune, 0);
      break;

#ifndef NO_BOOTLOADER
      // bootloader
    case 'b':
      emberSerialPrintf(APP_SERIAL, "starting bootloader...\r\n");
      emberSerialWaitSend(APP_SERIAL);
      halLaunchStandaloneBootloader(BOOTLOADER_MODE_MENU,NULL);
      break;
#endif

      // print address table
    case 'a':
      printAddressTable(EMBER_ADDRESS_TABLE_SIZE);
      break;

      // print multicast table
    case 'm':
      printMulticastTable(EMBER_MULTICAST_TABLE_SIZE);
      break;

      // send hello - multicast msg
    case 'l':
      sendMulticastHello();
      break;

      // 0 and 1 mean button presses
    case '0':
      buttonZeroPress = TRUE;
      break;
    case '1':
      buttonOnePress = TRUE;
      break;

    case 'c':
      printChildTable();
      break;

      // causes the node to leave the ZigBee network
      // and then reset to form a new network
    case 'e':
      emberSerialGuaranteedPrintf(APP_SERIAL, "BUTTON1: Resetting node...\r\n");
      halReboot();
      break;

      // print the keys
    case 'k':
      sensorCommonPrintKeys();
      break;

      // switch the network key by sending a new key and then sending a
      // switch key command 30 seconds later
    case '*':
      sinkAppSwitchNetworkKey();
      break;

      // send the switch key command. This is only necessary if the device
      // sent out the new key and then reset before it was able to send 
      // out the switch key command
    case '&':
      emberSerialPrintf(APP_SERIAL,
           "keyswitch: sending switch network key command, status %x\r\n",
                        emberBroadcastNetworkKeySwitch());
      break;

#ifdef GATEWAY_APP
      // quit
    case 'q':
      quit();
      break;
#endif

    case '\n':
    case '\r':
      break;


    default:
      emberSerialPrintf(APP_SERIAL, "unknown cmd\r\n");
      break;
    }

#ifdef SINK_APP
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\nsink-node>");
#else
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\ngateway-node>");
#endif
  }

}
// *********************************
// print help
// *********************************
#define PRINT(x) emberSerialPrintf(APP_SERIAL, x); emberSerialWaitSend(APP_SERIAL)

void printHelp(void)
{
  PRINT("? = help\r\n");
  PRINT("i = print node info\r\n");
#ifndef NO_BOOTLOADER
  PRINT("b = bootloader\r\n");
#endif
  PRINT("l = send multicast [hello]\r\n");
  PRINT("t = play tune\r\n");
  PRINT("a = print address table\r\n");
  PRINT("m = print multicast table\r\n");
  PRINT("f = send sink advertisement\r\n");
  PRINT("0 = button 0: turn allow join ON for 60 sec\r\n");
  PRINT("1 = button 1: leave ZigBee network\r\n");
  PRINT("e = reset node\r\n");
  PRINT("c = print child table\r\n");
#ifdef GATEWAY_APP
  PRINT("q = quit\r\n");
#endif
  PRINT("k = print the security keys\r\n");
  PRINT("* = switch the network key: send the key followed by a switch\r\n");
  PRINT("    key command 30 seconds later\r\n");
  PRINT("& = send a switch key command. This is needed only if the device\r\n");
  PRINT("    sent a new key and then reset before it was able to send the\r\n");
  PRINT("    switch key command\r\n");
}


// End utility functions
// *******************************************************************
