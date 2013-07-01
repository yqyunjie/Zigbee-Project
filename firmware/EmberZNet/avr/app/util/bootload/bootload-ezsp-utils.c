// *******************************************************************
// * bootload-ezsp-utils.c
// * 
// * Utilities used for bootloading.
// * See bootload-utils.h for more complete description
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/util/bootload/bootload-ezsp.h"

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"

#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/serial/serial.h"
#include "app/util/source-route-host.h"

#include "hal/micro/bootloader-interface-standalone.h"

#include <string.h>
#include <stdlib.h>

#include "app/util/ezsp/serial-interface.h"

#include "app/standalone-bootloader-demo-host/xmodem-spi.h"
#include "app/standalone-bootloader-demo-host/common.h"
#include "app/util/bootload/bootload-utils-internal.h"
#include "app/util/bootload/bootload-utils.h"
#include "app/util/bootload/bootload-ezsp-utils.h"

#ifndef NO_SERIAL_DATA
#define NO_SERIAL_DATA        0x01 
#endif

#ifdef GATEWAY_APP
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include <unistd.h>
// For those pesky POSIX compliant OSes
#if !defined(WIN32) && !defined(Sleep)
#define Sleep(time) usleep(time)
#endif
#ifndef halCommonDelayMicroseconds
#define halCommonDelayMicroseconds(X) Sleep(X)
#endif
#endif

#if defined(AVR_ATMEGA_32)
#include "micro/avr-atmega/32/reserved-ram.h"
#pragma segment="RSTACK"
#pragma segment="CSTACK"
#endif

#if defined(AVR_ATMEGA_32)
// reserved ram is special, see reserved-ram.h
__no_init __root int16u last_known_program_counter   @ HAL_RESERVED_RAM_PC_DIAG;
#endif

int8u ticksOn = 1;
int8u prevTicksOn = 1;
extern int32u passThruStart;
extern int32u queryStart;
extern int8u ledState;
int32u lastIBMtime;

// Initialize bootload state to normal (not currently participate in
// bootloading)
bootloadState blState = BOOTLOAD_STATE_NORMAL;

// There are three main bootload modes (serial, clone and passthru) that
// determines the responsibilities of the source node whether to bootload
// itself via serial (uart) interface, or to receive the image data via xmodem
// from the host PC to bootload another node or to read the image data from
// its own flash to bootload another node.
bootloadMode blMode = BOOTLOAD_MODE_NONE;

// bootload (xmodem) protocols variables
int8u currentRetriesRemaining;
int8u xmodemBlock[XMODEM_BLOCK_SIZE];
int8u bootloadBuffer[MAX_BOOTLOAD_MESSAGE_SIZE + BOOTLOAD_OTA_SIZE];
int8u expectedBlockNumber = 1;

// XModem protocol supports packet of 128 bytes in length, however,
// we can only send it in BOOTLOAD_OTA_SIZE (64 bytes) at a time
// over the air (OTA).  Hence, XModem packets are divided into two
// over-the-air messages. Values of the over-the-air block number are
// (2*expectedBlockNumber) and (2*expectedBlockNumber)-1
int8u currentOTABlock;

// Eui64 address of the target node
EmberEUI64 targetAddress;

// To keep track of how much time we should wait.  A simple timeout mechanism
int8u actionTimer = 0;
int16u lastBootTime = 0;

// Port for serial prints and bootloading.  In case of gateway app, we set
// bootloadSerial port to 0 to match BACKCHANNEL_SERIAL_PORT defined in 
// backchannel.h.
int8u appSerial = APP_SERIAL;
#ifdef GATEWAY_APP
int8u bootloadSerial = 0;
#else
int8u bootloadSerial = APP_SERIAL;
#endif

// Making a handy broadcast EUI64 address
EmberEUI64 broadcastEui64 = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Authentication block contains necessary information to perform
// security measure before going into bootload mode
int8u authBlock[BOOTLOAD_AUTH_CHALLENGE_SIZE];

// This message buffer stores the key while waiting for the
// challenge to be sent by the target node stack.
static int8u signingKey[BOOTLOAD_AUTH_COMMON_SIZE];

// This message buffer stores the authentication challenge while
// waiting for the authentication response to come back from the
// requesting node.
static int8u challenge[BOOTLOAD_AUTH_CHALLENGE_SIZE];

// This is needed because debug()
#if defined(ENABLE_BOOTLOAD_PRINT) || defined(ENABLE_DEBUG)
static int8u myBuf[70];
#endif

// Node build info
int16u nodeBlVersion;
int8u nodePlat;
int8u nodeMicro;
int8u nodePhy;

// Both of these need to be correctly handled in the applications's
// ezspErrorHandler().
// ezsp error info
EzspStatus bootloadEzspLastError = EZSP_SUCCESS;
// If this is not EZSP_SUCCESS, the next call to ezspErrorHandler()
// will ignore this error.
EzspStatus ignoreNextEzspError = EZSP_SUCCESS;

// Forward declaration of functions used internally by bootload 
// utilities library.  These functions are defined by bootload utility
// library.

static int8u bootloadMakeHeader(int8u *message, int8u type);
static int8u xmodemReadSerialByte(int8u timeout, int8u *byte);
static int8u xmodemReceiveBlock(void);
static void xmodemReceiveAndForward(void);
static void xmodemSendReady(void);
static boolean bootloadSendImage(int8u type);
static boolean isSleepy(EmberEUI64 eui);
static void sendRadioMessage(int8u type, boolean isBroadcast);
#define sendMACImageSegment() sendRadioMessage(XMODEM_SOH, FALSE)
#define sendMACCompleteSegment() sendRadioMessage(XMODEM_EOT, FALSE)
static void printEui(EmberEUI64 eui);
static void printDebugEOL(void);
static void printEOL(void);

// *******************************************************************
// Special debug switchinh functions.
// *******************************************************************

EmberStatus debugWaitSend(int8u port)
{
  if (port == BOOTLOAD_DEBUG_PORT)
    return EMBER_SUCCESS;
  else
    return emberSerialWaitSend(port);
}

