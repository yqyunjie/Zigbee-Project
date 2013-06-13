// *******************************************************************
// * attribute-storage.c
// *
// * Contains the per-endpoint configuration of attribute tables.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "attribute-storage.h"
#include "common.h"

//------------------------------------------------------------------------------
// Globals
// This is not declared CONST in order to handle dynamic endpoint information
// retrieved from tokens.
EmberAfDefinedEndpoint emAfEndpoints[MAX_ENDPOINT_COUNT];

#if ( ATTRIBUTE_MAX_SIZE == 0 )
#define ACTUAL_ATTRIBUTE_SIZE 1
#else
#define ACTUAL_ATTRIBUTE_SIZE ATTRIBUTE_MAX_SIZE
#endif

int8u attributeData[ACTUAL_ATTRIBUTE_SIZE];

#if (!defined(ATTRIBUTE_SINGLETONS_SIZE)) \
  || (ATTRIBUTE_SINGLETONS_SIZE == 0)
#define ACTUAL_SINGLETONS_SIZE 1
#else
#define ACTUAL_SINGLETONS_SIZE ATTRIBUTE_SINGLETONS_SIZE
#endif
int8u singletonAttributeData[ACTUAL_SINGLETONS_SIZE];

int8u emberEndpointCount = 0;

// If we have attributes that are more than 2 bytes, then
// we need this data block for the defaults
#ifdef GENERATED_DEFAULTS
const int8u generatedDefaults[]               = GENERATED_DEFAULTS;
#endif // GENERATED_DEFAULTS

#ifdef GENERATED_MIN_MAX_DEFAULTS
const EmberAfAttributeMinMaxValue minMaxDefaults[]          = GENERATED_MIN_MAX_DEFAULTS;
#endif //GENERATED_MIN_MAX_DEFAULTS

#ifdef GENERATED_FUNCTION_ARRAYS
GENERATED_FUNCTION_ARRAYS
#endif

#ifdef EMBER_AF_SUPPORT_COMMAND_DISCOVERY
const EmberAfCommandMetadata generatedCommands[] = GENERATED_COMMANDS;
#endif

const EmberAfAttributeMetadata generatedAttributes[] = GENERATED_ATTRIBUTES;
const EmberAfCluster generatedClusters[]          = GENERATED_CLUSTERS;
const EmberAfEndpointType generatedEmberAfEndpointTypes[]   = GENERATED_ENDPOINT_TYPES;
EmberAfNetwork emAfNetworks[] = EMBER_AF_GENERATED_NETWORKS;

const EmberAfManufacturerCodeEntry clusterManufacturerCodes[] = GENERATED_CLUSTER_MANUFACTURER_CODES;
const int16u clusterManufacturerCodeCount = GENERATED_CLUSTER_MANUFACTURER_CODE_COUNT;
const EmberAfManufacturerCodeEntry attributeManufacturerCodes[] = GENERATED_ATTRIBUTE_MANUFACTURER_CODES;
const int16u attributeManufacturerCodeCount = GENERATED_ATTRIBUTE_MANUFACTURER_CODE_COUNT;

//------------------------------------------------------------------------------
// Forward declarations

// Returns endpoint index within a given cluster
static int8u findClusterEndpointIndex(int8u endpoint, EmberAfClusterId clusterId, int8u mask);

//------------------------------------------------------------------------------

// Initial configuration
void emberAfEndpointConfigure(void) {
  int8u ep;
#ifdef FIXED_ENDPOINT_COUNT
  int8u fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;
  int16u fixedProfileIds[] = FIXED_PROFILE_IDS;
  int16u fixedDeviceIds[] = FIXED_DEVICE_IDS;
  int8u fixedDeviceVersions[] = FIXED_DEVICE_VERSIONS;
  int8u fixedEmberAfEndpointTypes[] = FIXED_ENDPOINT_TYPES;
  int8u fixedNetworks[] = FIXED_NETWORKS;
  emberEndpointCount = FIXED_ENDPOINT_COUNT;
  for ( ep = 0; ep < FIXED_ENDPOINT_COUNT; ep++ ) {
    emAfEndpoints[ep].endpoint      = fixedEndpoints[ep];
    emAfEndpoints[ep].profileId     = fixedProfileIds[ep];
    emAfEndpoints[ep].deviceId      = fixedDeviceIds[ep];
    emAfEndpoints[ep].deviceVersion = fixedDeviceVersions[ep];
    emAfEndpoints[ep].endpointType 
      = (EmberAfEndpointType*)&(generatedEmberAfEndpointTypes[fixedEmberAfEndpointTypes[ep]]);
    emAfEndpoints[ep].networkIndex  = fixedNetworks[ep];
    emAfEndpoints[ep].bitmask = EMBER_AF_ENDPOINT_ENABLED;
  }
#else
  for ( ep = 0; ep < MAX_ENDPOINT_COUNT; ep++ ) {
    emAfEndpoints[ep].endpoint      = endpointNumber(ep);
    emAfEndpoints[ep].profileId     = endpointProfileId(ep);
    emAfEndpoints[ep].deviceId      = endpointDeviceId(ep);
    emAfEndpoints[ep].deviceVersion = endpointDeviceVersion(ep);
    emAfEndpoints[ep].endpointType  = endpointType(ep);
    emAfEndpoints[ep].networkIndex  = endpointNetworkIndex(ep);
    emAfEndpoints[ep].bitmask       = EMBER_AF_ENDPOINT_ENABLED;
    if ( emAfEndpoints[ep].endpoint != 0xFF ) {
      emberEndpointCount++;
    } else {
      break;
    }
  }
#endif // FIXED_ENDPOINT_COUNT
}

