//----------------------------------------------------------------
// bootloader-demo-v2.c
// 
// Main loop for the standalone bootloader demonstration for EM250 and EM357 
// platforms.  Please see bootloader-demo.h for a description of the complete 
// application.  Also see bootload-utils.h for a description of various 
// bootload methods supported.  
//
// Copyright 2006-2010 by Ember Corporation. All rights reserved.           *80*
//----------------------------------------------------------------

#include "bootloader-demo-v2.h"

#include <stdarg.h>

// For this example, all nodes are routers.
EmberNodeType nodeType = EMBER_ROUTER;

// Generic buffer array used to create radio packet
#define APP_BUFFER_SIZE 32
int8u globalBuffer[APP_BUFFER_SIZE];

// Determine wheather to use unicast or broadcast
boolean useBroadcast = FALSE;

// Eui64 address of the target node to be bootloaded
EmberEUI64 bootloadNodeId;

//----------------------------------------------------------------
// Forward declarations for the benefit of the compiler.

static void printfEuid(EmberEUI64 eui, boolean isQueryMessage);

// We use two buttons, one to trigger network actions and one for version info.
#define NETWORK_BUTTON BUTTON0
#define VERSION_BUTTON BUTTON1

// This example uses one endpoint. We need only a minimal endpoint descriptions 
// because we do not use any of the ZDO's provisioning services.

int8u emberEndpointCount = 1;
EmberEndpointDescription PGM endpointDescription = { PROFILE_ID, 0, };
EmberEndpoint emberEndpoints[] = {
  { ENDPOINT, &endpointDescription }
};

EmberNetworkParameters networkParams;

//----------------------------------------------------------------
// Serial command interpreter

// See app/util/serial/command-interpreter2.h for documentation.

#ifndef SBL_LIB_TARGET
  PGM_NO_CONST PGM_P applicationString = "bootloader demo version 2";
#else
  PGM_NO_CONST PGM_P applicationString = "bootloader demo version 2 TARGET ONLY";
#endif

EmberCommandEntry emberCommandTable[] = {
  DEMO_APPLICATION_GENERIC_COMMANDS
  #ifndef SBL_LIB_TARGET
  DEMO_APPLICATION_SOURCE_NODE_COMMANDS
  #endif
  #ifndef SBL_LIB_SRC_NO_PASSTHRU
  DEMO_APPLICATION_PASSTHRU_COMMAND
  #endif
  DEMO_DEBUG_COMMANDS
  { NULL, NULL, NULL, NULL } // NULL action makes this a terminator
};

//----------------------------------------------------------------
// Application events.  The blink event toggles a heartbeat LED and its handler,
// toggleBlinker, is defined in util/common/common.c
EmberEventControl queryEndEvent;


// -------------------------------------------------------------------------
// This function is called after the query is finished
void queryEndEventHandler(void)
{
  emberSerialPrintf(APP_SERIAL, "\r\nQQ query end QQ\r\n");
  emberEventControlSetInactive(queryEndEvent);
}

//----------------------------------------------------------------
// All events supported by the application.  

EmberEventData appEvents[] =
{
  { &blinkEvent,      toggleBlinker           },
  { &queryEndEvent,   queryEndEventHandler    },
  { NULL, NULL }      // terminator
};

//----------------------------------------------------------------
// Indicating state changes

// If tuneOn variable is true then the device will play a simple tune
// when the network comes up, the network comes down, the network fails,
// or when a button action is recognized.
boolean tuneOn = TRUE;

// The following arrays each define a series of tones used to indicate 
// certain activities in the application

// Played when the device is completely associated to the network and
// ready to participate in network events
int8u PGM onlineTune[] = {      
        NOTE_B3,        2,
        NOTE_B4,        2,
        0,              0
};

// Played when the device is completely disassociated from the network and
// no longer processing any network events.
int8u PGM offlineTune[] = {     
        NOTE_B4,        2,
        NOTE_B3,        2,
        0,              0
};

// Played when the device encounters a problem related to network
// participation from which there is no method of recovery implemented.
int8u PGM failureTune[] = {     
        NOTE_E3,        6,
        0,              0
};

// Played when the application successfully processes a button action,
// so the user knows he/she executed a button-pushing sequence correctly.
int8u PGM buttonActionConfirmedTune[] = {       
        NOTE_D4,        1,
        0,              0
};


// The following functions are used to indicate changes in the application
// for the benefit of the user

// Indicates that the device has come online and is participating in  network
void indicateNetworkUp(void)
{
  halResetWatchdog();
  if (tuneOn) {
    halPlayTune_P(onlineTune, FALSE);
  }
}

