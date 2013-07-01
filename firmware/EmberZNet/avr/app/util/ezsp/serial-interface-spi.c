// File: serial-interface-spi.c
// 
// Description: Implementation of the interface described in serial-interface.h
// using the SPI protocol.
// 
// Copyright 2006 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"

#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/ezsp/ezsp-frame-utilities.h"
#include "hal/micro/avr-atmega/spi-protocol.h"

//------------------------------------------------------------------------------
// Global Variables

static boolean waitingForResponse = FALSE;
int8u *ezspFrameContents;
int8u *ezspFrameLengthLocation;

//------------------------------------------------------------------------------
// Serial Interface Downwards

EzspStatus ezspInit(void)
{
  ezspFrameLengthLocation = hal260Frame;
  ezspFrameContents = hal260Frame + 1;
  return hal260HardReset();
}

boolean ezspCallbackPending(void)
{
  if (!waitingForResponse) {
    return hal260HasData();
  } else {
    return FALSE;
  }
}

void ezspWakeUp(void)
{
  hal260WakeUp();
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
    status = hal260PollForResponse();
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
  hal260SendCommand();
  waitingForResponse = TRUE;
  return EZSP_SUCCESS;
}
