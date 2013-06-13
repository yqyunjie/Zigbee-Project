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
//  ('j') prints out the status of the JIT (Just In Time) message
//        storage
//  ('0') simulate button 0 press - turns permit join on for 60 seconds
//        this allows other nodes to join to this node
//  ('1') simulate button 1 press - leave the network
//  ('e') reset the node
//  ('B') attempts a passthru bootload of the first device in the address 
//        table. This is meant to show how standalone bootloader is 
//        integrated into an application and not meant as a full solution. 
//        All necessary code is defined'ed under USE_BOOTLOADER_LIB.
//  ('C') attempts a passthru bootload of the first device in the child table
//  ('Q') sends a bootload query message.
//  ('*') switch the network key: send the key followed by a switch key
//        command 30 seconds later
//  ('&') send a switch key command. This is needed only if the device 
//        sent a new key and then reset before it was able to send the
//        switch key command
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor/common.h"
#include "app/util/security/security.h"
#ifdef  PHY_BRIDGE
 #ifdef CORTEXM3
  #include "hal/micro/cortexm3/diagnostic.h"
 #endif
 #include "phy/bridge/zigbee-bridge.h"
 #ifdef  BRIDGE_TRACE
  #include "phy/bridge/zigbee-bridge-internal.h"
 #endif//BRIDGE_TRACE

 static const char* brgControlNames[BRG_CONTROL_ITEMS] = {
        "Normal", "Radio Only", "Bridge only", "Radio and Bridge",
 };
#endif//PHY_BRIDGE

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

int16u concentratorType = EMBER_LOW_RAM_CONCENTRATOR;

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

// the time a sink waits before advertising
int16u timeBeforeSinkAdvertise = TIME_BEFORE_SINK_ADVERTISE;

// buffer for organizing data before we send a message
int8u globalBuffer[100];

// keeps track of the last time a sink node heard from a sensor. If it doesn't 
// hear from a sensor in (MISS_PACKET_TOLERANCE * SEND_DATA_RATE) - meaning
// it missed MISS_PACKET_TOLERANCE data packets, it deletes the address
// table entry.
// *** NOTE: SINK_ADDRESS_TABLE_SIZE is used for the address table instead
// ***       of the usual EMBER_ADDRESS_TABLE_SIZE, as 5 entries are reserved
// ***       for use by the trust center code.
// *** See "app/sensor/sensor-configuration.h" for more details.
int16u ticksSinceLastHeard[SINK_ADDRESS_TABLE_SIZE];

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
void sinkInit(void);


// these are defines used for the variable networkFormMethod. This variable
// affects the serial output when a network is formed. 
#define SINK_FORM_NEW_NETWORK 1
#define SINK_USE_NETWORK_INIT 2
#define SINK_USE_SCAN_UTILS   3
int8u networkFormMethod = SINK_FORM_NEW_NETWORK;

//
// *******************************************************************

// *******************************************************************
// Begin main application loop

int main(void)
{
  // structure to store necessary network parameters of the node
  // (which are panId, enableRelay, radioTxPower, and radioChannel)
  EmberNetworkParameters networkParams;
  EmberStatus status;
  EmberNodeType nodeType;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  // inititialize the serial port
  // good to do this before emberInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1) 
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }

  // emberInit must be called before other EmberNet stack functions
  status = emberInit();

  // print the reason for the reset
  emberSerialGuaranteedPrintf(APP_SERIAL, "reset: %p\r\n", 
                              (PGM_P)halGetResetString());
#ifdef  PHY_BRIDGE
  #ifdef CORTEXM3
    if (halResetWasCrash()) {
      halPrintCrashSummary(APP_SERIAL);
      halPrintCrashDetails(APP_SERIAL);
      halPrintCrashData(APP_SERIAL);
    }
  #endif
