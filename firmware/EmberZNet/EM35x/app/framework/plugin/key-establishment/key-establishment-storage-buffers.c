// *******************************************************************
// * key-establishment-storage-buffers.c
// *
// * This file implements the routines for storing temporary data that
// * is needed for key establishment.  This is data is completely
// * public and is sent over-the-air and thus not required to be 
// * closely protected.
// *
// * This version usees Ember message buffers for storing the data.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


// this file contains all the common includes for clusters in the zcl-util
#include "../../util/common.h"

#include "key-establishment-storage.h"
#include "stack/include/cbke-crypto-engine.h"

//------------------------------------------------------------------------------
// Globals

static EmberMessageBuffer certAndPublicKeyBuffer = EMBER_NULL_MESSAGE_BUFFER;
static EmberMessageBuffer smacBuffer = EMBER_NULL_MESSAGE_BUFFER;

#define CERTIFICATE_OFFSET 0
#define PUBLIC_KEY_OFFSET  EMBER_CERTIFICATE_SIZE

//------------------------------------------------------------------------------
// Forward Declarations

static void releaseAndNullBuffer(EmberMessageBuffer* buffer);

//------------------------------------------------------------------------------

boolean storePublicPartnerData(boolean isCertificate,
                               int8u* data)
{
  // The expectation is that the certificate must be stored first
  // and the public key is stored second.  The first time this is called
  // the buffer should be null while second time around it should not be.
  if (isCertificate
      ? certAndPublicKeyBuffer != EMBER_NULL_MESSAGE_BUFFER 
      : certAndPublicKeyBuffer == EMBER_NULL_MESSAGE_BUFFER) {
    return FALSE;
  }
  if (isCertificate) {
    certAndPublicKeyBuffer = emberFillLinkedBuffers(data,
                                                    EMBER_CERTIFICATE_SIZE);
    if ( certAndPublicKeyBuffer == EMBER_NULL_MESSAGE_BUFFER ) {
      return FALSE;
    }
  } else {
    if (EMBER_SUCCESS
        != emberAppendToLinkedBuffers(certAndPublicKeyBuffer,
                                      data,
                                      EMBER_PUBLIC_KEY_SIZE)) {
      releaseAndNullBuffer(&certAndPublicKeyBuffer);
      return FALSE;
    }
  }
  return TRUE;
}

boolean retrieveAndClearPublicPartnerData(EmberCertificateData* partnerCertificate, 
                                          EmberPublicKeyData* partnerEphemeralPublicKey)
{
  int8u length = emberMessageBufferLength(certAndPublicKeyBuffer);
  if ( certAndPublicKeyBuffer == EMBER_NULL_MESSAGE_BUFFER ) {
    return FALSE;
  }

  if ((EMBER_CERTIFICATE_SIZE + EMBER_PUBLIC_KEY_SIZE) > length) {
    return FALSE;
  }
  emberCopyFromLinkedBuffers(certAndPublicKeyBuffer,
                             CERTIFICATE_OFFSET,
                             emberCertificateContents(partnerCertificate),
                             EMBER_CERTIFICATE_SIZE);
  
  emberCopyFromLinkedBuffers(certAndPublicKeyBuffer,
                             PUBLIC_KEY_OFFSET,
                             emberPublicKeyContents(partnerEphemeralPublicKey),
                             EMBER_PUBLIC_KEY_SIZE);

  releaseAndNullBuffer(&certAndPublicKeyBuffer);
  return TRUE;
}

boolean storeSmac(EmberSmacData* smac)
{
  if ( smacBuffer != EMBER_NULL_MESSAGE_BUFFER ) {
    emberReleaseMessageBuffer(smacBuffer);
  }
  emberAfKeyEstablishmentClusterPrintln("Storing SMAC");
  emberAfPrintZigbeeKey(emberKeyContents(smac));
  smacBuffer = emberFillLinkedBuffers(emberSmacContents(smac),
                                      EMBER_SMAC_SIZE);
  if ( smacBuffer == EMBER_NULL_MESSAGE_BUFFER ) {
    return FALSE;
  }
  return TRUE;
}

boolean getSmacPointer(EmberSmacData** smacPtr)
{
  if ( smacBuffer == EMBER_NULL_MESSAGE_BUFFER ) {
    return FALSE;
  }
  *smacPtr = (EmberSmacData*)emberMessageBufferContents(smacBuffer);
  return TRUE;
}

void clearAllTemporaryPublicData(void)
{
  EmberMessageBuffer* buffer = &certAndPublicKeyBuffer;
  int8u i;
  for ( i = 0; i < 2; i++ ) {
    if ( *buffer != EMBER_NULL_MESSAGE_BUFFER ) { 
      releaseAndNullBuffer(buffer);
    }
    buffer = &smacBuffer;
  }
}

static void releaseAndNullBuffer(EmberMessageBuffer* buffer)
{
  emberReleaseMessageBuffer(*buffer);
  *buffer = EMBER_NULL_MESSAGE_BUFFER;
}

//------------------------------------------------------------------------------
