

typedef int8u BackchannelState;

enum {
  NO_CONNECTION = 0,
  CONNECTION_EXISTS = 1,
  NEW_CONNECTION = 2,
  CONNECTION_ERROR = 3,
};

extern const boolean backchannelSupported;
extern boolean backchannelEnable;

EmberStatus backchannelStartServer(int8u port);
EmberStatus backchannelStopServer(int8u port);
EmberStatus backchannelReceive(int8u port, char* data);
EmberStatus backchannelSend(int8u port, int8u * data, int8u length);

BackchannelState backchannelCheckConnection(int8u port, 
                                            boolean waitForConnection);

EmberStatus backchannelMapStandardInputOutputToRemoteConnection(int port);
EmberStatus backchannelCloseConnection(int8u port);
EmberStatus backchannelServerPrintf(const char* formatString, ...);
EmberStatus backchannelClientPrintf(int8u port, const char* formatString, ...);
EmberStatus backchannelClientVprintf(int8u port, 
                                     const char* formatString, 
                                     va_list ap);