// Indicates that the device has gone offline and is completey removed from
// the network
void indicateNetworkDown(void)
{
  halResetWatchdog();
  if (tuneOn) {
    halPlayTune_P(offlineTune, FALSE);
  }
}

// Indicate that an unrecoverable network-related failure has occurred
void indicateNetworkFailure(void)
{
  halResetWatchdog();
  if (tuneOn) {
    halPlayTune_P(failureTune, FALSE);
  }
}

// Indicate that a button action was successfully processed by the button
// handling routines
void indicateButtonAction(void)
{
  halResetWatchdog();
  if (tuneOn) {
    halPlayTune_P(buttonActionConfirmedTune, FALSE);
  }
}

// The user has the option of turning tunes (that indicate device state
// changes) on or off.
void setTuneCommand(void)
{
  tuneOn = emberUnsignedCommandArgument(0);
}

//----------------------------------------------------------------
// The main() loop and the application's contribution.

int main(void)
{
  EmberStatus status;
  
  //Initialize the hal
  halInit();

  INTERRUPTS_ON();  // Safe to enable interrupts at this point
  
  // Initialize app serial to highest possible speed (115200 kbps).  If failed,
  // then initialize to 38400 kbps. If failed,
  // then initialize to 19200 kbps.
  if(emberSerialInit(APP_SERIAL, BAUD_115200, PARITY_NONE, 1)
      != EMBER_SUCCESS) {
    if (emberSerialInit(APP_SERIAL, BAUD_38400, PARITY_NONE, 1)
        != EMBER_SUCCESS) {
      emberSerialInit(APP_SERIAL, BAUD_19200, PARITY_NONE, 1);
      configureSerial(APP_SERIAL, BAUD_19200);
    } else {
      configureSerial(APP_SERIAL, BAUD_38400);
    }
  } else {
    configureSerial(APP_SERIAL, BAUD_115200);
  }
  #if defined(XAP2B) || defined(CORTEXM3)
    emberDebugInit(APP_SERIAL);
  #endif

  emberSerialPrintf(APP_SERIAL, "\r\n");
  emberSerialWaitSend(APP_SERIAL);

#ifdef ENABLE_BOOTLOAD_PRINT
  emberSerialPrintf(APP_SERIAL, "ENABLE_BOOTLOAD_PRINT build\r\n");
  emberSerialPrintf(APP_SERIAL, "Reset: %p\r\n", (PGM_P)halGetResetString());
  emberSerialWaitSend(APP_SERIAL);
#endif

#ifdef ENABLE_DEBUG
  emberSerialPrintf(APP_SERIAL, "ENABLE_DEBUG build\r\n");
  emberSerialWaitSend(APP_SERIAL);
#endif

  emberCommandReaderInit();
  emberEventControlSetDelayMS(blinkEvent, 500);       // every half second
  statusCommand();

  // clear the networkParams struct and set the join method.
  // This app always uses the MAC_ASSOCIATION join method.
  MEMSET(&networkParams, 0, sizeof(EmberNetworkParameters));
  networkParams.joinMethod = EMBER_USE_MAC_ASSOCIATION;

  #ifdef USE_HARDCODED_NETWORK_SETTINGS
  // Set network parameters
  MEMCOPY(networkParams.extendedPanId, 
      BOOTLOADER_DEMO_EXT_PAN_ID, 
      EXTENDED_PAN_ID_SIZE);
  networkParams.panId = BOOTLOADER_DEMO_PAN_ID;
  networkParams.radioTxPower = BOOTLOADER_DEMO_POWER;
  networkParams.radioChannel = BOOTLOADER_DEMO_CHANNEL;
  #endif

  // Initialize the Ember Stack.  For error status, please refer to 
  // stack/include/error-def.h
  status = emberInit();
  if (status != EMBER_SUCCESS) {
    emberSerialGuaranteedPrintf(APP_SERIAL, 
                                "ERROR: emberInit 0x%x\r\n", status);
    // The app can choose what to do here.  If the app is running
    // another device then it could stay running and report the 
    // error visually for example. This app asserts.
    assert(FALSE);
  }

  // Initialize bootload serial and application serial ports in bootload
  // utilities library.  The function also checks to see if the node
  // needs to do (default) bootload recovery before come up to the network.
  bootloadUtilInit(APP_SERIAL, BOOTLOAD_SERIAL);

#if defined(XAP2B) || defined(CORTEXM3)
  emberSerialPrintf(APP_SERIAL, 
    "Standalone Bootloader Demo V2. Reset cause: %p\r\n", 
    (PGM_P)halGetResetString());
#endif

  networkInitAction();
  
  // the run function lives in app/util/common.[ch]
  // it runs appTick() along with emberTick() and halResetWatchdog()
  // it also calls emberRunEvents on appEvents - to service application events
  run(appEvents, appTick);
  return 0;
}

