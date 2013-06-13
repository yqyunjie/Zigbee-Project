
void emAfStandaloneBootloaderClientPrintStatus(void);

void emAfStandaloneBootloaderClientGetInfo(int16u* bootloaderVersion,
                                           int8u* platformId,
                                           int8u* microId,
                                           int8u* phyId);

EmberStatus emAfStandaloneBootloaderClientLaunch(void);

void emAfStandaloneBootloaderClientGetMfgInfo(int16u* mfgIdReturnValue,
                                              int8u* boardNameReturnValue);

void emAfStandaloneBootloaderClientGetKey(int8u* returnData);

int32u emAfStandaloneBootloaderClientGetRandomNumber(void);
