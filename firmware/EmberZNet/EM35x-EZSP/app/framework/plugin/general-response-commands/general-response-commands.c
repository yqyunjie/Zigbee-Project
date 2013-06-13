// *******************************************************************
// * general-response-commands.c
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/util.h"

boolean emberAfReadAttributesResponseCallback(EmberAfClusterId clusterId, 
                                              int8u *buffer, 
                                              int16u bufLen)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEBUG)
  int16u bufIndex = 0;
  emberAfDebugPrint("%p_RESP: ", "READ_ATTR");
  emberAfDebugDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfDebugPrintln("");
  emberAfDebugFlush();

  // Each record in the response has a two-byte attribute id and a one-byte
  // status.  If the status is SUCCESS, there will also be a one-byte type and
  // variable-length data.
  while (bufIndex + 3 <= bufLen) {
    EmberAfAttributeId attributeId;
    EmberAfStatus status;
    attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                       bufIndex,
                                                       bufLen);
    bufIndex += 2;
    status = (EmberAfStatus)emberAfGetInt8u(buffer, bufIndex, bufLen);
    bufIndex++;
    emberAfDebugPrintln(" - attr:%2x, status:%x", attributeId, status);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
      int8u dataType, dataSize;
      dataType = emberAfGetInt8u(buffer, bufIndex, bufLen);
      bufIndex++;

      // For strings, the data size is the length of the string (specified by
      // the first byte of data) plus one for the length byte itself.  For
      // everything else, the size is just the size of the data type.
      dataSize = (emberAfIsThisDataTypeAStringType(dataType)
                  ? emberAfStringLength(buffer + bufIndex) + 1
                  : emberAfGetDataSize(dataType));

      emberAfDebugPrint("   type:%x, val:", dataType);
      if (bufIndex + dataSize <= bufLen) {
        if (emberAfIsThisDataTypeAStringType(dataType)) {
          emberAfDebugPrintString(buffer + bufIndex);
        } else {
          emberAfDebugPrintBuffer(buffer + bufIndex, dataSize, FALSE);
        }
      }
      emberAfDebugPrintln("");
      bufIndex += dataSize;
    }
    emberAfDebugFlush();
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_DEBUG
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfWriteAttributesResponseCallback(EmberAfClusterId clusterId, 
                                               int8u *buffer, 
                                               int16u bufLen)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEBUG)
  int16u bufIndex = 0;
  emberAfDebugPrint("%p_RESP: ", "WRITE_ATTR");
  emberAfDebugDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfDebugPrintln("");
  emberAfDebugFlush();

  // Each record in the response has a one-byte status.  If the status is not
  // SUCCESS, the record will also contain a two-byte attribute id.
  while (bufIndex + 1 <= bufLen) {
    EmberAfStatus status = (EmberAfStatus)emberAfGetInt8u(buffer,
                                                          bufIndex,
                                                          bufLen);
    bufIndex++;
    emberAfDebugPrintln(" - status:%x", status);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      EmberAfAttributeId attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                                            bufIndex,
                                                                            bufLen);
      bufIndex += 2;
      emberAfDebugPrintln("   attr:%2x", attributeId);
    }
    emberAfDebugFlush();
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_DEBUG
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfConfigureReportingResponseCallback(EmberAfClusterId clusterId, 
                                                  int8u *buffer, 
                                                  int16u bufLen)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_REPORTING)
  int16u bufIndex = 0;
  emberAfReportingPrint("%p_RESP: ", "CFG_RPT");
  emberAfReportingDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfReportingPrintln("");
  emberAfReportingFlush();

  // Each record in the response has a one-byte status.  If the status is not
  // SUCCESS, the record will also contain a one-byte direction and a two-byte
  // attribute id.
  while (bufIndex + 1 <= bufLen) {
    EmberAfStatus status = (EmberAfStatus)emberAfGetInt8u(buffer,
                                                          bufIndex,
                                                          bufLen);
    bufIndex++;
    emberAfReportingPrintln(" - status:%x", status);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      EmberAfReportingDirection direction;
      EmberAfAttributeId attributeId;
      direction =  (EmberAfReportingDirection)emberAfGetInt8u(buffer,
                                                              bufIndex,
                                                              bufLen);
      bufIndex++;
      attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                         bufIndex,
                                                         bufLen);
      bufIndex += 2;
      emberAfReportingPrintln("   direction:%x, attr:%2x",
                              direction,
                              attributeId);
    }
    emberAfReportingFlush();
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_REPORTING
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfReadReportingConfigurationResponseCallback(EmberAfClusterId clusterId, 
                                                          int8u *buffer, 
                                                          int16u bufLen)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_REPORTING)
  int16u bufIndex = 0;
  emberAfReportingPrint("%p_RESP: ", "READ_RPT_CFG");
  emberAfReportingDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfReportingPrintln("");
  emberAfReportingFlush();

  // Each record in the response has a one-byte status, a one-byte direction,
  // and a two-byte attribute id.  If the status is SUCCESS, the record will
  // contain additional fields.
  while (bufIndex + 4 <= bufLen) {
    EmberAfAttributeId attributeId;
    EmberAfStatus status;
    EmberAfReportingDirection direction;
    status = (EmberAfStatus)emberAfGetInt8u(buffer, bufIndex, bufLen);
    bufIndex++;
    direction = (EmberAfReportingDirection)emberAfGetInt8u(buffer,
                                                           bufIndex,
                                                           bufLen);
    bufIndex++;
    attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                       bufIndex,
                                                       bufLen);
    bufIndex += 2;
    emberAfReportingPrintln(" - status:%x, direction:%x, attr:%2x",
                            status,
                            direction,
                            attributeId);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
      // If the direction indicates the attribute is reported, the record will
      // contain a one-byte type and two two-byte intervals.  If the type is
      // analog, the record will contain a reportable change of the same data
      // type.  If the direction indicates reports of the attribute are
      // received, the record will contain a two-byte timeout.
      switch (direction) {
      case EMBER_ZCL_REPORTING_DIRECTION_REPORTED:
        {
          int16u minInterval, maxInterval;
          int8u dataType;
          dataType = emberAfGetInt8u(buffer, bufIndex, bufLen);
          bufIndex++;
          minInterval = emberAfGetInt16u(buffer, bufIndex, bufLen);
          bufIndex += 2;
          maxInterval = emberAfGetInt16u(buffer, bufIndex, bufLen);
          bufIndex += 2;
          emberAfReportingPrintln("   type:%x, min:%2x, max:%2x",
                                  dataType,
                                  minInterval,
                                  maxInterval);
          if (emberAfGetAttributeAnalogOrDiscreteType(dataType)
              == EMBER_AF_DATA_TYPE_ANALOG) {
            int8u dataSize = emberAfGetDataSize(dataType);
            emberAfReportingPrint("   change:");
            emberAfReportingPrintBuffer(buffer + bufIndex, dataSize, FALSE);
            emberAfReportingPrintln("");
            bufIndex += dataSize;
          }
          break;
        }
      case EMBER_ZCL_REPORTING_DIRECTION_RECEIVED:
        {
          int16u timeout = emberAfGetInt16u(buffer, bufIndex, bufLen);
          bufIndex += 2;
          emberAfReportingPrintln("   timeout:%2x", timeout);
          break;
        }
      default:
        emberAfReportingPrintln("ERR: unknown direction %x", direction);
        emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_INVALID_FIELD);
        return TRUE;
      }
    }
    emberAfReportingFlush();
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_REPORTING
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfReportAttributesCallback(EmberAfClusterId clusterId, 
                                        int8u *buffer, 
                                        int16u bufLen)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_REPORTING)
  int16u bufIndex = 0;
  emberAfReportingPrint("RPT_ATTR: ");
  emberAfReportingDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfReportingPrintln("");

  // Each record in the response has a two-byte attribute id, a one-byte
  // type, and variable-length data.
  while (bufIndex + 3 < bufLen) {
    EmberAfAttributeId attributeId;
    int8u dataType, dataSize;
    attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                       bufIndex,
                                                       bufLen);
    bufIndex += 2;
    dataType = emberAfGetInt8u(buffer, bufIndex, bufLen);
    bufIndex++;

    // For strings, the data size is the length of the string (specified by
    // the first byte of data) plus one for the length byte itself.  For
    // everything else, the size is just the size of the data type.
    dataSize = (emberAfIsThisDataTypeAStringType(dataType)
                ? emberAfStringLength(buffer + bufIndex) + 1
                : emberAfGetDataSize(dataType));

    emberAfReportingPrintln(" - attr:%2x", attributeId);
    emberAfReportingPrint("   type:%x, val:", dataType);
    if (emberAfIsThisDataTypeAStringType(dataType)) {
      emberAfReportingPrintString(buffer + bufIndex);
    } else {
      emberAfReportingPrintBuffer(buffer + bufIndex, dataSize, FALSE);
    }
    emberAfReportingPrintln("");
    emberAfReportingFlush();
    bufIndex += dataSize;
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_REPORTING
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfDefaultResponseCallback(EmberAfClusterId clusterId, 
                                       int8u commandId, 
                                       EmberAfStatus status)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEBUG)
  emberAfDebugPrint("%p_RESP: ", "DEFAULT");
  emberAfDebugDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfDebugPrintln(" cmd %x status %x", commandId, status);
  emberAfDebugFlush();
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_DEBUG
  return TRUE;
}

