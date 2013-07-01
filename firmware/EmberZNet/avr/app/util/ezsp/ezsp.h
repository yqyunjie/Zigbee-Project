// File: ezsp.h
// 
// Description: Host EZSP layer. Provides functions that allow the Host
// application to send every EZSP command to the EM260. The command and response
// parameters are defined in the datasheet.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_H__
#define __EZSP_H__

// Reset the EM260 and initialize the serial protocol (SPI or UART). After this
// function returns EZSP_SUCCESS, the EM260 has finished rebooting and is ready
// to accept a command.
EzspStatus ezspInit(void);

// Returns TRUE if there are one or more callbacks queued up on the EM260
// awaiting collection.
boolean ezspCallbackPending(void);

// The sleep mode to use in the frame control of every command sent. The Host
// application can set this to the desired EM260 sleep mode. Subsequent commands
// will pass this value to the EM260.
extern int8u ezspSleepMode;

// Wakes the EM260 up from deep sleep.
void ezspWakeUp(void);

// The Host application must call this function periodically to allow the EZSP
// layer to handle asynchronous events.
void ezspTick(void);

// The EZSP layer calls this function after sending a command while waiting for
// the response. The Host application can use this function to perform any tasks
// that do not involve the EM260.
void ezspWaitingForResponse(void);

// Callback from the EZSP layer indicating that the transaction with the EM260
// could not be completed due to a serial protocol error or that the response
// received from the EM260 reported an error. The status parameter provides more
// information about the error. This function must be provided by the Host
// application.
void ezspErrorHandler(EzspStatus status);

// Cleanly close down the serial protocol (SPI or UART). After this function has
// been called, ezspInit() must be called to resume communication with the
// EM260.
void ezspClose(void);

//----------------------------------------------------------------

#include "command-prototypes.h"

#endif // __EZSP_H__