int8u emberAfEndpointCount() 
{
  return emberEndpointCount;
}

boolean emberAfEndpointIndexIsEnabled(int8u index)
{
  return (emAfEndpoints[index].bitmask & EMBER_AF_ENDPOINT_ENABLED);
}

// some data types (like strings) are sent OTA in human readable order
// (how they are read) instead of little endian as the data types are.
boolean emberAfIsThisDataTypeAStringType(EmberAfAttributeType dataType)
{
  if ((dataType == ZCL_OCTET_STRING_ATTRIBUTE_TYPE) ||
      (dataType == ZCL_CHAR_STRING_ATTRIBUTE_TYPE) ||
      (dataType == ZCL_LONG_OCTET_STRING_ATTRIBUTE_TYPE) ||
      (dataType == ZCL_LONG_CHAR_STRING_ATTRIBUTE_TYPE))
    {
      return TRUE;
    }
  return FALSE;
}

boolean emberAfIsStringAttributeType(EmberAfAttributeType attributeType)
{
  return (attributeType == ZCL_OCTET_STRING_ATTRIBUTE_TYPE
          || attributeType == ZCL_CHAR_STRING_ATTRIBUTE_TYPE);
}

boolean emberAfIsLongStringAttributeType(EmberAfAttributeType attributeType)
{
  return (attributeType == ZCL_LONG_OCTET_STRING_ATTRIBUTE_TYPE
          || attributeType == ZCL_LONG_CHAR_STRING_ATTRIBUTE_TYPE);
}

// This function is used to call the per-cluster default response callback
void emberAfClusterDefaultResponseCallback(int8u endpoint,
                                           EmberAfClusterId clusterId,
                                           int8u commandId,
                                           EmberAfStatus status,
                                           int8u clientServerMask)
{
  EmberAfGenericClusterFunction f;
  EmberAfCluster *cluster = emberAfFindCluster(endpoint,
                                               clusterId,
                                               clientServerMask);
  if (cluster == NULL) return;
  f = emberAfFindClusterFunction(cluster,
                                 CLUSTER_MASK_DEFAULT_RESPONSE_FUNCTION);
  if (f == NULL) return;
  emberAfPushEndpointNetworkIndex(endpoint);
  ((EmberAfDefaultResponseFunction)f)(endpoint, commandId, status);
  emberAfPopNetworkIndex();
}

// This function is used to call the per-cluster message sent callback
void emberAfClusterMessageSentCallback(EmberOutgoingMessageType type,
                                       int16u indexOrDestination,
                                       EmberApsFrame *apsFrame,
                                       int16u msgLen,
                                       int8u *message,
                                       EmberStatus status)
{
  EmberAfGenericClusterFunction f;
  EmberAfCluster *cluster;
  if (apsFrame == NULL || message == NULL || msgLen == 0) return;
  cluster = emberAfFindCluster(apsFrame->sourceEndpoint,
                               apsFrame->clusterId,
                               (((message[0] & ZCL_FRAME_CONTROL_DIRECTION_MASK)
                                 == ZCL_FRAME_CONTROL_SERVER_TO_CLIENT)
                                ? CLUSTER_MASK_SERVER
                                : CLUSTER_MASK_CLIENT));
  if (cluster == NULL) return;
  f = emberAfFindClusterFunction(cluster, CLUSTER_MASK_MESSAGE_SENT_FUNCTION);
  if (f == NULL) return;
  emberAfPushEndpointNetworkIndex(apsFrame->sourceEndpoint);
  ((EmberAfMessageSentFunction)f)(type,
                                  indexOrDestination,
                                  apsFrame,
                                  msgLen,
                                  message,
                                  status);
  emberAfPopNetworkIndex();
}

