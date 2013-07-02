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
static void applicationTick(void);
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
  emberSerialPrintf(APP_SERIAL,
                    "channel 0x%x, panid 0x%2x, ",
                    networkParameters->radioChannel,
                    networkParameters->panId);
  printExtendedPanId(APP_SERIAL, networkParameters->extendedPanId);
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

// *******************************************************************
// Begin main application loop

void main(void)

{
  EmberStatus status;
  EmberResetType reset;
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  reset = halGetResetInfo();

  //
  //---

  // inititialize the serial port
  // good to do this before emberInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_38400, PARITY_NONE, 1);
  }

  // print the reason for the reset
  emberSerialGuaranteedPrintf(APP_SERIAL, "reset: %p\r\n",
                              (PGM_P)halGetResetString());

#if defined AVR_ATMEGA_128
  emberSerialGuaranteedPrintf(APP_SERIAL, "ATMEGA128L ");
#else
    #error Unknown AVR ATMEGA micro
#endif
  emberSerialGuaranteedPrintf(APP_SERIAL,
							  "Build on: "__TIME__" "__DATE__"\r\n");

  // emberInit must be called before other EmberNet stack functions
  status = emberInit(reset);
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

  // set LEDs
  appSetLEDsToInitialState();

  // init application state
  sensorInit();

  // print a startup message
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT: sensor app ");

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
    emberSerialPrintf(APP_SERIAL,
                      "SENSOR APP: joining network - ");
    printNetInfo(&parameters);
  } else {
    // were not able to join the old network
    emberSerialPrintf(APP_SERIAL,
         "SENSOR APP: push button 0 to join a network\r\n");
  }
  emberSerialWaitSend(APP_SERIAL);

  // event loop
  while(TRUE) {

    halResetWatchdog();

    emberTick();
    emberFormAndJoinTick();

#ifdef USE_MFG_CLI
    if (mfgMode) {
      mfgMode = mfgCmdProcessing();
    } else {
      processSerialInput();
    }
#else
    processSerialInput();
#endif

    // only blink LEDs if app is joined
    if (emberNetworkState() == EMBER_JOINED_NETWORK)
      applicationTick(); // check timeouts, buttons, flash LEDs
    else
      checkButtonEvents();

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

}

// this is called when the stack status changes
void emberStackStatusHandler(EmberStatus status)
{
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
  emberSerialGuaranteedPrintf(APP_SERIAL,
    "EVENT: could not find channel and panid - status: 0x%x\r\n", status);
}

void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{
  EmberNetworkParameters parameters;
  MEMCOPY(parameters.extendedPanId,
          networkFound->extendedPanId,
          EXTENDED_PAN_ID_SIZE);
  parameters.panId = networkFound->panId;
  parameters.radioTxPower = APP_POWER;
  parameters.radioChannel = networkFound->channel;
  emberJoinNetwork(EMBER_ROUTER, &parameters);
}

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
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

  #ifdef USE_BOOTLOADER_LIB
    bootloadUtilTick();
  #endif
  time = halCommonGetInt16uMillisecondTick();

  // Application timers are based on quarter second intervals, where each
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks.
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in
  // app/sensor/common.h.
  if ( (int16u)(time - lastBlinkTime) > TICKS_PER_QUARTER_SECOND ) {
    lastBlinkTime = time;

    // blink the LEDs
    appSetLEDsToRunningState();

    checkButtonEvents();
    // ********************************
    // check if we have waited long enough to send a response
    // of [sensor select sink] to the sink multicast advertisement
    // ********************************
    if (waitingToRespond == TRUE) {
      respondTimer = respondTimer - 1;
      if (respondTimer == 0) {
        handleSinkAdvertise(dataBuffer);
        waitingToRespond = FALSE;
        waitingForSinkReadyMessage = TRUE;
      }
    }

    // *******************************
    // if we are waiting for a sink ready message then
    // see if we have timed out waiting yet
    // *****************************
    if (waitingForSinkReadyMessage == TRUE) {
      timeToWaitForSinkReadyMessage = timeToWaitForSinkReadyMessage -1;

      if (timeToWaitForSinkReadyMessage == 0) {
        waitingForSinkReadyMessage = FALSE;
        emberSerialPrintf(APP_SERIAL,
                          "EVENT: didn't receive [sink ready] message,"
                          " not waiting any longer\r\n");
      }
    }

    // ********************************************/
    // if we are gathering data (we have a sink)
    // then see if it is time to send data
    // ********************************************/
    if (mainSinkFound == TRUE) {
      if (sendDataCountdown == SEND_DATA_RATE) {
        halStartAdcConversion(ADC_USER_APP2, ADC_REF_INT, TEMP_SENSOR_ADC_CHANNEL,
                              ADC_CONVERSION_TIME_US_256);
      }
      sendDataCountdown = sendDataCountdown - 1;
      if (sendDataCountdown == 0) {
        //emberSerialPrintf(APP_SERIAL, "sending data...\r\n");
        sendDataCountdown = SEND_DATA_RATE;
        sendData();
      }
    }

  }
}