// after reset, attempt to become a part of the network we were previously 
// a part of
void networkInitAction(void)
{
  EmberStatus status = emberNetworkInit();
  emberSerialPrintf(APP_SERIAL, "Network init status %X\r\n", status);
  switch (status) {
  case EMBER_SUCCESS:
    break;
  case EMBER_NOT_JOINED:
    // We can't resume our place in our former network.  We notify the user
    // that this node is effectively offline.
    indicateNetworkDown();
    break;
  case EMBER_INVALID_CALL:
    // The status is returned if called when network state is not in the
    // initial state or if scan is pending
    emberSerialPrintf(APP_SERIAL, "Invalid call of NetworkInit()\r\n");
    break;
  default:
    // Something is wrong and we cannot proceed.  Indicate user and terminate.
    indicateNetworkFailure();
    emberSerialPrintf(APP_SERIAL,
                                "ERROR: network initialization error 0x%x\r\n",
                                status);
    assert(FALSE);
  }
}

#ifndef USE_HARDCODED_NETWORK_SETTINGS
static int32u channelMask = ALL_CHANNELS;
#endif

void setChannelCommand(void)
{
  int8u channel;
  channel = emberUnsignedCommandArgument(0);
  #ifdef USE_HARDCODED_NETWORK_SETTINGS
    networkParams.radioChannel = channel;
  #else  
    channelMask = ((int32u) 1) << channel;
  #endif
  emberSerialPrintf(APP_SERIAL, "Channel set to %d\r\n", channel);
}

void setPanIDCommand(void)
{
  int16u panID;

  panID = emberUnsignedCommandArgument(0);
  #ifdef USE_HARDCODED_NETWORK_SETTINGS
    networkParams.panId = panID;
    emberSerialPrintf(APP_SERIAL, "Pan ID set to 0x%2x\r\n", panID);
  #endif
}

void setPowerCommand(void)
{
  int8s power;

  power = emberSignedCommandArgument(0);
  #ifdef USE_HARDCODED_NETWORK_SETTINGS
    networkParams.radioTxPower = power;
    emberSerialPrintf(APP_SERIAL, "Power set to %d\r\n", power);
  #endif
}

void getHelpCommand(void)
{
  int8u i, j;
  int8u text[EMBER_MAX_COMMAND_LENGTH + 1];

  for (i = 0; emberCommandTable[i].action != NULL; i++) {
    if (emberCommandTable[i].description) {
      PGM_P name = emberCommandTable[i].name;

      for(j=0; name[j]; j++) {
        text[j] = name[j];
      }
      for(; j < EMBER_MAX_COMMAND_LENGTH; j++) {
        text[j] = ' ';
      }
      text[j] = '\0';
      emberSerialPrintf(APP_SERIAL, "%s ", text);
      emberSerialWaitSend(APP_SERIAL);
      emberSerialPrintf(APP_SERIAL, "%p\r\n",
                        emberCommandTable[i].description);
      emberSerialWaitSend(APP_SERIAL);
    }
  }
}

void listCommands(void)
{
  int8u i, j;
  int8u text[EMBER_MAX_COMMAND_LENGTH + 1];

  emberSerialPrintf(APP_SERIAL, "List of commands:\r\n");
  emberSerialWaitSend(APP_SERIAL);
  for (i = 0; emberCommandTable[i].action != NULL; i++) {
    PGM_P name = emberCommandTable[i].name;
    emberSerialPrintf(APP_SERIAL, "  ");

    for(j=0; name[j]; j++) {
      text[j] = name[j];
    }
#ifdef LISTDETAILS
    text[j] = '\0';
    emberSerialPrintf(APP_SERIAL, "longName: %s\r\n", text);
#else
    for(; j < EMBER_MAX_COMMAND_LENGTH; j++) {
      text[j] = ' ';
    }
    text[j] = '\0';
    emberSerialPrintf(APP_SERIAL, "%s ", text);
#endif
    emberSerialWaitSend(APP_SERIAL);
    name = emberCommandTable[i].argumentTypes;
    for(j=0; name[j]; j++) {
      text[j] = name[j];
    }
#ifdef LISTDETAILS
    text[j] = '\0';
    emberSerialPrintf(APP_SERIAL, "argumentTypes: %s\r\n", text);
#else
    for(; j < 10; j++) {
      text[j] = ' ';
    }
    text[j] = '\0';
    emberSerialPrintf(APP_SERIAL, "%s ", text);
#endif
    emberSerialWaitSend(APP_SERIAL);
    if (emberCommandTable[i].description) {
#ifdef LISTDETAILS
      emberSerialPrintf(APP_SERIAL, "description: %p\r\n",
                        emberCommandTable[i].description);
#else
      emberSerialPrintf(APP_SERIAL, "%p\r\n",
                        emberCommandTable[i].description);
#endif
    } else {
      emberSerialPrintf(APP_SERIAL, "\r\n");
    }
    emberSerialWaitSend(APP_SERIAL);
  }
}

