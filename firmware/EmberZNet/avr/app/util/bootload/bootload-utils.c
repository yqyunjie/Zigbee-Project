// *******************************************************************
// * bootload-utils.c
// * 
// * Utilities used for bootloading.
// * See bootload-utils.h for more complete description
// *
// * Copyright 2006 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros

// Ember stack and related utilities
#include "stack/include/ember.h"         // Main stack definitions
#include "stack/include/packet-buffer.h" // Linked message buffers
#include "stack/include/error.h"         // Status codes

// HAL
#include "hal/hal.h" 
#include "hal/micro/xap2b/em250/em250-ebl.h"  // ebl file format
#include "hal/micro/xap2b/em250/flash.h"      // flash read mechanism
#if defined(XAP2B)
  #include "hal/micro/xap2b/em250/memmap.h"   // location of application
#endif

// Application utilities
#include "app/util/serial/serial.h"
#include "app/util/zigbee-framework/zigbee-device-library.h"
#include "app/util/bootload/bootload-utils-internal.h"  // internal declaration
#include "app/util/bootload/bootload-utils.h"           // public interface

// The variables below are used in bootloader-demo-v2.c, hence, need to be
// defined in case USE_BOOTLOADER_LIB is not defined.
// Making a handy broadcast EUI64 address
EmberEUI64 broadcastEui64 = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Initialize bootload state to normal (not currently participate in 
// bootloading)
bootloadState blState = BOOTLOAD_STATE_NORMAL;

#ifdef  USE_BOOTLOADER_LIB
// For now, lots of messages
#if defined(EMBER_APPLICATION_HAS_MAC_PASSTHROUGH_HANDLER) || defined(EMBER_APPLICATION_HAS_RAW_HANDLER)
  #define SBL_LIB_SRC_SUPPORT_V1
  #ifdef ENABLE_BOOTLOAD_PRINT
    #define DEBUG_V1_FULL
  #endif
#endif

#if !defined(XAP2B)
// Convert from (ebl) page to word address.
#define PAGE_TO_PROG_ADDR(x)    (((int16u)(x)) << 9)
#endif

// ***********************************************
// NOTE: applications that use the bootload utilities need to 
// define EMBER_APPLICATION_HAS_BOOTLOAD_HANDLERS
// within their CONFIGURATION_HEADER
// ***********************************************

// There are three main bootload modes (serial, clone and passthru) that 
// determines the responsibilities of the source node whether to bootload
// itself via serial (uart) interface, or to receive the image data via xmodem 
// from the host PC to bootload another node or to read the image data from 
// its own flash to bootload another node.
bootloadMode blMode = BOOTLOAD_MODE_NONE;

// bootload (xmodem) protocols variables
int8u currentRetriesRemaining;
int8u xmodemBlock[XMODEM_BLOCK_SIZE];
int8u bootloadBuffer[MAX_BOOTLOAD_MESSAGE_SIZE];
int8u expectedBlockNumber = 1;
int8u currentIndex = 0;
int32u flashStreamCrc = INITIAL_CRC;

// bootload clone variables
pageRange_t pageRanges[6];

// XModem protocol supports packet of 128 bytes in length, however, 
// we can only send it in BOOTLOAD_OTA_SIZE (64 bytes) at a time
// over the air (OTA).  Hence, XModem packets are divided into two
// over-the-air messages. Values of the over-the-air block number are 
// (2*expectedBlockNumber) and (2*expectedBlockNumber)-1
int8u currentOTABlock; 

// Eui64 address of the target node
EmberEUI64 targetAddress;

// To keep track of how much time we should wait.  A simple timeout mechanism
int8u actionTimer;

// Port for serial prints and bootloading
int8u appSerial = 0;
int8u bootloadSerial = 1;

// Generic global variables
EmberStatus emberStatus;
boolean booleanStatus;

// Authentication block contains necessary information to perform
// security measure before going into bootload mode
#pragma align authBlock
int8u authBlock[BOOTLOAD_AUTH_COMMON_SIZE];

// This message buffer stores the key while waiting for the challenge to be sent
// by the target node stack.
static EmberMessageBuffer signingKey = EMBER_NULL_MESSAGE_BUFFER;

// This message buffer stores the authentication challenge while waiting for
// the authentication response to come back from the requesting node.
static EmberMessageBuffer challenge = EMBER_NULL_MESSAGE_BUFFER;

#ifdef SBL_LIB_SRC_SUPPORT_V1
// v1 bootloader variables
static int8u mySeq = 0;                         // mac sequence number
static int8u bootV1State = BOOT_V1_STATE_IDLE;  // v1 bootloader state
static int8u bootV1Cnt = 0;                     // Retry count for V1
static int16u v1Time = 0;                       // Funky time
static int16u bootV1Time = 0;                   // Last used funky time
#endif // SBL_LIB_SRC_SUPPORT_V1

// Forward declaration of functions used internally by bootload 
// utilities library.  These functions are defined by bootload utility
// library.
static int8u isTheSameEui64(EmberEUI64 source, EmberEUI64 target);
static int8u bootloadMakeHeader(int8u *message, int8u type);
static void printEui(EmberEUI64 eui);
static void sendRadioMessage(int8u type, boolean isBroadcast);
static boolean isSleepy(EmberEUI64 eui);
#ifndef SBL_LIB_TARGET   // can be a source node
 static void xmodemReceiveAndForward(void);
 #ifndef SBL_LIB_SRC_NO_PASSTHRU // used in passthru mode
  static int8u xmodemReadSerialByte(int8u timeout, int8u *byte);
  static int8u xmodemReceiveBlock(void);
  static void xmodemSendReady(void);
 #endif
 #ifndef SBL_LIB_SRC_NO_CLONE // used in clone mode
  static void flashReadPageRange(void);
  static boolean flashStreamData(int16u *data, int8u len, int8u type);
  static boolean flashReadAndForwardBlock(int16u addr, int16u size);
  static boolean flashInsertAndForwardTag(int16u type, int16u length, int16u addr);
  static void flashComplete(void);
 #endif
 static void flashReadAndForward(void);
 static int32u calculateCrc32(int32u accumulator, int8u data);
 static boolean bootloadSendImage(int8u type);
#endif // SBL_LIB_TARGET


#define sendMACImageSegment() sendRadioMessage(XMODEM_SOH, FALSE)
#define sendMACCompleteSegment() sendRadioMessage(XMODEM_EOT, FALSE)

void emAesEncrypt(int8u block[16], int8u *key);


// ****************************************************************
// Public functions that can be called from the application to utilize
// bootload features.
// ****************************************************************

// -------------------------------------------------------------------------
// Initialize serial port
void bootloadUtilInit(int8u appPort, int8u bootloadPort)
{
  appSerial = appPort;
  bootloadSerial = bootloadPort;
  emberSerialInit(bootloadSerial, BOOTLOAD_BAUD_RATE, PARITY_NONE, 1);

  // tell the stack that we want the MAC to passthrough EmberNet packets
  // since these are the bootloader V1 packets we are interested in
  emberMacPassthroughFlags = emberMacPassthroughFlags 
                             | EMBER_MAC_PASSTHROUGH_EMBERNET;
}

#if (!defined(SBL_LIB_SRC_NO_CLONE) || !defined(SBL_LIB_SRC_NO_PASSTHRU))
// -------------------------------------------------------------------------
// Send bootload request to initiate bootload transaction with the target node.
EmberStatus bootloadUtilSendRequest(EmberEUI64 targetEui,
                                    int16u mfgId,
                                    int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                    int8u encryptKey[BOOTLOAD_AUTH_COMMON_SIZE],
                                    bootloadMode mode)
{
  int8u index;
  EmberMessageBuffer buffer = EMBER_NULL_MESSAGE_BUFFER;
  EmberStatus status;

  if (IS_BOOTLOADING) {
    // Another bootload launch request is in progress.
    return EMBER_ERR_FATAL;
  }

  blMode = mode;
  
  // Save the target eui for later use in bootloading (after launch).
  MEMCOPY(targetAddress, targetEui, EUI64_SIZE);
  
  index = bootloadMakeHeader(bootloadBuffer, XMODEM_LAUNCH_REQUEST);

  // create bootload launch request payload
  bootloadBuffer[index++] = LOW_BYTE(mfgId);
  bootloadBuffer[index++] = HIGH_BYTE(mfgId);
  MEMCOPY(&bootloadBuffer[index], hardwareTag, BOOTLOAD_HARDWARE_TAG_SIZE);
  index += BOOTLOAD_HARDWARE_TAG_SIZE;
  
  // Save the key to use later when sending the authentication response.
  signingKey = emberFillLinkedBuffers(encryptKey, BOOTLOAD_AUTH_COMMON_SIZE);

  if (EMBER_NULL_MESSAGE_BUFFER == signingKey) {
    // Not enough buffers were available for the authentication key.
    status = EMBER_NO_BUFFERS;
  }
  else {
    // copy the message into buffers
    buffer = emberFillLinkedBuffers(bootloadBuffer, index);

    // check to make sure a buffer is available
    if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
      bl_print("TX error: Out Of Buffers\r\n");
      status = EMBER_NO_BUFFERS;
    }
    else if ( emberSendBootloadMessage(FALSE, targetAddress, buffer)
            !=EMBER_SUCCESS ) {
      blState = BOOTLOAD_STATE_NORMAL;
      blMode = BOOTLOAD_MODE_NONE;
      status = EMBER_ERR_FATAL;
    } else {
      // message sent successfully.
      blState = BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE;
      actionTimer = TIMEOUT_AUTH_CHALLENGE;
      status = EMBER_SUCCESS;
    }
  }

  if (EMBER_SUCCESS != status) {
    // We failed somewhere above.  Release the key buffer if it was allocated.
    if (signingKey != EMBER_NULL_MESSAGE_BUFFER) {
      emberReleaseMessageBuffer(signingKey);
      signingKey = EMBER_NULL_MESSAGE_BUFFER;
    }
  }

  // Done with buffer.
  if (buffer != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(buffer);
  }
  
  return status;
}
#endif // (defined(SBL_LIB_SRC_NO_CLONE) && defined(SBL_LIB_SRC_NO_PASSTHRU))

