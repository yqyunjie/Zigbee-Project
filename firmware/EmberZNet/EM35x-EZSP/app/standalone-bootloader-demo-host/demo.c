// *******************************************************************
//  demo.c
//
//  sample app for Ember Stack API
//
//  This is an example of a bootloading application using the Ember
//  ZigBee stack.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#define USE_HARDCODED_NETWORK_SETTINGS

#include "app/util/bootload/bootload-ezsp.h"

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/serial/serial.h"
#include "hal/micro/bootloader-interface-standalone.h"
#ifndef GATEWAY_APP
#ifdef HAL_HOST
  #include "hal/host/spi-protocol-common.h"
#else //HAL_HOST
  #include "hal/micro/avr-atmega/spi-protocol.h"
#endif //HAL_HOST
#endif
#include <string.h>
#include <stdlib.h>

#include "app/util/ezsp/serial-interface.h"
#include "app/standalone-bootloader-demo-host/xmodem-spi.h"
#include "app/standalone-bootloader-demo-host/common.h"
#include "app/util/bootload/bootload-utils-internal.h"
#include "app/util/bootload/bootload-utils.h"
#include "app/util/bootload/bootload-ezsp-utils.h"
#include "app/util/serial/cli.h"

#include "app/util/gateway/gateway.h"

#ifdef GATEWAY_APP
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include <unistd.h>
// For those pesky POSIX compliant OSes
#if !defined(WIN32) && !defined(Sleep)
#define Sleep(time) usleep(time)
#endif
#ifndef halCommonDelayMicroseconds
#define halCommonDelayMicroseconds(X) Sleep(X)
#endif
#endif

#define BOOTLOADER_DEMO_POWER   (-1)
#define BOOTLOADER_DEMO_CHANNEL (11)
#define BOOTLOADER_DEMO_PAN_ID  (0x0113)
int8u BOOTLOADER_DEMO_EXT_PAN_ID[8] = {
  0xff, 0xff, 0xff, 0xff,  
  0xff, 0xff, 0xff, 0xff
};

//----------------------------------------------------------------
// Messages
//
// The cluster IDs we use.

#define ENABLE_JOINING_MESSAGE   0x11      // <duration:1>
#define VERSION_MESSAGE          0x12      // <EUI64:8> <app id:1> <version:1>
#define QUERY_MESSAGE            0x13      // no arguments

// Application specific information.  
#define APPLICATION_ID      0xBD  // Bootloader Demo
#define APPLICATION_VERSION 0x02
#define USE_KEY_WHEN_JOINING FALSE
// We always want to talk to the entire network.
#define BROADCAST_RADIUS EMBER_MAX_HOPS

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
// Application specific constants and globals

// buffer for organizing data before we send a datagram
int8u globalBuffer[128];

// passthru and query start time
int32u passThruStart;
int32u queryStart;

int8u resetType;

// Example version field of 0x1234 is version 1.2 build 34
int16u bootloader_version = 0x2000;

// End application specific constants and globals
// *******************************************************************

// *******************************************************************
// Forward declarations

void bootUpEm260(void);
static void ezspTaskTick(void);
void appSetLEDsToInitialState(void);
void printEUI64(int8u port, int8u* eui);
void setSecurityState(int16u bmask);

extern cliSerialCmdEntry cliCmdList[];
extern int8u cliCmdListLen;
extern PGM_P cliPrompt;

// *******************************************************************
// Begin main application loop

int main( MAIN_FUNCTION_PARAMETERS )
{
  EmberStatus status;
  EmberEUI64 eui;
#ifdef GATEWAY_APP
  int8u resetType2;
#endif

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  resetType = halGetResetInfo();

  status = gatewayInit( MAIN_FUNCTION_ARGUMENTS );
  if (status == EMBER_SUCCESS) {
    gatewayCliInit(cliPrompt, cliCmdList, cliCmdListLen);
  } else if (status != EMBER_LIBRARY_NOT_PRESENT) {
    return 1;
  }

  // inititialize the serial port
  // good to do this before ezspUtilInit, that way any errors that occur
  // can be printed to the serial port.
  // xmodem does not work on the avr32 when above 38400
  emberSerialInit(APP_SERIAL, BAUD_38400, PARITY_NONE, 1);

  emberSerialGuaranteedPrintf(APP_SERIAL, "Reset(0x%x):%p\r\n",
                              resetType, halGetResetString());

#ifdef ENABLE_BOOTLOAD_PRINT
  emberSerialPrintf(APP_SERIAL, "ENABLE_BOOTLOAD_PRINT build\r\n");
  emberSerialWaitSend(APP_SERIAL);
#endif
#ifdef ENABLE_DEBUG
  emberSerialPrintf(APP_SERIAL, "ENABLE_DEBUG build\r\n");
  emberSerialWaitSend(APP_SERIAL);
#endif
#ifdef DEBUG
  emberSerialPrintf(APP_SERIAL, "DEBUG build\r\n");
  emberSerialWaitSend(APP_SERIAL);
#endif

  // this calls ezspInit, ezspUtilInit, bootloadUtilInit and emberNetworkInit
  // and checks and prints status
  bootUpEm260();

#ifdef GATEWAY_APP
  resetType2 = halGetResetInfo();
  if (resetType != resetType2) {
    emberSerialGuaranteedPrintf(APP_SERIAL, "Reset2(0x%x):%p\r\n",
                                resetType2, halGetResetString());
    emberSerialGuaranteedPrintf(APP_SERIAL, "  Unexpected reset change!\r\n");
  }
  emberSerialGuaranteedPrintf(APP_SERIAL,
                    "After bootUpEm260: ncpError=0x%x, ashError=0x%x\r\n",
                     ncpError, ashError);
#endif

  // print a startup message
  emberSerialPrintf(APP_SERIAL,
                    "\r\nINIT: standalone-bootloader-demo-host ");
  ezspGetEui64(eui);
  printEUI64(APP_SERIAL, eui);
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

  // initialize the cli library to use the correct serial port. APP_SERIAL 
  // is defined in the makefile or project-file depending on the platform.
  cliInit(APP_SERIAL);

  // event loop
  while(TRUE) {
    halResetWatchdog();
    ezspTaskTick();           // Allow ezsp tasks to run
    bootloadUtilTick();       // Allow the bootload utils to run
    cliProcessSerialInput();  // process serial input from ths user
  }

}