#endif//PHY_BRIDGE

  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
              "ERROR: emberInit 0x%x\r\n", status);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
    assert(FALSE);
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: emberInit passed\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }

  // init application state
  sinkInit();

  // when acting as a trust center, a portion of the address table is
  // needed to keep track of shortID-to-EUI mappings. The mapping is
  // needed during the key exchanges that happen immediately after a join.
  // This function is found in app/util/security/security-address-cache.c.
  // The sink allocates address table entries from 0 to 
  // (SINK_ADDRESS_TABLE_SIZE-1) for the use of the sink app. It allocates 
  // entries from SINK_ADDRESS_TABLE_SIZE to (SINK_ADDRESS_TABLE_SIZE + 
  // SINK_TRUST_CENTER_ADDRESS_CACHE_SIZE) for the trust center use. 
  // The defines used here are defined in app/sensor/sensor-configuration.h
  securityAddressCacheInit(SINK_ADDRESS_TABLE_SIZE,
                           SINK_TRUST_CENTER_ADDRESS_CACHE_SIZE);

 
  // print a startup message
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT : sink app ");

  printEUI64(APP_SERIAL, (EmberEUI64*) emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  #ifdef USE_BOOTLOADER_LIB
    // Using the same port for application serial print and passthru 
    // bootloading.  User must be careful not to print anything to the port
    // while doing passthru bootload since that can interfere with the data
    // stream.  Also the port's baud rate will be set to 115200 kbps in order
    // to maximize bootload performance.
    bootloadUtilInit(APP_SERIAL, APP_SERIAL);
  #endif // USE_BOOTLOADER_LIB

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  // sink nodes need to be coordinators, so ensure we are a coordinator
  networkFormMethod = SINK_USE_NETWORK_INIT;
  if (((emberGetNodeType(&nodeType)) != EMBER_SUCCESS) ||
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
    sensorCommonSetupSecurity();

    // Bring the network up.
    #ifdef USE_HARDCODED_NETWORK_SETTINGS
      // set the mode we are using
      networkFormMethod = SINK_FORM_NEW_NETWORK;

      // use the settings from app/sensor/common.h
      networkParams.panId = APP_PANID;
      networkParams.radioTxPower = APP_POWER;
      networkParams.radioChannel = APP_CHANNEL;
      MEMCOPY(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);

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
        assert(FALSE);
      }
    #else
      // set the mode we are using
      networkFormMethod = SINK_USE_SCAN_UTILS;

      // tell the user what is going on
      emberSerialPrintf(APP_SERIAL,
           "FORM : scanning for an available channel and panid\r\n");
      emberSerialWaitSend(APP_SERIAL);

      // Use a function from app/util/common/form-and-join.c
      // that scans and selects a quiet channel to form on.
      // Once a PAN id and channel are chosen, calls
      // emberUnusedPanIdFoundHandler.
      emberScanForUnusedPanId(EMBER_ALL_802_15_4_CHANNELS_MASK, 5);
    #endif
  } 
  // don't need an else clause. The else clause means the emberNetworkInit
  // worked and the stackStatusHandler will be called.


#ifndef NO_LED
  // say hello with LEDs
  halToggleLed(BOARDLED1);
  halToggleLed(BOARDLED3);
