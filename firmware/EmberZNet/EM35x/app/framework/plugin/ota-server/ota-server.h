// *******************************************************************
// * ota-server.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

int8u emAfOtaServerGetBlockSize(void);
int8u emAfOtaImageBlockRequestHandler(EmberAfImageBlockRequestCallbackStruct* callbackData);

boolean emAfOtaPageRequestErrorHandler(void);

void emAfOtaPageRequestTick(int8u endpoint);

// Returns the status code to the request.
int8u emAfOtaPageRequestHandler(int8u endpoint,
                                const EmberAfOtaImageId* id,
                                int32u offset,
                                int8u maxDataSize,
                                int16u pageSize,
                                int16u responseSpacing);

boolean emAfOtaServerHandlingPageRequest(void);

// This will eventually be moved into a Plugin specific callbacks file.
void emberAfOtaServerSendUpgradeCommandCallback(EmberNodeId dest,
                                                int8u endpoint,
                                                const EmberAfOtaImageId* id);


#if defined(EMBER_TEST) && !defined(EM_AF_TEST_HARNESS_CODE)
  #define EM_AF_TEST_HARNESS_CODE
#endif