// A utility function called after a node has received bootload launch request
// message.  The challenge message is part of the handshake process that needs
// to be completed before a node goes into bootloader mode.
static EmberStatus bootloadSendAuthChallenge(EmberEUI64 sourceEui) {
  int8u index, challengeStart;
  EmberMessageBuffer message = EMBER_NULL_MESSAGE_BUFFER;
  int32u macTimer;
  EmberStatus status;
  int16u blVersion; // if the version is invalid, the value is 0xFFFF

  index = bootloadMakeHeader(bootloadBuffer, XMODEM_AUTH_CHALLENGE);
  challengeStart = index;
  
  // The authentication challenge is the entire bootload payload.
  // 1st byte -- a version setting.
  bootloadBuffer[index++] = 0x01;
  // Send bootloader version here also.
  // {2:blVersion}
  blVersion = halGetStandaloneBootloaderVersion();
  bootloadBuffer[index++] = HIGH_BYTE(blVersion); // bootloader version
  bootloadBuffer[index++] = LOW_BYTE(blVersion);  // bootloader build
  // (1:plat)
  bootloadBuffer[index++] = PLAT;                 // Needed in case blVersion unknown
  // (1:micro)
  bootloadBuffer[index++] = MICRO;                // Needed in case blVersion unknown
  // (1:phy)
  bootloadBuffer[index++] = PHY;                  // Needed in case blVersion unknown
  // The second part of the challenge is the local eui.
  MEMCOPY(&bootloadBuffer[index], emLocalEui64, EUI64_SIZE);
  index += EUI64_SIZE;
  // The next part of the challenge is a snapshot of the mac timer.
  macTimer = halStackGetInt32uSymbolTick();
  // The em250's mac timer is 20 bits long.  Disregard the first byte (zero),
  // even for other devices that may have a 32-bit symbol timer.
  MEMCOPY(&bootloadBuffer[index], &(((int8u*)(&macTimer))[1]), 3);
  index += 3;
  // The rest of the challenge is padded with random data.
  while ( (index-challengeStart) < BOOTLOAD_AUTH_CHALLENGE_SIZE) {
    bootloadBuffer[index++] = halCommonGetRandom();
  }

  // Save the challenge so that the response can be verified when received.
  if (challenge != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(challenge);
    // would set challenge to null, but it is set immediately after.
    //challenge = EMBER_NULL_MESSAGE_BUFFER;
  }
  challenge = emberFillLinkedBuffers(&bootloadBuffer[challengeStart],
                                     BOOTLOAD_AUTH_CHALLENGE_SIZE);
  if (EMBER_NULL_MESSAGE_BUFFER == challenge) {
    // Not enough buffers were available for the challenge.
    status = EMBER_NO_BUFFERS;
  }
  else {  
    // Copy the message into buffers.
    message = emberFillLinkedBuffers(bootloadBuffer, index);
    if (EMBER_NULL_MESSAGE_BUFFER == message) {
      // Not enough buffers were available for the message.
      // Clean up the challenge.
      emberReleaseMessageBuffer(challenge);
      challenge = EMBER_NULL_MESSAGE_BUFFER;
      status = EMBER_NO_BUFFERS;
    }
    else {
      // Send the message.
      if ( emberSendBootloadMessage(FALSE, sourceEui, message)
         !=EMBER_SUCCESS ) {
        status = EMBER_ERR_FATAL;
        bl_print("Error: send auth challenge failed\r\n");
      } else {
        status = EMBER_SUCCESS;
      }
    }
  }  

  // Done with buffer.
  if (message != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(message);
  }
  
  return status;
}

// A utility function called after a node has received a challenge
// message.  After the target has received a valid response message, it will go
// into bootloader mode.
static EmberStatus bootloadSendAuthResponse(int8u* authenticationBlock,
                                     EmberEUI64 targetEui) {
  int8u index;
  EmberMessageBuffer message;
  EmberStatus status;

  index = bootloadMakeHeader(bootloadBuffer, XMODEM_AUTH_RESPONSE);
  
  // AES encrypt the challenge in place to yield the response.
  assert(signingKey != EMBER_NULL_MESSAGE_BUFFER);
  emAesEncrypt(authenticationBlock, emberMessageBufferContents(signingKey));
  
  // The authentication response is the entire bootload payload.
  MEMCOPY(&bootloadBuffer[index],authenticationBlock,
          BOOTLOAD_AUTH_RESPONSE_SIZE);
  index += BOOTLOAD_AUTH_RESPONSE_SIZE;

  // Copy the message into buffers.
  message = emberFillLinkedBuffers(bootloadBuffer, index);
  if (EMBER_NULL_MESSAGE_BUFFER == message) {
    // Not enough buffers were available for the message.
    // Clean up the challenge.
    status = EMBER_NO_BUFFERS;
  }
  else {
    // Send the message.
    if (emberSendBootloadMessage(FALSE, targetEui, message) != EMBER_SUCCESS) {
      status = EMBER_ERR_FATAL;
      bl_print("Error: send auth response failed\r\n");
    } else {
      // The challenge was sent successfully.
      status = EMBER_SUCCESS;
    }
  }

  // Done with buffer.
  if (message != EMBER_NULL_MESSAGE_BUFFER) {
    emberReleaseMessageBuffer(message);
  }

  emberReleaseMessageBuffer(signingKey);
  signingKey = EMBER_NULL_MESSAGE_BUFFER;
  
  return status;
}


// -------------------------------------------------------------------------
// Send bootload query.  The function is called by the source node to get
// necessary information about nodes.
void bootloadUtilSendQuery(EmberEUI64 target)
{
  MEMCOPY(targetAddress, target, EUI64_SIZE);
  if(isTheSameEui64(broadcastEui64, target)) {
    sendRadioMessage(XMODEM_QUERY, TRUE);
  } else {
    sendRadioMessage(XMODEM_QUERY, FALSE);
  }
  blState = BOOTLOAD_STATE_QUERY;
  actionTimer = TIMEOUT_QUERY;

#ifdef SBL_LIB_SRC_SUPPORT_V1
  // Send a v1 also
  sendV1ControlPacket(targetAddress, XMODEM_QUERY, 0xfe);
#endif // SBL_LIB_SRC_SUPPORT_V1
}


// -------------------------------------------------------------------------
// Initiate bootloading process with remote node that is already in bootload
// mode.  The function is normally used to perform bootload recovery on the 
// nodes that have failed to bootload.  If failure happens during bootload, 
// the nodes will be in bootload mode and stay on the current channel, if
// it has not been power cycled.
void bootloadUtilStartBootload(EmberEUI64 target, bootloadMode mode)
{

  currentRetriesRemaining = NUM_PKT_RETRIES;
  actionTimer = TIMEOUT_QUERY;

#ifdef SBL_LIB_SRC_SUPPORT_V1
  if ( mode == BOOTLOAD_MODE_V1_PASSTHRU )
  {
    if (bootV1State == BOOT_V1_STATE_IDLE)
    {
      bootV1State = BOOT_V1_STATE_QRESP;
      bootV1Time = v1Time;
      bootV1Cnt = 0;
      actionTimer = TIMEOUT_QUERY * 10;
    }
    blMode = BOOTLOAD_MODE_PASSTHRU;
  }
  else
#endif // SBL_LIB_SRC_SUPPORT_V1
    blMode = mode;

  MEMCOPY(targetAddress, target, EUI64_SIZE);
  if(isTheSameEui64(target, broadcastEui64)) {
    blState = BOOTLOAD_STATE_START_BROADCAST_BOOTLOAD;
    sendRadioMessage(XMODEM_QUERY, TRUE);
  } else {
    blState = BOOTLOAD_STATE_START_UNICAST_BOOTLOAD;
    sendRadioMessage(XMODEM_QUERY, FALSE);
  }
}

// -------------------------------------------------------------------------
// Send authentication response to the end device target.  Normally called
// by the parent as part of a process to bootload sleepy or mobile end device.
void bootloadUtilSendAuthResponse(EmberEUI64 target)
{
  // Encrypt the challenge and send the result back as the response.
  if (EMBER_SUCCESS == bootloadSendAuthResponse(authBlock, target) ) {
    // Give the target time to reboot into the bootloader,
    // then start the bootload process.
    blState = BOOTLOAD_STATE_DELAY_BEFORE_START;
    actionTimer = TIMEOUT_START;
  }
  else {
    bl_print("unable to send authentication response\r\n");
    // Get out from bootload mode
    blState = BOOTLOAD_STATE_NORMAL;
    blMode = BOOTLOAD_MODE_NONE;
  }
}

#ifdef SBL_LIB_SRC_SUPPORT_V1

// This value (2000) was gotten via testing data.  This
// is in funky time units.

#define V1_RETRY_TIME 2000

// This is run once each time around the main loop and is used
// to execute the commands scheduled by the user for v1 bootloading.

