// *******************************************************************
// * zll-on-off-server.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../on-off/on-off.h"
#include "zll-on-off-server.h"
#include "zll-on-off-server-callback.h"

#define ZLL_ON_OFF_CLUSTER_ON_OFF_CONTROL_ACCEPT_ONLY_WHEN_ON_MASK BIT(0)

#define  readOnOff(endpoint, onOff)  readBoolean((endpoint), ZCL_ON_OFF_ATTRIBUTE_ID, "on/off", (onOff))
#define writeOnOff(endpoint, onOff) writeBoolean((endpoint), ZCL_ON_OFF_ATTRIBUTE_ID, "on/off", (onOff))
#define  readGlobalSceneControl(endpoint, globalSceneControl)  readBoolean((endpoint), ZCL_GLOBAL_SCENE_CONTROL_ATTRIBUTE_ID, "global scene control", (globalSceneControl))
#define writeGlobalSceneControl(endpoint, globalSceneControl) writeBoolean((endpoint), ZCL_GLOBAL_SCENE_CONTROL_ATTRIBUTE_ID, "global scene control", (globalSceneControl))
#define  readOnTime(endpoint, onTime)  readInt16u((endpoint), ZCL_ON_TIME_ATTRIBUTE_ID, "on time", (onTime))
#define writeOnTime(endpoint, onTime) writeInt16u((endpoint), ZCL_ON_TIME_ATTRIBUTE_ID, "on time", (onTime))
#define  readOffWaitTime(endpoint, offWaitTime)  readInt16u((endpoint), ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID, "off wait time", (offWaitTime))
#define writeOffWaitTime(endpoint, offWaitTime) writeInt16u((endpoint), ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID, "off wait time", (offWaitTime))

static EmberAfStatus readBoolean(int8u endpoint, EmberAfAttributeId attribute, PGM_P name, boolean *value)
{
  EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                                    ZCL_ON_OFF_CLUSTER_ID,
                                                    attribute,
                                                    (int8u *)value,
                                                    sizeof(boolean));
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: %ping %p %x", "read", name, status);
  }
  return status;
}

static EmberAfStatus writeBoolean(int8u endpoint, EmberAfAttributeId attribute, PGM_P name, boolean value)
{
  EmberAfStatus status = emberAfWriteServerAttribute(endpoint,
                                                     ZCL_ON_OFF_CLUSTER_ID,
                                                     attribute,
                                                     (int8u *)&value,
                                                     ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: %ping %p %x", "writ", name, status);
  }
  return status;
}

static EmberAfStatus readInt16u(int8u endpoint, EmberAfAttributeId attribute, PGM_P name, int16u *value)
{
  EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                                    ZCL_ON_OFF_CLUSTER_ID,
                                                    attribute,
                                                    (int8u *)value,
                                                    sizeof(int16u));
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: %ping %p %x", "read", name, status);
  }
  return status;
}

static EmberAfStatus writeInt16u(int8u endpoint, EmberAfAttributeId attribute, PGM_P name, int16u value)
{
  EmberAfStatus status = emberAfWriteServerAttribute(endpoint,
                                                     ZCL_ON_OFF_CLUSTER_ID,
                                                     attribute,
                                                     (int8u *)&value,
                                                     ZCL_INT16U_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfOnOffClusterPrintln("ERR: %ping %p %x", "writ", name, status);
  }
  return status;
}

