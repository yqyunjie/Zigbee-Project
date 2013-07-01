/**
 * File: token-utility-framework.c
 * Description: Token Utility framework.  This file provides the application
 * framework that hosts the token utility functions (found in token-utility.c).
 * The token utility functions can be used outside of this framework, or
 * within this framework which provides a simple human interface.  The commands
 * for the interface are found in token-utility-cmd.h.
 *
 * Culprit(s): Brooks Barrett
 *
 * Copyright 2005 by Ember Corporation.  All rights reserved.               *80*
 */
#include PLATFORM_HEADER

// The following headers are for the serial interface and dealing with human
// input and interpreting commands.
#include <stdio.h>
#include <string.h>
#ifdef __ICCAVR__
#include <pgmspace.h> // sscanf_P
#else
#define sscanf_P sscanf
#define strcmp_P strcmp
#endif

// The base set of header files needed to operate this application and
// manipulate the tokens.
#include "stack/include/ember.h"
#include "include/error.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"

// This structure holds the all the data needed by the command interpreter.
// This structure is used by token-utility-cmd.h and test-def.h below for
// generating the array of command data.
typedef struct cmdTag {
  void (*func)(int16u param1, int16u param2);
  char PGM *command;
  char PGM *params;
  char PGM *helpString;
} commandType;

// Pull in the command list file and run it through the test definition header
// to generate the necessary data structures for the command interpreter.
#define CMDHEADER "token-utility-cmd.h"
#include "test-def.h"
#undef CMDHEADER

int8u serialPort;
#define SER232 serialPort


// A friendly utility routine to generate an EOL sequence.
void printEOL(void)
{
  emberSerialPrintf(SER232,"\r\n");
}


// The 'REBOOT' command calls this function to perform a simple reboot
void rebootMe(int16u beep, int16u boop)
{
  halReboot();
}


// A sub function called from printHelp() below.
void printCommands(commandType PGM *list)
{
  int8u i;

  for(i=0; list[i].func != NULL; i++) {
    emberSerialPrintf(SER232,"  ");
    emberSerialPrintf(SER232,list[i].command);
    emberSerialPrintf(SER232," ");
    emberSerialPrintf(SER232,list[i].params);
    emberSerialPrintf(SER232," - ");
    emberSerialPrintf(SER232,list[i].helpString);
    printEOL();
  }
}


// This function is accessed by either the 'HELP' command or '?' command and
// prints out the help menu which is derived from the CMD list found in the
// file token-utility-cmd.h.
void printHelp(int16u param1, int16u param2)
{
  printCommands(baseCommandList);
  emberSerialPrintf(SER232,"    ** NOTE: all input values are in hex!");
  printEOL();
}


// This function is called from the main loop of main() when the user enters
// a string.  The string entered should match one of the commands found in
// token-utility-cmd.h and will execute the corresponding function found
// in the token-utility-cmd.h file.
boolean runCommand(char *cmd, char *params,commandType PGM *list)
{
  int8u i;
  int32u param1, param2;

  param1=0; param2=0;
  for(i=0; list[i].func != NULL; i++) {

    if(strcmp_P(cmd, list[i].command) == 0) {
      sscanf_P(params, "%lx %lx", &param1, &param2);
      list[i].func((int16u)param1,(int16u)param2);
      return TRUE;
    }
  }
  return FALSE;
}


// A simple function called by the startup sequence of main() intended to
// inform the user if either of the non-volatile data versions are incorrect.
// If either of these messages appear, the user should carefully check
// the token set used and the version number used, since one of the LOAD*
// commands might be needed to load the default values of the tokens and bring
// the token set up to date.
void haltestCheckTokens(void)
{
  tokTypeMfgNvdataVersion tokMfg;
  tokTypeStackNvdataVersion tokStack;
  halCommonGetToken(&tokMfg, TOKEN_MFG_NVDATA_VERSION);
  if (CURRENT_MFG_TOKEN_VERSION != tokMfg) {
    emberSerialPrintf(SER232, "Manufacturing token not up to date.\r\n");
    emberSerialPrintf(SER232, "Stack will not run with current version.\r\n");
    emberSerialPrintf(SER232, "LOADMFGTOKS command can reload default manufacturing tokens.\r\n");
    emberSerialPrintf(SER232, "Be very careful with this command!\r\n");
  }
  halCommonGetToken(&tokStack, TOKEN_STACK_NVDATA_VERSION);
  if( CURRENT_STACK_TOKEN_VERSION != tokStack) {
    emberSerialPrintf(SER232, "Warning:  stack token version not up to date.\r\n");
    emberSerialPrintf(SER232, "Stack will not run with current version.\r\n");
    emberSerialPrintf(SER232, "LOADTOKS command can reload default stack and app tokens.\r\n");
    emberSerialPrintf(SER232, "Be very careful with this command!\r\n");
  }
}