void bootloadV1Tick(void)
{
  int8u startByte;

  v1Time++;  // Some random time has passed.

  switch(bootV1State)
  {
  case BOOT_V1_STATE_QRESP: // waiting for QRESP
    if ((v1Time - bootV1Time) > V1_RETRY_TIME)
    {
      // Resend QUERY
      sendV1ControlPacket(targetAddress, XMODEM_QUERY, 0xff);
      bootV1Time = v1Time;
      bootV1Cnt++;
    }
    break;
  case BOOT_V1_STATE_XMODEM: // Waiting for xmodem data
    break;
  case BOOT_V1_STATE_DATA_ACK: // Waiting for data ACK
    if ((v1Time - bootV1Time) > V1_RETRY_TIME)
    {
      // Resend SOH (data block)
      if(currentOTABlock & 0x01) {
        startByte = 0;
      } else {
        startByte = 64;
      }
      sendV1DataPacket(targetAddress, currentOTABlock, &xmodemBlock[startByte]);
      bootV1Time = v1Time;
      bootV1Cnt++;
    }
    break;
  case BOOT_V1_STATE_EOT_ACK: // Waiting for EOT ACK
    if ((v1Time - bootV1Time) > V1_RETRY_TIME)
    {
      // Resend EOT
      sendV1ControlPacket(targetAddress, XMODEM_EOT, currentOTABlock);
      bootV1Time = v1Time;
      bootV1Cnt++;
    }
    break;
  case BOOT_V1_STATE_IDLE:
    break;
  }
  if (bootV1Cnt > 20)
  {
    bl_print("V1 bootload failed, state %d\r\n",
             bootV1State);
    bootV1State = BOOT_V1_STATE_IDLE;
    bootV1Cnt = 0;
    bootV1Time = v1Time;
    // Get out from bootload mode
    blState = BOOTLOAD_STATE_NORMAL;
    blMode = BOOTLOAD_MODE_NONE;
  }
}
#endif // SBL_LIB_SRC_SUPPORT_V1

// -------------------------------------------------------------------------
// Bootload State Machine
void bootloadUtilTick(void)
{
  static int16u lastBlinkTime = 0;
  int16u time;

  time = halCommonGetInt16uMillisecondTick();

  if ( time - lastBlinkTime > 200 /*ms*/ ) {
    lastBlinkTime = time;
        
    if (actionTimer == 0) {
      switch (blState)
      {  
        case BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE:
          // The authentication response was not received in time.
          // We need to free the challenge buffer.
          bl_print("do not receive auth response\r\n");
          assert(challenge != EMBER_NULL_MESSAGE_BUFFER);
          emberReleaseMessageBuffer(challenge);
          challenge = EMBER_NULL_MESSAGE_BUFFER;
          // Abort this bootloader session.
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
          break;
#ifndef SBL_LIB_TARGET
        case BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE:
          // The authentication challenge was not received in time.
          // We need to destroy the AES key.
          bl_print("do not receive auth challenge\r\n");
          assert(signingKey != EMBER_NULL_MESSAGE_BUFFER);
          emberReleaseMessageBuffer(signingKey);
          signingKey = EMBER_NULL_MESSAGE_BUFFER;
          // Abort this bootloader session.
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
          break;
        case BOOTLOAD_STATE_DELAY_BEFORE_START:
          // We have given the target node enough time to launch the bootloader.
          // Now we start the actual bootloader procedure.
          bl_print("start bootload\r\n");
          bl_print("free buffer: %x\r\n", emPacketBufferFreeCount);
          currentRetriesRemaining = NUM_PKT_RETRIES;
          bootloadUtilStartBootload(targetAddress, blMode);
          break;
        case BOOTLOAD_STATE_QUERY:
          // Stop receiving query response
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
#ifdef SBL_LIB_SRC_SUPPORT_V1
          bootV1State = BOOT_V1_STATE_IDLE;
          bootV1Cnt = 0;
          bootV1Time = v1Time;
#endif // SBL_LIB_SRC_SUPPORT_V1
          break;
        case BOOTLOAD_STATE_START_UNICAST_BOOTLOAD:
        case BOOTLOAD_STATE_START_BROADCAST_BOOTLOAD:
          if (currentRetriesRemaining > 0) {
            bl_print("--> start bootload retry\r\n");
            currentRetriesRemaining -= 1;
            if(blState == BOOTLOAD_STATE_START_UNICAST_BOOTLOAD) {
              sendRadioMessage(XMODEM_QUERY, FALSE);
            } else {
              sendRadioMessage(XMODEM_QUERY, TRUE);
            }
            actionTimer = TIMEOUT_QUERY;
          } else {
            blState = BOOTLOAD_STATE_NORMAL;
            bl_print("ERROR: do not see start bootload response\r\n");
          }
          break;
        case BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK:
        case BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK:
          // We didn't get an ack.  Retry (at the application level) in
          // the hopes that it was a temporary problem.
          if (currentRetriesRemaining > 0) {
            bl_print("data retry %x\r\n", currentRetriesRemaining);
            currentRetriesRemaining -= 1;
            actionTimer = TIMEOUT_IMAGE_SEND;
            if(blState == BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK) {
              sendMACImageSegment();
            } else {
              sendMACCompleteSegment();
            }
          } else {
            // If the source node doesn't get the ack from the target node in
            // respond to its EOT packet, the source node will retry for
            // NUM_PKT_RETRIES times.  If all retries fail, it will still 
            // consider the bootloading to be completed and ask the user to go
            // check the target node to verify that the application is loaded
            // ok.  
            if(blState == BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK) {
              bl_print("Send end of transmissiton to target node\r\n");
              bl_print("Please check target node if the app is loaded successfully\r\n");
              blState = BOOTLOAD_STATE_DONE;
            } else {
              blState = BOOTLOAD_STATE_NORMAL;
            }
          }
          break;
#endif // SBL_LIB_TARGET
        case BOOTLOAD_STATE_NORMAL:
          if(blMode != BOOTLOAD_MODE_NONE) {
            bl_print("ERROR: do not get response from target node\r\n");
            blMode = BOOTLOAD_MODE_NONE;
          }
        default:
          break;
        }
      } else {
        actionTimer -= 1;
      }
  } //end check time

#ifdef SBL_LIB_SRC_SUPPORT_V1
  bootloadV1Tick();
#endif // SBL_LIB_SRC_SUPPORT_V1
}

// ****************************************************************
// Handlers and function callback
// ****************************************************************

void emberIncomingBootloadMessageHandler(EmberEUI64 longId, 
                                         EmberMessageBuffer message)
{
  int8u type;
  int16u mfgId;  
#ifndef SBL_LIB_TARGET
  int8u block;
  int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE];
  int8s rssi;
  int8u length;
  int16u blVersion;
#endif // SBL_LIB_TARGET
  
  type = emberGetLinkedBuffersByte(message, OFFSET_MESSAGE_TYPE);
  switch(type) {
    case XMODEM_LAUNCH_REQUEST:
      {
        int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE];
        // Extract the manufacturer Id (transmitted low byte first).
        mfgId = HIGH_LOW_TO_INT(emberGetLinkedBuffersByte(message,OFFSET_MFG_ID+1),
                                emberGetLinkedBuffersByte(message,OFFSET_MFG_ID));
        // Extract the hardware tag.
        emberCopyFromLinkedBuffers(message,
                                   OFFSET_HARDWARE_TAG,
                                   hardwareTag,
                                   BOOTLOAD_HARDWARE_TAG_SIZE);
        if (bootloadUtilLaunchRequestHandler(mfgId,hardwareTag,longId)) {
          // The application has agreed to launch the bootloader.
          // Authenticate the requesting node before launching the bootloader.
          if (EMBER_SUCCESS == bootloadSendAuthChallenge(longId) ) {
            // The challenge was sent successfully.
            blState = BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE;
            actionTimer = TIMEOUT_AUTH_RESPONSE;
          }
          else {
            bl_print("unable to send authentication challenge\r\n");
            // Abort bootload session.
            blState = BOOTLOAD_STATE_NORMAL;
            blMode = BOOTLOAD_MODE_NONE;
          }
        }
        else {
          bl_print("application refused to launch bootloader\r\n");
          // Abort bootload session.
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
        }
      }
      break;

    case XMODEM_AUTH_RESPONSE:
      if (BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE == blState) {
        int8u response[BOOTLOAD_AUTH_RESPONSE_SIZE];
        int8u *challengePtr;
        tokTypeMfgBootloadAesKey verificationKey;

        // Extract the response.
        emberCopyFromLinkedBuffers(message,
                                   OFFSET_AUTH_RESPONSE,
                                   response,
                                   BOOTLOAD_AUTH_RESPONSE_SIZE);

        // Load the secret key to verify the response.
        halCommonGetToken(&verificationKey, TOKEN_MFG_BOOTLOAD_AES_KEY);

        // Encrypt the challenge.
        assert(challenge != EMBER_NULL_MESSAGE_BUFFER);
        challengePtr = emberMessageBufferContents(challenge);
        emAesEncrypt(challengePtr, (int8u*)verificationKey);
        
        // Compare the response to the encrypted challenge.
        if (!MEMCOMPARE(response,challengePtr,BOOTLOAD_AUTH_COMMON_SIZE) ) {
          // The response and the encrypted challenge match.
          // Launch the bootloader.
          // Would release the challenge buffer, but we are about to reboot.
          // emberReleaseMessageBuffer(challenge);
          // challenge = EMBER_NULL_MESSAGE_BUFFER;
          bl_print("Handshake Complete\r\n");
          halLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
        }
        else {
          // The response and the encrypted challenge do not match.
          bl_print("authentication failed.\r\n");
          // Get out from bootload mode
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
          emberReleaseMessageBuffer(challenge);
          challenge = EMBER_NULL_MESSAGE_BUFFER;
        }
      }
      break;
    case XMODEM_QUERY:
        // Record the address of the node that sent this query message.
        // Note that the targetAddress variable is used to store the destination
        // address in this case, not the address of the actual bootload
        // target node.
        if (!IS_BOOTLOADING) {
          MEMCOPY(targetAddress, longId, EUI64_SIZE);
          sendRadioMessage(XMODEM_QRESP, FALSE);
          bl_print("rx neighbor query\r\n");
        }
      break;

