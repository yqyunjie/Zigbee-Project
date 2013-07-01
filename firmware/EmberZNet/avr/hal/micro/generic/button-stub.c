/**
 * File: hal/micro/generic/button-stub.c
 * Description: stub implementation of button functions.
 *
 * Copyright 2005 by Ember Corporation. All rights reserved.
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "hal/hal.h"

void halInternalInitButton(void) {}

int8u halButtonState(int8u button) { return 0; }

int8u halButtonPinState(int8u button) { return 0; }

int16u halGpioIsr(int16u interrupt, int16u pcbContext) { return 0; }

int16u halTimerIsr(int16u interrupt, int16u pcbContext) { return 0; }