// **************************
// this is called from main to call ezspInit, ezspUtilInit, bootloadUtilInit,
// and emberNetworkInit
// **************************
void bootUpEm260(void)
{
  EzspStatus stat;
  EmberStatus status;
  EmberNodeType nodeType;
  EmberNetworkParameters parameters;

  //reset the EM260 and wait for it to come back
  //this will guarantee that the two MCUs start in the same state
  stat = ezspInit();
  if(stat != EZSP_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(
      APP_SERIAL,
      "ERROR: ezspInit 0x%x\r\n", stat);
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
  status = ezspUtilInit(APP_SERIAL);
  if (status != EMBER_SUCCESS) {
    // report status here
    emberSerialGuaranteedPrintf(APP_SERIAL,
              "ERROR: ezspUtilInit 0x%x\r\n", status);
    // the app can choose what to do here, if the app is running
    // another device then it could stay running and report the
    // error visually for example. This app asserts.
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: ezspUtilInit passed\r\n");
  }


  // init application state, call bootloadUtilInit
  queryStart = passThruStart = halCommonGetInt32uMillisecondTick();
  // initialized app port and bootload port.  In case of gateway app, we set
  // bootloadSerial port to 0 to match BACKCHANNEL_SERIAL_PORT defined in 
  // backchannel.h.
  #ifdef GATEWAY_APP
    bootloadUtilInit(APP_SERIAL, 0);
  #else
    bootloadUtilInit(APP_SERIAL, APP_SERIAL);
  #endif

  // try and rejoin the network this node was previously a part of
  // if status is not EMBER_SUCCESS then the node didn't rejoin it's
  // old network.
  status = ezspGetNetworkParameters(&nodeType, &parameters);
  
  if ((status == EMBER_SUCCESS) &&
      (emberNetworkInit() == EMBER_SUCCESS))
  {
    // were able to join the old network
    emberSerialPrintf(APP_SERIAL,
         "Joining network - channel 0x%x, panid 0x%2x\r\n",
         parameters.radioChannel, parameters.panId);
  } else {
    // were not able to join the old network
    emberSerialPrintf(APP_SERIAL,
         "Type join to join a network\r\n");
  }
  emberSerialWaitSend(APP_SERIAL);
}


// end main application loop
// *******************************************************************

//----------------------------------------------------------------
// Message-sending utilities.

// Send a broadcast using the give cluster ID and with the first 'length'
// bytes of 'contents' as a payload.
EmberStatus sendBroadcast(int8u clusterId, int8u *contents, int8u length, int8u radius)
{
  EmberApsFrame frame;
  EmberStatus status;
  int8u sequence;

  frame.profileId = PROFILE_ID;
  frame.clusterId = clusterId;
  frame.sourceEndpoint = ENDPOINT;
  frame.destinationEndpoint = ENDPOINT;
  frame.options = 0;
  status = ezspSendBroadcast(EMBER_BROADCAST_ADDRESS, &frame, radius, 43,
                             length, contents, &sequence);

  return status;
}

// When a device receives a query, this function is called to send the
// version information
// The version information is formatted as follows:
//
// <8 byte EUI><ID><APP VER><MICRO><BL VER>
//
// Where:
//  <8 byte EUI>
//  <ID>      is one byte application ID
//  <APP VER> is one byte application version number. This is whatever
//            you set it as, in bootloader-demo.h
//  <PLAT>   is one byte platform ID: 1:AVR128, 2:EM250
//  <BL VER>  is one byte bootloader version number.  Need to know how to
//            extract this information.
void sendVersionAction(void)
{
  ezspGetEui64(&globalBuffer[0]);
  globalBuffer[8] = APPLICATION_ID;
  globalBuffer[9] = APPLICATION_VERSION;
  globalBuffer[10] = nodePlat;
  globalBuffer[11] = HIGH_BYTE(nodeBlVersion); // bootloader version
  globalBuffer[12] = LOW_BYTE(nodeBlVersion); // bootloader build

  if (sendBroadcast(VERSION_MESSAGE, globalBuffer, 13, BROADCAST_RADIUS)
      == EMBER_SUCCESS)
    emberSerialPrintf(APP_SERIAL,"Sending version information\r\n");
  else
    emberSerialPrintf(APP_SERIAL,"Could not send version information\r\n");
}

// *******************************************************************
// Start app/util/bootload/bootload-ezsp-util.c callbacks

boolean hostBootloadUtilLaunchRequestHandler(int8u lqi,
                                         int8s rssi,
                                         int16u manufacturerId,
                                         int8u *hardwareTag,
                                         EmberEUI64 sourceEui)
{

  emberSerialPrintf(APP_SERIAL,"Received Bootloader Launch Request\r\n");

#ifdef GATEWAY_APP
  if (nodePlat == 2 /* em2xx */) {
    emberSerialPrintf(APP_SERIAL,"EM2XX via EZSP-UART does not support OtA. "
                                 "Refusing to launch bootloader.\r\n");
    emberSerialWaitSend(APP_SERIAL);
    return FALSE;
  }
#endif

  // COULD DO: Compare manufacturer and hardware id arguments to known values.
  // COULD DO: Check for minimum required radio signal strength (RSSI).
  // COULD DO: Do not agree to launch the bootloader if any of the above
  // conditions are not met.  For now, always agree to launch the bootloader.

  emberSerialPrintf(APP_SERIAL,"lqi:%x rssi:%d\r\n",
                    lqi, rssi);
  emberSerialWaitSend(APP_SERIAL);

  if (lqi < 236)  {
    // Either the LQI information is not available or the reported
    // LQI indicates that the link too poor to proceed.
    emberSerialPrintf(APP_SERIAL,"Link to source is not acceptable (LQI). "
                                 "Refusing to launch bootloader.\r\n");
    emberSerialWaitSend(APP_SERIAL);
    return FALSE;
  }

  emberSerialPrintf(APP_SERIAL,
                    "Starting Launch Authentication Protocol...\r\n");
  emberSerialWaitSend(APP_SERIAL);
  return TRUE;
}

void hostBootloadUtilQueryResponseHandler(int8u lqi,
                                          int8s rssi,
                                          boolean bootloaderActive,
                                          int16u manufacturerId,
                                          int8u *hardwareTag,
                                          EmberEUI64 targetEui,
                                          int8u bootloaderCapabilities,
                                          int8u platform,
                                          int8u micro,
                                          int8u phy,
                                          int16u blVersion)
{
  int8u i;
  int8u blV, blB;
  int32u deltaTime;

  deltaTime = halCommonGetInt32uMillisecondTick() - queryStart;
  emberSerialPrintf(APP_SERIAL,"@%d ms:\r\n", deltaTime);
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL,"QQ rx eui: ");
  printEUI64(APP_SERIAL, targetEui);

  emberSerialPrintf(APP_SERIAL," blActive:%x ", bootloaderActive);

  blV = HIGH_BYTE(blVersion); // bootloader version
  blB = LOW_BYTE(blVersion); // bootloader build

  emberSerialPrintf(APP_SERIAL,
                    "blVersion:%2x blV:%x blB:%x ",
                    blVersion, blV, blB);

  emberSerialPrintf(APP_SERIAL,"\r\nmfgId:%2x", manufacturerId);

  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL," hwTag:");

  for(i=0; i< BOOTLOAD_HARDWARE_TAG_SIZE; ++i) {
    emberSerialPrintf(APP_SERIAL, "%x", hardwareTag[i]);
  }
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL,
                    "\r\nblCap:%x plat:%x micro:%x phy:%x ",
                    bootloaderCapabilities, platform, micro, phy);

  emberSerialPrintf(APP_SERIAL,"lqi:%x rssi:%d QQ\r\n",
                    lqi,rssi);
  emberSerialWaitSend(APP_SERIAL);
}

