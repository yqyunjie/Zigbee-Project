// *******************************************************************
// * zll-scenes-client.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../scenes-client/scenes-client.h"

boolean emberAfScenesClusterEnhancedAddSceneResponseCallback(int8u status,
                                                             int16u groupId,
                                                             int8u sceneId)
{
  return emberAfPluginScenesClientParseAddSceneResponse(emberAfCurrentCommand(),
                                                        status,
                                                        groupId,
                                                        sceneId);
}

boolean emberAfScenesClusterEnhancedViewSceneResponseCallback(int8u status,
                                                              int16u groupId,
                                                              int8u sceneId,
                                                              int16u transitionTime,
                                                              int8u *sceneName,
                                                              int8u *extensionFieldSets)
{
  return emberAfPluginScenesClientParseViewSceneResponse(emberAfCurrentCommand(),
                                                         status,
                                                         groupId,
                                                         sceneId,
                                                         transitionTime,
                                                         sceneName,
                                                         extensionFieldSets);
}

boolean emberAfScenesClusterCopySceneResponseCallback(int8u status,
                                                      int16u groupIdFrom,
                                                      int8u sceneIdFrom)
{
  emberAfScenesClusterPrintln("RX: CopySceneResponse 0x%x, 0x%2x, 0x%x",
                              status,
                              groupIdFrom,
                              sceneIdFrom);
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}
