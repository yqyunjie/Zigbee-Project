/** @file linux-serial.c
 *  @brief  Simulate serial I/O for linux EZSP UART 
 *          applications.
 * 
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.       *80*-->
 */

#define _GNU_SOURCE 1  // This turns on certain non-standard GNU extensions
                       // to the C library (i.e. strnlen()).  
                       // Include before PLATFORM_HEADER since that also
                       // includes 'string.h'

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"
#include "stack/include/error.h"

#include "hal/hal.h"
#include "serial.h"
#include "command-interpreter2.h"
#include "cli.h"

#include <sys/types.h>         // for fstat()
#include <sys/stat.h>          // ""
#include <fcntl.h>             // for fcntl()
#include <stdlib.h>      
#include <unistd.h>            // for pipe(), fork()

#if defined NO_READLINE
  #define READLINE_SUPPORT 0
#else
  #define READLINE_SUPPORT 1
  #include <readline/readline.h> // for readline()
  #include <readline/history.h>  // ""

#endif

#include <signal.h>            // for trapping SIGTERM
#include <errno.h>             // for strerror() and errno
#include <stdarg.h>            // for vfprintf()
#include <sys/wait.h>          // for wait()

// All needed For ashSerialGetFd()
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

#include "app/util/gateway/backchannel.h"
#include "linux-serial.h"

// Don't like readline and the GPL requirements?  Use 'libedit'.
// It is a call-for-call compatible with the readline library but is
// released under the BSD license.
//   http://www.thrysoee.dk/editline/

//------------------------------------------------------------------------------
// Globals

#define MAX_COMMAND_LENGTH     50
#define NUM_PORTS              2
#define INVALID_FD             -1
#define INVALID_PID            0
#define MAX_PROMPT_LENGTH      20
#define MAX_NUMBER_OF_COMMANDS 500
#define LINE_FEED              0x0A
#define MAX_STRING_LENGTH      250  // arbitrary limit

static int STDIN = 0;

// The first index is the port, the second
// is the pipe number.  The first two are the data pipes,
// the second two are the control pipes.
static int pipeFileDescriptors[NUM_PORTS][4] = { 
  { INVALID_FD, INVALID_FD, INVALID_FD, INVALID_FD },
  { INVALID_FD, INVALID_FD, INVALID_FD, INVALID_FD }
};
#define DATA_READER(__port) pipeFileDescriptors[(__port)][0]
#define DATA_WRITER(__port) pipeFileDescriptors[(__port)][1]
#define CONTROL_READER(__port) pipeFileDescriptors[(__port)][2]
#define CONTROL_WRITER(__port) pipeFileDescriptors[(__port)][3]

static boolean amParent = TRUE;
static boolean useControlChannel = TRUE;
static boolean debugOn = FALSE;
static boolean promptSet = FALSE;
static char prompt[MAX_PROMPT_LENGTH];
static pid_t childPid[NUM_PORTS] = { INVALID_PID, INVALID_PID };

static boolean usingCommandInterpreter = FALSE;

// for readline() command completion
static const char** allCommands = NULL;
static int totalNumberOfCommands;

#ifdef __APPLE__
#define strnlen(string, n) strlen((string))
#define rl_completion_matches(a, b) completion_matches((a), (b))
#define strsignal(x) NULL   // This is a _GNU_SOURCE extension.
#endif

// This will be prefixed with $HOME
static const char readlineHistoryFilename[] = ".linux-serial.history";
static char readlineHistoryPath[MAX_STRING_LENGTH];

static int childProcessPort = -1;

//------------------------------------------------------------------------------
// Forward Declarations

static EmberStatus serialInitInternal(int8u port);
static void childRun(int8u port);
static void setNonBlockingFD(int fd);
static void debugPrint(const char* formatString, ...);
static void processSerialInput(int8u port);
static char* transformEmberPrintfToStandardPrintf(const char* input);
static void shiftStringRight(char* string, int8u length, int8u charsToShift);
static EmberStatus internalPrintf(PGM_P formatString, va_list ap);

#if READLINE_SUPPORT
  char** commandCompletion(const char *text, int start, int end);
  char* singleCommandCompletion(const char *text, int start);
  char* filenameCompletion(const char* text, int state);
  static void initializeHistory(void);
  static void writeHistory(void);
static char* duplicateString(const char* source);
#else  // use stubs
  #define initializeHistory()
  #define writeHistory()
  #define add_history(x)
#endif

static void parentCleanupAfterChildDied(void);
static void installSignalHandler(void);
static void childCleanupAndExit(void);

//------------------------------------------------------------------------------
// Initialization Functions

// In order to handle input more effeciently and cleanly,
// we use readline().  This is a blocking call, so we create a second
// process to handle the serial input while the main one can simply
// loop executing ezspTick() and other functionality.

