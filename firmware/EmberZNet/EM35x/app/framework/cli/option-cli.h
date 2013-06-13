// *****************************************************************************
// * option-cli.h
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#if !defined(OPTION_CLI_H)
#define OPTION_CLI_H

extern EmberCommandEntry emAfOptionCommands[];

void emAfCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result);

#endif