#ifndef SBL_LIB_TARGET	  
    case XMODEM_AUTH_CHALLENGE:
      if (BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE == blState) {
        // Extract the challenge.
        emberCopyFromLinkedBuffers(message,
                                   OFFSET_AUTH_CHALLENGE,
                                   authBlock,
                                   BOOTLOAD_AUTH_CHALLENGE_SIZE);

#ifdef SBL_LIB_SRC_SUPPORT_V1
        if ( (blMode == BOOTLOAD_MODE_PASSTHRU)
             && (authBlock[0] == 0x01) 
             && isTheSameEui64(targetAddress, &authBlock[6]) )
        {
          // If reported blVersion is V1, use V1
          if (authBlock[1] < 0x20)
            blMode = BOOTLOAD_MODE_V1_PASSTHRU;
          // If reported blVersion is unknown and a 2420, use V1
          if ((authBlock[1] == 0xff) && (authBlock[5] == 0x01))
            blMode = BOOTLOAD_MODE_V1_PASSTHRU;
        }
#endif // SBL_LIB_SRC_SUPPORT_V1
          
#ifdef DEBUG_V1_FULL
        bl_print("authBlock: ver:%x blV:%x blB:%x plat:%x micro:%x phy:%x\r\n  eui:",
                 authBlock[0], authBlock[1], authBlock[2], authBlock[3],
                 authBlock[4], authBlock[5]);
        printEui(&authBlock[6]);
        bl_print(" blMode=%d\r\n", blMode);
#endif // DEBUG_V1_FULL
  
        // If dealing with non-sleepy node (ex. router), then the library
        // will send authentication response message directly.
        if(!isSleepy(longId)) {
          // Encrypt the challenge and send the result back as the response.
          if (EMBER_SUCCESS == bootloadSendAuthResponse(authBlock, longId) ) {
            // Give the target time to reboot into the bootloader,
            // then start the bootload process.
            blState = BOOTLOAD_STATE_DELAY_BEFORE_START;
            actionTimer = TIMEOUT_START;
          }
          else {
            bl_print("unable to send authentication response\r\n");
            // Get out from bootload mode
            blState = BOOTLOAD_STATE_NORMAL;
            blMode = BOOTLOAD_MODE_NONE;
          }
        } else {
          // If dealing with end device node that can go to sleep during
          // the bootload security handshake process, then we want the
          // application to send authentication response message as JIT
          // message to guarantee that the end device target will get the 
          // message.

          // Give the target time to reboot into the bootloader,
          // then start the bootload process.
          blState = BOOTLOAD_STATE_DELAY_BEFORE_START;
          actionTimer = TIMEOUT_START;
        }
      }
      break;

    case XMODEM_QRESP:
      // only when we start bootload process
      debug("blState 0x%x\r\n", blState);
      if((blState == BOOTLOAD_STATE_START_UNICAST_BOOTLOAD) || 
          (blState == BOOTLOAD_STATE_START_BROADCAST_BOOTLOAD)) {
        if(blState == BOOTLOAD_STATE_START_UNICAST_BOOTLOAD) {
          // Check if it's the address of the target node that
          // we intend to bootload
          if(!isTheSameEui64(targetAddress, longId)) {
            bl_print("bootload message from unknown node\r\n");
            // Get out from bootload mode
            blState = BOOTLOAD_STATE_NORMAL;
            blMode = BOOTLOAD_MODE_NONE;
            return; 
          }
        } else { 
          // save the address of the node to be bootloaded
          MEMCOPY(targetAddress, longId, EUI64_SIZE);
        }
        // Check if protocol version is valid
        if ((emberGetLinkedBuffersByte(message, OFFSET_VERSION) 
          != BOOTLOAD_PROTOCOL_VERSION))
         {
          bl_print("RX bad packet, invalid protocl version\r\n");
          return;
        }
        
        // Check if the remote node is in bootload mode
        if (!emberGetLinkedBuffersByte(message, QRESP_OFFSET_BL_ACTIVE)) {
          bl_print("Error: remote node not in bootload mode\r\n");
          return;
        }
        bl_print("RX QUERY RESP from ");
        printEui(longId);
        if(emberGetLastHopRssi(&rssi) == EMBER_SUCCESS) {
          bl_print(" with rssiValue %d\r\n", rssi);
        } else {
          bl_print(" with rssiValue unknown\r\n", rssi);
        }
        // Initiate xmodem transfer with the host PC for passthru or start
        // reading flash for clone bootloading
        if(blMode == BOOTLOAD_MODE_PASSTHRU) {
          blState = BOOTLOAD_STATE_SENDING_IMAGE;
          emberSerialPrintf(appSerial, 
            "Please start .ebl upload image ...\r\n");
          xmodemReceiveAndForward();
        } else if(blMode == BOOTLOAD_MODE_CLONE) {
          // Check for platform, micro and phy values
          if((emberGetLinkedBuffersByte(message, QRESP_OFFSET_PLATFORM) != PLAT)
              || (emberGetLinkedBuffersByte(message, QRESP_OFFSET_MICRO) != MICRO)
              || (emberGetLinkedBuffersByte(message, QRESP_OFFSET_PHY) != PHY))
          {
            bl_print("Error: remote node does not have the same platform\r\n");
            return;
          }
          blState = BOOTLOAD_STATE_SENDING_IMAGE;
          emberSerialPrintf(appSerial, 
            "Start cloning ...\r\n");
          bl_print("free buffer: %x\r\n", emPacketBufferFreeCount);
          flashReadAndForward();
        }
      } else if(blState == BOOTLOAD_STATE_QUERY) {
        // Extract manufacturer id and hardware tag   
        mfgId = HIGH_LOW_TO_INT(
                  emberGetLinkedBuffersByte(message,QRESP_OFFSET_MFG_ID+1),
                  emberGetLinkedBuffersByte(message,QRESP_OFFSET_MFG_ID));
        emberCopyFromLinkedBuffers(message,
                                   QRESP_OFFSET_HARDWARE_TAG,
                                   hardwareTag,
                                   BOOTLOAD_HARDWARE_TAG_SIZE);
        
        length = emberMessageBufferLength(message);
        if (length >= (QRESP_OFFSET_BL_VERSION + 1)) {
          blVersion = HIGH_LOW_TO_INT(
                  emberGetLinkedBuffersByte(message,QRESP_OFFSET_BL_VERSION),
                  emberGetLinkedBuffersByte(message,QRESP_OFFSET_BL_VERSION+1));
        } else {
          blVersion = 0xFFFF;  // if the version is invalid, the value is 0xFFFF
        }
        // This is a result of issuing 'query_neighbor' command.
        // Notify the application of the query response received so the 
        // application can make decision on whether or who to bootload.
        bootloadUtilQueryResponseHandler(
          emberGetLinkedBuffersByte(message,QRESP_OFFSET_BL_ACTIVE),
          mfgId,         
          hardwareTag,
          longId, // eui of the node that sent it.
          emberGetLinkedBuffersByte(message,QRESP_OFFSET_BL_CAPS),
          emberGetLinkedBuffersByte(message,QRESP_OFFSET_PLATFORM),
          emberGetLinkedBuffersByte(message,QRESP_OFFSET_MICRO),
          emberGetLinkedBuffersByte(message,QRESP_OFFSET_PHY),
          blVersion);
      }
      // else ignore the query response
      else
        bl_print("Ignored QRESP, blState 0x%x\r\n", blState);
      break;
    case XMODEM_ACK:
      // Check if it's the expected block number.  If so, continue with the
      // image sending.  If not, ignore the duplicate.
      block = emberGetLinkedBuffersByte(message, OFFSET_BLOCK_NUMBER);
      if(blState == BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK) {
        if(block == currentOTABlock) {
          blState = BOOTLOAD_STATE_SENDING_IMAGE;
        }
      } else if(blState == BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK) {
         if(block == currentOTABlock) {
           blState = BOOTLOAD_STATE_DONE;
         }
         bl_print("RX EOT ACK seq %x, expected seq %x\r\n", 
                    block, currentOTABlock);
      }
      break;
    case XMODEM_NAK:
      block = emberGetLinkedBuffersByte(message, OFFSET_BLOCK_NUMBER);
      // This allows unicast recovery of node.  If the target node is in 
      // default recovery mode, it first needs to hear a broadcast query 
      // packet from the source node (ex. using "query_neighbor" cmd) in order
      // to extract source node's eui64 and pan id.  Then the source node
      // can send unicast recovery packet to recover the target node.  The
      // target node would actually send a nack to the (redundant) query 
      // packet which the source will ignore if the block number is one which
      // indicates that the taget node is expecting data packet with block
      // number one.
      if(blState == BOOTLOAD_STATE_START_UNICAST_BOOTLOAD &&
         block == 1) {
        // Check if it's the address of the target node that
        // we intend to bootload
        if(!isTheSameEui64(targetAddress, longId)) {
          bl_print("bootload message from unknown node\r\n");
          // Get out from bootload mode
          blState = BOOTLOAD_STATE_NORMAL;
          blMode = BOOTLOAD_MODE_NONE;
          return; 
        }
        
        // Check if protocol version is valid
        if ((emberGetLinkedBuffersByte(message, OFFSET_VERSION) 
          != BOOTLOAD_PROTOCOL_VERSION))
         {
          bl_print("RX bad packet, invalid protocl version\r\n");
          return;
        }
        
        bl_print("RX QUERY RESP from ");
        printEui(longId);
        if(emberGetLastHopRssi(&rssi) == EMBER_SUCCESS) {
          bl_print(" with rssiValue %d\r\n", rssi);
        } else {
          bl_print(" with rssiValue unknown\r\n", rssi);
        }
        // Initiate xmodem transfer with the host PC for passthru or start
        // reading flash for clone bootloading
        blState = BOOTLOAD_STATE_SENDING_IMAGE;
        if(blMode == BOOTLOAD_MODE_PASSTHRU) {
          emberSerialPrintf(appSerial, 
            "Please start .ebl upload image ...\r\n");
          xmodemReceiveAndForward();
        } else if(blMode == BOOTLOAD_MODE_CLONE) {
          emberSerialPrintf(appSerial, 
            "Start cloning ...\r\n");
          bl_print("free buffer: %x\r\n", emPacketBufferFreeCount);
          flashReadAndForward();
        }
      } else {
        // In case of normal NAK to data packet, we stay in the same state and 
        // wait for next retry
        debug("RX NAK block %x\r\n", block);
      }
      break;
#endif // SBL_LIB_TARGET	  

    default:
      break;
  }

}