EmberStatus emberSerialInit(int8u port, 
                            SerialBaudRate rate,
                            SerialParity parity,
                            int8u stopBits)
{
  static boolean emberSerialInitCalled = FALSE;

  debugPrint("emberSerialInit()\n");
  if (port > 1) {
    return EMBER_SERIAL_INVALID_PORT;
  }

  if (childPid[port] != INVALID_PID) {
    debugPrint("Serial port %d already initialized.\n", port);
    return EMBER_SUCCESS;
  }
  
  // Without the backchannel, there is only one serial port available (STDIN).
  if (!backchannelEnable
      && emberSerialInitCalled) {
    return EMBER_SERIAL_INVALID_PORT;
  }

  if (backchannelEnable) {
    // For the CLI, wait here until a new client connects for the first time.
    BackchannelState state = 
      backchannelCheckConnection(port, 
                                 (port                     // waitForConnection?
                                  == SERIAL_PORT_CLI));
    if (port == SERIAL_PORT_CLI && state != NEW_CONNECTION) {
      debugPrint("Failed to get new backchannel connection.\n");
      return EMBER_ERR_FATAL;
    } else if ( !(state == NEW_CONNECTION || state == CONNECTION_EXISTS) ) {
      // We will defer initializing the RAW serial port (spawning the child
      // and that jazz) until we actually have a new client connection.
      return EMBER_SUCCESS;
    }
  }
  
  EmberStatus status = serialInitInternal(port);
  
  if (status == EMBER_SUCCESS) {
    installSignalHandler();
    emberSerialInitCalled = TRUE;
  }
  
  return status;
}

static EmberStatus serialInitInternal(int8u port)
{
  // Create two pipes, control and data.
  // Fork
  // Parent closes the write end of the data pipe and read end of the control, 
  //   child closes the read end of the data pipe and write end of the control.
  // Child sits and waits for input via readline() (or read())
  //   When it receives input it writes it to the data pipe.  It then blocks
  //   on a read call to the control pipe (if it is running the CLI).
  // Parent loops normally, calling emberSerialReadByte() or emberSeriaReadLine()
  //   to get the input.  These calls map to reads of the data pipe, the former
  //   would NOT be blocking while the latter would be.
  //   Parent calls indirectly readyForSerialInput() after reading an entire 
  //   line of data (via the next emberSerialReadByte()).

  int status;
  int i;
  pid_t pid;

  for (i = 0; i < 2; i++) {
    char* string = (i == 0
                    ? "data"
                    : "control");
    int* pipePtr = (i == 0
                    ? &(pipeFileDescriptors[port][0])
                    : &(pipeFileDescriptors[port][2])); 
    status = pipe(pipePtr);
    if (status != 0) {
      fprintf(stderr, 
              "FATAL: Could not create %s pipe (%d): %s",
              string,
              status,
              strerror(status));
      assert(FALSE);
    }
    debugPrint("Serial Port %d, Created %s Pipe, Reader: %d, Writer %d\n",
               port,
               string,
               pipePtr[0],
               pipePtr[1]);
  }

  // Change the data reader to non-blocking so that we will
  // continue to be able to execute ezspTick().
  setNonBlockingFD(DATA_READER(port));

  pid = fork();
  if (pid == 0) {  // Child
    amParent = FALSE;
    childRun(port);
    return EMBER_ERR_FATAL;  // should never get here

  } else if (pid == -1) {
    fprintf(stderr, "FATAL: Could not fork! (%d): %s\n",
            errno,
            strerror(errno));
    assert(FALSE);
  }
  // Parent
  debugPrint("fork(): My child's pid is %d\n", pid);

  if (allCommands) {
    // We have no need for the CLI command list, since that
    // is something used only by the child.
    debugPrint("Freeing CLI command meta-data.\n");
    free(allCommands);
    allCommands = NULL;
  }

  close(DATA_WRITER(port));
  close(CONTROL_READER(port));
  DATA_WRITER(port) = INVALID_FD;
  CONTROL_READER(port) = INVALID_FD;

  if (STDIN != INVALID_FD) {
    close(STDIN);
    STDIN = INVALID_FD;
  }


  childPid[port] = pid;

  setMicroRebootHandler(&emberSerialCleanup);
  return EMBER_SUCCESS;
}

// Checks to see if there is a remote connection in place, or if a new
// one has come in.  When a new one comes in, spawn a child process to
// deal with it.
static boolean handleRemoteConnection(int8u port)
{
  BackchannelState state = 
    backchannelCheckConnection(port,
                               FALSE); // don't wait for new connection
  if (state == CONNECTION_ERROR
      || state == NO_CONNECTION) {
    return FALSE;
  } else if (state == CONNECTION_EXISTS) {
    return TRUE;
  } // else
    //   state == NEW_CONNECTION

  return (EMBER_SUCCESS == serialInitInternal(port));
}