#endif//NO_LED

  // event loop
  while(TRUE) {

    halResetWatchdog();

    emberTick();
    emberFormAndJoinTick();

    processSerialInput();

    applicationTick(); // check timeouts, buttons, flash LEDs

    #ifdef DEBUG
      emberSerialBufferTick();   // Needed for debug which uses buffered serial
    #endif
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
void emberMessageSentHandler(EmberOutgoingMessageType type,
                      int16u indexOrDestination,
                      EmberApsFrame *apsFrame,
                      EmberMessageBuffer message,
                      EmberStatus status)
{

}


#ifdef USE_BOOTLOADER_LIB
// When a device sends out a bootloader query, the bootloader
// query response messages are parsed by the bootloader-util
// libray and and handled to this function. This application
// simply prints out the EUI of the device sending the query
// response.
void bootloadUtilQueryResponseHandler(boolean bootloaderActive,
                                      int16u manufacturerId,
                                      int8u *hardwareTag,
                                      EmberEUI64 targetEui,
                                      int8u bootloaderCapabilities,
                                      int8u platform,
                                      int8u micro,
                                      int8u phy,
                                      int16u blVersion)
{
  emberSerialPrintf(APP_SERIAL,"RX [BL QUERY RESP] eui: ");
  printEUI64(APP_SERIAL, (EmberEUI64*)targetEui);
  emberSerialPrintf(APP_SERIAL," running %p\r\n", 
                    bootloaderActive ? "bootloader":"stack");
  emberSerialWaitSend(APP_SERIAL);
}

// This function is called by the bootloader-util library 
// to ask the application if it is ok to start the bootloader.
// This happens when the device is meant to be the target of
// a bootload. The application could compare the manufacturerId 
// and/or hardwareTag arguments to known values to enure that 
// the correct image will be bootloaded to this device.
boolean bootloadUtilLaunchRequestHandler(int16u manufacturerId,
                                         int8u *hardwareTag,
                                         EmberEUI64 sourceEui) {
  // TODO: Compare arguments to known values.
  
  // TODO: Check for minimum required radio signal strength (RSSI).
  
  // TODO: Do not agree to launch the bootloader if any of the above conditions
  // are not met.  For now, always agree to launch the bootloader.
  return TRUE;
}
#endif // USE_BOOTLOADER_LIB


void emberIncomingMessageHandler(EmberIncomingMessageType type,
                                 EmberApsFrame *apsFrame,
                                 EmberMessageBuffer message)
{
  // Called with an incoming message
  EmberEUI64 eui;
  EmberNodeId sender = emberGetSender();
  int8u length = emberMessageBufferLength(message);
  int8u addressTableIndex;

  #ifdef USE_BOOTLOADER_LIB
    // If we are in the middle of bootloading, then we want to limit the
    // radio activity to minimum to avoid causing any interruptions to the
    // bootloading process.
    if(IS_BOOTLOADING) {
      return;
    }
  #endif

  // make sure this is a valid packet of sensor/sink app
  // it must have a EUI64 address (8 bytes minimum)
  if (length < 8) {
    emberSerialPrintf(APP_SERIAL, 
                      "RX [bad packet] cluster 0x%2x of length %x\r\n",
                      apsFrame->clusterId,
                      length);
    return;
  }
  emberCopyFromLinkedBuffers(message, 0, (int8u*) &eui, 8);

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
    break;
    
  case MSG_DATA:
    // we just heard from this remote node so clear ticks since last heard
    addressTableIndex = findAddressTableLocation(eui);
    if (addressTableIndex != EMBER_NULL_ADDRESS_TABLE_INDEX)
      ticksSinceLastHeard[addressTableIndex] = 0;
    emberSerialPrintf(APP_SERIAL, "RX [DATA] from: ");
    printEUI64(APP_SERIAL, &eui);
    if (length < 10) {
      emberSerialPrintf(APP_SERIAL, "; len 0x%x / data NO DATA!\r\n");
    } else {
      emberSerialPrintf(APP_SERIAL, "; len 0x%x / data 0x%x%x\r\n",
                        length,
                        emberGetLinkedBuffersByte(message, EUI64_SIZE + 0),
                        emberGetLinkedBuffersByte(message, EUI64_SIZE + 1));
#ifdef DEBUG
      emberDebugPrintf("Sink received data: 0x%x%x \r\n",
                        emberGetLinkedBuffersByte(message, EUI64_SIZE + 0),
                        emberGetLinkedBuffersByte(message, EUI64_SIZE + 1));
#endif
    }
    break;

  case MSG_MULTICAST_HELLO:
    // ignore own multicast hello's
    if (!(emberIsLocalEui64(eui))) {
      emberSerialPrintf(APP_SERIAL, "RX [multicast hello] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
    }
    // store as a JIT (Just in Time) message for children
    appAddJitForAllChildren(MSG_MULTICAST_HELLO, eui, EUI64_SIZE);
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "RX [unknown (%2x)] from: ",
                      apsFrame->clusterId);
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; ignoring\r\n");
  }
}

