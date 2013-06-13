// *******************************************************************
// * zll-scenes-server.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../scenes/scenes.h"

#define ZCL_SCENES_CLUSTER_MODE_COPY_ALL_SCENES_MASK BIT(0)

boolean emberAfScenesClusterEnhancedAddSceneCallback(int16u groupId,
                                                     int8u sceneId,
                                                     int16u transitionTime,
                                                     int8u *sceneName,
                                                     int8u *extensionFieldSets)
{
  return emberAfPluginScenesServerParseAddScene(emberAfCurrentCommand(),
                                                groupId,
                                                sceneId,
                                                transitionTime,
                                                sceneName,
                                                extensionFieldSets);
}

boolean emberAfScenesClusterEnhancedViewSceneCallback(int16u groupId,
                                                      int8u sceneId)
{
  return emberAfPluginScenesServerParseViewScene(emberAfCurrentCommand(),
                                                 groupId,
                                                 sceneId);
}

boolean emberAfScenesClusterCopySceneCallback(int8u mode,
                                              int16u groupIdFrom,
                                              int8u sceneIdFrom,
                                              int16u groupIdTo,
                                              int8u sceneIdTo)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_INVALID_FIELD;
  boolean copyAllScenes = (mode & ZCL_SCENES_CLUSTER_MODE_COPY_ALL_SCENES_MASK);
  int8u i;

  emberAfScenesClusterPrintln("RX: CopyScene 0x%x, 0x%2x, 0x%x, 0x%2x, 0x%x",
                              mode,
                              groupIdFrom,
                              sceneIdFrom,
                              groupIdTo,
                              sceneIdTo);

  // If a group id is specified but this endpoint isn't in it, take no action.
  if ((groupIdFrom != ZCL_SCENES_GLOBAL_SCENE_GROUP_ID
       && !emberAfGroupsClusterEndpointInGroupCallback(emberAfCurrentEndpoint(),
                                                       groupIdFrom))
      || (groupIdTo != ZCL_SCENES_GLOBAL_SCENE_GROUP_ID
          && !emberAfGroupsClusterEndpointInGroupCallback(emberAfCurrentEndpoint(),
                                                          groupIdTo))) {
    status = EMBER_ZCL_STATUS_INVALID_FIELD;
    goto kickout;
  }

  for (i = 0; i < EMBER_AF_PLUGIN_SCENES_TABLE_SIZE; i++) {
    EmberAfSceneTableEntry from;
    emberAfPluginScenesServerRetrieveSceneEntry(from, i);
    if (from.endpoint == emberAfCurrentEndpoint()
        && from.groupId == groupIdFrom
        && (copyAllScenes || from.sceneId == sceneIdFrom)) {
      int8u j, index = EMBER_AF_SCENE_TABLE_NULL_INDEX;
      for (j = 0; j < EMBER_AF_PLUGIN_SCENES_TABLE_SIZE; j++) {
        EmberAfSceneTableEntry to;
        if (i == j) {
          continue;
        }
        emberAfPluginScenesServerRetrieveSceneEntry(to, j);
        if (to.endpoint == emberAfCurrentEndpoint()
            && to.groupId == groupIdTo
            && to.sceneId == (copyAllScenes ? from.sceneId : sceneIdTo)) {
          index = j;
          break;
        } else if (index == EMBER_AF_SCENE_TABLE_NULL_INDEX
                   && to.endpoint == EMBER_AF_SCENE_TABLE_UNUSED_ENDPOINT_ID) {
          index = j;
        }
      }

      // If the target index is still zero, the table is full.
      if (index == EMBER_AF_SCENE_TABLE_NULL_INDEX) {
        status = EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
        goto kickout;
      }

      // Save the "from" entry to the "to" index.  This makes a copy of "from"
      // with the correct group and scene ids and leaves the original in tact.
      from.groupId = groupIdTo;
      if (!copyAllScenes) {
        from.sceneId = sceneIdTo;
      }
      emberAfPluginScenesServerSaveSceneEntry(from, index);

      if (j != index) {
        emberAfPluginScenesServerIncrNumSceneEntriesInUse();
        emberAfScenesSetSceneCountAttribute(emberAfCurrentEndpoint(),
                                            emberAfPluginScenesServerNumSceneEntriesInUse());
     }

      // If we aren't copying all scenes, we can stop here.
      status = EMBER_ZCL_STATUS_SUCCESS;
      if (!copyAllScenes) {
        goto kickout;
      }
    }
  }

kickout:
  // Copy Scene commands are only responded to when they are addressed to a
  // single device.
  if (emberAfCurrentCommand()->type == EMBER_INCOMING_UNICAST
      || emberAfCurrentCommand()->type == EMBER_INCOMING_UNICAST_REPLY) {
    emberAfFillCommandScenesClusterCopySceneResponse(status,
                                                     groupIdFrom,
                                                     sceneIdFrom);
    emberAfSendResponse();
  }
  return TRUE;
}

EmberAfStatus emberAfPluginZllScenesServerRecallSceneZllExtensions(int8u endpoint)
{
  boolean globalSceneControl = TRUE;
  EmberAfStatus status = emberAfWriteServerAttribute(endpoint,
                                                     ZCL_ON_OFF_CLUSTER_ID,
                                                     ZCL_GLOBAL_SCENE_CONTROL_ATTRIBUTE_ID,
                                                     (int8u *)&globalSceneControl,
                                                     ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfScenesClusterPrintln("ERR: writing global scene control %x", status);
  }
  return status;
}
