// *******************************************************************
//  sensor.c
//
//  sample app for Ember Stack API
//
//  This is an example of a sensor application using the Ember ZigBee stack.
//  There are two types of nodes: sensor nodes, which collect data and
//  attempt to send it to a central point, and sink nodes, which are the
//  central collection point. In a real environment the sensor nodes
//  would collect their data from the environment around them: an electric
//  meter, a light panel, or a temperature sensor. In this example the
//  sensor nodes use a reading from the ADC attached to a temperature
//  sensor.
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
//  ('0') simulate button 0 press - attempts to join this node to the
//        network if it isn't already joined. If the node is joined to
//        the network then it turns permit join on for 60 seconds
//        this allows other nodes to join to this node
//  ('1') simulate button 1 press - leave the network
//  ('e') reset the node
//  ('B') attempts a passthru bootload of the first device in the address
//        table. This is meant to show how standalone bootloader is
//        integrated into an application and not meant as a full solution.
//        All necessary code is defined'ed under USE_BOOTLOADER_LIB.
//  ('C') attempts a passthru bootload of the first device in the child table
//  ('Q') sends a bootload query message.
//  ('s') print sensor data
//  ('r') send random data
//  ('T') send Temp data
//  ('v') send volts data
//  ('d') send bcd Temp data
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor/common.h"
#include "app/sensor/lcd.h"
#include <stdio.h>
#include "app/Driver/Timer.h"
#ifdef  PHY_BRIDGE
 #ifdef  CORTEXM3
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
// Ember endpoint and interface configuration

int8u emberEndpointCount = 1;
EmberEndpointDescription PGM endpointDescription = { PROFILE_ID, 0, };
EmberEndpoint emberEndpoints[] = {
  { ENDPOINT, &endpointDescription },
};

// End Ember endpoint and interface configuration
// *******************************************************************

// *******************************************************************
// Application specific constants and globals

int8u respondTimer = 0;

#define TIME_TO_WAIT_FOR_SINK_READY 40 // 10 seconds

// set to TRUE when a sink is found
boolean mainSinkFound = FALSE;
boolean waitingToRespond = FALSE;

EmberEUI64 sinkEUI;
EmberNodeId sinkShortAddress;

boolean waitingForSinkReadyMessage = FALSE;
int8u timeToWaitForSinkReadyMessage = TIME_TO_WAIT_FOR_SINK_READY;

int8u sendDataCountdown = SEND_DATA_RATE;

// "dataMode" controls what type of data the sensor sends to the sink.
// This can be changed using the serial input commands. The default
// is MODE_BCD_TEMP (Binary coded data temperature), which means the reading
// comes across as a human readable temperature number, for instance a
// reading of 0x3019 means 30.19 degress celsius.
// sensor sends random data

#define DATA_MODE_RANDOM   0   // send random data
#define DATA_MODE_VOLTS    1   // send the volts reading from the ADC
#define DATA_MODE_TEMP     2   // send the temp reading as a value
#define DATA_MODE_BCD_TEMP 3   // send the temp reading as binary coded data

// default to human readable format: i.e. 0x3019 means 30.19 celsius
int8u dataMode = DATA_MODE_BCD_TEMP;


// buffer for organizing data before we send a message
int8u globalBuffer[128];

// buffer for storing a message while we wait (random backoff) to
// send a response
int8u dataBuffer[20];

// constant string for temperature store
const char * adcTemp = "ADC Temp = ";
const char * adcTempUnit = "celsius";

// signed variable for adc temp raw value
int16s fvolts;

// Indicates if we are in MFG_MODE

#ifdef USE_MFG_CLI
boolean mfgMode = FALSE;
boolean  mfgCmdProcessing(void);
#endif

// for sensor nodes, number of data messages that have failed
int8u numberOfFailedDataMessages = 0;

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

// End application specific constants and globals
// *******************************************************************

