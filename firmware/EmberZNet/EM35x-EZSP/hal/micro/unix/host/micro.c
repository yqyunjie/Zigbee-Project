/**
 * File: hal/micro/unix/host/micro.c
 * Description: PC host files for the HAL.
 *
 * Copyright 2003 by Ember Corporation. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "include/error.h"

static void (*microRebootHandler)(void) = NULL;

void halInit(void)
{
}

void setMicroRebootHandler(void (*handler)(void))
{
  microRebootHandler = handler;
}

void halReboot(void)
{
  if (microRebootHandler) {
    microRebootHandler();
  }
  printf("\nReboot not supported.  Exiting instead.\n");
  exit(0);
}

int8u halGetResetInfo(void)
{
   return RESET_SOFTWARE;
}

PGM_P halGetResetString(void)
{
  static PGM_P resetString = "SOFTWARE";
  return (resetString);
}

// Ideally this should not be necessary, but the serial code references
// this for all platforms.
void simulatedSerialTimePasses(void) 
{
}

void halPowerDown(void)
{
}

void halPowerUp(void)
{
}

void halSleep(SleepModes sleepMode)
{
}

// Implement this to catch incorrect HAL calls for this platform.
void halCommonSetSystemTime(int32u time)
{
  fprintf(stderr, "FATAL:  halCommonSetSystemTime() not supported!\n");
  assert(0);
}
