// File: ezsp-frame-utilities.h
// 
// Description: Functions for reading and writing command and response frames.
// 
// Copyright 2007 by Ember Corporation. All rights reserved.                *80*

#ifndef __EZSP_FRAME_UTILITIES_H__
#define __EZSP_FRAME_UTILITIES_H__

// The contents of the current EZSP frame.  This pointer can be used inside
// ezspErrorHandler() to obtain information about the command that preceded
// the error (such as the command ID, index EZSP_FRAME_ID_INDEX).
extern int8u* ezspFrameContents;

// This pointer steps through the received frame as the contents are read.
extern int8u* ezspReadPointer;

// This pointer steps through the to-be-transmitted frame as the contents
// are written.
extern int8u* ezspWritePointer;

#pragma pagezero_on  // place these functions in zero-page memory for xap 
int8u fetchInt8u(void);
void appendInt8u(int8u value);
int16u fetchInt16u(void);
void appendInt16u(int16u value);
#pragma pagezero_off

void appendInt32u(int32u value);
int32u fetchInt32u(void);

void appendInt8uArray(int8u length, int8u *contents);
void fetchInt8uArray(int8u length, int8u *contents);

void appendInt16uArray(int8u length, int16u *contents);
void fetchInt16uArray(int8u length, int16u *contents);

// For functions that are used only once in the em260 code we use macros
// in order to save code space.

#define appendEmberNetworkParameters(value)             \
do {                                                    \
  appendInt8uArray(EXTENDED_PAN_ID_SIZE, (value)->extendedPanId);       \
  appendInt16u((value)->panId);                         \
  appendInt8u((value)->radioTxPower);                   \
  appendInt8u((value)->radioChannel);                   \
  appendInt8u((value)->joinMethod);                     \
  appendInt16u((value)->nwkManagerId);                  \
  appendInt8u((value)->nwkUpdateId);                    \
  appendInt32u((value)->channels);                      \
} while (0)
     
void fetchEmberNetworkParameters(EmberNetworkParameters *value);

void appendEmberApsFrame(EmberApsFrame *value);
void fetchEmberApsFrame(EmberApsFrame *apsFrame);

#define appendEmberBindingTableEntry(value)             \
do {                                                    \
  appendInt8u((value)->type);                           \
  appendInt8u((value)->local);                          \
  appendInt16u((value)->clusterId);                     \
  appendInt8u((value)->remote);                         \
  appendInt8uArray(EUI64_SIZE, (value)->identifier);    \
} while (0)

#define fetchEmberBindingTableEntry(value)              \
do {                                                    \
  (value)->type      = fetchInt8u();                    \
  (value)->local     = fetchInt8u();                    \
  (value)->clusterId = fetchInt16u();                   \
  (value)->remote    = fetchInt8u();                    \
  fetchInt8uArray(EUI64_SIZE, (value)->identifier);     \
} while (0)

void appendEmberMulticastTableEntry(EmberMulticastTableEntry *value);
void fetchEmberMulticastTableEntry(EmberMulticastTableEntry *value);

void appendEmberNeighborTableEntry(EmberNeighborTableEntry *value);
void fetchEmberNeighborTableEntry(EmberNeighborTableEntry *value);

#define appendEmberNeighborTableEntry(value)            \
do {                                                    \
  appendInt16u((value)->shortId);                       \
  appendInt8u((value)->averageLqi);                     \
  appendInt8u((value)->inCost);                         \
  appendInt8u((value)->outCost);                        \
  appendInt8u((value)->age);                            \
  appendInt8uArray(EUI64_SIZE, (value)->longId);        \
} while (0)

#define appendEmberRouteTableEntry(value)               \
do {                                                    \
  appendInt16u((value)->destination);                   \
  appendInt16u((value)->nextHop);                       \
  appendInt8u((value)->status);                         \
  appendInt8u((value)->age);                            \
} while (0)

void fetchEmberRouteTableEntry(EmberRouteTableEntry *value);

#define appendEmberKeyData(value) \
  (appendInt8uArray(EMBER_ENCRYPTION_KEY_SIZE, (value)->contents))

#define fetchEmberKeyData(value) \
  (fetchInt8uArray(EMBER_ENCRYPTION_KEY_SIZE, (value)->contents))

void appendEmberInitialSecurityState(EmberInitialSecurityState *value);

#define fetchEmberInitialSecurityState(value)           \
do {                                                    \
  (value)->bitmask = fetchInt16u();                     \
  fetchEmberKeyData(&((value)->preconfiguredKey));      \
  fetchEmberKeyData(&((value)->networkKey));            \
  (value)->networkKeySequenceNumber = fetchInt8u();     \
} while (0)

#define appendEmberCurrentSecurityState(value)          \
do {                                                    \
  appendInt16u((value)->bitmask);                       \
  appendInt8uArray(EUI64_SIZE, (value)->trustCenterLongAddress);\
} while (0)

void fetchEmberCurrentSecurityState(EmberCurrentSecurityState *value);

#define appendEmberKeyStruct(value)                     \
do {                                                    \
  appendInt16u((value)->bitmask);                       \
  appendInt8u((value)->type);                           \
  appendEmberKeyData(&((value)->key));                  \
  appendInt32u((value)->outgoingFrameCounter);          \
  appendInt32u((value)->incomingFrameCounter);          \
  appendInt8u((value)->sequenceNumber);                 \
  appendInt8uArray(EUI64_SIZE, (value)->partnerEUI64);  \
} while (0)

void fetchEmberKeyStruct(EmberKeyStruct *value);

void fetchEmberZigbeeNetwork(EmberZigbeeNetwork *value);

#define appendEmberCertificateData(value) \
 (appendInt8uArray(EMBER_CERTIFICATE_SIZE, (value)->contents))
#define fetchEmberCertificateData(value) \
  (fetchInt8uArray(EMBER_CERTIFICATE_SIZE, (value)->contents))
#define appendEmberPublicKeyData(value) \
  (appendInt8uArray(EMBER_PUBLIC_KEY_SIZE, (value)->contents))
#define fetchEmberPublicKeyData(value) \
  (fetchInt8uArray(EMBER_PUBLIC_KEY_SIZE, (value)->contents))
#define appendEmberPrivateKeyData(value) \
  (appendInt8uArray(EMBER_PRIVATE_KEY_SIZE, (value)->contents))
#define fetchEmberPrivateKeyData(value) \
  (fetchInt8uArray(EMBER_PRIVATE_KEY_SIZE, (value)->contents))
#define appendEmberSmacData(value) \
  (appendInt8uArray(EMBER_SMAC_SIZE, (value)->contents))
#define fetchEmberSmacData(value) \
  (fetchInt8uArray(EMBER_SMAC_SIZE, (value)->contents))
#define appendEmberSignatureData(value) \
  (appendInt8uArray(EMBER_SIGNATURE_SIZE, (value)->contents))
#define fetchEmberSignatureData(value) \
  (fetchInt8uArray(EMBER_SIGNATURE_SIZE, (value)->contents))

#endif // __EZSP_FRAME_UTILITIES_H__
