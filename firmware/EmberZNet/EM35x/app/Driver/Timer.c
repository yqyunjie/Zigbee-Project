// *******************************************************************
//  Timer.c
//
//  Timer Driver.c
//
//  Copyright by kaiser. All rights reserved.
// *******************************************************************

#include <stdio.h>
//#include "app/sensor/Language.h"
#ifdef  PHY_BRIDGE
 #ifdef  CORTEXM3
  #include "hal/micro/cortexm3/diagnostic.h"
 #endif
 #include "phy/bridge/zigbee-bridge.h"
 #ifdef  BRIDGE_TRACE
  #include "phy/bridge/zigbee-bridge-internal.h"
 #endif//BRIDGE_TRACE
#endif//PHY_BRIDGE

/*----------------------------------------------------------------------------
 *        Global Variable
 *----------------------------------------------------------------------------*/


//eof
