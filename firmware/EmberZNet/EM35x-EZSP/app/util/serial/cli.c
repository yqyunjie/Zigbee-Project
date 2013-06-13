// *******************************************************************
// cli.c
//
// Simple command line interface (CLI) for use with Ember applications.
// See cli.h for more information.
//
// The cli library is deprecated and will be removed in a future release.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


#include "app/util/serial/cli.h"
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"

// this variable is set by the user of this library with a call to cliInit
// if it isnt set it starts out at 1
int8u cliSerialPort = 1;


// storage for cmd line interaction
int8u serialBuff[CLI_MAX_NUM_SERIAL_ARGS][CLI_MAX_SERIAL_CMD_LINE];
int8u serialBuffPosition[CLI_MAX_NUM_SERIAL_ARGS];
int8u currentSerialBuff = 0;

// functions internal to this library
void processSerialLine(void);
void clearSerialLine(void);

// ***********************************
// initialize the correct serial port
// ***********************************
void cliInit(int8u serialPort)
{
  cliSerialPort = serialPort;
}


// ***********************************
// if a debug mode is defined, turn on printing of CLI state
// ***********************************
#ifdef APP_UTIL_CLI_DEBUG
void printCli(void)
{
  int8u i,j;

  // print the curresnt serial buffer (the one being written to)
  emberSerialPrintf(cliSerialPort, "-------------------------\r\n");
  emberSerialPrintf(cliSerialPort, "currentSerialBuff = %x\r\n", 
                    currentSerialBuff);
  emberSerialWaitSend(cliSerialPort);

  // print the contents of each serial buffer: buffNum, current position, and
  // the values of each slot in the buffer
  for (i=0; i<CLI_MAX_NUM_SERIAL_ARGS; i++) {
    emberSerialPrintf(cliSerialPort,"%x:%x: ", i, serialBuffPosition[i]);
    
    for (j=0; j<CLI_MAX_SERIAL_CMD_LINE; j++){
      emberSerialPrintf(cliSerialPort,"%x ", serialBuff[i][j]);
      emberSerialWaitSend(1);
    }
    emberSerialPrintf(1,"\r\n");
    emberSerialWaitSend(1);
  }
}
#else //APP_UTIL_CLI_DEBUG
// without debugging just remove the prints
  #define printCli() do { ; } while(FALSE)
#endif //APP_UTIL_CLI_DEBUG

// **********************************
// this should be called by users in their main loop. It processes serial
// characters on the port set in cliInit.
// **********************************
void cliProcessSerialInput(void) 
{
  int8u ch;
  int8u pos;

  if(emberSerialReadByte(cliSerialPort, &ch) == 0) {

    switch(ch) {

    case '\r':
      // terminate the line
      printCli();
      processSerialLine();
      break;

    case '\n':
      // terminate the line
      printCli();
      processSerialLine();
      break;

    case ' ':
      // echo
      emberSerialPrintf(cliSerialPort, "%c",ch);

      // ignore leading spaces by checking if we are at the start of the
      // buffer. This allows commands to be separated by more than 1 space.
      if (serialBuffPosition[currentSerialBuff] != 0) {
        // switch to next buffer
        currentSerialBuff = currentSerialBuff + 1;
        // check for too many args
        if (currentSerialBuff == CLI_MAX_NUM_SERIAL_ARGS) {
          emberSerialPrintf(cliSerialPort, "\r\nOUT OF SERIAL CMD BUFFER\r\n");
          // just clear the line
          clearSerialLine();
        }
        printCli();
      }
      break;
    default:
      // otherwise append to the buffer

      // echo
#if !defined (GATEWAY_APP)
      emberSerialPrintf(cliSerialPort, "%c",ch);
#endif

      // find the position
      pos = serialBuffPosition[currentSerialBuff];
      if (pos < CLI_MAX_SERIAL_CMD_LINE) {
        // add the character
        serialBuff[currentSerialBuff][pos] = ch;
        //emberSerialPrintf(cliSerialPort, "add %x to %x at %x\r\n", ch,
        //                  currentSerialBuff, pos);
        // increment the pos
        serialBuffPosition[currentSerialBuff] =
          serialBuffPosition[currentSerialBuff] + 1;
      } else {
        emberSerialPrintf(cliSerialPort, "\r\nOUT OF SERIAL CMD SPACE\r\n");
        // just clear the line
        clearSerialLine();
      }
      printCli();
    }
  }

}