void hostBootloadReinitHandler(void)
{
  bootUpEm260();
}

// End bootload-ezsp-utils callbacks
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
  if(status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, 
      "Error: sending msg type 0x%x status 0x%x\r\n", type, status);
  }
}

// called when a message is received
void ezspIncomingMessageHandler(EmberIncomingMessageType type,
                                EmberApsFrame *apsFrame,
                                int8u lastHopLqi,
                                int8s lastHopRssi,
                                EmberNodeId sender,
                                int8u bindingTableIndex,
                                int8u datagramReplyTag,
                                int8u length,
                                int8u *message)
{
  int8u dataIndex = 0;

  // Called with an incoming message
  // APS messages contain a 3 byte AF header, so account for that
  if (type == EMBER_INCOMING_UNICAST)
    dataIndex = 3;

  // Limit radio activities when doing boootload.
  if(!IS_BOOTLOADING) {

    // ********************************************
    // handle the incoming message
    // ********************************************
    switch (apsFrame->clusterId) {

    case ENABLE_JOINING_MESSAGE:
      emberSerialPrintf(APP_SERIAL, "rx enable joining\r\n");
      emberPermitJoining(message[dataIndex+0]);
      break;
    case VERSION_MESSAGE:
      emberSerialPrintf(APP_SERIAL,"QQ rx version: ");
      printEUI64(APP_SERIAL, &message[dataIndex+0]);
      emberSerialPrintf(APP_SERIAL,
                        " app:%x ver:%x plat:%x blV:%x blB:%x QQ\r\n",
                        message[dataIndex+8], message[dataIndex+9],
                        message[dataIndex+10], message[dataIndex+11],
                        message[dataIndex+12]);
      break;
    case QUERY_MESSAGE:
      sendVersionAction();
      emberSerialPrintf(APP_SERIAL, "rx version query\r\n");
      break;
    default:
      emberSerialPrintf(APP_SERIAL, "rx unhandled application message\r\n");
      break;
    }

  } else {
   emberSerialPrintf(APP_SERIAL,
     "NOTE: in bootloading, will limit radio activities\r\n");
  }
  emberSerialWaitSend(APP_SERIAL);
}

