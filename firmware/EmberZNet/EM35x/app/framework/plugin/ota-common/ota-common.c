// *******************************************************************
// * ota-comman.c
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading specific file.  These routines are command to the client
// * and server.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/util/common.h"
#include "app/framework/util/attribute-storage.h"
#include "enums.h"

// clusters specific header
#include "ota.h"

#include "app/framework/plugin/ota-storage-common/ota-storage.h"

// This relates all OTA command IDs in app/framework/gen/command-id.h
// to minimum message lengths (does not include EMBER_AF_ZCL_OVERHEAD)
PGM int8u emAfOtaMinMessageLengths[] = {
  2,      // Image Notify
  8,      // Query Next Image Request
  1,      // Query Next Image Response
  13,     // Image Block Request
  13,     // Image Page Request
  1,      // Image Block Response (abort is shortest)
  1,      // Upgrade End Request
  1,      // Upgrade End Response
  5,      // Run Upgrade Request
  18,     // Query Specific File Request
  11,     // Query Specific File Response
};

EmberAfOtaImageId emAfOtaCreateEmberAfOtaImageIdStruct(int16u manufacturerId,
                                         int16u imageType,
                                         int32u fileVersion)
{
  EmberAfOtaImageId id;
  id.manufacturerId = manufacturerId;
  id.imageTypeId = imageType;
  id.firmwareVersion = fileVersion;
  MEMSET(id.deviceSpecificFileEui64, 0, EUI64_SIZE);
  return id;
}

// This assumes the message has already been validated for its length
int8u emAfOtaParseImageIdFromMessage(EmberAfOtaImageId* returnId, 
                                     const int8u* buffer, 
                                     int8u length)
{
  returnId->manufacturerId  = emberAfGetInt16u(buffer, 0, length);
  returnId->imageTypeId     = emberAfGetInt16u(buffer, 2, length);
  returnId->firmwareVersion = emberAfGetInt32u(buffer, 4, length);
  MEMSET(returnId->deviceSpecificFileEui64, 0, EUI64_SIZE);
  return 8;
}


#if defined(EMBER_AF_PRINT_CORE)

// 32-bit math may be expensive and is only needed for printing
// purposes.  So this functionality should be conditionally compiled in.

// The 'currentOffset' and 'endOffset' parameters are relative to the static 
// variable 'startingOffset'.  It is expected the 'startingOffset' is set by a 
// previous call that did nothing but pass in the 'offsetStart' value
// (other parameters are ignored in that case).
static int8u printPercentage(PGM_P prefixString,
                            int8u updateFrequency,
                            int32u offsetStart,
                            int32u currentOffset,
                            int32u endOffset)
{
  static int8u percentageComplete = 0;
  static int8u oldPercentageComplete = 255; // invalid value
  static int32u startingOffset = 0;
  static int32u imageSize = 0;

  if (offsetStart > 0 || endOffset > 0) {
    oldPercentageComplete = 255;
    startingOffset = offsetStart;
    imageSize = endOffset - startingOffset;
    return 0;
  }
  // The rest of this code should NOT use 'offsetStart', but 'startingOffset'.

  currentOffset -= startingOffset;

  percentageComplete = emAfCalculatePercentage(currentOffset, imageSize);

  if (oldPercentageComplete == 255
      || (percentageComplete >= updateFrequency
          && (percentageComplete >= 
              (oldPercentageComplete + updateFrequency)))) {
    emberAfCorePrintln("%p: %d%% complete", prefixString, percentageComplete);
    oldPercentageComplete = percentageComplete;
  }
  return percentageComplete;
}

void emAfPrintPercentageSetStartAndEnd(int32u startingOffset, int32u endOffset)
{
  printPercentage(NULL,           // prefix string (ignored)
                  0,              // update frequency (ignored)
                  startingOffset,
                  0,              // current offset (ignored)
                  endOffset);     
}

int8u emAfPrintPercentageUpdate(PGM_P prefixString, 
                                int8u updateFrequency, 
                                int32u currentOffset)
{
  return printPercentage(prefixString,
                         updateFrequency,
                         0,               // offset start (ignored)
                         currentOffset,
                         0);              // offset end (ignored)
}

int8u emAfCalculatePercentage(int32u currentOffset, int32u imageSize)
{
  int8u percentage = 0;
  if (imageSize == 0) {
    return 0;
  }

  if (currentOffset >= imageSize) {
    percentage = 100;
  } else {
    percentage = (currentOffset * 100) / imageSize;
  }
  return percentage;
}

#endif // defined EMBER_AF_CORE_PRINT_ENABLED


