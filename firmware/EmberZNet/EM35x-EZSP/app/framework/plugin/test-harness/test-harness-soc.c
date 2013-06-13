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

//------------------------------------------------------------------------------
// Globals

//------------------------------------------------------------------------------
// Forward Declarations

// Internal stack routines
void emResetApsFrameCounter(void);
void emTestHarnessAdvanceApsFrameCounter(void);

//------------------------------------------------------------------------------
// Functions

void emAfTestHarnessResetApsFrameCounter(void)
{
  // Reseting the outgoing APS frame counter is non-standard and not
  // a good idea, especially on the TC.  This is necessary for Smart Energy
  // Key establihsment tests 15.39 and 15.40.  It is only necessary for a test
  // harness device.
  emResetApsFrameCounter();
}

void emAfTestHarnessAdvanceApsFrameCounter(void)
{
  emTestHarnessAdvanceApsFrameCounter();
}

//------------------------------------------------------------------------------
