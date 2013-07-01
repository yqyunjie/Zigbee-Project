// *******************************************************************
//  mfg-sample-host.c
//
//  host sample app for using the mfglib
//
//  This is an example of an app that uses the mfglib test framework.
//  It has a very simple serial interface to invoke all provided API.
//  It also has some additional example token and sleep commands.
//
//  Copyright 2006 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER //compiler/micro specifics, types
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/serial/serial.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/serial/cli.h"
#include "app/mfglib-host/mfg-sample-tokens.h"
#ifdef GATEWAY_APP
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include <unistd.h>
#endif

// the following variables are used by the application to keep track of
// what the current application state is
boolean mfgLibRunning = FALSE;
boolean mfgToneTestRunning = FALSE;
boolean mfgStreamTestRunning = FALSE;
boolean stackUp = FALSE;

// this variable keeps track of how many mfglib packets have been received 
// during a mfglib session (from mfglib start to mfglib end). This can be 
// accessed using the "info" command.
int16u  mfgTotalPacketCounter;

int16u ezspErrOverflow;
int16u ezspErrQueueFull;

// this buffer is filled with the contents to be sent using mfglibSendPacket
int8u   sendBuff[128];


// *******************************************************************
// Ember endpoint and interface configuration

#define ENDPOINT 1
#define PROFILE_ID 0xC00F

int8u emberEndpointCount = 1;
EmberEndpointDescription PGM endpointDescription = { PROFILE_ID, 0, };
EmberEndpoint emberEndpoints[] = {
  { ENDPOINT, &endpointDescription },
};

// End Ember endpoint and interface configuration
// *******************************************************************

// *******************************************************************
// MFGLIB callback function:

// ezspMfglibRxHandler is called whenever a mfglib packet is received if
// the device has called ezspMfglibStart and passed a parameter of TRUE 
// (indicating that the application wants callbacks when mfglib messages
// are received). 
//
// If the mfglib packets are coming in very fast, the receiving host
// should take care to not do a lot of work in ezspMfglibRxHandler or
// it will not be able to service callbacks fast enough and could get an
// ezsp error (0x34 = EZSP_ERROR_OVERFLOW) indicating this. For instance,
// if all incoming messages are printed using emberSerialPrintf then this
// error will occur. 
//
// This example application simply keeps track of the first packet received
// and how many have been received until it sees a pause in sending.
// If it does not receive packets for two "heartbeat" periods (200 ms - see 
// the heartBeat function) it considers the stream of packets to have ended 
// and prints the results for the user by calling appMfglibSendIsComplete
//
// The following variables relate to keeping the saved packet info and keeping
// track of the state (current packets received in this group, how many
// had been received since last heartbeat call.

// the number of packets in the current transmit group
int16u mfgCurrentPacketCounter = 0;

// the saved information for the first packet
int8u savedPktLength = 0;
int8u savedRssi = 0;
int8u savedLinkQuality = 0;
int8u savedPkt[128];

// if we are in a transmit group of packets or not 
boolean inReceivedStream = FALSE;

// this keeps track of the number of packets received as of the last heartbeat
// "tick" (200 ms). If this value stays constant for two ticks, then
// appMfglibSendIsComplete is called by heartBeat.
int16u heartbeatLastPacketCounterValue = 0;


// *****************************
// ezspMfglibRxHandler
//
// Called by the EM260 to the host whenever a mfglib message comes in if
// the device has called ezspMfglibStart and passed a parameter of TRUE.
// See the block comment and variables directly above this for more 
// information. 
// *****************************
void ezspMfglibRxHandler(int8u linkQuality,
                         int8s rssi,
                         int8u pktLength,
                         int8u *pkt)
{
  // This increments the total packets for the whole mfglib session
  // this starts when ezspMfglibStart is called and stops when ezspMfglibEnd
  // is called.
  mfgTotalPacketCounter++;

  // This keeps track of the number of packets in the current transmit group.
  // This starts when a mfglib packet is received and ends when no mfglib
  // packets are received for two heartBeat ticks (a tick is 200 ms)
  mfgCurrentPacketCounter++;

  // If this is the first packet of a transmit group then save the information
  // of the current packet. Don't do this for every packet, just the first one.
  if (!inReceivedStream) {
    inReceivedStream = TRUE;
    mfgCurrentPacketCounter = 1;
    savedRssi = rssi;
    savedLinkQuality = linkQuality;
    savedPktLength = pktLength;
    MEMCOPY(savedPkt, pkt, pktLength); 
  }
}