boolean emberAfDiscoverAttributesResponseCallback(EmberAfClusterId clusterId, 
                                                  boolean discoveryComplete, 
                                                  int8u *buffer, 
                                                  int16u bufLen,
												  boolean extended)
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEBUG)
  int16u bufIndex = 0;
  emberAfDebugPrint("%p%p_RESP: ", "DISC_ATTR", (extended ? "_EXT" : ""));
  emberAfDebugDebugExec(emberAfDecodeAndPrintCluster(clusterId));
  emberAfDebugPrintln(" comp %pDONE", discoveryComplete ? "" : "NOT_");
  emberAfDebugFlush();

  // Each record in the response has a two-byte attribute id and a one-byte
  // type.
  while (bufIndex + 3 <= bufLen) {
    EmberAfAttributeId attributeId;
    int8u dataType;
	int8u accessControl;
    attributeId = (EmberAfAttributeId)emberAfGetInt16u(buffer,
                                                       bufIndex,
                                                       bufLen);
    bufIndex += 2;
    dataType = emberAfGetInt8u(buffer, bufIndex, bufLen);
    bufIndex++;
    if (extended) {
	  accessControl = emberAfGetInt8u(buffer, bufIndex, bufLen);
	  bufIndex++;
	  emberAfDebugPrintln(" - attr:%2x, type:%x ac:%x", attributeId, dataType, accessControl);
    } else {
      emberAfDebugPrintln(" - attr:%2x, type:%x", attributeId, dataType);
    }
    emberAfDebugFlush();
  }
#endif //EMBER_AF_PRINT_ENABLE && EMBER_AF_PRINT_DEBUG
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}