// this is called when the stack status changes
void ezspStackStatusHandler(EmberStatus status)
{
  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
          "EVENT: stackStatus now EMBER_NETWORK_UP\r\n");
    {
      EmberStatus status;
      EmberNodeType nodeType;
      EmberNetworkParameters params;
      status = ezspGetNetworkParameters(&nodeType, &params);
      if (status == EMBER_SUCCESS) {
        emberSerialPrintf(APP_SERIAL,
             "EVENT: network joined - channel 0x%x, panid 0x%2x\r\n",
             params.radioChannel, params.panId);
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
  emberSerialWaitSend(APP_SERIAL);
}

// this is called when a scan is complete
void ezspScanCompleteHandler( int8u channel, EmberStatus status )
{}

// this is called when a network is found when app is performing scan
void ezspNetworkFoundHandler(
      EmberZigbeeNetwork *networkFound,
      int8u lastHopLqi,
      int8s lastHopRssi)
{}

void halNcpIsAwakeIsr(boolean isAwake)
{}

void ezspErrorHandler(EzspStatus status)
{
  // Set last bootload ezsp error info
  // see app/util/bootload/bootload-ezsp-utils.c for more information
  bootloadEzspLastError = status;

  if (status == ignoreNextEzspError) {
    ignoreNextEzspError = EZSP_SUCCESS;
    return;
  }
  ignoreNextEzspError = EZSP_SUCCESS;
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              "\r\nERROR: ezspErrorHandler 0x%x:",
                              status);
#ifndef GATEWAY_APP
  emberSerialGuaranteedPrintf(
    APP_SERIAL,
    " cause value 0x%x\r\n",
    halNcpSpipErrorByte);
#else
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              " ncpError=0x%x, ashError=0x%x\r\n",
                              ncpError, ashError);
#endif
}



// end Ember callback handlers
// *******************************************************************


// ezspTaskTick - called to do ezspTick() and check for ezsp callbacks.
static void ezspTaskTick(void)
{
  ezspTick();         // Allow the ezsp  to run
  while (ezspCallbackPending()) {
    ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;
    ezspCallback();
  }
}

// macro for use by next function which prints the bootload serial menu
#define PRINT_AND_WAIT(x) \
    emberSerialPrintf(APP_SERIAL, x); \
    emberSerialWaitSend(APP_SERIAL);

void printSerialBtlMenu(void)
{
  //print the btl name and version
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialPrintf(APP_SERIAL, "NCP Bootloader(host)");
  emberSerialPrintf(APP_SERIAL, " v%x", HIGH_BYTE(bootloader_version));
  emberSerialPrintf(APP_SERIAL, " b%x\r\n", LOW_BYTE(bootloader_version));
  emberSerialWaitSend(APP_SERIAL);
  //print the menu
  PRINT_AND_WAIT("1. upload ncp app image(.ebl)\r\n");
  PRINT_AND_WAIT("2. switch to bootloader via EZSP\r\n");
  PRINT_AND_WAIT("3. switch to bootloader via reset\r\n");
  PRINT_AND_WAIT("4. switch to app reset\r\n");
  PRINT_AND_WAIT("5. get running stack version\r\n");
  PRINT_AND_WAIT("6. exit\r\n");
  PRINT_AND_WAIT("BLH > ");
}

