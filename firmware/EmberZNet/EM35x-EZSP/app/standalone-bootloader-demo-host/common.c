// *******************************************************************
//  common.c
//
//  Contains functions used by standalone-bootloader-demo-host/demo.c
//  file.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include PLATFORM_HEADER //compiler/micro specifics, types
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "app/util/serial/serial.h"
#include "stack/include/error.h"

#include "hal/micro/bootloader-interface-standalone.h"
#include "app/util/ezsp/ezsp-protocol.h"
#include "app/util/ezsp/ezsp.h"
#include "app/util/ezsp/serial-interface.h"
#include "app/util/bootload/bootload-ezsp-utils.h"
#include "app/standalone-bootloader-demo-host/xmodem-spi.h"

#include "common.h"

//local varibles
int32u last260lbt;

// Start StandaloneBootloader without reporting an error.
EzspStatus hostLaunchStandaloneBootloader(int8u mode)
{
  EzspStatus ezspStat;
  int32u startTime;
#ifndef GATEWAY_APP
  int8u len;
  int8u * queryResp;
  int16u j;
  int8u str[20];
#endif

#ifndef GATEWAY_APP
  queryResp = XModemQuery(&len, FALSE);
  if (queryResp) // If bootloader
    return EZSP_SUCCESS;
#endif

  startTime = halCommonGetInt32uMillisecondTick();

  bootloadEzspLastError = EZSP_SUCCESS;
  ezspStat = ezspLaunchStandaloneBootloader(mode);
  if(ezspStat == EZSP_SUCCESS) {
#ifndef GATEWAY_APP
    ezspStat = waitFor260boot(&j);
    last260lbt = halCommonGetInt32uMillisecondTick() - startTime;
    formatFixed(str, (int32s)last260lbt, 6, 3, FALSE);
    emberSerialGuaranteedPrintf(APP_SERIAL,"\r\nWe looped %u for %s secs es=0x%x\r\n",
                                j, str, ezspStat);
    if ((ezspStat == EZSP_SPI_ERR_NCP_RESET) ||
        (ezspStat == EZSP_SUCCESS)) {
      queryResp = XModemQuery(&len, FALSE);
      if (queryResp) // If bootloader
        ezspStat = EZSP_SUCCESS;
      else
        ezspStat = EZSP_SPI_ERR_FATAL;
    }
#else
    ezspStat = EZSP_SUCCESS;
#endif
  }

  return ezspStat;
}

// Do a formatted conversion of a signed fixed point 32 bit number
// to decimal.  Make lots of assumtions.  Like minDig > dot;
// minDig < 20; etc.

void formatFixed(int8u* charBuffer, int32s value, int8u minDig, int8u dot, boolean showPlus)
{
  int8u numBuff[20];
  int8u i = 0;
  int32u calcValue = (int32u)value;

  if (value < 0)
  {
    calcValue = (int32u)-value;
    *charBuffer++ = '-';
  }
  else if (showPlus)
    *charBuffer++ = '+';

  while((i < minDig) || calcValue) {
    numBuff[i++] = (int8u)(calcValue % 10);
    calcValue /= 10;
  }
  while(i--) {
    *charBuffer++ = (numBuff[i] + '0');
    if (i == dot)
      *charBuffer++ = '.';
  }
  *charBuffer = 0;
}

// decode the EmberNodeType into text and display it.
void decodeNodeType(EmberNodeType nodeType)
{

  switch(nodeType) {
  default:
  case EMBER_UNKNOWN_DEVICE:
    emberSerialPrintf(APP_SERIAL, "unknown %d", nodeType);
    break;
  case EMBER_COORDINATOR:
    emberSerialPrintf(APP_SERIAL, "coordinator");
    break;
  case EMBER_ROUTER:
    emberSerialPrintf(APP_SERIAL, "router");
    break;
  case EMBER_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "end device");
    break;
  case EMBER_SLEEPY_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "sleepy end device");
    break;
  case EMBER_MOBILE_END_DEVICE:
    emberSerialPrintf(APP_SERIAL, "mobile end device");
    break;
  }
}
