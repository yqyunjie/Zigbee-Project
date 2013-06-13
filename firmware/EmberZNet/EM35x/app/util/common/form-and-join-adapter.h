// File: form-and-join-adapter.h
//
// Description: interface for adapting the form-and-join library to run on
// both an EZSP host processor and on a Zigbee SoC such as the EM250.
// Application writers do not need to look at this file.  See
// form-and-join.h for information on using the library.

// The EmberZigbeeNetwork struct type does not include lqi and rssi, but those
// values are typically desired as well.  Rather than changing the stack API,
// bundle them together in a new struct.
typedef struct {
  EmberZigbeeNetwork network;
  int8u lqi;
  int8s rssi;
} NetworkInfo;

// Functions the adapters must implement.
int8u        formAndJoinStackProfile(void);
NetworkInfo *formAndJoinGetNetworkPointer(int8u index);
void         formAndJoinSetCleanupTimeout(void);
int8u       *formAndJoinAllocateBuffer(void);
void         formAndJoinReleaseBuffer(void);
EmberStatus  formAndJoinSetBufferLength(int8u entryCount);

// The node implementation relies on the NetworkInfo struct being 16 bytes, in
// order to place two of them into a 32 byte EmberMessageBuffer.  This compile
// time check is courtesy of http://www.jaggersoft.com/pubs/CVu11_5.html.
#ifndef EZSP_HOST
extern char require_16_bytes[(sizeof(NetworkInfo) == 16) ? 1 : -1];
#endif
