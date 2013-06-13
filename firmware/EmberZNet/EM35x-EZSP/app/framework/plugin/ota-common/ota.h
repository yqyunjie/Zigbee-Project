// *******************************************************************
// * ota.h
// *
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************
enum {
  OTA_USE_OFFSET_TIME,
  OTA_USE_UTC_TIME
};
#define OTA_UPGRADE_END_RESPONSE_RUN_NOW  0x00

#define MFG_ID_WILD_CARD        0xFFFF 
#define IMAGE_TYPE_WILD_CARD    0xFFFF
#define IMAGE_TYPE_SECURITY     0xFFC0
#define IMAGE_TYPE_CONFIG       0xFFC1
#define IMAGE_TYPE_LOG          0xFFC2
#define FILE_VERSION_WILD_CARD  0xFFFFFFFF

#define ZIGBEE_2006_STACK_VERSION 0x0000
#define ZIGBEE_2007_STACK_VERSION 0x0001
#define ZIGBEE_PRO_STACK_VERSION  0x0002

// These apply to the field control of the Over-the-air messages
#define OTA_HW_VERSION_BIT_MASK   0x01
#define OTA_NODE_EUI64_BIT_MASK   0x01

// OTA Upgrade status as defined by OTA cluster spec.
enum {
  OTA_UPGRADE_STATUS_NORMAL,
  OTA_UPGRADE_STATUS_DOWNLOAD_IN_PROGRESS,
  OTA_UPGRADE_STATUS_DOWNLOAD_COMPLETE,
  OTA_UPGRADE_STATUS_WAIT,
  OTA_UPGRADE_STATUS_COUNTDOWN
};

#define OTA_TAG_UPGRADE_IMAGE               0x0000
#define OTA_TAG_ECDSA_SIGNATURE             0x0001
#define OTA_TAG_ECDSA_SIGNING_CERTIFICATE   0x0002
#define OTA_TAG_RESERVED_START              0x0003
#define OTA_TAG_RESERVED_END                0xEFFF
#define OTA_TAG_MANUFACTURER_SPECIFIC_START 0xFFFF

extern EmberNodeId upgradeServerNodeId;
extern PGM int8u emAfOtaMinMessageLengths[];

#define EM_AF_OTA_MAX_COMMAND_ID ZCL_QUERY_SPECIFIC_FILE_RESPONSE_COMMAND_ID

// Create a shorter name for printing to make the code more readable.
#define otaPrintln(...) emberAfOtaBootloadClusterPrintln(__VA_ARGS__)
#define otaPrint(...)   emberAfOtaBootloadClusterPrint(__VA_ARGS__)
#define otaPrintFlush()      emberAfOtaBootloadClusterFlush()

// -------------------------------------------------------
// Common to both client and server
// -------------------------------------------------------
EmberAfOtaImageId emAfOtaCreateEmberAfOtaImageIdStruct(int16u manufacturerId,
                                         int16u imageType,
                                         int32u fileVersion);

int8u emAfOtaParseImageIdFromMessage(EmberAfOtaImageId* returnId, 
                                     const int8u* buffer, 
                                     int8u length);

#if defined(EMBER_AF_PRINT_CORE)
void emAfPrintPercentageSetStartAndEnd(int32u startingOffset, int32u endOffset);
int8u emAfPrintPercentageUpdate(PGM_P prefixString, 
                               int8u updateFrequency, 
                               int32u currentOffset);
int8u emAfCalculatePercentage(int32u currentOffset, int32u imageSize);
#else
  #define emAfPrintPercentageSetStartAndEnd(x,y)
  #define emAfPrintPercentageUpdate(x,y,z)
  #define emAfCalculatePercentage(x,y) (0)
#endif