static void childRun(int8u port)
{
  childProcessPort = port;

  close(DATA_READER(port));
  close(CONTROL_WRITER(port));

  if (backchannelEnable) {
    if (EMBER_SUCCESS 
        ==  backchannelMapStandardInputOutputToRemoteConnection(port)) {
      if (port == SERIAL_PORT_CLI) {
        fprintf(stdout, "Connected.\r\n");
        fflush(stdout);
      } // else
        //   Raw port, don't print anything.
    } else {
      childCleanupAndExit();
    }
  } // else
    //   don't do anything


  // This fixes a bug on Cygwin where a process that
  // holds onto an open file descriptor after a fork()
  // can prevent the other parent process from performing a
  // close() and re-open() on it.  Specifically this only seems
  // to affect serial port FDs, which is what ASH uses.
  if (INVALID_FD != ashSerialGetFd()) {
    int port = ashSerialGetFd();
    if (0 != close(port)) {
      fprintf(stderr, "FATAL: Could not close EZSP file descriptor %d: %s\n",
              port,
              strerror(errno));
      assert(FALSE);
    }
  }

  installSignalHandler();
  processSerialInput(port);

  // Normally the above function NEVER returns
  // If we get here it is an error.
  assert(0);
}

void emberSerialSetPrompt(const char* thePrompt)
{
  if (thePrompt == NULL) {
    promptSet = FALSE;
    return;
  }

  // Substract one for the '>'
  snprintf(prompt, 
           MAX_PROMPT_LENGTH - 1, 
           "%s>", 
           thePrompt);
}

static void setNonBlockingFD(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  int status = fcntl(fd, F_SETFD, flags | O_NONBLOCK);
  if (status != 0) {
    fprintf(stderr, 
            "FATAL: Could not set pipe reader to non-blocking (%d): %s\n",
            errno,
            strerror(errno));
    assert(FALSE);
  }
}

// It is expected this is only called within the parent process.
void emberSerialCleanup(void)
{
  int8u port;
  for (port = 0; port < 2; port++) {
    if (childPid[port] != INVALID_PID) {
      int status;
      close(DATA_READER(port));
      close(CONTROL_WRITER(port));
      DATA_READER(port) = INVALID_FD;
      CONTROL_WRITER(port) = INVALID_FD;
      debugPrint("Waiting for child on port %d to terminate.\n", port);
      kill(childPid[port], SIGTERM);
      wait(&status);
      debugPrint("Child on port %d exited with status: %d\n", port, status);
      childPid[port] = INVALID_PID;
    }
  }
  if (backchannelEnable) {
      backchannelStopServer(SERIAL_PORT_CLI);
      backchannelStopServer(SERIAL_PORT_RAW);
  }
}

static void parentCleanupAfterChildDied(void)
{
  // First we need to determine which child died.  
  // We assume that a child has really died, otherwise the 
  // wait() call will block.
  int status;
  pid_t pid = wait(&status);
  int i;
  int childPort = -1;
  for (i = 0; i < NUM_PORTS; i++) {
    if (childPid[i] == pid) {
      childPort = i;
      break;
    }
  }
  if (childPort == -1) {
    // A child process died that we didn't know about!
    assert(0);
  }
  childPid[childPort] = INVALID_PID;
  close(DATA_READER(childPort));
  close(CONTROL_WRITER(childPort));
  DATA_READER(childPort) = INVALID_FD;
  CONTROL_WRITER(childPort) = INVALID_FD;
}

static void childCleanupAndExit(void)
{
  if (childProcessPort == SERIAL_PORT_CLI) {
    writeHistory();
  }
  if (childProcessPort != -1) {
    close(DATA_WRITER(childProcessPort));
    close(CONTROL_READER(childProcessPort));
    if (backchannelEnable) {
      backchannelStopServer(childProcessPort);
    }
  }

  exit(0);
}

// This works only for the command interpreter.
// Loop and get pointers to all the strings of the available commands.
void emberSerialCommandCompletionInit(EmberCommandEntry listOfCommands[])
{
  if (listOfCommands == NULL) {
    return;
  }

  if (allCommands == NULL) {
    int i = 0;
    totalNumberOfCommands = 0;
    while (listOfCommands[totalNumberOfCommands].name != NULL
           && totalNumberOfCommands < MAX_NUMBER_OF_COMMANDS) {
      int length = strnlen(listOfCommands[totalNumberOfCommands].name, 
                           MAX_COMMAND_LENGTH + 1);
      if (length > MAX_COMMAND_LENGTH) {
        fprintf(stderr, 
                "FATAL: Command '%s' is too long.  Adjust MAX_COMMAND_LENGTH (Currently it is %d).\n",
                listOfCommands[totalNumberOfCommands].name,
                MAX_COMMAND_LENGTH);
        assert(FALSE);
      }
      totalNumberOfCommands++;
      
    }
    
    // We add 1 to the number of commands to NULL terminate it.
    allCommands = malloc(sizeof(char*) * (totalNumberOfCommands + 1));

    while (i < totalNumberOfCommands) {
      allCommands[i] = (const char*)listOfCommands[i].name;
      //      debugPrint("Added Command: %s\n", allCommands[i]);
      i++;
    }
    allCommands[i] = NULL;

#if READLINE_SUPPORT
    rl_attempted_completion_function = commandCompletion;
    rl_completion_entry_function = filenameCompletion;
#endif
    usingCommandInterpreter = TRUE;
  }
}