void emberBootloadTransmitCompleteHandler(EmberMessageBuffer message,
                                           EmberStatus status)
{
#ifdef ENABLE_DEBUG
  int8u type;
  type = emberGetLinkedBuffersByte(message, OFFSET_MESSAGE_TYPE);
  if(status == EMBER_SUCCESS) {
    emberSerialPrintf(appSerial, 
      "message 0x%x is sent successfully\r\n", type);
  } else {
    emberSerialPrintf(appSerial, 
      "message 0x%x is failed to send\r\n", type);
  }
#endif
}

#ifdef SBL_LIB_SRC_SUPPORT_V1
void emberRawTransmitCompleteHandler(EmberMessageBuffer message,
                                     EmberStatus status)
{
#if defined(DEBUG_V1_FULL) && defined(ENABLE_DEBUG)
  int8u type;

  type = emberGetLinkedBuffersByte(message, RAW_OFFSET_MESSAGE_TYPE);
  if(status == EMBER_SUCCESS) {
    emberSerialPrintf(appSerial, 
      "raw message 0x%x is sent successfully\r\n", type);
  } else {
    emberSerialPrintf(appSerial, 
      "raw message 0x%x is failed to send 0x%x\r\n", type, status);
  }
#endif // SBL_LIB_SRC_SUPPORT_V1
}
#endif

// ****************************************************************
// Utilities Functions
// ****************************************************************

static int8u isTheSameEui64(EmberEUI64 source, EmberEUI64 target)
{
  return !(MEMCOMPARE(source, target, EUI64_SIZE)); 
}

// This is called to create generic bootload headers for
// various message types.  When finished, the function returns the
// offset value after the header.
static int8u bootloadMakeHeader(int8u *message, int8u type)
{
  //common header values
  message[0] = BOOTLOAD_PROTOCOL_VERSION;
  message[1] = type;

  // for XMODEM_QUERY and XMODEM_EOT messages, this represents the end of the
  // header.  However, for XMODEM_QRESP, XMODEM_SOH, XMODEM_ACK, XMODEM_NAK
  // messages, there are additional values that need to be added.
  // Note that the application will not have to handle creation of 
  // over the air XMODEM_ACK and XMODEM_NAK since these are all handled by 
  // the bootloader on the target node.

  return 2;
}

#ifdef SBL_LIB_SRC_SUPPORT_V1
// This is called to create generic v1 bootload headers for
// various message types.
static void v1bootloadMakeHeader(int8u *rawMessage, int8u size, int8u type)
{
  //common header values

  rawMessage[0] = 0x41; // MAC_FRAME_TYPE_DATA | MAC_FRAME_FLAG_INTRA_PAN
  rawMessage[1] = 0x88; // MAC_FRAME_DESTINATION_MODE_SHORT | MAC_FRAME_SOURCE_MODE_SHORT
  rawMessage[2] = mySeq++; // Sequence number
  rawMessage[3] = 0xFF; // dest pan id (we specify broadcast)
  rawMessage[4] = 0xFF; // ..
  rawMessage[5] = 0xFF; // dest addr (we specify broadcast)
  rawMessage[6] = 0xFF; // ..
  rawMessage[7] = 0x00; // src addr (we say 0 to match bootloader!)
  rawMessage[8] = 0x00; // ..
  rawMessage[RAW_OFFSET_SEG_BOOTLOAD] = V1_SEG_BOOTLOAD; // SEG_BOOTLOAD
  rawMessage[RAW_OFFSET_SIZE] = size;
  rawMessage[RAW_OFFSET_MESSAGE_TYPE] = type;
}

// This routine is used to talk to v1 standalone 2420 bootloaders.
void sendV1DataPacket(EmberEUI64 toEui, int8u expectedBlock, int8u data[64])
{
  EmberMessageBuffer message;
  EmberStatus status;
  int8u rawMessage[V1_RAW_MAC_OVERHEAD_SIZE + 2 + V1_DATA_PACKET_SIZE];

  v1bootloadMakeHeader(rawMessage, V1_DATA_PACKET_SIZE, XMODEM_SOH); // Type (SOH '\1'
  MEMCOPY(&rawMessage[RAW_OFFSET_DEST_EUI], toEui, EUI64_SIZE); // dest EUI
  rawMessage[20] = expectedBlock;
  rawMessage[21] = 0xff - expectedBlock;
  MEMCOPY(&rawMessage[22], data, 64);

  // v1 has no CRC bytes!

  // Allocate buffer big enough to hold the message (length)
  message = emberFillLinkedBuffers(rawMessage, sizeof(rawMessage));
  if (message == EMBER_NULL_MESSAGE_BUFFER)
  {
    bl_print("v1dp: Failed to get buffer!\r\n");
    return;
  }

  status = emberSendRawMessage(message);

  emberReleaseMessageBuffer(message);

  if (status != EMBER_SUCCESS)
  {
    bl_print("Failed to send v1dp(%x)(%x): %x\r\n",
             expectedBlock, mySeq - 1, status);
  } else {
    bl_print("Send v1dp(%x)(%x)\r\n",
             expectedBlock, mySeq - 1);
  }
}

// This command is used to talk to v1 standalone 2420 bootloaders.
void sendV1ControlPacket(EmberEUI64 toEui, int8u type, int8u sequenceNumber)
{
  EmberMessageBuffer message;
  EmberStatus status;
  int16u srcNodeId = emberGetNodeId();
  int8u rawMessage[V1_RAW_MAC_OVERHEAD_SIZE + 2 + V1_CONTROL_PACKET_SIZE];

  v1bootloadMakeHeader(rawMessage, V1_CONTROL_PACKET_SIZE, type);
  MEMCOPY(&rawMessage[RAW_OFFSET_DEST_EUI], toEui, EUI64_SIZE); // dest EUI
  MEMCOPY(&rawMessage[RAW_OFFSET_SRC_EUI], emberGetEui64(), EUI64_SIZE); // src EUI
  rawMessage[RAW_OFFSET_SEQ_NUMBER] = sequenceNumber;
  rawMessage[RAW_OFFSET_SRC_NODEID] = LOW_BYTE(srcNodeId);
  rawMessage[RAW_OFFSET_SRC_NODEID+1] = HIGH_BYTE(srcNodeId);

  // Allocate buffer big enough to hold the message (length)
  message = emberFillLinkedBuffers(rawMessage, sizeof(rawMessage));
  if (message == EMBER_NULL_MESSAGE_BUFFER)
  {
    bl_print("v1cp: Failed to get buffer!\r\n");
    return;
  }

  status = emberSendRawMessage(message);

  emberReleaseMessageBuffer(message);

  if (status != EMBER_SUCCESS)
  {
    bl_print("Failed to send v1cp(%x,%x)(%x): %x\r\n",
             type, sequenceNumber, mySeq - 1, status);
  } else {
    bl_print("Send v1cp(%x,%x)(%x)\r\n",
             type, sequenceNumber, mySeq - 1);
  }
}

// 15.4 Definitons
#define MAC_FRAME_FLAG_SECURITY_ENABLED       BIT(3)
#define MAC_HEADER_FRAME_CONTROL_LOW_OFFSET   0
#define MAC_HEADER_FRAME_CONTROL_HIGH_OFFSET  1
#define MAC_HEADER_SOURCE_ADDRESS_LOW_OFFSET  7
#define MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET 8

#define V1_KEY_DATA_SIZE 22

// Called when a v1 bootload message arrives.


void emberMacPassthroughMessageHandler(EmberMacPassthroughType messageType,
                                       EmberMessageBuffer message)
{
  int8u length, messageLength;
  int8u v1_header_buffer[V1_KEY_DATA_SIZE];
#ifndef SBL_LIB_TARGET	  
  int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE];