// bootload serial menu. This is called from the command line using 
// "bootload serial-menu"
void demoBootloadSerial(void)
{
  int8u ch = 0;
  boolean runFlag = TRUE;
  EzspStatus ezspStat;
  int8u protocolVersion;
  int8u stackId;
  int16u stackVer;

  printSerialBtlMenu();

  while (runFlag)  {
    halResetWatchdog();

#ifdef GATEWAY_APP
    halCommonDelayMicroseconds(1);
#endif

    if (emberSerialReadByte(APP_SERIAL, &ch) == 0) {
      emberSerialWriteByte(APP_SERIAL, ch);
      switch (ch) {
      case '1': //upload em260 app image(.ebl)
      {
#ifndef GATEWAY_APP
        int16u i;
        int32u startTime, deltaTime;
        int8u len;
        int8u * queryResp;
#endif

#ifndef GATEWAY_APP
        queryResp = XModemQuery(&len, TRUE);
        if (queryResp) { // If bootloader
          emberSerialPrintf(APP_SERIAL, "\r\nStarting SPI bootloading...\r\n");
          emberSerialWaitSend(APP_SERIAL);
          passThruStart = halCommonGetInt32uMillisecondTick();
        } else {
          emberSerialPrintf(APP_SERIAL,
                            "\r\nEM260 not currently running bootloader!\r\n");
          emberSerialWaitSend(APP_SERIAL);
          break;
        }

        startTime = halCommonGetInt32uMillisecondTick();

        bootloadUtilStartBootload(emberGetEui64(), BOOTLOAD_MODE_PASSTHRU);

        halResetWatchdog();
        for(i=0; i<30000; i++) {
          halCommonDelayMicroseconds(100);
          halResetWatchdog();
          bootloadUtilTick();
          if (!IS_BOOTLOADING)
            break;
        }

        emberSerialPrintf(APP_SERIAL, "\r\nBootload end\r\n");
        emberSerialWaitSend(APP_SERIAL);

        deltaTime = halCommonGetInt32uMillisecondTick() - startTime;
        emberSerialPrintf(APP_SERIAL,"\r\nwaited %d loops for %d secs\r\n",
                          i, deltaTime);
        emberSerialWaitSend(APP_SERIAL);
        // Redo em260 startup -- needed to get back to normal state.
        bootUpEm260();
        // Do one fetch of callbacks if enabled
        ezspTaskTick();
#else
        emberSerialPrintf(APP_SERIAL, "SPI bootloading is not available on this host!\r\n");
#endif
        break;
      }
      case '2': // switch to bootloader via EZSP
        emberSerialPrintf(APP_SERIAL, "\r\nstarting bootloader...\r\n");
        emberSerialWaitSend(APP_SERIAL);

        ezspStat = hostLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);

        if (ezspStat != EZSP_SUCCESS) {
          if (ezspStat == EZSP_ERROR_VERSION_NOT_SET) {
            emberSerialPrintf(APP_SERIAL, "could not send EZSP command. You must send an EZSP version command (option '5') first, ezspStat=0x%x\r\n", ezspStat);
          } else {
            emberSerialPrintf(APP_SERIAL, "reset NCP failed...Maybe NCP has no bootloader! ezspStat=0x%x\r\n", ezspStat);
          }
          
          emberSerialWaitSend(APP_SERIAL);
          break;
        } else {
#ifndef GATEWAY_APP
          emberSerialPrintf(APP_SERIAL, "now EM260 in standalone bootloader mode...\r\n");
#else
          emberSerialPrintf(APP_SERIAL, "now NCP in standalone bootloader mode...\r\n");
          ezspClose();
          exit(0);
#endif
        }
        break;
      case '3': // switch to bootloader via reset
#ifndef GATEWAY_APP
        emberSerialPrintf(APP_SERIAL, 
                          "\r\nResetting with bootload request em260...\r\n");
        emberSerialWaitSend(APP_SERIAL);

        //reset the EM260 and wait for it to come back
        //this will guarantee that the two MCUs start in the same state
        ezspStat = halNcpHardResetReqBootload(TRUE);
        if(ezspStat != EZSP_SUCCESS) {
          // report status here
          emberSerialGuaranteedPrintf(
            APP_SERIAL,
            "ERROR: halNcpHardResetReqBootload 0x%x\r\n", ezspStat);
        } else {
          emberSerialPrintf(APP_SERIAL, "now NCP in standalone bootloader mode...\r\n");
        }
#else
        emberSerialPrintf(APP_SERIAL, "Hardware reset is not available on this host!\r\n");
#endif
        break;
      case '4': // switch to app reset
        emberSerialPrintf(APP_SERIAL, "\r\nresetting ncp...\r\n");
        emberSerialWaitSend(APP_SERIAL);

        //reset the EM260 and wait for it to come back
        //this will guarantee that the two MCUs start in the same state
        ezspStat = ezspInit();
        if(ezspStat != EZSP_SUCCESS) {
          // report status here
          emberSerialGuaranteedPrintf(
            APP_SERIAL,
            "ERROR: ezspInit 0x%x\r\n", ezspStat);
        } else {
          emberSerialPrintf(APP_SERIAL, "ezspInit passed\r\n");
        }
        break;
      case '5': // get running stack version
      {
#ifndef GATEWAY_APP
        int8u len;
        int8u * queryResp;
#endif

#ifndef GATEWAY_APP
        queryResp = XModemQuery(&len, FALSE);
        if (queryResp) { // If bootloader
          emberSerialPrintf(APP_SERIAL, "\r\nGetting XModemQuery()...\r\n");
          emberSerialWaitSend(APP_SERIAL);
          // queryResp[0] == bootloader message type (always 'R')
          // queryResp[1] == Bootloader active flag (always = 1).
          // queryResp[2..3] == manufacturer's Id.
          // queryResp[4..19] == hardware tag.
          // queryResp[20] == Bootloader capabilities mask.
          // queryResp[21] == PLATform designator.
          // queryResp[22] == MICROcontroller designator.
          // queryResp[23] == PHYsical layer designator.
          // queryResp[24..25] == Bootloader Version.
          if (len >= 24) {
            emberSerialPrintf(APP_SERIAL, "bootloader ver %x%x\r\n",
                          queryResp[24], queryResp[25]);
            bootloader_version = 
              (int16u)HIGH_LOW_TO_INT(queryResp[24], queryResp[25]);
          }
          else
            emberSerialPrintf(APP_SERIAL,
                              "bootload unexpected length %d should be >= 24",
                              len);
        } else {
#endif
          emberSerialPrintf(APP_SERIAL, "\r\nGetting ezspVersion(EZSP_PROTOCOL_VERSION)...\r\n");
          emberSerialWaitSend(APP_SERIAL);
          bootloadEzspLastError = EZSP_SUCCESS;
          protocolVersion = ezspVersion(EZSP_PROTOCOL_VERSION, &stackId, &stackVer);
          if (bootloadEzspLastError == EZSP_SUCCESS)
            emberSerialPrintf(APP_SERIAL,
                              "ezsp ver %x stack id %x stack ver %2x\r\n",
                              protocolVersion, stackId, stackVer);
          else
            emberSerialPrintf(APP_SERIAL, "ezsp not available\r\n");
#ifndef GATEWAY_APP
        }
#endif
        break;
      }
      case '6': // exit the bootloader menu
        emberSerialPrintf(APP_SERIAL, "\r\nLeaving bootload menu...\r\n");
        emberSerialWaitSend(APP_SERIAL);
        runFlag = FALSE;
        break;
      // Debug Commands.  Note that these are not included in the printed
      // menu.
      case 'b': // bootUpEm260()
        emberSerialPrintf(APP_SERIAL, "ootUpEm260()...\r\n");
        emberSerialWaitSend(APP_SERIAL);
        // Redo em260 startup -- needed to get back to normal state.
        bootUpEm260();
        break;
      default:
        printSerialBtlMenu();
        break;
      }
    } else {
      ezspTaskTick();         // Needed to keep ASH happy.
    }
  }
  // get ezsp version before leaving the serial menu.  this is because we
  // need to send the version as the first command after the ncp comes up.
  ezspVersion(EZSP_PROTOCOL_VERSION, &stackId, &stackVer);
}