// This function is used to call the per-cluster attribute changed callback
void emAfClusterAttributeChangedCallback(int8u endpoint,
                                         EmberAfClusterId clusterId,
                                         EmberAfAttributeId attributeId,
                                         int8u clientServerMask,
                                         int16u manufacturerCode)
{
  EmberAfGenericClusterFunction f;
  EmberAfCluster *cluster = emberAfFindCluster(endpoint,
                                               clusterId,
                                               clientServerMask);
  if (cluster == NULL) {
    return;
  }
  if (manufacturerCode != EMBER_AF_NULL_MANUFACTURER_CODE) {
    f = emberAfFindClusterFunction(cluster,
                                   CLUSTER_MASK_MANUFACTURER_SPECIFIC_ATTRIBUTE_CHANGED_FUNCTION);
    if (f == NULL) {
      return;
    }
    emberAfPushEndpointNetworkIndex(endpoint);
    ((EmberAfManufacturerSpecificClusterAttributeChangedCallback)f)(endpoint, 
      attributeId, manufacturerCode);
    emberAfPopNetworkIndex();
  } else {
    f = emberAfFindClusterFunction(cluster,
                                   CLUSTER_MASK_ATTRIBUTE_CHANGED_FUNCTION);
    if (f == NULL) {
      return;
    }
    emberAfPushEndpointNetworkIndex(endpoint);
    ((EmberAfClusterAttributeChangedCallback)f)(endpoint, attributeId);
    emberAfPopNetworkIndex();
  }
}

static void initializeEndpoint(EmberAfDefinedEndpoint* definedEndpoint)
{
  int8u clusterIndex;
  EmberAfEndpointType* epType = definedEndpoint->endpointType;
  emberAfPushEndpointNetworkIndex(definedEndpoint->endpoint);
  for ( clusterIndex = 0; 
        clusterIndex < epType->clusterCount; 
        clusterIndex ++ ) {
    EmberAfCluster *cluster = &(epType->cluster[clusterIndex]);
    EmberAfGenericClusterFunction f;
    emberAfClusterInitCallback(definedEndpoint->endpoint, cluster->clusterId);
    f = emberAfFindClusterFunction(cluster, CLUSTER_MASK_INIT_FUNCTION);
    if ( f != NULL ) {
      ((EmberAfInitFunction)f)(definedEndpoint->endpoint);
    }
  }
  emberAfPopNetworkIndex();
}

// Calls the init functions.
void emAfCallInits(void) 
{
  int8u index;
  for ( index = 0; index < emberAfEndpointCount(); index++ ) {
    initializeEndpoint(&(emAfEndpoints[index]));
  }
}

// Returns the pointer to metadata, or null if it is not found
EmberAfAttributeMetadata* emberAfLocateAttributeMetadata(int8u endpoint,
                                                         EmberAfClusterId clusterId,
                                                         EmberAfAttributeId attributeId,
                                                         int8u mask,
                                                         int16u manufacturerCode)
{
  EmberAfAttributeMetadata *metadata = NULL;
  EmberAfAttributeSearchRecord record;
  record.endpoint = endpoint;
  record.clusterId = clusterId;
  record.clusterMask = mask;
  record.attributeId = attributeId;
  record.manufacturerCode = manufacturerCode;
  emAfReadOrWriteAttribute(&record,
                           &metadata,
                           NULL,   // buffer
                           0,      // buffer size
                           FALSE); // write?
  return metadata;
}

static int8u* singletonAttributeLocation(EmberAfAttributeMetadata *am) 
{
  EmberAfAttributeMetadata *m = (EmberAfAttributeMetadata *)&(generatedAttributes[0]);
  int16u index = 0;
  while ( m < am ) {
    if ( m->mask & ATTRIBUTE_MASK_SINGLETON ) {
      index += m->size;
    }
    m++;
  }
  return (int8u *)(singletonAttributeData + index);
}


// This function does mem copy, but smartly, which means that if the type is a
// string, it will copy as much as it can.
// If src == NULL, then this method will set memory to zeroes
static EmberAfStatus typeSensitiveMemCopy(int8u* dest,
                                          int8u* src,
                                          EmberAfAttributeMetadata * am,
                                          boolean write,
                                          int16u readLength)
{
  EmberAfAttributeType attributeType = am->attributeType;
  int16u size = (readLength == 0) ? am->size : readLength;

  if (emberAfIsStringAttributeType(attributeType)) {
    emberAfCopyString(dest, src, size - 1);
  } else if (emberAfIsLongStringAttributeType(attributeType)) {
    emberAfCopyLongString(dest, src, size - 2);
  } else {
    if (!write && readLength != 0 && readLength < am->size) {
        return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
    }
    if ( src == NULL ) {
      MEMSET(dest, 0, size);
    } else {
      MEMCOPY(dest, src, size);
    }
  }
  return EMBER_ZCL_STATUS_SUCCESS;
}