// *******************************************************************
// Forward declarations
void printHelp(void);
void addMulticastGroup(void);
void processSerialInput(void);
void handleSinkAdvertise(int8u* data);
void sendData(void);
void sensorInit(void);
void checkButtonEvents(void);
void appSetLEDsToInitialState(void);
void appSetLEDsToRunningState(void);
int32s voltsToCelsius (int16u voltage);
int16u toBCD(int16u number);
void readAndPrintSensorData(void);

//
// *******************************************************************

void printNetInfo(EmberNetworkParameters * networkParameters)
{
  
}

// *******************************************************************
// Begin main application loop

void main(void)

{
  EmberStatus status;
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;

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

  emberSerialGuaranteedPrintf(APP_SERIAL,
							  "PWM Driver Example \r\nBuild on: "__TIME__" "__DATE__"\r\n");

  #ifdef USE_BOOTLOADER_LIB
    // Using the same port for application serial print and passthru
    // bootloading.  User must be careful not to print anything to the port
    // while doing passthru bootload since that can interfere with the data
    // stream.  Also the port's baud rate will be set to 115200 kbps in order
    // to maximize bootload performance.
    bootloadUtilInit(APP_SERIAL, APP_SERIAL);
  #endif

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  // sensor nodes need to be routers, so ensure we are a router
  if (((emberGetNodeType(&nodeType)) == EMBER_SUCCESS) &&
      (nodeType == EMBER_ROUTER) &&
      (emberNetworkInit() == EMBER_SUCCESS))
  {
    // were able to join the old network
    emberGetNetworkParameters(&parameters);
  } 

  /** timer initial. */
  pwm_init(500, 20);   //freq = 1000hz duty = 80%

  // event loop
  while(TRUE) {

    halResetWatchdog();
    emberTick();
    emberFormAndJoinTick();

    #ifdef DEBUG
      emberSerialBufferTick();   // Needed for debug which uses buffered serial
    #endif
  }
}


// when app is not joined, the LEDs are
// 1: ON
// 2: ON
// 3: OFF
void appSetLEDsToInitialState(void)
{
#ifndef NO_LED
  halSetLed(BOARDLED1);
  halSetLed(BOARDLED3);
  halClearLed(BOARDLED2);
#endif//NO_LED
}

