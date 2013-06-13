// *******************************************************************
// * poll-control-client.h
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// Set fast polling mode
void emAfSetFastPollingMode(boolean mode);

// Set fast polling timeout
void emAfSetFastPollingTimeout(int16u timeout);

// Set response mode
void emAfSetResponseMode(boolean mode);

// Print mode and timeout
void emAfPollControlClientPrint(void);
