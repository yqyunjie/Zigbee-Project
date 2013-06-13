// *******************************************************************
//  common.h
//
//  Contains functions prototypes used by standalone-bootloader-demo-host/demo.c
//  file.
//
//  Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// Start StandaloneBootloader without reporting an error.
EzspStatus hostLaunchStandaloneBootloader(int8u mode);

// Do a formatted conversion of a signed fixed point 32 bit number
// to decimal.  Make lots of assumtions.  Like minDig > dot;
// minDig < 20; etc.
void formatFixed(int8u* charBuffer, int32s value, int8u minDig, int8u dot, boolean showPlus);

// decode the EmberNodeType into text and display it.
void decodeNodeType(EmberNodeType nodeType);
