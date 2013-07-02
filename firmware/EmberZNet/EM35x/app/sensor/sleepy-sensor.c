// *******************************************************************
//  sleepy-sensor.c
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
//  ('t') makes the node play a tune. Useful in identifying a node.
//  ('a') prints the address table of the node
//  ('m') prints the multicast table of the node
//  ('l') tells the node to send a multicast hello packet.
//  ('i') prints info about this node including channel, power, and app
//  ('b') puts the node into the bootloader menu (as an example).
//  ('s') sleep even when not joined to the network
//  ('0') simulate button 0 press - attempts to join this node to the
//        network if it isn't already joined. If the node is joined to
//        the network then it turns permit join on for 60 seconds
//        this allows other nodes to join to this node
//  ('1') simulate button 1 press - leave the network
//  ('e') reset the node
//  ('T') send test data
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor/common.h"

// *******************************************************************
// define the node type - sleepy or mobile
//
// EMBER_SLEEPY_END_DEVICE is an end device that can sleep
// EMBER_MOBILE_END_DEVICE is a sleepy end device that is meant to move.
// Mobile nodes are timed out of their parents child table aggressively.
// If a mobile node does not poll in EMBER_MOBILE_NODE_POLL_TIMEOUT
// quarter seconds, then it is removed. Default value is 20 quarter
// seconds = 5 seconds. Sleepy nodes are not removed from their parent's
// child table unless the parent gets a disassociation notification from
// the child (child leaves the network) or the parent gets an end device
// announcement that matches the child in it's child table (meaning the
// child moved to another parent.

#if defined(SLEEPY_SENSOR_APP)
  EmberNodeType applicationType = EMBER_SLEEPY_END_DEVICE;

#elif defined(MOBILE_SENSOR_APP)
  EmberNodeType applicationType = EMBER_MOBILE_END_DEVICE;

#else
  #error must be SLEEPY_SENSOR_APP or MOBILE_SENSOR_APP
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
// Application specific constants and globals

#define TIME_TO_WAIT_FOR_SINK_READY 80 // 20 seconds
#define TIME_TO_WAIT_FOR_SINK_ADVERTISE 40 // 10 seconds

// TRUE when a sink is found
boolean mainSinkFound = FALSE;
// TRUE when waiting for a transport ACK -  we must nap and poll to get it
boolean waitingForDataAck = FALSE;
// TRUE when waiting to receive a sink_ready message, this prevents the sensor
// from responding to other sink advertisement meassages
boolean waitingForSinkReadyMessage = FALSE;
int8u timeToWaitForSinkReadyMessage = TIME_TO_WAIT_FOR_SINK_READY;
// TRUE when waiting to receive a sink_advertise message, this prevents the
// sensor from resending the sink_query
boolean waitingForSinkAdvMessage = FALSE;
int8u timeToWaitForSinkAdvMessage = TIME_TO_WAIT_FOR_SINK_ADVERTISE;

int8u sendDataCountdown = SEND_DATA_RATE;

// buffer for organizing data before we send a datagram
int8u globalBuffer[128];

// for sensor nodes, number of data messages that have failed
int8u numberOfFailedDataMessages = 0;

boolean buttonZeroPress = FALSE;  // for button push (see halButtonIsr)
boolean buttonOnePress = FALSE;  // for button push (see halButtonIsr)

// controls if the node sleeps when it isn't part of a network
boolean sleepWhenNotJoined = FALSE;

// keeps track of if data was received in the last poll
boolean lastPollGotData;

// defines to make the code easier to read
#define NAP       0
#define HIBERNATE 1

// keeps track of how long the last sleep was
int16u lastSleepDuration = 0;

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

// if we are sleepy and get into a state where our parent is not responding
// we look for a new parent by attempting a secure rejoin. If the secure
// rejoin fails, we attempt to do an unsecure rejoin. This variable is the
// timer that determines if we attempt the unsecure rejoin. This variable
// is cleared if emberStackStatus handler gets a status of EMBER_NETWORK_UP
// (if the first rejoin worked). It is counted down in appTick and this
// is where the second rejoin is called from
int8u findAndRejoinTimer = 0;
#define FIND_AND_REJOIN_TIMEOUT 50

#define DATA_MODE_DEFAULT 0
#define DATA_MODE_TEST    1
int8u dataMode = DATA_MODE_DEFAULT;

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
void sendSinkQuery(void);
void sensorInit(void);
void checkButtonEvents(void);
void appSetLEDsToInitialState(void);
void appSetLEDsToRunningState(void);
void appEventHandler(void);

