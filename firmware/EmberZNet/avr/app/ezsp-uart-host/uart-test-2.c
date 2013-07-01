/** @file uart-test-2.c
 *  @brief EZSP-UART and ASH protocol test
 * 
 * <!-- Copyright 2008 by Ember Corporation. All rights reserved.       *80*-->
 */

#include PLATFORM_HEADER
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h> 
#include <string.h> 
#include <termios.h>
#include <unistd.h>
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "hal/micro/system-timer.h"
#include "hal/micro/generic/ash-protocol.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

//------------------------------------------------------------------------------
// Preprocessor definitions

#define MAX_TEST_NUMBER (sizeof(testParams)/sizeof(testParams[0]))

//------------------------------------------------------------------------------
// Global Variables

int timerCount;               // counts calls to ezspTimerHandler()
int overflowErrors;           // NCP overflow errors
int overrunErrors;            // NCP overrun errors
int framingErrors;            // NCP framing errors
int xoffsSent;                // NCP XOFFs sent
AshCount myCount;             // host counters

const char headerText[] =
"\n"
"Test     Repeat  Data Length   Timer\n"
"Number   Count   Min    Max    Period\n"
"                               (msec)\n";
const char testText[] =
"%3d      %4d    %3d    %3d   %4d\n";

const char footerText[] =
"(repeat count 0 means run forever)\n"
"(timer period 0 means no timer callbacks)\n";

typedef struct
{ int reps,           // repetitions of test - 0 means to run forever
      start,          // minimum payload length in bytes
      end,            // maximum payload length in bytes
      timer;          // timer callback period in msec (0 disables timer)
} TestParams;

const TestParams testParams[] =
//    reps        start       end       timer
{ {   10,         10,         10,       0   },
  {   50,         100,        100,      0   },
  {   200,        1,          100,      0   },
  {   200,        1,          100,      100 },
  {   200,        1,          100,      40  },
  {   200,        1,          100,      25  },
  {   0,          10,         10,       0   },
  {   0,          1,          100,      0   },
  {   0,          1,          100,      100 },
  {   0,          1,          100,      40  },
  {   0,          1,          100,      25  }
};

char kbin[10];        // keyboard input buffer
  
//------------------------------------------------------------------------------
// Forward Declarations

static void echoTest(int reps, int start, int end, int timer);
static boolean sendAndCheckEcho(int8u sndLen);
static void printNcpCounts(boolean displayXoffs);
static void readErrorCounts(void);
static void setKbNonblocking(void);
static boolean kbInput(void);

//------------------------------------------------------------------------------
// Test functions

int main( int argc, char *argv[] )
{
  EmberStatus status;
  int8u protocolVersion;
  int8u stackType;
  int16u ezspUtilStackVersion;
  char c;
  int i, test;
  const TestParams *t;
  boolean prompt = TRUE;

  if (!ashProcessCommandOptions(argc, argv)) {
    printf("Exiting.\n");
    return 1;
  }

  setKbNonblocking();
  printf("\nOpening serial port and initializing EZSP... ");
  fflush(stdout);
  status = ezspInit();
  ezspTick();
  if (status == EZSP_SUCCESS) {
    printf("succeeded.\n");
  } else {
    printf("EZSP error: 0x%02X = %s.\n", status, ashEzspErrorString(status));
    ezspClose();
    return 1;
  }

  printf("\nChecking EZSP version... ");
  fflush(stdout);
  protocolVersion = ezspVersion(EZSP_PROTOCOL_VERSION,
                              &stackType,
                              &ezspUtilStackVersion);
  ezspTick();
  if ( protocolVersion != EZSP_PROTOCOL_VERSION ) {
    printf("failed.\n");
    printf("Expected NCP EZSP version %d, but read %d.\n", 
            EZSP_PROTOCOL_VERSION, protocolVersion);
    ezspClose();
    return 1;
  }
  printf("succeeded.\n");

  ashWriteConfig(traceFlags, (ashReadConfig(traceFlags) | TRACE_EVENTS));

  while (TRUE) {
    ezspTick();
    if (prompt) {
      printf("\n\nEnter test number (1 to %d), s for statistics or q to quit: ",
              MAX_TEST_NUMBER);
      fflush(stdout);
      prompt = FALSE;
    }
    if (kbInput()) {
      prompt = TRUE;
      c = tolower(kbin[0]);
      if ( (c == 's') || (c == 'x') ) {
        printf("\n");
        ashPrintCounters(&myCount, FALSE);
        printf("\n");
        printNcpCounts(c == 'x');
      } else if (c == 'q') {
        ezspClose();
        return 0;
      } else if ( sscanf(kbin, "%d", &test) == 1 ) {
        if ( (test > 0) && (test <= MAX_TEST_NUMBER)) {
          t = &testParams[test-1];
          printf("\nTest %d\n", test);
          echoTest(t->reps, t->start, t->end, t->timer);
        } else {
          printf("Invalid test number.\n");
        }
      } else {
        printf(headerText);
        for (i = 1; i <= MAX_TEST_NUMBER; i++) {
          t = &testParams[i-1];
          printf(testText, i, t->reps, t->start, t->end, t-> timer);
        }
        printf(footerText);
      }
    }
  }
  return 0;
}

