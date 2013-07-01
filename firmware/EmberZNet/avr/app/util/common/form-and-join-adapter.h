// File: form-and-join-adapter.h
//
// Description: interface for adapting the form-and-join library to run on
// both an EZSP host processor and on a Zigbee processor such as the EM250.
// Application writers do not need to look at this file.  See
// form-and-join.h for information on using the library.

// The EmberZigbeeNetwork struct type does not include lqi and rssi, but those
// values are typically desired as well.  Rather than changing the stack API,
// bundle them together in a new struct.
typedef struct {
  EmberZigbeeNetwork network;
  int8u lqi;
  int8u rssi;
} NetworkInfo;

// Functions the adapters must implement.
int8u        formAndJoinStackProfile(void);
NetworkInfo *formAndJoinGetNetworkPointer(int8u index);
void         formAndJoinSetCleanupTimeout(void);
int8u       *formAndJoinAllocateBuffer(void);
void         formAndJoinReleaseBuffer(void);
EmberStatus  formAndJoinSetBufferLength(int8u entryCount);

// For use by the node adapter on expiration of the cleanup timer.
void emberFormAndJoinCleanup(EmberStatus status);