static int8u failedJoinAttempts = 0;

void decodeNodeType(EmberNodeType nodeType)
{

  switch(nodeType) {
  default:
  case EMBER_UNKNOWN_DEVICE:
    emberSerialPrintf(APP_SERIAL, "unknown %d", nodeType);
    break;
  case EMBER_COORDINATOR:
    emberSerialPrintf(APP_SERIAL, "coordinator");
    break;
  case EMBER_ROUTER:
    emberSerialPrintf(APP_SERIAL, "router");
    break;
  case EMBER_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "end device");
    break;
  case EMBER_SLEEPY_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "sleepy end device");
    break;
  case EMBER_MOBILE_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "mobile end device");
    break;
  }
}

void getNodeIdAction(void)
{
  EmberStatus status;
  EmberNodeType nodeType;

  emberSerialPrintf(APP_SERIAL, "node id: %2x\r\n", emberGetNodeId());

  status = emberGetNodeType(&nodeType);
  if (status == EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, " Node type: ");
    decodeNodeType(nodeType);
    emberSerialPrintf(APP_SERIAL, "\r\n");
  } else {
    emberSerialPrintf(APP_SERIAL, "\r\n");
    emberSerialPrintf(APP_SERIAL,
                      "Failed to get node type: 0x%x",
                      status);
    emberSerialPrintf(APP_SERIAL, "\r\n");
  }
#ifdef USE_HARDCODED_NETWORK_SETTINGS
  emberSerialPrintf(APP_SERIAL,
                    "  Channel 0x%x, panId 0x%2x, TxPower %d",
                    networkParams.radioChannel, networkParams.panId,
                    networkParams.radioTxPower);
  emberSerialPrintf(APP_SERIAL, "\r\n");
#endif
}

//----------------------------------------------------------------
// Other utility functions.

static void printfEuid(EmberEUI64 eui, boolean isQueryMessage)
{
  if ((emberEventControlGetActive(queryEndEvent) == FALSE)
      || isQueryMessage) {
    emberSerialPrintf(APP_SERIAL, "%x%x%x%x%x%x%x%x ",
                      eui[0], eui[1], eui[2], eui[3],
                      eui[4], eui[5], eui[6], eui[7]);
  }
}


//----------------------------------------------------------------
// Functions supported by this bootloader demonstration application.

// Forming a network.  Note that this application uses standard security 
// level 5 with no trust center.  The application also has both link key and 
// network key preconfigured which is recommended.  Please refer to sensor/sink 
// application for using standard security with trust center example.
void formAction(void)
{
  int16u bmask;

  if (nodeType == EMBER_ROUTER) {
    bmask = EMBER_NO_TRUST_CENTER_MODE
            | EMBER_TRUST_CENTER_GLOBAL_LINK_KEY
            | EMBER_HAVE_PRECONFIGURED_KEY
            | EMBER_HAVE_NETWORK_KEY;
    setSecurityState(bmask);
    emberSerialPrintf(APP_SERIAL, "Attempting to form network\r\n");
#ifdef USE_HARDCODED_NETWORK_SETTINGS
    emberSerialPrintf(APP_SERIAL, "Form network: %x\r\n",
                      emberFormNetwork(&networkParams) );
#else
    emberScanForUnusedPanId(channelMask, 5); // 507 ms duration
#endif
  }
}

// Join the network as router node.  Note that this application uses security 
// level 5 with standard security mode and with no trust center.  The joining 
// node only has link key pre-configured.  The node will be giving the network 
// key during the association.  Please refer to sensor/sink application for 
// using standard security with trust center example.
void joinAction(void)
{
  int16u bmask;

  if (emberStackIsUp()) {
    int8u message = JOIN_TIMEOUT;
    emberSerialPrintf(APP_SERIAL, "Allow join action\r\n");
    sendBroadcast(ENABLE_JOINING_MESSAGE, &message, 1, BROADCAST_RADIUS);
    emberPermitJoining(JOIN_TIMEOUT);
  } else {
    emberSerialPrintf(APP_SERIAL,"Attempting to join network\r\n");
    failedJoinAttempts = 0;
    bmask = EMBER_TRUST_CENTER_GLOBAL_LINK_KEY
            | EMBER_HAVE_PRECONFIGURED_KEY
            | EMBER_REQUIRE_ENCRYPTED_KEY;
    setSecurityState(bmask);
#ifdef USE_HARDCODED_NETWORK_SETTINGS
    emberSerialPrintf(APP_SERIAL,"Join network: %x\r\n",
                      emberJoinNetwork(nodeType,
                                       &networkParams));
#else
    emberScanForJoinableNetwork(channelMask, networkParams.extendedPanId);
#endif
  }
}

