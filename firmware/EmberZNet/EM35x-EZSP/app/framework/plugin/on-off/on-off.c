// *******************************************************************
// * on-off.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "on-off.h"

#ifdef EMBER_AF_PLUGIN_LEVEL_CONTROL
  #include "../level-control/level-control.h"
#endif //EMBER_AF_PLUGIN_LEVEL_CONTROL

#ifdef EMBER_AF_PLUGIN_SCENES
  #include "../scenes/scenes.h"
#endif //EMBER_AF_PLUGIN_SCENES

#ifdef EMBER_AF_PLUGIN_ZLL_ON_OFF_SERVER
  #include "../zll-on-off-server/zll-on-off-server.h"
#endif

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  #include "../zll-level-control-server/zll-level-control-server.h"
#endif

EmberAfStatus emberAfOnOffClusterSetValueCallback(int8u endpoint,
                                                  int8u command,
                                                  boolean initiatedByLevelChange)
{
  EmberAfStatus status;
  boolean currentValue, newValue;

  emberAfOnOffClusterPrintln("On/Off set value: %x %x", endpoint, command);

  // read current on/off value
  status = emberAfReadAttribute(endpoint,
                                ZCL_ON_OFF_CLUSTER_ID,
                                ZCL_ON_OFF_ATTRIBUTE_ID,
                                CLUSTER_MASK_SERVER,
                                (int8u *)&currentValue,
                                sizeof(currentValue),
                                NULL); // data type
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: reading on/off %x", status);
    return status;
  }

  // if the value is already what we want to set it to then do nothing
  if ((!currentValue && command == ZCL_OFF_COMMAND_ID) ||
      (currentValue && command == ZCL_ON_COMMAND_ID)) {
    emberAfOnOffClusterPrintln("On/off already set to new value");
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  // we either got a toggle, or an on when off, or an off when on,
  // so we need to swap the value
  newValue = !currentValue;
  emberAfOnOffClusterPrintln("Toggle on/off from %x to %x", currentValue, newValue);

  // write tge new on/off value
  status = emberAfWriteAttribute(endpoint,
                                 ZCL_ON_OFF_CLUSTER_ID,
                                 ZCL_ON_OFF_ATTRIBUTE_ID,
                                 CLUSTER_MASK_SERVER,
                                 (int8u *)&newValue,
                                 ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: writing on/off %x", status);
    return status;
  }

#ifdef EMBER_AF_PLUGIN_ZLL_ON_OFF_SERVER
  if (initiatedByLevelChange) {
    emberAfPluginZllOnOffServerLevelControlZllExtensions(endpoint);
  }
#endif

  if (!initiatedByLevelChange
      && emberAfContainsServer(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID)) {
    emberAfOnOffClusterLevelControlEffectCallback(endpoint, 
                                                  newValue);
  }

  // the scene has been changed (the value of on/off has changed) so
  // the current scene as descibed in the attribute table is invalid,
  // so mark it as invalid (just writes the valid/invalid attribute)
  if (emberAfContainsServer(endpoint, ZCL_SCENES_CLUSTER_ID)) {
    emberAfScenesClusterMakeInvalidCallback(endpoint);
  }

  // The returned status is based solely on the On/Off cluster.  Errors in the
  // Level Control and/or Scenes cluster are ignored.
  return EMBER_ZCL_STATUS_SUCCESS;
}

boolean emberAfOnOffClusterOffCallback(void)
{
  EmberAfStatus status = emberAfOnOffClusterSetValueCallback(emberAfCurrentEndpoint(),
      ZCL_OFF_COMMAND_ID,
      FALSE);
#ifdef EMBER_AF_PLUGIN_ZLL_ON_OFF_SERVER
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPluginZllOnOffServerOffZllExtensions(emberAfCurrentCommand());
  }
#endif
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfOnOffClusterOnCallback(void)
{
  EmberAfStatus status = emberAfOnOffClusterSetValueCallback(emberAfCurrentEndpoint(),
      ZCL_ON_COMMAND_ID,
      FALSE);
#ifdef EMBER_AF_PLUGIN_ZLL_ON_OFF_SERVER
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPluginZllOnOffServerOnZllExtensions(emberAfCurrentCommand());
  }
