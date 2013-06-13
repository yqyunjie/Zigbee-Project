// ****************************************************************************
//  zdo-host-sample.c
// 
//  host sample app sending and receiving ZDO messages
//
//  This is an example of a ZDO application using the Ember ZigBee stack.
//  This sample application builds a two node network and allows the user
//  to send the supported ZDO messages via the CLI. The following ZDO commands
//  are supported
//
//  Additionally, the sample also demonstrate usage of app/util/counters
//  utility library.  The utility provides a way for the node to query ember
//  counters values of other nodes over the air.  More details can be found in
//  app/util/counters/counters-ota.h.  Note that there is a minimum requirement
//  for available stack size to support this utility and avr32 host is currently
//  unable to support the utility.
//
// ****************************************************************************
//  ZDO commands
// ****************************************************************************
// ****************************************************************************
//  Abberviations
// ****************************************************************************
//  src = source
//  dst = destination
//  sep = source end point
//  dep = destination end point
//  tep = target end point
//  rep = requested end point
// ****************************************************************************
//  ZDO Device and Discovery Attributes
// ****************************************************************************
//  zdo netAddrReq        <dstEUI:8 Hex> 
//                        <OPT kids: 1 str, def=T> <OPT idx: 1 int, def=0> 
//  zdo ieeeAddrReq       <dstId:2 Hex> 
//                        <OPT kids: 1 str, def=T> <OPT idx: 1 int, def=0>
//  zdo nodeDescReq       <dstId:2 Hex>
//  zdo nodePwrDescReq    <dstId:2 Hex>
//  zdo nodeSmplDescReq   <dstId:2 Hex> <tep: 1 int>
//  zdo nodeActvEndPntReq <dstId:2 Hex>
//  zdo setInClusters     <num> <clusters>
//  zdo setOutClusters    <num> <clusters>
//  zdo nodeMtchDescReq   <dstId:2 Hex> <profile int>
// ****************************************************************************
//  ZDO Bind Manager Attributes
// ****************************************************************************
//  zdo endBindReq  <dstId:2 Hex> <dstEUI:8 Hex> <tep: 1 int> <profile int>
//  zdo setEndPnts  <sep: 1 int> <dep: 1 int> <rep: 1 int> 
//  zdo setBindInfo <clusterId 2 Hex> <type int> <grpAddr: 2 Hex> 
//  zdo bindReq     <dstId:2 Hex> <srcEUI:8 Hex> <dstEUI:8 Hex> 
//  zdo unBindReq   <dstId:2 Hex> <srcEUI:8 Hex> <dstEUI:8 Hex> 
// ****************************************************************************
//  ZDO Network Manager Attributes
// ****************************************************************************
//  zdo bindTblReq    <dstId:2 Hex> <idx: 1 int> 
//  zdo leaveReq      <dstId:2 Hex> <dstEUI:8 Hex> <leaveReqFlgs int>
//  zdo nwkUpdateReq chan <chan:1 int>
//  zdo nwkUpdateReq scan <dstId:2 Hex> <dur:1 int> <cnt:1 int>
//  zdo nwkUpdateReq set  <mgrId:2 Hex> <mask:4 Hex>
//  zdo lqiTblReq         <destId: 2 Hex> <idx: 1 int>
//  zdo routeTblReq       <destId: 2 Hex> <idx: 1 int>
//  zdo permitJoinReq     <destId: 2 Hex> <dur: 1 int> <auth: 1 int>
// ****************************************************************************
//  ZDO Send Unsupported Request
// ****************************************************************************
//  zdo unSupported  <dstId:2 Hex> <clusterId 2 Hex>
// ****************************************************************************
// ****************************************************************************
//  Counter Command
// ****************************************************************************
// counter request <dest> <eraseFlag: 1(True), 0(False)>
// counter print
// ****************************************************************************
//
//  This example also provides some simple commands that can be sent to the
//  node via the serial port. A brief explanation of the commands is
//  provided below:
//
//  ('help')    prints the help menu
//  ('version') prints the version of the application
//  ('info')    prints info about this node including channel, power, and app
//  ('network') network commands: form/join/leave/permit join
//  ('zdo')     Sends ZDO commands - see ZDO commands section
//  ('print')   prints the binding table
//  ('counter') Send counter request command.
//  ('reset')   resets the node
// ****************************************************************************
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// ****************************************************************************
// ./build.pl zdo-host-sample PLAT=avr-atmega MICRO=128 PHY=em250 BOARD=dev0473
// ./build.pl em260-spi PLAT=xap2b MICRO=em260 PHY=em250 BOARD=dev0470

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

// CLI
#include "app/util/serial/cli.h"

// EZSP
#include PLATFORM_HEADER //compiler/micro specifics, types
#include "stack/include/ember-types.h"
#include "app/util/ezsp/ezsp-protocol.h" 
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"

// ZDO
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "app/util/zigbee-framework/zigbee-device-host.h"
#include "app/util/zigbee-framework/network-manager.h"

//stack and serial
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/serial/serial.h"
#include "app/zdo/zdo-host-config.h"

// act as host to em260-uart
#ifdef GATEWAY_APP
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#endif

// counters utility
#include "app/util/counters/counters-ota.h"

// **************************************************
// defines that can be turned off to save space
// this define enables help from the CLI
#define APP_ENABLE_HELP_CLI

//**************************************************
// Numeric index for the first endpoint.  Up to 31 user
// endpoints are available for applications.  This application
// uses only one. This constant makes the code easier to read.
#define ENDPOINT     1
#define INTERFACE_ID 0

// *******************************************************************
// Ember endpoint and interface configuration
#define PROFILE_ID 0xC010
int8u emberEndpointCount = 2;
//Define clusters supported by the endpoints
int16u PGM ep1inList[3] = {2,3,4};
int16u PGM ep1outList[3] = {2,3,4};

// End point description structure
EmberEndpointDescription PGM epd1 = {
  PROFILE_ID,     // epd1.profileId = PROFILE_ID;
  1,              // epd1.deviceId = 1;
  0,              // epd1.deviceVersion = 0;
  3,              // epd1.inputClusterCount = 1;
  3               // epd1.outputClusterCount = 1;
};
 
// Endpoints used by this application, and referenced by the stack
EmberEndpoint emberEndpoints[2] = {
  {               // EmberEndpoint ep1
    1,            // ep1.endpoint = 1;
    &epd1,        // ep1.EmberEndpointDescription = &epd1;
    ep1inList,    // ep1.inputClusterList = &ep1inList;
    ep1outList    // ep1.outputClusterList = &ep1outList;
  },
  {               // EmberEndpoint ep2
    2,            // ep2.endpoint = 1;
    &epd1,        // ep2.EmberEndpointDescription = &epd1;
    ep1inList,    // ep2.inputClusterList = &ep1inList;
    ep1outList    // ep2.outputClusterList = &ep1outList;
  }
};

// Counters (string) names
PGM_NO_CONST PGM_P titleStrings[] = {
  EMBER_COUNTER_STRINGS
};
int16u emberCounters[EMBER_COUNTER_TYPE_COUNT];

// The serial ports we use.
#ifndef GATEWAY_APP
#define APP_SERIAL   0
#endif
#define DEBUG_SERIAL 0
#define ZDO_DEBUG

// Print banner for seperating print data
#define BANNER "***********************************************************\r\n"
// Print #defines for debug 
#ifdef ZDO_DEBUG
  #define ZDO_DEBUG_PRINT(x, y) emberSerialPrintf(APP_SERIAL, x, y)
  #define ZDO_DEBUG_PRINT_TEXT(x) emberSerialPrintf(APP_SERIAL, x)
  #define ZDO_PRINT_EUI64(x) printEUI64(APP_SERIAL, x)
