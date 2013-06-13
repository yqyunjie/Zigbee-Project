// *****************************************************************************
// * message.c
// *
// * Code for manipulating the incoming and outgoing messages in a flat
// * memory buffer.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/util.h"
#include "app/framework/util/config.h"

//------------------------------------------------------------------------------

// these variables are for storing responses that are created by zcl-utils
// in functions called from emberIncomingMsgHandler. The response is sent 
// from emberAfTick (meant to be called immediately after emberTick).
// There is only space for one response as each call to emberTick will
// only result in a single call to emberIncomingMsgHandler. If the device
// receives multiple ZCL messages, the stack will queue these and hand 
// these to the application via emberIncomingMsgHandler one at a time.
EmberApsFrame      emberAfResponseApsFrame;
EmberNodeId        emberAfResponseDestination;
int8u  appResponseData[EMBER_AF_RESPONSE_BUFFER_LEN];
int16u  appResponseLength;

// Used for empty string
static int16u zeroLenByte = 0;
static int8u* zeroLenBytePtr = (int8u *)&zeroLenByte;

//------------------------------------------------------------------------------
// Utilities for adding bytes to the response buffer: appResponseData. These
// functions take care of incrementing appResponseLength. 

void emberAfClearResponseData(void)
{
  emberAfResponseType = ZCL_UTIL_RESP_NORMAL;
  // To prevent accidentally sending to someone else, 
  // set the destination to ourselves.
  emberAfResponseDestination = emberAfGetNodeId();
  MEMSET(appResponseData, 0, EMBER_AF_RESPONSE_BUFFER_LEN);
  appResponseLength = 0;
  MEMSET(&emberAfResponseApsFrame, 0, sizeof(EmberApsFrame));
}

void emberAfPutInt8uInResp(int8u value)
{
  //emberAfDebugPrint("try %x max %x\r\n", appResponseLength, EMBER_AF_RESPONSE_BUFFER_LEN);
  if (appResponseLength < EMBER_AF_RESPONSE_BUFFER_LEN) {
    //emberAfDebugPrint("put %x at spot %x\r\n", value, appResponseLength);
    appResponseData[appResponseLength] = value;
    appResponseLength++;
  }
}

void emberAfPutInt16uInResp(int16u value)
{
  emberAfPutInt8uInResp(LOW_BYTE(value));
  emberAfPutInt8uInResp(HIGH_BYTE(value));
}

void emberAfPutInt32uInResp(int32u value)
{
  emberAfPutInt8uInResp(BYTE_0(value));
  emberAfPutInt8uInResp(BYTE_1(value));
  emberAfPutInt8uInResp(BYTE_2(value));
  emberAfPutInt8uInResp(BYTE_3(value));
}

void emberAfPutBlockInResp(const int8u* data, int16u length)
{
  if ((appResponseLength + length) < EMBER_AF_RESPONSE_BUFFER_LEN) {
    MEMCOPY(appResponseData + appResponseLength, data, length);
    appResponseLength += length;
  }
}

void emberAfPutStringInResp(const int8u *buffer)
{
  int8u length = emberAfStringLength(buffer);
  emberAfPutBlockInResp(buffer, length + 1);
}

// ------------------------------------
// Utilities for reading from RAM buffers (reading from incoming message
// buffer)
// ------------------------------------

// retrieves an int32u which contains between 1 and 4 bytes of relevent data
// depending on number of bytes requested.
int32u emberAfGetInt(const int8u* message, 
                     int16u currentIndex, 
                     int16u msgLen, 
                     int8u bytes)
{
  int32u result = 0;
  int8u i = bytes;
  if ((currentIndex + bytes) > msgLen) {
    emberAfDebugPrintln("GetInt, %x bytes short", bytes);
    emberAfDebugFlush();
    return 0;
  }
  while (i > 0) {
    result = (result << 8) + message[(currentIndex + i) - 1];
    i--;
  }
  return result;
}

int32u emberAfGetInt32u(const int8u* message, int16u currentIndex, int16u msgLen) 
{
  return emberAfGetInt(message, currentIndex, msgLen, 4);
}

int32u emberAfGetInt24u(const int8u* message, int16u currentIndex, int16u msgLen) 
{
  return emberAfGetInt(message, currentIndex, msgLen, 3);
}

int16u emberAfGetInt16u(const int8u* message, int16u currentIndex, int16u msgLen)
{
  return (int16u)emberAfGetInt(message, currentIndex, msgLen, 2);
}

int8u* emberAfGetString(int8u* message, int16u currentIndex, int16u msgLen)
{
  // Strings must contain at least one byte for the length.
  if (msgLen <= currentIndex) {
    emberAfDebugPrintln("GetString: %p", "buffer too short");
    return zeroLenBytePtr;
  }

  // Starting from the current index, there must be enough bytes in the message
  // for the string and the length byte.
  if (msgLen < currentIndex + emberAfStringLength(&message[currentIndex]) + 1) {
    emberAfDebugPrintln("GetString: %p", "len byte wrong");
    return zeroLenBytePtr;
  }

  return &message[currentIndex];
}

int8u* emberAfGetLongString(int8u* message, int16u currentIndex, int16u msgLen)
{
  // Long strings must contain at least two bytes for the length.
  if (msgLen <= currentIndex + 1) {
    emberAfDebugPrintln("GetLongString: %p", "buffer too short");
    return zeroLenBytePtr;
  }

  // Starting from the current index, there must be enough bytes in the message
  // for the string and the length bytes.
  if (msgLen
      < currentIndex + emberAfLongStringLength(&message[currentIndex]) + 2) {
    emberAfDebugPrintln("GetLongString: %p", "len bytes wrong");
    return zeroLenBytePtr;
  }

  return &message[currentIndex];
}

int8u emberAfStringLength(const int8u *buffer)
{
  // The first byte specifies the length of the string.  A length of 0xFF means
  // the string is invalid and there is no character data.
  return (buffer[0] == 0xFF ? 0 : buffer[0]);
}

int16u emberAfLongStringLength(const int8u *buffer)
{
  // The first two bytes specify the length of the long string.  A length of
  // 0xFFFF means the string is invalid and there is no character data.
  int16u length = emberAfGetInt16u(buffer, 0, 2);
  return (length == 0xFFFF ? 0 : length);
}
