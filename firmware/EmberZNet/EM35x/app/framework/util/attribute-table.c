// *******************************************************************
// * attribute-table.c
// *
// * This file contains the code to manipulate the Smart Energy attribute
// * table.  This handles external calls to read/write the table, as
// * well as internal ones.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the zcl-util
#include "common.h"

#include "attribute-storage.h"

// for pulling in defines dealing with EITHER server or client
#include "af-main.h"

#include "enums.h"

//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// External Declarations

//------------------------------------------------------------------------------
// Forward Declarations

//------------------------------------------------------------------------------
// Globals


EmberAfStatus emberAfWriteAttributeExternal(int8u endpoint,
                                            EmberAfClusterId cluster,
                                            EmberAfAttributeId attributeID,
                                            int8u mask,
                                            int16u manufacturerCode,
                                            int8u* dataPtr,
                                            EmberAfAttributeType dataType)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_SUCCESS;
  EmberAfAttributeWritePermission extWritePermission
    = emberAfAllowNetworkWriteAttributeCallback(endpoint, 
                                                cluster,
                                                attributeID,
                                                mask,
                                                manufacturerCode,
                                                dataPtr,
                                                dataType);
  switch (extWritePermission){
  case EMBER_ZCL_ATTRIBUTE_WRITE_PERMISSION_DENY_WRITE:
    return EMBER_ZCL_STATUS_FAILURE;
    break;
  case EMBER_ZCL_ATTRIBUTE_WRITE_PERMISSION_ALLOW_WRITE_NORMAL:
  case EMBER_ZCL_ATTRIBUTE_WRITE_PERMISSION_ALLOW_WRITE_OF_READ_ONLY:
    return emAfWriteAttribute(endpoint,
                                cluster,
                                attributeID,
                                mask,
                                manufacturerCode,
                                dataPtr,
                                dataType,
                                (extWritePermission
                                == EMBER_ZCL_ATTRIBUTE_WRITE_PERMISSION_ALLOW_WRITE_OF_READ_ONLY),
                                FALSE);
    break;
  default:
    return (EmberAfStatus)extWritePermission;
    break;
  }
}

//@deprecated use emberAfWriteServerAttribute or emberAfWriteClientAttribute
EmberAfStatus emberAfWriteAttribute(int8u endpoint,
                                    EmberAfClusterId cluster,
                                    EmberAfAttributeId attributeID,
                                    int8u mask,
                                    int8u* dataPtr,
                                    EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            mask,
                            EMBER_AF_NULL_MANUFACTURER_CODE,
                            dataPtr,
                            dataType,
                            TRUE,   // override read-only?
                            FALSE); // just test?
}

EmberAfStatus emberAfWriteClientAttribute(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u* dataPtr,
                                          EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            CLUSTER_MASK_CLIENT,
                            EMBER_AF_NULL_MANUFACTURER_CODE,
                            dataPtr,
                            dataType,
                            TRUE,   // override read-only?
                            FALSE); // just test?
}

EmberAfStatus emberAfWriteServerAttribute(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u* dataPtr,
                                          EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            CLUSTER_MASK_SERVER,
                            EMBER_AF_NULL_MANUFACTURER_CODE,
                            dataPtr,
                            dataType,
                            TRUE,   // override read-only?
                            FALSE); // just test?
}

EmberAfStatus emberAfWriteManufacturerSpecificClientAttribute(int8u endpoint,
                                                              EmberAfClusterId cluster,
                                                              EmberAfAttributeId attributeID,
                                                              int16u manufacturerCode,
                                                              int8u* dataPtr,
                                                              EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            CLUSTER_MASK_CLIENT,
                            manufacturerCode,
                            dataPtr,
                            dataType,
                            TRUE,   // override read-only?
                            FALSE); // just test?
}

EmberAfStatus emberAfWriteManufacturerSpecificServerAttribute(int8u endpoint,
                                                              EmberAfClusterId cluster,
                                                              EmberAfAttributeId attributeID,
                                                              int16u manufacturerCode,
                                                              int8u* dataPtr,
                                                              EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            CLUSTER_MASK_SERVER,
                            manufacturerCode,
                            dataPtr,
                            dataType,
                            TRUE,   // override read-only?
                            FALSE); // just test?
}


