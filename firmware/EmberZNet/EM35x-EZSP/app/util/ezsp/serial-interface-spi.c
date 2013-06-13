// File: serial-interface-spi.c
// 
// Description: Implementation of the interface described in serial-interface.h
// using the SPI protocol.
// 
// Copyright 2006-2010 by Ember Corporation. All rights reserved.           *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"

#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/ezsp/ezsp-frame-utilities.h"
#ifdef HAL_HOST
  #include "hal/host/spi-protocol-common.h"
#else //HAL_HOST
  #include "hal/micro/avr-atmega/spi-protocol.h"
#endif //HAL_HOST

//------------------------------------------------------------------------------
// Global Variables

static boolean waitingForResponse = FALSE;
int8u *ezspFrameContents;
int8u *ezspFrameLengthLocation;

//------------------------------------------------------------------------------
// Serial Interface Downwards

EzspStatus ezspInit(void)
{
  ezspFrameLengthLocation = halNcpFrame;
  ezspFrameContents = halNcpFrame + 1;
  return halNcpHardReset();
}

boolean ezspCallbackPending(void)
{
  if (!waitingForResponse) {
    return (halNcpHasData() || ncpHasCallbacks);
  } else {
    return FALSE;
  }
}

void ezspWakeUp(void)
{
  halNcpWakeUp();
}

void ezspClose(void)
{
  // Nothing to do.
}

int8u serialPendingResponseCount(void)
{
  return 0;
}

EzspStatus serialResponseReceived(void)
{
  if (waitingForResponse) {
    EzspStatus status;
    status = halNcpPollForResponse();
    if (status != EZSP_SPI_WAITING_FOR_RESPONSE) {
      waitingForResponse = FALSE;
    }
    return status;
  } else {
    return EZSP_SPI_WAITING_FOR_RESPONSE;
  }
}

EzspStatus serialSendCommand()
{
  halNcpSendCommand();
  waitingForResponse = TRUE;
  return EZSP_SUCCESS;
}