#endif // SBL_LIB_TARGET	  
  int8u *macHeader;
  int16u frameControl, sourceAddress;
  int8u payloadOffset = 0;

  length = emberMessageBufferLength(message);
  if(length < (payloadOffset + MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET + 1)) {
#ifdef DEBUG_V1_FULL
    bl_print("ILM: bad length %d < %d (%x)\r\n", length,
             payloadOffset + MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET + 1,
             payloadOffset);
#endif
    // invalid length for this application
    return; 
  }
  messageLength =  length - (payloadOffset + MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET + 1);

  macHeader = emberLinkedBufferContents(message);

  frameControl = HIGH_LOW_TO_INT(
    macHeader[payloadOffset+ MAC_HEADER_FRAME_CONTROL_HIGH_OFFSET],
    macHeader[payloadOffset + MAC_HEADER_FRAME_CONTROL_LOW_OFFSET]);
  sourceAddress = HIGH_LOW_TO_INT(
    macHeader[payloadOffset + MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET],
    macHeader[payloadOffset + MAC_HEADER_SOURCE_ADDRESS_LOW_OFFSET]);

  // emberMacPassthroughMessageHandler passes several types of legacy packets.
  if ( (frameControl & MAC_FRAME_FLAG_SECURITY_ENABLED)
     ||(sourceAddress != 0) ) {
    // This packet is not a v1 bootloader message.
#ifdef DEBUG_V1_FULL
    bl_print("ILM: not bootload fc=0x%2x sa=0x%2x\r\n",
             frameControl, sourceAddress);
#endif
    return;
  }

  // Copy from the buffer to allow for more direct access to the message.
  emberCopyFromLinkedBuffers(
    message,
    payloadOffset + MAC_HEADER_SOURCE_ADDRESS_HIGH_OFFSET + 1,       // skip Raw overhead
    v1_header_buffer,
    (messageLength <= V1_KEY_DATA_SIZE
     ? messageLength
     : V1_KEY_DATA_SIZE));

  if (v1_header_buffer[0] != V1_SEG_BOOTLOAD)
  {
#ifdef DEBUG_V1_FULL
    bl_print("QV1 not bootload: %x vs %x\r\n",
             v1_header_buffer[0], V1_SEG_BOOTLOAD);
#endif
  } else {
#ifdef DEBUG_V1_FULL
    bl_print("QV1 bootload: Type:%x at:%d from:%d (%d)\r\n  ",
             v1_header_buffer[2], v1Time, bootV1Time,
             v1Time - bootV1Time);
    if (v1_header_buffer[1] != (messageLength - 2))
    {
      bl_print("  !!! bad len:%x vs %x!!!\r\n  ",
               v1_header_buffer[1], messageLength - 2);
    }
#endif
    switch(v1_header_buffer[2]) {
    case XMODEM_QUERY:
#ifdef DEBUG_V1_FULL
      bl_print("Q: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" <= ");
      printEui(&v1_header_buffer[11]);
      bl_print("\r\n");
#endif
      break;
#ifndef SBL_LIB_TARGET	  
    case XMODEM_QRESP:
#ifdef DEBUG_V1_FULL
      bl_print("QR: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" <= ");
      printEui(&v1_header_buffer[11]);
      bl_print(" seq:%x ni:%x%x\r\n",
               v1_header_buffer[19],
               v1_header_buffer[21], v1_header_buffer[20]);
#endif
      if (bootV1State == BOOT_V1_STATE_QRESP)
      {
        MEMCOPY(targetAddress, &v1_header_buffer[11], EUI64_SIZE);
        bootV1State = BOOT_V1_STATE_XMODEM;
        bootV1Time = v1Time;
        bootV1Cnt = 0;
        blState = BOOTLOAD_STATE_SENDING_IMAGE;
        emberSerialPrintf(appSerial, 
                          "Please start .bin upload image ...\r\n");
        xmodemReceiveAndForward();
      } else if (blState == BOOTLOAD_STATE_QUERY) {
        MEMSET(hardwareTag, 0xFF, sizeof(hardwareTag));
        // This is a result of issuing 'query_neighbor' command.
        // Notify the application of the query response received so the 
        // application can make decision on whether or who to bootload.
        bootloadUtilQueryResponseHandler(
          0x01,                  // blActive
          0xFFFF,                // mfgId
          hardwareTag,           // hardwareTag
          &v1_header_buffer[11], // eui of the node that sent it.
          0x00,                  // blCap
          0x01,                  // plat
          0x02,                  // micro
          0x01,                  // phy
          0x1000);
      }
      // else ignore the query response
      else
        bl_print("Ignored V1 QRESP, blState 0x%x\r\n", blState);
      break;
    case XMODEM_SOH:
#ifdef DEBUG_V1_FULL
      bl_print("SOH: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" seq:%x nseq:%x\r\n",
               v1_header_buffer[11],v1_header_buffer[12]);
#endif
      break;
    case XMODEM_ACK:
#ifdef DEBUG_V1_FULL
      bl_print("ACK: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" <= ");
      printEui(&v1_header_buffer[11]);
      bl_print(" seq:%x ni:%x%x\r\n",
               v1_header_buffer[19],
               v1_header_buffer[21], v1_header_buffer[20]);
#endif
      if (bootV1State != BOOT_V1_STATE_IDLE)
      {
        // Check if it is from the right node
        if(isTheSameEui64(&v1_header_buffer[11], targetAddress))
        {
          // Check if it's the expected block number.  If so,
          // continue with the image sending.  If not, ignore the
          // duplicate.
          if(blState == BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK) {
            if(v1_header_buffer[19] == currentOTABlock) {
              blState = BOOTLOAD_STATE_SENDING_IMAGE;
              bootV1Cnt = 0;
              bootV1Time = v1Time;
            }
          } else if(blState == BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK) {
            if(v1_header_buffer[19] == currentOTABlock) {
              blState = BOOTLOAD_STATE_DONE;
              bootV1State = BOOT_V1_STATE_IDLE;
              bootV1Cnt = 0;
              bootV1Time = v1Time;
            }
            bl_print("RX EOT ACK seq %x, expected seq %x\r\n", 
                     v1_header_buffer[19], currentOTABlock);
          }
        }
        else
          bl_print("RX ack from unexpected node!\r\n");
      }
      break;
    case XMODEM_NAK:
#ifdef DEBUG_V1_FULL
      bl_print("NAK: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" <= ");
      printEui(&v1_header_buffer[11]);
      bl_print(" seq:%x ni:%x%x\r\n",
               v1_header_buffer[19],
               v1_header_buffer[21], v1_header_buffer[20]);
#endif
      if (bootV1State != BOOT_V1_STATE_IDLE)
      {
        // Check if it is from the right node
        if(isTheSameEui64(&v1_header_buffer[11], targetAddress))
        {
          // In case of normal NAK to data packet, we stay in the
          // same state and wait for next retry
          debug("RX NAK block %x\r\n", v1_header_buffer[19]);
          bootV1Time = v1Time;
        }
        else
          bl_print("RX nak from unexpected node!\r\n");
      }
      break;
    case XMODEM_EOT:
#ifdef DEBUG_V1_FULL
      bl_print("EOT: ");
      printEui(&v1_header_buffer[3]);
      bl_print(" <= ");
      printEui(&v1_header_buffer[3]);
      bl_print(" seq:%x ni:%x%x\r\n",
               v1_header_buffer[19],
               v1_header_buffer[21], v1_header_buffer[20]);
#endif
      break;
#endif // SBL_LIB_TARGET	  

    default:
      bl_print("unknown.\r\n");
      break;
    }    
    if (bootV1State == BOOT_V1_STATE_IDLE)
      bootV1Time = v1Time;
  }
}
#endif // SBL_LIB_SRC_SUPPORT_V1


#ifndef SBL_LIB_TARGET	  
// This function is used as a port of XModem communication.  It reads
// bootload serial port (uart port 1) one byte at a time and then
// returns appropriate status.
static int8u xmodemReadSerialByte(int8u timeout, int8u *byte)
{
  int16u time = halCommonGetInt16uMillisecondTick();

  while (timeout > 0) {
    halResetWatchdog();
    emberSerialBufferTick();
    if (emberSerialReadAvailable(bootloadSerial) > 0) {
      if(emberSerialReadByte(bootloadSerial, byte) == EMBER_SUCCESS) {
        return SERIAL_DATA_OK;
      }
    } else if ((halCommonGetInt16uMillisecondTick() - time) > 100) {
      timeout -= 1;
    }
  }

  return NO_SERIAL_DATA;
}

// This function is used to read bootload serial port one block at a time.
// XModem protocol specifies its data block length to be 128 bytes plus 
// 2 bytes crc and appropriate header bytes.  The function validates the header
// and the crc and return appropriate value.
static int8u xmodemReceiveBlock(void) {
  int8u header=0, blockNumber=0, blockCheck=0, crcLow=0, crcHigh=0;
  int8u ii, temp;
  int16u crc = 0x0000;
  int8u status = STATUS_NAK;
  
  if ((xmodemReadSerialByte(SERIAL_TIMEOUT_3S, &header) == NO_SERIAL_DATA) 
      || (header == XMODEM_CANCEL)
      || (header == XMODEM_CC)) 
    return STATUS_RESTART;
    
  if ((header == XMODEM_EOT) && 
      (xmodemReadSerialByte(SERIAL_TIMEOUT_3S, &temp) == NO_SERIAL_DATA)) 
    return STATUS_DONE;
      
  if ((header != XMODEM_SOH)
    || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &blockNumber) != SERIAL_DATA_OK)
    || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &blockCheck)  != SERIAL_DATA_OK)) {
    emberSerialFlushRx(bootloadSerial);
    return STATUS_NAK;
  }

  if (blockNumber + blockCheck == 0xFF) {
    if (blockNumber == expectedBlockNumber) {
      status = STATUS_OK;
    } else if (blockNumber == (int8u)(expectedBlockNumber - 1)) {
      // Sender probably missed our ack so ack again
      return STATUS_DUPLICATE;  
    }
  } 

  for (ii = 0; ii < XMODEM_BLOCK_SIZE; ii++) {
    if (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &xmodemBlock[ii]) != SERIAL_DATA_OK) {
      return STATUS_NAK;
    }
    crc = halCommonCrc16(xmodemBlock[ii], crc);
  }

  if ((xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &crcHigh) != SERIAL_DATA_OK)
      || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &crcLow) != SERIAL_DATA_OK) 
      || (crc != HIGH_LOW_TO_INT(crcHigh, crcLow))) {
    return STATUS_NAK;
  }
  return status;
}

// The function sends XModem ready character 'C' as stated by XModem protocol
// as a signal to the host (PC) to start transmit XModem data.
static void xmodemSendReady(void)
{
  emberSerialFlushRx(bootloadSerial);
  emberSerialWriteByte(bootloadSerial, XMODEM_READY);
}

// After the (source) node receives query response from the target node.  It
// signals the PC to transmit data and it reads out the data one XModem block
// at a time.  If all data bytes are read fine and the crc is checked out fine,
// it then breaks up the 128-byte data into two 64-byte chunks to send 
// over the air to the target node.  It repeats the process untill all data
// is transfered successfully.
static void xmodemReceiveAndForward(void)
{
#ifndef SBL_LIB_SRC_NO_PASSTHRU   // used in passthru mode
  boolean xmodemStart = FALSE;
  int8u counter=0, i;
  expectedBlockNumber = 1;
  
  for (;;) {
    int8u status;

    halResetWatchdog();
    xmodemSendReady();
    // Determine if the PC has cancelled the session.  We wait ~ 20 s
    // which is a bit higher than xmodem (retry) timeout which is 10 s.
    ++counter;
    if(xmodemStart && counter > 200) {
      bl_print("Error: PC stops sending us data\r\n");
      blState = BOOTLOAD_STATE_NORMAL;
      blMode = BOOTLOAD_MODE_NONE;
      return;
    }

    do {
      int8u outByte;

      status = xmodemReceiveBlock();
      
      switch (status)
      {
      case STATUS_OK:
        // When receive first good packet, that means we have 'officially'
        // started XModem transfer.
        xmodemStart = TRUE;
        counter = 0;
        // Forward data to the target node
        if(!bootloadSendImage(XMODEM_SOH)) {
          debug("Error: cannot forward data to target node\r\n");
          // Cancel XModem session with the host (PC)
          outByte = XMODEM_CANCEL;
          emberSerialFlushRx(bootloadSerial);
          for(i=0; i<5; ++i) {
            emberSerialWriteByte(bootloadSerial, outByte);
          }
          return;
        }
        expectedBlockNumber++;
        // Yes, fall through: a duplicate means the sender missed our ack.
        // Ack again as if everything's okay.
      case STATUS_DUPLICATE:
        outByte = XMODEM_ACK;
        break;
      case STATUS_NAK:
        outByte = XMODEM_NAK;
        break;
      case STATUS_DONE:
        if(!bootloadSendImage(XMODEM_EOT)) {
          debug("Error: cannot forward complete signal to target node\r\n");
          return;
        }
        emberSerialWriteByte(bootloadSerial, XMODEM_ACK);
        blState = BOOTLOAD_STATE_NORMAL;
        blMode  = BOOTLOAD_MODE_NONE;
        emberSerialPrintf(appSerial, "Bootload Complete!\r\n");
        return;
      default:
        continue;
      }
      emberSerialFlushRx(bootloadSerial);
      emberSerialWriteByte(bootloadSerial, outByte);
    } while (status != STATUS_RESTART);
  }
#else
  bl_print("ERR: called stub xmodemReceiveAndForward\r\n");
#endif // SBL_LIB_SRC_NO_PASSTHRU
}