// this works only for QA's cli
// Loop and get pointers to all the strings of the available commands.
void emberSerialCommandCompletionInitCli(cliSerialCmdEntry cliCmdList[], 
                                         int cliCmdListLength)
{
  if (allCommands == NULL) {
    int i = 0;

    debugPrint("commandCompletionInitCli() called.\n");

    totalNumberOfCommands = cliCmdListLength;
    
    // We add 1 to the number of commands to NULL terminate it.
    allCommands = malloc(sizeof(char*) * (totalNumberOfCommands + 1));

    while (i < totalNumberOfCommands) {
      allCommands[i] = (char*)cliCmdList[i].cmd;
      i++;
    }
    allCommands[i] = NULL;

#if READLINE_SUPPORT
    rl_attempted_completion_function = commandCompletion;
    rl_completion_entry_function = filenameCompletion;
#endif
  }
}

//------------------------------------------------------------------------------
// Serial Input

int emberSerialGetInputFd(int8u port)
{
  if (port > (NUM_PORTS - 1)) {
    return INVALID_FD;
  }
  return DATA_READER(port);
}

static void readyForSerialInput(int8u port)
{
  if (useControlChannel) {
    char data = '1';
    if (1 == write(CONTROL_WRITER(port), &data, 1)) {
      debugPrint("Sent reader process 'go-ahead' signal.\n");
    } else {
      // We assume this function is only used for CLI input.
      // If the CLI process died, then the parent should also
      // go away.
      // For the RAW input, which is only accessible with the
      // backchannel enabled, that child process may come and go
      // and we don't need to worry about it.
      emberSerialCleanup();
      exit(-1);
    }
  }
}

// returns # bytes available for reading
int8u emberSerialReadAvailable(int8u port)
{
  // I am not aware of a good way to get the exact number
  // of bytes available for reading from an anonymous pipe.
  // We use select() to see if there is any data available for
  // reading and that means there is at least one byte present.
  fd_set readSet;
  struct timeval timeout = { 0, 0 }; // return immediately
  int fdsWithData;

  if (DATA_READER(port) == INVALID_FD) {
    return 0;
  }

  FD_ZERO(&readSet);
  FD_SET(DATA_READER(port), &readSet);

  fdsWithData = select(DATA_READER(port) + 1,   // per the man page
                       &readSet, 
                       NULL, 
                       NULL,
                       &timeout);
  if (fdsWithData < 0) {
    fprintf(stderr, 
            "Fatal: select() returned error: %s\n", 
            strerror(errno));
    assert(FALSE);
  }
  return (fdsWithData > 0 ? 1 : 0);
}

// This should only be called by the parent process (i.e. the main app)
EmberStatus emberSerialReadByte(int8u port, int8u *dataByte)
{
  static boolean sendGoAhead = TRUE;
  static boolean waitForEol = FALSE;
  size_t bytes;

  if (backchannelEnable
      && !handleRemoteConnection(port)) {
    return EMBER_SERIAL_INVALID_PORT;
  }
  
  // The command interpreter reads bytes until it gets an EOL.
  // The CLI reads bytes until it gets a "\r\n".  
  // The latter is easily supported since the data is sitting
  // in our data pipe.  The former requires a little extra work.

  // For the command interpreter We don't want to send the 'go-ahead' until
  // the parent reads a byte of data after it has read the EOL.
  if (waitForEol) {
    waitForEol = FALSE;
    return EMBER_SERIAL_RX_EMPTY;
  }
  
  if (port == SERIAL_PORT_CLI && sendGoAhead) {
    readyForSerialInput(port);
    sendGoAhead = FALSE;
  }

  if (0 == emberSerialReadAvailable(port)) {
    return EMBER_SERIAL_RX_EMPTY;
  }

  //  debugPrint("Parent: Data is ready.\n");
  bytes = read(DATA_READER(port), (void*)dataByte, 1);

  // We have read the entire line of input, the serial input process will
  // be blocking until we tell it to go ahead and read more input.
  if (port == SERIAL_PORT_CLI && *dataByte == '\n') {
    sendGoAhead = TRUE;
    if (usingCommandInterpreter) {
      waitForEol = TRUE;
    }
  }

  if (bytes == 1) {
//    debugPrint("emberSerialReadByte(): %c\n", (char)*dataByte);
    return EMBER_SUCCESS;
  } else if (bytes == 0) {
    return EMBER_SERIAL_RX_EMPTY;
  } else {
    return EMBER_ERR_FATAL;
  }
}

