// *******************************************************************
// * attribute-storage.h
// *
// * Contains the per-endpoint configuration of attribute tables.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef __AF_ATTRIBUTE_STORAGE__
#define __AF_ATTRIBUTE_STORAGE__

#include PLATFORM_HEADER
#include "../include/af.h"

#if !defined (EMBER_SCRIPTED_TEST)
  #include "att-storage.h"
#endif

#if !defined(ATTRIBUTE_STORAGE_CONFIGURATION) && defined(EMBER_TEST)
#define ATTRIBUTE_STORAGE_CONFIGURATION "attribute-storage-test.h"
#endif

// ATTRIBUTE_STORAGE_CONFIGURATION macro
// contains the file that contains the initial set-up of the
// attribute data structures. If it is missing
// we use the provider sample.
#ifndef ATTRIBUTE_STORAGE_CONFIGURATION
  #error "Must define ATTRIBUTE_STORAGE_CONFIGURATION to specify the App. Builder default attributes file."
#else
  #include ATTRIBUTE_STORAGE_CONFIGURATION
#endif

// If we have fixed number of endpoints, then max is the same.
#ifdef FIXED_ENDPOINT_COUNT
#define MAX_ENDPOINT_COUNT FIXED_ENDPOINT_COUNT
#endif


#define CLUSTER_TICK_FREQ_ALL            (0x00)
#define CLUSTER_TICK_FREQ_QUARTER_SECOND (0x04)
#define CLUSTER_TICK_FREQ_HALF_SECOND    (0x08)
#define CLUSTER_TICK_FREQ_SECOND         (0x0C)

extern int8u attributeData[]; // main storage bucket for all attributes


extern int8u attributeDefaults[]; // storage bucked for > 2b default values

void emAfCallInits(void);

#define emberAfClusterIsClient(cluster) (((cluster)->mask & CLUSTER_MASK_CLIENT) != 0)
#define emberAfClusterIsServer(cluster) (((cluster)->mask & CLUSTER_MASK_SERVER) != 0)
#define emberAfDoesClusterHaveInitFunction(cluster) (((cluster)->mask & CLUSTER_MASK_INIT_FUNCTION) != 0)
#define emberAfDoesClusterHaveAttributeChangedFunction(cluster) (((cluster)->mask & CLUSTER_MASK_ATTRIBUTE_CHANGED_FUNCTION) != 0)
#define emberAfDoesClusterHaveDefaultResponseFunction(cluster) (((cluster)->mask & CLUSTER_MASK_DEFAULT_RESPONSE_FUNCTION) != 0)
#define emberAfDoesClusterHaveMessageSentFunction(cluster) (((cluster)->mask & CLUSTER_MASK_MESSAGE_SENT_FUNCTION) != 0)

// Initial configuration
void emberAfEndpointConfigure(void);
boolean emberAfExtractCommandIds(boolean outgoing,
                                 EmberAfClusterCommand *cmd,
                                 int16u clusterId,
                                 int8u *buffer,
                                 int16u bufferLength,
                                 int16u *bufferIndex,
                                 int8u startId,
                                 int8u maxIdCount);

EmberAfStatus emAfReadOrWriteAttribute(EmberAfAttributeSearchRecord *attRecord,
                                       EmberAfAttributeMetadata **metadata,
                                       int8u *buffer,
                                       int16u maxLength,
                                       boolean write);

boolean emAfMatchCluster(EmberAfCluster *cluster,
                         EmberAfAttributeSearchRecord *attRecord);
boolean emAfMatchAttribute(EmberAfCluster *cluster,
                           EmberAfAttributeMetadata *am,
                           EmberAfAttributeSearchRecord *attRecord);

// This function returns the index of cluster for the particular endpoint.
// Mask is either CLUSTER_MASK_CLIENT or CLUSTER_MASK_SERVER
// For example, if you have 3 endpoints, 10, 11, 12, and cluster X server is
// located on 11 and 12, and cluster Y server is located only on 10 then 
//    clusterIndex(X,11,CLUSTER_MASK_SERVER) returns 0, 
//    clusterIndex(X,12,CLUSTER_MASK_SERVER) returns 1,
//    clusterIndex(X,10,CLUSTER_MASK_SERVER) returns 0xFF
//    clusterIndex(Y,10,CLUSTER_MASK_SERVER) returns 0
//    clusterIndex(Y,11,CLUSTER_MASK_SERVER) returns 0xFF
//    clusterIndex(Y,12,CLUSTER_MASK_SERVER) returns 0xFF
int8u emberAfClusterIndex(int8u endpoint, 
                          EmberAfClusterId clusterId, 
                          int8u mask);


// If server == true, returns the number of server clusters,
// otherwise number of client clusters on this endpoint
int8u emberAfClusterCount(int8u endpoint, boolean server);

// Returns the clusterId of Nth server or client cluster,
// depending on server toggle.
EmberAfCluster *emberAfGetNthCluster(int8u endpoint, int8u n, boolean server);

// Returns number of clusters put into the passed cluster list
// for the given endpoint and client/server polarity
int8u emberAfGetClustersFromEndpoint(int8u endpoint, int16u *clusterList, int8u listLen, boolean server);

// Returns cluster within the endpoint, or NULL if it isn't there
EmberAfCluster *emberAfFindCluster(int8u endpoint, EmberAfClusterId clusterId, int8u mask);

// Function mask must contain one of the CLUSTER_MASK function macros,
// then this method either returns the function pointer or null if
// function doesn't exist. Before you call the function, you must
// cast it.
EmberAfGenericClusterFunction emberAfFindClusterFunction(EmberAfCluster *cluster, EmberAfClusterMask functionMask);

// Loads the attributes from built-in default
void emberAfLoadAttributesFromDefaults(int8u endpoint);

// This function loads from tokens all the attributes that
// are defined to be stored in tokens.
void emberAfLoadAttributesFromTokens(int8u endpoint);

// After the RAM value has changed, code should call this
// function. If this attribute has been 
// tagged as stored-to-token, then code will store
// the attribute to token. 
void emberAfSaveAttributeToToken(int8u *data,
                                 int8u endpoint, 
                                 int16u clusterId,
                                 EmberAfAttributeMetadata *metadata);

// Calls the attribute changed callback
void emAfClusterAttributeChangedCallback(int8u endpoint,
                                            EmberAfClusterId clusterId,
                                            EmberAfAttributeId attributeId,
                                            int8u clientServerMask,
                                            int16u manufacturerCode);

// Calls the default response callback
void emberAfClusterDefaultResponseCallback(int8u endpoint,
                                           EmberAfClusterId clusterId,
                                           int8u commandId,
                                           EmberAfStatus status,
                                           int8u clientServerMask);

// Calls the message sent callback
void emberAfClusterMessageSentCallback(EmberOutgoingMessageType type,
                                       int16u indexOrDestination,
                                       EmberApsFrame *apsFrame,
                                       int16u msgLen,
                                       int8u *message,
                                       EmberStatus status);

// Used to retrieve a manufacturer code from an attribute metadata
int16u emAfGetManufacturerCodeForCluster(EmberAfCluster *cluster);
int16u emAfGetManufacturerCodeForAttribute(EmberAfCluster *cluster,
                                           EmberAfAttributeMetadata *attMetaData);


// Checks a cluster mask byte against ticks passed bitmask
// returns TRUE if the mask matches a passed interval
boolean emberAfCheckTick(int8u mask, 
                         int8u passedMask);

boolean emberAfEndpointIsEnabled(int8u endpoint);

#endif // __AF_ATTRIBUTE_STORAGE__