EmberAfStatus emberAfVerifyAttributeWrite(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u mask,
                                          int16u manufacturerCode,
                                          int8u* dataPtr,
                                          EmberAfAttributeType dataType)
{
  return emAfWriteAttribute(endpoint,
                            cluster,
                            attributeID,
                            mask,
                            manufacturerCode,
                            dataPtr, 
                            dataType,
                            FALSE, // override read-only?
                            TRUE); // just test?
}

EmberAfStatus emberAfReadAttribute(int8u endpoint,
                                   EmberAfClusterId cluster,
                                   EmberAfAttributeId attributeID,
                                   int8u mask,
                                   int8u *dataPtr,
                                   int8u readLength,
                                   EmberAfAttributeType *dataType)
{
  return emAfReadAttribute(endpoint,
                           cluster,
                           attributeID,
                           mask,
                           EMBER_AF_NULL_MANUFACTURER_CODE,
                           dataPtr,
                           readLength,
                           dataType);
}


EmberAfStatus emberAfReadServerAttribute(int8u endpoint,
                                         EmberAfClusterId cluster,
                                         EmberAfAttributeId attributeID,
                                         int8u* dataPtr,
                                         int8u readLength)
{
  return emAfReadAttribute(endpoint,
                           cluster,
                           attributeID,
                           CLUSTER_MASK_SERVER,
                           EMBER_AF_NULL_MANUFACTURER_CODE,
                           dataPtr,
                           readLength,
                           NULL); 
}

EmberAfStatus emberAfReadClientAttribute(int8u endpoint,
                                         EmberAfClusterId cluster,
                                         EmberAfAttributeId attributeID,
                                         int8u* dataPtr,
                                         int8u readLength)
{
  return emAfReadAttribute(endpoint,
                           cluster,
                           attributeID,
                           CLUSTER_MASK_CLIENT,
                           EMBER_AF_NULL_MANUFACTURER_CODE,
                           dataPtr,
                           readLength,
                           NULL); 
}

EmberAfStatus emberAfReadManufacturerSpecificServerAttribute(int8u endpoint,
                                                             EmberAfClusterId cluster,
                                                             EmberAfAttributeId attributeID,
                                                             int16u manufacturerCode,
                                                             int8u* dataPtr,
                                                             int8u readLength)
{
  return emAfReadAttribute(endpoint,
                           cluster,
                           attributeID,
                           CLUSTER_MASK_SERVER,
                           manufacturerCode,
                           dataPtr,
                           readLength,
                           NULL);
}

EmberAfStatus emberAfReadManufacturerSpecificClientAttribute(int8u endpoint,
                                                             EmberAfClusterId cluster,
                                                             EmberAfAttributeId attributeID,
                                                             int16u manufacturerCode,
                                                             int8u* dataPtr,
                                                             int8u readLength)
{
  return emAfReadAttribute(endpoint,
                           cluster,
                           attributeID,
                           CLUSTER_MASK_CLIENT,
                           manufacturerCode,
                           dataPtr,
                           readLength,
                           NULL);                                     
}

