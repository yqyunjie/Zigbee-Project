

void emAfStandaloneBootloaderServerPrintTargetClientInfoCommand(void);
void emAfStandaloneBootloaderServerPrintStatus(void);


// Public API
EmberStatus emberAfPluginStandaloneBootloaderServerBroadcastQuery(void);
EmberStatus emberAfPluginStandaloneBootloaderServerStartClientBootload(EmberEUI64 longId,
                                                                       const EmberAfOtaImageId* id,
                                                                       int16u tag);
EmberStatus emberAfPluginStandaloneBootloaderServerStartClientBootloadWithCurrentTarget(const EmberAfOtaImageId* id,
                                                                                        int16u tag);
                                                                               