extern boolean emRadioAlwaysUseZeroBackoff;

// End forward declarations
// *******************************************************************

// *******************************************************************
// Application events

EmberEventControl appEvent;
EmberTaskId appTask;

EmberEventData appEvents[] = {
  { &appEvent,  appEventHandler },
  { NULL, NULL }
};

// End application events
// *******************************************************************

// *******************************************************************
// Begin main application loop

void main(void)

{
  EmberStatus status;
  EmberNodeType nodeType;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  //
  //---

  // inititialize the serial port
  // good to do this before emberInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }

  // print the reason for the reset
  emberSerialGuaranteedPrintf(APP_SERIAL, "reset: %p\r\n",
                              (PGM_P)halGetResetString());

#if defined CORTEXM3_EM357
  emberSerialGuaranteedPrintf(APP_SERIAL, "EM357 ");
#elif defined CORTEXM3_EM351
  emberSerialGuaranteedPrintf(APP_SERIAL, "EM351 ");
#else
    #error Unknown CORTEXM3 micro
#endif
  emberSerialGuaranteedPrintf(APP_SERIAL,
							  "Build on: "__TIME__" "__DATE__"\r\n");

  // emberInit must be called before other EmberZNet stack functions
  status = emberInit();
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
  emberFormAndJoinTaskInit();
  appTask = emberTaskInit(appEvents);
  emberEventControlSetDelayMS(appEvent, TICKS_PER_QUARTER_SECOND);

  // set LEDs
  appSetLEDsToInitialState();

  // init application state
  sensorInit();

  #ifdef USE_BOOTLOADER_LIB
    // Using the same port for application serial print and passthru
    // bootloading.  User must be careful not to print anything to the port
    // while doing passthru bootload since that can interfere with the data
    // stream.  Also the port's baud rate will be set to 115200 kbps in order
    // to maximize bootload performance.
    bootloadUtilInit(APP_SERIAL, APP_SERIAL);
  #endif // USE_BOOTLOADER_LIB

  // print a startup message
#if defined(SLEEPY_SENSOR_APP)
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT: sleepy-sensor app ");
#else
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT: mobile-sensor app ");
#endif

  printEUI64(APP_SERIAL, (EmberEUI64*) emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  emberSerialGuaranteedPrintf(APP_SERIAL,"RESET:%p\r\n",
                              (PGM_P)halGetResetString());

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's old network
  if (((emberGetNodeType(&nodeType)) == EMBER_SUCCESS) &&
      (nodeType == applicationType) &&
      (emberNetworkInit() == EMBER_SUCCESS))
  {
    // emberNetworkInit returning success on an end device means an orphan
    // scan was initiated. We can't use emberGetNetworkParameters yet (to
    // print the network params) until the emberStackStatusHandler returns
    // a status of EMBER_JOINED_NETWORK
    emberSerialPrintf(APP_SERIAL,
         "SENSOR APP: joining network - finding old parent...\r\n");
  } else {
    // were not able to call emberNetworkInit or it returned a failed status
    emberSerialPrintf(APP_SERIAL,
         "SENSOR APP: push button 0 to join a network\r\n");
  }
  emberSerialWaitSend(APP_SERIAL);

  emberTaskEnableIdling(TRUE);

  // event loop
  while(TRUE) {

    halResetWatchdog();

    emberTick();
    emberFormAndJoinRunTask();

    processSerialInput();

    applicationTick(); // Power save, watch timeouts

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
    // we aren't waiting for an ack any more
    waitingForDataAck = FALSE;

    // if the message failed, update fail counter and print a warning
    if (status != EMBER_SUCCESS) {
      numberOfFailedDataMessages++;
      emberSerialPrintf(APP_SERIAL,
         "WARNING: TX of [data] %x%x msg failed, consecutive failures:%x\r\n",
         emberGetLinkedBuffersByte(message, EUI64_SIZE + 0),
         emberGetLinkedBuffersByte(message, EUI64_SIZE + 1),
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
      if(dataMode != DATA_MODE_TEST) {
        emberSerialPrintf(APP_SERIAL, "RX ACK for [DATA]\r\n");
      }
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
  // this device never sends a query so it doesn't have to handle
  // the query response

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
      emberSerialPrintf(APP_SERIAL, "RX too short [sink advertise] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      return;
    }

    emberSerialPrintf(APP_SERIAL, "RX [sink advertise] from: ");
    printEUI64(APP_SERIAL, &eui);
    if ((mainSinkFound == FALSE) && (waitingForSinkReadyMessage == FALSE)) {
      int8u dataBuffer[EUI64_SIZE + 2];
      waitingForSinkAdvMessage = FALSE;
      emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
      emberCopyFromLinkedBuffers(message, 0, dataBuffer, EUI64_SIZE + 2);
      handleSinkAdvertise(dataBuffer);
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
#ifndef MOBILE_SENSOR_APP
    sendDataCountdown = SEND_DATA_RATE;
#else
    // send immediately for mobile node
    sendDataCountdown = 1;
#endif
    break;

  case MSG_SINK_QUERY:
    emberSerialPrintf(APP_SERIAL, "RX [sink query] from: ");
    printEUI64(APP_SERIAL, &eui);
    emberSerialPrintf(APP_SERIAL, "; this is an ERROR]\r\n");
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
        emberSerialPrintf(APP_SERIAL,
             "SENSOR APP: network joined - channel 0x%x, panid 0x%2x, ",
             networkParams.radioChannel, networkParams.panId);
        printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
        emberSerialPrintf(APP_SERIAL, "\r\n");
        emberSerialWaitSend(APP_SERIAL);
      }
    }

    // reset the timer that attempts a 2nd rejoin if the 1st rejoin fails
    // since the 1st rejoin has just succeeded
    findAndRejoinTimer = 0;

    // Add the multicast group to the multicast table - this is done
    // after the stack comes up
    addMulticastGroup();
    break;

  case EMBER_NETWORK_DOWN:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_DOWN\r\n");
    appSetLEDsToInitialState();
    mainSinkFound = FALSE;
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

  case EMBER_MOVE_FAILED:
    emberSerialPrintf(APP_SERIAL,
           "EVENT: stackStatus now MOVE_FAILED\r\n");
    appSetLEDsToInitialState();
    emberLeaveNetwork();
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

  MEMSET(&parameters, 0, sizeof(EmberNetworkParameters));
  MEMCOPY(parameters.extendedPanId,
          networkFound->extendedPanId,
          EXTENDED_PAN_ID_SIZE);
  parameters.panId = networkFound->panId;
  parameters.radioTxPower = APP_POWER;
  parameters.radioChannel = networkFound->channel;
  parameters.joinMethod = EMBER_USE_MAC_ASSOCIATION;
  emberJoinNetwork(applicationType, &parameters);
}

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
}

// keep track of the number of quarter seconds that a parent
// does not respond to a poll -- if a parent does not respond for
// 60 seconds (240 quarter seconds) then the child tries to find
// a new parent. This variable needs to handle numbers as large as
// 299, since polls missed after hibernating add 60, and the previous
// value could be as high as 239.
static int16u numQsParentGone = 0;

// this is called when a poll has completed. The app keeps track
// of various state using this function: if the last poll received
// data, and how many quarter-seconds it has been since the parent has
// responded to a poll.
void emberPollCompleteHandler(EmberStatus status)
{

  switch (status) {
  case EMBER_SUCCESS:
    if(dataMode != DATA_MODE_TEST) {
      emberSerialPrintf(APP_SERIAL, "poll: got data\r\n");
    }
    numQsParentGone = 0;
    lastPollGotData = TRUE;
    break;
  case EMBER_MAC_NO_DATA:
    if(dataMode != DATA_MODE_TEST) {
      emberSerialPrintf(APP_SERIAL, "poll: no data\r\n");
    }
    numQsParentGone = 0;
    lastPollGotData = FALSE;
    break;
  case EMBER_DELIVERY_FAILED:
    if(dataMode != DATA_MODE_TEST) {
      emberSerialPrintf(APP_SERIAL, "poll: failed to send\r\n");
    }
    lastPollGotData = FALSE;
    break;
  case EMBER_MAC_NO_ACK_RECEIVED:
    numQsParentGone = numQsParentGone + lastSleepDuration;
    emberSerialPrintf(APP_SERIAL, "poll: no parent %2x\r\n", numQsParentGone);
    lastPollGotData = FALSE;
    // hibernates are default length 20 seconds, naps are 1 second,
    // parent needs to be unresponsive to polls for 60 seconds before
    // we consider them gone.
    if (numQsParentGone > 239) {
      status = emberRejoinNetwork(TRUE);  // Assume we have the current NWK Key
      emberSerialPrintf(APP_SERIAL, "rejoin: %x\r\n", status);
      // set a timer to try a rejoin insecurely if we dont get a
      // stackStatus call of network up in a reasonable amount of time
      findAndRejoinTimer = FIND_AND_REJOIN_TIMEOUT;
    }
    break;
  }
  emberSerialWaitSend(APP_SERIAL);
}

// end Ember callback handlers
// *******************************************************************


// *******************************************************************
// Application functions dealing with sleeping.
// These are called from the heartbeat function


// handle making the poll call and print if we get an error
void appPoll(void)
{
  EmberStatus status;
  status = emberPollForData();

  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "poll: 0x%x\r\n", status);
    emberSerialWaitSend(APP_SERIAL);
  }
}


