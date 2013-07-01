/*
 * File: diagnostic.c
 * Description: AVR Atmega128-specific program counter diagnostic HAL functions
 *
 * Author(s): Lee Taylor, lee@ember.com,
 *            Jeff Mathews, jm@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#define ENABLE_BIT_DEFINITIONS
#include <ioavr.h>

#include <stdarg.h>

#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "include/error.h"

#include "hal/hal.h"
#include "app/util/serial/serial.h"

// reserved ram is special, see reserved-ram.h
__no_init __root int16u last_known_program_counter   @ HAL_RESERVED_RAM_PC_DIAG;

void halInternalStoreCurrentPC(void)
{
  // The stack:
  //   SP  : empty
  //   SP+1: PC1 (high byte)
  //   SP+2: PC1 (low byte)
  //   SP+3: PC2 (high byte)
  //   SP+4: PC2 (low byte)
  // PC1 is the instruction that will be executed when this function returns.
  // PC2 is the next instruction up the stack.
  int8u l,h;
  int16u pc;

#ifdef AVR_ATMEGA_64
  TCNT3L = 0;
  TCNT3H = 0;
#else
  TCNT3 = 0;
#endif
  // Store PC2
  h = *((int8u*)(SP+3));
  l = *((int8u*)(SP+4));
  pc = HIGH_LOW_TO_INT(h,l);
  // convert from word to byte address
  pc = pc << 1;
  last_known_program_counter = pc;
}

#pragma vector = TIMER3_OVF_vect
__interrupt void microStoreCurrentPC_interrupt (void)
{
  INT_DEBUG_BEG_MISC_INT();
  halInternalStoreCurrentPC();
  INT_DEBUG_END_MISC_INT();
}

int16u halGetPCDiagnostics(void)
{
  return last_known_program_counter;
}

void halStartPCDiagnostics(void)
{
   // Use 16 bit timer3 with a 1:8 prescalar

   ATOMIC_LITE(
     // ensure interrupt is cleared
     ETIFR = BIT(TOV3);  // write logic one to clear interrupts on AVRs

     // prescaler clk/8: ea tick is 2us. (ovf is 131ms)
     TCCR3B = BIT(CS31) | BIT(WGM33) | BIT(WGM32);  
     TCCR3A = BIT(WGM30) | BIT(WGM31);
     
     OCR3A = 200;  // will sample every 100us

#ifdef AVR_ATMEGA_64
     TCNT3L = 0;
     TCNT3H = 0;
#else
     TCNT3 = 0;
#endif
     ETIMSK |= BIT(TOIE3);  // enable timer 1 overflow interrupt
   ) // ATOMIC_LITE
}

void halStopPCDiagnostics(void)
{
  TCCR3B = 0;              // disable timer 3 to save power
  ETIMSK &= ~BIT(TOIE3);   // disable timer 3 overflow interrupt
}

int16u atomicStart;
int16u atomicMaxTime;
int8u atomicAddr[4];

void halResetAtomicClock(void)
{
  int8u i;
  ATOMIC_LITE (
    atomicMaxTime = 0;
    for (i = 0; i < 4; i++)
      atomicAddr[i] = 0;
  )
}

void halStartAtomicClock(void)
{
  atomicStart = TCNT1;
}

  // The stack:
  //   SP  : empty
  //   SP+1: PC (high byte)
  //   SP+2: PC (low byte)
  // PC is the instruction that will be executed when this function returns.
void halStopAtomicClock(void)
{
  int16u time;

  time = TCNT1 - atomicStart;
  if (time > atomicMaxTime) {
    atomicMaxTime = time;
    atomicAddr[0] = *((int8u*)(SP+1));
    atomicAddr[1] = *((int8u*)(SP+2));
    atomicAddr[2] = *((int8u*)(SP+3));
    atomicAddr[3] = *((int8u*)(SP+4));
  }  
}

void halPrintAtomicTiming(int8u port, boolean reset)
{
  int32u pc0, pc1;
  int16u time;

  time = atomicMaxTime * 8;
  pc0 = ((int32u)HIGH_LOW_TO_INT(atomicAddr[0], atomicAddr[1])) << 1;
  pc1 = ((int32u)HIGH_LOW_TO_INT(atomicAddr[2], atomicAddr[3])) << 1;
 
  emberSerialPrintf(port, 
    "Max ATOMIC time %d usecs at PC 0x%4X (caller: 0x%4X)\r\n", time, pc0, pc1);

  if (reset) {
    halResetAtomicClock();
  }
}

#ifdef ENABLE_DATA_LOGGING
#define LOG_DATA_MAX 32
static int8u logData[LOG_DATA_MAX];
static int8u logDataI;

void halLogData(int8u v1, int8u v2, int8u v3, int8u v4)
{
  ATOMIC_LITE(
    logData[logDataI+0] = v1;
    logData[logDataI+1] = v2;
    logData[logDataI+2] = v3;
    logData[logDataI+3] = v4;
    logDataI += 4;
    if (logDataI >= LOG_DATA_MAX) logDataI=0;
  )
}

void halPrintLogData(int8u port)
{
  int8u i;

  ATOMIC_LITE(
    for(i=logDataI; i < LOG_DATA_MAX; i += 4) {
      emberSerialGuaranteedPrintf(port,
                                  " %x%x%x%x",
                                  logData[i+0],
                                  logData[i+1],
                                  logData[i+2],
                                  logData[i+3]);
    }
    for(i=0; i < (logDataI); i += 4) {
      emberSerialGuaranteedPrintf(port,
                                  " %x%x%x%x",
                                  logData[i+0],
                                  logData[i+1],
                                  logData[i+2],
                                  logData[i+3]);
    }
  ) // ATOMIC_LITE
}
#endif