// **********************************
// internal to this library, called from cliProcessSerialInput
//
// this processes a line once a return has been hit. It looks for a matching
// command, and if one is found, the corresponding function is called.
// **********************************
void processSerialLine(void) 
{
  int8u i,j,commandLength;
  int8u match = TRUE;
  int8u commandLengthByte;

#if !defined (GATEWAY_APP)
  emberSerialPrintf(cliSerialPort, "\r\n");
#endif

  // ... processing ...
  if (serialBuffPosition[0] > 0) {
    // go through each command
    for (i=0; i<cliCmdListLen; i++) {

      // get the command
      PGM_P command = cliCmdList[i].cmd;

      // get the length of the cmd
      commandLength = 0;
      commandLengthByte = cliCmdList[i].cmd[0];
      while (commandLengthByte != 0) {
        commandLength++;
        commandLengthByte = cliCmdList[i].cmd[commandLength];
      }

      match = TRUE;
      // check for differences in the command and what was entered
      for (j=0; j<commandLength;j++) {
        if (command[j] != serialBuff[0][j]) {
          match = FALSE;
        }
      }

      // commands with 0 length are invalid
      if (commandLength < 1) {
        match = FALSE;
      }

      // if command matches command entered call the callback
      if (match == TRUE) {
        halResetWatchdog();
        (cliCmdList[i].action)();
        halResetWatchdog();
        i=cliCmdListLen+1;
      }

    }
    if (match == FALSE) {
      emberSerialPrintf(cliSerialPort, "command not recognized\r\n");
    }
  }

  clearSerialLine();
}


// **********************************
// internal to this library, called from cliProcessSerialInput
//
// clears all memory to 0.
// **********************************
void clearSerialLine(void) {
  int8u i;
  int8u j;

  for (j=0; j<CLI_MAX_NUM_SERIAL_ARGS; j++) {
    for (i=0; i<CLI_MAX_SERIAL_CMD_LINE; i++) {
      serialBuff[j][i] = 0;
    }
  }
  for (j=0; j<CLI_MAX_NUM_SERIAL_ARGS; j++) {
    serialBuffPosition[j] = 0;
  }
  currentSerialBuff = 0;

#if !defined(GATEWAY_APP)
  // This will be printed by the linux-serial.c code.
  emberSerialPrintf(cliSerialPort, "%p>", cliPrompt);
#endif
}


// **********************************
// this returns TRUE if the argument specified matches the keyword provided
// **********************************
boolean cliCompareStringToArgument(PGM_P keyword, int8u buffer) 
{
  int8u i = 0;

  // check that each byte of the keyword matches the buffer. If everything
  // matches and we have reached the end of the keyword, consider this a match
  while (TRUE) {
    if (*keyword == 0) {
      return TRUE;
    }
    else if (*keyword++ != serialBuff[buffer][i++]) {
      return FALSE;
    }
  }
}


static int8u hexCharToInt8u(int8u hexChar)
{
  if ('0' <= hexChar && hexChar <= '9')
    return hexChar - '0';
  else if ('a' <= hexChar && hexChar <= 'f')
    return hexChar - 'a' + 10;
  else if ('A' <= hexChar && hexChar <= 'F')
    return hexChar - 'A' + 10;
  else
    return 0;
}

// **********************************
// this returns the nth byte from an argument entered as a hex string
// as a byte value
// **********************************
int8u cliGetHexByteFromArgument(int8u whichByte, int8u bufferIndex) 
{
  // looking for whichByte byte, so this means whichByte*2, (whichByte*2)+1
  // *** workaround for compiler issue ***
  //int8u *pointer = serialBuff[bufferIndex] + (whichByte * 2);
  int8u *pointer = serialBuff[bufferIndex];
  pointer += (whichByte * 2);
  // *** end workaround ***

  // error check
  if (bufferIndex > CLI_MAX_NUM_SERIAL_ARGS) {
    return 0;
  }

  return (hexCharToInt8u(pointer[1])
          + hexCharToInt8u(pointer[0]) * 16);
}