#else
  #define ZDO_DEBUG_PRINT(x, y) do { ; } while(FALSE)
  #define ZDO_DEBUG_PRINT_TEXT(x) do { ; } while(FALSE)
  #define ZDO_PRINT_EUI64(x) do { ; } while(FALSE)
#endif

// ******************************************************************
// Forward declarations.
static void appTick(void);
void printZDOCliCmds(void);
void printBindingTableUtil(int8u, int8u);
int8u getNodeTypeFromBuffer(void);
void printZDOCmds(EmberNodeId sender,int8u addressIndex,
                  EmberApsFrame *apsFrame);
void run(void);
void quit(void);

#if defined(SECURE) 
  void setSecurityState(int16u bmask);
#endif
// form/join/rejoin times
int16u timeStartJoin = 0;
int16u timeEndJoin = 0;
int16u sendPktSeqNum;

// flags the user can turn on or off to make the printing behave differently
boolean printReceivedMessages = TRUE;
int8u printMsgSentInfo = FALSE;
boolean printMsgSentStatus = TRUE;

// ******************************************************************
// prints EUI 64 values
// ******************************************************************
void printEUI64(int8u port, int8u *eui64) {
  int8u i;
  for (i=8; i>0; i--) {
    emberSerialPrintf(port, "%X", eui64[i-1]);
  }
}

// ******************************************************************
//  prints to the CLI networking information about the application
// ******************************************************************
void infoCB(void)
{
  int8u i;
  EmberNodeType localNodeType; 
  EmberNetworkParameters localParams;
  EmberEUI64 eui;
  EmberStatus paramStatus,netStatus;
  
  ezspGetEui64(eui);
  paramStatus = ezspGetNetworkParameters(&localNodeType, &localParams);

  emberSerialPrintf(APP_SERIAL, "node [");
  printEUI64(APP_SERIAL, eui);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "] app [ZDO] chan [0x%x] pwr [0x%x]\r\n",
                    localParams.radioChannel,
                    localParams.radioTxPower);
  emberSerialPrintf(APP_SERIAL, "panID [0x%2x] nodeID [0x%2x]\r\n",
                    localParams.panId,
                    emberGetNodeId());
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "xpan ");
  emberSerialWaitSend(APP_SERIAL);
  for (i = 0 ; i < EXTENDED_PAN_ID_SIZE ; i++) {
    emberSerialPrintf(APP_SERIAL, "%X", localParams.extendedPanId[i]);  
    emberSerialWaitSend(APP_SERIAL);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);  


  emberSerialPrintf(APP_SERIAL, "nodeType status [0x%x] nodeType [0x%x]\r\n",
                    paramStatus,
                    localNodeType);
  emberSerialWaitSend(APP_SERIAL);

  netStatus = emberNetworkState();
  emberSerialPrintf(APP_SERIAL, "NETWORK STATE: ");
  emberSerialWaitSend(APP_SERIAL);
	if (netStatus == EMBER_NO_NETWORK) {
	     emberSerialPrintf(APP_SERIAL, "EMBER_NO_NETWORK");
	} else if (netStatus == EMBER_JOINING_NETWORK) {
	     emberSerialPrintf(APP_SERIAL, "EMBER_JOINING_NETWORK");
	} else if (netStatus == EMBER_JOINED_NETWORK) {
	     emberSerialPrintf(APP_SERIAL, "EMBER_JOINED_NETWORK");
	} else if (netStatus == EMBER_JOINED_NETWORK_NO_PARENT) {
	     emberSerialPrintf(APP_SERIAL, "EMBER_JOINED_NETWORK_NO_PARENT");
	} else if (netStatus == EMBER_LEAVING_NETWORK) {
	     emberSerialPrintf(APP_SERIAL, "EMBER_LEAVING_NETWORK");
	} else {
	     emberSerialPrintf(APP_SERIAL, "UNKNOWN");
	}
  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialPrintf(APP_SERIAL, "stack version [%2x]\r\n", ezspUtilStackVersion);
  emberSerialWaitSend(APP_SERIAL);
}

// ******************************************************************
//  prints to the CLI application version info
// ******************************************************************
void versionCB(void)
{
  emberSerialPrintf(APP_SERIAL, "security level [%x], app version [%2x]\r\n",
                  EMBER_SECURITY_LEVEL, APP_VERSION);
}

// *****************************
// msNetwork CB
// *****************************
// network form <channel> <power> <panid in hex>
// network join <nodeType> <channel> <power> <panid in hex>
// network leave
// network pjoin <time>
//
// devices always join as routers
// pjoin stands for permitjoining
void networkCB(void) {
  EmberStatus status;
  int8u nodeType;
  int8u permitJoinDuration;
  EmberNetworkParameters networkParams;

  timeStartJoin = halCommonGetInt16uMillisecondTick();

  // network form <channel> <power> <panid in hex>
  if (cliCompareStringToArgument("form", 1) == TRUE)
  {
    networkParams.radioChannel = cliGetInt16uFromArgument(2);
    networkParams.radioTxPower = cliGetInt16uFromArgument(3);
    networkParams.panId = (cliGetHexByteFromArgument(0, 4) * 256) + 
      (cliGetHexByteFromArgument(1, 4));
    // Cause a random extended PAN ID to be chosen.
    MEMSET(networkParams.extendedPanId, 
           0,
           EXTENDED_PAN_ID_SIZE);
    #if defined(SECURE)
      {
        int16u bmask;
        bmask = EMBER_TRUST_CENTER_GLOBAL_LINK_KEY
                | EMBER_HAVE_PRECONFIGURED_KEY
                | EMBER_HAVE_NETWORK_KEY;
        setSecurityState(bmask);
      }
    #endif
    status = emberFormNetwork(&networkParams);
    emberSerialPrintf(APP_SERIAL, "form 0x%x\r\n\r\n", status);
    return;
  }

  // network join <nodeType> <channel> <power> <panid in hex>
  else if (cliCompareStringToArgument("join", 1) == TRUE)
  {
    nodeType = getNodeTypeFromBuffer();
    MEMSET(networkParams.extendedPanId, 
           0,
           EXTENDED_PAN_ID_SIZE);
    networkParams.radioChannel = cliGetInt16uFromArgument(3);
    networkParams.radioTxPower = cliGetInt16uFromArgument(4);
    networkParams.panId = (cliGetHexByteFromArgument(0, 5) * 256) + 
      (cliGetHexByteFromArgument(1, 5));
    #if defined(SECURE)
      {
        int16u bmask;
        bmask = EMBER_TRUST_CENTER_GLOBAL_LINK_KEY
                | EMBER_HAVE_PRECONFIGURED_KEY
                | EMBER_REQUIRE_ENCRYPTED_KEY;
        setSecurityState(bmask);
      }
    #endif
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
    status = emberPermitJoining(permitJoinDuration);
    emberSerialPrintf(APP_SERIAL, "pJoin for %x sec: 0x%x\r\n\r\n", 
                      permitJoinDuration, status);
    return;
  }
  else if (cliCompareStringToArgument("init", 1) == TRUE) {
    status = emberNetworkInit();
    emberSerialPrintf(APP_SERIAL, "init 0x%x\r\n\r\n", status);
  }
#ifdef APP_ENABLE_HELP_CLI
  else {
    emberSerialPrintf(APP_SERIAL, 
                      "network form <channel> <power> <panid in hex>\r\n");
    emberSerialPrintf(APP_SERIAL, 
                      "network join <nodeType> <channel> <power> ",
                      "<panid in hex>\r\n");
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "network leave\r\n");
    emberSerialPrintf(APP_SERIAL, "network pjoin <time>\r\n");
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
#endif
}