#endif
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfOnOffClusterToggleCallback(void)
{
  EmberAfStatus status = emberAfOnOffClusterSetValueCallback(emberAfCurrentEndpoint(),
      ZCL_TOGGLE_COMMAND_ID,
      FALSE);
#ifdef EMBER_AF_PLUGIN_ZLL_ON_OFF_SERVER
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emberAfPluginZllOnOffServerToggleZllExtensions(emberAfCurrentCommand());
  }
#endif
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

void emberAfOnOffClusterLevelControlEffectCallback(int8u endpoint,
                                                   boolean newValue)
{
  int8u currentLevel, newLevel = 0xFF;
  int16u transitionTime = 0xFFFF;
  boolean onLevel = FALSE;
  int8u minLevel=0x00;
  EmberAfStatus status;

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  minLevel = EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER_MINIMUM_LEVEL;
#elif defined(EMBER_AF_PLUGIN_LEVEL_CONTROL)
  minLevel = EMBER_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL;
#endif

#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
  status = emberAfReadServerAttribute(emberAfCurrentEndpoint(),
                                      ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                      ZCL_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                      (int8u *)&transitionTime,
                                      sizeof(transitionTime));
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfLevelControlClusterPrintln("ERR: reading on/off transition time %x",
                                      status);
  }
#endif

#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_LEVEL_ATTRIBUTE
  onLevel = TRUE;
#endif

  if (onLevel) {
    status = emberAfReadAttribute(endpoint,
                                  ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                  ZCL_ON_LEVEL_ATTRIBUTE_ID,
                                  CLUSTER_MASK_SERVER,
                                  (int8u *)&newLevel,
                                  sizeof(newLevel),
                                  NULL); // data type
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfOnOffClusterPrintln("ERR: reading current level %x", status);
    } else {
      if (newLevel == 0xFF) {
        onLevel = FALSE;
      }
    }
  }

  status = emberAfReadAttribute(endpoint,
                                ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                CLUSTER_MASK_SERVER,
                                (int8u *)&currentLevel,
                                sizeof(currentLevel),
                                NULL); // data type
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: reading current level %x", status);
  } else {
    // Off
    if (!newValue) {
      if (onLevel) {
#ifdef EMBER_AF_PLUGIN_LEVEL_CONTROL
        emAfPluginLevelControlClusterOnOffEffectHandler(ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                                                        newLevel, 
                                                        TRUE,
                                                        transitionTime);
#endif
      }
      else {
#ifdef EMBER_AF_PLUGIN_LEVEL_CONTROL
        emAfPluginLevelControlClusterOnOffEffectHandler(ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                                                        currentLevel,
                                                        TRUE,
                                                        transitionTime);
#endif
      }
    }
    // On 
    else {
      newLevel = minLevel;
      status = emberAfWriteAttribute(endpoint,
                                     ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                     ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                     CLUSTER_MASK_SERVER,
                                     (int8u *)&newLevel,
                                     ZCL_INT8U_ATTRIBUTE_TYPE);
      if (onLevel) {
        status = emberAfReadAttribute(endpoint,
                                      ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                      ZCL_ON_LEVEL_ATTRIBUTE_ID,
                                      CLUSTER_MASK_SERVER,
                                      (int8u *)&newLevel,
                                      sizeof(newLevel),
                                      NULL); // data type
        if (status != EMBER_ZCL_STATUS_SUCCESS) {
          emberAfOnOffClusterPrintln("ERR: reading on level %x", status);
        }
        else {
#ifdef EMBER_AF_PLUGIN_LEVEL_CONTROL
          emAfPluginLevelControlClusterOnOffEffectHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID, 
                                                          newLevel, 
                                                          FALSE,
                                                          transitionTime);
#endif
        }
      } else {
#ifdef EMBER_AF_PLUGIN_LEVEL_CONTROL
        emAfPluginLevelControlClusterOnOffEffectHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
                                                        currentLevel,
                                                        FALSE,
                                                        transitionTime);
#endif
      }
    }
  }
}
