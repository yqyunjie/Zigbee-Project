/**
 * File: command-interpreter.c
 * Description: processes commands incoming over the serial port.
 *
 * Culprit(s): Richard Kelsey, Matteo Paris
 *
 * Copyright 2008 by Ember Corporation.  All rights reserved.               *80*
 */

#include PLATFORM_HEADER

#ifdef EZSP_HOST
  // Includes needed for ember related functions for the EZSP host
  #include "stack/include/error.h"
  #include "stack/include/ember-types.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/serial-interface.h"
  extern int8u emberEndpointCount;
#else
  #include "stack/include/ember.h"
#endif

#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "app/util/serial/command-interpreter2.h"

#if defined(EMBER_REQUIRE_FULL_COMMAND_NAME) \
  || defined(EMBER_REQUIRE_EXACT_COMMAND_NAME)
  #undef EMBER_REQUIRE_EXACT_COMMAND_NAME
  #define EMBER_REQUIRE_EXACT_COMMAND_NAME TRUE
#else
  #define EMBER_REQUIRE_EXACT_COMMAND_NAME FALSE
#endif

#if !defined APP_SERIAL
  extern int8u serialPort;
  #define APP_SERIAL serialPort
#endif

#if defined EMBER_COMMAND_INTEPRETER_HAS_DESCRIPTION_FIELD
  #define printIfEntryHasDescription(entry, ...) \
  if ((entry)->description != NULL) {            \
    emberSerialPrintf(APP_SERIAL,                \
                      __VA_ARGS__);              \
    }
  #define printIfEntryHasArgumentDescriptions(entry, ...) \
  if ((entry)->argumentDescriptions != NULL) {            \
    emberSerialPrintf(APP_SERIAL,                         \
                      __VA_ARGS__);                       \
  }
#else
  #define printIfEntryHasDescription(entry, ...) 
  #define printIfEntryHasArgumentDescriptions(entry, ...)
#endif

//------------------------------------------------------------------------------
// Forward declarations.
static void callCommandAction(void);
static int32u stringToUnsignedInt(int8u argNum, boolean swallowLeadingSign);
static int8u charDowncase(int8u c);

//------------------------------------------------------------------------------
// Command parsing state

typedef struct {

  // Finite-state machine's current state. 
  int8u state;

  // The command line is stored in this buffer.
  // Spaces and trailing '"' and '}' characters are removed,
  // and hex strings are converted to bytes. 
  int8u buffer[EMBER_COMMAND_BUFFER_LENGTH];

  // Indices of the tokens (command(s) and arguments) in the above buffer.
  // The (+ 1) lets us store the ending index.
  int8u tokenIndices[MAX_TOKEN_COUNT + 1];

  // The number of tokens read in, including the command(s). 
  int8u tokenCount;

  // Used while reading in the command line. 
  int8u index;

  // First error found in this command. 
  int8u error;

  // Storage for reading in a hex string. A value of 0xFF means unused. 
  int8u hexHighNibble;

  // The token number of the first true argument after possible nested commands.
  int8u argOffset;

} EmberCommandState;

static EmberCommandState commandState;

// Remember the previous character seen by emberProcessCommandString() to ignore
// an LF following a CR.
static int8u previousCharacter = 0;

EmberCommandEntry *emberCurrentCommand;

enum {
  CMD_AWAITING_ARGUMENT,
  CMD_READING_ARGUMENT,
  CMD_READING_STRING,                  // have read opening " but not closing "
  CMD_READING_HEX_STRING,              // have read opening { but not closing }
  CMD_READING_TO_EOL                   // clean up after error
};

// This byte is used to toggle certain internal features on or off.
// By default all are off.
int8u emberCommandInterpreter2Configuration = 0x00;

#ifdef EMBER_TEST
char *stateNames[] =
  {
    "awaiting argument",
    "reading argument",
    "reading string",
    "reading hex string",
    "reading to eol"
  };
#endif

//----------------------------------------------------------------
// Initialize the state maachine.

void emberCommandReaderInit(void)
{
  commandState.state = CMD_AWAITING_ARGUMENT;
  commandState.index = 0;
  commandState.tokenIndices[0] = 0;
  commandState.tokenCount = 0;
  commandState.error = EMBER_CMD_SUCCESS;
  commandState.hexHighNibble = 0xFF;
  commandState.argOffset = 0;
  emberCurrentCommand = NULL;
}

