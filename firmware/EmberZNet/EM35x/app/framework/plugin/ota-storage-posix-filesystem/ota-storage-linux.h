// Internal definitions for the OTA storage Linux.

typedef void (EmAfOtaStorageFileAddedHandler)(const EmberAfOtaHeader*);

typedef struct {
  boolean memoryDebug;
  boolean fileDebug;
  boolean fieldDebug;
  boolean ignoreFilesWithUnderscorePrefix;
  boolean printFileDiscoveryOrRemoval;
  EmAfOtaStorageFileAddedHandler* fileAddedHandler;
} EmAfOtaStorageLinuxConfig;

void emAfOtaStorageGetConfig(EmAfOtaStorageLinuxConfig* currentConfig);
void emAfOtaStorageSetConfig(const EmAfOtaStorageLinuxConfig* newConfig);