void checkButtonEvents(void) {
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
          // use the settings from app/sensor/common.h
          networkParams.panId = APP_PANID;
          networkParams.radioTxPower = APP_POWER;
          networkParams.radioChannel = APP_CHANNEL;
          MEMCOPY(networkParams.extendedPanId,
                  extendedPanId,
                  EXTENDED_PAN_ID_SIZE);

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

}

// The sensor reads (or fabricates) data and sends it out to the sink
// There are four types of data that are sent:
// - Temperature data as BCD (binary coded decimal) - this is the default
// - Temperature data as a value
// - ADC Volts reading
// - random data
// The type of data sent depends on the dataMode variable.
void sendData(void) {
  EmberApsFrame apsFrame;
  int16u data;
  int16s fvolts;
  int32s tempC;
  int8u maximumPayloadLength;
  EmberStatus status;
  EmberMessageBuffer buffer;
  int8u i;
  int8u sendDataSize = SEND_DATA_SIZE;

  switch (dataMode) {
    default:
    case DATA_MODE_RANDOM:
      // get a random piece of data
      data = halCommonGetRandom();
      break;
    case DATA_MODE_VOLTS:
    case DATA_MODE_TEMP:
    case DATA_MODE_BCD_TEMP:
      if(halRequestAdcData(ADC_USER_APP2, &data) == EMBER_ADC_CONVERSION_DONE) {
        fvolts = halConvertValueToVolts(data / TEMP_SENSOR_SCALE_FACTOR);
        if (dataMode == DATA_MODE_VOLTS) {
          data = (int16u)fvolts;
        } else {
          tempC = voltsToCelsius(fvolts) / 100;
          if (dataMode == DATA_MODE_TEMP) {
            data = (int16u)tempC;
          } else {
            data = toBCD((int16u)tempC);
          }
        }
      } else {
        data = 0xFBAD;
      }
      break;
   }

#ifdef DEBUG
  emberDebugPrintf("sensor has data ready: 0x%2x \r\n", data);
#endif

  maximumPayloadLength = emberMaximumApsPayloadLength();

  // make sure the size of the data we are sending is not too large,
  // if it is too large then print a warning and chop it to a size
  // that fits. The max payload isn't a constant size since it
  // changes depending on if security is being used or not.
  if ((sendDataSize + EUI64_SIZE) > maximumPayloadLength) {

    // the payload is data plus the eui64
    sendDataSize = maximumPayloadLength - EUI64_SIZE;

    emberSerialPrintf(APP_SERIAL,
                   "WARN: SEND_DATA_SIZE (%d) too large, changing to %d\r\n",
                   SEND_DATA_SIZE, sendDataSize);
  }

  // sendDataSize must be an even number
  sendDataSize = sendDataSize & 0xFE;

  // the data - my long address and data
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);
  for (i=0; i<(sendDataSize / 2); i++) {
    globalBuffer[EUI64_SIZE + (i*2)] = HIGH_BYTE(data);
    globalBuffer[EUI64_SIZE + (i*2) + 1] = LOW_BYTE(data);
  }

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers(globalBuffer,
                                  EUI64_SIZE + sendDataSize);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
          "TX ERROR [data], OUT OF BUFFERS\r\n");
    return;
  }

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_DATA;            // the message we are sending
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY; // Default to retry
  //apsFrame.groupId = 0;        // multicast ID not used for unicasts
  //apsFrame.sequence = 0;       // the stack sets this to the seq num it uses


  // send the message
  status = emberSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                            SINK_ADDRESS_TABLE_INDEX,
                            &apsFrame,
                            buffer);

  if (status != EMBER_SUCCESS) {
    numberOfFailedDataMessages++;
    emberSerialPrintf(APP_SERIAL,
      "WARNING: SEND [data] failed, err 0x%x, failures:%x\r\n",
                      status, numberOfFailedDataMessages);

    // if too many data messages fails from sensor node then
    // invalidate the sink and find another one
    if (numberOfFailedDataMessages >= MISS_PACKET_TOLERANCE) {
      emberSerialPrintf(APP_SERIAL,
          "ERROR: too many data msg failures, looking for new sink\r\n");
      mainSinkFound = FALSE;
      numberOfFailedDataMessages = 0;
    }
  }

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  // print a status message
  emberSerialPrintf(APP_SERIAL,
                    "TX [DATA] status: 0x%x  data: 0x%2x / len 0x%x\r\n",
                    status, data, sendDataSize + EUI64_SIZE);
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
  EmberMessageBuffer buffer;

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
  //apsFrame.sequence = 0;                  // seq not used for multicast

  // send the message
  status = emberSendMulticast(&apsFrame, // multicast ID & cluster
                              10,        // radius
                              6,         // non-member radius
                              buffer);   // message to send

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  emberSerialPrintf(APP_SERIAL,
       "TX [multicast hello], status is 0x%x\r\n", status);
}