// when app is joined, the LEDs are
// 1: ON
// 2: alternating ON/OFF with 3
// 3: alternating ON/OFF with 2
void appSetLEDsToRunningState(void)
{
#ifndef NO_LED
  halToggleLed(BOARDLED3);
  halToggleLed(BOARDLED2);
#endif//NO_LED
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
  // only check data messages, if other messages fail, the exchange
  // to connect the sensor with a sink will be attempted again
  if (apsFrame->clusterId == MSG_DATA) {

    // if the message failed, update fail counter and print a warning
    if (status != EMBER_SUCCESS) {
      numberOfFailedDataMessages++;
      emberSerialPrintf(APP_SERIAL,
         "WARNING: TX of [data] msg failed, consecutive failures:%x\r\n",
         numberOfFailedDataMessages);

      // if too many data messages fails from sensor node then
      // invalidate the sink and find another one
      if (numberOfFailedDataMessages >= MISS_PACKET_TOLERANCE) {
        emberSerialPrintf(APP_SERIAL,
           "ERROR: too many data msg failures, looking for new sink\r\n");
        mainSinkFound = FALSE;
        numberOfFailedDataMessages = 0;
      }
    }

    // if we successfully sent a data message then set the error
    // counter back to zero
    else {
      numberOfFailedDataMessages = 0;
    }
  }

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
#if 0
  // Called with an incoming message
  EmberEUI64 eui;
  int8u length = emberMessageBufferLength(message);

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
      emberSerialPrintf(APP_SERIAL, "RX BAD [sink advertise] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      return;
    }

    emberSerialPrintf(APP_SERIAL, "RX [sink advertise] from: ");
    printEUI64(APP_SERIAL, &eui);
    if ((mainSinkFound == FALSE) && (waitingForSinkReadyMessage == FALSE) &&
        (waitingToRespond == FALSE)) {
      waitingToRespond = TRUE;
      emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
      emberCopyFromLinkedBuffers(message, 0, dataBuffer, EUI64_SIZE + 2);

      // wait for 1 - 5 seconds before responding
      respondTimer = ((halCommonGetRandom()) % 17) + 4;
      timeToWaitForSinkReadyMessage = TIME_TO_WAIT_FOR_SINK_READY;
      emberSerialPrintf(APP_SERIAL,
                        "EVENT waiting %x ticks before reponding\r\n",
                        respondTimer);
    } else {
      emberSerialPrintf(APP_SERIAL, "; ignoring\r\n");
    }
    break;

  case MSG_SENSOR_SELECT_SINK:
    emberSerialPrintf(APP_SERIAL, "RX [sensor select sink] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; this is an ERROR]\r\n");
    break;

  case MSG_SINK_READY:
    emberSerialPrintf(APP_SERIAL, "RX [sink ready] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; will start sending data\r\n");
    waitingForSinkReadyMessage = FALSE;
    mainSinkFound = TRUE;
    sendDataCountdown = SEND_DATA_RATE;
    break;

  case MSG_SINK_QUERY:
    emberSerialPrintf(APP_SERIAL, "RX [sink query] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
    handleSinkQuery(emberGetSender());
    break;

  case MSG_DATA:
    emberSerialPrintf(APP_SERIAL, "RX [DATA] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; this is an ERROR\r\n");
    break;

  case MSG_MULTICAST_HELLO:
    // ignore own multicast advertisements
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
#endif
}

// this is called when the stack status changes
void emberStackStatusHandler(EmberStatus status)
{
#if 0
  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");
#ifndef USE_HARDCODED_NETWORK_SETTINGS
    {
      EmberStatus status;
      EmberNetworkParameters networkParams;
      status = emberGetNetworkParameters(&networkParams);
      if (status == EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL,
                          "SENSOR APP: network joined - ");
        printNetInfo(&networkParams);
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
    appSetLEDsToInitialState();
    break;

  case EMBER_JOIN_FAILED:
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "EVENT: stackStatus now EMBER_JOIN_FAILED\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : if using hardcoded network settings,\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : make sure the parent has the same network parameters\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : use '!' for parent to leave network and '1' to reset parent\r\n");
    appSetLEDsToInitialState();
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "EVENT: stackStatus now 0x%x\r\n", status);
  }
  emberSerialWaitSend(APP_SERIAL);
#endif
}

void emberSwitchNetworkKeyHandler(int8u sequenceNumber)
{
  emberSerialPrintf(APP_SERIAL, "EVENT: network key switched, new seq %x\r\n",
                    sequenceNumber);
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
  
}

void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{
  /*EmberNetworkParameters parameters;

  MEMSET(&parameters, 0, sizeof(EmberNetworkParameters));
  MEMCOPY(parameters.extendedPanId,
          networkFound->extendedPanId,
          EXTENDED_PAN_ID_SIZE);
  parameters.panId = networkFound->panId;
  parameters.radioTxPower = APP_POWER;
  parameters.radioChannel = networkFound->channel;
  parameters.joinMethod = EMBER_USE_MAC_ASSOCIATION;
  emberJoinNetwork(EMBER_ROUTER, &parameters);*/
}

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
}

// end Ember callback handlers
// *******************************************************************


// *******************************************************************
// Functions that use EmberNet

