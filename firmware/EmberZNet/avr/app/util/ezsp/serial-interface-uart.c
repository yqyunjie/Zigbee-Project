// File: serial-interface-uart.c
// 
// Description: Implementation of the interface described in serial-interface.h
// using the ASH UART protocol.
// 
// Copyright 2006 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER

#include "stack/include/ember-types.h"

#include "hal/hal.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/serial-interface.h"
#include "hal/micro/generic/ash-protocol.h"
#include "hal/micro/generic/ash-common.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-io.h"
#include "app/ezsp-uart-host/ash-host-priv.h"
#include "app/ezsp-uart-host/ash-host-queues.h"
#include "app/util/ezsp/ezsp-frame-utilities.h"

#define elapsedTimeInt16u(oldTime, newTime)      \
  ((int16u) ((int16u)(newTime) - (int16u)(oldTime)))

//------------------------------------------------------------------------------
// Global Variables

static boolean waitingForResponse = FALSE;
static int16u waitStartTime;
#define WAIT_FOR_RESPONSE_TIMEOUT (ASH_MAX_TIMEOUTS * ashReadConfig(ackTimeMax))

static int8u ezspFrameLength;
int8u *ezspFrameLengthLocation = &ezspFrameLength;
static int8u ezspFrameContentsStorage[EZSP_MAX_FRAME_LENGTH];
int8u *ezspFrameContents = ezspFrameContentsStorage;

//------------------------------------------------------------------------------
// Serial Interface Downwards

EzspStatus ezspInit(void)
{
  EzspStatus status;
  int8u i;
  for (i = 0; i < 5; i++) {
    status = ashResetNcp();
    if (status != EZSP_SUCCESS) {
      return status;
    }
    status = ashStart();
    if (status == EZSP_SUCCESS) {
      return status;
    }
  }
  return status;
}

boolean ezspCallbackPending(void)
{
  return FALSE;
}

void ezspWakeUp(void)
{
  // Not supported.
}

void ezspClose(void)
{
  ashSerialClose();
}

static boolean checkConnection(void)
{
  boolean connected = ashIsConnected();
  if (!connected) {
    // Attempt to restore the connection. This will reset the EM260.
    ezspClose();
    ezspInit();
  }
  return connected;
}

int8u serialPendingResponseCount(void)
{
  return ashQueueLength(&rxQueue);
}

EzspStatus serialResponseReceived(void)
{
  EzspStatus status;
  AshBuffer *buffer;
  AshBuffer *dropBuffer = NULL;
  if (!checkConnection()) {
    ashTraceEzspVerbose("serialResponseReceived(): EZSP_ASH_NOT_CONNECTED");
    return EZSP_ASH_NOT_CONNECTED;
  }
  ashSendExec();
  status = ashReceiveExec();
  if (status != EZSP_SUCCESS
      && status != EZSP_ASH_IN_PROGRESS
      && status != EZSP_ASH_NO_RX_DATA) {
    ashTraceEzspVerbose("serialResponseReceived(): ashReceiveExec(): 0x%x",
                        status);
    return status;
  }
  if (waitingForResponse
      && elapsedTimeInt16u(waitStartTime, halCommonGetInt16uMillisecondTick())
         > WAIT_FOR_RESPONSE_TIMEOUT) {
    waitingForResponse = FALSE;
    ashTraceEzspFrameId("no response", ezspFrameContents);
    ashTraceEzspVerbose("serialResponseReceived(): EZSP_ERROR_NO_RESPONSE");
    return EZSP_ERROR_NO_RESPONSE;
  }
  status = EZSP_ASH_NO_RX_DATA;
  buffer = ashQueuePrecedingEntry(&rxQueue, NULL);
  while (buffer != NULL) {
    // While we are waiting for a response to a command, we use the frame ID
    // to ignore callbacks. This allows our caller to assume that no
    // callbacks will appear between sending a command and receiving its
    // response.
    if (waitingForResponse
        && buffer->data[EZSP_FRAME_ID_INDEX]
           != ezspFrameContents[EZSP_FRAME_ID_INDEX]
        && buffer->data[EZSP_FRAME_ID_INDEX]
           != EZSP_INVALID_COMMAND) {
      if (ashFreeListLength(&rxFree) == 0) {
        dropBuffer = buffer;
      }
      buffer = ashQueuePrecedingEntry(&rxQueue, buffer);
    } else {
      ashTraceEzspVerbose("serialResponseReceived(): ID=0x%x Seq=0x%x Buffer=%u",
                          buffer->data[EZSP_FRAME_ID_INDEX],
                          buffer->data[EZSP_SEQUENCE_INDEX],
                          buffer);
      ashRemoveQueueEntry(&rxQueue, buffer);
      memcpy(ezspFrameContents, buffer->data, buffer->len);  
      ashTraceEzspFrameId("got response", buffer->data);
      ezspFrameLength = buffer->len;
      ashFreeBuffer(&rxFree, buffer);
      ashTraceEzspVerbose("serialResponseReceived(): ashFreeBuffer(): %u", buffer);
      buffer = NULL;
      status = EZSP_SUCCESS;
      waitingForResponse = FALSE;
    }
  }
  if (dropBuffer != NULL) {
    ashRemoveQueueEntry(&rxQueue, dropBuffer);
    ashFreeBuffer(&rxFree, dropBuffer);
    ashTraceEzspFrameId("dropping", dropBuffer->data);
    ashTraceEzspVerbose("serialResponseReceived(): ashFreeBuffer(): drop %u", dropBuffer);
    ashTraceEzspVerbose("serialResponseReceived(): ezspErrorHandler(): EZSP_ERROR_QUEUE_FULL");
    ezspErrorHandler(EZSP_ERROR_QUEUE_FULL);
  }
  return status;
}

EzspStatus serialSendCommand()
{
  EzspStatus status;
  if (!checkConnection()) {
    ashTraceEzspVerbose("serialSendCommand(): EZSP_ASH_NOT_CONNECTED");
    return EZSP_ASH_NOT_CONNECTED;
  }
  ashTraceEzspFrameId("send command", ezspFrameContents);
  status = ashSend(ezspFrameLength, ezspFrameContents);
  if (status != EZSP_SUCCESS) {
    ashTraceEzspVerbose("serialSendCommand(): ashSend(): 0x%x", status);
    return status;
  }
  waitingForResponse = TRUE;
  ashTraceEzspVerbose("serialSendCommand(): ID=0x%x Seq=0x%x",
                      ezspFrameContents[EZSP_FRAME_ID_INDEX],
                      ezspFrameContents[EZSP_SEQUENCE_INDEX]);
  waitStartTime = halCommonGetInt16uMillisecondTick();
  return status;
}
