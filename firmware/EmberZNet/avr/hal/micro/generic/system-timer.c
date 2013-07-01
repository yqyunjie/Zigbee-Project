/**
 * File: hal/micro/avr-atmega/128/system-timer-sim.c
 * Description: simulation files for the system timer part of the HAL
 *
 * Copyright 2003 by Ember Corporation. All rights reserved.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/time.h>

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"

int32u halCommonGetInt32uMillisecondTick(void)
{
  struct timeval tv;
  int32u now;  

  gettimeofday(&tv, NULL);
  now = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
  return now;
}

int16u halCommonGetInt16uMillisecondTick(void)
{
  return (int16u)halCommonGetInt32uMillisecondTick();
}

int16u halCommonGetInt16uQuarterSecondTick(void)
{
  return (int16u)(halCommonGetInt32uMillisecondTick() >> 8);
}