EmberStatus debugWriteData(int8u port, int8u *data, int8u length)
{
  if (port == BOOTLOAD_DEBUG_PORT) {
#ifndef GATEWAY_APP
    if (ticksOn & BOOTTICKON)
      return EMBER_SUCCESS;
    else
#endif
      return ezspSerialWrite(length, data);
  }
  else
    return emberSerialWriteData(port, data, length);
}

EmberStatus debugPrintf(int8u port, PGM_P formatString, ...)
{
  va_list ap;
  EmberStatus stat = EMBER_SUCCESS;

  va_start (ap, formatString);
  if(!emPrintfInternal(debugWriteData, port, formatString, ap))
    stat = EMBER_ERR_FATAL;
  va_end (ap);

  return stat;
}

// ****************************************************************
// Internal support functions for bootload features.
// ****************************************************************

static void setActionTimer(int8u newActionTimer)
{
  actionTimer = newActionTimer;
  lastBootTime = halCommonGetInt16uMillisecondTick();
}

static void idleBootloadUtils(void)
{
  blState = BOOTLOAD_STATE_NORMAL;
  blMode = BOOTLOAD_MODE_NONE;
  ticksOn = prevTicksOn;
}

// ****************************************************************
// Public functions that can be called from the application to utilize
// bootload features.
// ****************************************************************

// -------------------------------------------------------------------------
// Initialize serial port
void bootloadUtilInit(int8u appPort, int8u bootloadPort)
{
  MEMSET(broadcastEui64, 0xFF, EUI64_SIZE);     

  appSerial = appPort;
  bootloadSerial = bootloadPort;
  emberSerialInit(bootloadSerial, BOOTLOAD_BAUD_RATE, PARITY_NONE, 1);

  // Get node build info
  nodeBlVersion = ezspGetStandaloneBootloaderVersionPlatMicroPhy(&nodePlat,
                                                                 &nodeMicro,
                                                                 &nodePhy);
}

// -------------------------------------------------------------------------
// Send bootload request to initiate bootload transaction with the target node.
EmberStatus bootloadUtilSendRequest(EmberEUI64 targetEui,
                                    int16u mfgId,
                                    int8u hardwareTag[BOOTLOAD_HARDWARE_TAG_SIZE],
                                    int8u encryptKey[BOOTLOAD_AUTH_COMMON_SIZE],
                                    bootloadMode mode)
{
  int8u index;
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
  MEMCOPY(signingKey, encryptKey, BOOTLOAD_AUTH_COMMON_SIZE);

  if ( (status = ezspSendBootloadMessage(FALSE, targetAddress, index, bootloadBuffer))
       !=EMBER_SUCCESS ) {
    idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
  } else {
    // message sent successfully.
    blState = BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE;
    setActionTimer(TIMEOUT_AUTH_CHALLENGE);
  }

  if (EMBER_SUCCESS != status) {
    // We failed somewhere above.  Release the key buffer if it was allocated.
    MEMSET(signingKey, 0, sizeof(signingKey));
  }

  return status;
}

EmberStatus bootloadSendAuthChallenge(EmberEUI64 sourceEui)
{
  int8u index, challengeStart;
  int32u macTimer;
  EmberStatus status;

  index = bootloadMakeHeader(bootloadBuffer, XMODEM_AUTH_CHALLENGE);
  challengeStart = index;

  // The authentication challenge is the entire bootload payload.
  // 1st byte -- a version setting.
  bootloadBuffer[index++] = 0x01;
  // Send bootloader version here also.
  // {2:blVersion}
  bootloadBuffer[index++] = HIGH_BYTE(nodeBlVersion); // node bootloader version
  bootloadBuffer[index++] = LOW_BYTE(nodeBlVersion);  // node bootloader build
  // (1:plat)
  bootloadBuffer[index++] = nodePlat; // Needed in case blVersion unknown
  // (1:micro)
  bootloadBuffer[index++] = nodeMicro; // Needed in case blVersion unknown
  // (1:phy)
  bootloadBuffer[index++] = nodePhy; // Needed in case blVersion unknown
  // The second part of the challenge is the local eui.
  MEMCOPY(&bootloadBuffer[index], emLocalEui64, EUI64_SIZE);
  index += EUI64_SIZE;
  // The next part of the challenge is a snapshot of the mac timer.
  // Which we do not have, so use our time.
  macTimer = halCommonGetInt32uMillisecondTick();
  // The em250's mac timer is 20 bits long.  Disregard the first byte (zero),
  // even for other devices that may have a 32-bit symbol timer.
  MEMCOPY(&bootloadBuffer[index], &(((int8u*)(&macTimer))[1]), 3);
  index += 3;
  // The rest of the challenge is padded with random data.
  while ( (index-challengeStart) < BOOTLOAD_AUTH_CHALLENGE_SIZE) {
    bootloadBuffer[index++] = halCommonGetRandom();
  }

  // Save the challenge so that the response can be verified when received.
  MEMCOPY(challenge,&bootloadBuffer[challengeStart],
          BOOTLOAD_AUTH_CHALLENGE_SIZE);

  // Send the message.
  if ( ezspSendBootloadMessage(FALSE, sourceEui, index, bootloadBuffer)
       !=EMBER_SUCCESS ) {
    status = EMBER_ERR_FATAL;
    bl_print("Error: send auth challenge failed\r\n");
  } else {
    status = EMBER_SUCCESS;
  }

  return status;
}


