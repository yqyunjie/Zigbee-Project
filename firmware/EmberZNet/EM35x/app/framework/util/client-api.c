// Copyright 2009 by Ember Corporation. All rights reserved.
//
// This file contains the C portion of the client api.
//

#include PLATFORM_HEADER
#include "common.h"
#include "util.h"
#include "client-api.h"

int8u *emAfZclBuffer = NULL;
int16u emAfZclBufferLen = 0;

// Pointer to where this API should put the length
int16u *emAfResponseLengthPtr = NULL;

// Pointer to where the API should put the cluster ID
EmberApsFrame *emAfCommandApsFrame = NULL;

/////////////////

// Method that fills in the buffer.
// It returns number of bytes filled
// and 0 on error.
static int16u vFillBuffer(int8u *buffer,
                          int16u bufferLen,
                          int8u frameControl,
                          int16u manufacturerCode,
                          int8u commandId,
                          PGM_P format,
                          va_list argPointer)
{
  int32u value;
  int8u valueLen;
  int8u *data;
  int16u dataLen;
  int8u i;
  int16u bytes = 0;

  // The destination buffer must be at least large enough to hold the ZCL
  // overhead: frame control, manufacturer code (if applicable), sequence
  // number, and command id.  If it is, add these in order.
  if (bufferLen < EMBER_AF_ZCL_OVERHEAD
      || (manufacturerCode != EMBER_AF_NULL_MANUFACTURER_CODE
          && bufferLen < EMBER_AF_ZCL_MANUFACTURER_SPECIFIC_OVERHEAD)) {
    emberAfDebugPrintln("ERR: Buffer too short for ZCL header");
    return 0;
  }
  if (manufacturerCode != EMBER_AF_NULL_MANUFACTURER_CODE) {
    frameControl |= ZCL_MANUFACTURER_SPECIFIC_MASK;
  }
  buffer[bytes++] = frameControl;
  if (manufacturerCode != EMBER_AF_NULL_MANUFACTURER_CODE) {
    buffer[bytes++] = LOW_BYTE(manufacturerCode);
    buffer[bytes++] = HIGH_BYTE(manufacturerCode);
  }
  buffer[bytes++] = emberAfNextSequence();
  buffer[bytes++] = commandId;

  // Each argument comes in as an integer value, a pointer to a buffer, or a
  // pointer to a buffer followed by an integer length.
  for (i = 0; format[i] != 0; i++) {
    char cmd = format[i];
    if (cmd <= 's') {
      //  0--9, A--G, L, S, b, l, and s all have a pointer to a buffer.  The
      // length of that buffer is implied by 0--9 and A--G (0 to 16 bytes).
      // For L, S, and b, a separate integer specifies the length.  That length
      // will precede the data in the destination buffer for L and S, which
      // turns them into regular ZigBee strings.  In this case, care must be
      // taken to account for invalid strings, which have length 0xFFFF or 0xFF
      // for L and S respectively.  In the case of invalid strings, the length
      // byte(s) are copied to the destination buffer but the string itself is
      // not.  Finally, l and s are just ZigBee strings and the length of the
      // string data is contained within the buffer itself and the entire
      // buffer is copied as is to the destination buffer.  Note that
      // emberAf(Long)StringLength handles invalid strings, but it does not
      // include the length byte(s) in the total length, so the overhead
      // must be maually accounted for when copying.
      data = (int8u *)va_arg(argPointer, int8u *);
      valueLen = 0;
      if (cmd == 'L' || cmd == 'S' || cmd == 'b') {
        dataLen = (int16u)va_arg(argPointer, int);
        if (cmd == 'L') {
          value = dataLen;
          valueLen = (value == 0xFFFF ? 0 : 2);
        } else if (cmd == 'S') {
          value = (int8u)dataLen;
          valueLen = (value == 0xFF ? 0 : 1);
        }
      } else if (cmd == 'l') {
        dataLen = emberAfLongStringLength(data) + 2;
      } else if (cmd == 's') {
        dataLen = emberAfStringLength(data) + 1;
      } else if ('0' <= cmd && cmd <= '9') {
        dataLen = cmd - '0';
      } else if ('A' <= cmd && cmd <= 'G') {
        dataLen = cmd - 'A' + 10;
      } else {
        emberAfDebugPrintln("ERR: Unknown format '%c'", cmd);
        return 0;
      }
    } else {
      // u, v, x, and w are one-, two-, three-, or four-byte integers.  u and v
      // must be extracted as an int while x and w come through as an int32u.
      // In all cases, the value is copied to the destination buffer in little-
      // endian format.
      dataLen = 0;
      if (cmd == 'u') {
        valueLen = 1;
      } else if (cmd == 'v') {
        valueLen = 2;
      } else if (cmd == 'x') {
        valueLen = 3;
      } else if (cmd == 'w') {
        valueLen = 4;
      } else {
        emberAfDebugPrintln("ERR: Unknown format '%c'", cmd);
        return 0;
      }
      value = (int32u)(valueLen <= 2
                       ? va_arg(argPointer, int)
                       : va_arg(argPointer, int32u));
    }

    // The destination buffer must be at least as large as the running total
    // plus the length of the integer value (if applicable) plus the length of
    // the data (if applicable).
    if (bufferLen < bytes + dataLen + valueLen) {
      emberAfDebugPrintln("ERR: Buffer too short for %d bytes for format '%c'",
                          dataLen + valueLen,
                          cmd);
      return 0;
    }

    // If there is an integer value, add it to destination buffer in little-
    // endian format.
    for (; 0 < valueLen; valueLen--) {
      buffer[bytes++] = LOW_BYTE(value);
      value = value >> 8;
    }

    // Finally, if there is data, add it to the destination buffer as is.
    MEMCOPY(buffer + bytes, data, dataLen);
    bytes += dataLen;
  }

  return bytes;
}

