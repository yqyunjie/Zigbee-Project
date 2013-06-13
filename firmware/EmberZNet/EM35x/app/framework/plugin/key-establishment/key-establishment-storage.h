// *******************************************************************
// * key-establishment-storage.h
// *
// * API for the storage of public temporary partner data.
// * - Partner Certificate
// * - Partner Ephemeral Public Key
// * - A single SMAC
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// If isCertificate is FALSE, data is a public key.
boolean storePublicPartnerData(boolean isCertificate,
                               int8u* data);
boolean retrieveAndClearPublicPartnerData(EmberCertificateData* partnerCertificate, 
                                          EmberPublicKeyData* partnerEphemeralPublicKey);

boolean storeSmac(EmberSmacData* smac);
boolean getSmacPointer(EmberSmacData** smacPtr);

void clearAllTemporaryPublicData(void);
