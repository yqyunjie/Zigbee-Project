// *******************************************************************
//  mfg-sample-tokens.c
//
//  token functions for use with mfglib 
//
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// The following is needed for sscan_P
#ifdef __ICCAVR__
#include <pgmspace.h> // sscanf_P
#else
#define sscanf_P sscanf
#endif

#include PLATFORM_HEADER //compiler/micro specifics, types
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/serial/serial.h"
#include "app/util/serial/cli.h"

#ifdef GATEWAY_APP
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include <unistd.h>
#endif


int8u serialPort = APP_SERIAL;

// the manufacturing tokens are enumerated in app/util/ezsp/ezsp-protocol.h
// the names are enumerated here to make it easier for the user
PGM_NO_CONST PGM_P ezspMfgTokenNames[] =
  {
    "EZSP_MFG_CUSTOM_VERSION..",
    "EZSP_MFG_STRING..........",
    "EZSP_MFG_BOARD_NAME......",
    "EZSP_MFG_MANUF_ID........",
    "EZSP_MFG_PHY_CONFIG......",
    "EZSP_MFG_BOOTLOAD_AES_KEY",
    "EZSP_MFG_ASH_CONFIG......",
    "EZSP_MFG_EZSP_STORAGE...."
  };

// the number of tokens that can be written using ezspSetToken and read
// using ezspGetToken
#define MFGSAMP_NUM_EZSP_TOKENS 8
// the size of the tokens that can be written using ezspSetToken and
// written using ezspGetToken
#define MFGSAMP_EZSP_TOKEN_SIZE 8
// the number of manufacturing tokens 
#define MFGSAMP_NUM_EZSP_MFG_TOKENS 8
// the size of the largest EZSP Mfg token
#define MFGSAMP_EZSP_TOKEN_MFG_MAXSIZE 40

// ******************************
// mfgappTokenDump
// ******************************
// called to dump all of the tokens. This dumps the indices, the names, 
// and the values using ezspGetToken and ezspGetMfgToken. The indices 
// are used for read and write functions below.
//
void mfgappTokenDump(void)
{
  EmberStatus status;
  int8u tokenData[MFGSAMP_EZSP_TOKEN_MFG_MAXSIZE];
  int8u index, i, tokenLength;

  // first go through the tokens accessed using ezspGetToken
  emberSerialPrintf(serialPort,"(data shown little endian)\r\n");
  emberSerialPrintf(serialPort,"Tokens:\r\n");
  emberSerialPrintf(serialPort,"idx  value:\r\n");
  for(index=0; index<MFGSAMP_NUM_EZSP_TOKENS; index++) {

    // get the token data here
    status = ezspGetToken(index, tokenData);
    emberSerialPrintf(serialPort,"[%d]", index);
    if (status == EMBER_SUCCESS) {

      // Print out the token data
      for(i = 0; i < MFGSAMP_EZSP_TOKEN_SIZE; i++) {
        emberSerialPrintf(serialPort, " %X", tokenData[i]);
      }

      emberSerialWaitSend(serialPort);
      emberSerialPrintf(serialPort,"\r\n");
    }
    // handle when ezspGetToken returns an error
    else {
      emberSerialPrintf(serialPort," ... error 0x%x ...\r\n",
                        status);
    }
  }

  // now go through the tokens accessed using ezspGetMfgToken
  // the manufacturing tokens are enumerated in app/util/ezsp/ezsp-protocol.h
  // this file contains an array (ezspMfgTokenNames) representing the names.
  emberSerialPrintf(serialPort,"Manufacturing Tokens:\r\n");
  emberSerialPrintf(serialPort,
                    "idx  token name                 len   value\r\n");
  for(index=0; index<MFGSAMP_NUM_EZSP_MFG_TOKENS; index++) {

    // ezspGetMfgToken returns a length, be careful to only access
    // valid token indices.
    tokenLength = ezspGetMfgToken(index, tokenData);
    emberSerialPrintf(serialPort,"[%x] %p: 0x%x:", index,
                      ezspMfgTokenNames[index], tokenLength);

    // Print out the token data
    for(i = 0; i < tokenLength; i++) {
      if ((i != 0) && ((i % 8) == 0)) {
        emberSerialPrintf(serialPort,
                          "\r\n                                    :");
        emberSerialWaitSend(serialPort);
      }
      emberSerialPrintf(serialPort, " %X", tokenData[i]);
    }
    emberSerialWaitSend(serialPort);
    emberSerialPrintf(serialPort,"\r\n");
  }
  emberSerialPrintf(serialPort,"\r\n");
}

