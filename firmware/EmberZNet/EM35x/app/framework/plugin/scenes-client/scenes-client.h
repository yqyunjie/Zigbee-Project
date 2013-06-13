// *******************************************************************
// * scenes-client.h
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

boolean emberAfPluginScenesClientParseAddSceneResponse(const EmberAfClusterCommand *cmd,
                                                       int8u status,
                                                       int16u groupId,
                                                       int8u sceneId);

boolean emberAfPluginScenesClientParseViewSceneResponse(const EmberAfClusterCommand *cmd,
                                                        int8u status,
                                                        int16u groupId,
                                                        int8u sceneId,
                                                        int16u transitionTime,
                                                        const int8u *sceneName,
                                                        const int8u *extensionFieldSets);