#if defined(SECURE)
void setSecurityState(int16u bmask)
{
  int8u linkKey[EMBER_ENCRYPTION_KEY_SIZE] = 
    {1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6};
  int8u nwkKey[EMBER_ENCRYPTION_KEY_SIZE] = 
    {7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7};
  EmberKeyData linkKeyData;
  EmberKeyData nwkKeyData;
  EmberInitialSecurityState secState;
  
  // set link key
  MEMCOPY(linkKeyData.contents, linkKey, EMBER_ENCRYPTION_KEY_SIZE);
  // set nwk key
  MEMCOPY(nwkKeyData.contents, nwkKey, EMBER_ENCRYPTION_KEY_SIZE);
  
  // set securit state
  secState.bitmask = bmask;
  secState.preconfiguredKey = linkKeyData;
  secState.networkKey = nwkKeyData;
  secState.networkKeySequenceNumber = 0;

  if(EMBER_SUCCESS != emberSetInitialSecurityState(&secState)) {
    emberSerialPrintf(APP_SERIAL, "set security state failed\r\n");
  }
}
#endif

// ******************************************************************
// counter CB
// ******************************************************************
// counter request <dest> <eraseFlag: 1(True), 0(False)>
// counter print
void counterCB(void) {
  int8u i=0, flag;
  EmberNodeId dest;
  EmberStatus status;

  if(cliCompareStringToArgument("request", 1)) {
    dest = cliGetInt16uFromHexArgument(2);
    flag = (int8u)cliGetInt16uFromArgument(3);
    status = emberSendCountersRequest(dest, flag);
    if(status != EMBER_SUCCESS) {
      emberSerialPrintf(APP_SERIAL, "Counter request, status %x\r\n", status);
    }
    emberSerialPrintf(APP_SERIAL, "EraseFlag: %p\r\n", (flag? "True": "False")); 
  } else if(cliCompareStringToArgument("print", 1)) {
    ezspReadAndClearCounters(emberCounters);
    while ( titleStrings[i] != NULL && i < EMBER_COUNTER_TYPE_COUNT) {
    emberSerialPrintf(APP_SERIAL, "%p: %d\r\n", 
                        titleStrings[i],
                        emberCounters[i]);
    emberSerialWaitSend(APP_SERIAL);
    i++;
  }
  }
}

// *******************************************************************
// reset the node
// *******************************************************************
void resetCB(void)
{
  halReboot();
}

// *******************************************************************
// The main() loop and the application's contribution.
// *******************************************************************
int main( MAIN_FUNCTION_PARAMETERS ) {
  EmberStatus status;
  int8u resetType;

  //Initialize the hal
  halInit();

  // allow interrupts
  INTERRUPTS_ON();

  resetType = halGetResetInfo();

  // inititialize the serial port
  // good to do this before ezspUtilInit, that way any errors that occur
  // can be printed to the serial port.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
     != EMBER_SUCCESS) {
    emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
  }
  emberSerialGuaranteedPrintf(APP_SERIAL, "INIT ZDO-host-sample App"
                              ", em260\r\n");

  emberSerialPrintf(APP_SERIAL, "Reset is 0x%x:%p\r\n", resetType,
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
      // error visually for example. This app quits.
      quit();
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
    // error visually for example. This app quits.
    quit();
  } else {
    emberSerialPrintf(APP_SERIAL, "EVENT: ezspUtilInit passed\r\n");
    emberSerialWaitSend(APP_SERIAL);
  }

  // initialize the cli library to use the correct serial port. APP_SERIAL 
  // is defined in the makefile or project-file depending on the platform.
  cliInit(APP_SERIAL);

  while(TRUE) {
    run();
    appTick();
    cliProcessSerialInput(); // process user input from the serial port
  }
  
}

// *******************************************************************
// helper app funtions
// *******************************************************************
void run() {
  halResetWatchdog();
  #ifdef GATEWAY_APP
    ezspTick();
  #endif
  if (ezspCallbackPending()) {
    ezspCallback();
  }
}

// *******************************************************************
// appTick() is used blink the LED by default
// *******************************************************************
static void appTick(void)
{
  static int16u lastBlinkTime = 0;
  int16u time;

  time = halCommonGetInt16uMillisecondTick();

  if (time - lastBlinkTime > 200 /*ms*/) {
    // blink the LED
    halToggleLed(BOARD_HEARTBEAT_LED);
    lastBlinkTime = time;
  }
}

// ******************************************************************
// Helper functions

void printStatus(PGM_P command, int8u status)
{
  emberSerialPrintf(APP_SERIAL, "SEND %p status 0x%x\r\n\r\n", command, status);
}

// *******************************************************************
// Handlers required to use the Ember Stack.
// *******************************************************************
// Called when the stack status changes, usually as a result of an
// attempt to form, join, or leave a network.
// *******************************************************************
void ezspStackStatusHandler(EmberStatus status)
{
  timeEndJoin = halCommonGetInt16uMillisecondTick();

  switch (status) {
  case EMBER_NETWORK_UP:
    emberSerialPrintf(APP_SERIAL,
                      "EVENT: EMBER_NETWORK_UP (%d ms)\r\n",
                      (timeEndJoin - timeStartJoin));
    break;

  case EMBER_NETWORK_DOWN:
    emberSerialPrintf(APP_SERIAL,
                      "EVENT: EMBER_NETWORK_DOWN (%d ms)\r\n",
                      (timeEndJoin - timeStartJoin));
    break;
  case EMBER_JOIN_FAILED:
    emberSerialPrintf(APP_SERIAL,
                      "EVENT: EMBER_JOIN_FAILED (%d ms)\r\n",
                      (timeEndJoin - timeStartJoin));
    break;
  case EMBER_CHANNEL_CHANGED:
    emberSerialPrintf(APP_SERIAL,
                      "EVENT: EMBER_CHANNEL_CHANGED (%d)\r\n",
                      emberGetRadioChannel());
    break;

  default:
    emberSerialPrintf(APP_SERIAL, 
                      "EVENT: stackStatus 0x%x\r\n", status);
  }

  emberSerialWaitSend(APP_SERIAL);
}



// Called when a message arrives.
void ezspIncomingMessageHandler(EmberIncomingMessageType type, 
                                EmberApsFrame *apsFrame, 
                                int8u lastHopLqi, int8s lastHopRssi, 
                                EmberNodeId sender, int8u bindingIndex, 
                                int8u addressIndex, int8u messageLength, 
                                int8u *messageContents)
{
  int8u i;

  // if it's counter request message, let the utility handle it and get
  // out of the function.  We check for counter request before checking for
  // messageLength because the request message is null.
  if(emberIsIncomingCountersRequest(apsFrame, sender)) {
    return;  
  }
  
  // process ZDO energy scan reports
  nmUtilProcessIncoming(apsFrame, messageLength, messageContents);

  // print len & cluster

  // print the ZDO details message if message printing is on
  if (printReceivedMessages == TRUE) {
    emberSerialPrintf(APP_SERIAL, BANNER);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "INCOMING MESSAGE\r\n"); 
    emberSerialPrintf(APP_SERIAL, "[EmberIncomingMessageType] %x\r\n", type); 
  
    //If this is a ZDO message decode it
    if (apsFrame->profileId == 0x0000) {
      printZDOCmds(sender,addressIndex,apsFrame);
      
      emberSerialPrintf(APP_SERIAL, "[Zigbee Device Profile V2] [len] %x\r\n", 
                        messageLength);
      emberSerialPrintf(APP_SERIAL, "[msg] ");      
      emberSerialWaitSend(APP_SERIAL);      
      //Print the message
      for (i=0; i<messageLength; i++) {
        emberSerialPrintf(APP_SERIAL, "%x ", messageContents[i]);
        emberSerialWaitSend(APP_SERIAL);
      }
     	emberSerialPrintf(APP_SERIAL, "\r\n");
    	emberSerialWaitSend(APP_SERIAL);
  
    } 
    // if it's counter response message, we decode the message and display it.
    else if(emberIsIncomingCountersResponse(apsFrame)) {
      emberSerialPrintf(APP_SERIAL, "\r\nRX counter response:\r\n");
      if((messageLength%3) != 0) {
        emberSerialPrintf(APP_SERIAL, 
          "invalid message length, 0x%x\r\n", messageLength);
      } else {
        for(i=0; i<messageLength; i=i+3) {
          emberSerialPrintf(APP_SERIAL, "%p: %d\r\n", 
            titleStrings[messageContents[i]], 
            HIGH_LOW_TO_INT(messageContents[i+2], messageContents[i+1]));
          emberSerialWaitSend(APP_SERIAL);
        }
        emberSerialPrintf(APP_SERIAL, "\r\n");
        emberSerialWaitSend(APP_SERIAL);
      }
    } else {
      emberSerialPrintf(APP_SERIAL, "NON ZDO message received");
      emberSerialPrintf(APP_SERIAL, "RX len: %x, clus 0x%2x ", 
                    messageLength, apsFrame->clusterId);
    }

    //Print the radio info
    emberSerialPrintf(APP_SERIAL, "[lqi]%u \r\n", lastHopLqi);
    emberSerialPrintf(APP_SERIAL, "[rssi]%d\r\n", lastHopRssi);
    emberSerialWaitSend(APP_SERIAL);

    emberSerialPrintf(APP_SERIAL, BANNER);
    emberSerialPrintf(APP_SERIAL, "\r\n\r\n");
    emberSerialWaitSend(APP_SERIAL);
  } 
}