// Returns a value > 15 if ch is not a hex digit.
static int8u hexToInt(int8u ch)
{
  return ch - (ch >= 'a' ? 'a' - 10
               : (ch >= 'A' ? 'A' - 10
                  : (ch <= '9' ? '0'
                     : 0)));
}

static int8u tokenLength(int8u num)
{
  return (commandState.tokenIndices[num + 1] 
          - commandState.tokenIndices[num]);
}

static int8u *tokenPointer(int8s tokenNum)
{
  return commandState.buffer + commandState.tokenIndices[tokenNum];
}


//----------------------------------------------------------------
// This is a state machine for parsing commands.  If 'input' is NULL
// 'sizeOrPort' is treated as a port and characters are read from there.
// 
// Goto's are used where one parse state naturally falls into another,
// and to save flash.

boolean emberProcessCommandString(int8u *input, int8u sizeOrPort)
{
  boolean isEol = FALSE;
  boolean isSpace, isQuote;

  while (TRUE) {
    int8u next;
    
    if (input == NULL) {
      switch (emberSerialReadByte(sizeOrPort, &next)) {
      case EMBER_SUCCESS:
        break;
      case EMBER_SERIAL_RX_EMPTY:
        return isEol;
      default:
        commandState.error = EMBER_CMD_ERR_PORT_PROBLEM;
        goto READING_TO_EOL;
      }
    } else if (sizeOrPort == 0) {
      return isEol;
    } else {
      next = *input;
      input += 1;
      sizeOrPort -= 1;
    }

    //   fprintf(stderr, "[processing '%c' (%s)]\n", next, stateNames[commandState.state]);

    if (previousCharacter == '\r' && next == '\n') {
      previousCharacter = next;
      continue;
    }
    previousCharacter = next;
    isEol = ((next == '\r') || (next == '\n'));
    isSpace = (next == ' ');
    isQuote = (next == '"');


    switch (commandState.state) {

    case CMD_AWAITING_ARGUMENT:
      if (isEol) {
        callCommandAction();
      } else if (! isSpace) {
        if (isQuote) {
          commandState.state = CMD_READING_STRING;
        } else if (next == '{') {
          commandState.state = CMD_READING_HEX_STRING;
        } else {
          commandState.state = CMD_READING_ARGUMENT;
        }
        goto WRITE_TO_BUFFER;
      }
      break;

    case CMD_READING_ARGUMENT:
      if (isEol || isSpace) {
        goto END_ARGUMENT;
      } else {
        goto WRITE_TO_BUFFER;
      }

    case CMD_READING_STRING:
      if (isQuote) {
        goto END_ARGUMENT;
      } else if (isEol) {
        commandState.error = EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR;
        goto READING_TO_EOL;
      } else {
        goto WRITE_TO_BUFFER;
      }

    case CMD_READING_HEX_STRING: {
      boolean waitingForLowNibble = (commandState.hexHighNibble != 0xFF);
      if (next == '}') {
        if (waitingForLowNibble) {
          commandState.error = EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR;
          goto READING_TO_EOL;
        }
        goto END_ARGUMENT;
      } else {
        int8u value = hexToInt(next);
        if (value < 16) {
          if (waitingForLowNibble) {
            next = (commandState.hexHighNibble << 4) + value;
            commandState.hexHighNibble = 0xFF;
            goto WRITE_TO_BUFFER;
          } else {
            commandState.hexHighNibble = value;
          }
        } else if (! isSpace) {
          commandState.error = EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR;
          goto READING_TO_EOL;
        }
      }
      break;
    }

    READING_TO_EOL:
      commandState.state = CMD_READING_TO_EOL;
      
    case CMD_READING_TO_EOL:
      if (isEol) {
        if (commandState.error != EMBER_CMD_SUCCESS) {
          emberCommandErrorHandler(commandState.error);
        }
        emberCommandReaderInit();
      }
      break;

    END_ARGUMENT:
      if (commandState.tokenCount == MAX_TOKEN_COUNT) {
        commandState.error = EMBER_CMD_ERR_WRONG_NUMBER_OF_ARGUMENTS;
        goto READING_TO_EOL;
      }
      commandState.tokenCount += 1;
      commandState.tokenIndices[commandState.tokenCount] = commandState.index;
      commandState.state = CMD_AWAITING_ARGUMENT;
      if (isEol) {
        callCommandAction();
      }
      break;

    WRITE_TO_BUFFER:
      if (commandState.index == EMBER_COMMAND_BUFFER_LENGTH) {
        commandState.error = EMBER_CMD_ERR_STRING_TOO_LONG;
        goto READING_TO_EOL;
      }
      if (commandState.state == CMD_READING_ARGUMENT) {
        next = charDowncase(next);
      }
      commandState.buffer[commandState.index] = next;
      commandState.index += 1;
      break;

    default: {
    }
    } //close switch.
  }
}

