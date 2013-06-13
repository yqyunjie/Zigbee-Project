// *****************************************************************************
// * standalone-bootloader-client-soc.c
// *
// * This file defines the SOC specific client behavior for the Ember
// * proprietary bootloader protocol.
// * 
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"


void emAfStandaloneBootloaderClientGetInfo(int16u* bootloaderVersion,
                                           int8u* platformId,
                                           int8u* microId,
                                           int8u* phyId)
{
  *bootloaderVersion = halGetStandaloneBootloaderVersion();

  *platformId = PLAT;
  *microId = MICRO;
  *phyId = PHY;
}

EmberStatus emAfStandaloneBootloaderClientLaunch(void)
{
  return halLaunchStandaloneBootloader(STANDALONE_BOOTLOADER_NORMAL_MODE);
}

void emAfStandaloneBootloaderClientGetMfgInfo(int16u* mfgIdReturnValue,
                                              int8u* boardNameReturnValue)
{
  halCommonGetToken(mfgIdReturnValue, TOKEN_MFG_MANUF_ID);
  halCommonGetToken(boardNameReturnValue, TOKEN_MFG_BOARD_NAME);
}

int32u emAfStandaloneBootloaderClientGetRandomNumber(void)
{
  return halStackGetInt32uSymbolTick();
}

#if !defined(EMBER_TEST)

void emAfStandaloneBootloaderClientGetKey(int8u* returnData)
{
  halCommonGetToken(returnData, TOKEN_MFG_BOOTLOAD_AES_KEY);
}

#endif