// Leave network
void leaveAction(void)
{
  emberSerialPrintf(APP_SERIAL,"Attempting leave action\r\n");
  emberLeaveNetwork();
}

// The user has pressed the NETWORK_BUTTON to trigger a network event.
// We use the current state to determine what action to take.
void networkAction(void)
{
  if (emberNetworkState() == EMBER_JOINED_NETWORK)
    leaveAction();
  else
    joinAction();
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
  int16u blVersion; // if the version is invalid, the value is 0xFFFF

  MEMCOPY(&globalBuffer[0], emberGetEui64(), 8);
  globalBuffer[8] = APPLICATION_ID;
  globalBuffer[9] = APPLICATION_VERSION;
  globalBuffer[10] = PLAT;
  blVersion = halGetStandaloneBootloaderVersion();
  globalBuffer[11] = HIGH_BYTE(blVersion); // bootloader version
  globalBuffer[12] = LOW_BYTE(blVersion); // bootloader build

  if (sendBroadcast(VERSION_MESSAGE, globalBuffer, 13, BROADCAST_RADIUS) 
      == EMBER_SUCCESS)
    emberSerialPrintf(APP_SERIAL,"Sending version information\r\n");
  else
    emberSerialPrintf(APP_SERIAL,"Could not send version information\r\n");
    
}

// This command is used to query all devices on the network. When queries
// come back a message it printed out the serial port which is picked up
// by the java application (or user) running the query
void queryNetworkAction(void)
{
  if (emberEventControlGetActive(queryEndEvent) == FALSE) {
    emberSerialPrintf(APP_SERIAL, "QQ query network start QQ\r\n");
    if(sendBroadcast(QUERY_MESSAGE, NULL, 0, BROADCAST_RADIUS) 
       == EMBER_SUCCESS) {
       emberEventControlSetDelayMS(queryEndEvent, 2500);
    }
  }
}


// This command is used to query all neighboring devices. When queries
// come back, the node printed out a message to the serial port. The information
// returned from the neighbors tells the query node about the current state
// of the node (running application or bootload), if the node expects any 
// security during bootload, and manufacture Id and hardware tag of the node.
// The information is useful in choosing which node to bootload.  
void queryNeighborAction(void)
{
  if (emberEventControlGetActive(queryEndEvent) == FALSE) {
    // Set the target node id to all 0xFF for broadcast message
    MEMSET(bootloadNodeId, 0xFF, EUI64_SIZE);     
  
    emberSerialPrintf(APP_SERIAL, "QQ query neighbor start QQ\r\n");
    // Send MAC level broadcast query to all neighbors.
    bootloadUtilSendQuery(bootloadNodeId);
    emberEventControlSetDelayMS(queryEndEvent, 2500);
  }
}

#ifndef SBL_LIB_SRC_NO_PASSTHRU
// This function is called when the device attempts to bootload a remote
// device using the gateway node (this node) as the source node. This
// bootloading strategy is called 'passthru'.
void bootloadRemoteAction(void)
{
// The key that is used to encrypt the challenge issued from the
// target node as part of the bootloader launch authentication
// protocol.  It is normally stored as a manufacturing token.
#ifdef HALGETTOKEN
  tokTypeMfgBootloadAesKey encryptKey;
#else
  int8u encryptKey[16] = {
    0xff, 0xff, 0xff, 0xff,  // All 0xff's corresponds to the default key value.
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff
  };
#endif

#ifdef HALGETTOKEN
  // Load the secret key to encrypt the challenge.
  halCommonGetToken(&encryptKey, TOKEN_MFG_BOOTLOAD_AES_KEY);
#endif

  // The PC passes us the target EUI64 as a command argument.
  emberCopyEui64Argument(0, bootloadNodeId);
    
  // We now want to send a bootload message to request the target node 
  // to go into bootload mode.  
  bootloadUtilSendRequest(bootloadNodeId,
                          mfgId,
                          hardwareTag,
                          encryptKey,
                          BOOTLOAD_MODE_PASSTHRU);
  
  emberSerialPrintf(APP_SERIAL, "Attempting to bootload remote node.\r\n");
}
#endif // SBL_LIB_SRC_NO_PASSTHRU