// Called when a message we sent is acked by the destination or when an
// ack fails to arrive after several retransmissions.
void ezspMessageSentHandler(EmberOutgoingMessageType type, int16u 
      indexOrDestination, EmberApsFrame *apsFrame, int8u messageTag, 
      EmberStatus status, int8u messageLength, int8u *messageContents) {
  int8u i;

	if (printMsgSentInfo == TRUE) {
    emberSerialPrintf(APP_SERIAL, "[type]%x\r\n", type);
    emberSerialPrintf(APP_SERIAL, "[indexOrDestination]%2x\r\n",  
                      indexOrDestination);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame profileId]%2x\r\n", 
                      apsFrame->profileId);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame clusterId]%2x\r\n", 
                      apsFrame->clusterId);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame sourceEndpoint]%x\r\n", 
                      apsFrame->sourceEndpoint);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame destinationEndpoint]%x\r\n", 
                      apsFrame->destinationEndpoint);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame options]%2x\r\n", 
                      apsFrame->options);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame groupId]%2x\r\n", 
                      apsFrame->groupId);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[EmberApsFrame sequence]%x\r\n", 
                      apsFrame->sequence);
    emberSerialPrintf(APP_SERIAL, "[messageTag]%x\r\n", messageTag);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[status]%x\r\n",  status);
    emberSerialPrintf(APP_SERIAL, "[messageLength]%x\r\n", messageLength);
    emberSerialWaitSend(APP_SERIAL);
    emberSerialPrintf(APP_SERIAL, "[messageContents]");
    emberSerialWaitSend(APP_SERIAL);
    for (i = 0 ; i < messageLength ; i++) {
      emberSerialPrintf(APP_SERIAL, "%x ", messageContents[i]);
      emberSerialWaitSend(APP_SERIAL);  
    }
    emberSerialPrintf(APP_SERIAL, "\r\n\r\n"); 
  }
}

// Sets the binding for ZDO bind request 
void ezspRemoteSetBindingHandler(
      EmberBindingTableEntry *entry,
      int8u index,
      EmberStatus status)
{
  EmberBindingTableEntry candidate;
  int8u i;
 
  for (i=0; i<EMBER_BINDING_TABLE_SIZE; i++) {
     status = emberGetBinding(i, &candidate);
     if (status == EMBER_SUCCESS) {
         // see if the binding is empty
         if (candidate.type == EMBER_UNUSED_BINDING) {
             status = emberSetBinding(i, entry);
         }
     }
  }
}      

// Deletes the binding for ZDO unbind request 
void ezspRemoteDeleteBindingHandler(int8u index, EmberStatus status)
{ 
  emberSerialPrintf(APP_SERIAL, "Remote binding delete status 0x%x\r\n\r\n", 
                    status);
}     

//  Funtion to see if the 260 is awake
void halNcpIsAwakeIsr(boolean isAwake)
{}

// Called to handle EZSP errors
void ezspErrorHandler(EzspStatus status)
{
  emberSerialGuaranteedPrintf(APP_SERIAL,
                              "ERROR: ezspErrorHandler 0x%x\r\n",
                              status);
  #ifdef GATEWAY_APP
  emberSerialGuaranteedPrintf(APP_SERIAL,
                                "ncpError=0x%x, ashError=0x%x\r\n",
                                ncpError, ashError);
  #endif
  quit();
}


// This is called when a network is found when app is performing scan.
void ezspNetworkFoundHandler(
      EmberZigbeeNetwork *networkFound,
      int8u lastHopLqi,
      int8s lastHopRssi)
{}      

// this is called when a scan is complete
void ezspScanCompleteHandler( int8u channel, EmberStatus status )
{}

// Called by the network-manager utility code after receiving
// enough energy scan reports.  
void nmUtilWarningHandler(void)
{
  EmberStatus status;
  emberSerialPrintf(APP_SERIAL, 
                    "%d scan reports received in the last %d minutes\r\n",
                    NM_WARNING_LIMIT,
                    NM_WINDOW_SIZE);
  status = nmUtilChangeChannelRequest();
  printStatus("nwkUpdateReq", status);
}

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

// ****************************************************************************
// ZDO commands
// ****************************************************************************
EmberMessageBuffer zdoInClusterMsg,zdoOutClusterMsg;
int16u zdoInCluster[MAX_NUM_CLUSTERS],zdoOutCluster[MAX_NUM_CLUSTERS];
int16u pZdoInCluster[MAX_NUM_CLUSTERS*2],pZdoOutCluster[MAX_NUM_CLUSTERS*2];
int8u inNumClusters,outNumClusters;

