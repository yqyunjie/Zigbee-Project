/** @file uart-test-3.c
 *  @brief EZSP-UART and ASH flow control test
 * 
 * <!-- Copyright 2009-2010 by Ember Corporation. All rights reserved.   *80*-->
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
#include "app/util/ezsp/serial-interface.h"
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

//------------------------------------------------------------------------------
// Preprocessor definitions


//------------------------------------------------------------------------------
// Global Variables


//------------------------------------------------------------------------------
// Forward Declarations


//------------------------------------------------------------------------------
// Test functions

// See if the NCP can hold off the host by sending a frame that will overflow
// the NCP UART receive buffer unless flow control is working. ezspDelayTest()
// adds a delay before the NCP reads the next command from the receive buffer
// to prevent the data from being read out as soon as it is received. This
// simulates delays that occur during heavy network activity.
int main( int argc, char *argv[] )
{
  EmberStatus status;
  int8u protocolVersion;
  int8u stackType;
  int16u ezspUtilStackVersion;
  char *flowType;

  if (!ashProcessCommandOptions(argc, argv)) {
    printf("Exiting.\n");
    return 1;
  }
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
  printf("Checking EZSP version... ");
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

  flowType = ashReadConfig(rtsCts) ? "RTS/CTS" : "XON/XOFF";
  printf("\nPress ENTER to test %s flow control:", flowType);
  fflush(stdout);
  (void)getchar();

  if (serialTestFlowControl() != EZSP_SUCCESS) {
    printf("\nFailed.\n");
    return 1;
  } 
  printf("\nSucceeded.\n");\
  return 0;
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

void ezspTimerHandler(int8u timerId)
{}