// This function is called when a transmit group has completed, which means
// the host has received a mfglib packet and it has been at least two heartBeat
// ticks since it has seen another mfglib packet. This function prints the
// saved info of the first packet in the group and prints the number of
// packets seen. 
void appMfglibSendIsComplete(void)
{
  int8u i;

  // print the number of packets and the saved info for the first one
  emberSerialPrintf(APP_SERIAL, 
                    "\r\nMFG RX %2x pkts\r\n",mfgCurrentPacketCounter);
  emberSerialPrintf(APP_SERIAL, "first packet: lqi %x, rssi %x, len %x\r\n",
                    savedLinkQuality, savedRssi, savedPktLength);
  emberSerialWaitSend(APP_SERIAL);

  // print the data of the packet in rows of 16 bytes
  for (i=0; i<savedPktLength; i++) {
    if ((i % 16) == 0) {
      emberSerialPrintf(APP_SERIAL, "      : ");
      emberSerialWaitSend(APP_SERIAL);
    }
    emberSerialPrintf(APP_SERIAL, "%x ", savedPkt[i]);
    if ((i % 16) == 15) {
      emberSerialPrintf(APP_SERIAL, "\r\n");
    }
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  // reset the state 
  inReceivedStream = FALSE;
  mfgCurrentPacketCounter = 0;
  heartbeatLastPacketCounterValue = 0;
}

//----------------------------------------------------------------


// *****************************
// heartBeat
//
// This blinks the LEDs to let the user know the application is active and 
// checks if a mfglib transmit group has completed. 
// *****************************
static void heartBeat(void) {
  static int16u lastBlinkTime = 0;
  int16u time;
  static int8u numTimesPacketCountStayedSame = 0;

  time = halCommonGetInt16uMillisecondTick();

  // make a tick be 200 ms
  // the int16u cast is important in the case where this is running
  // on the PC talking to a network coprocessor over UART/EZSP. Without
  // the cast a higher precision number is used and once the time wraps
  // this check is never true (the ticks stop occuring)
  if ((int16u)(time - lastBlinkTime) > 200 /*ms*/) {

    // **************
    // blink the LED
    #ifndef GATEWAY_APP
    halToggleLed(BOARD_HEARTBEAT_LED);
    #endif
    lastBlinkTime = time;

    // **************
    // check if we are in a transmit group. This means at least one
    // mfglib packet has been received.
    if (inReceivedStream) {

      // check if the value of mfgCurrentPacketCounter (saved in 
      // heartbeatLastPacketCounterValue) is the same as the last time we
      // checked (the last tick)
      if (heartbeatLastPacketCounterValue == mfgCurrentPacketCounter) {
        numTimesPacketCountStayedSame++;

        // if the number of received mfglib messages has not incremented
        // within two ticks, then this group has ended. 
        if (numTimesPacketCountStayedSame == 2) {
          appMfglibSendIsComplete();
        } 
      } 
      else {
        // else means the value of mfgCurrentPacketCounter changed
        numTimesPacketCountStayedSame = 0;
      }
      // remember the current value for mfgCurrentPacketCounter
      heartbeatLastPacketCounterValue = mfgCurrentPacketCounter;
    }

  }
}


//----------------------------------------------------------------


#ifdef GATEWAY_APP
int main( int argc, char *argv[] )
#else
int main(void)
#endif
{
  EmberResetType reset;
  EmberStatus status;
  EzspStatus ezspStatus;

  // Determine the cause of the reset (powerup, etc).
  reset = halGetResetInfo();

  halInit(); // Initialize the micro, board peripherals, etc

#ifdef DEBUG
  crashInfo = *halInternalGetCrashInfo();

  // needs to be called prior to emInitAndSelectSerialPort() for VUART support
  //  currently only valid for non-serial based debug channel implementations
  (void)emDebugInit();
#endif

  INTERRUPTS_ON();  // Safe to enable interrupts at this point

#ifdef GATEWAY_APP
  if (!ashProcessCommandOptions(argc, argv))
    return 1;
#endif

  // inititialize the serial port
  // good to do this before ezspUtilInit, that way any errors that occur
  // can be printed to the serial port.
  if (emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
      != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_38400, PARITY_NONE, 1);
  }

  emberSerialGuaranteedPrintf(APP_SERIAL, "Reset(0x%x):%p\r\n",
                              reset, halGetResetString());


  //reset the EM260 and wait for it to come back
  //this will guarantee that the two MCUs start in the same state
  ezspStatus = ezspInit();
  if(ezspStatus != EZSP_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
                                "ERROR: ezspInit 0x%x:", ezspStatus);
    emberSerialGuaranteedPrintf(APP_SERIAL, "\r\n");
#ifdef GATEWAY_APP
    emberSerialGuaranteedPrintf(APP_SERIAL,
                                "ncpError=0x%x, ashError=0x%x\r\n",
                                ncpError, ashError);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts. But only
    // for the gateway version.
    assert(FALSE);
#endif
  } else {
    emberSerialGuaranteedPrintf(APP_SERIAL, "ezspInit passed\r\n");
  }
  

  // ezspUtilInit must be called before other EmberNet stack functions
  status = ezspUtilInit(APP_SERIAL, reset);
  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
                                "ERROR: ezspUtilInit 0x%x\r\n", status);
  } else {
    emberSerialPrintf(APP_SERIAL, "ezspUtilInit passed\r\n");
  }

  // initialize the cli library to use the correct serial port. APP_SERIAL 
  // is defined in the makefile or project-file depending on the platform.
  cliInit(APP_SERIAL);

  while(1) {
    
    halResetWatchdog();      // Periodically reset the watchdog
    emberTick();             // Allow the stack and ezsp to run
    heartBeat();             // blink the LEDs
    cliProcessSerialInput(); // process serial input from ths user
  }
}


