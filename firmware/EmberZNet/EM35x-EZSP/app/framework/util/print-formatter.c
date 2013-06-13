// ****************************************************************************
// * print-formatter.c
// *
// * Utilities for printing in common formats: buffers, EUI64s, keys
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"

//------------------------------------------------------------------------------

static void emAfPrintBuffer(int16u area,
                            const int8u *buffer,
                            int16u bufferLen,
                            PGM_P formatString)
{
  if (emberAfPrintEnabled(area)) {
    int16u index = 0;
    for (; index < bufferLen; index++) {
      emberAfPrint(area, formatString, buffer[index]);
      if (index % 16 == 6) {
        emberAfFlush(area);
      }
    }
  }
}

void emberAfPrintBuffer(int16u area, 
                        const int8u *buffer, 
                        int16u bufferLen,
                        boolean withSpace) 
{
  emAfPrintBuffer(area, buffer, bufferLen, (withSpace ? "%x " : "%x"));
}

void emberAfPrintString(int16u area, const int8u *buffer) {
  emAfPrintBuffer(area, buffer + 1, emberAfStringLength(buffer), "%c");
}

void emberAfPrintLongString(int16u area, const int8u *buffer) {
  emAfPrintBuffer(area, buffer + 2, emberAfLongStringLength(buffer), "%c");
}

void emberAfPrintLittleEndianEui64(const EmberEUI64 eui64)
{
  emberAfPrint(emberAfPrintActiveArea,
               "(%c)%X%X%X%X%X%X%X%X",
               '<',
               eui64[0], 
               eui64[1], 
               eui64[2], 
               eui64[3],
               eui64[4], 
               eui64[5], 
               eui64[6], 
               eui64[7]);
}

void emberAfPrintBigEndianEui64(const EmberEUI64 eui64)
{
  emberAfPrint(emberAfPrintActiveArea,
               "(%c)%X%X%X%X%X%X%X%X",
               '>',
               eui64[7], 
               eui64[6], 
               eui64[5], 
               eui64[4],
               eui64[3], 
               eui64[2], 
               eui64[1], 
               eui64[0]);
}

void emberAfPrintZigbeeKey(const int8u *key)
{
  // Zigbee Keys are 16 bytes long
  emberAfPrint8ByteBlocks(2, key, FALSE);
}

void emberAfPrintCert(const int8u *cert)
{
  // Certificates are 48 bytes long
  emberAfPrint8ByteBlocks(6, cert, TRUE);
}

void emberAfPrintKey(boolean publicKey, const int8u *key)
{
  // ECC Public Keys are 22 bytes
  // ECC Private Keys are 21 bytes

  emberAfPrintZigbeeKey(key);
  emberAfPrintBuffer(emberAfPrintActiveArea, key + 16, 5, TRUE);
  emberAfPrintln(emberAfPrintActiveArea, (publicKey ? "%X" : ""), key[21]);
  emberAfFlush(emberAfPrintActiveArea);
}

void emberAfPrintIeeeLine(const EmberEUI64 ieee)
{
  emberAfPrintBigEndianEui64(ieee);
  emberAfPrintln(emberAfPrintActiveArea, "");
}

void emberAfPrintTextLine(PGM_P text)
{
  emberAfPrintln(emberAfPrintActiveArea, "%p", text);
}

void emberAfPrint8ByteBlocks(int8u numBlocks,
                             const int8u *block,
                             boolean crBetweenBlocks)
{
  int8u i;
  for (i = 0; i < numBlocks; i++) {
    emberAfPrintBuffer(emberAfPrintActiveArea, block + 8 * i, 8, TRUE);
    // By moving the '%p' to a separate function call, we can
    // save CONST space.  The above string is duplicated elsewhere in the
    // code and therefore will be deadstripped.
    emberAfPrint(emberAfPrintActiveArea,
                 " %p",
                 (crBetweenBlocks || ((i + 1) == numBlocks) ? "\r\n" : ""));
    emberAfFlush(emberAfPrintActiveArea);
  }
}

void emberAfPrintIssuer(const int8u* issuer)
{
  // The issuer field in the certificate is in big-endian form.
  emberAfPrint(emberAfPrintActiveArea, "(>) ");
  emberAfPrint8ByteBlocks(1, issuer, TRUE);
}

void emberAfPrintChannelListFromMask(int32u channelMask)
{
  if (emberAfPrintEnabled(emberAfPrintActiveArea)) {
    int8u i;
    boolean firstPrint = TRUE;
    channelMask >>= EMBER_MIN_802_15_4_CHANNEL_NUMBER;
    for (i = EMBER_MIN_802_15_4_CHANNEL_NUMBER;
         i <= EMBER_MAX_802_15_4_CHANNEL_NUMBER;
         i++) {
      if (channelMask & 0x01UL) {
        if (!firstPrint) {
          emberAfPrint(emberAfPrintActiveArea, ", ");
        }
        emberAfPrint(emberAfPrintActiveArea, "%d", i);
        emberAfFlush(emberAfPrintActiveArea);
        firstPrint = FALSE;
      }
      channelMask >>= 1;
    }
  }
}