// Returns the manufacturer code or ::EMBER_AF_NULL_MANUFACTURER_CODE if none
// could be found.
static int16u getManufacturerCode(EmberAfManufacturerCodeEntry *codes,
                                  int16u codeTableSize,
                                  int16u tableIndex)
{
  int16u i;
  for (i = 0; i < codeTableSize; i++, codes++) {
    if (codes->index == tableIndex) {
      return codes->manufacturerCode;
    }
  }
  return EMBER_AF_NULL_MANUFACTURER_CODE;
}

int16u emAfGetManufacturerCodeForAttribute(EmberAfCluster *cluster,
                                           EmberAfAttributeMetadata *attMetaData)
{
  return (emberAfClusterIsManufacturerSpecific(cluster)
          ? emAfGetManufacturerCodeForCluster(cluster)
          : getManufacturerCode((EmberAfManufacturerCodeEntry *)attributeManufacturerCodes,
                                attributeManufacturerCodeCount,
                                (attMetaData - generatedAttributes)));
}

int16u emAfGetManufacturerCodeForCluster(EmberAfCluster *cluster)
{
  return getManufacturerCode((EmberAfManufacturerCodeEntry *)clusterManufacturerCodes,
                             clusterManufacturerCodeCount,
                             (cluster - generatedClusters));
}

/**
 * @brief Matches a cluster based on cluster id, direction and manufacturer code.
 *   This function assumes that the passed cluster's endpoint already
 *   matches the endpoint of the EmberAfAttributeSearchRecord.
 *
 * Cluster's match if:
 *   1. Cluster ids match AND
 *   2. Cluster directions match as defined by cluster->mask 
 *        and attRecord->clusterMask AND
 *   3. If the clusters are mf specific, their mf codes match.
 */
boolean emAfMatchCluster(EmberAfCluster *cluster,
                         EmberAfAttributeSearchRecord *attRecord)
{
  return (cluster->clusterId == attRecord->clusterId
          && cluster->mask & attRecord->clusterMask
          && (!emberAfClusterIsManufacturerSpecific(cluster)
              || (emAfGetManufacturerCodeForCluster(cluster)
                  == attRecord->manufacturerCode)));
}

/**
 * @brief Matches an attribute based on attribute id and manufacturer code.
 *   This function assumes that the passed cluster already matches the 
 *   clusterId, direction and mf specificity of the passed 
 *   EmberAfAttributeSearchRecord.
 * 
 * Note: If both the attribute and cluster are manufacturer specific,
 *   the cluster's mf code gets precedence.
 * 
 * Attributes match if:
 *   1. Att ids match AND
 *      a. cluster IS mf specific OR
 *      b. both stored and saught attributes are NOT mf specific OR
 *      c. stored att IS mf specific AND mfg codes match.
 */
boolean emAfMatchAttribute(EmberAfCluster *cluster,
                           EmberAfAttributeMetadata *am,
                           EmberAfAttributeSearchRecord *attRecord)
{
  return (am->attributeId == attRecord->attributeId
          && (emberAfClusterIsManufacturerSpecific(cluster)
              || (emAfGetManufacturerCodeForAttribute(cluster, am)
                  == attRecord->manufacturerCode)));
}

// The callbacks to read and write externals have the same signature, so it is
// easy to use function pointers to call the right one.  This typedef covers
// both and makes the code a bit easier to read.
typedef EmberAfStatus (*ExternalReadWriteCallback)(int8u, EmberAfClusterId, EmberAfAttributeMetadata *, int16u, int8u *);