// This function is called in the case where we are trying to recover
// a device that has moved to the default bootload channel. This happens 
// in the case where the bootload is interrupted by the receiver resetting,
// or after the bootload interruption, the receiver was reset. Since the
// device has no idea what channel it should be on, it defaults to
// channel 13 (BOOTLOADER_DEFAULT_CHANNEL).
//
// Before calling this function, the node needs to already be on the default
// channel (13).  
//
// Note that the parameter passed into the command is the recovery mode,
// passthru (1).
void recoverDefaultAction(void)
{
  int16u mode;
  emberSerialPrintf(APP_SERIAL, "Attempting default channel recovery.\r\n");
   
  if(emberGetNetworkParameters(&networkParams) != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Error:  cannot read current channel\r\n");
    return;
  } else {
    if(networkParams.radioChannel != BOOTLOADER_DEFAULT_CHANNEL) {
      emberSerialPrintf(APP_SERIAL, 
        "Error: invalid channel, 0x%x.  Should be on 0x%x\r\n", 
        networkParams.radioChannel, BOOTLOADER_DEFAULT_CHANNEL);
      return;
    }
  }

  mode = emberUnsignedCommandArgument(0);
  // verify requested mode was built-in here instead of changing command format
#ifdef SBL_LIB_SRC_NO_PASSTHRU
  if (mode == 1) {// passthru
    emberSerialPrintf(APP_SERIAL, "passthru mode not available.\r\n");
    return;
  }
#endif

  // In case of default channel recovery, we currently always send 
  // broadcast start bootload message since we do not save the eui64 of
  // the node to be recovered in the token.
  bootloadUtilStartBootload(broadcastEui64, mode);
}

// This function is called in the case where we are trying to recover
// a device that is still on the same channel that the original
// bootload was started on. This happens in the case where the bootload
// is interrupted but the received node does not reset.
//
// Note that the parameter passed into the command is the recovery mode,
// passthru (1).
void recoverAction(void)
{
  int16u mode;

  emberSerialPrintf(APP_SERIAL, "Attempting recovery.\r\n");

  // The PC passes us the target EUI64 as a command argument.
  emberCopyEui64Argument(0, bootloadNodeId);
  
  mode = emberUnsignedCommandArgument(1);

  // verify requested mode was built-in here instead of changing command format
#ifdef SBL_LIB_SRC_NO_PASSTHRU
  if (mode == 1) {// passthru
    emberSerialPrintf(APP_SERIAL, "passthru mode not available.\r\n");
    return;
  }
#endif

  // Pass in the target eui64 (all 0xFF in case of broadcast) and bootload mode
  bootloadUtilStartBootload(bootloadNodeId, mode);
}

// We enter the bootloader as a serial target.  The bootload mode is set
// to zero in this case.
void serialBootloaderAction(void)
{
  emberSerialPrintf(APP_SERIAL, "Entering serial bootloader. BYE!\r\n");
  emberSerialWaitSend(APP_SERIAL);
  halLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
}
//----------------------------------------------------------------
// Buttons
//
// We use two buttons, NETWORK_BUTTON (BUTTON0) and VERSION_BUTTON (BUTTON1).
// 
// The NETWORK_BUTTON triggers a network action, such as joining or leaving
// the network.  The VERSION_BUTTON triggers the transmission of an information
// message, containing this node's EUID and program version.
//
// Button actions are procedures.

typedef void (*ButtonAction)(void);

// The button ISR sets this to whatever action is appropriate for the user's
// button presses.  The action is called by the main loop via the buttonTick()
// function.

static ButtonAction buttonAction = NULL;

// WARNING: this callback is an ISR so the best approach is to set a
// flag here when an action should be taken and then perform the action
// somewhere else. In this case the actions are serviced in the appTick
// function.

void halButtonIsr(int8u button, int8u event)
{
  // This ISR fires for both a button press and a button release.
  // We take action only on the release.
  if (event == BUTTON_RELEASED) {
    if (button == NETWORK_BUTTON)
      buttonAction = networkAction;
    else
      buttonAction = sendVersionAction;
  }
}

// This is run once each time around the main loop and is used
// to execute the commands scheduled by the button ISR.

void buttonTick(void)
{
  DECLARE_INTERRUPT_STATE;
  ButtonAction temp;

  DISABLE_INTERRUPTS();
  temp = buttonAction;
  buttonAction = NULL;
  RESTORE_INTERRUPTS();

  // If there is a valid button action ready to happen, perform it
  // and notify the user that something is happening.
  if (temp != NULL)
  {
    indicateButtonAction();
    (temp)();
  }
}

//----------------------------------------------------------------
// This is called once for each iteration of the main loop.  