void zdoCB(void)
{
  int16u leaveReqFlgs,profile,duration,authentication;;
  EmberNodeId dstId;
  int8u tep,idx,i;
  boolean kids;     
  EmberEUI64 srcEUI,dstEUI;
  EmberStatus status;
  EmberApsOption apsOptions;
  
  static int8u sep,dep,rep,type;
  static int16u clusterId;
  static EmberMulticastId grpAddr;
  
  //set defaults  
  kids = TRUE;
  idx = 0; 
  
  //set APS options
  apsOptions = EMBER_APS_OPTION_RETRY | 
    EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY |
    EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY;
    
  // ***************************************************************************
  //ZDO Device and Discovery Attributes
  // ***************************************************************************
  //zdo netAddrReq        <dstEUI:8 Hex> 
  //                      <OPT kids: 1 str, def=t> <OPT idx: 1 int, def=0> 
  if ((cliCompareStringToArgument("netAddrReq", 1)) == TRUE) {
    for (i=0; i<8; i++)
      dstEUI[7-i] =  cliGetHexByteFromArgument(i, 2);
      
   //If the kids are supplied set else default
    if (cliCompareStringToArgument("t",3)) {
        kids = TRUE;
        idx = cliGetInt16uFromArgument(4);
    } else if (cliCompareStringToArgument("f",3)) {
        kids = FALSE;
        idx = cliGetInt16uFromArgument(4);
    }
    ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
    ZDO_DEBUG_PRINT_TEXT("dstEUI ");
    ZDO_PRINT_EUI64(dstEUI);
    ZDO_DEBUG_PRINT("\r\nkids: %d\r\n",kids);
    ZDO_DEBUG_PRINT("idx: %d\r\n",idx);
    
    status = emberNetworkAddressRequest(dstEUI, kids, idx);
    printStatus("netAddrReq", status);
    
  // zdo ieeeAddrReq      <dstId:2 Hex> 
  //                      <OPT kids: 1 str, def=t> <OPT idx: 1 int, def=0>  
  } else if ((cliCompareStringToArgument("ieeeAddrReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
      
      //If the kids are supplied set else default
      if (cliCompareStringToArgument("t",3)) {
          kids = TRUE;
          idx = cliGetInt16uFromArgument(4);
      } else if (cliCompareStringToArgument("f",3)) {
          kids = FALSE;
          idx = cliGetInt16uFromArgument(4);
      } 
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("kids: %d\r\n",kids);
      ZDO_DEBUG_PRINT("idx: %d\r\n",idx);
      
      status = emberIeeeAddressRequest(dstId, kids, idx, 
                                       apsOptions);
      printStatus("ieeeAddrReq", status);
  
  // zdo nodeDescReq       <dstId:2 Hex>    
  } else if ((cliCompareStringToArgument("nodeDescReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");          
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n ",dstId );
      
      status = emberNodeDescriptorRequest(dstId, apsOptions);
      printStatus("nodeDescReq", status);
  
  // zdo nodePwrDescReq    <dstId:2 Hex>
  } else if ((cliCompareStringToArgument("nodePwrDescReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");          
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n ",dstId);
      
      status = emberPowerDescriptorRequest(dstId, apsOptions);
      printStatus("nodePwrDescReq", status);
      
  // zdo nodeSmplDescReq   <dstId:2 Hex> <tep: 1 int>
  } else if ((cliCompareStringToArgument("nodeSmplDescReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
                  
      tep = cliGetInt16uFromArgument(3);    
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");      
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n ",dstId );
      ZDO_DEBUG_PRINT("TEP: %d\r\n",tep);
      
      status = emberSimpleDescriptorRequest(dstId, tep, apsOptions);
      printStatus("nodeSimplDescReq", status);

  // zdo nodeActvEndPntReq <dstId:2 Hex>
  } else if ((cliCompareStringToArgument("nodeActvEndPntReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
      
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");          
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n ",dstId );
      status = emberActiveEndpointsRequest(dstId, apsOptions);
      printStatus("nodeActvEndPntReq", status);
  
  // zdo setInClusters     <num> <clusters>
  } else if (cliCompareStringToArgument("setInClusters", 1) == TRUE) {    
      inNumClusters = cliGetInt16uFromArgument(2);
      
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("number of clusters: %d\r\n",inNumClusters);
      if (inNumClusters > MAX_NUM_CLUSTERS) {
        emberSerialPrintf(APP_SERIAL, "# of clusters set is > than the # "
                          "of supported clusters: %d\r\n",MAX_NUM_CLUSTERS);
        return;
      }         
      for (i=0; i<inNumClusters; i++) {
        zdoInCluster[i] =  ((cliGetHexByteFromArgument(0, 3 + i) << 8) +
                            (cliGetHexByteFromArgument(1, 3 + i)));
                            
        ZDO_DEBUG_PRINT("In cluster idx: %d ",i);
        ZDO_DEBUG_PRINT("value: 0x%2x\r\n", zdoInCluster[i]);   
        pZdoInCluster[i*2] = LOW_BYTE(zdoInCluster[i]);
        pZdoInCluster[i*2+1] = HIGH_BYTE(zdoInCluster[i]);
      }        
      ZDO_DEBUG_PRINT("numClusters %d\r\n",inNumClusters);

      
  // zdo setOutClusters    <num> <clusters>  
  } else if ((cliCompareStringToArgument("setOutClusters", 1)) == TRUE) {    
      outNumClusters = cliGetInt16uFromArgument(2);
      
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("number of clusters: %d\r\n",outNumClusters);
      if (outNumClusters > MAX_NUM_CLUSTERS) {
        emberSerialPrintf(APP_SERIAL, "# of clusters set is > than the ", 
                          "MAX_NUM_CLUSTERS # of supported clusters: %d\r\n",
                          MAX_NUM_CLUSTERS);
        return;
      }

      for (i=0; i<outNumClusters; i++) {
        zdoOutCluster[i] =  ((cliGetHexByteFromArgument(0, 3 + i) << 8) +
                             (cliGetHexByteFromArgument(1, 3 + i)));
        ZDO_DEBUG_PRINT("Out cluster idx: %d ",i);
        ZDO_DEBUG_PRINT("value: 0x%2x\r\n", zdoOutCluster[i]);
        pZdoOutCluster[i*2] = LOW_BYTE(zdoOutCluster[i]);
        pZdoOutCluster[i*2+1] = HIGH_BYTE(zdoOutCluster[i]);    
      }     
      ZDO_DEBUG_PRINT("numClusters %d\r\n",outNumClusters);
  
  //zdo nodeMtchDescReq   <dstId:2 Hex> <profile int>
  } else if ((cliCompareStringToArgument("nodeMtchDescReq", 1)) == TRUE) {    
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));
                  
      profile =  ((cliGetHexByteFromArgument(0, 3) << 8) +
                (cliGetHexByteFromArgument(1, 3)));
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("profile: 0x%2x\r\n",profile);
      
      status = ezspMatchDescriptorsRequest(dstId, profile,
                                         inNumClusters,outNumClusters,
                                         pZdoInCluster, pZdoOutCluster,
                                         apsOptions);
      printStatus("nodeMtchDescReq", status);

  // ***************************************************************************
  //ZDO Bind Manager Attributes
  // ***************************************************************************
  // zdo endBindReq  <dstId:2 Hex> <dstEUI:8 Hex> <tep: 1 int> <profile int>
  } else if ((cliCompareStringToArgument("endBindReq", 1)) == TRUE) {      
      dstId = cliGetInt16uFromHexArgument(2);
      for (i=0; i<8; i++) {
        dstEUI[7-i] =  cliGetHexByteFromArgument(i, 3);
      }
      tep = cliGetInt16uFromArgument(4);    
      profile = cliGetInt16uFromHexArgument(5);
     
      status = ezspEndDeviceBindRequest(dstId,
                                     dstEUI,
                                     tep,
                                     profile,
                                     inNumClusters,
                                     outNumClusters,
                                     pZdoInCluster,
                                     pZdoOutCluster,
                                     apsOptions);
      emberSerialPrintf(APP_SERIAL, "SEND endBindReq status 0x%x\r\n\r\n",status);

  // zdo setEndPnts  <sep: 1 int> <dep: 1 int> <rep: 1 int> 
  } else if ((cliCompareStringToArgument("setEndPnts", 1)) == TRUE) { 
      sep = cliGetInt16uFromArgument(2);
      dep = cliGetInt16uFromArgument(3);
      rep = cliGetInt16uFromArgument(4);    
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("SEP: %d\r\n",sep);
      ZDO_DEBUG_PRINT("DEP: %d\r\n",dep );
      ZDO_DEBUG_PRINT("REP: %d\r\n",rep);     

  // zdo setBindInfo <clusterId 2 Hex> <type int> <grpAddr: 2 Hex> 
  } else if ((cliCompareStringToArgument("setBindInfo", 1)) == TRUE) {
      clusterId = ((cliGetHexByteFromArgument(0, 2) << 8) +
                  (cliGetHexByteFromArgument(1, 2)));
      type = cliGetInt16uFromArgument(3);
      grpAddr = ((cliGetHexByteFromArgument(0, 4) << 8) +
                (cliGetHexByteFromArgument(1, 4)));
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("clusterId: 0x%2x \r\n",clusterId );
      ZDO_DEBUG_PRINT("type: %d\r\n",type);
      ZDO_DEBUG_PRINT("grpAddr: 0x%2x \r\n", grpAddr);
    
  // zdo bindReq     <dstId:2 Hex> <srcEUI:8 Hex> <dstEUI:8 Hex> 
  } else if ((cliCompareStringToArgument("bindReq", 1)) == TRUE) {      
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) +
                (cliGetHexByteFromArgument(1, 2)));

      
      for (i=0; i<8; i++)
        srcEUI[7-i] =  cliGetHexByteFromArgument(i, 3);
      
      for (i=0; i<8; i++)
        dstEUI[7-i] =  cliGetHexByteFromArgument(i, 4);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT_TEXT("srcEUI ");
      ZDO_PRINT_EUI64(srcEUI);
      ZDO_DEBUG_PRINT("\r\nSEP: %d\r\n",sep);
      ZDO_DEBUG_PRINT("clusterId: 0x%2x \r\n",clusterId );
      ZDO_DEBUG_PRINT("type: %d\r\n",type);
      ZDO_DEBUG_PRINT_TEXT("dstEUI ");
      ZDO_PRINT_EUI64(dstEUI);
      ZDO_DEBUG_PRINT("\r\ngrpAddr: 0x%2x \r\n", grpAddr);
      ZDO_DEBUG_PRINT("DEP: %d\r\n",dep );
      ZDO_DEBUG_PRINT("REP: %d\r\n",rep);     
      
      status = emberBindRequest(dstId, srcEUI, sep, clusterId, type, dstEUI, 
                                grpAddr, rep, apsOptions);
      printStatus("bindReq", status);

  // zdo unBindReq   <dstId:2 Hex> <srcEUI:8 Hex> <dstEUI:8 Hex> 
  } else if ((cliCompareStringToArgument("unBindReq", 1)) == TRUE) {   
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) + 
                (cliGetHexByteFromArgument(1, 2)));
      
      for (i=0; i<8; i++)
        srcEUI[7-i] =  cliGetHexByteFromArgument(i, 3);
      
      for (i=0; i<8; i++)
        dstEUI[7-i] =  cliGetHexByteFromArgument(i, 4);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT_TEXT("srcEUI ");
      ZDO_PRINT_EUI64(srcEUI);
      ZDO_DEBUG_PRINT("\r\nSEP: %d\r\n",sep);
      ZDO_DEBUG_PRINT("clusterId: 0x%2x \r\n",clusterId );                
      ZDO_DEBUG_PRINT("type: %d\r\n",type);
      ZDO_DEBUG_PRINT_TEXT("dstEUI ");
      ZDO_PRINT_EUI64(dstEUI);
      ZDO_DEBUG_PRINT("\r\ngrpAddr: 0x%2x \r\n", grpAddr);
      ZDO_DEBUG_PRINT("DEP: %d\r\n",dep );
      ZDO_DEBUG_PRINT("REP: %d\r\n",rep);     
      status = emberUnbindRequest(dstId, srcEUI, sep, clusterId, type, dstEUI,
                                  grpAddr, rep, apsOptions);
      printStatus("UnBindReq", status);                       
      
  // ***************************************************************************
  //ZDO Network Manager Attributes
  // ***************************************************************************
  //zdo bindTblReq    <dstId:2 Hex> <idx: 1 int> 
  } else if ((cliCompareStringToArgument("bindTblReq", 1)) == TRUE) {      
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) + 
                (cliGetHexByteFromArgument(1, 2)));
      
      idx = cliGetInt16uFromArgument(3);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("idx: %d\r\n",idx);
      
      status = emberBindingTableRequest(dstId,idx,apsOptions);
      printStatus("bindTblReq", status);                       
      
  // zdo leaveReq <dstId:2 Hex> <dstEUI:8 Hex> <leaveReqFlgs int>
  } else if ((cliCompareStringToArgument("leaveReq", 1)) == TRUE) {
      dstId =  ((cliGetHexByteFromArgument(0, 2) << 8) + 
                (cliGetHexByteFromArgument(1, 2)));
        
      for (i=0; i<8; i++)
        dstEUI[7-i] =  cliGetHexByteFromArgument(i, 3);
      
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");        
      ZDO_DEBUG_PRINT_TEXT("dstEUI ");
      ZDO_PRINT_EUI64(dstEUI);  
      leaveReqFlgs = cliGetInt16uFromArgument(4);
         
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("leaveReqFlgs: %d\r\n",leaveReqFlgs);
      
      status = emberLeaveRequest(dstId,dstEUI,leaveReqFlgs,
                                        apsOptions);
      printStatus("leaveReq", status);                       

  } else if (cliCompareStringToArgument("nwkUpdateReq", 1)) {
    if (cliCompareStringToArgument("chan", 2)) {
      int8u channel = cliGetInt16uFromArgument(3);
      if (channel < EMBER_MIN_802_15_4_CHANNEL_NUMBER
          || channel > EMBER_MAX_802_15_4_CHANNEL_NUMBER) {
        emberSerialPrintf(APP_SERIAL, "invalid channel: %d\r\n", channel);
        return;
      }
      status = emberChannelChangeRequest(channel);
    } else if (cliCompareStringToArgument("scan", 2)) {
      int16u target = cliGetInt16uFromHexArgument(3);
      int16u duration = cliGetInt16uFromArgument(4);
      int16u count = cliGetInt16uFromArgument(5);
      if (duration > 5 || count == 0 || count > 8) {
        emberSerialPrintf(APP_SERIAL, "duration must be in range 0 - 5\r\n");
        emberSerialPrintf(APP_SERIAL, "count must be in range 1 - 8\r\n");
        return;
      }
      status = emberEnergyScanRequest(target,
                                      EMBER_ALL_802_15_4_CHANNELS_MASK,
                                      duration,
                                      count);      
    } else if (cliCompareStringToArgument("set", 2)) {
      int16u managerId = cliGetInt16uFromHexArgument(3);
      int32u channelMask = cliGetInt32uFromHexArgument(4);
      status = emberSetNetworkManagerRequest(managerId, channelMask);
    } else {
      emberSerialPrintf(APP_SERIAL, "invalid nwkUpdateReq argument\r\n");
      return;
    }
    printStatus("nwkUpdateReq", status);

  //zdo lqiTblReq    <dstId:2 Hex> <idx: 1 int> 
  } else if ((cliCompareStringToArgument("lqiTblReq", 1))) {      
      dstId = cliGetInt16uFromHexArgument(2);
      idx = cliGetInt16uFromArgument(3);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("idx: %d\r\n",idx);
      
      status = emberLqiTableRequest(dstId,idx, apsOptions);
      printStatus("lqiTblReq", status);                       

  //zdo routeTblReq    <dstId:2 Hex> <idx: 1 int> 
  } else if ((cliCompareStringToArgument("routeTblReq", 1))) {      
      dstId = cliGetInt16uFromHexArgument(2);
      idx = cliGetInt16uFromArgument(3);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("idx: %d\r\n",idx);
      
      status = emberRoutingTableRequest(dstId,idx, apsOptions);
      printStatus("routeTblReq", status);                       
 
  //zdo permitJoinReq     <destId: 2 Hex> <dur: 1 int> <auth: 1 int>
  } else if ((cliCompareStringToArgument("permitJoinReq", 1))) {      
      dstId = cliGetInt16uFromHexArgument(2);
      duration = cliGetInt16uFromArgument(3);
      authentication = cliGetInt16uFromArgument(4);
      ZDO_DEBUG_PRINT_TEXT("\r\n\r\n");
      ZDO_DEBUG_PRINT("dstId: 0x%2x \r\n",dstId );
      ZDO_DEBUG_PRINT("dur: %d\r\n",duration);
      ZDO_DEBUG_PRINT("auth: %d\r\n",authentication);

      status = emberPermitJoiningRequest(dstId,duration,authentication, apsOptions);
      printStatus("permitJoinlReq", status);                       
  
  // zdo unSupported <dstId:2 Hex> <clusterId 2 Hex> 
  } else if ((cliCompareStringToArgument("unSupported", 1))) {     
    dstId = cliGetInt16uFromHexArgument(2);
    clusterId = cliGetInt16uFromHexArgument(3);
    status = emberSendZigDevRequestTarget(dstId, clusterId, apsOptions);
    printStatus("unSupportedReq", status);
  } else {
      emberSerialPrintf(APP_SERIAL, "non valid ZDO command use:\r\n\r\n");
      printZDOCliCmds();
  }  
  return;
  
}

