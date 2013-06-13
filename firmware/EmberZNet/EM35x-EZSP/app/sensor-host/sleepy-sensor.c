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
//        network if it isn't already joined. 
//  ('1') simulate button 1 press - leave network
//  ('e') reset the node
//  ('k') print the security keys
//  ('?') prints the help menu
//
//  Copyright 2005 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/sensor-host/common.h"

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

// Keep track of the current network state
static EmberNetworkStatus networkState = EMBER_NO_NETWORK;

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
// TRUE when called emberLeaveNetwork but havent seen stack down yet
boolean inProcessOfLeaving = FALSE;

int8u sendDataCountdown = SEND_DATA_RATE;

// buffer for organizing data before we send a datagram
int8u globalBuffer[128];

// for sensor nodes, number of data messages that have failed
int8u numberOfFailedDataMessages = 0;

volatile boolean buttonZeroPress = FALSE;  // for button push (see halButtonIsr)
volatile boolean buttonOnePress = FALSE;  // for button push (see halButtonIsr)

// controls if the node sleeps when it isn't part of a network
boolean sleepWhenNotJoined = FALSE;

// Is the EM260 awake?
volatile boolean em260IsAwake = TRUE;

// defines to make the code easier to read
#define NAP       0
#define HIBERNATE 1

// Current polling interval in quarter seconds. 0 indicates that we are
// not polling.
static int16u pollInterval = 0;

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

  // ezspUtilInit must be called before other EmberZNet stack functions
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
  status = ezspGetNetworkParameters(&nodeType, &parameters);
  if ((status == EMBER_SUCCESS) &&
      (nodeType == applicationType) &&
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

    applicationTick(); // Power save, watch timeouts
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
    // we aren't waiting for an ack any more
    waitingForDataAck = FALSE;

    // if the message failed, update fail counter and print a warning
    if (status != EMBER_SUCCESS) {
      numberOfFailedDataMessages++;
      emberSerialPrintf(APP_SERIAL,
         "WARNING: TX of [data] %x%x msg failed, consecutive failures:%x\r\n",
         messageContents[EUI64_SIZE + 0],
         messageContents[EUI64_SIZE + 1],
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
      emberSerialPrintf(APP_SERIAL, "RX ACK for [DATA]\r\n");
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
      emberSerialPrintf(APP_SERIAL, "RX too short [sink advertise] from: ");
      printEUI64(APP_SERIAL, &eui);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      return;
    } 

    emberSerialPrintf(APP_SERIAL, "RX [sink advertise] from: ");
    printEUI64(APP_SERIAL, &eui);
    if ((mainSinkFound == FALSE) && (waitingForSinkReadyMessage == FALSE)) {
      waitingForSinkAdvMessage = FALSE;
      emberSerialPrintf(APP_SERIAL, "; processing message\r\n");
      handleSinkAdvertise(message);
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
    inProcessOfLeaving = FALSE;
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

  EmberStatus status = emberJoinNetwork(applicationType, &networkParams);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Join error 0x%x", status);
  }
}

// called by form-and-join library
void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{}

// this is called only when a poll has failed for 60 seconds (240
// quarter seconds) -- if a parent does not respond for during this
// period then the child tries to find a new parent.
void ezspPollCompleteHandler(EmberStatus status)
{

  switch (status) {
  case EMBER_SUCCESS:
  case EMBER_MAC_NO_DATA:
    break;
  default:
    if ((networkState == EMBER_JOINED_NETWORK) ||
        (networkState == EMBER_JOINED_NETWORK_NO_PARENT)) {
      emberSerialPrintf(APP_SERIAL, "poll: no parent\r\n");

      // turn off regular polling
      ezspPollForData(0, EMBER_EVENT_INACTIVE, 2);

      status = emberFindAndRejoinNetwork(TRUE, 0);
      emberSerialPrintf(APP_SERIAL, "looking for new parent: rejoin: %x\r\n", 
                        status);
      emberSerialWaitSend(APP_SERIAL);

      // set a timer to try a rejoin insecurely if we dont get a 
      // stackStatus call of network up in a reasonable amount of time
      findAndRejoinTimer = FIND_AND_REJOIN_TIMEOUT;
    }
    break;
  }
}

void halNcpIsAwakeIsr(boolean isAwake)
{
  em260IsAwake = isAwake;
}

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
// Application functions dealing with sleeping.
// These are called from the heartbeat function

// handle going to sleep, the sleep duration is passed in.
void sensorFullSleep(int32u* sleepDuration, int8u type)
{
  EmberStatus status;
  int32u time, sleepTimeRemaining, elapsedSleepTime;
  EmberEventUnits units;
  boolean repeat;

  // print a message which type of sleep
  if (type == NAP)
    emberSerialPrintf(APP_SERIAL, "EVENT: napping...");
  if (type == HIBERNATE)
    emberSerialPrintf(APP_SERIAL, "EVENT: hibernating...");
  emberSerialWaitSend(APP_SERIAL);

  // if we're no longer joined to the network ... 
  if (networkState == EMBER_NO_NETWORK) {
    // ... and the stack is still polling tell it to stop
    if (pollInterval != 0) {
      ezspPollForData(0, EMBER_EVENT_INACTIVE, 2);
      pollInterval = 0;
    }
  } else if (pollInterval != *sleepDuration) {
    // if the desired sleep duration is changeing then we need to inform
    // the stack to poll at a different interval.
    status = ezspPollForData(*sleepDuration, EMBER_EVENT_QS_TIME,
                             (240 / (*sleepDuration)));
    if (status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, "poll: 0x%x\r\n", status);
      emberSerialWaitSend(APP_SERIAL);
      return;
    }
    pollInterval = *sleepDuration;
  }

  time = halCommonGetInt32uMillisecondTick();

  // set the variable that will instruct the stack to sleep after
  // processing the next EZSP command
  ezspSleepMode = EZSP_FRAME_CONTROL_DEEP_SLEEP;

  // make sure we are awakened after the sleepDuration since there
  // could be work we need to do.
  status = ezspSetTimer(0, *sleepDuration, EMBER_EVENT_QS_TIME, FALSE);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "setTimer: 0x%x\r\n", status);
    emberSerialWaitSend(APP_SERIAL);
    return;
  }

  // We just told the EM260 to sleep...
  em260IsAwake = FALSE;

  // this disables interrupts and reenables them once it completes
  ATOMIC_LITE(
    halPowerDown();  // turn off board and peripherals
    halSleep(SLEEPMODE_POWERDOWN);
    halPowerUp();   // power up board and peripherals
    appSetLEDsToInitialState();
  )

  // Make sure the EM260 is awake.  If it's not then wake it.
  if (em260IsAwake == FALSE) {
    ezspWakeUp();
    do { simulatedTimePasses(); } while (em260IsAwake == FALSE);
  }

  ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;

  // keep track of how long we slept for the purpose of
  // knowing when to send data.  ezspGetTimer returns the time left in the
  // unit used in ezspSetTimer, which is in quarter seconds
  sleepTimeRemaining = (int32u)ezspGetTimer(0, &units, &repeat);
  elapsedSleepTime = (*sleepDuration) - sleepTimeRemaining;
  halCommonSetSystemTime(time + (elapsedSleepTime << 8));
  *sleepDuration -= elapsedSleepTime;

  // message to know we are awake
  emberSerialPrintf(APP_SERIAL, "wakeup\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

// End Application functions dealing with sleeping.
// *******************************************************************


// *******************************************************************
// Application functions for event handling and messaging.


// applicationTick - called to check event timeouts and sleep the device
//
// sleeping is handled in this function. There are two modes of sleeping
// 1. napping - device sleeps for 1 second intervals. The device can
//    receive normal APS messages reliably.
// 2. hibernating - device sleeps for a number of seconds equal to the
//   rate at which data is sent (SEND_DATA_RATE). Hibernating devices cannot
//   reliably receive normal APS messages. 
//
static void applicationTick(void) {
  static int16u lastBlinkTime = 0;
  int16u time;

  // sleepDurationAttempted is how long the device wanted to sleep
  int32u sleepDurationAttempted = 0;
  // after sleeping, sleepDuration is how long the device actually slept
  int32u sleepDuration = 0;

  // After the device sleeps, application ticks need to be taken off of 
  // the timers that handle events in an amount that equals the amount of 
  // time the device slept. This variable is used to keep track of the 
  // number of quarter-seconds that the device successfully slept 
  // (interrupts could wake it before it was done)
  int8u qsTicksToTakeOff;


  // ******************************
  // We need to nap (not hibernate) if we are waiting on events.
  // There are 3 events we wait on:
  // 1. (mainSinkFound == FALSE) or
  // 2. (waitingForSinkReadyMessage == TRUE) or
  // 3. (waitingForDataAck == TRUE)
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


  // this device normally only sleeps when it is joined to the network.
  // To force the device sleep when it is not joined to a network use
  // the serial command ('s'). This application does not sleep when not
  // joined (by default) to allow the user to interact with the device
  // using the serial port. Once the device is sleeping, interaction with
  // the serial port is nearly impossible.
  if (networkState == EMBER_JOINED_NETWORK)
  {
    // if we are waiting on events then we nap, otherwise hibernate
    if ((mainSinkFound == FALSE) ||
        (waitingForSinkReadyMessage == TRUE) ||
        (waitingForDataAck == TRUE) ||
        (sendDataCountdown == 1))
    {
      // **************************
      // napping
      sleepDurationAttempted = 4; // 1 second
      sleepDuration = sleepDurationAttempted;
      // sleep until stack polls and wakes us up
      sensorFullSleep(&sleepDuration, NAP);
    }

    // **************************
    // we need to hibernate
    else {
      // sleepy sensors hibernate in between sending data
#ifndef MOBILE_SENSOR_APP
      sleepDurationAttempted = SEND_DATA_RATE;
#else
      // When mobile sensors wake up they will need to find a sink
      // again, and when they do that they will send data immediately.
      sleepDurationAttempted = TIME_BEFORE_SINK_ADVERTISE;
#endif
      sleepDuration = sleepDurationAttempted;
      // sleep and until stack polls and wakes us up
      sensorFullSleep(&sleepDuration, HIBERNATE);
    }
  }

  // if we are not joined and should sleep (which is turned on with
  // a serial cmd) then sleep here. No checks for emberOkToNap() or
  // emberOkToHibernate(). We sleep more aggressively here, hard coding
  // to 60 seconds and not basing the sleep time on the SEND_DATA_RATE
  if (networkState == EMBER_NO_NETWORK)
  {
    if (sleepWhenNotJoined == TRUE)
    {
      sleepDurationAttempted = 240; // 60 seconds
      sleepDuration = sleepDurationAttempted;
      sensorFullSleep(&sleepDuration, HIBERNATE);
    } else if (pollInterval != 0) {
      // ... and the stack is still polling tell it to stop
      ezspPollForData(0, EMBER_EVENT_INACTIVE, 2);
      pollInterval = 0;
    }
  }

  // we may have been interrupted during sleep, from a button press
  // for instance, in the case when sleep was interrupted, the
  // sleepDuration variable will contain the number of quarter seconds
  // left that we did not sleep.
  qsTicksToTakeOff = (int8u) ((sleepDurationAttempted - sleepDuration));

  time = halCommonGetInt16uMillisecondTick();

  // Application timers are based on quarter second intervals, where each 
  // quarter second is equal to TICKS_PER_QUARTER_SECOND millisecond ticks. 
  // Only service the timers (decrement and check if they are 0) after each
  // quarter second. TICKS_PER_QUARTER_SECOND is defined in 
  // app/sensor-host/common.h.
  if ( (int16u)(time - lastBlinkTime) > TICKS_PER_QUARTER_SECOND ) {
    lastBlinkTime = time;

    // blink the LEDs
    if (networkState == EMBER_JOINED_NETWORK)
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
        //emberSerialPrintf(APP_SERIAL, "sending data...\r\n");
        sendDataCountdown = SEND_DATA_RATE;
        sendData();
      }
    } else {
      // ********************************************/
      // We don't have a sink yet - check to see if
      // we need to query for it.
      // ********************************************/
      if ((networkState == EMBER_JOINED_NETWORK) &&
          (waitingForSinkAdvMessage == FALSE) &&
          (inProcessOfLeaving == FALSE) &&
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
}


void checkButtonEvents(void) {

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
        {
          // structure to store necessary network parameters of the node
          // (which are panId, enableRelay, radioTxPower, and radioChannel)
          EmberNetworkParameters networkParams;
          EmberStatus status;
          int8u extendedPanId[EXTENDED_PAN_ID_SIZE] = APP_EXTENDED_PANID;

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
          status = emberJoinNetwork(applicationType, 
                                    &networkParams);
          if (status != EMBER_SUCCESS) {
            emberSerialPrintf(APP_SERIAL,
              "error returned from emberJoinNetwork: 0x%x\r\n", status);
          } else {
            emberSerialPrintf(APP_SERIAL, "waiting for stack up...\r\n");
          }
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
        emberSerialPrintf(APP_SERIAL, "BUTTON0: app already trying to join\r\n");
        break;

      // if already joined, turn allow joining on
      case EMBER_JOINED_NETWORK:
        emberSerialPrintf(APP_SERIAL, "BUTTON0: already joined, no action\r\n");
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
      inProcessOfLeaving = TRUE;
      //mainSinkFound = FALSE;
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
  } else {
    waitingForDataAck = TRUE;
  }

  // print a status message
  emberSerialPrintf(APP_SERIAL,
                    "TX [DATA] status: 0x%x  data: 0x%2x / len 0x%x)\r\n",
                    status, data, sendDataSize + EUI64_SIZE);
}

// Ask our parent who the sink is
void sendSinkQuery(void) {
  EmberEUI64 parentEui64;
  EmberNodeId parentNodeId;
  EmberApsFrame apsFrame;
  EmberStatus status;

  // Get info about our parent
  ezspGetParentChildParameters(parentEui64, &parentNodeId);
  
  // the data
  MEMCOPY(globalBuffer, emberGetEui64(), EUI64_SIZE);

  // all of the defined values below are from app/sensor-host/common.h
  // with the exception of the options from stack/include/ember.h
  apsFrame.profileId = PROFILE_ID;          // profile unique to this app
  apsFrame.clusterId = MSG_SINK_QUERY;
  apsFrame.sourceEndpoint = ENDPOINT;       // sensor endpoint
  apsFrame.destinationEndpoint = ENDPOINT;  // sensor endpoint
  apsFrame.options = EMBER_APS_OPTION_RETRY;

  // send the message
  status = ezspSendUnicast(EMBER_OUTGOING_DIRECT,
                           parentNodeId,
                           &apsFrame,
                           MSG_SINK_QUERY,
                           EUI64_SIZE,
                           globalBuffer,
                           &apsFrame.sequence);
  emberSerialPrintf(APP_SERIAL,
                    "TX [sink query], status:0x%x\r\n",
                    status);

  waitingForSinkAdvMessage = TRUE;
  timeToWaitForSinkAdvMessage = TIME_TO_WAIT_FOR_SINK_ADVERTISE;
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
  // with the exception of the options from stack/include/ember.h
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
  EmberEUI64 sinkEUI;
  int16u sinkShortAddress;
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
  // button 1 (button 1 on the dev board) was pushed down
  if (button == BUTTON1 && state == BUTTON_PRESSED) {
    buttonOnePress = TRUE;
  }

  // button 0 (button 0 on the dev board) was pushed down
  if (button == BUTTON0 && state == BUTTON_PRESSED) {
    buttonZeroPress = TRUE;
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

    case 'e':
      emberSerialGuaranteedPrintf(APP_SERIAL, "Resetting node...\r\n");
      halReboot();
      break;

      // print the keys
    case 'k':
      sensorCommonPrintKeys();
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
    case 'a':
      switch(networkState){
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
        emberSerialPrintf(APP_SERIAL, "unknown %x\r\n", networkState);
      }
      emberSerialWaitSend(APP_SERIAL);
      break;
#endif // DEBUG_NETWORK_STATE

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
#ifndef NO_BOOTLOADER
  PRINT("b = send node into bootloader\r\n");
#endif
  PRINT("l = send multicast [hello]\r\n");
  PRINT("t = play tune\r\n");
  PRINT("a = print address table\r\n");
  PRINT("m = print multicast table\r\n");
  PRINT("0 = simulate button 0: join to network\r\n");
  PRINT("1 = simulate button 1: leave ZigBee network\r\n");
  PRINT("e = reset node\r\n");
  PRINT("s = turn on sleeping when not joined\r\n");
  PRINT("k = print the security keys\r\n");
  PRINT("r = reset node\r\n");
}


// End utility functions
// *******************************************************************

