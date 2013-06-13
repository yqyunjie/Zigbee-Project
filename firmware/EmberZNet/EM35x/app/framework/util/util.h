// *******************************************************************
// * util.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef __AF_UTIL_H__
#define __AF_UTIL_H__

// This controls the type of response. Normally The library sends an automatic
// response (if appropriate) on the same PAN. The reply can be disabled by
// calling emberAfSetNoReplyForNextMessage.
#define ZCL_UTIL_RESP_NORMAL   0
#define ZCL_UTIL_RESP_NONE     1
#define ZCL_UTIL_RESP_INTERPAN 2

// Cluster name structure
typedef struct {
  int16u id;
  PGM_P name;
} EmberAfClusterName;


extern PGM EmberAfClusterName zclClusterNames[];

#define ZCL_NULL_CLUSTER_ID 0xFFFF

#include "../include/af.h"

// Override APS retry: 0 - don't touch, 1 - always set, 2 - always unset
#define EMBER_AF_RETRY_OVERRIDE_NONE 0
#define EMBER_AF_RETRY_OVERRIDE_SET 1
#define EMBER_AF_RETRY_OVERRIDE_UNSET 2

#define ZCL_DIRECTION_CLIENT_TO_SERVER 0
#define ZCL_DIRECTION_SERVER_TO_CLIENT 1

// EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH is defined in config.h
#define EMBER_AF_RESPONSE_BUFFER_LEN EMBER_AF_MAXIMUM_SEND_PAYLOAD_LENGTH

void emberAfInit(void);
void emberAfTick(void);
int16u emberAfFindClusterNameIndex(int16u cluster);
void emberAfStackDown(void);

void emberAfDecodeAndPrintCluster(int16u cluster);


boolean emberAfProcessMessage(EmberApsFrame *apsFrame,
                              EmberIncomingMessageType type,
                              int8u *message,
                              int16u msgLen,
                              EmberNodeId source,
                              InterPanHeader *interPanHeader);

/**
 * Retrieves the difference between the two passed values.
 * This function assumes that the two values have the same
 * endianess and it only works on datasizes of 4 bytes or less
 */
int32u emberAfGetDifference(int8u* pData,
                            int32u value,
                            int8u dataSize);


/**
 * Retrieves an int32u from the given Zigbee payload. The integer retrieved 
 * may be cast into an integer of the appropriate size depending on the 
 * number of bytes requested from the message. In Zigbee, all integers are
 * passed over the air in LSB form. LSB to MSB conversion is 
 * done within this function automatically before the integer is returned.
 * 
 * Obviously (due to return value) this function can only handle 
 * the retrieval of integers between 1 and 4 bytes in length.
 * 
 */
int32u emberAfGetInt(const int8u* message, int16u currentIndex, int16u msgLen, int8u bytes);

void emberAfClearResponseData(void);
void emberAfPutInt8uInResp(int8u value);
void emberAfPutInt16uInResp(int16u value);
void emberAfPutInt32uInResp(int32u value);
void emberAfPutBlockInResp(const int8u* data, int16u length);
void emberAfPutStringInResp(const int8u *buffer);

extern int8u emberAfApsRetryOverride;
extern boolean emAfApsSecurityOff;


#ifdef EZSP_HOST
// the EM260 host application is expected to provide these functions if using
// a cluster that needs it.
EmberNodeId emberGetSender(void);
EmberStatus emberGetSenderEui64(EmberEUI64 senderEui64);
#endif //EZSP_HOST

// DEPRECATED.
extern int8u emberAfIncomingZclSequenceNumber;

// the caller to the library can set a flag to say do not respond to the
// next ZCL message passed in to the library. Passing TRUE means disable
// the reply for the next ZCL message. Setting to FALSE re-enables the
// reply (in the case where the app disables it and then doesnt send a 
// message that gets parsed).
void emberAfSetNoReplyForNextMessage(boolean set);

// this function determines if APS Link key should be used to secure 
// the message. It is based on the clusterId and specified in the SE
// app profile.  If the message is outgoing then the 
boolean emberAfDetermineIfLinkSecurityIsRequired(int8u commandId,
                                                 boolean incoming,
                                                 boolean broadcast,
                                                 EmberAfProfileId profileId,
                                                 EmberAfClusterId clusterId);

#define isThisDataTypeSentLittleEndianOTA(dataType) \
                    (!(emberAfIsThisDataTypeAStringType(dataType)))

boolean emAfProcessGlobalCommand(EmberAfClusterCommand *cmd);
boolean emAfProcessClusterSpecificCommand(EmberAfClusterCommand *cmd);

extern int8u emberAfResponseType;

#endif // __AF_UTIL_H__