boolean emberAfReadSequentialAttributesAddToResponse(int8u endpoint,
                                                     EmberAfClusterId clusterId,
                                                     EmberAfAttributeId startAttributeId,
                                                     int8u mask,
                                                     int16u manufacturerCode,
                                                     int8u maxAttributeIds,
													 boolean includeAccessControl)
{
  int16u i;
  int16u discovered = 0;
  int16u skipped = 0;
  int16u total = 0;

  EmberAfCluster *cluster = emberAfFindCluster(endpoint, clusterId, mask);

  EmberAfAttributeSearchRecord record;
  record.endpoint = endpoint;
  record.clusterId = clusterId;
  record.clusterMask = mask;
  record.attributeId = startAttributeId;
  record.manufacturerCode = manufacturerCode;
  
  // If we don't have the cluster or it doesn't match the search, we're done.
  if (cluster == NULL || !emAfMatchCluster(cluster, &record)) {
    return TRUE;
  }

  for (i = 0; i < cluster->attributeCount; i++) {
    EmberAfAttributeMetadata *metadata = &cluster->attributes[i];

    // If the cluster is not manufacturer-specific, an attribute is considered
    // only if its manufacturer code matches that of the command (which may be
    // unset).
    if (!emberAfClusterIsManufacturerSpecific(cluster)) {
      record.attributeId = metadata->attributeId;
      if (!emAfMatchAttribute(cluster, metadata, &record)) {
        continue;
      }
    }

    if (metadata->attributeId < startAttributeId) {
      skipped++;
    } else if (discovered < maxAttributeIds) {
      emberAfPutInt16uInResp(metadata->attributeId);
      emberAfPutInt8uInResp(metadata->attributeType);
      if (includeAccessControl) {
	    // Attribute access control field is little endian
	    // bit 0 : Readable <-- All our attributes are readable
	    // bit 1 : Writeable <-- The only thing we track in the attribute metadata mask
	    // bit 2 : Reportable <-- All our attributes are reportable
        emberAfPutInt8uInResp(((metadata->mask & ATTRIBUTE_MASK_WRITABLE) ?
			0xE0 : 0xA0));
      }
      discovered++;
    }
    total++;
  }

  // We are finished if there are no more attributes to find, which means the
  // number of attributes discovered plus the number skipped equals the total
  // attributes in the cluster.  For manufacturer-specific clusters, the total
  // includes all attributes in the cluster.  For standard ZCL clusters, if the
  // the manufacturer code is set, the total is the number of attributes that
  // match the manufacturer code.  Otherwise, the total is the number of
  // standard ZCL attributes in the cluster.
  return (discovered + skipped == total);
}

static void emberAfAttributeDecodeAndPrintCluster(EmberAfClusterId cluster)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_ATTRIBUTES)
  int16u index = emberAfFindClusterNameIndex(cluster);
  if (index != 0xFFFF) { 
    emberAfAttributesPrintln("(%p)", zclClusterNames[index].name);
  }
  emberAfAttributesFlush();
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_ATTRIBUTES)
}


void emberAfPrintAttributeTable(void) 
{
  int8u data[ATTRIBUTE_LARGEST];
  int8u endpointIndex, clusterIndex;
  int16u attributeIndex;
  EmberAfStatus status;
  int16u mfCode;
  for (endpointIndex = 0;
       endpointIndex < emberAfEndpointCount();
       endpointIndex++) {
    EmberAfDefinedEndpoint *ep = &(emAfEndpoints[endpointIndex]);
    emberAfAttributesPrintln("ENDPOINT %x", ep->endpoint);
    emberAfAttributesPrintln("indx  clus / attr /type(len)/ rw / t / [mfcode] / data (raw)");
    emberAfAttributesFlush();
    for (clusterIndex = 0;
         clusterIndex < ep->endpointType->clusterCount;
         clusterIndex++) {
      EmberAfCluster *cluster = &(ep->endpointType->cluster[clusterIndex]);
      for (attributeIndex = 0;
           attributeIndex < cluster->attributeCount;
           attributeIndex++) {
        EmberAfAttributeMetadata *metaData = &(cluster->attributes[attributeIndex]);
        emberAfAttributesPrint("%2x: %2x / %2x / ",
                               attributeIndex,
                               cluster->clusterId,
                               metaData->attributeId);
        emberAfAttributesPrint("%x (%x) / %p / %p /",
                               metaData->attributeType,
                               emberAfAttributeSize(metaData),
                               (emberAfAttributeIsReadOnly(metaData) ? "RO": "RW"),
                               (emberAfAttributeIsTokenized(metaData) ? "Y": "N"));
        emberAfAttributesFlush();
        mfCode = emAfGetManufacturerCodeForAttribute(cluster, metaData);
        if (mfCode != EMBER_AF_NULL_MANUFACTURER_CODE) {
          emberAfAttributesPrint(" %2x /", mfCode);
        }
        status = emAfReadAttribute(ep->endpoint,
                                   cluster->clusterId,
                                   metaData->attributeId,
                                   (emberAfAttributeIsClient(metaData)
                                     ? CLUSTER_MASK_CLIENT
                                     : CLUSTER_MASK_SERVER),
                                   mfCode,
                                   data,
                                   ATTRIBUTE_LARGEST,
                                   NULL);
        if (status == EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE) {
          emberAfAttributesPrintln("Unsupported");
        } else {
          emberAfAttributesPrintBuffer(data,
                                       emberAfAttributeSize(metaData),
                                       TRUE);
          emberAfAttributesFlush();
          emberAfAttributeDecodeAndPrintCluster(cluster->clusterId);
        }
      }
    }
    emberAfAttributesFlush();
  }
}

