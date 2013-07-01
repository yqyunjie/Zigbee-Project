/** @file uart-test-1.c
 *  @brief ASH protocol reset and startup test
 * 
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.       *80*-->
 */

#include PLATFORM_HEADER
#include <stdio.h>
#include <stdlib.h>
#include "stack/include/ember-types.h"
#include "hal/micro/generic/ash-protocol.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-ui.h"

static void printResults(EzspStatus status);

int main( int argc, char *argv[] )
{
  int16u savedTimeRst;
  EzspStatus status;
  
  if (!ashProcessCommandOptions(argc, argv)) {
    printf("Exiting.\n");
    return 1;
  }

// Open serial port - verifies the name and permissions
// Disable RTS/CTS since the open might fail if CTS is not asserted.
  printf("\nOpening serial port... ");
  ashWriteConfig(resetMethod, ASH_RESET_METHOD_NONE);
  ashWriteConfig(rtsCts, FALSE);
  status = ashResetNcp();           // opens the serial port
  printResults(status);

// Listen for RSTACK frame from the NCP - this verifies that the NCP is powered
// and not stuck in reset, and for the serial connection it verifies baud rate,
// character structure and that the NCP TXD to host RXD connection works.
  printf("\nManually reset the network co-processor, then press enter:");
  (void)getchar();
  savedTimeRst = ashReadConfig(timeRst);  
  ashWriteConfig(timeRst, 10000);
  status = ashStart();              // waits for RSTACK
  printf("\nWaiting for RSTACK from network co-processor... ");
  printResults(status);

// Send a RST frame to the NCP, and listen for RSTACK coming back - this 
// verifies the data connection from the host TXD to the NCP RXD.
  printf("\nClosing and re-opening serial port... ");
  ashSerialClose();
  ashWriteConfig(timeRst, savedTimeRst);
  ashWriteConfig(resetMethod, ASH_RESET_METHOD_RST);
  status = ashResetNcp();         // opens the serial port
  printResults(status);
  printf("\nSending RST frame to network co-processor... ");
  status = ashStart();            // sends RST and waits for RSTACK
  printResults(status);
  return 0;
} 

static void printResults(EzspStatus status)
{
  if (status == EZSP_SUCCESS) {
    printf("succeeded.\n");
    return;
  } else {
    printf("failed.\n");
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
  }
  exit(1);
}