// this is called when the stack status changes
void emberStackStatusHandler(EmberStatus status)
{

  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");

    {
      EmberStatus status;
      EmberNetworkParameters networkParams;
      status = emberGetNetworkParameters(&networkParams);
      if (status == EMBER_SUCCESS) {
        switch (networkFormMethod) {
        case SINK_USE_NETWORK_INIT:
          emberSerialPrintf(APP_SERIAL,
                            "FORM : network started using network init\r\n");
          break;
        case SINK_FORM_NEW_NETWORK:
          emberSerialPrintf(APP_SERIAL,
                            "FORM : new network formed\r\n");
          break;
        case SINK_USE_SCAN_UTILS:
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


// ******************************************************************
// the following two functions are redefined in the form-and-join utilities

// this is called when a scan is complete
//void emberScanCompleteHandler( int8u channel, EmberStatus status )
//{}

// this is called when a network is found when app is performing scan
//void emberNetworkFoundHandler(int8u channel,
//                              int16u panId,
//                              int8u *extendedPanId,
//                              boolean expectingJoin,
//                              int8u stackProfile)
//{}
// ******************************************************************

void emberScanErrorHandler(EmberStatus status)
{
  emberSerialGuaranteedPrintf(APP_SERIAL,
    "EVENT: could not find an available channel and panid - status: 0x%x\r\n", 
    status);
}

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
  EmberNetworkParameters parameters;
  int8u extendedPanId[8] = APP_EXTENDED_PANID;
  MEMCOPY(parameters.extendedPanId, 
          extendedPanId,
          EXTENDED_PAN_ID_SIZE);
  parameters.panId = panId;
  parameters.radioTxPower = APP_POWER;
  parameters.radioChannel = channel;
  emberFormNetwork(&parameters);
}

void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{
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
  EmberStatus status = 0;
  
  #if EMBER_SECURITY_LEVEL == 5
    static int16u permitJoinsTimer; // quarter second timer
  #endif // EMBER_SECURITY_LEVEL == 5

  // set the join timeout to 60 seconds
  // this is used as the value passed to emberPermitJoining. It is also
  // used to determine when to turn off joining at the trust center
  int16u joinTimeout = 60; 

  #ifdef USE_BOOTLOADER_LIB
    bootloadUtilTick();
  #endif // USE_BOOTLOADER_LIB
    
  time = halCommonGetInt16uMillisecondTick();

  // Application timers are based on quarter second intervals, where each 
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks. 
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in 
  // app/sensor/common.h.
  if ( (int16u)(time - lastBlinkTime) > TICKS_PER_QUARTER_SECOND ) {
    lastBlinkTime = time;

    // **************************************
    // there are some events we only do when we are joined to the network
    // **************************************
    if (emberNetworkState() == EMBER_JOINED_NETWORK)
    {
#ifndef NO_LED
      // *******************
      // blink the LEDs
      // *******************
      halToggleLed(BOARDLED3);
      halToggleLed(BOARDLED2);
#endif//NO_LED

#if EMBER_SECURITY_LEVEL == 5
      // *******************
      // Increment our timer for joining.  Turn off joining at the trust
      // center when it has reached the join timeout.
      // *******************
      if ( trustCenterIsPermittingJoins() ) {
        permitJoinsTimer++;
        
        // permitJoinsTimer is in quarter-seconds, joinTimeout is in seconds
        if ( permitJoinsTimer > (joinTimeout * 4)) {
          trustCenterPermitJoins(FALSE);
          permitJoinsTimer = 0;
        }
      }
#endif

      // ******************************************
      // see if it is time to advertise
      // ******************************************
      timeBeforeSinkAdvertise = timeBeforeSinkAdvertise - 1;

      // at 0.5 seconds to go, send the "many-to-one" route request
      if (timeBeforeSinkAdvertise == 2) {
        status = emberSendManyToOneRouteRequest(concentratorType,
                                                10);        // radius
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
    for (i=0; i<SINK_ADDRESS_TABLE_SIZE; i++) {
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
      emberSerialPrintf(APP_SERIAL, "BUTTON0: turn permit join ON for 60 seconds\r\n");

      // turn allow join on
      emberPermitJoining(joinTimeout);
#if EMBER_SECURITY_LEVEL == 5
      trustCenterPermitJoins(TRUE);
      permitJoinsTimer = 0;
#endif // EMBER_SECURITY_LEVEL == 5
    }

    // **** check for BUTTON1 press
    if (buttonOnePress) {
      buttonOnePress = FALSE;
      emberSerialPrintf(APP_SERIAL, "BUTTON1: leave network status 0x%x\r\n",
                        emberLeaveNetwork());
    }
  }
}

// add the multicast group
void addMulticastGroup(void) {
  EmberMulticastTableEntry *entry = &emberMulticastTable[MULTICAST_TABLE_INDEX];
  entry->multicastId = MULTICAST_ID;
  entry->endpoint = ENDPOINT;
  emberSerialPrintf(APP_SERIAL,
           "EVENT: setting multicast table entry\r\n");
}

// sendMulticastHello
void sendMulticastHello(void) {
  EmberStatus status;
  EmberApsFrame apsFrame;
  int8u data[HELLO_MSG_SIZE] = {'h','e','l','l','o'};
  EmberMessageBuffer buffer = 0;

  // the data - my long address and the string "hello"
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);
  MEMCOPY(&(globalBuffer[EUI64_SIZE]), data, HELLO_MSG_SIZE);

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers((int8u*) globalBuffer,
                                  EUI64_SIZE + HELLO_MSG_SIZE);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
        "TX ERROR [multicast hello], OUT OF BUFFERS\r\n");
    return;
  }

  // all of the defined values below are from app/sensor/common.h
  // with the exception of EMBER_APS_OPTION_NONE from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_MULTICAST_HELLO; // message type
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_NONE; // none for multicast
  apsFrame.groupId = MULTICAST_ID;          // multicast ID unique to this app 
  apsFrame.sequence = 0;                    // use seq of 0

  // send the message
  status = emberSendMulticast(&apsFrame, // multicast ID & cluster
                              10,        // radius
                              6,         // non-member radius
                              buffer);   // message to send 

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  emberSerialPrintf(APP_SERIAL,
       "TX [multicast hello], status 0x%x\r\n", status);
}

// sinkAdvertise
void sinkAdvertise(void) {
  EmberStatus status;
  EmberMessageBuffer buffer = 0;
  EmberApsFrame apsFrame;

  // the data - sink long address (EUI), sink short address
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);
  emberStoreLowHighInt16u(&(globalBuffer[EUI64_SIZE]), emberGetNodeId());

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers((int8u*) globalBuffer, EUI64_SIZE + 2);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
                      "TX ERROR [sink advertise], OUT OF BUFFERS\r\n");
    return;
  }

  // all of the defined values below are from app/sensor/common.h
  // with the exception of EMBER_APS_OPTION_NONE from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SINK_ADVERTISE; // message type
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_NONE; // none for multicast
  apsFrame.groupId = MULTICAST_ID;          // multicast ID unique to this app 
  apsFrame.sequence = 0;                    // use seq of 0

  // send the message
  status = emberSendMulticast(&apsFrame, // multicast ID & cluster
                              10,        // radius
                              6,         // non-member radius
                              buffer);   // message to send 

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  emberSerialPrintf(APP_SERIAL,
       "TX [sink advertise], status 0x%x\r\n", status);
}


