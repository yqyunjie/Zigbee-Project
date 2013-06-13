// *******************************************************************
// * af-main.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include CONFIGURATION_HEADER
#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros
#include "stack/include/ember-types.h"

#if !defined(EMBER_AF_CUSTOM_NETWORK_INIT_OPTIONS)
  #define EMBER_AF_CUSTOM_NETWORK_INIT_OPTIONS EMBER_NETWORK_INIT_NO_OPTIONS

  // For EZSP hosts we must be careful not to use emberNetworkInitExtended() unless
  // the application needs it (i.e. they have set custom options).  Old NCP images
  // do not have support for this EZSP frame and thus will reject it, triggering
  // an EZSP error.
  #if defined(EZSP_HOST)
    #define EMBER_AF_USE_STANDARD_NETWORK_INIT
  #endif
#endif

#define MFG_STRING_MAX_LENGTH 16

// returnData must be MFG_STRING_MAX_LENGTH in length and 
// is NOT expected to be NULL terminated (could be though)
void emberAfGetMfgString(int8u* returnData);

// Functions common to both SOC and Host versions of the application.
boolean emberAfIsFullSmartEnergySecurityPresent(void);

#if defined(EZSP_HOST)
  void emAfClearNetworkCache(int8u networkIndex);
#else
  #define emAfClearNetworkCache(index)
  int8u emAfCopyMessageIntoRamBuffer(EmberMessageBuffer message,
                                     int8u *buffer,
                                     int16u bufLen);
#endif

#if defined EZSP_HOST
// Normally this is provided by the stack code, but on the host
// it is provided by the application code.
void emberReverseMemCopy(int8u* dest, const int8u* src, int16u length);

// utility for setting an EZSP config value and printing the result
EzspStatus emberAfSetEzspConfigValue(EzspConfigId configId,
                                     int16u value,
                                     PGM_P configIdName);

// utility for setting an EZSP policy and printing the result
EzspStatus emberAfSetEzspPolicy(EzspPolicyId policyId,
                                EzspDecisionId decisionId,
                                PGM_P policyName,
                                PGM_P decisionName);

// utility for setting an EZSP value and printing the result
EzspStatus emberAfSetEzspValue(EzspValueId valueId,
                               int8u valueLength,
                               int8u *value,
                               PGM_P valueName);

EmberStatus emberAfEzspSetSourceRoute(EmberNodeId id);

boolean emberAfNcpNeedsReset(void);

#endif // EZSP_HOST

void emAfPrintStatus(PGM_P task,
                     EmberStatus status);

int8u emberAfGetSecurityLevel(void);
int8u emberAfGetKeyTableSize(void);
int8u emberAfGetBindingTableSize(void);
int8u emberAfGetAddressTableSize(void);
int8u emberAfGetChildTableSize(void);
int8u emberAfGetRouteTableSize(void);
int8u emberAfGetNeighborTableSize(void);
int8u emberAfGetStackProfile(void);
int8u emberAfGetSleepyMulticastConfig(void);


void emAfCopyCounterValues(int16u* counters);
int8u emAfGetPacketBufferFreeCount(void);
int8u emAfGetPacketBufferTotalCount(void);

EmberStatus emberAfGetChildData(int8u index,
                                EmberNodeId *childId,
                                EmberEUI64 childEui64,
                                EmberNodeType *childType);

void emAfCliVersionCommand(void);

EmberStatus emAfPermitJoin(int8u duration, 
                           boolean broadcastMgmtPermitJoin);
void emAfStopSmartEnergyStartup(void);

boolean emAfProcessZdo(EmberNodeId sender,
                       EmberApsFrame* apsFrame,
                       int8u* message, 
                       int16u length);

void emAfIncomingMessageHandler(EmberIncomingMessageType type,
                                EmberApsFrame *apsFrame,
                                int8u lastHopLqi,
                                int8s lastHopRssi,
                                int16u messageLength,
                                int8u *messageContents);
EmberStatus emAfSend(EmberOutgoingMessageType type,
                     int16u indexOrDestination,
                     EmberApsFrame *apsFrame,
                     int8u messageLength,
                     int8u *message);
void emAfMessageSentHandler(EmberOutgoingMessageType type,
                            int16u indexOrDestination,
                            EmberApsFrame *apsFrame,
                            EmberStatus status,
                            int16u messageLength,
                            int8u *messageContents);

void emAfStackStatusHandler(EmberStatus status);
EmberStatus emAfNetworkInit(void);

// For Test purposes only
extern EmberStatus emAfNetworkInitReturnCodeStatus;

int8u emberAfCopyBigEndianEui64Argument(int8s index, EmberEUI64 destination);
void emAfScheduleFindAndRejoinEvent(void);

extern const EmberEUI64 emberAfNullEui64;

void emberAfFormatMfgString(int8u* mfgString);
EmberStatus emberAfPermitJoin(int8u duration,
                              boolean broadcastMgmtPermitJoin);
#define emberAfBroadcastPermitJoin(duration) \
  emberAfPermitJoin((duration), TRUE)

extern boolean emberAfPrintReceivedMessages;

void emAfParseAndPrintVersion(EmberVersion versionStruct);
void emAfPrintEzspEndpointFlags(int8u endpoint);

// Old names
#define emberAfMoveInProgress() emberAfMoveInProgressCallback()
#define emberAfStartMove()      emberAfStartMoveCallback()
#define emberAfStopMove()       emberAfStopMoveCallback()