EmberStatus bootloadSendAuthResponse(int8u* authenticationBlock,
                                     EmberEUI64 targetEui)
{
  int8u index;
  EmberStatus status;

  index = bootloadMakeHeader(bootloadBuffer, XMODEM_AUTH_RESPONSE);

  // AES encrypt the challenge in place to yield the response.
  ezspAesEncrypt(authenticationBlock, signingKey, authenticationBlock);

  // The authentication response is the entire bootload payload.
  MEMCOPY(&bootloadBuffer[index],authenticationBlock,
          BOOTLOAD_AUTH_RESPONSE_SIZE);
  index += BOOTLOAD_AUTH_RESPONSE_SIZE;

  if ( ezspSendBootloadMessage(FALSE, targetEui, index, bootloadBuffer)
       !=EMBER_SUCCESS ) {
    status = EMBER_ERR_FATAL;
    bl_print("Error: send auth response failed\r\n");
  } else {
    // The challenge was sent successfully.
    status = EMBER_SUCCESS;
  }

  MEMSET(signingKey, 0, sizeof(signingKey));

  return status;
}

boolean isTheSameEui64(EmberEUI64 source, EmberEUI64 target)
{
  return !(MEMCOMPARE(source, target, EUI64_SIZE));
}

// *******************************************************************
// Begin Ember callback handlers
// Populate as needed
//

// Callback
// A callback invoked by the EmberZNet stack when a bootload message is
// received.
void ezspIncomingBootloadMessageHandler(
      // The EUI64 of the sending node.
      EmberEUI64 longId,
      // The link quality from the node that last relayed the message.
      int8u lastHopLqi,
      // The energy level (in units of dBm) observed during the reception.
      int8s lastHopRssi,
      // The length of the messageContents parameter in bytes.
      int8u messageLength,
      // The bootload message that was sent.
#if defined(ENABLE_BOOTLOAD_PRINT) || defined(ENABLE_DEBUG)
      int8u *messageContents)
#else
      int8u *myBuf)
