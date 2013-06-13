// *******************************************************************
// * zll-on-off-server.h
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

EmberAfStatus emberAfPluginZllOnOffServerOffZllExtensions(const EmberAfClusterCommand *cmd);
EmberAfStatus emberAfPluginZllOnOffServerOnZllExtensions(const EmberAfClusterCommand *cmd);
EmberAfStatus emberAfPluginZllOnOffServerToggleZllExtensions(const EmberAfClusterCommand *cmd);

EmberAfStatus emberAfPluginZllOnOffServerLevelControlZllExtensions(int8u endpoint);
