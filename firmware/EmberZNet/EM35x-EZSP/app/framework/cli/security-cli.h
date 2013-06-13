// File: security-cli.h
//
// Routines to print info about the security keys on the device.
//
// Author(s): Rob Alexander <ralexander@ember.com>
//
// Copyright 2008 by Ember Corporation.  All rights reserved.               *80*

extern EmberCommandEntry cbkeCommands[];
extern EmberCommandEntry changeKeyCommands[];

extern EmberKeyData cliPreconfiguredLinkKey;
extern EmberKeyData cliNetworkKey;

void changeKeyCommand(void);

void cbkeStartCommand(void);
void cbkeInterPanCommand(void);

void printKeyInfo(void);

extern EmberCommandEntry emAfSecurityCommands[];
