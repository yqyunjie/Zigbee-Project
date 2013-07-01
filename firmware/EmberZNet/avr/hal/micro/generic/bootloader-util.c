/*
 * File: bootload-util.c
 * Description: Convenience functions and wrappers
 *              for use with the application bootloader.
 *
 * Culprit(s): Cliff Bowman, cliff@ember.com
 *
 * Copyright 2005 by Ember Corporation. All rights reserved.               *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "stack/include/error.h"
#include "bootloader-util.h"

int32u calculateCrc32(int32u previous,
                      int8u *data,
                      int16u length,
                      boolean reset);

//--------------------------------------------------------------

boolean  emberGetImageData(int16u startSegment, 
                           int16u *length,
                           int32u *timestamp,
                           int8u  *userData,
                           int8u  userDataLength)
{
  int8u buffer[BOOTLOADER_SEGMENT_SIZE];

  if (halBootloaderReadDownloadSpace(startSegment, buffer) == EMBER_SUCCESS
      && imageSignatureValid(buffer + IMAGE_HEADER_OFFSET_SIGNATURE)) {

    if (length != NULL)
      *length = emberFetchLowHighInt16u(buffer + IMAGE_HEADER_OFFSET_LENGTH);

    if (timestamp != NULL)
      *timestamp = emberFetchLowHighInt32u(buffer + IMAGE_HEADER_OFFSET_TIMESTAMP);

    if (userData != NULL)
      MEMCOPY(userData, buffer + IMAGE_HEADER_OFFSET_USER, userDataLength);

    return TRUE;
  } else
    return FALSE;
}

boolean emberImageIsValidSpecifyingSpace(int16u startSegment,
                                         int8u memorySpace)
{
  int8u buffer[BOOTLOADER_SEGMENT_SIZE];
  int16u length, ii;
  int32u calculatedTag = 0;
  int32u embeddedTag = 0;


  // Get the image length and the tag stored with the image.
  if (halInternalReadNvmSegment(startSegment, buffer, memorySpace) != EMBER_SUCCESS
      || ! imageSignatureValid(buffer + IMAGE_HEADER_OFFSET_SIGNATURE))
    return FALSE;

  length = emberFetchLowHighInt16u(buffer + IMAGE_HEADER_OFFSET_LENGTH);
  embeddedTag = emberFetchLowHighInt32u(buffer + IMAGE_HEADER_OFFSET_VALIDITY);

  // Calculate a tag and compare.
  // Note: Validity check tag does not include header page.
  for (ii = 1; ii < length; ii++) {
    if (halInternalReadNvmSegment(startSegment + ii, buffer, memorySpace) 
        != EMBER_SUCCESS) {
      return FALSE; }

    halResetWatchdog();
    calculatedTag = calculateCrc32(calculatedTag,
                                   buffer,
                                   BOOTLOADER_SEGMENT_SIZE,
                                   (ii == 1));
  }

  return (calculatedTag == embeddedTag);
}

// Local utility functions.

//--------------------------------------------------------------
// CRC-32 for image validation.
#define CRC32_START 0xFFFFFFFF
#define POLYNOMIAL  0xEDB88320L

static int32u updateCrcNoTable(int32u accumulator,
                               int8u data)

{
  int8u jj;
  int32u previous;
  int32u operator;

  previous = (accumulator >> 8) & 0x00FFFFFF;
  operator = (accumulator ^ data) & 0xFF;
  for (jj = 0; jj < 8; jj++) {
    operator = ((operator & 0x01)
                ? ((operator >> 1) ^ POLYNOMIAL)
                : (operator >> 1));
  }

  return (previous ^ operator);
}

// Deliberately not static to support testing.
int32u calculateCrc32(int32u previous,
                        int8u *data,
                        int16u length,
                        boolean reset)
{
  int16u ii;
  int32u result;

  result = ((reset) ? CRC32_START : ~previous);

  for (ii = 0; ii < length; ii++) {
    result = updateCrcNoTable(result, data[ii]);
  }

  return ~result;
}