// print binding table
void printCB(void) {
  if (cliCompareStringToArgument("bind", 1) == TRUE) {
      printBindingTableUtil(APP_SERIAL, EMBER_BINDING_TABLE_SIZE);
  } else if (cliCompareStringToArgument("debug", 1) == TRUE) {
      if (cliCompareStringToArgument("on", 2) == TRUE) {
        printReceivedMessages = TRUE;
        printMsgSentStatus = TRUE;
      } else {
        printReceivedMessages = FALSE;
        printMsgSentStatus = FALSE;
      }
  } else {
    emberSerialPrintf(APP_SERIAL, "print supports: bind and debug on off",
                      "\r\n\r\n");
  }  
}

// Print binding table helper
void printBindingTableUtil(int8u port, int8u bindingTableSize) {
    int8u i;
    EmberStatus status;
    EmberBindingTableEntry result;
    int8u* id;
    int8u indexLow;
    int8u indexHigh;

    emberSerialPrintf(port,
           "status index type local remote id  cost active\r\n");

    for (i=0; i<bindingTableSize; i++) {
      emberSerialWaitSend(port);

      status = emberGetBinding(i, &result);
      indexLow = (i % 10) + 48;
      indexHigh = ((i - (i % 10))/10) + 48;
      id = result.identifier;

      emberSerialPrintf(port, "0x%x  | %c%c | ",
                        status, indexHigh, indexLow);

      if (EMBER_SUCCESS == status) {
        switch(result.type) {
        case EMBER_UNUSED_BINDING:
          emberSerialPrintf(port, "unused....|");
          break;
        case EMBER_UNICAST_BINDING:
          emberSerialPrintf(port, "unicast...|");
          break;
        case EMBER_MANY_TO_ONE_BINDING:
          emberSerialPrintf(port, "many to one.|");
          break;  
        case EMBER_MULTICAST_BINDING:
          emberSerialPrintf(port, "multicast.|");
          break;
        default:
          emberSerialPrintf(port, "?????????.|");
        }

        emberSerialPrintf(port, "0x%x  0x%x  ",
                          result.local, result.remote);

      #ifdef AVR_ATMEGA_32
        emberSerialPrintf(port, "%x %x %x %x %x %x %x %x\r\n",
                          id[0],id[1],id[2],id[3],id[4],id[5],id[6],id[7]);
    	#else 
        emberSerialPrintf(port, "%x %x %x %x %x %x %x %x\r\n",
                          id[7],id[6],id[5],id[4],id[3],id[2],id[1],id[0]);
    	#endif  

  
      } else {
        emberSerialPrintf(port, " --- ERROR ---\r\n ");
      }
    }
    emberSerialWaitSend(port);
}


