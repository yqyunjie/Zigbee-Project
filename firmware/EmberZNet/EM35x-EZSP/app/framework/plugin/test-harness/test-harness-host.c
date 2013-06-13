// *****************************************************************************
// * test-harness-soc.c
// *
// *  Test harness code specific to the SOC.
// *
// * Copyright 2012 by Silicon Laboratories. All rights reserved.            80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/util.h"

#include "test-harness.h"

#include "app/xncp-test-harness/xncp-test-harness.h"

//------------------------------------------------------------------------------
// Globals

// For easier readability
#define RESET_FRAME_COUNTER FALSE
#define ADVANCE_FRAME_COUNTER TRUE

//------------------------------------------------------------------------------
// Forward Declarations

//------------------------------------------------------------------------------
// Functions

// Returns false if XNCP is not the right software and doesn't support our
// custom EZSP frames, TRUE if it does.
static boolean checkXncpSoftware(void)
{
  int16u manufacturerId;
  int16u versionNumber;

  EmberStatus status;

  status  = ezspGetXncpInfo(&manufacturerId, &versionNumber);

  if (status != EMBER_SUCCESS) {
    emberAfCorePrintln("Error: XNCP test harness software not present on NCP.");
    return FALSE;

  } else if (manufacturerId != EMBER_MANUFACTURER_ID
             || versionNumber != EMBER_XNCP_TEST_HARNESS_VERSION_NUMBER) {
    emberAfCorePrintln("Error: Wrong XNCP software loaded on NCP.");
    emberAfCorePrintln("  Wanted Manuf ID: 0x%2X, Version: 0x%2X", 
                       EMBER_MANUFACTURER_ID,
                       EMBER_XNCP_TEST_HARNESS_VERSION_NUMBER);
    emberAfCorePrintln("     Got Manuf ID: 0x%2X, Version: 0x%2X",
                       manufacturerId,
                       versionNumber);
    return FALSE;
  }

  return TRUE;
}

static void resetOrAdvanceApsFrameCounter(boolean advance)
{
  int8u customEzspMessage[1];
  int8u reply[1];
  int8u replyLength;
  EmberStatus status;

  customEzspMessage[0] = (advance
                          ? EMBER_XNCP_TEST_HARNESS_COMMAND_ADVANCE_APS_FRAME_COUNTER
                          : EMBER_XNCP_TEST_HARNESS_COMMAND_RESET_APS_FRAME_COUNTER);

  if (!checkXncpSoftware()) {
    return;
  }
  
  status = ezspCustomFrame(1, // length of custom EZSP message
                           customEzspMessage,
                           &replyLength,
                           reply);
  emberAfCorePrintln("%p APS Frame counter: %p (0x%X)",
                     (advance
                      ? "Advancing"
                      : "Reset"),
                     ((status == EMBER_SUCCESS)
                      ? "Success"
                      : "FAILED"),
                     status);

}

void emAfTestHarnessResetApsFrameCounter(void)
{

  // Reseting the outgoing APS frame counter is non-standard and not
  // a good idea, especially on the TC.  This is necessary for Smart Energy
  // Key establihsment tests 15.39 and 15.40.  It is only necessary for a test
  // harness device.

  // In the case of the Host, the XNCP test harness software must be loaded
  // or this cannot be done.  Since resetting frame counters is non-standard
  // and can break things, it is not part of the default NCP binary.

  resetOrAdvanceApsFrameCounter(RESET_FRAME_COUNTER);
}

void emAfTestHarnessAdvanceApsFrameCounter(void)
{
  resetOrAdvanceApsFrameCounter(ADVANCE_FRAME_COUNTER);
}

//------------------------------------------------------------------------------
