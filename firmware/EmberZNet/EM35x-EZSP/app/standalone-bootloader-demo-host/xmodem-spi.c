/** @file app/sensor-host/xmodem-spi.c
 *  @brief Example AVR XMODEM over SPI Protocol implementation for interfacing
 *  with the EM260.
 *
 *
 * XMODEM over SPI Protocol Interface:
 *
 *  get version info via spi
 *    int8u * XModemQuery(int8u * pLen, boolean reportError);
 *
 *  Run the bootload protocol
 *    void bootTick(void);
 *
 *  Send a bootload message
 *    EzspStatus bootSendMessage(int8u len, int8u * bytes);
 *
 *  Wait for the em260 to finish a reboot.
 *    EzspStatus waitFor260boot(int16u * pLoops);
 *
 * <!-- Author(s): ArrAy, Inc -->
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.        *80*-->
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "stack/include/error.h"

#include "hal/micro/bootloader-interface-standalone.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/ezsp-utils.h"
#include "app/util/ezsp/serial-interface.h"

#include "app/standalone-bootloader-demo-host/xmodem-spi.h"
#include "app/util/bootload/bootload-utils-internal.h"
#ifdef HAL_HOST
  #include "hal/host/spi-protocol-common.h"
#endif //HAL_HOST

// This is an extension of the BLOCKERR_MASK etc from the various
// standalone bootloader files.
#define BLOCKERR_RESPONSE   0x28   // the response from EM260 doesn't match
                                   // the bootload protocol value.

#define XMODEM_FRAME_SZ     133    // a whole Frame of a xmodem block's size
#define XMODEM_FRAME_START_INDEX 2 // the start index of real Xmodem frame in
                                   // Xmodem-over-spi protocol


//extern varible
extern int8u halNcpSpipBuffer[];

//internal info
EzspStatus lastSendStat;

//pass throuth the block to the EM260 via SPI
static EzspStatus sendBlockToEm260(int8u * pxStat)
{
  EzspStatus status;

  //mark the command type
  halNcpSpipBuffer[0] = 0xFD; //Bootloader payload

  //mark the xmodem frame length
  if(halNcpSpipBuffer[2] == XMODEM_SOH) {
    halNcpSpipBuffer[1] = XMODEM_FRAME_SZ;
  } else { //else command type is XMODEM_EOT or XMODEM_CANCEL
    halNcpSpipBuffer[1] = 1;
  }

  halResetWatchdog(); // Full time

  //send the xmodem frame to EM260 over spi
  halNcpSendRawCommand();

  //wait for response of EM260.
  halResetWatchdog(); // Full time
  status = halNcpPollForResponse();
  while (status == EZSP_SPI_WAITING_FOR_RESPONSE) {
    status = halNcpPollForResponse();
  }
  
  lastSendStat = status;

  //return the EM260 response status
  if((status != EZSP_SUCCESS) || (halNcpSpipBuffer[0] != 0xFD))
    *pxStat = BLOCKERR_RESPONSE;
  else
    *pxStat = halNcpSpipBuffer[2];
  return status;
}

int8u * XModemQuery(int8u * pLen, boolean reportError)
{
  EzspStatus status;
  int8u xStat;

  halNcpSpipBuffer[2] = XMODEM_QUERY;
  status = sendBlockToEm260(&xStat);

  *pLen = 255;
  if (status != EZSP_SUCCESS) {
    if (reportError)
      ezspErrorHandler(status);
    return NULL;
  }

  // Now poll for response
  while(!halNcpHasData()) {
    // halCommonDelayMicroseconds(1); // ...
  }

  halNcpSpipBuffer[2] = XMODEM_QUERY;
  status = sendBlockToEm260(&xStat);

  *pLen = 254;
  if (status != EZSP_SUCCESS) {
    if (reportError)
      ezspErrorHandler(status);
    return NULL;
  }

  *pLen = halNcpSpipBuffer[1];
  return &halNcpSpipBuffer[2];
}

void bootTick(void)
{
  int8u messageLength;
  EzspStatus status;
  int8u xStat;

  if (halNcpHasData()) {
    halNcpSpipBuffer[2] = XMODEM_QUERY;
    status = sendBlockToEm260(&xStat);
    if (status != EZSP_SUCCESS)
      ezspErrorHandler(status);
    if (xStat != BLOCKERR_RESPONSE) {
      messageLength = halNcpSpipBuffer[1] + 1;
      halNcpSpipBuffer[1] = BOOTLOAD_PROTOCOL_VERSION;
      ezspIncomingBootloadMessageHandler(
        emberGetEui64(),
        255,
        -128,
        messageLength,
        &halNcpSpipBuffer[1]);
    }
  }
}

EzspStatus bootSendMessage(int8u len, int8u * bytes)
{
  EzspStatus status = EZSP_SPI_ERR_FATAL;
  int8u xStat;

  if (len > 1) {
    MEMCOPY(&halNcpSpipBuffer[XMODEM_FRAME_START_INDEX], &bytes[1], len-1);
    status = sendBlockToEm260(&xStat);
    if (status != EZSP_SUCCESS)
      ezspErrorHandler(status);
  }
  return status;
}

EzspStatus waitFor260boot(int16u * pLoops)
{
  int16u j;
  boolean spiActive = FALSE;

  for (j=0; (j<40000) && !halNcpHasData(); j++) {
    halResetWatchdog();
    halCommonDelayMicroseconds(100);
  }
  *pLoops = j;

  // the first call is expected to fail right after a reset, 
  //  since the SPI is reporting the error that a reset occurred.
  halNcpVerifySpiProtocolActive();

  // the second check should return true but sometimes we have to
  // wait a few more times (do 10 for safety)
  for (j = 0; j < 10 && !spiActive; j++) {
    halResetWatchdog();
    halCommonDelayMicroseconds(100);
    spiActive = halNcpVerifySpiProtocolActive();
  }

  // Make sure that the SPI activated again
  if(spiActive == FALSE)
    return EZSP_SPI_ERR_STARTUP_FAIL;

  // Get the SPI protocol version just to make sure the SPI is running
  if(halNcpVerifySpiProtocolVersion()!=TRUE)
    return EZSP_SPI_ERR_STARTUP_FAIL;
  
  return EZSP_SUCCESS;  
}