#endif
{
  int8u type;
  int8u block;
  int8u version;
  int16u mfgId;
  int16u blVersion;

#if defined(ENABLE_BOOTLOAD_PRINT) || defined(ENABLE_DEBUG)
  int8u myBufLen = messageLength;

  if (myBufLen > 70)
    myBufLen = 70;
  MEMCOPY(myBuf, messageContents, myBufLen);

  if (myBufLen != messageLength)
    debug("Unexpectedly large pkt: %d vs %d: %x%x%x%x %x%x%x%x\r\n",
          messageLength, myBufLen,
          myBuf[0], myBuf[1], myBuf[2], myBuf[3],
          myBuf[4], myBuf[5], myBuf[6], myBuf[7]);
#endif

  if (messageLength >= 2) {
    version = myBuf[OFFSET_VERSION];
    type = myBuf[OFFSET_MESSAGE_TYPE];
    switch(type) {
    case XMODEM_LAUNCH_REQUEST:
    {
      // Extract the manufacturer Id (transmitted low byte first).
      mfgId = HIGH_LOW_TO_INT(myBuf[OFFSET_MFG_ID+1],
                              myBuf[OFFSET_MFG_ID]);
      if (hostBootloadUtilLaunchRequestHandler(
        lastHopLqi, lastHopRssi, mfgId,
        &myBuf[OFFSET_HARDWARE_TAG], longId)) {
        // The application has agreed to launch the bootloader.
        // Authenticate the requesting node before launching the bootloader.
        if (EMBER_SUCCESS == bootloadSendAuthChallenge(longId) ) {
          // The challenge was sent successfully.
          blState = BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE;
          setActionTimer(TIMEOUT_AUTH_RESPONSE);
        }
        else {
          bl_print("unable to send authentication challenge\r\n");
          // Abort bootload session.
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
        }
      }
      else {
        bl_print("application refused to launch bootloader\r\n");
        // Abort bootload session.
        idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
      }
    }
    break;
    case XMODEM_AUTH_CHALLENGE:
      if (BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE == blState) {
        // Extract the challenge.
        MEMCOPY(authBlock, &myBuf[OFFSET_AUTH_CHALLENGE],
                BOOTLOAD_AUTH_CHALLENGE_SIZE);

        // If dealing with non-sleepy node (ex. router), then the library
        // will send authentication response message directly.
        if(!isSleepy(longId)) {
          // Encrypt the challenge and send the result back as the response.
          if (EMBER_SUCCESS == bootloadSendAuthResponse(authBlock, longId) ) {
            // Give the target time to reboot into the bootloader,
            // then start the bootload process.
            blState = BOOTLOAD_STATE_DELAY_BEFORE_START;
            setActionTimer(TIMEOUT_START);
          }
          else {
            bl_print("unable to send authentication response\r\n");
            // Get out from bootload mode
            idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
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
          setActionTimer(TIMEOUT_START);
        }
      }
      break;
    case XMODEM_AUTH_RESPONSE:
      if (BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE == blState) {
        int8u response[BOOTLOAD_AUTH_RESPONSE_SIZE];
        int8u verificationKey[16];

        // Extract the response.
        MEMCOPY(response, &myBuf[OFFSET_AUTH_RESPONSE],
                BOOTLOAD_AUTH_RESPONSE_SIZE);

        // Load the secret key to verify the response.
        // TOKEN_MFG_BOOTLOAD_AES_KEY has index 5
        ezspGetMfgToken(5, (int8u*)&verificationKey);

        // Encrypt the challenge.
        ezspAesEncrypt(challenge, (int8u*)verificationKey, challenge);

        // Compare the response to the encrypted challenge.
        if (!MEMCOMPARE(response,challenge,BOOTLOAD_AUTH_COMMON_SIZE) ) {
          EzspStatus ezspStat;
          // The response and the encrypted challenge match.
          // Launch the bootloader.
          emberSerialPrintf(appSerial, "Handshake complete.");
          printEOL();
          MEMSET(challenge, 0, sizeof(challenge));
          ezspStat = hostLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
          if (ezspStat != EZSP_SUCCESS) {
            emberSerialPrintf(appSerial, "reset NCP failed...Maybe NCP has no bootloader! ezspStat=0x%x :", ezspStat);
            emberSerialWaitSend(appSerial);
            printEOL();
            break;
          } else {
            emberSerialPrintf(appSerial,
              "now NCP in standalone bootloader mode...\r\n");
          }
#ifdef GATEWAY_APP
          emberSerialPrintf(appSerial, "Bye!\r\n");
          emberSerialWaitSend(appSerial);
          ezspClose();
          exit(0);
#else
          while(!hal260HasData()) {
            halResetWatchdog(); // Full time
            halCommonDelayMicroseconds(200); // ...
          }
          //the first check better return false due to EM260 resetting
          if(hal260VerifySpiProtocolActive()!=FALSE) {
            emberSerialPrintf(appSerial, "Restart EM260 failed 1!");
            printEOL();
            break;
          }
          //the second check better return true indicating the SPIP is active
          if(hal260VerifySpiProtocolActive()!=TRUE) {
            emberSerialPrintf(appSerial, "Restart EM260 failed 2!");
            printEOL();
            break;
          }
          //the third check better return true indicating the proper SPIP version
          if(hal260VerifySpiProtocolVersion()!=TRUE) {
            emberSerialPrintf(appSerial, "Restart EM260 failed SPI version!");
            printEOL();
            break;
          }
#endif
          // Redo em260 startup -- needed to get back to normal state.
          hostBootloadReinitHandler();
        }
        else {
          // The response and the encrypted challenge do not match.
          emberSerialPrintf(appSerial, "Authentication failed!");
          printEOL();
          // Get out from bootload mode
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          MEMSET(challenge, 0, sizeof(challenge));
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
    case XMODEM_QRESP:
      // Extract manufacturer id and hardware tag
      mfgId = HIGH_LOW_TO_INT(myBuf[QRESP_OFFSET_MFG_ID+1],
                              myBuf[QRESP_OFFSET_MFG_ID]);

      if (messageLength >= (QRESP_OFFSET_BL_VERSION + 1)) {
        blVersion = HIGH_LOW_TO_INT(myBuf[QRESP_OFFSET_BL_VERSION],
                                    myBuf[QRESP_OFFSET_BL_VERSION+1]);
      } else {
        blVersion = 0xFFFF;  // if the version is invalid, the value is 0xFFFF
      }
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
            idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
            return;
          }
        } else {
          // save the address of the node to be bootloaded
          MEMCOPY(targetAddress, longId, EUI64_SIZE);
        }
        // Check if protocol version is valid
        if ((version != BOOTLOAD_PROTOCOL_VERSION))
        {
          bl_print("RX bad packet, invalid protocl version 0x%x\r\n",
                   version);
          return;
        }

        // Check if the remote node is in bootload mode
        if (!myBuf[QRESP_OFFSET_BL_ACTIVE]) {
          bl_print("Error: remote node not in bootload mode\r\n");
          return;
        }
        bl_print("RX QUERY RESP from ");
        printEui(longId);
        bl_print(" with rssiValue %d\r\n", lastHopRssi);

        // Initiate xmodem transfer with the host PC for passthru or start
        // reading flash for clone bootloading
        if(blMode == BOOTLOAD_MODE_PASSTHRU) {
          blState = BOOTLOAD_STATE_START_SENDING_IMAGE;
          setActionTimer(0);
          emberSerialPrintf(appSerial,
                            "Please start .ebl upload image ...\r\n");
        } else {
          emberSerialPrintf(appSerial,
                            "Unexpected blMode=%d; aborting!\r\n",
                            blMode);
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          return;
        }
      } else if(blState == BOOTLOAD_STATE_QUERY) {
        // This is a result of issuing 'query_neighbor' command.
        // Notify the application of the query response received so the
        // application can make decision on whether or who to bootload.
        hostBootloadUtilQueryResponseHandler(lastHopLqi, lastHopRssi,
          myBuf[QRESP_OFFSET_BL_ACTIVE],
          mfgId,
          &myBuf[OFFSET_HARDWARE_TAG],
          longId, // eui of the node that sent it.
          myBuf[QRESP_OFFSET_BL_CAPS],
          myBuf[QRESP_OFFSET_PLATFORM],
          myBuf[QRESP_OFFSET_MICRO],
          myBuf[QRESP_OFFSET_PHY],
          blVersion);
      }
      // else ignore the query response
      else
        bl_print("Ignored QRESP, blState 0x%x\r\n", blState);
      break;
    case XMODEM_ACK:
      // Check if it's the expected block number.  If so, continue with the
      // image sending.  If not, ignore the duplicate.
      block = myBuf[OFFSET_BLOCK_NUMBER];
      if(blState == BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK) {
        if(block == currentOTABlock) {
          blState = BOOTLOAD_STATE_SENDING_IMAGE;
        } else {
          bl_print("RX other ACK seq %x, expected seq %x\r\n",
                   block, currentOTABlock);
        }
      } else if(blState == BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK) {
        if(block == currentOTABlock) {
          blState = BOOTLOAD_STATE_DONE;
        }
        bl_print("RX EOT ACK seq %x, expected seq %x\r\n",
                 block, currentOTABlock);
      } else {
        debug("Unexpected blState %d, ACK dropped\r\n",
              blState);
      }
      break;
    case XMODEM_NAK:
      block = myBuf[OFFSET_BLOCK_NUMBER];
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
          bl_print("bootload message from unknown node: ");
          printEui(longId);
          bl_print("; aborting!\r\n");
          // Get out from bootload mode
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          return;
        }

        // Check if protocol version is valid
        if ((version != BOOTLOAD_PROTOCOL_VERSION))
        {
          bl_print("RX bad packet, invalid protocl version\r\n");
          return;
        }

        bl_print("RX QUERY RESP from ");
        printEui(longId);
        bl_print(" with rssiValue %d\r\n", lastHopRssi);

        // Initiate xmodem transfer with the host PC for passthru or start
        // reading flash for clone bootloading
        if(blMode == BOOTLOAD_MODE_PASSTHRU) {
          blState = BOOTLOAD_STATE_START_SENDING_IMAGE;
          setActionTimer(0);
          emberSerialPrintf(appSerial,
                            "Please start .ebl upload image ...\r\n");
        } else {
          emberSerialPrintf(appSerial,
                            "Unexpected blMode=%d; aborting!\r\n",
                            blMode);
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          return;
        }
      } else {
        // In case of normal NAK to data packet, we stay in the same state and
        // wait for next retry
        debug("RX NAK block %x\r\n", block);
      }
      break;
    case XMODEM_CANCEL:
      if (!isTheSameEui64(targetAddress, longId)) {
        int32u deltaTime;
        int8u str[20];

        deltaTime = halCommonGetInt32uMillisecondTick() - queryStart;
        formatFixed(str, (int32s)deltaTime, 6, 3, FALSE);
        emberSerialPrintf(appSerial,"\r\n@%s secs:\r\n", str);
        emberSerialWaitSend(appSerial);

        emberSerialPrintf(appSerial,
                          "Remote requested abort=0x%x; from unexpected node: ",
                          myBuf[OFFSET_ERROR_TYPE]);
        printLittleEndianEui64(appSerial, longId);
        emberSerialPrintf(appSerial, " but I expect ");
        printLittleEndianEui64(appSerial, targetAddress);
        emberSerialPrintf(appSerial, "\r\n");
        emberSerialWaitSend(appSerial);
        break;
      }
      if (blState != BOOTLOAD_STATE_NORMAL) {
        emberSerialPrintf(appSerial,
                          "Remote requested abort=0x%x; aborting!\r\n",
                          myBuf[OFFSET_ERROR_TYPE]);
      } else {
        debug("Remote requested abort=0x%x; aborting!\r\n",
              myBuf[OFFSET_ERROR_TYPE]);
      }

      if (messageLength >= OFFSET_ERROR_BLOCK)
        debug("  on block=%d.\r\n", myBuf[OFFSET_ERROR_BLOCK]);

      idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
      currentRetriesRemaining = 0;
      break;
    default:
      debug("Unknown bootload type %x\r\n", type);
      break;
    }
  }
}