// When reading non-string attributes, this function returns an error when destination
// buffer isn't large enough to accommodate the attribute type.  For strings, the
// function will copy at most readLength bytes.  This means the resulting string
// may be truncated.  The length byte(s) in the resulting string will reflect
// any truncation.  If readLength is zero, we are working with backwards-
// compatibility wrapper functions and we just cross our fingers and hope for
// the best.
//
// When writing attributes, readLength is ignored.  For non-string attributes,
// this function assumes the source buffer is the same size as the attribute
// type.  For strings, the function will copy as many bytes as will fit in the
// attribute.  This means the resulting string may be truncated.  The length
// byte(s) in the resulting string will reflect any truncated.
EmberAfStatus emAfReadOrWriteAttribute(EmberAfAttributeSearchRecord *attRecord,
                                       EmberAfAttributeMetadata **metadata,
                                       int8u *buffer,
                                       int16u readLength,
                                       boolean write)
{
  int8u i;
  int16u attributeOffsetIndex = 0;

  for (i = 0; i < emberAfEndpointCount(); i++) {
    if (emAfEndpoints[i].endpoint == attRecord->endpoint) {
      EmberAfEndpointType *endpointType = emAfEndpoints[i].endpointType;
      int8u clusterIndex;
      if (!emberAfEndpointIndexIsEnabled(i)) {
        continue;
      }
      for (clusterIndex = 0;
           clusterIndex < endpointType->clusterCount;
           clusterIndex++) {
        EmberAfCluster *cluster = &(endpointType->cluster[clusterIndex]);
        if (emAfMatchCluster(cluster, attRecord)) { // Got the cluster
          int16u attrIndex;
          for (attrIndex = 0;
               attrIndex < cluster->attributeCount;
               attrIndex++) {
            EmberAfAttributeMetadata *am = &(cluster->attributes[attrIndex]);
            if (emAfMatchAttribute(cluster,
                                   am,
                                   attRecord)) { // Got the attribute

              // If passed metadata location is not null, populate
              if (metadata != NULL) {
                *metadata = am;
              }

              {
                int8u *attributeLocation = (am->mask & ATTRIBUTE_MASK_SINGLETON
                                            ? singletonAttributeLocation(am)
                                            : attributeData + attributeOffsetIndex);
                int8u *src, *dst;
                ExternalReadWriteCallback callback;
                if (write) {
                  src = buffer;
                  dst = attributeLocation;
                  callback = &emberAfExternalAttributeWriteCallback;
                } else {
                  if (buffer == NULL) {
                    return EMBER_ZCL_STATUS_SUCCESS;
                  }

                  src = attributeLocation;
                  dst = buffer;
                  callback = &emberAfExternalAttributeReadCallback;
                }

                return (am->mask & ATTRIBUTE_MASK_EXTERNAL_STORAGE
                        ? (*callback)(attRecord->endpoint,
                                      attRecord->clusterId,
                                      am,
                                      emAfGetManufacturerCodeForAttribute(cluster, am),
                                      buffer)
                        : typeSensitiveMemCopy(dst,
                                               src,
                                               am,
                                               write,
                                               readLength));
              }
            } else { // Not the attribute we are looking for
              // Increase the index if attribute is not externally stored
              if (!(am->mask & ATTRIBUTE_MASK_EXTERNAL_STORAGE)
                   && !(am->mask & ATTRIBUTE_MASK_SINGLETON) ) {
                attributeOffsetIndex += emberAfAttributeSize(am);
              }
            }
          }
        } else { // Not the cluster we are looking for
          attributeOffsetIndex += cluster->clusterSize;
        }
      }
    } else { // Not the endpoint we are looking for
      attributeOffsetIndex += emAfEndpoints[i].endpointType->endpointSize;
    }
  }
  return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE; // Sorry, attribute was not found.
}

// mask = 0 -> find either client or server
// mask = CLUSTER_MASK_CLIENT -> find client
// mask = CLUSTER_MASK_SERVER -> find server
static EmberAfCluster *emberAfFindClusterInType(EmberAfEndpointType *endpointType, 
                                                EmberAfClusterId clusterId,
                                                int8u mask)
{
  int8u i;
  for (i = 0; i < endpointType->clusterCount; i++) {
    EmberAfCluster *cluster = &(endpointType->cluster[i]);
    if (cluster->clusterId == clusterId
        && (mask == 0
            || (mask == CLUSTER_MASK_CLIENT && emberAfClusterIsClient(cluster))
            || (mask == CLUSTER_MASK_SERVER && emberAfClusterIsServer(cluster)))) {
      return cluster;
    }
  }
  return NULL;
}

int8u emberAfClusterIndex(int8u endpoint, 
                          EmberAfClusterId clusterId, 
                          int8u mask)
{
  int8u ep;
  int8u index = 0xFF;
  for ( ep=0; ep < emberAfEndpointCount(); ep++ ) {
    EmberAfEndpointType *endpointType = emAfEndpoints[ep].endpointType;
    if ( emberAfFindClusterInType(endpointType, clusterId, mask) != NULL ) {
      index++;
      if ( emAfEndpoints[ep].endpoint == endpoint ) 
        return index;
    }
  }
  return 0xFF;
}

// Returns TRUE If endpoint contains passed cluster
boolean emberAfContainsCluster(int8u endpoint, EmberAfClusterId clusterId) {
  return ( emberAfFindCluster(endpoint, clusterId, 0) != NULL );
}

boolean emberAfContainsServer(int8u endpoint, EmberAfClusterId clusterId) {
  return ( emberAfFindCluster(endpoint, clusterId, CLUSTER_MASK_SERVER) != NULL );
}

boolean emberAfContainsClient(int8u endpoint, EmberAfClusterId clusterId) {
  return ( emberAfFindCluster(endpoint, clusterId, CLUSTER_MASK_CLIENT) != NULL );
}

EmberAfCluster *emberAfFindCluster(int8u endpoint, 
                                   EmberAfClusterId clusterId, 
                                   int8u mask) {
  int8u ep = emberAfIndexFromEndpoint(endpoint);
  if ( ep == 0xFF ) 
    return NULL;
  else
    return emberAfFindClusterInType(emAfEndpoints[ep].endpointType, clusterId, mask);
}