// this runs a remote bootload to the EUI specified. called from the
// command line by "bootload remote <eui>"
void demoBootloadRemote(EmberEUI64 bootloadNodeId)
{
  EmberStatus status;
  // The key that is used to encrypt the challenge issued from the
  // target node as part of the bootloader launch authentication
  // protocol.  It is normally stored as a manufacturing token.
  int8u encryptKey[16];

  int16u mfgId = 0x1234;
  int8u hardwareTag[16] = {
    'E', 'M', 'B', 'E',
    'R', ' ', 'D', 'E',
    'V', ' ', 'K', 'I',
    'T', '!', '!', '!'
  };
  int16u i;
  int32u startTime, deltaTime;

  startTime = halCommonGetInt32uMillisecondTick();

  // Load the secret key to encrypt the challenge.
  // TOKEN_MFG_BOOTLOAD_AES_KEY has index 5
  ezspGetMfgToken(5, (int8u*)&encryptKey);

  // We now want to send a bootload message to request the target node
  // to go into bootload mode.
  status = bootloadUtilSendRequest(bootloadNodeId,
                                   mfgId,
                                   hardwareTag,
                                   encryptKey,
                                   BOOTLOAD_MODE_PASSTHRU);
  if (status == EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Attempting to bootload remote node: ");
    printEUI64(APP_SERIAL, bootloadNodeId);
    emberSerialPrintf(APP_SERIAL, "\r\n");
    for(i=0; i<1000; i++) {
      halCommonDelayMicroseconds(100);
      halResetWatchdog();
      ezspTaskTick();
      bootloadUtilTick();
      if (!IS_BOOTLOADING)
        break;
    }

    emberSerialPrintf(APP_SERIAL, "\r\nRR Remote end RR\r\n");
    emberSerialWaitSend(APP_SERIAL);

    deltaTime = halCommonGetInt32uMillisecondTick() - startTime;
    emberSerialPrintf(APP_SERIAL,"We waited for %d ms\r\n",
                      deltaTime);
  } else {
    emberSerialPrintf(APP_SERIAL, "Failed bootloadUtilSendRequest()=0x%x\r\n",
                      status);
  }
  emberSerialWaitSend(APP_SERIAL);
}


// this runs a recover bootload to the EUI specified. called from the
// command line by "bootload recover <eui>"
void demoBootloadRecover(EmberEUI64 bootloadNodeId)
{
  bootloadMode mode = BOOTLOAD_MODE_PASSTHRU;
  emberSerialPrintf(APP_SERIAL, "Attempting recovery: ");

  // Pass in the target eui64 (all 0xFF in case of broadcast) and bootload mode
  bootloadUtilStartBootload(bootloadNodeId, mode);

  printEUI64(APP_SERIAL, bootloadNodeId);
  emberSerialPrintf(APP_SERIAL, " mode %d\r\n", mode);
}


// this runs a recover bootload on the default channel. Called from the
// command line by "bootload default-recover"
void demoBootloadDefault(void)
{
  EmberEUI64 bootloadNodeId;
  int16u i;
  int8u j;

  // Set the target node id to all 0xFF for broadcast message
  MEMSET(bootloadNodeId, 0xFF, EUI64_SIZE);

  emberSerialPrintf(APP_SERIAL, "Attempting default recovery...\r\n");
  emberSerialWaitSend(APP_SERIAL);

  // Set channel.  Note that this functions below used to set
  // channel are ember internal calls.  They should not be used
  // anywhere else, especially in the normal application.  If you
  // need API calls with similar feature, please contact
  // support@ember.com
  ezspOverrideCurrentChannel(BOOTLOADER_DEFAULT_CHANNEL);

  // Pass in the target eui64 (all 0xFF in case of broadcast) and bootload mode
  bootloadUtilStartBootload(bootloadNodeId, BOOTLOAD_MODE_PASSTHRU);

  for(i=0; i<1000; i++) {
    for(j=0; j<150; j++) {
      halCommonDelayMicroseconds(100);
      halResetWatchdog();
      ezspTaskTick();
      bootloadUtilTick();
      if (!IS_BOOTLOADING)
        break;
    }
    if (!IS_BOOTLOADING)
      break;
  }

  emberSerialWaitSend(APP_SERIAL);
  if (i>=1000) {
    emberSerialPrintf(APP_SERIAL, "\r\nBootloading failed to stop.\r\n");
  } else {
    emberSerialPrintf(APP_SERIAL, "\r\nDefault recovery end.\r\n");
  }
  emberSerialWaitSend(APP_SERIAL);

  // Redo em260 startup -- needed to get back to normal state.
  bootUpEm260();
}

// *******************************
// utility function for reading an EUI from a command-line argument
// *******************************
void getEui64Argument(int8u argument, int8u* bootloadNodeId)
{
  int8u i;

  for (i = 0; i<8; i++) { 
    bootloadNodeId[i] = cliGetHexByteFromArgument(i, argument);
  }
}