// Callback
// A callback invoked by the EmberZNet stack when the MAC has finished
// transmitting a bootload message.
void ezspBootloadTransmitCompleteHandler(
      // An EmberStatus value of EMBER_SUCCESS if an ACK was received from the
      // destination or EMBER_DELIVERY_FAILED if no ACK was received.
      EmberStatus status,
      // The length of the messageContents parameter in bytes.
      int8u messageLength,
      // The message that was sent.
      int8u *messageContents)
{
#if defined(ENABLE_DEBUG)
  int8u type;

  if (messageLength >= 2) {
    type = messageContents[OFFSET_MESSAGE_TYPE];
    if(status == EMBER_SUCCESS) {
      debug("message 0x%x is sent successfully", type);
    } else {
      debug("message 0x%x is failed to send", type);
    }
  }
  printDebugEOL();
  debugWaitSend(DEMO_DEBUG_PORT);
#endif
}

// end Ember callback handlers
// *******************************************************************


// -------------------------------------------------------------------------
// Bootload State Machine
void bootloadUtilTick(void)
{
  int16u time;
  int32u deltaTime;
  int8u str[20];
#ifdef ENABLE_DEBUG
  int8u messLen;
  int8u mess[40];
  int32u nextTime;
  int8u myDebugPort = BOOTLOAD_DEBUG_PORT;
#endif

#ifndef GATEWAY_APP
  if (ticksOn & BOOTTICKON) bootTick();         // Allow the boot  to run
#endif

  time = halCommonGetInt16uMillisecondTick();

#ifdef ENABLE_DEBUG
  bootloadEzspLastError = EZSP_SUCCESS;
  if (ticksOn & EMBERTICKON) {
#ifdef DEBUG_INT_VUART
    messLen = ezspSerialRead(sizeof(mess), mess);
#else
    messLen = 0;
#endif
  } else {
    messLen = 0;
  }
  if ((messLen == 0) && (DEMO_DEBUG_PORT != BOOTLOAD_DEBUG_PORT)) {
    myDebugPort = DEMO_DEBUG_PORT;
    if (emberSerialReadByte(DEMO_DEBUG_PORT, &mess[0]) == 0) {
      messLen = 1;
      bootloadEzspLastError = EZSP_SUCCESS;
    }
  }
  if (!bootloadEzspLastError && messLen) {
    nextTime = halCommonGetInt32uMillisecondTick();
    deltaTime = nextTime - lastIBMtime;
    lastIBMtime = nextTime;
    formatFixed(str, (int32s)deltaTime, 6, 3, FALSE);
    debugPrintf(myDebugPort,
                "@%s HOST[%d] at=%d, bs=%d bm=%d t=%d, lbt=%d\r\n",
                str, messLen, actionTimer, blState, blMode, time,
                lastBootTime);
#ifdef GATEWAY_APP
    fprintf(stderr, "@%s HOST[%d]\n", str, messLen);
#endif
    switch(mess[0]) {
    case '1':
      // allow interrupts
      INTERRUPTS_ON();
      break;
#if defined(AVR_ATMEGA_32)
    case '8':
      last_known_program_counter = BOOTLOADER_APP_REQUEST_KEY;
#endif
    case '9':
      halReboot();
      break;
    case '\r':
    case '\n':
    default:
      break;
    }
  }
#endif


  if ( (int16s)(time - lastBootTime) > 200 /*ms*/ ) {
    lastBootTime = time;

    halResetWatchdog();

    if (actionTimer == 0) {
      switch (blState)
      {
      case BOOTLOAD_STATE_WAIT_FOR_AUTH_CHALLENGE:
        // The authentication challenge was not received in time.
        // We need to destroy the AES key.
        bl_print("do not receive auth challenge\r\n");
        MEMSET(signingKey, 0, sizeof(signingKey));
        // Abort this bootloader session.
        idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
        break;
      case BOOTLOAD_STATE_WAIT_FOR_AUTH_RESPONSE:
        // The authentication response was not received in time.  We
        // need to free the challenge buffer.
        bl_print("do not receive auth response\r\n");
        MEMSET(challenge, 0, sizeof(challenge));
        // Abort this bootloader session.
        idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
        break;
      case BOOTLOAD_STATE_DELAY_BEFORE_START:
        // We have given the target node enough time to launch the
        // bootloader.  Now we start the actual bootloader
        // procedure.
        bl_print("start bootload\r\n");
        currentRetriesRemaining = NUM_PKT_RETRIES;
        bootloadUtilStartBootload(targetAddress, blMode);
        break;
      case BOOTLOAD_STATE_QUERY:
        // Stop receiving query response
        idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
        break;
      case BOOTLOAD_STATE_START_UNICAST_BOOTLOAD:
      case BOOTLOAD_STATE_START_BROADCAST_BOOTLOAD:
        if (currentRetriesRemaining > 0) {
          bl_print("--> start bootload retry %d\r\n",
                   currentRetriesRemaining);
          currentRetriesRemaining -= 1;
          if(blState == BOOTLOAD_STATE_START_UNICAST_BOOTLOAD) {
            sendRadioMessage(XMODEM_QUERY, FALSE);
          } else {
            sendRadioMessage(XMODEM_QUERY, TRUE);
          }
          setActionTimer(TIMEOUT_QUERY);
        } else {
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          bl_print("ERROR: do not see start bootload response\r\n");
        }
        break;
      case BOOTLOAD_STATE_START_SENDING_IMAGE:
        blState = BOOTLOAD_STATE_SENDING_IMAGE;
        setActionTimer(TIMEOUT_IMAGE_SEND);

        deltaTime = halCommonGetInt32uMillisecondTick() - passThruStart;
        formatFixed(str, (int32s)deltaTime, 6, 3, FALSE);
        emberSerialPrintf(appSerial,"\r\nSetup took %s secs\r\n",
                          str);
        emberSerialWaitSend(appSerial);

        xmodemReceiveAndForward();
        deltaTime = halCommonGetInt32uMillisecondTick() - passThruStart;
        formatFixed(str, (int32s)deltaTime, 6, 3, FALSE);
        emberSerialPrintf(appSerial,"\r\nPassthru took %s secs\r\n",
                          str);
        emberSerialWaitSend(appSerial);

        break;
      case BOOTLOAD_STATE_WAIT_FOR_COMPLETE_ACK:
      case BOOTLOAD_STATE_WAIT_FOR_IMAGE_ACK:
        // We didn't get an ack.  Retry (at the application level)
        // in the hopes that it was a temporary problem.
        if (currentRetriesRemaining > 0) {
          bl_print("data retry %x\r\n", currentRetriesRemaining);
          currentRetriesRemaining -= 1;
          setActionTimer(TIMEOUT_IMAGE_SEND);
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
            idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          }
        }
        break;
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
  }     //end check time
}

// -------------------------------------------------------------------------
// Initiate bootloading process with remote node that is already in bootload
// mode.  The function is normally used to perform bootload recovery on the
// nodes that have failed to bootload.  If failure happens during bootload,
// the nodes will be in bootload mode and stay on the current channel, if
// it has not been power cycled.
void bootloadUtilStartBootload(EmberEUI64 target, bootloadMode mode)
{

#ifndef GATEWAY_APP
  if (isTheSameEui64(target, emberGetEui64())) {
    if (!(ticksOn & BOOTTICKON))
      prevTicksOn = ticksOn;
    ticksOn = BOOTTICKON;
  }
#endif

  currentRetriesRemaining = NUM_PKT_RETRIES;
  setActionTimer(TIMEOUT_QUERY);

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


#ifdef EMHALCRC16
// emHalCrc16 - for XModem block validation
//
// This CRC seems to take about the same amount of time as the table driven CRC
// which was timed at 34 cycles on the mega128 (8.5us @4MHz).
static int16u emHalCrc16( int8u byte, int16u crc  )
{
  crc = (crc >> 8) | (crc << 8);
  crc ^= byte;
  crc ^= (crc & 0xff) >> 4;
  crc ^= (crc << 8) << 4;

  // What I wanted is the following function:
  // crc ^= ((crc & 0xff) << 4) << 1;
  // Unfortunately the compiler does this in 46 cycles.  The next line of code
  // does the same thing, but the compiler uses only 10 cycles to implement
  // it.
  crc ^= ( (int8u) ( (int8u) ( (int8u) (crc & 0xff) ) << 5)) |
    ((int16u) ( (int8u) ( (int8u) (crc & 0xff)) >> 3) << 8);

  return crc;
}
#endif

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
      halCommonDelayMicroseconds(1);
    }
    if ((int16s)(halCommonGetInt16uMillisecondTick() - time) > 100 /*ms*/) {
      time += 100;
      timeout -= 1;
      ezspTick();  // Needed to keep ASH happy.
    }
  }

  return NO_SERIAL_DATA;
}