#if !READLINE_SUPPORT
// Support for those systems without the readline library.
static char* readline(const char* prompt)
{
  int8u i = 0;
  char* data = malloc(MAX_COMMAND_LENGTH + 1);  // add 1 for '\0'
  EmberStatus status;
  boolean done = FALSE;

  debugPrint("Allocated %d bytes for readline()\n", MAX_COMMAND_LENGTH + 1);

  if (data == NULL) 
    return NULL;

  fprintf(stdout, "%s", prompt);
  fflush(stdout);
  
  while (!done && i < MAX_COMMAND_LENGTH) {
    ssize_t bytes = read(STDIN, &(data[i]), 1);
    if (bytes == -1) {
      if (errno == EINTR) {
        continue;
      } else {
        fprintf(stderr, "Attempt to read from STDIN failed!\n");
        assert(FALSE);
      }
    } else if (bytes == 0) {
      continue;
    }

//    debugPrint("char: 0x%02x\n", (int)data[i]);
    if (data[i] == LINE_FEED        // line feed
        || data[i] == EOF)
      done = TRUE;
    i++;
  }
  if (data[i-1] == LINE_FEED) {
    // Don't need the LF delineator, it is implied when this function
    // returns.
    i--;
  }
  data[i] = '\0';
  return data;
}
#endif

// This should only be called by the parent process (i.e. the main app)
EmberStatus emberSerialReadLine(int8u port, char *data, int8u max)
{
  int8u count = 0;
  EmberStatus status;
  while (count < max) {
    status = emberSerialReadByte(port, &(data[count]));
    if (EMBER_SUCCESS == status) {
      count++;
    } else if (EMBER_SERIAL_RX_EMPTY == status) {
      return EMBER_SUCCESS;
    }
  }
  return EMBER_SUCCESS;
}

// Read a line of data and write it to our pipe.
// Wait for the other process to give us the go-ahead.

static void processSerialInput(int8u port)
{
  int length;
  char* readData = NULL;
  char newLine[] = "\r\n";  // The CLI code requires \r\n as the final bytes.
  char goAhead;
  char singleByte[2];

  if (port == SERIAL_PORT_CLI) {
    initializeHistory();
  }

  debugPrint("Processing input for port %d.\n", port);

  while (1) {
    if (port == SERIAL_PORT_CLI && useControlChannel) {
      // Wait for the parent to give the "go ahead"
      debugPrint("Child process waiting for parent's 'go-ahead'.\n");
      read(CONTROL_READER(port), &goAhead, 1);
    }

    if (port == SERIAL_PORT_CLI) {
      // If we are operating in a backchannel environment, then STDIN has
      // already been re-mapped to the correct socket.  

      // This performs a malloc()
      readData = readline(prompt);
    } else {
      // Raw data
      readData = &(singleByte[0]);
      boolean success = (backchannelEnable
                         ? (EMBER_SUCCESS == backchannelReceive(port, readData))
                         : (1 != read(STDIN, readData, 1)));

      if (!success) {
        childCleanupAndExit();
      }
      singleByte[1] = '\0';
    }
    if (readData == NULL) {
      fprintf(stderr,
              "Child process for serial input got EOF.  Exiting.\n");
      childCleanupAndExit();
    }
    length = (port == SERIAL_PORT_CLI
              ? strnlen(readData, 255)  // 255 is an arbitrary maximum
              : 1);                     
    assert(-1 < write(DATA_WRITER(port), readData, length));

    if (port == SERIAL_PORT_CLI) {
      debugPrint("readline() input (%d bytes): %s\n", length, readData);
      assert(2 == write(DATA_WRITER(port), &newLine, 2));
      if (length > 0) {
        add_history(readData);
      }
    }

    if (port == SERIAL_PORT_CLI) {
      free(readData);
    }
  }
}

//------------------------------------------------------------------------------
// Serial Output

EmberStatus emberSerialGuaranteedPrintf(int8u port, PGM_P format, ...) 
{
  va_list vargs;
  EmberStatus stat = EMBER_SUCCESS;

  va_start(vargs, format);
  stat = emberSerialPrintfVarArg(port, format, vargs);
  va_end(vargs);
  return stat;
}

// This is implemented because a few applications count on this internal
// function.  This is essentially a 'sprintf()' where the application wants 
// formatted text output but uses its own mechanism to print.  We will ignore
// that use since it doesn't make much sense in the case of a gateway 
// application.  If the application really wants to do something special for
// printing formatted output it should call sprintf() on its own.
int8u emPrintfInternal(emPrintfFlushHandler handler, 
                       int8u port, 
                       PGM_P buff, 
                       va_list list)
{
  return (EmberStatus)emberSerialPrintfVarArg(port, buff, list);
}

