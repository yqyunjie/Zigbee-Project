// *******************************************************************
// * ota-server-policy.h
// *
// * A sample policy file that implements the callbacks for the 
// * Zigbee Over-the-air bootload cluster server.
// * 
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

void emAfOtaServerPolicyPrint(void);

void emAfOtaServerSetQueryPolicy(int8u value);
void emAfOtaServerSetBlockRequestPolicy(int8u value);
void emAfOtaServerSetUpgradePolicy(int8u value);

boolean emAfServerPageRequestTickCallback(int16u relativeOffset, int8u blockSize);
void emAfSetPageRequestMissedBlockModulus(int16u modulus);
void emAfOtaServerSetPageRequestPolicy(int8u value);
void emAfOtaServerPolicySetMinBlockRequestPeriod(int16u minBlockRequestPeriodSeconds);
int8u emAfOtaServerImageBlockRequestCallback(EmberAfImageBlockRequestCallbackStruct* data);