// **********************************
// This is the main worker function for reading decimal values from the CLI
// input strings. It reads a 32-bit unsigned value, and leaves the caller
// to handle any truncation or negation.
// **********************************
int32u cliGetInt32FromBuffer(int8u bufferIndex) 
{
  int8u i;
  int8u len = serialBuffPosition[bufferIndex];
  int32u value = 0;
  int32u digitMod = 1;
  int8u placeVal;
  int8u startPos = 0;

  int8u iMinusOne;

  // handle negative numbers
  if (serialBuff[bufferIndex][0] == '-') {
    startPos = 1;
  }

  for (i=len; i>startPos; i--) {
    // *** workaround for compiler issue ***
    //placeVal = (serialBuff[bufferIndex][i-1]) - '0';
    iMinusOne = i - 1;
    placeVal = (serialBuff[bufferIndex][iMinusOne]) - '0';
    // *** end workaround ***

    value = value + (digitMod * placeVal);
    digitMod = digitMod * 10;
  }

  return value;
}

// **********************************
// this returns an int16s from an argument entered as a string in decimal
// **********************************
int16s cliGetInt16sFromArgument(int8u bufferIndex) 
{
  int16s value;

  // error check
  if (bufferIndex > CLI_MAX_NUM_SERIAL_ARGS) {
    return 0;
  }

  // Cast a possible 32-bit value down to a 16-bit value.
  value = (int16s)cliGetInt32FromBuffer(bufferIndex);

  // handle negative numbers
  if (serialBuff[bufferIndex][0] == '-') {
    value = -value;
  }

  return value;
}

// **********************************
// this returns an int16u from an argument entered as a string in decimal
// **********************************
int16u cliGetInt16uFromArgument(int8u bufferIndex) 
{
  int16u value;

  // error check
  if (bufferIndex > CLI_MAX_NUM_SERIAL_ARGS) {
    return 0;
  }

  // Cast a possible 32-bit value down to a 16-bit value.
  value = (int16u)cliGetInt32FromBuffer(bufferIndex);

  // handle negative numbers (cast is to avoid a warning in xide)
  if (serialBuff[bufferIndex][0] == '-') {
    value = (int16u) (-value);
  }

  return value;
}

// **********************************
// This returns an int32u from an argument entered as a string in decimal
// **********************************
int32u cliGetInt32uFromArgument(int8u bufferIndex) 
{
  int32u value;

  // error check
  if (bufferIndex > CLI_MAX_NUM_SERIAL_ARGS) {
    return 0;
  }

  value = cliGetInt32FromBuffer(bufferIndex);

  // handle negative numbers (cast is to avoid a warning in xide)
  if (serialBuff[bufferIndex][0] == '-') {
    value = (int32u)(-value);
  }

  return value;
}


// **********************************
// this copies the string at the argument specified into the buffer passed 
// in. If the string being copied in is larger than maxBufferToFillLength, 
// then nothing is copied and 0 is returned. If the string being
// copied is smaller than maxBufferToFillLength, then the copy is done
// and TRUE is returned.
// **********************************
int8u cliGetStringFromArgument(int8u bufferIndex, 
                               int8u* bufferToFill,
                               int8u maxBufferToFillLength) 
{
  int8u len = serialBuffPosition[bufferIndex];

  // check that the buffer passed in is large enough
  if (len > maxBufferToFillLength) 
    return 0;

  // copy the string at the argument (bufferIndex) desired 
  MEMCOPY(bufferToFill, serialBuff[bufferIndex], len);

  // return the length of the argument
  return len;
}

int16u cliGetInt16uFromHexArgument(int8u index)
{
  return HIGH_LOW_TO_INT(cliGetHexByteFromArgument(0, index),
                         cliGetHexByteFromArgument(1, index));
}

int32u cliGetInt32uFromHexArgument(int8u index)
{
  return (((int32u)cliGetHexByteFromArgument(0, index) << 24)
          + ((int32u)cliGetHexByteFromArgument(1, index) << 16)
          + ((int32u)cliGetHexByteFromArgument(2, index) << 8)
          + ((int32u)cliGetHexByteFromArgument(3, index)));
}
