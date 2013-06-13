// File: gateway-support.c
//
// Description:  Gateway specific behavior for a host application.
//   In this case we assume our application is running on
//   a PC with Unix library support, connected to an NCP via serial uart.
//
// Author(s): Rob Alexander <ralexander@ember.com>
//
// Copyright 2012 by Ember Corporation.  All rights reserved.               *80*
//
//------------------------------------------------------------------------------

#include "app/framework/include/af.h"
#include "gateway-callback.h"

#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

#include "app/util/serial/command-interpreter2.h"
#include "app/util/serial/linux-serial.h"
#include "app/framework/plugin/gateway/gateway-support.h"

#include <sys/time.h>   // for select()
#include <sys/types.h>  // ""
#include <unistd.h>     // ""
#include <errno.h>      // ""

//------------------------------------------------------------------------------
// Globals

// This defines how long select() will wait for data.  0 = wait forever.
// Because ASH needs to check for timeout in messages sends, we
// must periodically wakeup.  Timeouts are rare but could occur.
#define READ_TIMEOUT_MS  100
#define MAX_FDS 10
#define INVALID_FD -1

static const char* debugLabel = "gateway-debug";
static const boolean debugOn = FALSE;

static const char cliPrompt[] = ZA_PROMPT;

//------------------------------------------------------------------------------
// External Declarations

//------------------------------------------------------------------------------
// Forward Declarations

static void getFdsToWatch(int* list, int maxSize);
static void debugPrint(const char* formatString, ...);

//------------------------------------------------------------------------------
// Functions

static EmberStatus gatewayBackchannelStart(void)
{
  if (backchannelEnable) {
    if (EMBER_SUCCESS != backchannelStartServer(SERIAL_PORT_CLI)) {
      fprintf(stderr, 
              "Fatal: Failed to start backchannel services for CLI.\n");
      return EMBER_ERR_FATAL;
    }

    if (EMBER_SUCCESS != backchannelStartServer(SERIAL_PORT_RAW)) {
      fprintf(stderr, 
              "Fatal: Failed to start backchannel services for RAW data.\n");
      return EMBER_ERR_FATAL;
    }
  }
  return EMBER_SUCCESS;
}

void gatewayBackchannelStop(void)
{
  if (backchannelEnable) {
      backchannelStopServer(SERIAL_PORT_CLI);
      backchannelStopServer(SERIAL_PORT_RAW);
  }
}

boolean emberAfMainStartCallback(int* returnCode,
                                 int argc, 
                                 char** argv)
{
  debugPrint("gatewaitInit()");

  // This will process EZSP command-line options as well as determine
  // whether the backchannel should be turned on.
  if (!ashProcessCommandOptions(argc, argv)) {
    return 1;
  }

  *returnCode = gatewayBackchannelStart();
  if (*returnCode != EMBER_SUCCESS) {
    return TRUE;
  }

  if (cliPrompt != NULL) {
    emberSerialSetPrompt(cliPrompt);
  }
  
  emberSerialCommandCompletionInit(emberCommandTable);
  return FALSE;
}

// Rather than looping like a simple em250 application we can actually
// do the proper thing here, which is wait for EZSP or CLI events to fire.
// This is done via our good friend select().  As a warning, the select() call
// may not work in certain scenarios (I'm looking at you Cygwin) and thus one
// will have to fall back to polling loops.

static void gatewayWaitForEventsWithTimeout(int32u timeoutMs)
{
  static boolean debugPrintedOnce = FALSE;
  int fdsWithData;
  int fdsToWatch[MAX_FDS];
  fd_set readSet;
  int highestFd = 0;
  int i;

  timeoutMs = (timeoutMs < READ_TIMEOUT_MS
               ? timeoutMs
               : READ_TIMEOUT_MS);

  if (timeoutMs == 0) {
    // Prevent waiting forever (see select() call below).  
    // This causes problems due to synchronization between the parent and child processes.
    timeoutMs = READ_TIMEOUT_MS;
  }

  struct timeval timeoutStruct = { 
    0,                      // seconds
    timeoutMs * 1000        // micro seconds
  };

  static boolean firstRun = TRUE;
  if (firstRun) {
    firstRun = FALSE;
    debugPrint("gatewayWaitForEvents() first run, not waiting for data.");
    return;
  }

  FD_ZERO(&readSet);
  getFdsToWatch(fdsToWatch, MAX_FDS);

  for (i = 0; i < MAX_FDS; i++) {
    if (fdsToWatch[i] != INVALID_FD) {
      if (!debugPrintedOnce) {
        debugPrint("Watching FD %d for data.", fdsToWatch[i]);
        debugPrint("Current Highest FD: %d", highestFd);
      }
      FD_SET(fdsToWatch[i], &readSet);
      if (fdsToWatch[i] > highestFd) {
        highestFd = fdsToWatch[i];
      }
    }
  }
  if (!debugPrintedOnce) {
    debugPrint("Highest FD: %d", highestFd);
  }
  debugPrintedOnce = TRUE;

  //  debugPrint("calling select(): %d", timeoutMs);
  fdsWithData = select(highestFd + 1,           // per select() manpage
                       &readSet,                // read FDs
                       NULL,                    // write FDs
                       NULL,                    // exception FDs
                       (timeoutMs > 0           // passing NULL means wait 
                        ? &timeoutStruct        //   forever
                        : NULL));
  if (fdsWithData < 0) {
    fprintf(stderr, "FATAL: select() returned error: %s\n",
            strerror(errno));
    for (i = 0; i < MAX_FDS; i++) {
      if (fdsToWatch[i] != INVALID_FD) {
        debugPrint("Was Watching FD %d for data.", fdsToWatch[i]);
      }
    }
    debugPrint("Highest FD: %d", highestFd);
    assert(FALSE);
  }

  if (fdsWithData != 0) {
    debugPrint("data is ready to read");
  }
}

int32u emberAfCheckForSleepCallback(void)
{
  int32u start = halCommonGetInt32uMillisecondTick();
  gatewayWaitForEventsWithTimeout(emberAfMsToNextEvent(0xFFFFFFFFUL));
  return elapsedTimeInt32u(start, halCommonGetInt32uMillisecondTick());
}

static void getFdsToWatch(int* list, int maxSize)
{
  int i = 0;
  MEMSET(list, 0xFF, sizeof(int) * maxSize);
  list[i++] = emberSerialGetInputFd(0);
  list[i++] = emberSerialGetInputFd(1);
  list[i++] = ashSerialGetFd();

  i += emberAfPluginGatewaySelectFileDescriptorsCallback(&(list[i]),
                                                         maxSize - i);

  assert(maxSize >= i);
}

static void debugPrint(const char* formatString, ...)
{
  if (debugOn) {
    va_list ap;
    fprintf(stderr, "[%s] ", debugLabel);
    va_start (ap, formatString);
    vfprintf(stderr, formatString, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fflush(stderr);
  }
}