// This utility function is called from initAndSelectSerialPort() below.  It
// returns the serial port to be used by the application depending on which
// port is available.  If both ports are available, this function blocks,
// scanning them, and selects the port that first receives a return from the
// user allowing the user to select the port of operation.
int8u selectSerialPort(EmberStatus err0, EmberStatus err1)
{
  // If we only have one available port, dont wait and just choose it.
  if(err0) {
    return 1;
  } else if(err1) {
    return 0;
  } else {
    {
      int8u c;
      // Both ports are available, so wait until we hear a return key from
      // from the user then choose that port for use by the application.
      while(1) {
        if(halInternalForceReadUartByte(0, &c) == EMBER_SUCCESS) {
          if(c == '\r' || c == '\n' || c == '*') {
            return 0;
          }
        }

        if(halInternalForceReadUartByte(1, &c) == EMBER_SUCCESS) {
          if(c == '\r' || c == '\n' || c == '*') {
            return 1; 
          }
        }
      }
    }
  }
}


// This utility function initializes the serial port to the highest available
// baud rate on all available serial ports.  If the serial port and/or baud
// rate is not available, the emberSerialInit function will return an error
// indicating so and fall through to the next available port and baud.
int8u initAndSelectSerialPort(void)
{
  EmberStatus err0, err1;

  // Start with the highest baud rate until we find a working one.
  err0 = emberSerialInit(0, BAUD_115200, PARITY_NONE, 1); 
  if (err0) 
    err0 = emberSerialInit(0, BAUD_38400, PARITY_NONE, 1); 
  if (err0) 
    err0 = emberSerialInit(0, BAUD_19200, PARITY_NONE, 1); 

  // Now perform the same operation on the other serial port.
  err1 = emberSerialInit(1, BAUD_115200, PARITY_NONE, 1); 
  if (err1) 
    err1 = emberSerialInit(1, BAUD_38400, PARITY_NONE, 1); 
  if (err1) 
    err1 = emberSerialInit(1, BAUD_19200, PARITY_NONE, 1); 

  // Oops!  We have to have at least one working serial port!
  assert(err0 == EMBER_SUCCESS || err1 == EMBER_SUCCESS);

  // If both serial ports are active, wait for the user to select one to use.
  return selectSerialPort(err0, err1);
}


//  -------------main-----------------

int main()
{
  char buff[30];
  int8u paramStart;

  //Bring up the board and micro like usual.
  halInit();

  //Make sure the watchdog is not running since this is just a utility app.
  halInternalDisableWatchDog(MICRO_DISABLE_WATCH_DOG_KEY);
  
  //Bringup the serial functions on what is available.
  SER232 = initAndSelectSerialPort();

  //Enable interrupts and say hello to the world.
  INTERRUPTS_ON();
  printEOL();
  printEOL();
  emberSerialPrintf(SER232,"Ember Token Utility v1.0");
  printEOL();
  emberSerialPrintf(SER232,__DATE__);
  emberSerialPrintf(SER232,":");
  emberSerialPrintf(SER232,__TIME__);
  printEOL();
  printEOL();
  //Tell me what, if anything, I'm using for configuration and app tokens..
  #ifdef CONFIGURATION_HEADER
    emberSerialPrintf(SER232,"CONFIGURATION_HEADER=");
    emberSerialPrintf(SER232,CONFIGURATION_HEADER);
    printEOL();
  #else
    emberSerialPrintf(SER232,"CONFIGURATION_HEADER is not defined - using defaults.");
    printEOL();
  #endif
  #ifdef APPLICATION_TOKEN_HEADER
    emberSerialPrintf(SER232,"APPLICATION_TOKEN_HEADER=");
    emberSerialPrintf(SER232,APPLICATION_TOKEN_HEADER);
    printEOL();
  #else
    emberSerialPrintf(SER232,"APPLICATION_TOKEN_HEADER is not defined - no app tokens.");
    printEOL();
  #endif
  
  //Bringup the token system and check for errors.
  {
    EmberStatus tokenStatus = halStackInitTokens();

    if (tokenStatus == EMBER_EEPROM_MFG_STACK_VERSION_MISMATCH) {
      emberSerialGuaranteedPrintf(SER232, "EEPROM MFG and STACK version mismatch");
    } else if (tokenStatus == EMBER_EEPROM_MFG_VERSION_MISMATCH) {
      emberSerialGuaranteedPrintf(SER232, "EEPROM MFG version mismatch");
    } else if (tokenStatus == EMBER_EEPROM_STACK_VERSION_MISMATCH) {
      emberSerialGuaranteedPrintf(SER232, "EEPROM STACK version mismatch");
    } else if (tokenStatus != EMBER_SUCCESS) {
      emberSerialGuaranteedPrintf(SER232, "Unknown EEPROM error");
    }
  }

  //Check the version numbers of the mfg and stack tokens.
  haltestCheckTokens();

  while(1) {
    printEOL();
    //command prompt
    emberSerialPrintf(SER232,"> ");
    emberSerialReadLine(SER232,buff, 30);
    // scan for end of first word
    for(paramStart=0; (buff[paramStart] != ' ') && (buff[paramStart] != '\0'); paramStart++) {
      ; // nada
    }
    if (buff[paramStart] != '\0')   // args exist
      buff[paramStart++] = '\0';

    //run the entered command
    runCommand(buff, &buff[paramStart], baseCommandList);
  }
}


/////////////
//  STUBS  //
/////////////
void halButtonIsr(int8u but, int8u state) {}
void emberRadioReceiveIsr(void) {}
void halStackSymbolDelayAIsr(void) {}
boolean emRadioNeedsRebooting = FALSE;