// handle going to sleep, the sleep duration is passed in.
// Keeps track of how long the node actually slept (because an
// interrupt could wake the node before the time specified) in
// the global lastSleepDuration, used by emberPollCompleteHandler
// to determine when a parent is unresponsive.
void sensorFullSleep(int32u* sleepDuration, int8u type)
{
  EmberStatus status;
  int32u desiredSleepDuration = (*sleepDuration);

  assert(INTERRUPTS_ARE_OFF());

  // do not display message if we are in test mode since this can effect
  // power consumption
  if(dataMode != DATA_MODE_TEST) {
    // print a message which type of sleep
    if (type == NAP) {
      emberSerialGuaranteedPrintf(APP_SERIAL, "EVENT: napping...");
    }
    if (type == HIBERNATE) {
      emberSerialGuaranteedPrintf(APP_SERIAL, "EVENT: hibernating...");
    }
  }

  // turn off the radio
  emberStackPowerDown();
  // turn off board and peripherals
  halPowerDown();
  // turn micro to power save mode - wakes on external interrupt
  // or when the time specified expires
  status = halSleepForQuarterSeconds(sleepDuration);
  // power up board and peripherals
  halPowerUp();
  // power up radio
  emberStackPowerUp();

  // keep track of how long we slept for the purpose of
  // knowing how long our parent has been unresponsive
  // see code in emberPollCompleteHandler
  lastSleepDuration = (int16u) (desiredSleepDuration - (*sleepDuration));

  if(dataMode != DATA_MODE_TEST) {
    // message to know we are awake
    emberSerialGuaranteedPrintf(APP_SERIAL, "wakeup %x\r\n", status);
  }
}