// *******************************
// This is a callback (CB) for handling the command "query"
//    query network
//    query neighbor
//    query node
// *******************************
void demoQueryCB(void)
{
  int16u i;
  int32u deltaTime;
  EmberEUI64 bootloadNodeId;

  // ****************
  // query network
  // ****************
  if (cliCompareStringToArgument("network", 1) == TRUE) {
    emberSerialPrintf(APP_SERIAL, "QQ query network start QQ\r\n");
    queryStart = halCommonGetInt32uMillisecondTick();
    if(sendBroadcast(QUERY_MESSAGE, NULL, 0, BROADCAST_RADIUS) == EMBER_SUCCESS)
    {
      for(i=0; i<300; i++) {
        halCommonDelayMicroseconds(100);
        halResetWatchdog();
        ezspTaskTick();
      }
    }
    emberSerialPrintf(APP_SERIAL, "\r\nQQ query end QQ\r\n");
    emberSerialWaitSend(APP_SERIAL);
    return;
  }
  // ****************
  // query neighbor
  // ****************
  else if (cliCompareStringToArgument("neighbor", 1) == TRUE) {
    
    // Set the target node id to all 0xFF for broadcast message
    MEMSET(bootloadNodeId, 0xFF, EUI64_SIZE);
    
    emberSerialPrintf(APP_SERIAL, "QQ query neighbor start QQ\r\n");
    // the real work is done below
  }
  // ****************
  // query node
  // ****************
  else if (cliCompareStringToArgument("node", 1) == TRUE) {

    // The PC passes us the target EUI64 as a command argument.
    getEui64Argument(2, bootloadNodeId);
    
    emberSerialPrintf(APP_SERIAL, "QQ query node start QQ\r\n");    
    // the real work is done below
  }
  // invalid input - print usage
  else {
    emberSerialPrintf(APP_SERIAL, "query network\r\n");
    emberSerialPrintf(APP_SERIAL, "query neighbor\r\n");
    emberSerialPrintf(APP_SERIAL, "query node <EUI in hex>\r\n\r\n");
    return;
  }

  // ********
  // the code below is common to "query neighbor" and "query node"
  // ********

  queryStart = halCommonGetInt32uMillisecondTick();
    
  // Send MAC level broadcast query to all neighbors.
  bootloadUtilSendQuery(bootloadNodeId);
  
  halResetWatchdog();
  for(i=0; i<30000; i++) {
    halCommonDelayMicroseconds(100);
    halResetWatchdog();
    ezspTaskTick();
    bootloadUtilTick();
    if (!IS_BOOTLOADING)
      break;
  }
  
  emberSerialPrintf(APP_SERIAL, "\r\nQQ query end QQ\r\n");
  emberSerialWaitSend(APP_SERIAL);
  
  deltaTime = halCommonGetInt32uMillisecondTick() - queryStart;
  emberSerialPrintf(APP_SERIAL,"\r\nwaited %d loops for %d ms\r\n",
                    i, deltaTime);
  emberSerialWaitSend(APP_SERIAL);
}

// *******************************
// This is a callback (CB) for handling the command "bootload"
//    bootload serial-menu
//    bootload remote <target EUI>
//    bootload recover <target EUI>
//    bootload default-recover
// *******************************
void demoBootloadCB(void)
{
  EmberEUI64 eui;

  if (cliCompareStringToArgument("serial", 1) == TRUE) {
    demoBootloadSerial();
  }
  else if (cliCompareStringToArgument("remote", 1) == TRUE) {
    getEui64Argument(2, eui);
    demoBootloadRemote(eui);
  }
  else if (cliCompareStringToArgument("recover", 1) == TRUE) {
    getEui64Argument(2, eui);
    demoBootloadRecover(eui);
  }
  else if (cliCompareStringToArgument("default", 1) == TRUE) {
    demoBootloadDefault();
  }
  else {
    emberSerialPrintf(APP_SERIAL, "bootload serial-menu\r\n");
    emberSerialPrintf(APP_SERIAL, "bootload remote <target EUI>\r\n");
    emberSerialPrintf(APP_SERIAL, "bootload recover <target EUI>\r\n");
    emberSerialPrintf(APP_SERIAL, "bootload default-recover\r\n\r\n");
  }
}
  