// given a clusterId and an attribute to read, this crafts the response
// and places it in the response buffer. Response is one of two items:
// 1) unsupported: [attrId:2] [status:1]
// 2) supported:   [attrId:2] [status:1] [type:1] [data:n]
//
void emberAfRetrieveAttributeAndCraftResponse(int8u endpoint,
                                              EmberAfClusterId clusterId,
                                              EmberAfAttributeId attrId,
                                              int8u mask,
                                              int16u manufacturerCode,
                                              int16u readLength)
{
  EmberAfStatus status;
  int8u data[ATTRIBUTE_LARGEST];
  int8u dataType;
  int8u dataLen;

  // account for at least one byte of data
  if (readLength < 5) {
    return;
  }

  emberAfAttributesPrintln("OTA READ: ep:%x cid:%2x attid:%2x msk:%x mfcode:%2x", 
                           endpoint, 
                           clusterId, attrId, mask, manufacturerCode);

  // lookup the attribute in our table
  status = emAfReadAttribute(endpoint,
                             clusterId,
                             attrId,
                             mask,
                             manufacturerCode,
                             data,
                             ATTRIBUTE_LARGEST,
                             &dataType);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    dataLen = (emberAfIsThisDataTypeAStringType(dataType)
               ? emberAfStringLength(data) + 1
               : emberAfGetDataSize(dataType));
    if (readLength < (4 + dataLen)) { // Not enough space for attribute.
      return;
    }
  } else {
    emberAfPutInt16uInResp(attrId);
    emberAfPutInt8uInResp(status);
    emberAfAttributesPrintln("READ: clus %2x, attr %2x failed %x",
                             clusterId,
                             attrId,
                             status);
    emberAfAttributesFlush();
    return;
  }
  
  // put attribute in least sig byte first
  emberAfPutInt16uInResp(attrId);

  // attribute is found, so copy in the status and the data type 
  emberAfPutInt8uInResp(EMBER_ZCL_STATUS_SUCCESS);
  emberAfPutInt8uInResp(dataType);

  if ((appResponseLength + dataLen) < EMBER_AF_RESPONSE_BUFFER_LEN) {
#if (BIGENDIAN_CPU)     
    // strings go over the air as length byte and then in human
    // readable format. These should not be flipped. Other attributes
    // need to be flipped so they go little endian OTA
    if (isThisDataTypeSentLittleEndianOTA(dataType)) {
      int8u i;
      for (i = 0; i<dataLen; i++) {
        appResponseData[appResponseLength + i] = data[dataLen - i - 1];
      }
    } else {
      MEMCOPY(&(appResponseData[appResponseLength]), data, dataLen);
    }
#else //(BIGENDIAN_CPU)
    MEMCOPY(&(appResponseData[appResponseLength]), data, dataLen);
#endif //(BIGENDIAN_CPU)
    appResponseLength += dataLen;
  }
  
  emberAfAttributesPrintln("READ: clus %2x, attr %2x, dataLen: %x, OK",
                           clusterId,
                           attrId,
                           dataLen);
  emberAfAttributesFlush();
}

// This function appends the attribute report fields for the given endpoint,
// cluster, and attribute to the buffer starting at the index.  If there is
// insufficient space in the buffer or an error occurs, buffer and bufIndex will
// remain unchanged.  Otherwise, bufIndex will be incremented appropriately and
// the fields will be written to the buffer.
EmberAfStatus emberAfAppendAttributeReportFields(int8u endpoint,
                                                 EmberAfClusterId clusterId,
                                                 EmberAfAttributeId attributeId,
                                                 int8u mask,
                                                 int8u *buffer,
                                                 int8u bufLen,
                                                 int8u *bufIndex)
{
  EmberAfStatus status;
  EmberAfAttributeType type;
  int8u size;
  int8u data[ATTRIBUTE_LARGEST];

  status = emberAfReadAttribute(endpoint,
                                clusterId,
                                attributeId,
                                mask,
                                data,
                                sizeof(data),
                                &type);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    goto kickout;
  }

  size = (emberAfIsThisDataTypeAStringType(type)
          ? emberAfStringLength(data) + 1
          : emberAfGetDataSize(type));
  if (bufLen < *bufIndex + size + 3) {
    status = EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
    goto kickout;
  }

  buffer[(*bufIndex)++] = LOW_BYTE(attributeId);
  buffer[(*bufIndex)++] = HIGH_BYTE(attributeId);
  buffer[(*bufIndex)++] = type;