// End Application functions dealing with sleeping.
// *******************************************************************


// *******************************************************************
// Application functions for event handling and messaging.


// applicationTick - called to check event timeouts and sleep the device
//
// sleeping is handled in this function. There are two modes of sleeping
// 1. napping - device sleeps for 1 second intervals. The device can
//    receive normal transport messages reliably.
// 2. hibernating - device sleeps for a number of seconds equal to the
//   rate at which data is sent (SEND_DATA_RATE). Hibernating devices cannot
//   reliably receive transport messages, but they can reliably receive
//   JIT (Just In Time) messages or Alarm messages. This application
//   only makes use of JIT messages (does not use Alarms).
//
static void applicationTick(void) {
  // sleepDurationAttempted is how long the device wanted to sleep
  int32u sleepDurationAttempted = 0;
  // after sleeping, sleepDuration is how long the device actually slept
  int32u sleepDuration = 0;

  // ******************************
  // We need to nap (not hibernate) if we are waiting on events.
  // There are 4 events we wait on:
  // 1. (mainSinkFound == FALSE) or
  // 2. (waitingForSinkReadyMessage == TRUE) or
  // 3. (waitingForDataAck == TRUE) or
  // 4. (lastPollGotData == TRUE)
  //
  // mainSinkFound == FALSE means that the node is waiting for a
  // SINK_ADVERTISE
  //
  // waitingForSinkReady message means the application is waiting on
  // the sink to send a SINK_READY in response to the SENSOR_SELECT_SINK
  // and will wait for up to 10 seconds for SINK_READY
  //
  // waitingForDataAck means the device has sent a MSG_DATA transport
  // message and needs to poll soon to pick up the ack.
  //
  // lastPollGotData means that the device got data on it's last poll
  // and must poll again soon since there could be a second JIT message
  // waiting on the parent. The app is structured such that a single JIT
  // message is sent in response to one poll and the child is responsible
  // for polling again until it gets no data.
  //
  // napping is done in 1 second intervals
  //
  // Hibernating is done based in the SEND_DATA_RATE (default 20 sec)
  // napping is done in 1 second intervals -- this allows the device to:
  // 1. reliably receive transport messages (for receiving DATA acks and
  //    SINK_READY messages)
  // 2. respond to SINK_ADVERTISEMENTS in a reasonable time while still
  //    performing a backoff (from 1-5 seconds)
  // 3. poll again if it received data to see if it's parent has another
  //    message to send to it.
  //
  // Hibernating while joined is done based in the SEND_DATA_RATE (default
  // 20 seconds). When the sensor has a sink and is sending data, it only
  // needs to wake up to fabricate data and send it to the sink. So the
  // sleep time and the send time are the same.  If the sensor does not
  // have a sink, it could wake at any interval to see if it can find one.
  // waking up in 20 second intervals simplifies the code and ensures the
  // sensor checks in with it's parent often.
  // ******************************

  #ifdef USE_BOOTLOADER_LIB
    bootloadUtilTick();
  #endif // USE_BOOTLOADER_LIB

  // this device normally only sleeps when it is joined to the network.
  // To force the device sleep when it is not joined to a network use
  // the serial command ('s'). This application does not sleep when not
  // joined (by default) to allow the user to interact with the device
  // using the serial port. Once the device is sleeping, interaction with
  // the serial port is nearly impossible.
  if (emberNetworkState() == EMBER_JOINED_NETWORK)
  {
    // if we are waiting on events then we nap, otherwise hibernate
    INTERRUPTS_OFF();  // interrupts are re-enabled by sleeping or idling
    if ((mainSinkFound == FALSE) ||
        (waitingForSinkReadyMessage == TRUE) ||
        (waitingForDataAck == TRUE) ||
        (lastPollGotData == TRUE))
    {
      // **************************
      // napping
      sleepDurationAttempted = 4; // 1 second
      sleepDuration = sleepDurationAttempted;
      if(emberOkToNap() && !emberSerialWriteUsed(APP_SERIAL))
      {
        // sleep and then poll when we wake up
        sensorFullSleep(&sleepDuration, NAP);
        appPoll();
      } else {
        emberMarkTaskIdle(appTask);
        // since ideally we want to deep sleep make sure no other task
        //   allows the processor to idle before we get to check if its
        //   ok to deep sleep again or not.
        emberMarkTaskActive(appTask);
      }
    }

    // **************************
    // we need to hibernate
    else {
      // sleepy sensors hibernate in between sending data
#ifndef MOBILE_SENSOR_APP
      if(dataMode == DATA_MODE_TEST) {
        sleepDurationAttempted = (SEND_DATA_RATE/2);
      } else {
        sleepDurationAttempted = SEND_DATA_RATE;
      }
#else
      // When mobile sensors wake up they will need to find a sink
      // again, and when they do that they will send data immediately.
      sleepDurationAttempted = TIME_BEFORE_SINK_ADVERTISE;
#endif
      sleepDuration = sleepDurationAttempted;
      // emberOkToHibernate checks emberOkToNap already
      if (emberOkToHibernate() && !emberSerialWriteUsed(APP_SERIAL))
      {
        // sleep and then poll when we wake up
        sensorFullSleep(&sleepDuration, HIBERNATE);
        appPoll();
        // add additional sleep before sending data in test mode
        if(dataMode == DATA_MODE_TEST) {
          // Use a separate loop so that we can prevent the normal app events
          //  from running.  However, in order to properly idle the processor
          //  we must declare the event inactive, while still run the task.
          emberEventControlSetInactive(appEvent);
          while(TRUE) {
            halResetWatchdog();
            emberTick();
            emberRunTask(appTask);

            INTERRUPTS_OFF();
            if(emberOkToHibernate()) {
              sleepDuration = sleepDurationAttempted;
              sensorFullSleep(&sleepDuration, HIBERNATE);
              break;
            } else {
              emberMarkTaskIdle(appTask);
              // since ideally we want to deep sleep make sure no other task
              //   allows the processor to idle before we get to check if its
              //   ok to deep sleep again or not.
              emberMarkTaskActive(appTask);
            }
          }
          // Now we can allow the normal app events to run immediately.
          emberEventControlSetActive(appEvent);
        }
      } else {
        emberMarkTaskIdle(appTask);
        // since ideally we want to deep sleep make sure no other task
        //   allows the processor to idle before we get to check if its
        //   ok to deep sleep again or not.
        emberMarkTaskActive(appTask);
      }
    }
  }

  // if we are not joined and should sleep (which is turned on with
  // a serial cmd) then sleep here. We still check emberOkToHibernate()
  // since it is possible that the stack is performing an energy or active
  // scan during this time, in which case the NetworkStatus is still
  // NO_NETWORK, but the MAC is actively doing something, so putting the
  // device to sleep without checking emberOkToHibernate() will cause the
  // MAC to assert in zigbee-stack.c in emberStackPowerDown(). The
  // application sleeps more aggressively here, hard coding
  // to 60 seconds and not basing the sleep time on the SEND_DATA_RATE
  ATOMIC(
    if ((emberNetworkState() == EMBER_NO_NETWORK) &&
        (sleepWhenNotJoined == TRUE) &&
        emberOkToHibernate())
    {
      sleepDurationAttempted = 240; // 60 seconds
      sleepDuration = sleepDurationAttempted;
      sensorFullSleep(&sleepDuration, HIBERNATE);
    }
  )

  emberRunTask(appTask);
}