// this handles the sink advertisement message on the sensor
// the sensor responds with a SENSOR_SELECT_SINK message if it
// is not yet bound to a sink
void handleSinkAdvertise(int8u* data) {
  EmberApsFrame apsFrame;
  EmberStatus status;
  EmberMessageBuffer buffer;

  // save the EUI64 and short address of the sink so that is a mobile
  // node becomes a child of this node it can immediately inform the
  // mobile node who the parent sink is.
  MEMCOPY(sinkEUI, &(data[0]), EUI64_SIZE);
  sinkShortAddress = emberFetchLowHighInt16u(&(data[EUI64_SIZE]));

  status = emberSetAddressTableRemoteEui64(SINK_ADDRESS_TABLE_INDEX, sinkEUI);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL,
           "TX ERROR [sensor select sink], set remote EUI64 failure,"
           " status 0x%x\r\n",
           status);
    return;
  }
  emberSetAddressTableRemoteNodeId(SINK_ADDRESS_TABLE_INDEX, sinkShortAddress);

  emberSerialPrintf(APP_SERIAL,
                    "EVENT: sensor set address table entry %x to sink [",
                    SINK_ADDRESS_TABLE_INDEX);
  printEUI64(APP_SERIAL, &sinkEUI);
  emberSerialPrintf(APP_SERIAL, "]\r\n");

  // send a message picking this sink
  MEMCOPY(&(globalBuffer[0]), emberGetEui64(), EUI64_SIZE);

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers((int8u*)globalBuffer, EUI64_SIZE);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
        "TX ERROR can't send [sensor select sink], OUT OF BUFFERS\r\n");
    mainSinkFound = FALSE;
    return;
  }

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SENSOR_SELECT_SINK; // the message we are sending
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY; // Default to retry
  //apsFrame.groupId = 0;        // multicast ID not used for unicasts
  //apsFrame.sequence = 0;       // the stack sets this to the seq num it uses


  // send the message
  status = emberSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                            SINK_ADDRESS_TABLE_INDEX,
                            &apsFrame,
                            buffer);

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  emberSerialPrintf(APP_SERIAL,
                    "TX [sensor select sink], status:0x%x\r\n",
                    status);

  waitingForSinkReadyMessage = TRUE;
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