// This function is used to read bootload serial port one block at a time.
// XModem protocol specifies its data block length to be 128 bytes plus
// 2 bytes crc and appropriate header bytes.  The function validates the header
// and the crc and return appropriate value.
static int8u xmodemReceiveBlock(void)
{
  int8u header=0, blockNumber=0, blockCheck=0, crcLow=0, crcHigh=0;
  int8u ii, temp;
  int16u crc = 0x0000;
  int8u status = STATUS_NAK;

  if (xmodemReadSerialByte(SERIAL_TIMEOUT_3S, &header) == NO_SERIAL_DATA) {
    bl_print("xmodem: timeout\r\n");
    return STATUS_RESTART;
  }

  if ((header == XMODEM_CANCEL) || (header == XMODEM_CC)) {
    bl_print("xmodem: restart\r\n");
    return STATUS_RESTART;
  }

  if ((header == XMODEM_EOT) &&
      (xmodemReadSerialByte(SERIAL_TIMEOUT_3S, &temp) == NO_SERIAL_DATA)) {
    bl_print("xmodem: done\r\n");
    return STATUS_DONE;
  }
  
  if ((header != XMODEM_SOH)
    || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &blockNumber) != SERIAL_DATA_OK)
    || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &blockCheck)  != SERIAL_DATA_OK)) {
    emberSerialFlushRx(bootloadSerial);
    bl_print("xmodem: NAK\r\n");
    return STATUS_NAK;
  }

  if (blockNumber + blockCheck == 0xFF) {
    if (blockNumber == expectedBlockNumber) {
      status = STATUS_OK;
    } else if (blockNumber == (int8u)(expectedBlockNumber - 1)) {
      // Sender probably missed our ack so ack again.  Before returning, we
      // also want to read out the rest of the duplicate packet.
      bl_print("xmodem: duplicate block %x\r\n", blockNumber);
      while(xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &xmodemBlock[0]) == SERIAL_DATA_OK) {}
      return STATUS_DUPLICATE;
    } // end check for duplicate 
  }
  
  // Read more data.
  for (ii = 0; ii < XMODEM_BLOCK_SIZE; ii++) {
    if (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &xmodemBlock[ii]) != SERIAL_DATA_OK) {
      bl_print("xmodem: timeout at %d\r\n", ii);
      return STATUS_NAK;
    }
    crc = halCommonCrc16(xmodemBlock[ii], crc);
  }
  // Read and validate crc.
  if ((xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &crcHigh) != SERIAL_DATA_OK)
      || (xmodemReadSerialByte(SERIAL_TIMEOUT_1S, &crcLow) != SERIAL_DATA_OK)
      || (crc != HIGH_LOW_TO_INT(crcHigh, crcLow))) {
      bl_print("xmodem: crc issue\r\n");
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
  boolean xmodemStart = FALSE;
  int8u counter=0, i;
  expectedBlockNumber = 1;
  int8u status = STATUS_OK;

  while(blState != BOOTLOAD_STATE_NORMAL &&
        blMode != BOOTLOAD_MODE_NONE) {

    halResetWatchdog();
    xmodemSendReady();
    // Determine if the PC has cancelled the session.  We wait ~ 10 s
    // which is the same as xmodem (retry) timeout.
    ++counter;
    if(xmodemStart && counter > 100) {
      bl_print("Error: PC stops sending us data\r\n");
      idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
      return;
    }

    do {
      int8u outByte;
      status = xmodemReceiveBlock();

      switch (status)
      {
      case STATUS_OK:
        if (!xmodemStart) {
          passThruStart = halCommonGetInt32uMillisecondTick();

          // When receive first good packet, that means we have 'officially'
          // started XModem transfer.
          xmodemStart = TRUE;
        }
        // Forward data to the target node
        if(!bootloadSendImage(XMODEM_SOH)) {
          bl_print("Error: cannot forward data to target node\r\n");
          // Cancel XModem session with the host (PC)
          outByte = XMODEM_CANCEL;
          emberSerialFlushRx(bootloadSerial);
          for(i=0; i<5; ++i) {
            emberSerialWriteByte(bootloadSerial, outByte);
          }
          return;
        }
        expectedBlockNumber++;
        counter = 0; // reset counter after receiving a good packet
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
          bl_print("Error: cannot forward complete signal to target node\r\n");
          idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
          return;
        }
        emberSerialWriteByte(bootloadSerial, XMODEM_ACK);
#ifndef GATEWAY_APP
        if (ticksOn & BOOTTICKON) {
          int16u j;
          EzspStatus stat;

          stat = waitFor260boot(&j);
          if (stat != EZSP_SUCCESS) 
            emberSerialPrintf(appSerial, "em260 reboot(%d) failed!\r\n", j);
          else
            emberSerialPrintf(appSerial, "em260 reboot(%d) passed!\r\n", j);
        }
#endif
        idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
        emberSerialPrintf(appSerial, "Bootload Complete!\r\n");
        bl_print("bootload complete\r\n");
        return;
      default:
        continue;
      }
      emberSerialFlushRx(bootloadSerial);
      emberSerialWriteByte(bootloadSerial, outByte);
    } while (status != STATUS_RESTART);
  }
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