// Main printing routine.
// Calls into normal C 'vprintf()'
EmberStatus emberSerialPrintfVarArg(int8u port, PGM_P formatString, va_list ap)
{
  EmberStatus stat = EMBER_SERIAL_INVALID_PORT;
  char* newFormatString = transformEmberPrintfToStandardPrintf(formatString);
  if (newFormatString == NULL) {
    return EMBER_NO_BUFFERS;
  }
  if (backchannelEnable) {
    if (handleRemoteConnection(port)) {
      stat = backchannelClientVprintf(port, newFormatString, ap);
    }
  } else {
    stat = (0 == vprintf(newFormatString, ap)
            ? EMBER_ERR_FATAL
            : EMBER_SUCCESS);
    fflush(stdout);
  }
  free(newFormatString);
  return stat;
}

// Compare two characters, ignoring case (like strcasecmp)
static boolean charCaseCompare(const char c1, const char c2) {
  int8u i;
  char c[2];
  c[0] = c1;
  c[1] = c2;
  for (i = 0; i < 2; i++) {
    // Convert both to lowercase if they are letters
    if (c[i] >= 'A' && c[i] <= 'Z') {
      c[i] += 0x20;
    }
  }
  return (c[0] == c[1]);
}

// Transform Ember printfs to standard printfs.
//
// Ember uses %p to represent a pointer to a flash string, change this to the
// standard %s.  In addition, we don't need both '\r' and '\n' at the end of the
// string, '\n' will suffice.  The last and most difficult conversion is
// because Ember Printf interprets the %x, %2x, and %4x in a non-standard way.
// Normally a number after a '%' is interpreted as a minimum width field
// (with spaces instead of 0 padding).  For Ember, this is interpreted to mean
// a 1, 2, or 4 byte integer value respectively and always 0 padding.  
// Therefore to maintain parity we change %x to %02x, %2x to %04x, and
//  %4x to %08x.
//
// Returns a pointer to a newly allocated and modified string (caller must free).
//   Or NULL if it failed to allocate memory.

static char* transformEmberPrintfToStandardPrintf(const char* input)
{
  // Assume no string is longer than 255.  This is true for most of
  // our embedded code so we assume host specific code was written
  // the same way.
  int8u length = strnlen(input, 254) + 1;   // add 1 for '\0'
  boolean percentFound = FALSE;
  int8u paddingNeeded = 0;
  int8u i = 0;
  char* newFormatString = malloc(length);

  if (newFormatString == NULL) {
    return NULL;
  }

  strncpy(newFormatString, input, length - 1);
  newFormatString[length - 1] = '\0';

  // Replacing %p with %s is straightforward because it doesn't change the size
  // of the string.  Changing %x, %2x and %4x to the correct values involves
  // some additional space in the string.  So record how much space is needed for
  // lengthening the string later to add characters.

  while (i < (length - 1)) {
    char a = newFormatString[i];
    if (percentFound) {
      if (a == 'p') {
        newFormatString[i] = 's';
      } else if (charCaseCompare(a, 'x')) {
        // Need to add two characters to change %x -> %02x
        paddingNeeded += 2;
      } else if (((i+1 < (length - 1)
                   && charCaseCompare(newFormatString[i+1], 'x')))) {
        paddingNeeded++;
      }

      percentFound = FALSE;
    } else if (a == '%') {
      percentFound = TRUE;
    }

    // Filter out all '\r'.  Unnecessary for Cygwin/Linux.
    // In the case of remote connections, we need native line-endings.
    // DOS telnet client wants both, therefore we send it.
    if (!backchannelEnable && a == '\r') {
      newFormatString[i] = ' ';
    }

    i++;
  }

  assert((int16u)length + (int16u)paddingNeeded <= 255);
  length += paddingNeeded;

  if (paddingNeeded) {
    char* temp = realloc(newFormatString, length);
    if (temp == NULL) {
      debugPrint("Failed to realloc, freeing existing memory.\n");
      free(newFormatString);
      return NULL;
    }
    newFormatString = temp;

    i = 0;
    percentFound = FALSE;

    // Change %x  -> %02x
    //        %2x -> %04x
    //        %4x -> %08x
    while (i < (length - 1)) {
      char a = newFormatString[i];
      if (percentFound
          && (charCaseCompare(a, 'x')
              || (i+1 < length
                  && charCaseCompare(newFormatString[i+1], 'x')
                  && (a == '2'
                      || a == '4')))) {
        int8u charsToAdd = charCaseCompare(a, 'x') ? 2 : 1;
        shiftStringRight(&(newFormatString[i]), length - i, charsToAdd);

        newFormatString[i] = '0';
        if (charsToAdd == 2) {
          newFormatString[i+1] = '2';
        } else if (a == '2') {
          newFormatString[i+1] = '4';
        } else {
          newFormatString[i+1] = '8';
        }
        percentFound = FALSE;
      } else if (a == '%') {
        percentFound = TRUE;
      } else {
        percentFound = FALSE;
      }
      i++;
    }
  }
  return newFormatString;
}