#if (BIGENDIAN_CPU)
  if (isThisDataTypeSentLittleEndianOTA(type)) {
    emberReverseMemCopy(buffer + *bufIndex, data, size);
  } else {
    MEMCOPY(buffer + *bufIndex, data, size);
  }
#else
  MEMCOPY(buffer + *bufIndex, data, size);
#endif
  *bufIndex += size;

kickout:
  emberAfAttributesPrintln("REPORT: clus 0x%2x, attr 0x%2x: 0x%x",
                           clusterId,
                           attributeId,
                           status);
  emberAfAttributesFlush();

  return status;
}

//------------------------------------------------------------------------------
// Internal Functions

// writes an attribute (identified by clusterID and attrID to the given value. 
// this returns:
// - EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE: if attribute isnt supported by the device (the
//           device is not found in the attribute table)
// - EMBER_ZCL_STATUS_INVALID_DATA_TYPE: if the data type passed in doesnt match the type
//           stored in the attribute table
// - EMBER_ZCL_STATUS_READ_ONLY: if the attribute isnt writable
// - EMBER_ZCL_STATUS_INVALID_VALUE: if the value is set out of the allowable range for
//           the attribute
// - EMBER_ZCL_STATUS_SUCCESS: if the attribute was found and successfully written
// 
// if TRUE is passed in for overrideReadOnlyAndDataType then the data type is
// not checked and the read-only flag is ignored. This mode is meant for 
// testing or setting the initial value of the attribute on the device.
//
// if TRUE is passed for justTest, then the type is not written but all 
// checks are done to see if the type could be written
// reads the attribute specified, returns FALSE if the attribute is not in
// the table or the data is too large, returns TRUE and writes to dataPtr
// if the attribute is supported and the readLength specified is less than
// the length of the data.
EmberAfStatus emAfWriteAttribute(int8u endpoint,
                                 EmberAfClusterId cluster,
                                 EmberAfAttributeId attributeID,
                                 int8u mask,
                                 int16u manufacturerCode,
                                 int8u *data,
                                 EmberAfAttributeType dataType,
                                 boolean overrideReadOnlyAndDataType,
                                 boolean justTest)
{
  EmberAfAttributeMetadata *metadata = NULL;
  EmberAfAttributeSearchRecord record;
  record.endpoint = endpoint;
  record.clusterId = cluster;
  record.clusterMask = mask;
  record.attributeId = attributeID;
  record.manufacturerCode = manufacturerCode;
  emAfReadOrWriteAttribute(&record,
                           &metadata,
                           NULL,   // buffer
                           0,      // buffer size
                           FALSE); // write?

  // if we dont support that attribute
  if (metadata == NULL) {
    emberAfAttributesPrintln("%pep %x clus %2x attr %2x not supported",
                             "WRITE ERR: ",
                             endpoint, 
                             cluster, 
                             attributeID);
    emberAfAttributesFlush();
    return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
  }

  // if the data type specified by the caller is incorrect
  if (!(overrideReadOnlyAndDataType)) {
    if (dataType != metadata->attributeType) {
      emberAfAttributesPrintln("%pinvalid data type", "WRITE ERR: ");
      emberAfAttributesFlush();
      return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
    }
    
    if (emberAfAttributeIsReadOnly(metadata)) {
      emberAfAttributesPrintln("%pattr not writable", "WRITE ERR: ");
      emberAfAttributesFlush();
      return EMBER_ZCL_STATUS_READ_ONLY;
    }
  }

  // if the value the attribute is being set to is out of range
  // return EMBER_ZCL_STATUS_INVALID_VALUE
  if (metadata->mask & ATTRIBUTE_MASK_MIN_MAX) {
    EmberAfDefaultAttributeValue minv = metadata->defaultValue.ptrToMinMaxValue->minValue;
    EmberAfDefaultAttributeValue maxv = metadata->defaultValue.ptrToMinMaxValue->maxValue;
    int8u dataLen = emberAfAttributeSize(metadata);
    if (dataLen <= 2) {
      int8s minR, maxR;
      int8u* minI = (int8u*)&(minv.defaultValue);
      int8u* maxI = (int8u*)&(maxv.defaultValue);
      //On big endian cpu with length 1 only the second byte counts
      #if (BIGENDIAN_CPU)
        if (dataLen == 1) {
          minI++;
          maxI++;
        }
      #endif //BIGENDIAN_CPU
      minR = emberAfCompareValues(minI, data, dataLen);
      maxR = emberAfCompareValues(maxI, data, dataLen);
      if ((minR == 1) || (maxR == -1)) {
        return EMBER_ZCL_STATUS_INVALID_VALUE;
      }
    } else {
      if ((emberAfCompareValues(minv.ptrToDefaultValue, data, dataLen) == 1) ||
          (emberAfCompareValues(maxv.ptrToDefaultValue, data, dataLen) == -1)) {
        return EMBER_ZCL_STATUS_INVALID_VALUE;
      }
    }
  }

  // write the data unless this is only a test
  if (!justTest) {
    EmberAfStatus status;
    // Pre write attribute callback
    emberAfPreAttributeChangeCallback(endpoint,
                                      cluster,
                                      attributeID,
                                      mask,
                                      manufacturerCode,
                                      dataType,
                                      emberAfAttributeSize(metadata),
                                      data );

    // write the attribute
    status = emAfReadOrWriteAttribute(&record,
                                      NULL,    // metadata
                                      data,
                                      0,       // buffer size - unused
                                      TRUE);   // write?

    if (status != EMBER_ZCL_STATUS_SUCCESS){
        return status;
    }

    // Save the attribute to token if needed
    // Function itself will weed out tokens that are not tokenized.
    emberAfSaveAttributeToToken(data, endpoint, cluster, metadata);


    emberAfReportingAttributeChangeCallback(endpoint,
                                            cluster,
                                            attributeID,
                                            mask,
                                            manufacturerCode,
                                            dataType,
                                            data);

    // Post write attribute callback
    emberAfPostAttributeChangeCallback(endpoint,
                                       cluster,
                                       attributeID,
                                       mask,
                                       manufacturerCode,
                                       dataType,
                                       emberAfAttributeSize(metadata),
                                       data);

    emAfClusterAttributeChangedCallback(endpoint,
                                        cluster,
                                        attributeID,
                                        mask,
                                        manufacturerCode);
  } else {
    // bug: 11618, we are not handling properly external attributes
    // in this case... We need to do something. We don't really
    // know if it will succeed.
    emberAfAttributesPrintln("WRITE: no write, just a test");
    emberAfAttributesFlush();
  }

  return EMBER_ZCL_STATUS_SUCCESS;
}