// this is called when the stack status changes
void ezspStackStatusHandler(EmberStatus status)
{
  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");
    stackUp = TRUE;
    break;

  case EMBER_NETWORK_DOWN:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_DOWN\r\n");
    stackUp = FALSE;
    break;

  case EMBER_JOIN_FAILED:
    emberSerialPrintf(APP_SERIAL,
           "EVENT: stackStatus now EMBER_JOIN_FAILED\r\n");
    stackUp = FALSE;
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
{
}

// this is called when an error occurs while performing a
// scanAndFormNetwork or a scanAndJoinNetwork
void ezspScanErrorHandler(EmberStatus status)
{
}

void hal260IsAwakeIsr(boolean isAwake)
{
}

// This is called when an EZSP error is reported
void ezspErrorHandler(EzspStatus status)
{

  // this error means the EM260 ran out of resources and had to drop a
  // callback. This can happen if the mfglib packets are spaced too 
  // closely together. Don't reset on this error, just count it.
  if (status == EZSP_ERROR_OVERFLOW) {
    ezspErrOverflow++;
    return;
  }

  // this error usually means that the host got a lot of callbacks in between
  // sending a command and getting the response for that command. The 
  // callbacks are held onto until the command response comes back. This
  // can happen if the host happens to send commands (by using the "info"
  // serial command) while receiving callbacks (mfglib packets) 
  if (status == EZSP_ERROR_QUEUE_FULL) {
    ezspErrQueueFull++;
    return;
  }


  emberSerialGuaranteedPrintf(APP_SERIAL,
                              "ERROR: ezspErrorHandler 0x%x\r\n",
                              status);

#ifndef GATEWAY_APP
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              " while processing command 0x%x,0x%x:",
                              hal260Frame[3], hal260Frame[5]);
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              " cause value 0x%x",
                              hal260SpipErrorByte);