// This function determines if a node is sleepy (or mobile) by checking to see
// if it's eui is found in the child table.
static boolean isSleepy(EmberEUI64 eui)
{
  int8u i;
  EmberNodeId id;
  EmberEUI64 childEui64;
  EmberNodeType type;

  for (i = 0; i < EMBER_MAX_END_DEVICE_CHILDREN; i++) {
    if ( (EMBER_SUCCESS == ezspGetChildData(i, &id, childEui64, &type) )
       &&(0 == MEMCOMPARE(eui, childEui64, EUI64_SIZE) ) ) {
      return TRUE;
    }
  }
  return FALSE;
}

// The function is called to send (new application) data to the target node
// over the air.  Note that the source and the target nodes need to be one
// hop away from each other and have good link to each other to ensure
// quick and successful bootloading.
static boolean bootloadSendImage(int8u type)
{
  int8u block[2];
  int8u i;
  int8u iMax;

#ifndef GATEWAY_APP
  if (ticksOn & BOOTTICKON) {
    block[0] = expectedBlockNumber;
    iMax = 1;
  } else {
#endif
    // Send 128 bytes of XModem data in two
    // 64-byte radio packets
    block[0] = ((2 * expectedBlockNumber) - 1);
    block[1] = (2 * expectedBlockNumber);
    iMax = 2;
#ifndef GATEWAY_APP
  }
#endif

  for(i=0; i<iMax; ++i) {
    debug("sending OTA block %x\r\n", block[i]);
    currentRetriesRemaining = NUM_PKT_RETRIES;
    currentOTABlock = block[i];
    setActionTimer(TIMEOUT_IMAGE_SEND);
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
      if (ticksOn & EMBERTICKON) emberTick();       // Allow the stack to run
      if (ticksOn & EZSPTICKON) ezspTick();         // Allow the ezsp  to run
#ifndef GATEWAY_APP
      if (ticksOn & BOOTTICKON) bootTick();         // Allow the boot  to run
#endif
      bootloadUtilTick();
      emberSerialBufferTick();
    }
    if(blState == BOOTLOAD_STATE_NORMAL) {
      emberSerialPrintf(appSerial, "ERROR: Image send OTA failed");
      printEOL();
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

// This function actually crafts and sends out the radio packets.  XMODEM_SOH
// is the data packet and XMODEM_EOT is the end of transmission packet.
static void sendRadioMessage(int8u messageType, boolean isBroadcast)
{
  // allow space for the longest possible message
  int8u length=0, index, i, startByte, data, iMax;
  int16u crc = 0x0000;

#ifndef GATEWAY_APP
  if (ticksOn & BOOTTICKON) {
    iMax = BOOTLOAD_OTA_SIZE + BOOTLOAD_OTA_SIZE;
  } else {
#endif
    iMax = BOOTLOAD_OTA_SIZE;
#ifndef GATEWAY_APP
  }
#endif

  // for convenience, we form the message in a byte array first.
  index = bootloadMakeHeader(bootloadBuffer, messageType);

  switch (messageType)
  {
  case XMODEM_QRESP:
    {
      // {1:blActive(0)}
      bootloadBuffer[index++] = FALSE;
      // {2:mfgId}
      // TOKEN_MFG_MANUF_ID has index 3
      ezspGetMfgToken(3, (int8u*)&bootloadBuffer[index]);

      // TOKEN_MFG_MANUF_ID is size 2
      index += 2;
      // {16:hwTag}

      // TOKEN_MFG_BOARD_NAME has index 2
      ezspGetMfgToken(2, (int8u*)&bootloadBuffer[index]);

      // TOKEN_MFG_BOARD_NAME is size 16
      index += 16;
      // {1:blCapabilities}
      // In the future, we should read the capabilities mask from fixed
      // location in bootloader.  However, currently we do not have any
      // bootload capabilities implemented.  This filed is included for
      // future use.
      bootloadBuffer[index++] = 0;
      bootloadBuffer[index++] = nodePlat;
      bootloadBuffer[index++] = nodeMicro;
      bootloadBuffer[index++] = nodePhy;
      // {2:blVersion}
      bootloadBuffer[index++] = HIGH_BYTE(nodeBlVersion); // bootloader version
      bootloadBuffer[index++] = LOW_BYTE(nodeBlVersion); // bootloader build
      length = index;
    }
    break;
  case XMODEM_SOH:
    bootloadBuffer[index++] = currentOTABlock;
    bootloadBuffer[index++] = 0xFF - currentOTABlock;
#ifndef GATEWAY_APP
    if ( (ticksOn & BOOTTICKON) || (currentOTABlock & 0x01) ) {
#else
    if (currentOTABlock & 0x01) {
#endif
      startByte = 0;
    } else {
      startByte = 64;
    }
    for(i = index; i<(iMax + index); ++i) {
      data = xmodemBlock[startByte++];
      bootloadBuffer[i] = data;
      crc = halCommonCrc16(data, crc);
    }
    // Include crc in the packet
    bootloadBuffer[i++] = HIGH_BYTE(crc);
    bootloadBuffer[i++] = LOW_BYTE(crc);
    length = i;
    break;
  case XMODEM_EOT:
  case XMODEM_QUERY:
    length = index;
    break;
  default:
    return;
  }

  halResetWatchdog(); // Give a full time for SPI...
#ifndef GATEWAY_APP
  if (ticksOn & BOOTTICKON) {
    if (bootSendMessage(length, bootloadBuffer) != EZSP_SUCCESS) {
      bl_print("boot send bootload msg failed");
      printDebugEOL();
      idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
    }
  } else {
#endif
    if ( ezspSendBootloadMessage(isBroadcast, targetAddress, length, bootloadBuffer)
         != EMBER_SUCCESS ) {
      bl_print("send bootload msg failed");
      printDebugEOL();
      idleBootloadUtils(); // back to BOOTLOAD_STATE_NORMAL
    }
#ifndef GATEWAY_APP
  }
#endif
}

// -------------------------------------------------------------------------
// Send bootload query.  The function is called by the source node to get
// necessary information about nodes.
void bootloadUtilSendQuery(EmberEUI64 target)
{

#ifndef GATEWAY_APP
  if (isTheSameEui64(target, emberGetEui64())) {
    if (!(ticksOn & BOOTTICKON))
      prevTicksOn = ticksOn;
    ticksOn = BOOTTICKON;
  }
#endif

  MEMCOPY(targetAddress, target, EUI64_SIZE);
  if(isTheSameEui64(broadcastEui64, target)) {
    sendRadioMessage(XMODEM_QUERY, TRUE);
  } else {
    sendRadioMessage(XMODEM_QUERY, FALSE);
  }
  blState = BOOTLOAD_STATE_QUERY;
  setActionTimer(TIMEOUT_QUERY);
}

static void printEui(EmberEUI64 eui)
{
  bl_print("%x%x%x%x%x%x%x%x",
           eui[0], eui[1], eui[2], eui[3],
           eui[4], eui[5], eui[6], eui[7]);
}

static void printDebugEOL(void)
{
  debugPrintf(DEMO_DEBUG_PORT, "\r\n");
}

static void printEOL(void)
{
  emberSerialPrintf(appSerial, "\r\n");
}

// The initial < or > is meant to indicate the endian-ness of the EUI64
// '>' is big endian (most significant first)
// '<' is little endian (least significant first)

void printLittleEndianEui64(int8u port, EmberEUI64 eui64)
{
  debugPrintf(port, "(<)%X%X%X%X%X%X%X%X",
                    eui64[0], eui64[1], eui64[2], eui64[3],
                    eui64[4], eui64[5], eui64[6], eui64[7]);
}

void printBigEndianEui64(int8u port, EmberEUI64 eui64)
{
  debugPrintf(port, "(>)%X%X%X%X%X%X%X%X",
                    eui64[7], eui64[6], eui64[5], eui64[4],
                    eui64[3], eui64[2], eui64[1], eui64[0]);
}

// End utility functions
// *******************************************************************