void appTick(void)
{
  // run bootload state machine
  bootloadUtilTick();

  // check for any button action
  buttonTick();
}


//----------------------------------------------------------------
// Handlers required to use the Ember Stack.

// Called when the stack status changes, usually as a result of an
// attempt to form, join, or leave a network.  Failure to join is
// handled by a device-specific procedure.

void emberStackStatusHandler(EmberStatus status)
{
  emberSerialPrintf(APP_SERIAL, "Stack status %X\r\n", status);

  switch (status) {
  case EMBER_NETWORK_UP:
    indicateNetworkUp();
    break;
  case EMBER_NETWORK_DOWN:
    indicateNetworkDown();
    break;
  case EMBER_MOVE_FAILED:
    // An end device has failed to find a new parent.  We try again.
    if (failedJoinAttempts < 3) {
      failedJoinAttempts += 1;
      emberRejoinNetwork(TRUE);  // Assume we have the current NWK key
    } else {
      // We can't seem to keep ourselves in the network, so notify user
      indicateNetworkFailure();
    }
    break;
  case EMBER_JOIN_FAILED:
    // We failed to join or rejoin the desired network.  Possible reasons
    // include joining not being enabled, there are no available child slots,
    // and communication difficulties.
    if (failedJoinAttempts < 3) {
      failedJoinAttempts += 1;
#ifdef USE_HARDCODED_NETWORK_SETTINGS
      emberSerialPrintf(APP_SERIAL, "Join network: %x\r\n",
                          emberJoinNetwork(nodeType,
                                           &networkParams));
#else
      emberScanForJoinableNetwork(channelMask, networkParams.extendedPanId);
#endif
    } else {
      // We can't seem to get into the network, so notify user
      indicateNetworkFailure();
    }
    break;
  }
}


// Called when a message arrives.

void emberIncomingMessageHandler(EmberIncomingMessageType type,
                                 EmberApsFrame *apsFrame,
                                 EmberMessageBuffer message)
{
  int8u messageLength;
  // Limit radio activities when doing boootload.
  if(!IS_BOOTLOADING) {

    messageLength = emberMessageBufferLength(message);

    // Copy from the buffer to allow for more direct access to the message.
    emberCopyFromLinkedBuffers(message,
                               0,
                               globalBuffer,
                               (messageLength <= APP_BUFFER_SIZE
                                ? messageLength
                                : APP_BUFFER_SIZE));

    switch (apsFrame->clusterId) {
      
    case ENABLE_JOINING_MESSAGE:
      emberSerialPrintf(APP_SERIAL, "rx enable joining\r\n");
      emberPermitJoining(globalBuffer[0]);
      break;
    case VERSION_MESSAGE:
      emberSerialPrintf(APP_SERIAL,"QQ rx version: ");
      printfEuid(&globalBuffer[0], TRUE);
      emberSerialPrintf(APP_SERIAL,"app:%x ver:%x plat:%x blV:%x blB:%x QQ\r\n", 
                        globalBuffer[8], globalBuffer[9], globalBuffer[10], 
                        globalBuffer[11], globalBuffer[12]);
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


// It is up to the application to change the binding, if it wishes to.
// We don't.

EmberStatus emberRemoteSetBindingHandler(EmberBindingTableEntry *entry)
{
  // Do not permit others to change our bindings.
  return EMBER_INVALID_CALL;
}

EmberStatus emberRemoteDeleteBindingHandler(int8u index)
{
  // Do not permit others to change our bindings.
  return EMBER_INVALID_CALL;
}

//----------------------------------------------------------------
// Handler called by the form-and-join utility code.

void emberUnusedPanIdFoundHandler(EmberPanId panId, int8u channel)
{
  EmberStatus status;
  networkParams.panId = panId;
  networkParams.radioChannel = channel;

  status = emberFormNetwork(&networkParams);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Form error 0x%x", status);
  }
}

void emberScanErrorHandler(EmberStatus status)
{
  emberSerialPrintf(APP_SERIAL, "Scan error 0x%x", status);
}

void emberJoinableNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                                      int8u lqi,
                                      int8s rssi)
{
  EmberStatus status;
  networkParams.panId = networkFound->panId;
  networkParams.radioChannel = networkFound->channel;

  status = emberJoinNetwork(nodeType, &networkParams);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Join error 0x%x", status);
  }
}

//----------------------------------------------------------------
// Callbacks we don't care about.

// Called when a message we sent is acked by the destination or when an
// ack fails to arrive after several retransmissions.

void emberMessageSentHandler(EmberOutgoingMessageType type,
                      int16u indexOrDestination,
                      EmberApsFrame *apsFrame,
                      EmberMessageBuffer message,
                      EmberStatus status)
{
}