// Work backwards from the end of the string shifting all charactes to
// the right by a number of characters
static void shiftStringRight(char* string, int8u length, int8u charsToShift)
{
  int i = length - 1;
  while (i >= charsToShift) {
    string[i] = string[i - charsToShift];
    i--;
  }
}

EmberStatus emberSerialPrintf(int8u port, PGM_P format, ...)
{
  va_list vargs;
  EmberStatus stat = EMBER_SUCCESS;

  va_start(vargs, format);
  stat = emberSerialPrintfVarArg(port, format, vargs);
  va_end(vargs);
  return stat;
}

EmberStatus emberSerialPrintfLine(int8u port, PGM_P formatString, ...)
{
  EmberStatus stat = EMBER_SUCCESS;
  va_list ap;
  va_start (ap, formatString);
  stat = emberSerialPrintfVarArg(port, formatString, ap);
  va_end (ap);
  emberSerialPrintCarriageReturn(port);
  return stat;
}


EmberStatus emberSerialPrintCarriageReturn(int8u port)
{
  return emberSerialPrintf(port, "\r\n");
}

EmberStatus emberSerialWriteByte(int8u port, int8u dataByte)
{
  return emberSerialWriteData(port, &dataByte, 1);
}

EmberStatus emberSerialWriteHex(int8u port, int8u dataByte)
{
  int8u hex[2];
  sprintf(hex, "%2x", dataByte);
  return emberSerialWriteData(port, hex, 2);
}

EmberStatus emberSerialWriteData(int8u port, int8u *data, int8u length)
{
  EmberStatus stat = EMBER_ERR_FATAL;
  if (backchannelEnable) {
    if (handleRemoteConnection(port)) {
      // Ideally we expect the "emberSerialWriteData()" call to be atomic,
      // and not block.  We probably should try to determine if the write()
      // will block before executing it.
      stat = backchannelSend(port, data, length);
      if (stat == EMBER_ERR_FATAL) {
        parentCleanupAfterChildDied();
      }
    } // else
      //   failure, fall-thru

  } else {
    // Normal IO
    if (fwrite(data, length, 1, stdout) == 1) {
      stat = EMBER_SUCCESS;
    }
  }
  return stat;
}

EmberStatus emberSerialWaitSend(int8u port)
{
  if (!backchannelEnable) {
    fflush(stdout);
  }
  return EMBER_SUCCESS;
}

//------------------------------------------------------------------------------
// Serial buffer maintenance

void emberSerialFlushRx(int8u port)
{
  char buf;
  while (0 < emberSerialReadAvailable(port)) {
    read(DATA_READER(port), &buf, 1);
  }
}

//------------------------------------------------------------------------------
// Serial Buffer Cleanup Tick

void emberSerialBufferTick(void)
{
}

//------------------------------------------------------------------------------
// Command Completion support

#if READLINE_SUPPORT

// See readline manual for additional details:
//   http://tiswww.case.edu/php/chet/readline/readline.html#SEC45

// Right now we have very limited command completion support.  It only completes
// the first word.  This could be expanded in the future to complete
// other tokens (e.g. complex commands like "network join") or at least print
// help for the next element (e.g. in a command like "network join" the next
// item is supposed to be the channel in decimal format).

// Per readline integration:
// Attempt to complete on the contents of TEXT.  START and END bound the
// region of rl_line_buffer that contains the word to complete.  TEXT is
// the word to complete.  We can use the entire contents of rl_line_buffer
// in case we want to do some simple parsing.  Return the array of matches,
// or NULL if there aren't any.

char** commandCompletion(const char* text, int start, int end)
{
  char** matches;

  matches = (char **)NULL;

  debugPrint("commandCompletion(): %s, start: %d, end: %d\n",
             text,
             start,
             end);

  // If this word is at the start of the line, then it is the first
  // token in a command.  Complete the command.
  if (start == 0) {
    matches = rl_completion_matches (text, singleCommandCompletion);
  } else {
    // Future support: handle complex command-line completions.
  }

  return (matches);
}

// Generator function for command completion.  STATE lets us know whether
// to start from scratch; without any state (i.e. STATE == 0), then we
// start at the top of the list.

char* singleCommandCompletion(const char *text, int state)
{
  static int listIndex;
  static int length;

  if (!state) {
    listIndex = 0;
    length = strnlen(text, MAX_COMMAND_LENGTH + 1);
    // MAX_COMMAND_LENGTH needs to be adjusted if this assert() is hit.
    assert(length <= MAX_COMMAND_LENGTH);
  }

  debugPrint("singleCommandCompletion(): listIndex: %d\n", listIndex);

  while (allCommands[listIndex] != NULL) {
    const char* command = allCommands[listIndex];
    listIndex++;

    // The trick here is that an empty command line has a length of 0,
    // and thus will match in our strncmp()
    if (0 == strncmp(command, text, length)) {
//      debugPrint("singleCommandCompeletion(): found %s\n", command);

      // It is expected that the readline library will free the data.
      return duplicateString(command);
    }
  }

  debugPrint("singleCommandCompeletion(): no more completions\n");
  return (char*)NULL;
}