void emberAfOnOffClusterServerTickCallback(int8u endpoint)
{
  int16u onTime, offWaitTime;
  boolean onOff;

  if (readOnOff(endpoint, &onOff) != EMBER_ZCL_STATUS_SUCCESS
      || readOnTime(endpoint, &onTime) != EMBER_ZCL_STATUS_SUCCESS
      || readOffWaitTime(endpoint, &offWaitTime) != EMBER_ZCL_STATUS_SUCCESS) {
    return;
  }

  // If the values of the OnTime and OffWaitTime attributes are both less than
  // 0xFFFF, the device shall then update the device every 1/10th second until
  // both the OnTime and OffWaitTime attributes are equal to 0x0000, as
  // follows:
  //
  //   If the value of the OnOff attribute is equal to 0x01 (on) and the value
  //   of the OnTime attribute is greater than zero, the device shall decrement
  //   the value of the OnTime attribute. If the value of the OnTime attribute
  //   reaches 0x0000, the device shall set the OffWaitTime and OnOff
  //   attributes to 0x0000 and 0x00, respectively.
  //
  //   If the value of the OnOff attribute is equal to 0x00 (off) and the value
  //   of the OffWaitTime attribute is greater than zero, the device shall
  //   decrement the value of the OffWaitTime attribute. If the value of the
  //   OffWaitTime attribute reaches 0x0000, the device shall terminate the
  //   update.
  if (onOff && 0x0000 < onTime) {
    onTime--;
    writeOnTime(endpoint, onTime);
    if (onTime == 0x0000) {
      offWaitTime = 0x0000;
      writeOffWaitTime(endpoint, offWaitTime);
      onOff = FALSE;
      writeOnOff(endpoint, onOff);
      return;
    }
  } else if (!onOff && 0x0000 < offWaitTime) {
    offWaitTime--;
    writeOffWaitTime(endpoint, offWaitTime);
    if (offWaitTime == 0x0000) {
      return;
    }
  }

  emberAfScheduleClusterTick(endpoint,
                             ZCL_ON_OFF_CLUSTER_ID,
                             EMBER_AF_SERVER_CLUSTER_TICK,
                             MILLISECOND_TICKS_PER_SECOND / 10,
                             EMBER_AF_OK_TO_HIBERNATE);
}

boolean emberAfOnOffClusterOffWithEffectCallback(int8u effectId,
                                                 int8u effectVariant)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_INVALID_VALUE;
  boolean globalSceneControl;
  int8u endpoint = emberAfCurrentEndpoint();

  // Ensure parameters have values withing proper range.
  if (effectId > EMBER_ZCL_ON_OFF_EFFECT_IDENTIFIER_DYING_LIGHT
      || effectVariant > EMBER_ZCL_ON_OFF_DELAYED_ALL_OFF_EFFECT_VARIANT_50_PERCENT_DIM_DOWN_IN_0P8_SECONDS_THEN_FADE_TO_OFF_IN_12_SECONDS
      || (effectId == EMBER_ZCL_ON_OFF_EFFECT_IDENTIFIER_DYING_LIGHT
          && effectVariant > EMBER_ZCL_ON_OFF_DYING_LIGHT_EFFECT_VARIANT_20_PERCENTER_DIM_UP_IN_0P5_SECONDS_THEN_FADE_TO_OFF_IN_1_SECOND)) {
    goto kickout;
  }

  // If the GlobalSceneControl attribute is equal to TRUE, the application on
  // the associated endpoint shall store its settings in its global scene then
  // set the GlobalSceneControl attribute to FALSE.
  status = readGlobalSceneControl(endpoint, &globalSceneControl);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  } else if (globalSceneControl) {
    status = emberAfScenesClusterStoreCurrentSceneCallback(endpoint,
                                                           ZCL_SCENES_GLOBAL_SCENE_GROUP_ID,
                                                           ZCL_SCENES_GLOBAL_SCENE_SCENE_ID);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfOnOffClusterPrintln("ERR: %ping %p %x", "stor", "global scene", status);
      goto kickout;
    }
    globalSceneControl = FALSE;
    status = writeGlobalSceneControl(endpoint, globalSceneControl);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
  }

  // The application will handle the actual effect and variant.
  status = emberAfPluginZllOnOffServerOffWithEffectCallback(endpoint,
                                                            effectId,
                                                            effectVariant);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    // If the application handled the effect, the endpoint shall enter its
    // "off" state, update the OnOff attribute accordingly, and set the OnTime
    // attribute to 0x0000.
    status = emberAfOnOffClusterSetValueCallback(endpoint, ZCL_OFF_COMMAND_ID, FALSE);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
      status = writeOnTime(endpoint, 0x0000);
    }
  }