// ****************************************************************************
// print to the CLI the ZDO CLI options
// ****************************************************************************
void printZDOCliCmds(void) {
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "  abberviations\r\n");
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "  src = source\r\n");
  emberSerialPrintf(APP_SERIAL, "  dst = destination\r\n");
  emberSerialPrintf(APP_SERIAL, "  sep = source end point\r\n");
  emberSerialPrintf(APP_SERIAL, "  dep = destination end point\r\n");
  emberSerialPrintf(APP_SERIAL, "  tep = target end point\r\n");
  emberSerialPrintf(APP_SERIAL, "  rep = requested end point\r\n");
  // ***************************************************************************
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "ZDO Device and Discovery Attributes\r\n");
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "  zdo netAddrReq  <dstEUI:8 Hex>\r\n");
  emberSerialPrintf(APP_SERIAL, "                  <OPT kids: 1 str, def=t> <OPT idx: 1 int, def=0> \r\n");
  emberSerialWaitSend(APP_SERIAL);                        
  emberSerialPrintf(APP_SERIAL, "  zdo ieeeAddrReq <dstId:2 Hex> \r\n");
  emberSerialPrintf(APP_SERIAL, "                  <OPT kids: 1 str, def=t> <OPT idx: 1 int, def=0>\r\n");
  emberSerialWaitSend(APP_SERIAL);                        
  emberSerialPrintf(APP_SERIAL, "  zdo nodeDescReq         <dstId:2 Hex>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nodePwrDescReq      <dstId:2 Hex>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nodeSmplDescReq     <dstId:2 Hex> <tep: 1 int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nodeActvEndPntReq   <dstId:2 Hex>\r\n");
  emberSerialWaitSend(APP_SERIAL);                                
  
  emberSerialPrintf(APP_SERIAL, "  zdo setInClusters   <num> <clusters> \r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo setOutClusters  <num> <clusters> \r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nodeMtchDescReq <dstId:2 Hex> <profile int>\r\n");
  emberSerialWaitSend(APP_SERIAL);                                
  // ***************************************************************************
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "ZDO Bind Manager Attributes\r\n");
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL, "  zdo endBindReq   <dstId:2 Hex> <dstEUI:8 Hex> \r\n");
  emberSerialPrintf(APP_SERIAL, "                   <tep: 1 int> <profile int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo setEndPnts   <sep: 1 int> <dep: 1 int> "
                                "<rep: 1 int> \r\n");
  emberSerialWaitSend(APP_SERIAL);                                
  emberSerialPrintf(APP_SERIAL, "  zdo setBindInfo  <clusterId int> "
                                "<type int> <grpAddr: int> \r\n");
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "  zdo bindReq      <dstId:2 Hex> "
                                "<srcEUI:8 Hex> <dstEUI:8 Hex> \r\n");
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "  zdo unBindReq    <dstId:2 Hex> "
                                "<srcEUI:8 Hex> <dstEUI:8 Hex> \r\n");  
  emberSerialWaitSend(APP_SERIAL);
  // ***************************************************************************
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "ZDO Network Manager Attributes\r\n");
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
  
  emberSerialPrintf(APP_SERIAL, "  zdo bindTblReq   dstId:2 Hex> <idx: 1 int> \r\n");
  emberSerialWaitSend(APP_SERIAL);                                
  emberSerialPrintf(APP_SERIAL, "  zdo leaveReq     <dstId:2 Hex> <dstEUI:8 Hex>"
                                " <leaveReqFlgs int>\r\n");
  emberSerialWaitSend(APP_SERIAL);                                
  emberSerialPrintf(APP_SERIAL, "  zdo nwkUpdateReq chan <chan:1 int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nwkUpdateReq scan <dstId:2 Hex>"
                                 " <dur:1 int> <cnt:1 int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo nwkUpdateReq set  <mgrId:2 Hex> <mask:4 Hex>\r\n");
  emberSerialWaitSend(APP_SERIAL);    
  emberSerialPrintf(APP_SERIAL, "  zdo lqiTblReq      <destId: 2 Hex> <idx: 1 int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo routeTblReq    <destId: 2 Hex> <idx: 1 int>\r\n");
  emberSerialPrintf(APP_SERIAL, "  zdo permitJoinReq  <destId: 2 Hex> <dur: 1 int> <auth: 1 int>\r\n");
  emberSerialWaitSend(APP_SERIAL);
  // ****************************************************************************
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialPrintf(APP_SERIAL, "ZDO Send Unsupported Request\r\n");
  emberSerialPrintf(APP_SERIAL, BANNER);

  emberSerialPrintf(APP_SERIAL, "zdo unSupported  <dstId:2 Hex> <clusterId 2 Hex>\r\n");
  emberSerialWaitSend(APP_SERIAL);   
}