// this handles the message from the sensor to the sink to select the
// sink as a destination for the data generated by the sensor
// NOTE: be sure to only call call-back safe EmberNet
// functions, since this is called from inside
// emberIncomingMessageHandler()!
void handleSensorSelectSink(EmberEUI64 eui,
                            EmberNodeId sender) {
  int8u addressTableIndex;
  EmberMessageBuffer buffer;
  EmberStatus status = 0;

  // check for a duplicate message, if duplicate then 
  // we've already added the sender's addresses to the
  // address table so just skip over that logic.  We
  // still need to do the sendReply because the sender 
  // never received to previous reply.
  // unicast messages do not guarantee no duplicates
  // because of this, the node must check to make sure it
  // hasn't already received this message.
  addressTableIndex = findAddressTableLocation(eui);
  if (addressTableIndex != EMBER_NULL_ADDRESS_TABLE_INDEX) {
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

  // send a message indicating success
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers((int8u*)globalBuffer, EUI64_SIZE);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
                      "TX ERROR [sink ready], OUT OF BUFFERS\r\n");
    emberSetAddressTableRemoteNodeId(addressTableIndex, 
                                     EMBER_TABLE_ENTRY_UNUSED_NODE_ID);
    emberSerialPrintf(APP_SERIAL,
                      "deleting address table index %x, status %x\r\n",
                      addressTableIndex, status);
    return;
  }

  // send the message
  status = emberSendReply(MSG_SINK_READY, buffer);

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  // status message
  emberSerialPrintf(APP_SERIAL,
                    "TX [sink ready], status:0x%x\r\n",
                    status);
}

// look through the address table for a free location.
int8u findFreeAddressTableLocation(void) {
  int8u i;

  // note that this uses SINK_ADDRESS_TABLE_SIZE for the address table size
  // instead of the usual EMBER_ADDRESS_TABLE_SIZE, as some entries are 
  // reserved for use by the trust center code. 
  // See "app/sensor/sensor-configuration.h" for more details.
  for (i=0; i<SINK_ADDRESS_TABLE_SIZE; i++) {
    if (emberGetAddressTableRemoteNodeId(i) 
        == EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      return i;
    }
  }

  return EMBER_NULL_ADDRESS_TABLE_INDEX;
}

