// *******************************************************************
// * print.h
// *
// * Macros for functional area and per-cluster debug printing
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef __AF_DEBUG_PRINT__
#define __AF_DEBUG_PRINT__

#include "../include/af.h"

#if defined (EMBER_TEST) && !defined(EMBER_AF_PRINT_NAMES)
#include "debug-printing-test.h"
#endif

#if !defined(EMBER_AF_PRINT_OUTPUT) && defined(APP_SERIAL)
  #define EMBER_AF_PRINT_OUTPUT APP_SERIAL
#endif

extern int16u emberAfPrintActiveArea;


// These print functions are required by the CBKE crypto engine.
#define emberAfPrintZigbeeKey printZigbeeKey
#define emberAfPrintCert      printCert
#define emberAfPrintKey       printKey
#define emberAfPrintIeeeLine  printIeeeLine
#define emberAfPrintTextLine  printTextLine

#define emberAfPrintPublicKey(key)  printKey(TRUE,  key)
#define emberAfPrintPrivateKey(key) printKey(FALSE, key)

void emberAfPrintZigbeeKey(const int8u *key);
void emberAfPrintCert(const int8u *cert);
void emberAfPrintKey(boolean publicKey, const int8u *key);
void emberAfPrintIeeeLine(const EmberEUI64 ieee);
void emberAfPrintTextLine(PGM_P text);

void emberAfPrint8ByteBlocks(int8u numBlocks,
                             const int8u *block,
                             boolean crBetweenBlocks);
void emberAfPrintIssuer(const int8u *issuer);

void emberAfPrintChannelListFromMask(int32u channelMask);

#endif // __AF_DEBUG_PRINT__