//----------------------------------------------------------------
// Command lookup and processing

// Returs true if entry is a nested command, and in this case
// it populates the nestedCommand pointer.
// Otherwise it returns false, and does nothing with nestedCommand
//
// Nested commands are implemented by setting the action
// field to NULL, and the argumentTypes field is a pointer
// to a nested EmberCommandEntry array. The older mechanism is
// to set argumentTypes to "n" and then the action field
// contains the EmberCommandEntry, but that approach has a problem
// on AVR128, therefore it is technically deprecated. If you have 
// a choice, put NULL for action and a table under argumentTypes.
static boolean getNestedCommand(EmberCommandEntry *entry,
                                EmberCommandEntry **nestedCommand) {
  if ( entry -> action == NULL ) {
    *nestedCommand = (EmberCommandEntry*)entry->argumentTypes;
    return TRUE;
  } else if ( entry -> argumentTypes[0] == 'n' ) {
    *nestedCommand = (EmberCommandEntry*)(void*)entry->action;
    return TRUE;
  } else {
    return FALSE;
  }
}

static int8u charDowncase(int8u c)
{
  if ('A' <= c && c <= 'Z')
    return c + 'a' - 'A';
  else
    return c;
}

static int8u firstByteOfArg(int8u argNum)
{
  int8u tokenNum = argNum + commandState.argOffset;
  return commandState.buffer[commandState.tokenIndices[tokenNum]];
}

// To support existing lazy-typer functionality in the app framework, 
// we allow the user to shorten the entered command so long as the
// substring matches no more than one command in the table.
//
// To allow CONST savings by storing abbreviated command names, we also
// allow matching if the input command is longer than the stored command.
// To reduce complexity, we do not handle multiple inexact matches.  
// For example, if there are commands 'A' and 'AB', and the user enters 
// 'ABC', nothing will match.

static EmberCommandEntry *commandLookup(EmberCommandEntry *commandFinger, 
                                        int8u tokenNum)
{
  EmberCommandEntry *inexactMatch = NULL;
  int8u *inputCommand = tokenPointer(tokenNum);
  int8u inputLength = tokenLength(tokenNum);
  boolean multipleMatches = FALSE;

  for (; commandFinger->name != NULL; commandFinger++) {
    PGM_P entryFinger = commandFinger->name;
    int8u *inputFinger = inputCommand;
    for (;; entryFinger++, inputFinger++) {
      boolean endInput = (inputFinger - inputCommand == inputLength);
      boolean endEntry = (*entryFinger == 0);
      if (endInput && endEntry) {
        return commandFinger;  // Exact match.
      } else if (endInput || endEntry) {
        if (inexactMatch != NULL) {
          multipleMatches = TRUE;  // Multiple matches.
          break;
        } else {
          inexactMatch = commandFinger;
          break;
        }
      } else if (charDowncase(*inputFinger) != charDowncase(*entryFinger)) {
        break;
      }
    }
  }
  return (multipleMatches || EMBER_REQUIRE_EXACT_COMMAND_NAME ? NULL : inexactMatch);
}

