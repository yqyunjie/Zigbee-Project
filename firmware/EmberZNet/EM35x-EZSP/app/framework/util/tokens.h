// *******************************************************************
// * tokens.h
// *
// * Framework header file, responsible for including generated tokens
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifdef CUSTOM_TOKEN_HEADER
#include CUSTOM_TOKEN_HEADER
#endif

#include "app/framework/include/af-types.h"

#ifdef GENERATED_TOKEN_HEADER

// If we have generated header, use it.
#include GENERATED_TOKEN_HEADER

#else 

// We don't have generated header. Default is to have no tokens.

#define GENERATED_TOKEN_LOADER(endpoint) {}
#define GENERATED_TOKEN_SAVER {}

#endif // GENERATED_TOKEN_HEADER


// These are the tokens for the scenes table if they are needed
#ifdef EMBER_AF_PLUGIN_SCENES_USE_TOKENS
#define CREATOR_SCENES_NUM_ENTRIES 0x8723
#define CREATOR_SCENES_TABLE 0x8724
#ifdef DEFINETOKENS
DEFINE_BASIC_TOKEN(SCENES_NUM_ENTRIES, int8u, 0x00)
DEFINE_INDEXED_TOKEN(SCENES_TABLE,
                     EmberAfSceneTableEntry,
                     EMBER_AF_PLUGIN_SCENES_TABLE_SIZE,
                     {EMBER_AF_SCENE_TABLE_UNUSED_ENDPOINT_ID})
#endif //DEFINETOKENS
#endif //EMBER_AF_PLUGIN_SCENES_USE_TOKEN

// These are the tokens for the report table if they are needed
#ifdef EMBER_AF_PLUGIN_REPORTING
#define CREATOR_REPORT_TABLE 0x8725
#ifdef DEFINETOKENS
DEFINE_INDEXED_TOKEN(REPORT_TABLE,
                     EmberAfPluginReportingEntry,
                     EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE,
                     {FALSE})
#endif //DEFINETOKENS
#endif //EMBER_AF_PLUGIN_REPORTING