// If dataPtr is NULL, no data is copied to the caller.  
// readLength should be 0 in that case.

EmberAfStatus emAfReadAttribute(int8u endpoint,
                                EmberAfClusterId cluster,
                                EmberAfAttributeId attributeID,
                                int8u mask,
                                int16u manufacturerCode,
                                int8u *dataPtr,
                                int16u readLength,
                                EmberAfAttributeType *dataType)
{
  EmberAfAttributeMetadata *metadata = NULL;
  EmberAfAttributeSearchRecord record;
  EmberAfStatus status;
  record.endpoint = endpoint;
  record.clusterId = cluster;
  record.clusterMask = mask;
  record.attributeId = attributeID;
  record.manufacturerCode = manufacturerCode;
  status = emAfReadOrWriteAttribute(&record,
                                    &metadata,
                                    dataPtr,
                                    readLength,
                                    FALSE); // write?

  if (status == EMBER_ZCL_STATUS_SUCCESS){
    // It worked!  If the user asked for the type, set it before returning.
    if (dataType != NULL) {
      (*dataType) = metadata->attributeType;
    }
  } else { // failed, print debug info
    if (status == EMBER_ZCL_STATUS_INSUFFICIENT_SPACE){
      emberAfAttributesPrintln("READ: attribute size too large for caller");
      emberAfAttributesFlush();
    }
  }

  return status;
}
