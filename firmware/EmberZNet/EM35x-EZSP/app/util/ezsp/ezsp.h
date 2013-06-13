// File: ezsp.h
// 
// Description: Host EZSP layer. Provides functions that allow the Host
// application to send every EZSP command to the EM260. The command and response
// parameters are defined in the datasheet.
// 
// Copyright 2007-2010 by Ember Corporation. All rights reserved.           *80*

#ifndef __EZSP_H__
#define __EZSP_H__

// Reset the EM260 and initialize the serial protocol (SPI or UART). After this
// function returns EZSP_SUCCESS, the EM260 has finished rebooting and is ready
// to accept a command.
EzspStatus ezspInit(void);

// For ezsp-uart, must be called before setting sleep mode and enabling
// synchronous callbacks (read via ezspCallback()).
void ezspEnableNcpSleep(boolean enable);

// Returns TRUE if there are one or more callbacks queued up on the EM260
// awaiting collection.
boolean ezspCallbackPending(void);

// The sleep mode to use in the frame control of every command sent. The Host
// application can set this to the desired EM260 sleep mode. Subsequent commands
// will pass this value to the EM260.
extern int8u ezspSleepMode;

// Wakes the EM260 up from deep sleep.
void ezspWakeUp(void);

// For ezsp-uart, indicate whether the Host can sleep without disrupting the
// uart protocol. In particular the Host may not sleep unless the NCP is 
// already asleep.
boolean ezspOkToSleep(void);

// The Host application must call this function periodically to allow the EZSP
// layer to handle asynchronous events.
void ezspTick(void);

// The EZSP layer calls this function after sending a command while waiting for
// the response. The Host application can use this function to perform any tasks
// that do not involve the EM260.
void ezspWaitingForResponse(void);

// Callback from the EZSP layer indicating that the transaction with the EM260
// could not be completed due to a serial protocol error or that the response
// received from the EM260 reported an error. The status parameter provides more
// information about the error. This function must be provided by the Host
// application.
void ezspErrorHandler(EzspStatus status);

// Cleanly close down the serial protocol (SPI or UART). After this function has
// been called, ezspInit() must be called to resume communication with the
// EM260.
void ezspClose(void);

//----------------------------------------------------------------
// Functions with special handling

EzspStatus ezspGetVersionStruct(EmberVersion* returnData);

EzspStatus ezspSetEndpointFlags(int8u endpoint,
                                EzspEndpointFlags flags);
EzspStatus ezspGetEndpointFlags(int8u endpoint,
                                EzspEndpointFlags* returnFlags);

#define ezspEnableEndpoint(endpoint) \
  ezspSetEndpointFlags((endpoint), EZSP_ENDPOINT_ENABLED)

boolean emberStackIsPerformingRejoin(void);
EmberStatus emberSetMfgSecurityConfig(int32u magicNumber,
                                      const EmberMfgSecurityStruct* settings);
EmberStatus emberGetMfgSecurityConfig(EmberMfgSecurityStruct* settings);
EmberStatus emberStartWritingStackTokens(void);
EmberStatus emberStopWritingStackTokens(void);
EmberStatus emberSetExtendedSecurityBitmask(EmberExtendedSecurityBitmask mask);
EmberStatus emberGetExtendedSecurityBitmask(EmberExtendedSecurityBitmask* mask);
EmberStatus emberSetNodeId(EmberNodeId nodeId);
void emberSetMaximumIncomingTransferSize(int16u size);
void emberSetMaximumOutgoingTransferSize(int16u size);
void emberSetDescriptorCapability(int8u capability);
int8u emberGetLastStackZigDevRequestSequence(void);
EmberStatus emberSendUnicastNetworkKeyUpdate(EmberNodeId targetShort,
                                             EmberEUI64  targetLong,
                                             EmberKeyData* newKey);
EmberStatus emberAesHashSimple(int8u totalLength,
                               const int8u* data,
                               int8u* result);
int8u emberGetCurrentNetwork(void);
EmberStatus emberSetCurrentNetwork(int8u index);
int8u emberGetCallbackNetwork(void);

EmberStatus emberFindAndRejoinNetworkWithReason(boolean haveCurrentNetworkKey,
                                                int32u channelMask,
                                                EmberRejoinReason reason);
EmberStatus emberFindAndRejoinNetwork(boolean haveCurrentNetworkKey,
                                      int32u channelMask);

EmberRejoinReason emberGetLastRejoinReason(void);
EmberLeaveReason emberGetLastLeaveReason(EmberNodeId* id);

#define EZSP_MAXIMIZE_PACKET_BUFFER_COUNT 0xFF

//----------------------------------------------------------------
// ZLL methods

boolean emberIsZllNetwork(void);
void emberZllGetTokenStackZllData(EmberTokTypeStackZllData *token);
void emberZllGetTokenStackZllSecurity(EmberTokTypeStackZllSecurity *token);
void emberZllSetTokenStackZllData(EmberTokTypeStackZllData *token);
void emberZllSetNonZllNetwork(void);
EmberStatus emberZllSetInitialSecurityState(const EmberKeyData *networkKey,
                                            const EmberZllInitialSecurityState *securityState);
EmberStatus emberZllFormNetwork(EmberZllNetwork* networkInfo,
                                int8s radioTxPower);
EmberStatus emberZllStartScan(int32u channelMask,
                              int8s radioPowerForScan,
                              EmberNodeType nodeType);
EmberStatus emberZllSetRxOnWhenIdle(int16u durationMs);
EmberStatus emberZllJoinTarget(const EmberZllNetwork* targetNetworkInfo);

//----------------------------------------------------------------

#include "command-prototypes.h"

#endif // __EZSP_H__