// Read all flash page ranges that image resides.
static void flashReadPageRange(void)
{
  int16u range;
  int8u i;
  int8u totalRanges = 6;

  // We are reading the (header) flash to obtain page range values which would 
  // tell us all the flash pages that the application image resides.  
  // The offset of page ranges is 8 words into the header.
  int16u pageRangeAddr = HEADER_ADDR+8;

  for(i=0; i<totalRanges; ++i) {
    halInternalFlashBlockRead(pageRangeAddr+i, &range, 1);
    // store the page range
    #if defined(XAP2B)
      pageRanges[i].begPg = HIGH_BYTE(range);
      pageRanges[i].endPg = LOW_BYTE(range);
    #elif defined(AVR_ATMEGA_128)
      pageRanges[i].begPg = LOW_BYTE(range);
      pageRanges[i].endPg = HIGH_BYTE(range);
    #endif
  }
}

static boolean flashStreamData(int16u *data, int8u len, int8u type)
{
  int8u i = 0;
  int8u temp;

  // process data one word at a time.  len is word length.
  while(len--) {
    #if defined(XAP2B)
      temp = HIGH_BYTE(data[i]);
    #elif defined(AVR_ATMEGA_128)
      if(type == DATA_TYPE) {
        temp = LOW_BYTE(data[i]);
      } else { // tag info
        temp = HIGH_BYTE(data[i]);
      }
    #endif
    xmodemBlock[currentIndex++] = temp;
    flashStreamCrc = calculateCrc32(flashStreamCrc, temp);
    if(currentIndex >= XMODEM_BLOCK_SIZE) {
      debug("Error: currentIndex>=XMODEM_BLOCK_SIZE, %x\r\n", currentIndex);
    }
    #if defined(XAP2B)
      temp = LOW_BYTE(data[i]);
    #elif defined(AVR_ATMEGA_128)
      if(type == DATA_TYPE) {
        temp = HIGH_BYTE(data[i]);
      } else { // tag info
        temp = LOW_BYTE(data[i]);
      }
    #endif
    xmodemBlock[currentIndex++] = temp;
    flashStreamCrc = calculateCrc32(flashStreamCrc, temp);
    if(currentIndex == XMODEM_BLOCK_SIZE) {
      if(!bootloadSendImage(XMODEM_SOH)) {
        return FALSE;
      }
      expectedBlockNumber++;
      currentIndex = 0;
    }
    ++i;
  }
  return TRUE;
}

#ifndef SBL_LIB_SRC_NO_CLONE // used in clone mode
static boolean flashReadAndForwardBlock(int16u addr, int16u size) 
{
  int8u dataSize = 16;  // read 16 words at a time
  int8u count = 0;
  int16u data[16];
  
  // size needs to be multiple of 16
  if(size & 0x000F) {
    bl_print("invalid flash read size\r\n, 0x%4x", size);
    return FALSE;
  }

  do
  {
    halInternalFlashBlockRead(addr+(dataSize*count), data, dataSize);
    if(!flashStreamData(data, dataSize, DATA_TYPE)) {
      return FALSE;
    }
    ++count;
  }
  while (size -= dataSize);
  return TRUE;
}

// Insert tag at the beginning of each flash page, before forwarding the data.
// The length includes all the data to be programmed plus two bytes of address
// field.  For more information on different types of tags, refer to 
// em250-ebl.h.  Note that the length is byte length, but the address is 
// word address.
static boolean flashInsertAndForwardTag(int16u type, int16u length, int16u addr)
{
  // A generic tag composes of type, length, and address.  'Type' dictates
  // flash operation ex. erase, program.  Length is the number of data byte
  // to be programmed to flash.  And address tells where to start programming
  // the flash.  
  int8u tagSize = 3;
  int16u data[3];
  boolean status;

  data[0] = type;
  data[1] = length;

  if(type == EBLTAG_END) {
    // In case of end tag, there is no address filed, just type and length.
    // The address filed is replaced by file crc values.
    status = flashStreamData(data, 2, TAG_TYPE);
  } else {
    data[2] = addr;
    status = flashStreamData(data, tagSize, TAG_TYPE);
  }
  return status;
}

static void flashComplete(void)
{
  // Include eblCrc, used in verification process, in the last packet
  // eblCrc is emplaced little-endian rather than big-endian
  // because our CRC algorithm is little-endian.  This allows
  // other end to include the transmitted CRC in its CRC calculation
  // and wind up with the correct CRC32_END magic remainder.
  flashStreamCrc = ~flashStreamCrc;
  xmodemBlock[currentIndex++] = (int8u)(flashStreamCrc >> 0);
  xmodemBlock[currentIndex++] = (int8u)(flashStreamCrc >> 8);
  xmodemBlock[currentIndex++] = (int8u)(flashStreamCrc >> 16);
  xmodemBlock[currentIndex++] = (int8u)(flashStreamCrc >> 24);

  // Add padding
  while(currentIndex < XMODEM_BLOCK_SIZE) {
    xmodemBlock[currentIndex++] = 0xFF;
  }
  // Send last data packet
  if(!bootloadSendImage(XMODEM_SOH)) {
    return;
  }
  
  // Send end of transmission packet
  expectedBlockNumber++;
  if(!bootloadSendImage(XMODEM_EOT)) {
    return;
  }

  // Set state variables to default values
  blState = BOOTLOAD_STATE_NORMAL;
  blMode  = BOOTLOAD_MODE_NONE;
  emberSerialPrintf(appSerial, "Bootload Complete!\r\n");
}
#endif // SBL_LIB_SRC_NO_CLONE

static void flashReadAndForward(void)
{
#ifndef SBL_LIB_SRC_NO_CLONE // used in clone mode
  int8u range, page;
  int16u addr;
  int8u firstPage;
  // Reinitialize values
  currentIndex = 0;
  expectedBlockNumber = 1;
  flashStreamCrc = INITIAL_CRC;

  // obtain all page ranges
  flashReadPageRange();

  firstPage = pageRanges[0].begPg;
  for(range=0; range<6; ++range) {
    for(page = pageRanges[range].begPg; 
        page <= pageRanges[range].endPg;
        ++page)
    {
      // end of flash
      if(page == 0xFF) {
        // Insert end tag.  The address field does not matter in this case.
        if(!flashInsertAndForwardTag(EBLTAG_END, 4, 0)) {
          return;
        }
        flashComplete();
        return;
      }
      addr = PAGE_TO_PROG_ADDR(page);
        
      // Processing first ebl flash page is a little special since for EM250, 
      // the page contains header information and for avr128, the first 
      // 256 bytes of the page is reserved for interrupt vectors.  
      // Before reading and sending application data, we want to read and 
      // send the header information first since that is what the bootloader 
      // expects.  
      if(page == firstPage) {
        #if defined(XAP2B)
          // Read and send header block of 0x20 word (64 bytes).  Note that 
          // tag is not needed for for header data since the bootloader 
          //  already knows where to program the header information.
          if(!flashReadAndForwardBlock(addr+0x20, 0x20)) {
            return;
          }

          // Insert tag for the first data block, which will erase the whole 
          // flash page and program the number of bytes (64 + 2 address bytes) 
          // specified to the flash.  These 64 bytes are vector data, which are 
          // part of stack info block.
          if(!flashInsertAndForwardTag(EBLTAG_ERASEPROG, 0x0042, addr)) {
            return;
          }
          // Read and send vector data, 64 bytes
          if(!flashReadAndForwardBlock(addr, 0x20)) {
            return;
          }

          // Insert next tag to program the rest of the data in the page.
          // Since we have already erase the whole page (from previous tag),
          // we do not need to erase the flash again, just program.  The 
          // number of bytes to program is:
          // PAGE_SIZE_BYTE - 64 (vector data) - 64 (header) + 2 (address)
          // = (PAGE_SIZE_BYTE - 126) bytes.  The address is offset by 128 byte 
          // (or 0x40 word) from the beginning of the page.
          if(!flashInsertAndForwardTag(
                          EBLTAG_PROG, 
                          (PAGE_SIZE_BYTE-126), 
                          (addr+0x40))) {
            return;
          }
          // Read the send the rest of data in the page
          if(!flashReadAndForwardBlock(
                          (addr+0x40), 
                          ((PAGE_SIZE_BYTE-128)/2))) {
            return;
          }
        #elif defined(AVR_ATMEGA_128)
          // Read header block of 0x20 word (64 bytes).  Note that tag is not
          // needed for for header data since the bootloader already knows
          // where to program the header information.
          if(!flashReadAndForwardBlock(HEADER_ADDR, 0x20)) {
            return;
          }

          // For the first ebl page, we skip the first 256 bytes of 
          // interrupt vector.  The total of data to program is:
          // PAGE_SIZE_BYTE - 256 (vector) + 2 (address) 
          // = (PAGE_SIZE_BYTE - 254) bytes.
          if(!flashInsertAndForwardTag(
                      EBLTAG_ERASEPROG, 
                      (PAGE_SIZE_BYTE-254),   
                      APP_ADDR)) {
            return;
          }
          // Read the send the rest of data in the page
          if(!flashReadAndForwardBlock(
                        APP_ADDR, 
                        ((PAGE_SIZE_BYTE-256)/2))) {  // length in words
            return;
          }
        #endif
      } 
      #ifdef AVR_ATMEGA_128
        else if (page == LAST_DATA_PAGE) {
          // For avr128, last page is also special because it contains
          // ebl header information.  We can utilize the page up until the
          // ebl header.
          if(!flashInsertAndForwardTag(
                      EBLTAG_ERASEPROG, 
                      (LAST_PAGE_SIZE+2),     // include 2 bytes of address
                      addr)) {
            return;
          }
          if(!flashReadAndForwardBlock(
                        addr, 
                        (LAST_PAGE_SIZE/2))) {  // length in words
            return;
          }
        }
      #endif
      else {
        if(!flashInsertAndForwardTag(
                      EBLTAG_ERASEPROG, 
                      (PAGE_SIZE_BYTE+2),     // include 2 bytes of address
                      addr)) {
          return;
        }
        if(!flashReadAndForwardBlock(
                      addr, 
                      (PAGE_SIZE_BYTE/2))) {  // length in words
          return;
        }
      }
      halResetWatchdog();
    } // end loop for pages
  } // end loop for ranges
#else 
  bl_print("ERR: called stub flashReadAndForward\r\n");
#endif // SBL_LIB_SRC_NO_CLONE
}