char* filenameCompletion(const char* text, int state)
{
  // We don't do filename completion.

  // Implementitng this function prevents readline() from using its default
  // implementation, which displays filenames from the CWD.  Not very helpful.
  return (char*)NULL;
}

static char* duplicateString(const char* source)
{
  char* returnData;
  returnData = malloc(strnlen(source, MAX_COMMAND_LENGTH) + 1);

  assert(returnData != NULL);

  strcpy(returnData, source);
  return returnData;
}

static void initializeHistory(void)
{
  int myErrno;
  char* homeDirectory;

  readlineHistoryPath[0] = '\0';

  using_history();  // initialize readline() history.

  homeDirectory = getenv("HOME");
  if (homeDirectory == NULL) {
    debugPrint("Error: HOME directory env variable is not defined.\n");
    return;
  } else {
    debugPrint("Home Directory: %s\n", homeDirectory);
  }

  snprintf(readlineHistoryPath,
           MAX_STRING_LENGTH - 1, 
           "%s/%s", 
           homeDirectory, 
           readlineHistoryFilename);
  readlineHistoryPath[MAX_STRING_LENGTH - 1] = '\0';

  myErrno = read_history(readlineHistoryPath);
  if (myErrno != 0) {
    debugPrint("Could not open history file '%s': %s\n", 
               readlineHistoryPath, 
               strerror(myErrno));
  } else {
    debugPrint("%d history entries read from file '%s'\n", 
               history_length,         // readline global
               readlineHistoryPath);
  }
}

static void writeHistory(void)
{
  int myErrno;
  if (readlineHistoryPath[0] == '\0') {
    debugPrint("Readline history path is empty, not writing history.\n");
    return;
  }
  myErrno = write_history(readlineHistoryPath);
  if (myErrno != 0) {
    debugPrint("Failed to write history file '%s': %s\n",
               readlineHistoryPath,
               strerror(myErrno));
  } else {
    debugPrint("Wrote %d entries to history file '%s'\n",
               history_length,       // readline global
               readlineHistoryPath);
  }
}

#endif // #if READLINE_SUPPORT

//------------------------------------------------------------------------------

/*
Ideally we want the parent process to catch SIGCHLD and then 
cleanup after the child.  If the CLI terminated, the parent
process will also quit.

  The question is, how do I determine which child quit?
  Presumably this can be done with wait(), which returns the
  child's pid and its associated status.

  The one tricky part is the issue with handling the signal
  SIGCHLD during normal shutdown.  I presume that a signal
  handler cannot be fired within a signal handel, but I am
  not certain.  I need a way of telling whether this
  is a graceful shutdown or not.  Basically if the parent
  initiated the shutdown the children should just go away
  and the parent would ignore notification that they did that
  (SIGCHLD).  If the child died unexpectedly then the parent 
  will not ignore that and should nicely cleanup
  and not necessarily exit (in the case of the CLI we presume 
  it would).
*/

static void signalHandler(int signal)
{
  const char* signalName = strsignal(signal);

  debugPrint("Caught signal %s (%d)\n", 
             (signalName == NULL ? "???" : signalName),
             signal);
  if (signal == SIGPIPE) {
    // Ignore this.
    // In general SIGPIPE is useless because it doesn't
    //   give you any context for understanding what read/write from
    //   a pipe caused the error.
    // Instead the send(), receive(), read(), and write() primitives
    //  will return with an error and set errno to EPIPE indicating
    //  that failure.
    return;
  }

  /*
  if (signal == SIGCHLD) {
    if (amParent) {
      parentCleanupAfterChildDied();
    }
    return;
  }
  */

  // Assume that this is only called for SIGTERM and SIGINT
  if (amParent) {
    emberSerialCleanup();
    exit(-1);
  } else {
    childCleanupAndExit();
  }
}

static void installSignalHandler(void)
{
  int i = 0;
  int signals[] = { SIGTERM, SIGINT, SIGPIPE, 0 };

  struct sigaction sigAction;
  MEMSET(&sigAction, 0, sizeof(struct sigaction));
  sigAction.sa_handler = signalHandler;

  while (signals[i] != 0) {
    if (-1 == sigaction(signals[i], &sigAction, NULL)) {
      debugPrint("Could not install signal handler.\n");
    } else {
      const char* signalName = strsignal(signals[i]);
      debugPrint("Installed signal handler for %s (%d).\n", 
                 (signalName == NULL
                  ? "???"
                  : signalName),
                 signals[i]);
    }
    i++;
  }
}

//------------------------------------------------------------------------------
// Debug

static void debugPrint(const char* formatString, ...)
{
  if (debugOn) {
    va_list ap;
    fprintf(stderr, "[linux-serial debug] ");
    va_start (ap, formatString);
    vfprintf(stderr, formatString, ap);
    va_end(ap);
    fflush(stderr);
  }
}
