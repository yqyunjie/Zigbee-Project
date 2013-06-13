// *****************************************************************************
// * standalone-bootloader-client-ncp.c
// *
// * This file defines the NCP specific client behavior for the Ember
// * proprietary bootloader protocol.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "hal/micro/bootloader-interface-standalone.h"

void emAfStandaloneBootloaderClientGetInfo(int16u* bootloaderVersion,
                                           int8u* platformId,
                                           int8u* microId,
                                           int8u* phyId)
{
  *bootloaderVersion = ezspGetStandaloneBootloaderVersionPlatMicroPhy(platformId,
                                                                      microId,
                                                                      phyId);
}

EmberStatus emAfStandaloneBootloaderClientLaunch(void)
{
  EmberStatus status = ezspLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
  if (status != EMBER_SUCCESS) {
    return status;
  }

  // Need to wait here until the NCP resets us.
  while (1) {};
}


void emAfStandaloneBootloaderClientGetMfgInfo(int16u* mfgIdReturnValue,
                                              int8u* boardNameReturnValue)
{
  int8u tokenData[2]; // MFG ID is 2-bytes

  ezspGetMfgToken(EZSP_MFG_MANUF_ID, tokenData);
  *mfgIdReturnValue = HIGH_LOW_TO_INT(tokenData[1], tokenData[0]);

  ezspGetMfgToken(EZSP_MFG_BOARD_NAME, boardNameReturnValue);
}

int32u emAfStandaloneBootloaderClientGetRandomNumber(void)
{
  // Although we are supposed to return 32-bits, we only return 16.
  // This is because the client only uses 16-bits of the actual data.
  int16u value;
  ezspGetRandomNumber(&value);
  return value;
}

#if !defined(EMBER_TEST)

void emAfStandaloneBootloaderClientGetKey(int8u* returnData)
{
  ezspGetMfgToken(EZSP_MFG_BOOTLOAD_AES_KEY, returnData);
}

#endif
