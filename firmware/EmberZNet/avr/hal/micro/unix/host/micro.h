/**
 * File: hal/micro/unix/host/micro.h
 * Description: Interface definitions for HAL micro unix host.
 *
 * Copyright 2008 by Ember Corporation. All rights reserved.
 */

#ifndef __UNIX_MICRO_H__
#define __UNIX_MICRO_H__

#ifndef __MICRO_H__
#error do not include this file directly - include micro/micro.h
#endif

#define EM_NUM_SERIAL_PORTS 2

void setMicroRebootHandler(void (*handler)(void));


// the number of ticks (as returned from halCommonGetInt32uMillisecondTick)
// that represent an actual second. This can vary on different platforms.
#define MILLISECOND_TICKS_PER_SECOND 1000

#endif //__UNIX_MICRO_H__