void appEventHandler(void)
{
  static int16u lastRunTime = 0;
  int16u thisRunTime;

  // After the device sleeps, application ticks need to be taken off of
  // the timers that handle events in an amount that equals the amount of
  // time the device slept. This variable is used to keep track of the
  // number of quarter-seconds that the device successfully slept
  // (interrupts could wake it before it was done)
  int8u qsTicksToTakeOff;

  // Application timers are based on quarter second intervals, where each
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks.
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in
  // app/sensor/common.h.
  emberEventControlSetDelayMS(appEvent, TICKS_PER_QUARTER_SECOND);

  // we may have been asleep and not run for a while.  Calculate how long
  //  it has been since we last ran.
  thisRunTime = halCommonGetInt16uQuarterSecondTick();
  qsTicksToTakeOff = elapsedTimeInt16u(lastRunTime, thisRunTime);
  lastRunTime = thisRunTime;

  // blink the LEDs
  if (emberNetworkState() == EMBER_JOINED_NETWORK)
    appSetLEDsToRunningState();

  // check for button events
  checkButtonEvents();

  // always take off at least 1 tick
  if (qsTicksToTakeOff == 0)
    qsTicksToTakeOff = 1;

  // *******************************
  // if we are waiting for a sink advertise message then
  // see if we have timed out waiting yet
  // *****************************
  if (waitingForSinkAdvMessage == TRUE) {
    // make sure we don't send the timer negative
    if (qsTicksToTakeOff > timeToWaitForSinkAdvMessage)
      qsTicksToTakeOff = timeToWaitForSinkAdvMessage;
    timeToWaitForSinkAdvMessage = timeToWaitForSinkAdvMessage -
                                          qsTicksToTakeOff;

    if (timeToWaitForSinkAdvMessage == 0) {
      waitingForSinkAdvMessage = FALSE;
      emberSerialPrintf(APP_SERIAL,
                        "EVENT: didn't receive [sink advertise] message,");
      emberSerialPrintf(APP_SERIAL, "resending sink query\r\n");
      emberSerialWaitSend(APP_SERIAL);
    }
  }

  // *******************************
  // if we are waiting for a sink ready message then
  // see if we have timed out waiting yet
  // *****************************
  if (waitingForSinkReadyMessage == TRUE) {
    // make sure we don't send the timer negative
    if (qsTicksToTakeOff > timeToWaitForSinkReadyMessage)
      qsTicksToTakeOff = timeToWaitForSinkReadyMessage;
    timeToWaitForSinkReadyMessage = timeToWaitForSinkReadyMessage -
                                          qsTicksToTakeOff;

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
    // make sure we don't send the timer negative
    if (qsTicksToTakeOff > sendDataCountdown)
      qsTicksToTakeOff = sendDataCountdown;

    sendDataCountdown = sendDataCountdown - qsTicksToTakeOff;
    if (sendDataCountdown == 0) {
      sendDataCountdown = SEND_DATA_RATE;
      // don't send if out parent has been gone for 2 hibernate
      // durations. If we send and then move without getting the
      // ack the stack will not return TRUE from emberOkToNap
      if (numQsParentGone < 161) {
        sendData();
      }
    }
  } else {
    // ********************************************/
    // We don't have a sink yet - check to see if
    // we need to query for it.
    // ********************************************/
    if ((emberNetworkState() == EMBER_JOINED_NETWORK) &&
        (waitingForSinkAdvMessage == FALSE) &&
        (waitingForSinkReadyMessage == FALSE)) {
      sendSinkQuery();
    }
  }

  // **************************************
  // if we get into a state where our parent is not responding, we look
  // for a new parent. We try to rejoin securely, if that fails (meaning
  // we dont get an emberStackStatusHandler call with status NETWORK_UP)
  // we attempt an insecure rejoin
  // **************************************
  if (findAndRejoinTimer > 0) {
    findAndRejoinTimer--;
    if (findAndRejoinTimer == 0) {
      // try unsecure since secure failed
      EmberStatus status = emberRejoinNetwork(FALSE);
      emberSerialPrintf(APP_SERIAL,
                    "secure rejoin failed, trying unsecure rejoin: %x\r\n",
                        status);
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
        {
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
               "SENSOR APP: joining network - channel 0x%x, panid 0x%2x, ",
               networkParams.radioChannel, networkParams.panId);
          printExtendedPanId(APP_SERIAL, networkParams.extendedPanId);
          emberSerialPrintf(APP_SERIAL, "\r\n");
          emberSerialWaitSend(APP_SERIAL);

          // attempt to join the network
          status = emberJoinNetwork(applicationType,
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
                                      (int8u *) extendedPanId);

        #endif // USE_HARDCODED_NETWORK_SETTINGS
        break;

      // if in the middle of joining, do nothing
      case EMBER_JOINING_NETWORK:
        emberSerialPrintf(APP_SERIAL,
                          "BUTTON0: app already trying to join\r\n");
        break;

      // if already joined, do nothing
      case EMBER_JOINED_NETWORK:
        emberSerialPrintf(APP_SERIAL,
                          "BUTTON0: already joined, no action\r\n");
        break;

      // if leaving, do nothing
      case EMBER_LEAVING_NETWORK:
        emberSerialPrintf(APP_SERIAL, "BUTTON0: app leaving, no action\r\n");
        break;

      case EMBER_JOINED_NETWORK_NO_PARENT:
        emberSerialPrintf(APP_SERIAL, "BUTTON0: app will leave\r\n");
        emberLeaveNetwork();
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
  int16u data;
  int8u maximumPayloadLength;
  EmberStatus status;
  EmberApsFrame apsFrame;
  EmberMessageBuffer buffer = 0;
  int8u i;
  int8u sendDataSize = SEND_DATA_SIZE;

  // get a random piece of data
  data = halCommonGetRandom();

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

  // we are doing things slightly different for DATA_MODE_TEST by sending 27
  // bytes of data with APS encryption enable.
  if(dataMode == DATA_MODE_TEST) {
    sendDataSize = 27;
    for (i=0; i<sendDataSize; i++) {
      globalBuffer[i] = HIGH_BYTE(data);
    }
    // copy the data into a packet buffer
    buffer = emberFillLinkedBuffers(globalBuffer, sendDataSize);
  } else {
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
  }

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
          "TX ERROR [data], OUT OF BUFFERS\r\n");
    return;
  }

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_DATA;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;
  if(dataMode == DATA_MODE_TEST) {
    apsFrame.options |= EMBER_APS_OPTION_ENCRYPTION;
  }

  // send the message
  status = emberSendUnicast(EMBER_OUTGOING_VIA_ADDRESS_TABLE,
                            SINK_ADDRESS_TABLE_INDEX,
                            &apsFrame,
                            buffer /* data to send */);

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
  } else {
    waitingForDataAck = TRUE;
  }

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  if(dataMode != DATA_MODE_TEST) {
    // print a status message
    emberSerialPrintf(APP_SERIAL,
                      "TX [DATA] status: 0x%x  data: 0x%2x / len 0x%x)\r\n",
                      status, data, sendDataSize + EUI64_SIZE);
  }
}