kickout:
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfOnOffClusterOnWithRecallGlobalSceneCallback(void)
{
  EmberAfStatus status;
  boolean globalSceneControl;
  int8u endpoint = emberAfCurrentEndpoint();

  status = readGlobalSceneControl(endpoint, &globalSceneControl);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  } else if (!globalSceneControl) {
    int16u onTime;
    status = emberAfScenesClusterRecallSavedSceneCallback(endpoint,
                                                          ZCL_SCENES_GLOBAL_SCENE_GROUP_ID,
                                                          ZCL_SCENES_GLOBAL_SCENE_SCENE_ID);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfOnOffClusterPrintln("ERR: %ping %p %x", "recall", "global scene", status);
      goto kickout;
    }
    globalSceneControl = TRUE;
    status = writeGlobalSceneControl(endpoint, globalSceneControl);
    if (status  != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
    status = readOnTime(endpoint, &onTime);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
    if (onTime == 0x0000) {
      status = writeOffWaitTime(endpoint, onTime);
      if (status != EMBER_ZCL_STATUS_SUCCESS) {
        goto kickout;
      }
    }
  } else {
    goto kickout;
  }

kickout:
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfOnOffClusterOnWithTimedOffCallback(int8u onOffControl,
                                                  int16u onTime,
                                                  int16u offWaitTime)
{
  EmberAfStatus status;
  int16u onTimeAttribute, offWaitTimeAttribute;
  boolean onOffAttribute;
  int8u endpoint = emberAfCurrentEndpoint();

  // The valid range of the OnTime and OffWaitTime fields is 0x0000 to 0xFFFF.
  if (onTime == 0xFFFF || offWaitTime == 0xFFFF) {
    status = EMBER_ZCL_STATUS_INVALID_FIELD;
    goto kickout;
  }

  // On receipt of this command, if the accept only when on sub-field of the
  // on/off control field is set to 1 and the value of the OnOff attribute is
  // equal to 0x00 (off), the command shall be discarded.
  status = readOnOff(endpoint, &onOffAttribute);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  } else if ((onOffControl & ZLL_ON_OFF_CLUSTER_ON_OFF_CONTROL_ACCEPT_ONLY_WHEN_ON_MASK)
             && !onOffAttribute) {
    goto kickout;
  }

  status = readOnTime(endpoint, &onTimeAttribute);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  }
  status = readOffWaitTime(endpoint, &offWaitTimeAttribute);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  }

  // If the value of the OffWaitTime attribute is greater than zero and the
  // value of the OnOff attribute is equal to 0x00, then the device shall set
  // the OffWaitTime attribute to the minimum of the OffWaitTime attribute and
  // the value specified in the off wait time field.  In all other cases, the
  // device shall set the OnTime attribute to the maximum of the OnTime
  // attribute and the value specified in the on time field, set the
  // OffWaitTime attribute to the value specified in the off wait time field
  // and set the OnOff attribute to 0x01 (on).
  if (0x0000 < offWaitTimeAttribute && !onOffAttribute) {
    if (offWaitTime < offWaitTimeAttribute) {
      offWaitTimeAttribute = offWaitTime;
    }
    status = writeOffWaitTime(endpoint, offWaitTimeAttribute);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
  } else {
    if (onTimeAttribute < onTime) {
      onTimeAttribute = onTime;
    }
    status = writeOnTime(endpoint, onTimeAttribute);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
    offWaitTimeAttribute = offWaitTime;
    status = writeOffWaitTime(endpoint, offWaitTimeAttribute);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
    onOffAttribute = TRUE;
    status = writeOnOff(endpoint, onOffAttribute);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      goto kickout;
    }
  }

  // If the values of the OnTime and OffWaitTime attributes are both less than
  // 0xFFFF, the device shall then update the device every 1/10th second until
  // both the OnTime and OffWaitTime attributes are equal to 0x0000.
  if (onTimeAttribute < 0xFFFF && offWaitTimeAttribute < 0xFFFF) {
    emberAfScheduleClusterTick(endpoint,
                               ZCL_ON_OFF_CLUSTER_ID,
                               EMBER_AF_SERVER_CLUSTER_TICK,
                               MILLISECOND_TICKS_PER_SECOND / 10,
                               EMBER_AF_OK_TO_HIBERNATE);
  } else {
    emberAfDeactivateClusterTick(endpoint,
                                 ZCL_ON_OFF_CLUSTER_ID,
                                 EMBER_AF_SERVER_CLUSTER_TICK);
  }