// ******************************
// mfgappTokenRead
// ******************************
// this uses ezspGetToken to read the token specified by the index passed in.
// 
void mfgappTokenRead(int8u index)
{
  EmberStatus status;
  int8u tokenData[MFGSAMP_EZSP_TOKEN_SIZE];
  int8u i;

  // get the token data
  status = ezspGetToken(index, tokenData);
  emberSerialPrintf(serialPort,"[%d]", index);
  if (status == EMBER_SUCCESS) {
    // Print out the token data
    for(i = 0; i < MFGSAMP_EZSP_TOKEN_SIZE; i++) {
      emberSerialPrintf(serialPort, " %X", tokenData[i]);
    }
    emberSerialWaitSend(serialPort);
    emberSerialPrintf(serialPort,"\r\n\r\n");
  } else {
    emberSerialPrintf(serialPort," ... error 0x%x ...\r\n\r\n",
                      status);
  }
}

// ******************************
// mfgappTokenReadMfg
// ******************************
// this uses ezspGetMfgToken to read the token specified by the index passed in.
// 
void mfgappTokenReadMfg(int8u index)
{
  int8u tokenData[MFGSAMP_EZSP_TOKEN_MFG_MAXSIZE];
  int8u i, tokenLength;

  // get the token data
  tokenLength = ezspGetMfgToken(index, tokenData);

  // error check since we could have passed an invalid index to ezspGetMfgToken
  if (index < MFGSAMP_NUM_EZSP_MFG_TOKENS) {
    emberSerialPrintf(serialPort,"[%x] %p: 0x%x:", index,
                      ezspMfgTokenNames[index], tokenLength);

    // Print out the token data 
    for(i = 0; i < tokenLength; i++) {
      if ((i != 0) && ((i % 8) == 0)) {
        emberSerialPrintf(serialPort,
                          "\r\n                                    :");
        emberSerialWaitSend(serialPort);
      }
      emberSerialPrintf(serialPort, " %X", tokenData[i]);
    }
  }

  // handle errors (index out of bounds)
  else {
    emberSerialPrintf(serialPort,"[%x] OUT OF BOUNDS: 0x%x", index,
                      tokenLength);
  }

  emberSerialWaitSend(serialPort);
  emberSerialPrintf(serialPort,"\r\n\r\n");
}

// this is a utility function used by mfgappTokenWrite
void myReadLine(int8u port, char *data, int8u max)
{
  int8u index = 0;
#ifdef GATEWAY_APP
  while(emberSerialReadLine(port, data, max) != EMBER_SUCCESS)
#else
  while(emberSerialReadPartialLine(port, data, max, &index) != EMBER_SUCCESS)
#endif
  {
    halResetWatchdog();         // Periodically reset the watchdog

    // Allow the stack and ezsp to run
    ezspTick();
    while (ezspCallbackPending()) {
      ezspSleepMode = EZSP_FRAME_CONTROL_IDLE;
      ezspCallback();
    }

  }
  
}

// ******************************
// mfgappTokenWrite
// ******************************
//
// This sets the token specified by index to a value read in from the user.
// This uses ezspSetToken to set the token.
//
void mfgappTokenWrite(int8u tokIndex)
{
  //Each data entry is 2 bytes plus a terminating character.
  char userInput[3];
  int8u i;
#ifdef GATEWAY_APP
  unsigned int word;
#else
  int16u word;
#endif
  EmberStatus status;
  int8u data[MFGSAMP_EZSP_TOKEN_SIZE];

  // Get the token so we can show the user what the previous data is.
  // Checking the status of this call is a good check that we are 
  // accessing a valid token
  status = ezspGetToken(tokIndex, data);

  if (status == EMBER_SUCCESS) {
    // Gather data bytes from the user
    for(i = 0; i < MFGSAMP_EZSP_TOKEN_SIZE; i++) {
      emberSerialPrintf(serialPort, "data %X byte[%d]:",
                        data[i], i);
      myReadLine(serialPort, userInput, sizeof(userInput));
      if (userInput[0]) {
        sscanf_P(userInput, "%x", &word);
        data[i] = (word&0xFF);
      }
    }

    // set the token
    ezspSetToken(tokIndex, data);
  } 
  // handle error case where we could not read the token at index 
  else {
    emberSerialPrintf(serialPort,"Failed to get index %2X: 0x%x\r\n",
                      tokIndex, status);
  }
  emberSerialWaitSend(serialPort);
}
