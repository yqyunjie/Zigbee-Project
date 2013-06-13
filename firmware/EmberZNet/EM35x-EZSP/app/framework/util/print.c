// *******************************************************************
// * print.c
// *
// * Utilities and command line interface for printing, and enabling/disabling
// * printing to different areas.
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"

//------------------------------------------------------------------------------
// Globals

// Enable this if you want area names printed. It proved annoying
// so we're disabling it here.
//#define EMBER_AF_PRINT_AREA_NAME

#ifdef EMBER_AF_PRINT_AREA_NAME
  static void reallyPrintAreaName(int16u area);
  #define printAreaName(area) reallyPrintAreaName(area)
#else
  #define printAreaName(area)
#endif //EMBER_AF_PRINT_AREA_NAME

#ifdef EMBER_AF_PRINT_NAMES
static PGM_P areaNames[] = EMBER_AF_PRINT_NAMES;
#endif

#ifdef EMBER_AF_PRINT_BITS
static int8u enablementBytes[] = EMBER_AF_PRINT_BITS;
#endif

int16u emberAfPrintActiveArea = 0;

//------------------------------------------------------------------------------

// Returns true if the area print is enabled
boolean emberAfPrintEnabled(int16u area) 
{
#ifndef EMBER_AF_PRINT_BITS
  return FALSE;
#else 
  int8u index = (int8u)(area >> 8);
  if ( area == 0xFFFF ) {
    return TRUE;
  }
  if ( index >= sizeof(enablementBytes) ) {
    return FALSE;
  } else {
    int8u byte = enablementBytes[index];
    return ( ( byte & (int8u)(area & 0xFF) ) != 0x00 );
  }
#endif // EMBER_AF_PRINT_BITS
}

static void printEnable(int16u area, boolean on) 
{
#ifdef EMBER_AF_PRINT_BITS
  int8u index = (int8u)(area >> 8);
  if ( index < sizeof(enablementBytes) ) {
    if ( on ) {
      enablementBytes[index] |= (int8u)(area & 0xFF);
    } else {
      enablementBytes[index] &= ~(int8u)(area & 0xFF);
    }
  }
#endif // EMBER_AF_PRINT_BITS
}

static int16u convertUserNumberAreaToInternalArea(int16u userNumberedArea)
{
  int16u index = userNumberedArea / 8;
  index = (int16u)(index << 8 ) 
    + (int16u)( ((int16u)0x0001) << ( userNumberedArea % 8 ) );
  return index;
}

#if defined EMBER_AF_PRINT_AREA_NAME
static void reallyPrintAreaName(int16u area)
{
#ifdef EMBER_AF_PRINT_NAMES
  int8u hi,lo,count;
  int16u index;

  hi = (int8u)(area >> 8);
  lo = (int8u)(area & 0xFF);
  count = 0;
  
  if ( lo != 0 ) {
    while ( !(lo & 0x01) ) {
      lo >>= 1;
      count++;
    }
  }
  index = ((8 * hi) + count);

  if (area != 0xFFFF 
      && index < EMBER_AF_PRINT_NAME_NUMBER) {
    emberSerialPrintf(EMBER_AF_PRINT_OUTPUT, "%p:", areaNames[index]);
  }
#endif // EMBER_AF_PRINT_NAMES
}
#endif //EMBER_AF_PRINT_AREA_NAME

// Prints the trace if trace is enabled
static void emAfPrintInternalVarArg(int16u area, 
                                    boolean newLine, 
                                    PGM_P formatString, 
                                    va_list ap) {
  if ( !emberAfPrintEnabled(area) ) {
    return;
  }
  printAreaName(area);

  emberSerialPrintfVarArg(EMBER_AF_PRINT_OUTPUT, formatString, ap);

  if (newLine) {
    emberSerialPrintf(EMBER_AF_PRINT_OUTPUT, "\r\n");
  }
  emberAfPrintActiveArea = area;
}

void emberAfFlush(int16u area) 
{
  if ( emberAfPrintEnabled(area) ) {
    emberSerialWaitSend(EMBER_AF_PRINT_OUTPUT);
  }
}

void emberAfPrintln(int16u area, PGM_P formatString, ...) 
{
  va_list ap;
  va_start (ap, formatString);
  emAfPrintInternalVarArg(area, TRUE, formatString, ap);
  va_end(ap);
}

void emberAfPrint(int16u area, PGM_P formatString, ...) 
{
  va_list ap;
  va_start (ap, formatString);
  emAfPrintInternalVarArg(area, FALSE, formatString, ap);
  va_end(ap);
}

void emberAfPrintStatus(void)
{
#ifdef EMBER_AF_PRINT_NAMES
  int8u i;
  for (i = 0; i < EMBER_AF_PRINT_NAME_NUMBER; i++) {
    emberSerialPrintfLine(EMBER_AF_PRINT_OUTPUT, "[%d] %p : %p",
                          i,
                          areaNames[i],
                          (emberAfPrintEnabled(
                            convertUserNumberAreaToInternalArea(i)) 
                           ? "YES" 
                           : "no"));
    emberSerialWaitSend(EMBER_AF_PRINT_OUTPUT);
  }
#endif // EMBER_AF_PRINT_NAMES
}

void emberAfPrintAllOn(void)
{
#ifdef EMBER_AF_PRINT_BITS
  MEMSET(enablementBytes, 0xFF, sizeof(enablementBytes));
#endif
}

void emberAfPrintAllOff(void) 
{
#ifdef EMBER_AF_PRINT_BITS
  MEMSET(enablementBytes, 0x00, sizeof(enablementBytes));
#endif
}

// These are CLI functions where a user will supply a sequential numbered
// area; as opposed to the bitmask area number that we keep track
// of internally.

void emberAfPrintOn(int16u userArea) 
{
  printEnable(convertUserNumberAreaToInternalArea(userArea), 
              TRUE);   // enable?
}

void emberAfPrintOff(int16u userArea) 
{
  printEnable(convertUserNumberAreaToInternalArea(userArea), 
              FALSE);  // enable?
}

