/*
 * File: hal/micro/generic/mem-util.c
 * Description: generic memory manipulation routines.
 *
 * Author(s): Lee Taylor, lee@ember.com,
 *            Jeff Mathews, jm@ember.com
 *
 * Copyright 2004 by Ember Corporation. All rights reserved.                *80*
 */

#include PLATFORM_HEADER
#include "stack/include/ember-types.h"
#include "include/error.h"

#include "hal/hal.h"

#if !defined(XAP2B) && !defined(UNIX)
// A version of memcopy that can handle overlapping source and
// destination regions.

void halCommonMemCopy(void *dest, const void *source, int8u bytes)
{
  int8u *d = (int8u *)dest;
  int8u *s = (int8u *)source;

  if (d > s) {
    d += bytes - 1;
    s += bytes - 1;
    while(bytes >= 4) {
      bytes -= 4;
      *d-- = *s--;
      *d-- = *s--;
      *d-- = *s--;
      *d-- = *s--;
    }
    for(; bytes; bytes--) {
      *d-- = *s--;
    }
  } else {
    while(bytes >= 4) {
      bytes -= 4;
      *d++ = *s++;
      *d++ = *s++;
      *d++ = *s++;
      *d++ = *s++;
    }
    for(; bytes; bytes--) {
      *d++ = *s++;
    }
  }
}

// Same as above except copies from Program space to RAM.
void halCommonMemPGMCopy(void* dest, void PGM *source, int8u bytes)
{
  int8u *d = (int8u *)dest;
  PGM_PU s = (PGM_PU)source;

  if (d > s) {
    d += bytes - 1;
    s += bytes - 1;
    while(bytes >= 4) {
      bytes -= 4;
      *d-- = *s--;
      *d-- = *s--;
      *d-- = *s--;
      *d-- = *s--;
    }
    for(; bytes; bytes--) {
      *d-- = *s--;
    }
  } else {
    while(bytes >= 4) {
      bytes -= 4;
      *d++ = *s++;
      *d++ = *s++;
      *d++ = *s++;
      *d++ = *s++;
    }
    for(; bytes; bytes--) {
      *d++ = *s++;
    }
  }
}

void halCommonMemSet(void *dest, int8u val, int16u bytes)
{
  int8u *d=(int8u *)dest;

  for(;bytes;bytes--) {
    *d++ = val;
  }
}

int8s halCommonMemCompare(const void *source0, const void *source1, int8u bytes)
{
  int8u *s0 = (int8u *)source0;
  int8u *s1 = (int8u *)source1;

  for(; 0 < bytes; bytes--, s0++, s1++) {
    int8u b0 = *s0;
    int8u b1 = *s1;
    if (b0 != b1)
      return b0 - b1;
  }
  return 0;
}

// Test code for halCommonMemCompare().  There is no good place for unit tests
// for this file.  If you change the function you should probably rerun the
// test.
//  {
//    int8u s0[3] = { 0, 0, 0};
//    int8u s1[3] = { 0, 0, 0};
//    int8u i;
//    assert(halCommonMemCompare(s0, s1, 0) == 0);
//    assert(halCommonMemCompare(s0, s1, 3) == 0);
//    for (i = 0; i < 3; i++) {
//      s0[i] = 1;
//      assert(halCommonMemCompare(s0, s1, 3) > 0);
//      s1[i] = 2;
//      assert(halCommonMemCompare(s0, s1, 3) < 0);
//      s0[i] = 2;
//    }
//  }

// Same again, except that the second source is in program space.

int8s halCommonMemPGMCompare(const void *source0, void PGM *source1, int8u bytes)
{
  int8u *s0 = (int8u *)source0;
  int8u PGM *s1 = (int8u PGM *)source1;

  for(; 0 < bytes; bytes--, s0++, s1++) {
    int8u b0 = *s0;
    int8u b1 = *s1;
    if (b0 != b1)
      return b0 - b1;
  }
  return 0;
}
#endif