////////////////////// Public API ////////////////////////

void emberAfSetExternalBuffer(int8u *buffer,
                              int16u bufferLen,
                              int16u *lenPtr,
                              EmberApsFrame *apsFrame)
{
  emAfZclBuffer = buffer;
  emAfZclBufferLen = bufferLen;
  emAfResponseLengthPtr = lenPtr;
  emAfCommandApsFrame = apsFrame;
}

int16u emberAfFillExternalManufacturerSpecificBuffer(int8u frameControl,
                                                     EmberAfClusterId clusterId,
                                                     int16u manufacturerCode,
                                                     int8u commandId,
                                                     PGM_P format,
                                                     ...)
{
  int16u returnValue;
  va_list argPointer;

  va_start(argPointer, format);
  returnValue = vFillBuffer(emAfZclBuffer,
                            emAfZclBufferLen,
                            frameControl,
                            manufacturerCode,
                            commandId,
                            format,
                            argPointer);
  va_end(argPointer);
  *emAfResponseLengthPtr = returnValue;
  emAfCommandApsFrame->clusterId = clusterId;
  emAfCommandApsFrame->options = EMBER_AF_DEFAULT_APS_OPTIONS;
  return returnValue;
}

int16u emberAfFillExternalBuffer(int8u frameControl,
                                 EmberAfClusterId clusterId,
                                 int8u commandId,
                                 PGM_P format,
                                 ...)
{
  int16u returnValue;
  va_list argPointer;

  va_start(argPointer, format);
  returnValue = vFillBuffer(emAfZclBuffer,
                            emAfZclBufferLen,
                            frameControl,
                            EMBER_AF_NULL_MANUFACTURER_CODE,
                            commandId,
                            format,
                            argPointer);
  va_end(argPointer);
  *emAfResponseLengthPtr = returnValue;
  emAfCommandApsFrame->clusterId = clusterId;
  emAfCommandApsFrame->options = EMBER_AF_DEFAULT_APS_OPTIONS;
  return returnValue;
}

int16u emberAfFillBuffer(int8u *buffer,
                         int16u bufferLen,
                         int8u frameControl,
                         int8u commandId,
                         PGM_P format,
                         ...)
{
  int16u returnValue;
  va_list argPointer;

  va_start(argPointer, format);
  returnValue = vFillBuffer(buffer,
                            bufferLen,
                            frameControl,
                            EMBER_AF_NULL_MANUFACTURER_CODE,
                            commandId,
                            format,
                            argPointer);
  va_end(argPointer);
  return returnValue;
}

EmberStatus emberAfSendCommandUnicastToBindings(void)
{
  return emberAfSendUnicastToBindings(emAfCommandApsFrame,
                                      *emAfResponseLengthPtr,
                                      emAfZclBuffer);
}

EmberStatus emberAfSendCommandMulticast(EmberMulticastId multicastId)
{
  return emberAfSendMulticast(multicastId,
                              emAfCommandApsFrame,
                              *emAfResponseLengthPtr,
                              emAfZclBuffer);
}

EmberStatus emberAfSendCommandUnicast(EmberOutgoingMessageType type,
                                      int16u indexOrDestination)
{
  return emberAfSendUnicast(type,
                            indexOrDestination,
                            emAfCommandApsFrame,
                            *emAfResponseLengthPtr,
                            emAfZclBuffer);
}

EmberStatus emberAfSendCommandBroadcast(EmberNodeId destination)
{
  return emberAfSendBroadcast(destination,
                              emAfCommandApsFrame,
                              *emAfResponseLengthPtr,
                              emAfZclBuffer);
}

EmberStatus emberAfSendCommandInterPan(EmberPanId panId,
                                       const EmberEUI64 destinationLongId,
                                       EmberNodeId destinationShortId,
                                       EmberMulticastId multicastId,
                                       EmberAfProfileId profileId)
{
  return emberAfSendInterPan(panId,
                             destinationLongId,
                             destinationShortId,
                             multicastId,
                             emAfCommandApsFrame->clusterId,
                             profileId,
                             *emAfResponseLengthPtr,
                             emAfZclBuffer);
}

EmberApsFrame *emberAfGetCommandApsFrame()
{
  return emAfCommandApsFrame;
}

void emberAfSetCommandEndpoints(int8u sourceEndpoint, int8u destinationEndpoint)
{
  emAfCommandApsFrame->sourceEndpoint = sourceEndpoint;
  emAfCommandApsFrame->destinationEndpoint = destinationEndpoint;
}