static void echoTest(int reps, int start, int end, int timer)
{
  int i, j;
  boolean passed;
  boolean stopped;
  int32u startTime;
  int32u elapsedTime;

  if (reps) {
    printf("Sending %d ezspEcho commands ", reps);
  } else {
    printf("Sending an infinite number of ezspEcho commands ");
  }
    if (start == end) {
    printf("with %d bytes of data", start);
  } else {
    printf("with %d to %d bytes of data", start, end);
  }    
  if (timer) {
    printf(",\nwith timer callbacks every %d milliseconds", timer);
    timerCount = 0;
    ezspSetTimer(0, timer, EMBER_EVENT_MS_TIME, TRUE);
  }
  printf("... ");
  fflush(stdout);

  ashClearCounters(&ashCount);
  xoffsSent = overflowErrors = overrunErrors = framingErrors = 0;
  startTime = halCommonGetInt32uMillisecondTick();
  for ( i = 0, j = start, passed = TRUE, stopped = FALSE; 
        ((i < reps) || (reps == 0)) && passed && !stopped;
        i++, j++) {
    if (j > end) {
      j = start;
    }
    passed = sendAndCheckEcho(j);
    stopped = kbInput();
  }
  myCount = ashCount;
  elapsedTime = halCommonGetInt32uMillisecondTick() - startTime;
  if (timer) {
    ezspSetTimer(0, 0, EMBER_EVENT_MS_TIME, FALSE);
  }
  
  readErrorCounts();

  if (!stopped) {
    if (passed) {
      printf("succeeded.\n");
    } else {
      printf("failed.\n");
    }
  } else {
    printf("Stopped by keyboard input.\n");
    printf("Sent %d ezspEcho commands.\n", i);
  }
  if (timer) {
    printf("Received %d timer callbacks.\n", timerCount);
  }
  printf("Elapsed time: %d.%03d seconds.\n", 
    elapsedTime / 1000, elapsedTime % 1000);
}

static boolean sendAndCheckEcho(int8u sndLen)
{
  int8u sndBuf[256], recBuf[256];
  int8u recLen;
  int8u data = 0xFF;
  int i;
  int errors;
  boolean status;

  memset(sndBuf, 0xAA, sizeof(sndBuf));
  memset(recBuf, 0xAA, sizeof(recBuf));
  for (i = 0; i < sndLen; i++) {
    sndBuf[i] = data--;
  }
  recLen = ezspEcho(sndLen, sndBuf, recBuf);
  ezspTick();
  status = (recLen == sndLen) && !memcmp(sndBuf, recBuf, sizeof(sndBuf));
  if (!status) {
    for (errors = 0, i = 0; i < sizeof(sndBuf); i++) {
      if (sndBuf[i] != recBuf[i]) {
        errors++;
      }
    }
    printf("ezspEcho() failed!\n"); 
    printf("Sent %d bytes, received %d, %d different\n", sndLen, recLen, errors);
  }
  return status;
}

static void printNcpCounts(boolean displayXoffs)
{
  printf("NCP Counts\n");
  printf("Overflow errors  %10d\n",  overflowErrors);
  printf("Overrun errors   %10d\n",  overrunErrors);
  printf("Framing errors   %10d\n",  framingErrors);
  if (displayXoffs) {
    printf("XOFFs sent       %10d\n",  xoffsSent);
  }
}

static void readErrorCounts(void)
{
  int16u values[EMBER_COUNTER_TYPE_COUNT];
  ezspReadAndClearCounters(values);
  overflowErrors = values[EMBER_COUNTER_ASH_OVERFLOW_ERROR];
  overrunErrors = values[EMBER_COUNTER_ASH_OVERRUN_ERROR];
  framingErrors = values[EMBER_COUNTER_ASH_FRAMING_ERROR];  
  xoffsSent = values[EMBER_COUNTER_UTILITY];
}

static void setKbNonblocking(void)
{
  int fd = fileno(stdin);
  int flags = fcntl(fileno(stdin), F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static boolean kbInput(void)
{
  return ( read(fileno(stdin), kbin, sizeof(kbin)) > 0 );
}

void ezspErrorHandler(EzspStatus status)
{
  switch (status) {
  case EZSP_ASH_HOST_FATAL_ERROR:
    printf("Host error: %s (0x%02X).\n", ashErrorString(ashError), ashError);
    break;
  case EZSP_ASH_NCP_FATAL_ERROR:
    printf("NCP error: %s (0x%02X).\n", ashErrorString(ncpError), ncpError);
    break;
  default:
    printf("\nEZSP error: %s (0x%02X).\n", ashEzspErrorString(status), status);
    break;
  }
  printf("Exiting.\n");
  exit(1);
}

void ezspTimerHandler(int8u timerId)
{
  timerCount++;
}

//------------------------------------------------------------------------------
// EZSP callback function stubs

void ezspStackStatusHandler(
      EmberStatus status) 
{}

void ezspNetworkFoundHandler(EmberZigbeeNetwork *networkFound,
                             int8u lastHopLqi,
                             int8s lastHopRssi)
{}

void ezspScanCompleteHandler(
      int8u channel,
      EmberStatus status) 
{}

void ezspScanErrorHandler(
      EmberStatus status)
{}

void ezspMessageSentHandler(
      EmberOutgoingMessageType type,
      int16u indexOrDestination,
      EmberApsFrame *apsFrame,
      int8u messageTag,
      EmberStatus status,
      int8u messageLength,
      int8u *messageContents)
{}

void ezspIncomingMessageHandler(
      EmberIncomingMessageType type,
      EmberApsFrame *apsFrame,
      int8u lastHopLqi,
      int8s lastHopRssi,
      EmberNodeId sender,
      int8u bindingIndex,
      int8u addressIndex,
      int8u messageLength,
      int8u *messageContents) 
{}
