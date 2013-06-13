// *******************************************************************
// * ota-cli.h
// *
// * Zigbee Over-the-air bootload cluster for upgrading firmware and 
// * downloading specific file.  This is the CLI to interact with the
// * main cluster code.
// * 
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


// Common CLI interface

void emAfOtaPrintAllImages(void);
EmberAfOtaImageId emAfOtaFindImageIdByIndex(int8u index);
void emAfOtaReloadStorageDevice(void);

#define OTA_COMMON_COMMANDS  \
  emberCommandEntryAction("printImages", emAfOtaPrintAllImages, "", ""), \
  emberCommandEntryAction("delete",      (CommandAction)emAfOtaImageDelete, "u", ""), \
  emberCommandEntryAction("reload",      emAfOtaReloadStorageDevice,        "", ""), \
  emberCommandEntryAction("storage-info",emAfOtaStorageInfoPrint,           "", ""), \
  emberCommandEntryTerminator(),\


// Client CLI interface

#if !defined (EMBER_AF_PLUGIN_OTA_CLIENT)
  #define OTA_CLIENT_COMMANDS
#endif

void otaFindServerCommand(void);
void otaQueryServerCommand(void);
void otaUsePageRequestCommand(void);
void otaQuerySpecificFileCommand(void);
void otaSendUpgradeCommand(void);
void emAfOtaImageDelete(void);

// Server CLI interface

#if !defined (EMBER_AF_PLUGIN_OTA_SERVER)
  #define OTA_SERVER_COMMANDS
#endif

void otaImageNotifyCommand(void);
