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
//  ('0') simulate button 0 press - attempts to join this node to the
//        network if it isn't already joined. If the node is joined to
//        the network then it turns permit join on for 60 seconds
//        this allows other nodes to join to this node
//  ('1') simulate button 1 press - leave the ZigBee network 
//  ('e') reset node
//  ('k') print the security keys
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor-host/common.h"

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

// Keep track of the current network state
static EmberNetworkStatus networkState = EMBER_NO_NETWORK;

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

// buffer for organizing data before we send a message
int8u globalBuffer[128];

// buffer for storing a message while we wait (random backoff) to
// send a response
int8u dataBuffer[20];

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

//
// *******************************************************************

// *******************************************************************
// Begin main application loop

int main(void)

{
  EmberStatus status;
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  //
  //---

  // inititialize the serial port
  // good to do this before ezspUtilInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }

  emberSerialPrintf(APP_SERIAL, "Reset is 0x%x:%p\r\n", halGetResetInfo(),
                    halGetResetString());

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
      assert(FALSE);
    }
  }

  // ezspUtilInit must be called before other EmberNet stack functions
  status = ezspUtilInit(APP_SERIAL);
  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
              "ERROR: ezspUtilInit 0x%x\r\n", status);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
    assert(FALSE);
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: ezspUtilInit passed\r\n");
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

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  // sensor nodes need to be routers, so ensure we are a router
  status = ezspGetNetworkParameters(&nodeType, &parameters);
  if ((status == EMBER_SUCCESS) &&
      (nodeType == EMBER_ROUTER) &&
      (emberNetworkInit() == EMBER_SUCCESS))
  {
    // were able to join the old network
    emberSerialPrintf(APP_SERIAL,
         "SENSOR APP: joining network - channel 0x%x, panid 0x%2x, ",
         parameters.radioChannel, parameters.panId);
    printExtendedPanId(APP_SERIAL, parameters.extendedPanId);
    emberSerialPrintf(APP_SERIAL, "\r\n");
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

    processSerialInput();

    // only blink LEDs if app is joined
    if (networkState == EMBER_JOINED_NETWORK)
      applicationTick(); // check timeouts, buttons, flash LEDs
    else
      checkButtonEvents();
  }

}


// when app is not joined, the LEDs are
// 1: ON
// 2: OFF
void appSetLEDsToInitialState(void)
{
  halSetLed(BOARDLED0);
  halClearLed(BOARDLED1);
}

// when app is joined, the LEDs are
// 1: alternating ON/OFF with 2
// 2: alternating ON/OFF with 1
void appSetLEDsToRunningState(void)
{
  halToggleLed(BOARDLED0);
  halToggleLed(BOARDLED1);
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
      MEMCOPY(dataBuffer, message, EUI64_SIZE + 2);

      // wait for 1 - 5 seconds before responding
      respondTimer = ((halCommonGetRandom()) % 17) + 4;
      timeToWaitForSinkReadyMessage = TIME_TO_WAIT_FOR_SINK_READY;
      emberSerialPrintf(APP_SERIAL,
                        "EVENT waiting %x ticks before responding\r\n",
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
    handleSinkQuery(sender);
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
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "RX [unknown (%2x)] from: ",
                      apsFrame->clusterId);
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; ignoring\r\n");
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
             "SENSOR APP: network joined - channel 0x%x, panid 0x%2x, ",
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
    appSetLEDsToInitialState();
    break;

  case EMBER_JOIN_FAILED:
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "EVENT: stackStatus now EMBER_JOIN_FAILED\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : if using hardcoded network settings,\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : make sure the parent is on the same channel & PAN ID\r\n");
    emberSerialGuaranteedPrintf(APP_SERIAL,
      "     : use '!' for parent to leave network and '1' to reset parent\r\n");
    appSetLEDsToInitialState();
    break;

  default:
    emberSerialPrintf(APP_SERIAL, "EVENT: stackStatus now 0x%x\r\n", status);
  }
  emberSerialWaitSend(APP_SERIAL);
}

// this is called when the stack receives a message to switch to a new
// network key
void ezspSwitchNetworkKeyHandler(int8u sequenceNumber)
{
  emberSerialPrintf(APP_SERIAL, "EVENT: network key switched, new seq %x\r\n",
                    sequenceNumber);
}

// called by form-and-join library
void emberScanErrorHandler(EmberStatus status)
{
  emberSerialPrintf(APP_SERIAL, "Scan error 0x%x", status);
}

// called by form-and-join library
void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{
  EmberNetworkParameters networkParams;
  int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;
  MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
  MEMCOPY(networkParams.extendedPanId, extendedPanId, EXTENDED_PAN_ID_SIZE);
  networkParams.panId = networkFound->panId;
  networkParams.radioTxPower = APP_POWER;
  networkParams.radioChannel = networkFound->channel;

  EmberStatus status = emberJoinNetwork(EMBER_ROUTER, &networkParams);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Join error 0x%x", status);
  }
}