// find an existing address table entry that matches the EUI64 passed in 
int8u findAddressTableLocation(EmberEUI64 eui64) {
  int8u i;


  // note that this uses SINK_ADDRESS_TABLE_SIZE for the address table size
  // instead of the usual EMBER_ADDRESS_TABLE_SIZE, as some entries are 
  // reserved for use by the trust center code. 
  // See "app/sensor/sensor-configuration.h" for more details.
  for (i=0; i<SINK_ADDRESS_TABLE_SIZE; i++) {
    if (emberGetAddressTableRemoteNodeId(i) 
        != EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
      EmberEUI64 remoteEui64;
      emberGetAddressTableRemoteEui64(i, remoteEui64);
      if (MEMCOMPARE(eui64, remoteEui64, EUI64_SIZE) == 0)
        return i;
    }
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
#if     (defined(BUTTON0) || defined(BUTTON1))
void halButtonIsr(int8u button, int8u state)
{
#ifdef  BUTTON0
  // button 0 (button 0 on the dev board) was pushed down
  if (button == BUTTON0 && state == BUTTON_PRESSED) {
    buttonZeroPress = TRUE;
  }
#endif//BUTTON0

#ifdef  BUTTON1
  // button 1 (button 1 on the dev board) was pushed down
  if (button == BUTTON1 && state == BUTTON_PRESSED) {
    buttonOnePress = TRUE;
  }
#endif//BUTTON1
}
#endif//(defined(BUTTON0) || defined(BUTTON1))

//
// *******************************************************************


// *******************************************************************
// Utility functions


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
  status = emberGenerateRandomKey(&newKey);

  // abort on an error
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "keyswitch: key gen FAILED!\r\n");
    return;
  }

  // print out the key
  emberSerialPrintf(APP_SERIAL, "keyswitch: new key: ");
  sensorCommonPrint16ByteKey(newKey.contents);

  // broadcast the key out
  status = emberBroadcastNextNetworkKey(&newKey);
  emberSerialPrintf(APP_SERIAL, "keyswitch: bcast new key status %x\r\n",
                    status);
  
  // if the broadcast key succeeded, set the flag for sending the key
  // switch command
  if (status == EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
         "keyswitch: waiting 30 seconds to send key switch command\r\n");
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

  // init the address table and the table that keeps track of when 
  // we last heard from an address in the address table.
  // note that this uses SINK_ADDRESS_TABLE_SIZE for the address table size
  // instead of the usual EMBER_ADDRESS_TABLE_SIZE, as some entries are 
  // reserved for use by the trust center code. 
  // See "app/sensor/sensor-configuration.h" for more details.
  for (i=0; i<SINK_ADDRESS_TABLE_SIZE; i++) {
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
#ifdef  PHY_BRIDGE
      // Bridge Tx Control
    case '>':
    {
      EmZigBrgControl txControl = (emZigBrgGetTxControl() + 1) % BRG_CONTROL_ITEMS;
      (void) emZigBrgSetTxControl(txControl);
      emberSerialPrintf(APP_SERIAL, "Bridge TxControl %p\r\n", brgControlNames[txControl]);
      break;
    }
      // Bridge Rx Control
    case '<':
    {
      EmZigBrgControl rxControl = (emZigBrgGetRxControl() + 1) % BRG_CONTROL_ITEMS;
      (void) emZigBrgSetRxControl(rxControl);
      emberSerialPrintf(APP_SERIAL, "Bridge RxControl %p\r\n", brgControlNames[rxControl]);
      break;
    }
#ifdef  BRIDGE_TRACE
      // Toggle Bridge Tracing
    case '#':
      emZigBrgTrace = !emZigBrgTrace;
      emberSerialPrintf(APP_SERIAL, "Bridge Trace %p\r\n", emZigBrgTrace ? "on" : "off");
      break;
#endif//BRIDGE_TRACE
#endif//PHY_BRIDGE

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
      
#if EMBER_SECURITY_LEVEL == 5
     // print keys
    case 'k':
      sensorCommonPrintKeys();
      break;
#endif //EMBER_SECURITY_LEVEL == 5

      // identify tune
    case 't':
      halPlayTune_P(tune, 0);
      halPlayTune_P(tune, 0);
      break;

      // bootloader
    case 'b':
      emberSerialPrintf(APP_SERIAL, "starting bootloader...\r\n");
      emberSerialWaitSend(APP_SERIAL);
      halLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
      break;

#ifdef USE_BOOTLOADER_LIB
      // This command initiates a passthru bootloading of the first
      // device in the address table
    case 'B':
      {
        int8u index;
        EmberEUI64 eui;
        if (emberGetAddressTableRemoteNodeId(0) 
            == EMBER_TABLE_ENTRY_UNUSED_NODE_ID) {
          // error
          emberSerialPrintf(APP_SERIAL,
                            "ERROR: no device in address table at"
                            " location 0\r\n");
          break;
        }
        emberGetAddressTableRemoteEui64(0, eui);
        emberSerialPrintf(APP_SERIAL, "INFO : attempt BL\r\n");
        if (isMyChild(eui, &index)) {
          bootloadMySleepyChild(eui);
        } 
        else if (isMyNeighbor(eui)) {
          bootloadMyNeighborRouter(eui);
        } 
        else {
          // error  
          emberSerialPrintf(APP_SERIAL,
                            "ERROR: can't bootload device whose address is "
                            " stored at location 0 of the address table,"
                            " not neighbor or child\r\n");
        }
      }
      break;

      // This command initiates a passthru bootloading of the first
      // device in the child table
    case 'C':
      {
        EmberEUI64 eui;
        EmberNodeType type;
        EmberStatus status;
        status = emberGetChildData(0, eui, &type);
        if (status != EMBER_SUCCESS) {
          emberSerialPrintf(APP_SERIAL, 
                            "ERROR: first slot in child table is empty\r\n");
          break;
        }

        bootloadMySleepyChild(eui);    
      }
      break;

    case 'Q': // send bootload query to neighbors
      {
        // sending a query broadcast (mac only - so just neighbors)
        // gets responses into bootloadUtilQueryResponseHandler
        EmberEUI64 addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
        bootloadUtilSendQuery(addr);
      }
      break;
#endif // USE_BOOTLOADER_LIB

      // print address table
    case 'a':
      // note that this uses SINK_ADDRESS_TABLE_SIZE for the address table
      // size instead of the usual EMBER_ADDRESS_TABLE_SIZE, as some entries 
      // are reserved for use by the trust center code. 
      // See "app/sensor/sensor-configuration.h" for more details.
      printAddressTable(SINK_ADDRESS_TABLE_SIZE);
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

    case 'x':
      printTokens();
      break;
    case 'j':
      jitMessageStatus();
      break;

    case 'c':
      printChildTable();
      break;

      // reset node
    case 'e':
      emberSerialGuaranteedPrintf(APP_SERIAL, "Resetting node...\r\n");
      halReboot();
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

    case '\n':
    case '\r':
      break;


    default:
      emberSerialPrintf(APP_SERIAL, "unknown cmd\r\n");
      break;
     }

    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\nsink-node>");
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
#if EMBER_SECURITY_LEVEL == 5  
  PRINT("k = print keys\r\n");
