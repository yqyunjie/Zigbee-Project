
void emberAfPluginEepromInit(void);
void emberAfPluginEepromNoteInitializedState(boolean state);
const HalEepromInformationType* emberAfPluginEepromInfo(void);
int8u emberAfPluginEepromWrite(int32u address, const int8u *data, int16u totalLength);
int8u emberAfPluginEepromRead(int32u address, int8u *data, int16u totalLength);
int8u emberAfPluginEepromErase(int32u address, int16u totalLength);
boolean emberAfPluginEepromBusy(void);
boolean emberAfPluginEepromShutdown(void);