static void echoPrint(void)
{
  int8u tokenNum = 0;
  for ( ; tokenNum < commandState.tokenCount; tokenNum++ ) {
    int8u *ptr = tokenPointer(tokenNum);
    int8u len = tokenLength(tokenNum);
    emberSerialWriteData(APP_SERIAL, ptr, len);
    emberSerialPrintf(APP_SERIAL, " ");
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

static void callCommandAction(void)
{
  EmberCommandEntry *commandFinger = emberCommandTable;
  int8u tokenNum = 0;
  // We need a separate argTypeNum index because of the '*' arg type.
  int8u argTypeNum, argNum; 

  if (commandState.tokenCount == 0) {
    goto kickout2;
  }

  // If we have echo, we echo here.
  if ( emberCommandInterpreterIsEchoOn() ) {
    echoPrint();
  }

  // Lookup the command.
  while (TRUE) {
    commandFinger = commandLookup(commandFinger, tokenNum);
    if (commandFinger == NULL) {
      commandState.error = EMBER_CMD_ERR_NO_SUCH_COMMAND;
      goto kickout;
    } else {
      emberCurrentCommand = commandFinger;
      tokenNum += 1;
      commandState.argOffset += 1;

      if ( getNestedCommand(commandFinger, &commandFinger) ) {
        if (tokenNum >= commandState.tokenCount) {
          commandState.error = EMBER_CMD_ERR_WRONG_NUMBER_OF_ARGUMENTS;
          goto kickout;
        }
      } else {
        break;
      }
    }
  }

  // If you put '?' as the first character
  // of the argument format string, then you effectivelly
  // prevent the argument validation, and the command gets executed.
  // At that point it is down to the command to deal with whatever
  // arguments it got.
  if ( commandFinger->argumentTypes[0] == '?' ) 
    goto kickout;
  
  // Validate the arguments.
  for(argTypeNum = 0, argNum = 0; 
      tokenNum < commandState.tokenCount; 
      tokenNum++, argNum++) {
    int8u type = commandFinger->argumentTypes[argTypeNum];
    int8u firstChar = firstByteOfArg(argNum);
    switch(type) {

    // Integers
    case 'u':
    case 'v':
    case 'w':
    case 's': {
      int32u limit = (type == 'u' ? 0xFF
                      : (type == 'v' ? 0xFFFF
                         : (type =='s' ? 0x7F : 0xFFFFFFFFUL)));
      if (stringToUnsignedInt(argNum, TRUE) > limit) {
        commandState.error = EMBER_CMD_ERR_ARGUMENT_OUT_OF_RANGE;
      }
      break;
    }

    // String
    case 'b':
      if (firstChar != '"' && firstChar != '{') {
        commandState.error = EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR;
      }
      break;

    case 0:
      commandState.error = EMBER_CMD_ERR_WRONG_NUMBER_OF_ARGUMENTS;
      break;

    default:
      commandState.error = EMBER_CMD_ERR_INVALID_ARGUMENT_TYPE;
      break;
    }

    if (commandFinger->argumentTypes[argTypeNum + 1] != '*') {
      argTypeNum += 1;
    }

    if (commandState.error != EMBER_CMD_SUCCESS) {
      goto kickout;
    }
  }

  if (! (commandFinger->argumentTypes[argTypeNum] == 0
         || commandFinger->argumentTypes[argTypeNum + 1] == '*')) {
    commandState.error = EMBER_CMD_ERR_WRONG_NUMBER_OF_ARGUMENTS;
  }

 kickout:

  if (commandState.error == EMBER_CMD_SUCCESS) {
    emberCommandActionHandler(commandFinger->action);
  } else {
    emberCommandErrorHandler(commandState.error);
  }

 kickout2:

  emberCommandReaderInit();
}


//----------------------------------------------------------------
// Retrieving arguments

int8u emberCommandArgumentCount(void)
{
  return (commandState.tokenCount - commandState.argOffset);
}

static int32u stringToUnsignedInt(int8u argNum, boolean swallowLeadingSign)
{
  int8u tokenNum = argNum + commandState.argOffset;
  int8u *string = commandState.buffer + commandState.tokenIndices[tokenNum];
  int8u length = tokenLength(tokenNum);
  int32u result = 0;
  int8u base = 10;
  int8u i;
  for (i = 0; i < length; i++) {
    int8u next = string[i];
    if (swallowLeadingSign && i == 0 && next == '-') {
      // do nothing
    } else if ((next == 'x' || next == 'X')
               && result == 0
               && (i == 1 || i == 2)) {
      base = 16;
    } else {
      int8u value = hexToInt(next);
      if (value < base) {
        result = result * base + value;
      } else {
        commandState.error = EMBER_CMD_ERR_ARGUMENT_SYNTAX_ERROR;
        return 0;
      }
    }
  }
  return result;
}

int32u emberUnsignedCommandArgument(int8u argNum) 
{
  return stringToUnsignedInt(argNum, FALSE);
}

int16s emberSignedCommandArgument(int8u argNum)
{
  boolean negative = (firstByteOfArg(argNum) == '-');
  int16s result = (int16s) stringToUnsignedInt(argNum, negative);
  return (negative ? -result : result);
}

int8u *emberStringCommandArgument(int8s argNum, int8u *length)
{
  int8u tokenNum = argNum + commandState.argOffset;
  int8u leadingQuote = (argNum < 0 ? 0 : 1);
  if (length != NULL) {
    *length = tokenLength(tokenNum) - leadingQuote;
  }
  return tokenPointer(tokenNum) + leadingQuote;
}

int8u emberCopyStringArgument(int8s argNum, 
                              int8u *destination, 
                              int8u maxLength,
                              boolean leftPad)
{
  int8u padLength;
  int8u argLength;
  int8u *contents = emberStringCommandArgument(argNum, &argLength);
  if (argLength > maxLength) {
    argLength = maxLength;
  }
  padLength = leftPad ? maxLength - argLength : 0;
  MEMSET(destination, 0, padLength);
  MEMCOPY(destination + padLength, contents, argLength);
  return argLength;
}

#if !defined(EMBER_APPLICATION_HAS_COMMAND_ACTION_HANDLER)
void emberCommandActionHandler(const CommandAction action)
{
  (*action)();
}
#endif

#if !defined(EMBER_APPLICATION_HAS_COMMAND_ERROR_HANDLER)
PGM_NO_CONST PGM_P emberCommandErrorNames[] =
  {
    "",
    "Serial port error",
    "No such command",
    "Wrong number of args",
    "Arg out of range",
    "Arg syntax error",
    "Too long",
    "Bad arg type"
  };


static void printCommandUsage(boolean singleCommandUsage,
                              EmberCommandEntry *entry) 
{
  PGM_P arg = entry->argumentTypes;
  emberSerialPrintf(APP_SERIAL, "%p", entry->name);

  if ( entry -> action == NULL ) {
    emberSerialPrintf(APP_SERIAL, "...");
  } else if (singleCommandUsage) {
    int8u argumentIndex = 0;
    printIfEntryHasDescription(entry, " (args) \n");
    while (*arg) {
      int8u c = *arg;
      printIfEntryHasArgumentDescriptions(entry,
                                          "  ");
      emberSerialPrintf(APP_SERIAL,
                        (c == 'u' ? " <int8u>"
                         : c == 'v' ? " <int16u>"
                         : c == 'w' ? " <int32u>"
                         : c == 's' ? " <int8s>"
                         : c == 'b' ? " <string>"
                         : c == 'n' ? " ..."
                         : c == '*' ? " *"
                         : " ?"));
      printIfEntryHasArgumentDescriptions(entry,
                                          "  %p\n",
                                          entry->argumentDescriptions[argumentIndex]);
      argumentIndex++;
      arg += 1;
    }
    emberSerialPrintfLine(APP_SERIAL, "");
  }
  emberSerialWaitSend(APP_SERIAL);
  printIfEntryHasDescription(entry, " - %p", entry->description);
  
  emberSerialPrintfLine(APP_SERIAL, "");
  emberSerialWaitSend(APP_SERIAL);
}

void emberPrintCommandUsage(EmberCommandEntry *entry)
{
  EmberCommandEntry *commandFinger;
  printCommandUsage(TRUE,
                    entry);

  if ( getNestedCommand(entry, &commandFinger) ) {
    for (; commandFinger->name != NULL; commandFinger++) {
      emberSerialPrintf(APP_SERIAL, "  ");
      printCommandUsage(FALSE,
                        commandFinger);
    }   
  }
}

void emberPrintCommandUsageNotes(void)
{
  emberSerialPrintf(APP_SERIAL, 
                    "Usage:\r\n"
                    "<int>: 123 or 0x1ABC\r\n"
                    "<string>: \"foo\" or {0A 1B 2C}\r\n\r\n");
  emberSerialWaitSend(APP_SERIAL);
}

void emberPrintCommandTable(void)
{
  EmberCommandEntry *commandFinger = emberCommandTable;
  emberPrintCommandUsageNotes();
  for (; commandFinger->name != NULL; commandFinger++) {
    printCommandUsage(FALSE, commandFinger);
  }
}

void emberCommandErrorHandler(EmberCommandStatus status)
{
  emberSerialPrintf(APP_SERIAL, "%p\r\n", emberCommandErrorNames[status]);

  if (emberCurrentCommand == NULL) {
    emberPrintCommandTable();
  } else {
    int8u *finger;
    int8u tokenNum, i;
    emberPrintCommandUsageNotes();
    // Reconstruct any parent commands from the buffer.
    for (tokenNum = 0; tokenNum < commandState.argOffset - 1; tokenNum++) {
      finger = tokenPointer(tokenNum);
      for (i = 0; i < tokenLength(tokenNum); i++) {
        emberSerialPrintf(APP_SERIAL, "%c", finger[i]);
      }
      emberSerialPrintf(APP_SERIAL, " ");
    }
    emberPrintCommandUsage(emberCurrentCommand);
  }
}
#endif