// Ask our parent who the sink is
void sendSinkQuery(void) {
  EmberMessageBuffer buffer;
  EmberApsFrame apsFrame;
  EmberStatus status;

  // the data
  MEMCOPY(globalBuffer, emberGetEui64(), EUI64_SIZE);

  // copy the data into a packet buffer
  buffer = emberFillLinkedBuffers((int8u*)globalBuffer, EUI64_SIZE);

  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    emberSerialPrintf(APP_SERIAL,
                      "TX ERROR [sink query], OUT OF BUFFERS\r\n");
    return;
  }

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SINK_QUERY;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = emberSendUnicast(EMBER_OUTGOING_DIRECT,
                            emberGetParentNodeId(),
                            &apsFrame,
                            buffer);

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);

  emberSerialPrintf(APP_SERIAL,
                    "TX [sink query], status:0x%x\r\n",
                    status);

  waitingForSinkAdvMessage = TRUE;
  timeToWaitForSinkAdvMessage = TIME_TO_WAIT_FOR_SINK_ADVERTISE;
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
       "TX [multicast hello], status is 0x%x\r\n", status);
}


// this handles the sink advertisement message on the sensor
// the sensor responds with a SENSOR_SELECT_SINK message if it
// is not yet bound to a sink
void handleSinkAdvertise(int8u* data) {
  EmberEUI64 sinkEUI;
  int16u sinkShortAddress;
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
  apsFrame.clusterId = MSG_SENSOR_SELECT_SINK;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

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
  timeToWaitForSinkReadyMessage = TIME_TO_WAIT_FOR_SINK_READY;
}