// called by form-and-join library
void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{}

void halNcpIsAwakeIsr(boolean isAwake)
{}

void ezspErrorHandler(EzspStatus status)
{
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              "ERROR: ezspErrorHandler 0x%x\r\n",
                              status);
  assert(FALSE);
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

  time = halCommonGetInt16uMillisecondTick();

  // Application timers are based on quarter second intervals, where each 
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks. 
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in 
  // app/sensor-host/common.h.
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
      switch (networkState) {
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
        #if EMBER_SECURITY_LEVEL == 5
          sensorCommonSetupSecurity();
        #endif //EMBER_SECURITY_LEVEL == 5


        #ifdef USE_HARDCODED_NETWORK_SETTINGS
          networkParams.panId = APP_PANID;
          networkParams.radioTxPower = APP_POWER;
          networkParams.radioChannel = APP_CHANNEL;
          MEMCOPY(networkParams.extendedPanId, 
                  extendedPanId, 
                  EXTENDED_PAN_ID_SIZE);
		  networkParams.joinMethod = EMBER_USE_MAC_ASSOCIATION;
          emberSerialPrintf(APP_SERIAL,
               "SENSOR APP: joining network - channel 0x%x, panid 0x%2x, ",
               networkParams.radioChannel, networkParams.panId);
          printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
          emberSerialPrintf(APP_SERIAL, "\r\n");
          emberSerialWaitSend(APP_SERIAL);
          status = emberJoinNetwork(EMBER_ROUTER, 
                                    &networkParams); 
          if (status != EMBER_SUCCESS) {
            emberSerialPrintf(APP_SERIAL,
              "error returned from emberJoinNetwork: 0x%x\r\n", status);
          } else {
            emberSerialPrintf(APP_SERIAL, "waiting for stack up...\r\n");
          }
        #else
          emberSerialPrintf(APP_SERIAL,
               "SENSOR APP: scanning for channel and panid\r\n");
          emberScanForJoinableNetwork(EMBER_ALL_802_15_4_CHANNELS_MASK, 
                                      APP_EXTENDED_PANID);
        #endif
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

// The sensor fabricates data and sends it out to the sink
void sendData(void) {
  EmberApsFrame apsFrame;
  int16u data;
  int8u maximumPayloadLength;
  EmberStatus status;
  int8u i;
  int8u sendDataSize = SEND_DATA_SIZE;

  // get a random piece of data
  data = halCommonGetRandom();

  maximumPayloadLength = ezspMaximumPayloadLength();

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

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_DATA;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = ezspSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                           SINK_ADDRESS_TABLE_INDEX,
                           &apsFrame,
                           MSG_DATA,
                           EUI64_SIZE + sendDataSize,
                           globalBuffer,
                           &apsFrame.sequence);

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

  // print a status message
  emberSerialPrintf(APP_SERIAL,
                    "TX [DATA] status: 0x%x  data: 0x%2x / len 0x%x\r\n",
                    status, data, sendDataSize + EUI64_SIZE);
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
       "TX [multicast hello], status is 0x%x\r\n", status);
}


// this handles the sink advertisement message on the sensor
// the sensor responds with a SENSOR_SELECT_SINK message if it
// is not yet bound to a sink
void handleSinkAdvertise(int8u* data) {
  EmberApsFrame apsFrame;
  EmberStatus status;

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

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SENSOR_SELECT_SINK;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = ezspSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                           SINK_ADDRESS_TABLE_INDEX,
                           &apsFrame,
                           MSG_SENSOR_SELECT_SINK,
                           EUI64_SIZE,
                           globalBuffer,
                           &apsFrame.sequence);

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

      // tune
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

      // hello - multicast msg
    case 'l':
      if (networkState == EMBER_JOINED_NETWORK) {
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

      // causes the node to reset
    case 'e':
      emberSerialGuaranteedPrintf(APP_SERIAL, "EVENT: Resetting node...\r\n");
      halReboot();
      break;

      // print the keys
    case 'k':
      sensorCommonPrintKeys();
      break;

    // print out the child table
    case 'c':
      printChildTable();
      break;

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
#ifndef NO_BOOTLOADER
  PRINT("b = send node into bootloader\r\n");
#endif
  PRINT("l = send multicast [hello]\r\n");
  PRINT("t = play tune\r\n");
  PRINT("a = print address table\r\n");
  PRINT("m = print multicast table\r\n");
  PRINT("0 = simulate button 0: join to network or if already joined,\r\n");
  PRINT("       turn allow join ON for 60 sec\r\n");
  PRINT("1 = simulate button 1: leave ZigBee network\r\n");
  PRINT("e = reset node\r\n");
  PRINT("k = print the security keys\r\n");
  PRINT("c = print child table\r\n");
}


// End utility functions
// *******************************************************************