#else
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              " ncpError=0x%x, ashError=0x%x",
                              ncpError, ashError);
#endif

  // exit the application
#ifdef GATEWAY_APP
  ezspClose();
  exit(-1);
#else
  assert(FALSE);
#endif

}


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
{}

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
  emberSerialPrintf(APP_SERIAL, "RX type %x from %2x, cluster %2x, len %x",
                    type, sender, apsFrame->clusterId, length);
}


// end Ember callback handlers
// *******************************************************************



// *****************************
// msVersionCB
// *****************************
void msVersionCB(void)
{
  emberSerialPrintf(APP_SERIAL,
      "Manufacturing Library Sample Host Application, version 1.0\r\n\r\n");
}

// *****************************
// printEUI64 - utility function
// *****************************
void printEUI64(int8u port, EmberEUI64* eui) {
  int8u i;
  int8u* p = (int8u*)eui;

  emberSerialPrintf(port, "EUI: ");
  for (i=8; i>0; i--) {
    emberSerialPrintf(port, "%x", p[i-1]);
  }
  emberSerialPrintf(port, "\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

// *********************************
// printExtendedPanId
// *********************************
void printExtendedPanId(int8u port, int8u *extendedPanId) {
  int8u i;
  emberSerialPrintf(port, "   ExtendedPanId: ");
  for (i = 0 ; i < EXTENDED_PAN_ID_SIZE ; i++) {
    emberSerialPrintf(port, "%x", extendedPanId[i]);
  }
  emberSerialPrintf(port, "\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

// *****************************
// msInfoCB
// *****************************
// mfg-sample-host, version x
// stack aabb
// EUI X
// in mfg mode: YES / NO
//    mfg channel: X, mfg power: X
//    tone running / stream running 
// joined to network: NO
//    shortID / channel / power / panid / ext pan id
//
void msInfoCB(void)
{
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;
  ezspGetNetworkParameters(&nodeType, &parameters);

  msVersionCB();
  printEUI64(APP_SERIAL, (EmberEUI64*)emberGetEui64());
  emberSerialPrintf(APP_SERIAL, "stack [%2x]\r\n",
                    ezspUtilStackVersion);
  emberSerialPrintf(APP_SERIAL, "in mfg mode: %p\r\n",
                    mfgLibRunning ? "YES" : "NO");
  emberSerialWaitSend(APP_SERIAL);

  // print info on mfg lib if it is running
  if (mfgLibRunning == TRUE) {
    emberSerialPrintf(APP_SERIAL, "   mfg channel: 0x%x, mfg power: 0x%x\r\n",
                      mfglibGetChannel(), mfglibGetPower());
    emberSerialPrintf(APP_SERIAL, "   mfg pkts received: 0x%2x\r\n",
                      mfgTotalPacketCounter);
    emberSerialPrintf(APP_SERIAL, "   ezsp errors: overflow 0x%2x, queue full 0x%2x\r\n", ezspErrOverflow, ezspErrQueueFull);
    if (mfgToneTestRunning) { 
      emberSerialPrintf(APP_SERIAL, "   mfg Tone test running\r\n");
    }
    if (mfgStreamTestRunning) {
      emberSerialPrintf(APP_SERIAL, "   mfg Stream test running\r\n");
    }
  }

  emberSerialPrintf(APP_SERIAL, "stack is up: %p\r\n",
                    stackUp ? "YES" : "NO");
  emberSerialWaitSend(APP_SERIAL);

  // print info on stack if it is up
  if (stackUp == TRUE) {
    emberSerialPrintf(APP_SERIAL, "   short ID [%2x]\r\n", emberGetNodeId());
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL,
                      "   chan [0x%x], power [0x%x], panId [0x%2x]\r\n",
                      parameters.radioChannel,
                      parameters.radioTxPower,
                      parameters.panId);
    printExtendedPanId(APP_SERIAL, parameters.extendedPanId);
    emberSerialWaitSend(APP_SERIAL);
  } 
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

// *****************************
// msNetwork CB
// *****************************
// network form <channel> <power> <panid in hex>
// network join <channel> <power> <panid in hex>
// network leave
// network pjoin <time>
//
// devices always join as routers
// pjoin stands for permitjoining
void msNetworkCB(void) {
  EmberStatus status;
  int8u nodeType;
  int8u permitJoinDuration;
  EmberNetworkParameters networkParams;

  // network form <channel> <power> <panid in hex>
  if (cliCompareStringToArgument("form", 1) == TRUE)
  {
    networkParams.radioChannel = cliGetInt16uFromArgument(2);
    networkParams.radioTxPower = cliGetInt16uFromArgument(3);
    networkParams.panId = (cliGetHexByteFromArgument(0, 4) * 256) + 
      (cliGetHexByteFromArgument(1, 4));

    status = emberFormNetwork(&networkParams);
    emberSerialPrintf(APP_SERIAL, "form 0x%x\r\n\r\n", status);
    return;
  }

  // network join <channel> <power> <panid in hex>
  else if (cliCompareStringToArgument("join", 1) == TRUE)
  {
    nodeType = EMBER_ROUTER;
    MEMSET(networkParams.extendedPanId, 
           0,
           EXTENDED_PAN_ID_SIZE);
    networkParams.radioChannel = cliGetInt16uFromArgument(2);
    networkParams.radioTxPower = cliGetInt16uFromArgument(3);
    networkParams.panId = (cliGetHexByteFromArgument(0, 4) * 256) + 
      (cliGetHexByteFromArgument(1, 4));

    status = emberJoinNetwork(nodeType, &networkParams);
    emberSerialPrintf(APP_SERIAL, "join 0x%x\r\n\r\n", status);
    return;
  }

  // network leave
  else if (cliCompareStringToArgument("leave", 1) == TRUE) {
    status = emberLeaveNetwork();
    emberSerialPrintf(APP_SERIAL, "leave 0x%x\r\n\r\n", status);
    return;
  }

  // network pjoin <time>
  else if (cliCompareStringToArgument("pjoin", 1) == TRUE)
  {
    permitJoinDuration = cliGetInt16uFromArgument(2);
    emberSerialPrintf(APP_SERIAL, "val 0x%x\r\n", permitJoinDuration);
    status = emberPermitJoining(permitJoinDuration);
    emberSerialPrintf(APP_SERIAL, "pJoin 0x%x\r\n\r\n", status);
    return;
  }

  else {
    emberSerialPrintf(APP_SERIAL, 
                      "network form <channel> <power> <panid in hex>\r\n");
    emberSerialPrintf(APP_SERIAL, 
                      "network join <channel> <power> <panid in hex>\r\n");
    emberSerialPrintf(APP_SERIAL, "network leave\r\n");
    emberSerialPrintf(APP_SERIAL, "network pjoin <time>\r\n");
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
}

// *****************************
// msTokenCB
// *****************************
// token dump
// token read <index>
// token read mfg <index>
// token write <index>
void msTokenCB(void) {

  // token dump
  if (cliCompareStringToArgument("dump", 1) == TRUE)
  {
    mfgappTokenDump();
  }

  // token read <index>
  // token read mfg <index>
  else if (cliCompareStringToArgument("read", 1) == TRUE)
  {
    if (cliCompareStringToArgument("mfg", 2) == TRUE) 
    {
      int16u index = cliGetInt16uFromArgument(3);
      mfgappTokenReadMfg(index);
    }
    else 
    {
      int16u index = cliGetInt16uFromArgument(2);
      mfgappTokenRead(index);
    }
  }

  // token write <index>
  else if (cliCompareStringToArgument("write", 1) == TRUE)
  {
    int16u index = cliGetInt16uFromArgument(2);
    mfgappTokenWrite(index);
  }

  // error
  else {
    emberSerialPrintf(APP_SERIAL, "token dump\r\n");
    emberSerialPrintf(APP_SERIAL, "token read <index>\r\n");
    emberSerialPrintf(APP_SERIAL, "token read mfg <index>\r\n");
    emberSerialPrintf(APP_SERIAL, "token write <index>\r\n");
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
}

#define MFGAPP_TEST_PACKET_MAX_SIZE 0x80
int8u testPacket[] = { 
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
  0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
  0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60,
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
  0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80
};

// utility used by msMfgCB
void fillBuffer(int8u* buff, int8u length, boolean random)
{
  int8u i;
  // length byte does not include itself. If the user asks for 10
  // bytes of packet this means 1 byte length, 7 bytes, and 2 bytes CRC
  // this example will have a length byte of 9, but 10 bytes will show
  // up on the receive side
  buff[0] = length - 1;

  if (random) {
    for (i=1; i < length; i++) {
      buff[i] = halCommonGetRandom();
    }
  } 
  else {
    for (i=1; i < length; i++) {
      buff[i] = testPacket[i-1];
    }
  }

}


// *****************************
// msMfgCB
// *****************************
// mfg start <want callback, 0=False, 1=True>
// mfg end
// mfg tone start
// mfg tone stop
// mfg stream start
// mfg stream stop
// mfg send <numPackets> test-packet <length>
// mfg send <numPackets> random <length> 
// mfg send <numPackets> message <first 8 bytes> <second 8 bytes>
// mfg chan set <channel>
// mfg chan get
// mfg power set <power> mode <powermode>
// mfg power get
void msMfgCB(void) 
{
  EmberStatus status;

  // mfg start <callback>
  if (cliCompareStringToArgument("start", 1) == TRUE) {
  
    int8u callback = cliGetInt16uFromArgument(2);
    boolean bcallback = FALSE;
    if (callback == 1) {
      bcallback = TRUE;
    }
    status = ezspMfglibStart(bcallback);
    emberSerialPrintf(APP_SERIAL, "mfglib start (%x), status 0x%x\r\n\r\n",
                      callback, status);

    // set a flag indicating to the app that the mfg lib is running
    if (status == EMBER_SUCCESS) {
      mfgLibRunning = TRUE;    
      mfgTotalPacketCounter = 0;
      ezspErrOverflow = 0;
      ezspErrQueueFull = 0;
    }
  } 

  // mfg end
  else if (cliCompareStringToArgument("end", 1) == TRUE) {
    emberSerialPrintf(APP_SERIAL, "rx 0x%2x packets while in mfg mode\r\n", 
                      mfgTotalPacketCounter);
    status = mfglibEnd();
    emberSerialPrintf(APP_SERIAL, "mfglib end status 0x%x\r\n\r\n", status);

    // set a flag indicating to the app that the mfg lib is not running
    if (status == EMBER_SUCCESS) {
      mfgLibRunning = FALSE;
    }
  } 

  // mfg tone
  else if (cliCompareStringToArgument("tone", 1) == TRUE) {
    if (cliCompareStringToArgument("start", 2) == TRUE) {
      status = mfglibStartTone();
      emberSerialPrintf(APP_SERIAL, "start tone 0x%x\r\n\r\n", status);
      if (status == EMBER_SUCCESS) {
        mfgToneTestRunning = TRUE;
      }
    } 
    else {
      status = mfglibStopTone();
      emberSerialPrintf(APP_SERIAL, "stop tone 0x%x\r\n\r\n", status);
      if (status == EMBER_SUCCESS) {
        mfgToneTestRunning = FALSE;
      }
    }
  } 

  // mfg stream
  else if (cliCompareStringToArgument("stream", 1) == TRUE) {
    if (cliCompareStringToArgument("start", 2) == TRUE) {
      status = mfglibStartStream();
      emberSerialPrintf(APP_SERIAL, "start stream 0x%x\r\n\r\n", status);
      if (status == EMBER_SUCCESS) {
        mfgStreamTestRunning = TRUE;
      }
    } 
    else {
      status = mfglibStopStream();
      emberSerialPrintf(APP_SERIAL, "stop stream 0x%x\r\n\r\n", status);
      if (status == EMBER_SUCCESS) {
        mfgStreamTestRunning = FALSE;
      }
    }
  } 

  //  0   1       2          3       4
  // mfg send <numPackets> random <length> 
  //                       message <up to 8 bytes>
  else if (cliCompareStringToArgument("send", 1) == TRUE) {
    int16u i;
    int16u numPackets = cliGetInt16uFromArgument(2);
    int16u length = cliGetInt16uFromArgument(4);
    int16u now, sendTime;

    // handle the case where the data is random
    if (cliCompareStringToArgument("random", 3) == TRUE) {
      fillBuffer((int8u*) &sendBuff, length, TRUE);
    }

    // handle the case where the data is the test packet
    else if (cliCompareStringToArgument("test", 3) == TRUE) {
      // error check the length, if you hit this just increase
      // the size and definition of testPacket above.
      if (length > MFGAPP_TEST_PACKET_MAX_SIZE) {
        emberSerialPrintf(APP_SERIAL, 
            "length of 0x%x larger than current test packet (0x%x)\r\n",
                          length, MFGAPP_TEST_PACKET_MAX_SIZE);
        return;
      }
      fillBuffer((int8u*) &sendBuff, length, FALSE);
    }

    // handle the case where the data is specified by the user
    else if (cliCompareStringToArgument("message", 3) == TRUE) {
      for (i=0; i<8; i++) {
        sendBuff[i] = cliGetHexByteFromArgument(i, 4);
        sendBuff[i+8] = cliGetHexByteFromArgument(i, 5);
        sendBuff[i+16] = cliGetHexByteFromArgument(i, 6);
        sendBuff[i+24] = cliGetHexByteFromArgument(i, 7);
      }
      // length byte does not include itself, so if the length byte
      // says 7, this means 7 bytes of data and the length byte = 8 total
      length = sendBuff[0] + 1;
    }

    // handle the case where the input is unrecognized - error
    else {
      emberSerialPrintf(APP_SERIAL, "not recognized, try \"mfg\" for help\r\n");
      return;
    }

    // check the length for min and max
    // in this case, "length" counts the 15.4 length byte. 
    if (length > 128) {
      emberSerialPrintf(APP_SERIAL, 
           "adjusting length from 0x%x to maximum of 128\r\n");
      length = 128;
      sendBuff[0] = 127;
    }
    if (length < 6) {
      emberSerialPrintf(APP_SERIAL, 
           "adjusting length from 0x%x to minimum of 6\r\n");
      length = 6;
      sendBuff[0] = 5;
    }

    // debug message describing packet that will be sent
    emberSerialPrintf(APP_SERIAL, 
                      "sending mfglib 0x%2x packet(s), length 0x%x", 
                      numPackets, length);

    for (i=0; i<length; i++) {
      if ((i % 16) == 0) {
        emberSerialPrintf(APP_SERIAL, "\r\n   : ");
      }
      emberSerialPrintf(APP_SERIAL, " %x", sendBuff[i]);
      emberSerialWaitSend(APP_SERIAL);
    }
    emberSerialPrintf(APP_SERIAL, "\r\n");

    // send the packet(s)
    for (i=0 ;i<numPackets; i++) {
      status = mfglibSendPacket(sendBuff[0], &sendBuff[1]);

      // reset the watchdog so it doesnt reset us
      halResetWatchdog();
      emberTick();
      
      // Add 10 ms delay in between sending messages. On UART hosts sometimes
      // the receiver can't keep up with the sender if the sender doesn't use
      // any delay. This helps prevent false failures - devices can hear fine
      // but they can't keep up with the sender.
      now = sendTime = halCommonGetInt32uMillisecondTick();
      while ((int16u)(now - sendTime) < 10) {
        now = halCommonGetInt32uMillisecondTick();
      }  

      // print an error on failure
      if (status != EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL, 
                          "mfg send err 0x%x index 0x%x\r\n\r\n",
                          status, i); 
      }
    }
  } 

  // mfg chan
  else if (cliCompareStringToArgument("chan", 1) == TRUE) {
    if (cliCompareStringToArgument("get", 2) == TRUE) {
      emberSerialPrintf(APP_SERIAL, "mfg get channel 0x%x\r\n\r\n", 
                        mfglibGetChannel());
    }
    else if (cliCompareStringToArgument("set", 2) == TRUE) {
      int16u channel = cliGetInt16uFromArgument(3);
      status = mfglibSetChannel(LOW_BYTE(channel));
      emberSerialPrintf(APP_SERIAL, 
                        "mfg set channel to 0x%x, status 0x%x\r\n\r\n",
                        LOW_BYTE(channel), status); 
    }
  } 

  // mfg power
  else if (cliCompareStringToArgument("power", 1) == TRUE) {
    if (cliCompareStringToArgument("get", 2) == TRUE) {
      emberSerialPrintf(APP_SERIAL, "mfg get power 0x%x\r\n\r\n", 
                        mfglibGetPower());
    }
    else if (cliCompareStringToArgument("set", 2) == TRUE) {
      if (cliCompareStringToArgument("mode", 4) == TRUE) {
        int16u power = cliGetInt16uFromArgument(3);
        int16u powermode = cliGetInt16uFromArgument(5);
        // can use EMBER_TX_POWER_MODE_DEFAULT = 0
        //         EMBER_TX_POWER_MODE_BOOST = 1
        //         EMBER_TX_POWER_MODE_ALTERNATE = 2
        if (powermode <= EMBER_TX_POWER_MODE_ALTERNATE) {
          status = mfglibSetPower(powermode, LOW_BYTE(power));
          emberSerialPrintf(APP_SERIAL, 
                            "mfg set power to 0x%x, mode 0x%x, status 0x%x\r\n\r\n", 
                            LOW_BYTE(power), powermode, status); 
        }
      }
    }
  }

  // error
  else {
    emberSerialPrintf(APP_SERIAL, 
         "mfg start <want callback, 0=False, 1=True>\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg end\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg tone start\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "mfg tone stop\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg stream start\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg stream stop\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, 
         "mfg send <numPackets> test-packet <length>\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg send <numPackets> random <length>\r\n");
    emberSerialPrintf(APP_SERIAL, 
         "mfg send <numPackets> message <first 8 bytes> <second 8 bytes>\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "mfg channel set <channel 11-26>\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg channel get\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "mfg power set <power> mode <0=default, 1=boost, 2=alternate>\r\n");
    emberSerialPrintf(APP_SERIAL, "mfg power get\r\n");
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }
  
}

// *****************************
// msResetCB
// *****************************
void msResetCB(void)
{
  ezspClose();
  halReboot();
}

void msHelpCB(void);

// *****************************
// required setup for command line interface (CLI)
// see app/util/serial/cli.h for more information
// *****************************
cliSerialCmdEntry cliCmdList[] = {
  {"help", msHelpCB},
  {"version", msVersionCB},
  {"info", msInfoCB},
  {"mfg", msMfgCB},
  {"network", msNetworkCB},
  {"token", msTokenCB},
  {"reset", msResetCB}
};
int8u cliCmdListLen = sizeof(cliCmdList)/sizeof(cliSerialCmdEntry);
PGM_P cliPrompt = "mfg-sample-host";

// *****************************
// msHelpCB
// *****************************
void msHelpCB() {
  int8u i;
  for (i=0; i<cliCmdListLen; i++) {
    emberSerialPrintf(APP_SERIAL, "%p\r\n", cliCmdList[i].cmd);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}
