// *******************************************************************
// * common.h
// *
// * This file contains the includes that are common to all clusters
// * in the util.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// App framework
#include "app/framework/include/af.h"
#include "app/framework/util/util.h"
#include "app/framework/util/attribute-table.h"
#include "app/framework/util/attribute-storage.h"

// the variables used to setup and send responses to cluster messages
extern EmberApsFrame emberAfResponseApsFrame;
extern int8u  appResponseData[EMBER_AF_RESPONSE_BUFFER_LEN];
extern int16u appResponseLength;
extern EmberNodeId emberAfResponseDestination;
