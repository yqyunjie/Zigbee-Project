// *******************************************************************
// * key-establishment-storage-static.c
// *
// * This file implements the routines for storing temporary data that
// * is needed for key establishment.  This is data is completely
// * public and is sent over-the-air and thus not required to be 
// * closely protected.
// *
// * This version uses static memory buffers that are not dynamically
// * allocated.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************


// this file contains all the common includes for clusters in the zcl-util
#include "../../util/common.h"

#include "key-establishment-storage.h"

#ifndef EZSP_HOST
  #include "stack/include/cbke-crypto-engine.h"
#endif

//------------------------------------------------------------------------------
// Globals

static EmberCertificateData partnerCert;
static EmberPublicKeyData partnerPublicKey;
static EmberSmacData storedSmac;

//------------------------------------------------------------------------------

boolean storePublicPartnerData(boolean isCertificate,
                               int8u* data)
{
  int8u* ptr = (isCertificate
                ? emberCertificateContents(&partnerCert)
                : emberPublicKeyContents(&partnerPublicKey));
  int8u size = (isCertificate 
                ? EMBER_CERTIFICATE_SIZE
                : EMBER_PUBLIC_KEY_SIZE);
  MEMCOPY(ptr, data, size);
  return TRUE;
}

boolean retrieveAndClearPublicPartnerData(EmberCertificateData* partnerCertificate,
                                          EmberPublicKeyData* partnerEphemeralPublicKey)
{
  if ( partnerCertificate != NULL ) {
    MEMCOPY(partnerCertificate,
            &partnerCert,
            EMBER_CERTIFICATE_SIZE);
  }
  if ( partnerEphemeralPublicKey != NULL ) {
    MEMCOPY(partnerEphemeralPublicKey,
            &partnerPublicKey,
            EMBER_PUBLIC_KEY_SIZE);
  }
  MEMSET(&partnerCert, 0, EMBER_CERTIFICATE_SIZE);
  MEMSET(&partnerPublicKey, 0, EMBER_PUBLIC_KEY_SIZE);
  return TRUE;
}

boolean storeSmac(EmberSmacData* smac)
{
  MEMCOPY(&storedSmac, smac, EMBER_SMAC_SIZE);
  return TRUE;
}

boolean getSmacPointer(EmberSmacData** smacPtr)
{
  *smacPtr = &storedSmac;
  return TRUE;
}

void clearAllTemporaryPublicData(void)
{
  MEMSET(&storedSmac, 0, EMBER_SMAC_SIZE);
  retrieveAndClearPublicPartnerData(NULL, NULL);
}