kickout:
  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

EmberAfStatus emberAfPluginZllOnOffServerOffZllExtensions(const EmberAfClusterCommand *cmd)
{
  // On receipt of the off command, the OnTime attribute shall be set to
  // 0x0000.
  return writeOnTime(cmd->apsFrame->destinationEndpoint, 0x0000);
}

EmberAfStatus emberAfPluginZllOnOffServerOnZllExtensions(const EmberAfClusterCommand *cmd)
{
  // On receipt of the on command, if the value of the OnTime attribute is
  // equal to 0x0000, the device shall set the OffWaitTime attribute to 0x0000.
  int16u onTime;
  int8u endpoint = cmd->apsFrame->destinationEndpoint;
  EmberAfStatus status = readOnTime(endpoint, &onTime);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return status;
  } else if (onTime == 0x0000) {
    status = writeOffWaitTime(endpoint, 0x0000);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      return status;
    }
  }

  // The GlobalSceneControl attribute shall be set to TRUE after the reception
  // of a standard ZCL on command.
  return writeGlobalSceneControl(endpoint, TRUE);
}

EmberAfStatus emberAfPluginZllOnOffServerToggleZllExtensions(const EmberAfClusterCommand *cmd)
{
  // On receipt of the toggle command, if the value of the OnOff attribute is
  // equal to 0x00 and if the value of the OnTime attribute is equal to 0x0000,
  // the device shall set the OffWaitTime attribute to 0x0000.  If the value of
  // the OnOff attribute is equal to 0x01, the OnTime attribute shall be set to
  // 0x0000.  When this function is called, the OnOff attribute has already
  // been toggled, so the logic is reversed.
  boolean onOff;
  int8u endpoint = cmd->apsFrame->destinationEndpoint;
  EmberAfStatus status = readOnOff(endpoint, &onOff);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return status;
  } else if (onOff) {
    int16u onTime;
    status = readOnTime(endpoint, &onTime);
    if (status == EMBER_ZCL_STATUS_SUCCESS && onTime == 0x0000) {
      status = writeOffWaitTime(endpoint, 0x0000);
    }
    return status;
  } else {
    return writeOnTime(endpoint, 0x0000);
  }
}

EmberAfStatus emberAfPluginZllOnOffServerLevelControlZllExtensions(int8u endpoint)
{
  // On receipt of a level control cluster command that causes the OnOff
  // attribute to be set to 0x00, the OnTime attribute shall be set to 0x0000.
  // On receipt of a level control cluster command that causes the OnOff
  // attribute to be set to 0x01, if the value of the OnTime attribute is equal
  // to 0x0000, the device shall set the OffWaitTime attribute to 0x0000.
  boolean onOff;
  EmberAfStatus status = readOnOff(endpoint, &onOff);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    return status;
  } else if (!onOff) {
    return writeOnTime(endpoint, 0x0000);
  } else {
    int16u onTime;
    status = readOnTime(endpoint, &onTime);
    if (status == EMBER_ZCL_STATUS_SUCCESS && onTime == 0x0000) {
      status = writeOffWaitTime(endpoint, 0x0000);
    }
    return status;
  }
}