// End application functions for event handling and messaging.
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

      // bootloader
    case 'b':
      emberSerialPrintf(APP_SERIAL, "starting bootloader...\r\n");
      emberSerialWaitSend(APP_SERIAL);
      halLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
      break;

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

    case 'x':
      printTokens();
      break;
    case 's':
      if (sleepWhenNotJoined)
        sleepWhenNotJoined = FALSE;
      else
        sleepWhenNotJoined = TRUE;

      emberSerialPrintf(APP_SERIAL, "sleepWhenNotJoined: %x",
                        sleepWhenNotJoined);
      break;

#ifdef DEBUG_NETWORK_STATE
    case 'd':
      switch(emberNetworkState()){
      case EMBER_NO_NETWORK:
        emberSerialPrintf(APP_SERIAL, "EMBER_NO_NETWORK\r\n");
        break;
      case EMBER_JOINING_NETWORK:
        emberSerialPrintf(APP_SERIAL, "EMBER_JOINING_NETWORK\r\n");
        break;
      case EMBER_JOINED_NETWORK:
        emberSerialPrintf(APP_SERIAL, "EMBER_JOINED_NETWORK\r\n");
        break;
      case EMBER_JOINED_NETWORK_NO_PARENT:
        emberSerialPrintf(APP_SERIAL, "EMBER_JOINED_NETWORK_NO_PARENT\r\n");
        break;
      case EMBER_LEAVING_NETWORK:
        emberSerialPrintf(APP_SERIAL, "EMBER_LEAVING_NETWORK\r\n");
        break;
      default:
        emberSerialPrintf(APP_SERIAL, "unknown %x\r\n", emberNetworkState());
      }
      emberSerialWaitSend(APP_SERIAL);
      if (emberOkToNap())
        emberSerialPrintf(APP_SERIAL, "nap OK\r\n");
      else
        emberSerialPrintf(APP_SERIAL, "nap FALSE\r\n");
      if (emberOkToHibernate())
        emberSerialPrintf(APP_SERIAL, "hib OK\r\n");
      else
        emberSerialPrintf(APP_SERIAL, "hib FALSE\r\n");
      emberSerialWaitSend(APP_SERIAL);

      // check which stack tasks are currently preventing sleep
      {
        int16u tasks = emberCurrentStackTasks();
        emberSerialPrintf(APP_SERIAL, "curr tasks 0x%2x\r\n", tasks);
        emberSerialWaitSend(APP_SERIAL);
        if (EMBER_OUTGOING_MESSAGES & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_OUTGOING_MESSAGES\r\n", tasks);
        if (EMBER_INCOMING_MESSAGES & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_INCOMING_MESSAGES\r\n", tasks);
        emberSerialWaitSend(APP_SERIAL);
        if (EMBER_RADIO_IS_ON & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_RADIO_IS_ON\r\n", tasks);
        if (EMBER_TRANSPORT_ACTIVE & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_TRANSPORT_ACTIVE\r\n", tasks);
        emberSerialWaitSend(APP_SERIAL);
        if (EMBER_APS_LAYER_ACTIVE & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_APS_LAYER_ACTIVE\r\n", tasks);
        if (EMBER_ASSOCIATING & tasks)
          emberSerialPrintf(APP_SERIAL, "EMBER_ASSOCIATING\r\n", tasks);
        emberSerialWaitSend(APP_SERIAL);
      }
      break;
#endif // DEBUG_NETWORK_STATE

      // send test data
    case 'T':
      dataMode = DATA_MODE_TEST;
      // Enabling the internal variable emRadioAlwaysUseZeroBackoff puts the
      //  MAC into a mode were only CSMA backoffs of zero are used.
      //  This is intended only to remove variability when doing power
      //  consumption measurements and eases comparison between different
      //  scenarios.
      // Enabling this variable in a normal application will have a negative
      //  impact on overall network performance.
      emRadioAlwaysUseZeroBackoff = TRUE;
      emberSerialPrintf(APP_SERIAL, "Enter test data mode\r\n");
      break;

    case '\n':
    case '\r':
      break;

    default:
      emberSerialPrintf(APP_SERIAL, "unknown cmd\r\n");
      break;
     }
#ifndef MOBILE_SENSOR_APP
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\nsleepy-sensor>");
#else
    if (cmd != '\n') emberSerialPrintf(APP_SERIAL, "\r\nmobile-sensor>");
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
  PRINT("s = turn on sleeping when not joined\r\n");
}


// End utility functions
// *******************************************************************