// ****************************************************************************
//  Decode and print the received ZDO message
// ****************************************************************************
void printZDOCmds(EmberNodeId sender,int8u addressIndex,EmberApsFrame *apsFrame)
{

  emberSerialPrintf(APP_SERIAL, "Received a ZDO command\r\n");
  emberSerialPrintf(APP_SERIAL, "[EmberApsFrame clusterId] %2x\r\n", 
                    apsFrame->clusterId); 
  emberSerialWaitSend(APP_SERIAL);
   
  //See \stack\include\zigbee-device-stack.h for the ZDO command hex values
  switch (apsFrame->clusterId) {
    case NETWORK_ADDRESS_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] NWK_addr_req \r\n");
      break;
    case NETWORK_ADDRESS_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] NWK_addr_rsp \r\n");
      break;
    case IEEE_ADDRESS_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] IEEE_addr_req \r\n");
      break;
    case IEEE_ADDRESS_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] IEEE_addr_rsp \r\n");
      break;
    case NODE_DESCRIPTOR_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Node_Desc_req \r\n");
      break;
    case NODE_DESCRIPTOR_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Node_Desc_rsp \r\n");
      break;
    case POWER_DESCRIPTOR_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Power_Desc_req \r\n");
      break;
    case POWER_DESCRIPTOR_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Power_Desc_rsp \r\n");
      break;
    case SIMPLE_DESCRIPTOR_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Simple_Desc_req \r\n");
      break;
    case SIMPLE_DESCRIPTOR_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Simple_Desc_rsp \r\n");
      break;
    case ACTIVE_ENDPOINTS_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Active_EP_req \r\n");
      break;
    case ACTIVE_ENDPOINTS_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Active_EP_rsp \r\n");
      break;
    case MATCH_DESCRIPTORS_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Match_Desc_req \r\n");
      break;
    case MATCH_DESCRIPTORS_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Match_Desc_rsp \r\n");
      break;
    case DISCOVERY_CACHE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Discovery_Cache_req \r\n");
      break;
    case NETWORK_DISCOVERY_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Discovery_Cache_rsp \r\n");
      break;
    case END_DEVICE_ANNOUNCE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Device_annce \r\n");
      break;
    case END_DEVICE_ANNOUNCE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Device_annce_rsp \r\n");
      break;
    case SYSTEM_SERVER_DISCOVERY_REQUEST:
      emberSerialPrintf(APP_SERIAL, 
                        "[ZDO Command] System_Server_Discover_req \r\n");
      break;
    case SYSTEM_SERVER_DISCOVERY_RESPONSE:
      emberSerialPrintf(APP_SERIAL, 
                        "[ZDO Command] System_Server_Discover_rsp \r\n");
      break;
    case FIND_NODE_CACHE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Find_node_cache_req \r\n");
      break;
    case FIND_NODE_CACHE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Find_node_cache_rsp \r\n");
      break;
    case END_DEVICE_BIND_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] End_Device_Bind_req \r\n");
      break;
    case END_DEVICE_BIND_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] End_Device_Bind_rsp \r\n");
      break;
    case BIND_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Bind_req \r\n");
      break;
    case BIND_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Bind_rsp \r\n");
      break;
    case UNBIND_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Unbind_req \r\n");      
      break;
    case UNBIND_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Unbind_rsp \r\n");      
      break;
    case LQI_TABLE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Lqi_req \r\n");  
      break;
    case LQI_TABLE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Lqi_rsp \r\n");  
      break;
    case BINDING_TABLE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Bind_req \r\n");   
      break;
    case BINDING_TABLE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Bind_rsp \r\n");   
      break;
    case LEAVE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Leave_req \r\n");  
      break;
    case LEAVE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Leave_rsp \r\n");  
      break;
    case PERMIT_JOINING_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Permit_Joining_req\r\n");
      break;
    case PERMIT_JOINING_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Permit_Joining_rsp\r\n");
      break;
    case DIRECT_JOIN_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Direct_Join_req \r\n");  
      break;
    case DIRECT_JOIN_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Direct_Join_rsp \r\n");  
      break;
    case NWK_UPDATE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Nwk_Update_req \r\n");
      break;  
    case NWK_UPDATE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Mgmt_Nwk_Update_rsp \r\n");
      break;  
    case ROUTING_TABLE_REQUEST:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Routing_Table_req \r\n");
      break;
    case ROUTING_TABLE_RESPONSE:
      emberSerialPrintf(APP_SERIAL,"[ZDO Command] Routing_Table_rsp \r\n");
      break;
    default:
      emberSerialPrintf(APP_SERIAL, 
                        "Not a decoded ZDO command\r\n");
  }
  emberSerialPrintf(APP_SERIAL, BANNER);
  emberSerialWaitSend(APP_SERIAL);
 
  emberSerialPrintf(APP_SERIAL, "[EmberApsFrame sourceEndpoint] %x\r\n", 
                    apsFrame->sourceEndpoint); 
  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL, "[EmberApsFrame destinationEndpoint] %x\r\n", 
                    apsFrame->destinationEndpoint);  
  emberSerialPrintf(APP_SERIAL, "[EmberApsFrame profileId] %2x\r\n", 
                    apsFrame->profileId); 
  emberSerialPrintf(APP_SERIAL, "[EmberApsFrame options] %2x\r\n", 
                    apsFrame->options);
  emberSerialWaitSend(APP_SERIAL);
   // the call is above 
  emberSerialPrintf(APP_SERIAL, "[emberGetSender()] %2x\r\n", sender);
  emberSerialPrintf(APP_SERIAL, "[emberGetBindingIndex()] %x\r\n",addressIndex);
  emberSerialWaitSend(APP_SERIAL);


}

void helpCB(void);
// *****************************
// required setup for command line interface (CLI)
// see app/util/serial/cli.h for more information
// *****************************
cliSerialCmdEntry cliCmdList[] = {
  {"help", helpCB},
  {"version", versionCB},
  {"info", infoCB},
  {"network", networkCB},
  {"zdo", zdoCB},
  {"print", printCB},
  {"counter", counterCB},
  {"reset", resetCB}
};
int8u cliCmdListLen = sizeof(cliCmdList)/sizeof(cliSerialCmdEntry);
PGM_P cliPrompt = "ZDO-HOST";

// *****************************
// msHelpCB
// *****************************
void helpCB() {
  int8u i;
  for (i=0; i<cliCmdListLen; i++) {
    emberSerialPrintf(APP_SERIAL, "%p\r\n", cliCmdList[i].cmd);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

// ****************************************************************************
//  getNodeTypeFromBuffer
//  for serial input, reads the node type from the buffer
// ****************************************************************************
int8u getNodeTypeFromBuffer(void) {
  if (cliCompareStringToArgument("coordinator", 2) == TRUE){
    return EMBER_COORDINATOR;
  }
  else if (cliCompareStringToArgument("router", 2) == TRUE){
    return EMBER_ROUTER;
  }
  else if (cliCompareStringToArgument("end", 2) == TRUE){
    return EMBER_END_DEVICE;
  }
  else if (cliCompareStringToArgument("sleepy", 2) == TRUE){
    return EMBER_SLEEPY_END_DEVICE;
  }
  else if (cliCompareStringToArgument("mobile", 2) == TRUE){
    return EMBER_MOBILE_END_DEVICE;
  } else {
    return EMBER_UNKNOWN_DEVICE;
  }  
}