#endif //EMBER_SECURITY_LEVEL == 5
  PRINT("b = bootloader\r\n");
  PRINT("l = send multicast [hello]\r\n");
  PRINT("t = play tune\r\n");
  PRINT("a = print address table\r\n");
  PRINT("m = print multicast table\r\n");
  PRINT("f = send sink advertisement\r\n");
  PRINT("0 = button 0: turn allow join ON for 60 sec\r\n");
  PRINT("1 = button 1: leave ZigBee network\r\n");
  PRINT("e = reset node\r\n");
  PRINT("x = print node tokens\r\n");
  PRINT("c = print child table\r\n");
  PRINT("j = print JIT storage status\r\n");
  PRINT("B = attempt to bootload the device whose EUI is stored at"
        " location 0 of the address table\r\n");
  PRINT("C = attempt to bootload the first device in the child table\r\n");
  PRINT("Q = send out a BOOTLOADER_QUERY message\r\n");
  PRINT("* = switch the network key: send the key followed by a switch\r\n");
  PRINT("    key command 30 seconds later\r\n");
  PRINT("& = send a switch key command. This is needed only if the device\r\n");
  PRINT("    sent a new key and then reset before it was able to send the\r\n");
  PRINT("    switch key command\r\n");
#ifdef  PHY_BRIDGE
  PRINT("> = cycle to next Bridge TxControl\r\n");
  PRINT("< = cycle to next Bridge RxControl\r\n");
#ifdef  BRIDGE_TRACE
  PRINT("# = toggle Bridge debug tracing\r\n");
#endif//BRIDGE_TRACE
#endif//PHY_BRIDGE
}


// End utility functions
// *******************************************************************