// *******************************
// This is a callback (CB) for handling the command "network"
//    network form (args are optional) <channel> <power> <panid in hex>
//    network join (args are optional) <channel> <power> <panid in hex>
//    network leave
//    network pjoin
// *******************************
void demoNetworkCB(void)
{
  EmberStatus status;
  int8u nodeType;
  EmberNetworkParameters networkParams;
  int16u bmask;

  // network form <channel> <power> <panid in hex>
  if (cliCompareStringToArgument("form", 1) == TRUE)
  {
    networkParams.radioChannel = cliGetInt16uFromArgument(2);
    networkParams.radioTxPower = cliGetInt16uFromArgument(3);
    networkParams.panId = (cliGetHexByteFromArgument(0, 4) * 256) + 
      (cliGetHexByteFromArgument(1, 4));
    MEMCOPY(networkParams.extendedPanId,
            BOOTLOADER_DEMO_EXT_PAN_ID,
            EXTENDED_PAN_ID_SIZE);

    // provide defaults if the user did not specify any
    if ((cliGetHexByteFromArgument(0,2) == 0) &&
        (cliGetHexByteFromArgument(0,3) == 0) && 
        (cliGetHexByteFromArgument(0,4) == 0) &&
        (cliGetHexByteFromArgument(1,4) == 0)) {
      networkParams.radioChannel = BOOTLOADER_DEMO_CHANNEL;
      networkParams.radioTxPower = BOOTLOADER_DEMO_POWER;
      networkParams.panId = BOOTLOADER_DEMO_PAN_ID;
    }
    bmask = EMBER_NO_TRUST_CENTER_MODE
            | EMBER_TRUST_CENTER_GLOBAL_LINK_KEY
            | EMBER_HAVE_PRECONFIGURED_KEY
            | EMBER_HAVE_NETWORK_KEY;
    setSecurityState(bmask);
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

    // provide defaults if the user did not specify any
    if ((cliGetHexByteFromArgument(0,2) == 0) &&
        (cliGetHexByteFromArgument(0,3) == 0) && 
        (cliGetHexByteFromArgument(0,4) == 0) &&
        (cliGetHexByteFromArgument(1,4) == 0)) {
      networkParams.radioChannel = BOOTLOADER_DEMO_CHANNEL;
      networkParams.radioTxPower = BOOTLOADER_DEMO_POWER;
      networkParams.panId = BOOTLOADER_DEMO_PAN_ID;
    }
	networkParams.joinMethod = EMBER_USE_MAC_ASSOCIATION;
	
    bmask = EMBER_GLOBAL_LINK_KEY
            | EMBER_HAVE_PRECONFIGURED_KEY
            | EMBER_REQUIRE_ENCRYPTED_KEY;
    setSecurityState(bmask);
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

  // network pjoin
  else if (cliCompareStringToArgument("pjoin", 1) == TRUE)
  {
    int8u permitJoinDuration = 60;
    status = emberPermitJoining(permitJoinDuration);
    emberSerialPrintf(APP_SERIAL, "permit join %x status 0x%x\r\n\r\n", 
                      permitJoinDuration, status);
    return;
  }

  // network init
  else if (cliCompareStringToArgument("init", 1) == TRUE)
  {
    status = emberNetworkInit();
    if(status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, "network init failed status 0x%x\r\n\r\n", 
                        status);
    }
    return;
  }

  else {
    emberSerialPrintf(APP_SERIAL, 
      "network form (args are optional) <channel> <power> <panid in hex>\r\n");
    emberSerialPrintf(APP_SERIAL, 
      "network join (args are optional) <channel> <power> <panid in hex>\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "network leave\r\n");
    emberSerialPrintf(APP_SERIAL, "network pjoin\r\n");
    emberSerialPrintf(APP_SERIAL, "network init\r\n");
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }
}

// *****************************
// printEUI64 - utility function
// *****************************
void printEUI64(int8u port, int8u* eui) {
  int8u i;

  emberSerialPrintf(port, "EUI: ");
  for (i=0; i<8; i++) {
    emberSerialPrintf(port, "%x", eui[i]);
  }
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


// *******************************
// This is a callback (CB) for handling the command "info"
// *******************************
void demoInfoCB(void)
{
  int16u stackVer;
  int8u protocolVersion;
  int8u stackId;
  EmberNetworkStatus networkState;
  EmberEUI64 eui;

  emberSerialPrintf(APP_SERIAL, "standalone-bootloader-demo-host 1.0\r\n");
  ezspGetEui64(eui);
  printEUI64(APP_SERIAL, eui);
  protocolVersion = ezspVersion(EZSP_PROTOCOL_VERSION, &stackId, &stackVer);
  emberSerialPrintf(APP_SERIAL, 
                    "\r\nstack [%2x], ezspVersion [%2x], stackId [%2x]\r\n",
                    stackVer, protocolVersion, stackId);

  networkState = emberNetworkState();
  if( networkState == EMBER_JOINED_NETWORK )
  {
    EmberStatus status;
    EmberNodeType nodeType;
    EmberNetworkParameters params;

    emberSerialPrintf(APP_SERIAL, "   network is UP\r\n");
    emberSerialPrintf(APP_SERIAL, "   short ID [%2x]\r\n", emberGetNodeId());
    emberSerialWaitSend(APP_SERIAL);

    status = ezspGetNetworkParameters(&nodeType, &params);
    if (status == EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL,
                        "   chan [0x%x], power [0x%x], panId [0x%2x]\r\n",
                        params.radioChannel,
                        params.radioTxPower,
                        params.panId);
      printExtendedPanId(APP_SERIAL, params.extendedPanId);
      emberSerialWaitSend(APP_SERIAL);
    }
  } else {
    emberSerialPrintf(APP_SERIAL, "   network is not up (state = 0x%x)\r\n",
                      networkState);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

// *******************************
// This is a callback (CB) for handling the command "reboot"
// *******************************
void demoRebootCB(void)
{
  ezspClose();
  halReboot();
}

void demoHelpCB(void);

// *****************************
// required setup for command line interface (CLI)
// see app/util/serial/cli.h for more information
// *****************************
cliSerialCmdEntry cliCmdList[] = {
  {"help", demoHelpCB},
  {"query", demoQueryCB},
  {"bootload", demoBootloadCB},
  {"network", demoNetworkCB},
  {"info", demoInfoCB},
  {"reboot", demoRebootCB}
};
int8u cliCmdListLen = sizeof(cliCmdList)/sizeof(cliSerialCmdEntry);
PGM_P cliPrompt = "demo-host";

// *****************************
// This is a callback (CB) for handling the command "help"
// *****************************
void demoHelpCB() {
  int8u i;
  for (i=0; i<cliCmdListLen; i++) {
    emberSerialPrintf(APP_SERIAL, "%p\r\n", cliCmdList[i].cmd);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

void setSecurityState(int16u bmask)
{
  int8u linkKey[EMBER_ENCRYPTION_KEY_SIZE] = 
    {'e','m','b','e','r',' ','E','M','2','5','0',' ','l','i','n','k'};
    //  "ember EM250 link";
  int8u networkKey[EMBER_ENCRYPTION_KEY_SIZE] = 
    {'e','m','b','e','r',' ','E','M','2','5','0',' ','n','w','k',' '};
    //  "ember EM250 nwk ";
  EmberKeyData linkKeyData;
  EmberKeyData nwkKeyData;
  EmberInitialSecurityState secState;
  
  // set link key
  MEMCOPY(linkKeyData.contents, linkKey, EMBER_ENCRYPTION_KEY_SIZE);
  // set nwk key
  MEMCOPY(nwkKeyData.contents, networkKey, EMBER_ENCRYPTION_KEY_SIZE);
  
  // set securit state
  secState.bitmask = bmask;
  secState.preconfiguredKey = linkKeyData;
  secState.networkKey = nwkKeyData;
  secState.networkKeySequenceNumber = 0;

  if(EMBER_SUCCESS != emberSetInitialSecurityState(&secState)) {
    emberSerialPrintf(APP_SERIAL, "Error: set security state failed\r\n");
  }
}
