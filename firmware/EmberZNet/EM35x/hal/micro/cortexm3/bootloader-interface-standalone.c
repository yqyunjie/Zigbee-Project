/*
 * File: bootloader-interface-standalone.c
 * Description: EM3XX-specific standalone bootloader HAL functions
 *
 * Copyright 2008 by Ember Corporation. All rights reserved.                *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "stack/include/error.h"
#include "stack/include/stack-info.h"
#include "hal/hal.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/diagnostic.h"

extern int8u emGetPhyRadioChannel(void);
extern int8s emGetPhyRadioPower(void);

EmberStatus halLaunchStandaloneBootloader(int8u mode)
{
  if(BOOTLOADER_BASE_TYPE(halBootloaderAddressTable.bootloaderType) 
     == BL_TYPE_STANDALONE) {
    // should never return
    if(mode == STANDALONE_BOOTLOADER_NORMAL_MODE) {
      // Version 0x0106 is where OTA bootloader support was added and the 
      //  RESET_BOOTLOADER_OTAVALID reset type was introduced.
      if(halBootloaderAddressTable.baseTable.version < 0x0106) {
        halInternalSysReset(RESET_BOOTLOADER_BOOTLOAD);
      } else {
        tokTypeStackCalData calData;
        // Convert channel number to index (bootloader only uses index).
        int8u channelIndex =  emGetPhyRadioChannel() - 11;

        halCommonGetIndexedToken(&calData, TOKEN_STACK_CAL_DATA, channelIndex);
        halResetInfo.boot.panId        = PAN_ID_REG;
        halResetInfo.boot.radioChannel = channelIndex;
        halResetInfo.boot.radioPower   = emGetPhyRadioPower();
        halResetInfo.boot.radioLnaCal  = calData.lna;
        // ota parameters valid bootloader reset
        halInternalSysReset(RESET_BOOTLOADER_OTAVALID);
      }
    } else if(mode == STANDALONE_BOOTLOADER_RECOVERY_MODE) {
      // standard bootloader reset
      halInternalSysReset(RESET_BOOTLOADER_BOOTLOAD);
    }
  }
  return EMBER_ERR_FATAL;
}

int16u halGetStandaloneBootloaderVersion(void)
{
  if(BOOTLOADER_BASE_TYPE(halBootloaderAddressTable.bootloaderType) 
     == BL_TYPE_STANDALONE) {
    return halGetBootloaderVersion();
  } else {
    return BOOTLOADER_INVALID_VERSION;
  }
}