// Server wrapper for findClusterEndpointIndex
int8u emberAfFindClusterServerEndpointIndex(int8u endpoint, EmberAfClusterId clusterId)
{
  return findClusterEndpointIndex(endpoint, clusterId, CLUSTER_MASK_SERVER);
}

// Client wrapper for findClusterEndpointIndex
int8u emberAfFindClusterClientEndpointIndex(int8u endpoint, EmberAfClusterId clusterId)
{
  return findClusterEndpointIndex(endpoint, clusterId, CLUSTER_MASK_CLIENT);
}

// Returns the endpoint index within a given cluster
static int8u findClusterEndpointIndex(int8u endpoint, EmberAfClusterId clusterId, int8u mask)
{
  int8u i, epi = 0;

  if (emberAfFindCluster(endpoint, clusterId, mask) == NULL) {
    return 0xFF;
  }

  for (i = 0; i < emberAfEndpointCount(); i++) {
    if (emAfEndpoints[i].endpoint == endpoint) {
      break;
    }
    epi += (emberAfFindCluster(emAfEndpoints[i].endpoint, clusterId, mask) != NULL) ? 1 : 0;
  }

  return epi;
}

static int8u findIndexFromEndpoint(int8u endpoint, boolean ignoreDisabledEndpoints)
{
  int8u epi;
  for (epi = 0; epi < emberAfEndpointCount(); epi++) {
    if (emAfEndpoints[epi].endpoint == endpoint
        && (!ignoreDisabledEndpoints 
            || emAfEndpoints[epi].bitmask & EMBER_AF_ENDPOINT_ENABLED)) {
      return epi;
    }
  }
  return 0xFF;
}

boolean emberAfEndpointIsEnabled(int8u endpoint)
{
  int8u index = findIndexFromEndpoint(endpoint,
                                      FALSE);    // ignore disabled endpoints?

  EMBER_TEST_ASSERT(0xFF != index);

  if (0xFF == index) {
    return FALSE;
  }

  return emberAfEndpointIndexIsEnabled(index);
}

boolean emberAfEndpointEnableDisable(int8u endpoint, boolean enable)
{
  int8u index = findIndexFromEndpoint(endpoint,
                                      FALSE);    // ignore disabled endpoints?
  boolean currentlyEnabled;

  if (0xFF == index) {
    return FALSE;
  }

  currentlyEnabled = emAfEndpoints[index].bitmask & EMBER_AF_ENDPOINT_ENABLED;
  
  if (enable) {
    emAfEndpoints[index].bitmask |= EMBER_AF_ENDPOINT_ENABLED;
  } else {
    emAfEndpoints[index].bitmask &= EMBER_AF_ENDPOINT_DISABLED;
  }

#if defined(EZSP_HOST)
  ezspSetEndpointFlags(endpoint, 
                       (enable
                        ? EZSP_ENDPOINT_ENABLED
                        : EZSP_ENDPOINT_DISABLED));
#endif

  if (currentlyEnabled ^ enable) {
    if (enable) {
      initializeEndpoint(&(emAfEndpoints[index]));
    } else {
      int8u i;
      for (i = 0; i < emAfEndpoints[index].endpointType->clusterCount; i++) {
        EmberAfCluster* cluster = &((emAfEndpoints[index].endpointType->cluster)[i]);
        EmberStatus status;
//        emberAfCorePrintln("Disabling cluster tick for ep:%d, cluster:0x%2X, %p",
//                           endpoint,
//                           cluster->clusterId,
//                           ((cluster->mask & CLUSTER_MASK_CLIENT)
//                            ? "client"
//                            : "server"));
//        emberAfCoreFlush();
        status = emberAfDeactivateClusterTick(endpoint, 
                                              cluster->clusterId,
                                              (cluster->mask & CLUSTER_MASK_CLIENT
                                               ? TRUE
                                               : FALSE));
      }
    }
  }
  
  return TRUE;
}

// Returns the index of a given endpoint.  Does not consider disabled endpoints.
int8u emberAfIndexFromEndpoint(int8u endpoint) 
{
  return findIndexFromEndpoint(endpoint, 
                               TRUE);    // ignore disabled endpoints?
}

int8u emberAfEndpointFromIndex(int8u index)
{
  return emAfEndpoints[index].endpoint;
}

