// get version info via spi
extern int8u * XModemQuery(int8u * pLen, boolean reportError);

// Run the bootload protocol
void bootTick(void);

// Send a bootload message
EzspStatus bootSendMessage(int8u len, int8u * bytes);

// Wait for the em260 to finish a reboot.
EzspStatus waitFor260boot(int16u * pLoops);
