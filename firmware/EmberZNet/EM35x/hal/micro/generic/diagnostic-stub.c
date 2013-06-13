/**
 * File: hal/micro/generic/diagnostic-stub.c
 * Description: stubs for HAL diagnostic functions.
 *
 * Copyright 2003 by Ember Corporation. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "hal/hal.h"
#include "include/error.h"

void halStartPCDiagnostics(void)
{
}

void halStopPCDiagnostics(void)
{
}

int16u halGetPCDiagnostics(void)
{
  return 0;
}
