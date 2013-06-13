// *******************************************************************
// * scenes.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

EmberAfStatus emberAfScenesSetSceneCountAttribute(int8u endpoint,
                                                  int8u newCount);
EmberAfStatus emberAfScenesMakeValid(int8u endpoint,
                                     int8u sceneId,
                                     int16u groupId);

// DEPRECATED.
#define emberAfScenesMakeInvalid emberAfScenesClusterMakeInvalidCallback

void emAfPluginScenesServerPrintInfo(void);

extern int8u emberAfPluginScenesServerEntriesInUse;
#if defined(EMBER_AF_PLUGIN_SCENES_USE_TOKENS) && !defined(EZSP_HOST)
  // In this case, we use token storage
  #define emberAfPluginScenesServerRetrieveSceneEntry(entry, i) \
    halCommonGetIndexedToken(&entry, TOKEN_SCENES_TABLE, i)
  #define emberAfPluginScenesServerSaveSceneEntry(entry, i) \
    halCommonSetIndexedToken(TOKEN_SCENES_TABLE, i, &entry)
  #define emberAfPluginScenesServerNumSceneEntriesInUse() \
    (halCommonGetToken(&emberAfPluginScenesServerEntriesInUse, TOKEN_SCENES_NUM_ENTRIES), \
     emberAfPluginScenesServerEntriesInUse)
  #define emberAfPluginScenesServerSetNumSceneEntriesInUse(x) \
    (emberAfPluginScenesServerEntriesInUse = x, \
     halCommonSetToken(TOKEN_SCENES_NUM_ENTRIES, &emberAfPluginScenesServerEntriesInUse))
  #define emberAfPluginScenesServerIncrNumSceneEntriesInUse() \
    ((halCommonGetToken(&emberAfPluginScenesServerEntriesInUse, TOKEN_SCENES_NUM_ENTRIES), \
      ++emberAfPluginScenesServerEntriesInUse), \
     halCommonSetToken(TOKEN_SCENES_NUM_ENTRIES, &emberAfPluginScenesServerEntriesInUse))
  #define emberAfPluginScenesServerDecrNumSceneEntriesInUse() \
    ((halCommonGetToken(&emberAfPluginScenesServerEntriesInUse, TOKEN_SCENES_NUM_ENTRIES), \
      --emberAfPluginScenesServerEntriesInUse), \
     halCommonSetToken(TOKEN_SCENES_NUM_ENTRIES, &emberAfPluginScenesServerEntriesInUse))
#else
  // Use normal RAM storage
  extern EmberAfSceneTableEntry emberAfPluginScenesServerSceneTable[];
  #define emberAfPluginScenesServerRetrieveSceneEntry(entry, i) \
    (entry = emberAfPluginScenesServerSceneTable[i])
  #define emberAfPluginScenesServerSaveSceneEntry(entry, i) \
    (emberAfPluginScenesServerSceneTable[i] = entry)
  #define emberAfPluginScenesServerNumSceneEntriesInUse() \
    (emberAfPluginScenesServerEntriesInUse)
  #define emberAfPluginScenesServerSetNumSceneEntriesInUse(x) \
    (emberAfPluginScenesServerEntriesInUse = x)
  #define emberAfPluginScenesServerIncrNumSceneEntriesInUse() \
    (++emberAfPluginScenesServerEntriesInUse)
  #define emberAfPluginScenesServerDecrNumSceneEntriesInUse() \
    (--emberAfPluginScenesServerEntriesInUse)
#endif // Use tokens

boolean emberAfPluginScenesServerParseAddScene(const EmberAfClusterCommand *cmd,
                                               int16u groupId,
                                               int8u sceneId,
                                               int16u transitionTime,
                                               int8u *sceneName,
                                               int8u *extensionFieldSets);
boolean emberAfPluginScenesServerParseViewScene(const EmberAfClusterCommand *cmd,
                                                int16u groupId,
                                                int8u sceneId);