// If server == true, returns the number of server clusters,
// otherwise number of client clusters on this endpoint
int8u emberAfClusterCount(int8u endpoint, boolean server) 
{
  int8u index = emberAfIndexFromEndpoint(endpoint);
  int8u i, c=0;
  EmberAfDefinedEndpoint *de;
  EmberAfCluster *cluster;

  if ( index == 0xFF ) {
    return 0;
  }
  de = &(emAfEndpoints[index]);
  if ( de->endpointType == NULL) {
    return 0;
  }
  for ( i=0; i<de->endpointType->clusterCount; i++ ) {
    cluster = &(de->endpointType->cluster[i]);
    if ( server && emberAfClusterIsServer(cluster) )
      c++;
    if ( (!server) && emberAfClusterIsClient(cluster) )
      c++;
  }
  return c;
}

// Returns the clusterId of Nth server or client cluster,
// depending on server toggle.
EmberAfCluster *emberAfGetNthCluster(int8u endpoint, int8u n, boolean server) 
{
  int8u index = emberAfIndexFromEndpoint(endpoint);
  EmberAfDefinedEndpoint *de;
  int8u i,c=0;
  EmberAfCluster *cluster;

  if ( index == 0xFF ) return NULL;
  de = &(emAfEndpoints[index]);
  
  for ( i = 0; i < de->endpointType->clusterCount; i++ ) {
    cluster = &(de->endpointType->cluster[i]);
    
    if ( ( server && emberAfClusterIsServer(cluster) )
         || ( (!server) &&  emberAfClusterIsClient(cluster) ) ) {
      if ( c == n ) return cluster;
      c++;
    }
    
  }
  return NULL;
}

// Returns number of clusters put into the passed cluster list
// for the given endpoint and client/server polarity
int8u emberAfGetClustersFromEndpoint(int8u endpoint, int16u *clusterList, int8u listLen, boolean server) {
  int8u clusterCount = emberAfClusterCount(endpoint, server);
  int8u i;
  EmberAfCluster *cluster;
  if (clusterCount > listLen) {
    clusterCount = listLen;
  }
  for (i = 0; i < clusterCount; i++) {
    cluster = emberAfGetNthCluster(endpoint, i, server);
    clusterList[i] = (cluster == NULL ? 0xFFFF : cluster->clusterId);
  }
  return clusterCount;
}


void emberAfLoadAttributesFromDefaults(int8u endpoint) 
{
  int8u ep, clusterI, curNetwork = emberGetCurrentNetwork();
  int16u attr;
  int8u *ptr;
  for ( ep = 0; ep < emberAfEndpointCount(); ep++ ) {
    EmberAfDefinedEndpoint *de;
    if (endpoint != EMBER_BROADCAST_ENDPOINT) {
      ep = emberAfIndexFromEndpoint(endpoint);
    } 
    de = &(emAfEndpoints[ep]);

    // Ensure that the endpoint is on the current network
    if (endpoint == EMBER_BROADCAST_ENDPOINT && de->networkIndex != curNetwork) {
      continue;
    }
    for ( clusterI = 0; clusterI < de->endpointType->clusterCount; clusterI++) {
      EmberAfCluster *cluster = &(de->endpointType->cluster[clusterI]);
      for ( attr = 0; attr < cluster->attributeCount; attr++) {
        EmberAfAttributeMetadata *am = &(cluster->attributes[attr]);
        if (!(am->mask & ATTRIBUTE_MASK_EXTERNAL_STORAGE)) {
          EmberAfAttributeSearchRecord record;
          record.endpoint = de->endpoint;
          record.clusterId = cluster->clusterId;
          record.clusterMask = (emberAfAttributeIsClient(am)
                                ? CLUSTER_MASK_CLIENT
                                : CLUSTER_MASK_SERVER);
          record.attributeId = am->attributeId;
          record.manufacturerCode = emAfGetManufacturerCodeForAttribute(cluster,
                                                                        am);
          if (am->mask & ATTRIBUTE_MASK_MIN_MAX) {
            if ( emberAfAttributeSize(am) <= 2 ) {
              ptr = (int8u*)&(am->defaultValue.ptrToMinMaxValue->defaultValue.defaultValue);
            } else {
              ptr = (int8u*)am->defaultValue.ptrToMinMaxValue->defaultValue.ptrToDefaultValue;
            }
          } else {
            if ( emberAfAttributeSize(am) <= 2 ) {
              ptr = (int8u*)&(am->defaultValue.defaultValue);
            } else {
              ptr = (int8u*)am->defaultValue.ptrToDefaultValue; 
            }
          }
          // At this point, ptr either points to a default value, or is NULL, in which case
          // it should be treated as if it is pointing to an array of all zeroes.

#if (BIGENDIAN_CPU)
          // The default value for one- and two-byte attributes is stored in an
          // int16u.  On big-endian platforms, a pointer to the default value of
          // a one-byte attribute will point to the wrong byte.  So, for those
          // cases, nudge the pointer forward so it points to the correct byte.
          if (emberAfAttributeSize(am) == 1 && ptr != NULL) {
            *ptr++;
          }
#endif //BIGENDIAN
          emAfReadOrWriteAttribute(&record,
                                   NULL,  // metadata
                                   ptr,
                                   0,     // buffer size - unused
                                   TRUE); // write?
        }
      }
    }
    if(endpoint != EMBER_BROADCAST_ENDPOINT) {
      break;
    }
  }
}

