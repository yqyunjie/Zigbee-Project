// File: gateway-stub.c
///
// Description:  Stubbed gateway specific behavior for a host application.
//   In this case we assume our application is running on
//   a PC with Unix library support, connected to a 260 via serial uart.
//
// Author(s): Rob Alexander <ralexander@ember.com>
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*
//
//------------------------------------------------------------------------------

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"
#include "stack/include/error.h"

#include "hal/hal.h"

#include "app/util/serial/serial.h"
#include "app/util/serial/command-interpreter.h"
#include "app/util/serial/cli.h"                 // not used, but needed for 

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------
// External Declarations

//------------------------------------------------------------------------------
// Forward Declarations

//------------------------------------------------------------------------------
// Functions

// This function signature different than the non-stub version because of
// cross-platform compatibility.  We assume that those host applications without
// arguments to main (i.e. embedded ones) will use this stub code.

EmberStatus gatewayInit( void )
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus gatewayCommandInterpreterInit(PGM_P cliPrompt,
                                          EmberCommandEntry commands[])
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus gatewayCliInit(PGM_P cliPrompt,
                           cliSerialCmdEntry cliCmdList[],
                           int cliCmdListLength)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

void gatewayWaitForEvents(void)
{
}

void gatewayWaitForEventsWithTimeout(int32u timeoutMs)
{
}
