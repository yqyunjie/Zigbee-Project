// *******************************************************************
// * attribute-table.h
// *
// * This file contains the definitions for the attribute table, its
// * sizes, count, and API.
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef ZCL_UTIL_ATTRIBUTE_TABLE_H
#define ZCL_UTIL_ATTRIBUTE_TABLE_H

#include "../include/af.h"

#define ZCL_NULL_ATTRIBUTE_TABLE_INDEX 0xFFFF

// Remote devices writing attributes of local device
EmberAfStatus emberAfWriteAttributeExternal(int8u endpoint,
                                            EmberAfClusterId cluster,
                                            EmberAfAttributeId attributeID,
                                            int8u mask,
                                            int16u manufacturerCode,
                                            int8u* dataPtr,
                                            EmberAfAttributeType dataType);


void emberAfRetrieveAttributeAndCraftResponse(int8u endpoint,
                                              EmberAfClusterId clusterId,
                                              EmberAfAttributeId attrId,
                                              int8u mask,
                                              int16u manufacturerCode,
                                              int16u maxLength);
EmberAfStatus emberAfAppendAttributeReportFields(int8u endpoint,
                                                 EmberAfClusterId clusterId,
                                                 EmberAfAttributeId attributeId,
                                                 int8u mask,
                                                 int8u *buffer,
                                                 int8u bufLen,
                                                 int8u *bufIndex);
void emberAfPrintAttributeTable(void);

boolean emberAfReadSequentialAttributesAddToResponse(int8u endpoint,
                                                     EmberAfClusterId clusterId,
                                                     EmberAfAttributeId startAttributeId,
                                                     int8u mask,
                                                     int16u manufacturerCode,
                                                     int8u maxAttributeIds,
													 boolean includeAccessControl);

EmberAfStatus emAfWriteAttribute(int8u endpoint,
                                 EmberAfClusterId cluster,
                                 EmberAfAttributeId attributeID,
                                 int8u mask,
                                 int16u manufacturerCode,
                                 int8u* data,
                                 EmberAfAttributeType dataType,
                                 boolean overrideReadOnlyAndDataType,
                                 boolean justTest);

EmberAfStatus emAfReadAttribute(int8u endpoint,
                                EmberAfClusterId cluster,
                                EmberAfAttributeId attributeID,
                                int8u mask,
                                int16u manufacturerCode,
                                int8u* dataPtr,
                                int16u maxLength,
                                int8u* dataType);

#endif // ZCL_UTIL_ATTRIBUTE_TABLE_H