void emberAfLoadAttributesFromTokens(int8u endpoint) 
{
  // On EZSP host we currently do not support this. We need to come up with some 
  // callbacks.
#ifndef EZSP_HOST
  GENERATED_TOKEN_LOADER(endpoint);
#endif // EZSP_HOST
}

void emberAfSaveAttributeToToken(int8u *data,
                                 int8u endpoint, 
                                 int16u clusterId,
                                 EmberAfAttributeMetadata *metadata) 
{
  // Get out of here if this attribute doesn't have a token.
  if ( !emberAfAttributeIsTokenized(metadata)) return;

// On EZSP host we currently do not support this. We need to come up with some 
// callbacks.
#ifndef EZSP_HOST
  GENERATED_TOKEN_SAVER;
#endif // EZSP_HOST
}

// This function returns the actual function point from the array, 
// iterating over the function bits.
EmberAfGenericClusterFunction 
emberAfFindClusterFunction(EmberAfCluster *cluster,
                           EmberAfClusterMask functionMask) {
  EmberAfClusterMask mask = 0x01;
  int8u functionIndex = 0;

  if ( (cluster->mask & functionMask) == 0 )
    return NULL;
  
  while ( mask < functionMask ) {
    if ( (cluster->mask & mask) != 0 ) 
      functionIndex++;
    mask <<= 1;
  }
  return cluster->functions[functionIndex];
}

#ifdef EMBER_AF_SUPPORT_COMMAND_DISCOVERY
/**
 * This function populates command IDs into a given buffer.
 * 
 * It returns TRUE if commands are complete, meaning there are NO MORE
 * commands that would be returned after the last command. 
 * It returns FALSE, if there were more commands, but were not populated
 * because of maxIdCount limitation.
 */
boolean emberAfExtractCommandIds(boolean outgoing,
                                 EmberAfClusterCommand *cmd,
                                 int16u clusterId,
                                 int8u *buffer,
                                 int16u bufferLength,
                                 int16u *bufferIndex,
                                 int8u startId,
                                 int8u maxIdCount) {
  int16u i, count=0;
  boolean returnValue = TRUE;
  int8u cmdDirMask = 0;

  // determine the appropriate mask to match the request
  if (outgoing && (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER)) {
    cmdDirMask = COMMAND_MASK_OUTGOING_CLIENT;	
  } else if (outgoing && (cmd->direction == ZCL_DIRECTION_SERVER_TO_CLIENT)) {
    cmdDirMask = COMMAND_MASK_OUTGOING_SERVER;
  } else if (!outgoing && (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER)) {
    cmdDirMask = COMMAND_MASK_INCOMING_SERVER;
  } else {
    cmdDirMask = COMMAND_MASK_INCOMING_CLIENT;
  }
  
  for ( i = 0; i < EMBER_AF_GENERATED_COMMAND_COUNT; i++ ) {
    if ( generatedCommands[i].clusterId != clusterId ) 
      continue;
    
    if ((generatedCommands[i].mask & cmdDirMask) == 0 )
      continue;
    
    //Only start from the passed command id
    if (generatedCommands[i].commandId < startId)
      continue;
    
    // According to spec: if cmd->mfgSpecific is set, then we ONLY return the
    // mfg specific commands. If it's not, then we ONLY return non-mfg specific.
    if ( generatedCommands[i].mask & COMMAND_MASK_MANUFACTURER_SPECIFIC ) {
       // Command is Mfg specific
      if ( !cmd->mfgSpecific ) continue; // ignore if asking for not mfg specific
   } else {
      // Command is not mfg specific.
      if ( cmd->mfgSpecific ) continue; // Ignore if asking for mfg specific
    }

    // The one we are about to put in, is beyond the maxIdCount, 
    // so instead of populating it in, we set the return flag to 
    // false and get out of here.
    if ( maxIdCount == count || count >= bufferLength ) {
      returnValue = FALSE;
      break;
    }
    buffer[count] = generatedCommands[i].commandId;
    (*bufferIndex)++;
    count++;
  }
  return returnValue;
}
#else
// We just need an empty stub if we don't support it
boolean emberAfExtractCommandIds(boolean outgoing,
                                 EmberAfClusterCommand *cmd,
                                 int16u clusterId,
                                 int8u *buffer,
                                 int16u bufferLength,
                                 int16u *bufferIndex,
                                 int8u startId,
                                 int8u maxIdCount) {
  return TRUE;
}
#endif