// As a router we never poll.

void emberPollCompleteHandler(EmberStatus status)
{
}

//----------------------------------------------------------------
// Handler called by the bootload-utils code.

boolean bootloadUtilLaunchRequestHandler(int16u manufacturerId,
                                         int8u *hardwareTag,
                                         EmberEUI64 sourceEui) {
  int8u lqi;
  int8s rssi;
  EmberStatus lqiStatus, rssiStatus;

  emberSerialPrintf(APP_SERIAL,"Received Bootloader Launch Request\r\n");
  
  // COULD DO: Compare manufacturer and hardware id arguments to known values.
  
  // COULD DO: Check for minimum required radio signal strength (RSSI).
  
  // COULD DO: Do not agree to launch the bootloader if any of the above 
  // conditions are not met.  For now, always agree to launch the bootloader.

  lqiStatus = emberGetLastHopLqi(&lqi);
  rssiStatus = emberGetLastHopRssi(&rssi);
  emberSerialPrintf(APP_SERIAL,"lqiStatus:%x lqi:%x rssiStatus:%x rssi:%d\r\n",
                    lqiStatus,lqi,rssiStatus,rssi);
  emberSerialWaitSend(APP_SERIAL);

  if ( (lqiStatus != EMBER_SUCCESS) || (lqi < 236) ) {
    // Either the LQI information is not available or the reported LQI indicates
    // that the link too poor to proceed.
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
  int8u i;
  int8u lqi;
  int8s rssi;
  EmberStatus lqiStatus, rssiStatus;
  int8u blV, blB;
  
  emberSerialPrintf(APP_SERIAL,"QQ rx eui: ");
  printLittleEndianEui64(APP_SERIAL, targetEui);

  emberSerialPrintf(APP_SERIAL," blActive:%x ", bootloaderActive);

  blV = HIGH_BYTE(blVersion); // bootloader version
  blB = LOW_BYTE(blVersion); // bootloader build

  emberSerialPrintf(APP_SERIAL,"blVersion:%2x blV:%x blB:%x ",
    blVersion, blV, blB);
  
  emberSerialPrintf(APP_SERIAL,"\r\nmfgId:%2x", manufacturerId);

  emberSerialWaitSend(APP_SERIAL);
  emberSerialPrintf(APP_SERIAL," hwTag:");

  for(i=0; i< BOOTLOAD_HARDWARE_TAG_SIZE; ++i) {
    emberSerialPrintf(APP_SERIAL, "%x", hardwareTag[i]);
  }
  emberSerialWaitSend(APP_SERIAL);

  emberSerialPrintf(APP_SERIAL,"\r\nblCap:%x plat:%x micro:%x phy:%x ",
    bootloaderCapabilities, platform, micro, phy);

  lqiStatus = emberGetLastHopLqi(&lqi);
  rssiStatus = emberGetLastHopRssi(&rssi);
  emberSerialPrintf(APP_SERIAL,"lqiStatus:%x lqi:%x rssiStatus:%x rssi:%d QQ\r\n",
                    lqiStatus,lqi,rssiStatus,rssi);
  emberSerialWaitSend(APP_SERIAL);
}

//----------------------------------------------------------------
// Message-sending utilities.

// Send a broadcast using the give cluster ID and with the first 'length'
// bytes of 'contents' as a payload.

EmberStatus sendBroadcast(int8u clusterId, int8u *contents, int8u length, int8u radius)
{
  EmberMessageBuffer message = EMBER_NULL_MESSAGE_BUFFER;
  EmberApsFrame frame;
  EmberStatus status;
  
  // Allocate a buffer 
  message = emberAllocateStackBuffer();
  if (message == EMBER_NULL_MESSAGE_BUFFER) {
    return EMBER_NO_BUFFERS;
  }
  // Workaround for bug 7090 - the node does not get broadcast loopback
  // message if the payload has zero length.
  if(length == 0) {
   length = length + 1;
   contents = &length;
  }
  // Set message buffer length
  status = emberSetLinkedBuffersLength(message, length);
  if (status != EMBER_SUCCESS) {
    return status;
  }

  // Fill message buffer with data
  emberCopyToLinkedBuffers(contents, message, 0, length);

  frame.profileId = PROFILE_ID;
  frame.clusterId = clusterId;
  frame.sourceEndpoint = ENDPOINT;
  frame.destinationEndpoint = ENDPOINT;
  frame.options = 0;
  status = emberSendBroadcast(EMBER_BROADCAST_ADDRESS, &frame, radius, message);
  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL,"Error: send broadcast 0x%x\r\n", status);
    return status;
  }

  emberReleaseMessageBuffer(message);
  
  return status;
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
