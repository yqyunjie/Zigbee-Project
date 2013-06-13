// *******************************************************************
// * bootload-ezsp.h
// * 
// * Common defines for setup.
// * Warning:  Needs to be included early.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef DEMO_DEBUG_PORT
#define DEMO_DEBUG_PORT APP_SERIAL
#endif

#define BOOTLOAD_PRINTF(p,...) debugPrintf(DEMO_DEBUG_PORT, __VA_ARGS__)
#define BOOTLOAD_WAITSEND(x)

#define EMBERTICKON 1
#define EZSPTICKON 2
#ifndef GATEWAY_APP
#define BOOTTICKON 4
#endif