// init common state for sensor nodes
void sensorInit(void) {

  // init the global state
  mainSinkFound = FALSE;
  sendDataCountdown = SEND_DATA_RATE;
  numberOfFailedDataMessages = 0;
}


// *****************************
// for processing serial cmds
// *****************************
void processSerialInput(void) {
  int8u cmd;

  if(emberSerialReadByte(APP_SERIAL, &cmd) == 0) {
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\n");

    switch(cmd) {

      // info
    case 'i':
      printNodeInfo();
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

      // tune
    case 't':
#ifndef DEBUG
      halPlayTune_P(tune, 0);
      halPlayTune_P(tune, 0);
#endif
      break;


      // send random data
    case 'r':
      dataMode = DATA_MODE_RANDOM;
      emberSerialPrintf(APP_SERIAL, "Send random data\r\n");
      break;

      // send Temp data
    case 'T':
      dataMode = DATA_MODE_TEMP;
      emberSerialPrintf(APP_SERIAL, "Send temp data\r\n");
      break;

      // send volts data
    case 'v':
      dataMode = DATA_MODE_VOLTS;
      emberSerialPrintf(APP_SERIAL, "Send volts data\r\n");
      break;

      // send bcd temp data
    case 'd':
      dataMode = DATA_MODE_BCD_TEMP;
      emberSerialPrintf(APP_SERIAL, "Send bcd temp data\r\n");
      break;

      // read and print sensor data
    case 's':
      readAndPrintSensorData();
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
        emberSerialPrintf(APP_SERIAL, "INFO: attempt BL\r\n");
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
      printAddressTable(EMBER_ADDRESS_TABLE_SIZE);
      break;

      // print multicast table
      // this was removed for debug builds due to size constraints
    case 'm':
#ifndef DEBUG
      printMulticastTable(EMBER_MULTICAST_TABLE_SIZE);
#endif //DEBUG
      break;

      // hello - multicast msg
    case 'l':
      if (emberNetworkState() == EMBER_JOINED_NETWORK) {
        sendMulticastHello();
      } else {
        emberSerialPrintf(APP_SERIAL, "WARN: app not joined, can't send\r\n");
      }
      break;

      // 0 and 1 mean button presses
    case '0':
      buttonZeroPress = TRUE;
      break;
    case '1':
      buttonOnePress = TRUE;
      break;

      // reset node
    case 'e':
      emberSerialGuaranteedPrintf(APP_SERIAL, "Resetting node...\r\n");
      halReboot();
      break;

      // print the tokens
      // this was removed for debug builds due to size constraints
    case 'x':
#ifndef DEBUG
      printTokens();
#endif
      break;

    // print out the JIT storage
    case 'j':
      jitMessageStatus();
      break;

    // print out the child table
    case 'c':
      printChildTable();
      break;

#ifdef USE_MFG_CLI
    // ENTER MFG_MODE
    case 'M':
      mfgMode = TRUE;
      break;
#endif

    case '\n':
    case '\r':
      break;

    default:
      emberSerialPrintf(APP_SERIAL, "unknown cmd\r\n");
      break;
     }

    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\nsensor-node>");
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
  PRINT("b = send node into bootloader\r\n");
  PRINT("l = send multicast [hello]\r\n");
  PRINT("t = play tune\r\n");
  PRINT("a = print address table\r\n");
  PRINT("m = print multicast table\r\n");
  PRINT("0 = simulate button 0: join to network or if already joined,\r\n");
  PRINT("       turn allow join ON for 60 sec\r\n");
  PRINT("1 = simulate button 1: leave ZigBee network\r\n");
  PRINT("e = reset node\r\n");
  PRINT("x = print node tokens\r\n");
  PRINT("c = print child table\r\n");
  PRINT("j = print JIT storage status\r\n");
  PRINT("B = attempt to bootload the device whose EUI is stored at"
        " location 0 of the address table\r\n");
  PRINT("C = attempt to bootload the first device in the child table\r\n");
  PRINT("Q = send out a BOOTLOADER_QUERY message\r\n");
  PRINT("s = print sensor data\r\n");
  PRINT("r = send random data\r\n");
  PRINT("T = send Temp data\r\n");
  PRINT("v = send volts data\r\n");
  PRINT("d = send bcd Temp data\r\n");
#ifdef USE_MFG_CLI
  PRINT("M = enter MFGMODE\r\n");
#endif // ifdef MFG_CLI
}

// Function to read data from the ADC, do conversions to volts and
// BCD temp and print to the serial port

void readAndPrintSensorData()
{
  int16u value;
  int16s fvolts;
  int32s tempC, tempF;
  int8u str[20];
  EmberStatus readStatus;

  emberSerialPrintf(APP_SERIAL, "Printing sensor data...\r\n");
  halStartAdcConversion(ADC_USER_APP, ADC_REF_INT, TEMP_SENSOR_ADC_CHANNEL,
                        ADC_CONVERSION_TIME_US_256);
  emberSerialWaitSend(APP_SERIAL);
  readStatus = halReadAdcBlocking(ADC_USER_APP, &value);

  if( readStatus == EMBER_ADC_CONVERSION_DONE) {
    fvolts = halConvertValueToVolts(value / TEMP_SENSOR_SCALE_FACTOR);
    formatFixed(str, (int32s)fvolts, 5, 4, TRUE);
    emberSerialPrintf(APP_SERIAL, "ADC Voltage V = %s\r\n", str);

    tempC = voltsToCelsius(fvolts);
    formatFixed(str, tempC, 5, 4, TRUE);
    emberSerialPrintf(APP_SERIAL, "ADC temp = %s celsius, ", str);

    tempF = ((tempC * 9) / 5) + 320000L;
    formatFixed(str, tempF, 5, 4, TRUE);
    value = toBCD((int16u)(tempF / 100));
    emberSerialPrintf(APP_SERIAL, "%sF %2xF)\r\n", str, value);
    emberSerialWaitSend(APP_SERIAL);

  } else {
    emberSerialPrintf(APP_SERIAL, "ADC read error: 0x%x\r\n", readStatus);
    emberSerialWaitSend(APP_SERIAL);
  }
}

// Convert volts to celsius in LM20 temp sensor.  The numbers
// are both in fixed point 4 digit format. I.E. 860 is 0.0860

int32s voltsToCelsius (int16u voltage)
{
  // equation: temp = -0.17079*mV + 159.1887
  return 1591887L - (171 * (int32s)voltage);
}

int16u toBCD(int16u number)
{
  int8u numBuff[3];
  int8u i;

  for(i = 0; i < 3; i++) {
    numBuff[i] = number % 10;
    number /= 10;
  }
  number = (number << 4) + numBuff[2];
  number = (number << 4) + numBuff[1];
  number = (number << 4) + numBuff[0];

  return number;
}

void printDataMode(void)
{
  switch (dataMode) {
  case DATA_MODE_RANDOM:
    emberSerialPrintf(APP_SERIAL, "data mode [random]\r\n");
    break;
  case DATA_MODE_VOLTS:
    emberSerialPrintf(APP_SERIAL, "data mode [volts]\r\n");
    break;
  case DATA_MODE_TEMP:
    emberSerialPrintf(APP_SERIAL, "data mode [temp]\r\n");
    break;
  case DATA_MODE_BCD_TEMP:
    emberSerialPrintf(APP_SERIAL, "data mode [BCD temp]\r\n");
    break;
  default:
    emberSerialPrintf(APP_SERIAL, "data mode [unknown]\r\n");
    break;
  }
}

// End utility functions
// *******************************************************************
