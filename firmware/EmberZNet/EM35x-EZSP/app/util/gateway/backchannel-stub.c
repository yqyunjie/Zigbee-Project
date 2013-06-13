// File: backchannel-stub.c
//
// Description: Stub code for simulating an ISA backchannel for EZSP host 
//   applications.
//
// Author(s): Rob Alexander <ralexander@ember.com>
//
// Copyright 2009 by Ember Corporation.  All rights reserved.               *80*
//------------------------------------------------------------------------------

#include PLATFORM_HEADER //compiler/micro specifics, types

#include "stack/include/ember-types.h"
#include "stack/include/error.h"

//------------------------------------------------------------------------------

const boolean backchannelSupported = FALSE;
boolean backchannelEnable = FALSE;
int backchannelSerialPortOffset = 0;

//------------------------------------------------------------------------------

EmberStatus backchannelStartServer(int8u port)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelStopServer(int8u port)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

int backchannelReceive(int8u port, char* data)
{
  return -1;
}

EmberStatus backchannelSend(int8u port, int8u * data, int8u length)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelGetConnection(int8u port, 
                                     boolean remapStdinStdout)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelCloseConnection(int8u port)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelMapStandardInputOutputToRemoteConnection(int port)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelServerPrintf(const char* formatString, ...)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelClientPrintf(int8u port, const char* formatString, ...)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

EmberStatus backchannelClientVprintf(int8u port, 
                                     const char* formatString, 
                                     va_list ap)
{
  return EMBER_LIBRARY_NOT_PRESENT;
}