// The function is called to send (new application) data to the target node
// over the air.  Note that the source and the target nodes need to be one
// hop away from each other and have good link to each other to ensure
// quick and successful bootloading.
static boolean bootloadSendImage(int8u type)
{
  int8u block[2];
  int8u i;
  // Send 128 bytes of XModem data in two
  // 64-byte radio packets
  block[0] = ((2 * expectedBlockNumber) - 1);
  block[1] = (2 * expectedBlockNumber);

  for(i=0; i<2; ++i) {
    debug("sending OTA block %x\r\n", block[i]);
    debug("free buffer: %x\r\n", emPacketBufferFreeCount);
    currentRetriesRemaining = NUM_PKT_RETRIES;
    currentOTABlock = block[i];
    actionTimer = TIMEOUT_IMAGE_SEND;
    if(type == XMODEM_SOH) {
      blState = BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK;      
      sendMACImageSegment();
    } else if(type == XMODEM_EOT) {
      blState = BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK;      
      sendMACCompleteSegment();
    } 
    while((blState == BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK) ||
          (blState == BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK)) {
      halResetWatchdog();
      emberTick();
      bootloadUtilTick();
      emberSerialBufferTick();
    }
    
    if(blState == BOOTLOAD_STATE_NORMAL) {
      emberSerialPrintf(appSerial, "ERROR: Image send OTA failed\r\n");
      return FALSE;
    }
    // If the target node sends ACK in respond to our EOT packet, then
    // we're done.
    if(blState == BOOTLOAD_STATE_DONE) {
      return TRUE;
    }
  }
    
  return TRUE;

}
// taken from bootload.c
static int32u calculateCrc32(int32u accumulator, int8u data)
{
  int8u jj;
  int32u previous;
  int32u oper;

  previous = (accumulator >> 8) & 0x00FFFFFFL;
  oper = (accumulator ^ data) & 0xFF;
  for (jj = 0; jj < 8; jj++) {
    oper = ((oper & 0x01)
                ? ((oper >> 1) ^ POLYNOMIAL)
                : (oper >> 1));
  }

  return (previous ^ oper);
}
#endif // SBL_LIB_TARGET

// This function actually crafts and sends out the radio packets.  XMODEM_SOH
// is the data packet and XMODEM_EOT is the end of transmission packet.
static void sendRadioMessage(int8u messageType, boolean isBroadcast)
{  
  // allow space for the longest possible message
  int8u length, index, i, startByte, data;
  EmberMessageBuffer buffer;
  int16u crc = 0x0000;
  int16u blVersion; // if the version is invalid, the value is 0xFFFF
  
#ifdef SBL_LIB_SRC_SUPPORT_V1
  if (bootV1State != BOOT_V1_STATE_IDLE)
  {
    if(currentOTABlock & 0x01) {
      startByte = 0;
    } else {
      startByte = 64;
    }
    if (messageType == XMODEM_SOH)
    {
      sendV1DataPacket(targetAddress, currentOTABlock, &xmodemBlock[startByte]);
      bootV1State = BOOT_V1_STATE_DATA_ACK;
      bootV1Time = v1Time;
      bootV1Cnt = 0;
    }
    else
    {
      if (messageType == XMODEM_EOT)
      {
        bootV1State = BOOT_V1_STATE_EOT_ACK;
        bootV1Time = v1Time;
        bootV1Cnt = 0;
      }
      sendV1ControlPacket(targetAddress, messageType, currentOTABlock);
    }
    return;
  }
#endif // SBL_LIB_SRC_SUPPORT_V1

  // for convenience, we form the message in a byte array first.
  index = bootloadMakeHeader(bootloadBuffer, messageType);
  
  switch (messageType)
  {
  case XMODEM_QRESP:
    {
      // {1:blActive(0)}
      bootloadBuffer[index++] = FALSE;
      // {2:mfgId}
      halCommonGetToken(&bootloadBuffer[index], TOKEN_MFG_MANUF_ID);
      index += sizeof(tokTypeMfgManufId);
      // {16:hwTag}
      halCommonGetToken(&bootloadBuffer[index], TOKEN_MFG_BOARD_NAME);
      index += sizeof(tokTypeMfgBoardName);
      // {1:blCapabilities}
      // In the future, we should read the capabilities mask from fixed 
      // location in bootloader.  However, currently we do not have any
      // bootload capabilities implemented.  This filed is included for
      // future use.
      bootloadBuffer[index++] = 0;
      bootloadBuffer[index++] = PLAT;
      bootloadBuffer[index++] = MICRO;
      bootloadBuffer[index++] = PHY;
      // {2:blVersion}
      blVersion = halGetStandaloneBootloaderVersion();
      bootloadBuffer[index++] = HIGH_BYTE(blVersion); // bootloader version
      bootloadBuffer[index++] = LOW_BYTE(blVersion); // bootloader build
      length = index;
    }
    break; 
  case XMODEM_SOH: 
    bootloadBuffer[index++] = currentOTABlock;
    bootloadBuffer[index++] = 0xFF - currentOTABlock;
    if(currentOTABlock & 0x01) {
      startByte = 0;
    } else {
      startByte = 64;
    }
    for(i = index; i<(BOOTLOAD_OTA_SIZE + index); ++i) {
      data = xmodemBlock[startByte++];
      bootloadBuffer[i] = data;
      crc = halCommonCrc16(data, crc);
    }
    // Include crc in the packet
    bootloadBuffer[i] = HIGH_BYTE(crc);
    bootloadBuffer[++i] = LOW_BYTE(crc);
    length = MAX_BOOTLOAD_MESSAGE_SIZE;
    break;
  case XMODEM_EOT: 
    bl_print("TX EOT\r\n");
  case XMODEM_QUERY:
    length = index;
    break; 
  default:
    return;
  }

  buffer = emberFillLinkedBuffers(bootloadBuffer, length);
  // check to make sure a buffer is available
  if (buffer == EMBER_NULL_MESSAGE_BUFFER) {
    bl_print("TX error: Out Of Buffers\r\n");
    return;
  }

  if ( emberSendBootloadMessage(isBroadcast, targetAddress, buffer)
     !=EMBER_SUCCESS ) {
    bl_print("send bootload msg failed\r\n");
    blState = BOOTLOAD_STATE_NORMAL;
    blMode = BOOTLOAD_MODE_NONE;
  } 

  // done with the packet buffer
  emberReleaseMessageBuffer(buffer);
  
}

static void printEui(EmberEUI64 eui)
{
  bl_print("%x%x%x%x%x%x%x%x",
           eui[0], eui[1], eui[2], eui[3],
           eui[4], eui[5], eui[6], eui[7]);
}

// This function determines if a node is sleepy (or mobile) by checking to see
// if it's eui is found in the child table.
static boolean isSleepy(EmberEUI64 eui)
{
  int8u i;
  for (i = 0; i < EMBER_CHILD_TABLE_SIZE; i++) {
    EmberEUI64 childEui64;
    EmberNodeType childType;
    if ( (EMBER_SUCCESS == emberGetChildData(i, childEui64, &childType) )
       &&(0 == MEMCOMPARE(eui, childEui64, EUI64_SIZE) ) ) {
      return TRUE;
    }
  }
  return FALSE;
}

#else //USE_BOOTLOADER_LIB
  #ifdef SBL_LIB_SRC_SUPPORT_V1
    void emberRawTransmitCompleteHandler(EmberMessageBuffer message,
                                     EmberStatus status) {}
  #endif // SBL_LIB_SRC_SUPPORT_V1
  // stub functions if USE_BOOTLOADER_LIB is not defined
  void bootloadUtilTick(void){}
  void bootloadUtilStartBootload(EmberEUI64 target, bootloadMode mode){}
  EmberStatus bootloadUtilSendRequest(EmberEUI64 targetEui,
                                    int16u mfgId,
                                    int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                    int8u encryptKey[BOOTLOAD_AUTH_COMMON_SIZE],
                                    bootloadMode mode) {}
  void bootloadUtilSendQuery(EmberEUI64 target) {}
  void bootloadUtilInit(int8u appPort, int8u bootloadPort) {}
#endif//USE_BOOTLOADER_LIB