void checkButtonEvents(void) {
#if 0
  // structure to store necessary network parameters of the node
  // (which are panId, enableRelay, radioTxPower, and radioChannel)
  EmberNetworkParameters networkParams;
  EmberStatus status;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;

    // ********************************
    // button 0 is pressed
    // ********************************
    if (buttonZeroPress) {
      buttonZeroPress = FALSE;

      // if not joined with a network, join
      switch (emberNetworkState()) {
      case EMBER_NO_NETWORK:
        emberSerialPrintf(APP_SERIAL, "BUTTON0: join network\r\n");
        emberSerialWaitSend(APP_SERIAL);

        // Set the security keys and the security state - specific to this
        // application, all variants of this application (sink, sensor,
        // sleepy-sensor, mobile-sensor) need to use the same security setup.
        // This function is in app/sensor/common.c. This function should only
        // be called when a network is formed as the act of setting the key
        // sets the frame counters to 0. On reset and networkInit this should
        // not be called.
        sensorCommonSetupSecurity();

        #ifdef USE_HARDCODED_NETWORK_SETTINGS

          MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
          // use the settings from app/sensor/common.h
          networkParams.panId = APP_PANID;
          networkParams.radioTxPower = APP_POWER;
          networkParams.radioChannel = APP_CHANNEL;
          MEMCOPY(networkParams.extendedPanId,
                  extendedPanId,
                  EXTENDED_PAN_ID_SIZE);
          networkParams.joinMethod = EMBER_USE_MAC_ASSOCIATION;

          // tell the user what is going on
          emberSerialPrintf(APP_SERIAL,
                            "SENSOR APP: joining network - ");
          printNetInfo(&networkParams);

          // attempt to join the network
          status = emberJoinNetwork(EMBER_ROUTER,
                                    &networkParams);
          if (status != EMBER_SUCCESS) {
            emberSerialPrintf(APP_SERIAL,
              "error returned from emberJoinNetwork: 0x%x\r\n", status);
          } else {
            emberSerialPrintf(APP_SERIAL, "waiting for stack up...\r\n");
          }

          // the else case means we are NOT using hardcoded settings and are
          // picking a random PAN ID and channel and either using
          // APP_EXTENDED_PANID (from app/sensor/common.h) for the
          // extended PAN ID or picking a random one if APP_EXTENDED_PANID
          // is "0".
        #else

          // tell the user what is going on
          emberSerialPrintf(APP_SERIAL,
                            "SENSOR APP: scanning for channel and panid\r\n");

          // Use a function from app/util/common/form-and-join.c
          // that scans and selects a beacon that has:
          // 1) allow join=TRUE
          // 2) matches the stack profile that the app is using
          // 3) matches the extended PAN ID passed in unless "0" is passed
          // Once a beacon match is found, emberJoinableNetworkFoundHandler
          // is called.
          emberScanForJoinableNetwork(EMBER_ALL_802_15_4_CHANNELS_MASK,
                                      (int8u*) extendedPanId);

        #endif // USE_HARDCODED_NETWORK_SETTINGS
        break;

      // if in the middle of joining, do nothing
      case EMBER_JOINING_NETWORK:
        emberSerialPrintf(APP_SERIAL,
                          "BUTTON0: app already trying to join\r\n");
        break;

      // if already joined, turn allow joining on
      case EMBER_JOINED_NETWORK:
        emberSerialPrintf(APP_SERIAL,
                          "BUTTON0: turn permit join ON for 60 seconds\r\n");

        // turn allow join on
        emberPermitJoining(60);
        break;

      // if leaving, do nothing
      case EMBER_LEAVING_NETWORK:
        emberSerialPrintf(APP_SERIAL, "BUTTON0: app leaving, no action\r\n");
        break;
      }
    }

    // ********************************
    // button 1 is pressed
    // ********************************
    if (buttonOnePress) {
      buttonOnePress = FALSE;
      emberSerialPrintf(APP_SERIAL, "BUTTON1: leave network status 0x%x\r\n",
                        emberLeaveNetwork());
      mainSinkFound = FALSE;
      numberOfFailedDataMessages = 0;
    }
#endif
}

// The sensor reads (or fabricates) data and sends it out to the sink
// There are four types of data that are sent:
// - Temperature data as BCD (binary coded decimal) - this is the default
// - Temperature data as a value
// - ADC Volts reading
// - random data
// The type of data sent depends on the dataMode variable.
void sendData(void) {
  
}

// add the multicast group
void addMulticastGroup(void) {
  
}

// sendMulticastHello
void sendMulticastHello(void) {
  
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


// *********************************
// print help
// *********************************
#define PRINT(x) emberSerialPrintf(APP_SERIAL, x); emberSerialWaitSend(APP_SERIAL)



// Function to read data from the ADC, do conversions to volts and
// BCD temp and print to the serial port


// End utility functions
// *******************************************************************
