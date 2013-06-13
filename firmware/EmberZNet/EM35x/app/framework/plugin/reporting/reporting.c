// *******************************************************************
// * reporting.c
// *
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/util/attribute-storage.h"
#include "reporting.h"
#include "reporting-callback.h"

#ifdef ATTRIBUTE_LARGEST
#define READ_DATA_SIZE ATTRIBUTE_LARGEST
#else
#define READ_DATA_SIZE 8 // max size if attributes aren't present
#endif

#define NULL_INDEX 0xFF

static void conditionallySendReport(int8u endpoint, EmberAfClusterId clusterId);
static void scheduleTick(void);
static void removeConfiguration(int8u index);
static void removeConfigurationAndScheduleTick(int8u index);
static EmberAfStatus configureReceivedAttribute(const EmberAfClusterCommand *cmd,
                                                EmberAfAttributeId attributeId,
                                                int8u mask,
                                                int16u timeout);
static void putReportableChangeInResp(const EmberAfPluginReportingEntry *entry,
                                      EmberAfAttributeType dataType);

EmberEventControl emberAfPluginReportingTickEventControl;

typedef struct {
  int32u lastReportTime;
  int32u lastReportValue;
  boolean reportableChange;
} EmAfPluginReportingVolatileData;
static EmAfPluginReportingVolatileData volatileData[EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE];

#ifdef EZSP_HOST
static EmberAfPluginReportingEntry table[EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE];
void emAfPluginReportingGetEntry(int8u index, EmberAfPluginReportingEntry *result)
{
  MEMCOPY(result, &table[index], sizeof(EmberAfPluginReportingEntry));
}
void emAfPluginReportingSetEntry(int8u index, EmberAfPluginReportingEntry *value)
{
  MEMCOPY(&table[index], value, sizeof(EmberAfPluginReportingEntry));
}
#else
void emAfPluginReportingGetEntry(int8u index, EmberAfPluginReportingEntry *result)
{
  halCommonGetIndexedToken(result, TOKEN_REPORT_TABLE, index);
}
void emAfPluginReportingSetEntry(int8u index, EmberAfPluginReportingEntry *value)
{
  halCommonSetIndexedToken(TOKEN_REPORT_TABLE, index, value);
}
#endif

void emberAfPluginReportingInitCallback(void)
{
  scheduleTick();
}

void emberAfPluginReportingTickEventHandler(void)
{
  EmberApsFrame *apsFrame = NULL;
  EmberAfStatus status;
  EmberAfAttributeType dataType;
  int32u currentTime = halCommonGetInt32uMillisecondTick();
  int16u manufacturerCode;
  int8u readData[READ_DATA_SIZE];
  int8u i, dataSize;
  boolean clientToServer;

  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    EmberAfPluginReportingEntry entry;
    emAfPluginReportingGetEntry(i, &entry);

    // We will only send reports for active reported attributes and only if a
    // reportable change has occurred and the minimum interval has elapsed or
    // if the maximum interval is set and has elapsed.
    if (entry.endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID
        || entry.direction != EMBER_ZCL_REPORTING_DIRECTION_REPORTED
        || (currentTime - volatileData[i].lastReportTime
            < entry.data.reported.minInterval * MILLISECOND_TICKS_PER_SECOND)
        || (!volatileData[i].reportableChange
            && (currentTime - volatileData[i].lastReportTime
                < (entry.data.reported.maxInterval
                   * MILLISECOND_TICKS_PER_SECOND)))) {
      continue;
    }

    status = emAfReadAttribute(entry.endpoint,
                               entry.clusterId,
                               entry.attributeId,
                               entry.mask,
                               entry.manufacturerCode,
                               (int8u *)&readData,
                               READ_DATA_SIZE,
                               &dataType);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfReportingPrintln("ERR: reading cluster 0x%2x attribute 0x%2x: 0x%x",
                              entry.clusterId,
                              entry.attributeId,
                              status);
      continue;
    }

    // If we have already started a report for a different attribute or
    // destination, send it and create a new one.
    if (apsFrame != NULL &&
        !(entry.endpoint == apsFrame->sourceEndpoint
          && entry.clusterId == apsFrame->clusterId
          && emberAfClusterIsClient(&entry) == clientToServer
          && entry.manufacturerCode == manufacturerCode)) {
      conditionallySendReport(apsFrame->sourceEndpoint, apsFrame->clusterId);
      apsFrame = NULL;
    }

    // If we haven't made the message header, make it.
    if (apsFrame == NULL) {
      apsFrame = emberAfGetCommandApsFrame();
      clientToServer = emberAfClusterIsClient(&entry);
      // The manufacturer-specfic version of the fill API only creates a
      // manufacturer-specfic command if the manufacturer code is set.  For
      // non-manufacturer-specfic reports, the manufacturer code is unset, so
      // we can get away with using this API for both cases.
      emberAfFillExternalManufacturerSpecificBuffer((clientToServer
                                                     ? (ZCL_PROFILE_WIDE_COMMAND
                                                        | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                                                        | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS)
                                                     : (ZCL_PROFILE_WIDE_COMMAND
                                                        | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                                                        | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS)),
                                                    entry.clusterId,
                                                    entry.manufacturerCode,
                                                    ZCL_REPORT_ATTRIBUTES_COMMAND_ID,
                                                    "");
      apsFrame->sourceEndpoint = entry.endpoint;
      apsFrame->options = EMBER_AF_DEFAULT_APS_OPTIONS;
      manufacturerCode = entry.manufacturerCode;
    }

    // Payload is [attribute id:2] [type:1] [data:N].
    emberAfPutInt16uInResp(entry.attributeId);
    emberAfPutInt8uInResp(dataType);

    dataSize = (emberAfIsThisDataTypeAStringType(dataType)
                ? emberAfStringLength(readData) + 1
                : emberAfGetDataSize(dataType));
#if (BIGENDIAN_CPU)
    if (isThisDataTypeSentLittleEndianOTA(dataType)) {
      int8u i;
      for (i = 0; i < dataSize; i++) {
        emberAfPutInt8uInResp(readData[dataSize - i - 1]);
      }
    } else {
      emberAfPutBlockInResp(readData, dataSize);
    }
#else
    emberAfPutBlockInResp(readData, dataSize);
#endif

    // Store the last reported time and value so that we can track intervals
    // and changes.  We only track changes for data types that are small enough
    // for us to compare.
    volatileData[i].reportableChange = FALSE;
    volatileData[i].lastReportTime = currentTime;
    if (dataSize <= sizeof(volatileData[i].lastReportValue)) {
      volatileData[i].lastReportValue = 0;
#if (BIGENDIAN_CPU)
      MEMCOPY(((int8u *)&volatileData[i].lastReportValue
               + sizeof(volatileData[i].lastReportValue)
               - dataSize),
              readData,
              dataSize);
#else
      MEMCOPY(&volatileData[i].lastReportValue, readData, dataSize);
#endif
    }
  }

  if (apsFrame != NULL) {
    conditionallySendReport(apsFrame->sourceEndpoint, apsFrame->clusterId);
  }
  scheduleTick();
}

static void conditionallySendReport(int8u endpoint, EmberAfClusterId clusterId)
{
  if (emberAfIsDeviceEnabled(endpoint)
      || clusterId == ZCL_IDENTIFY_CLUSTER_ID) {
    emberAfSendCommandUnicastToBindings();
  }
}

boolean emberAfConfigureReportingCommandCallback(const EmberAfClusterCommand *cmd)
{
  int16u bufIndex = cmd->payloadStartIndex;
  int8u frameControl, mask;
  boolean failures = FALSE;

  emberAfReportingPrint("%p: ", "CFG_RPT");
  emberAfReportingDebugExec(emberAfDecodeAndPrintCluster(cmd->apsFrame->clusterId));
  emberAfReportingPrintln("");
  emberAfReportingFlush();

  if (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER) {
    frameControl = (ZCL_PROFILE_WIDE_COMMAND
                    | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                    | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS);
    mask = CLUSTER_MASK_SERVER;
  } else {
    frameControl = (ZCL_PROFILE_WIDE_COMMAND
                    | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                    | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS);
    mask = CLUSTER_MASK_CLIENT;
  }

  // The manufacturer-specfic version of the fill API only creates a
  // manufacturer-specfic command if the manufacturer code is set.  For non-
  // manufacturer-specfic reports, the manufacturer code is unset, so we can
  // get away with using this API for both cases.
  emberAfFillExternalManufacturerSpecificBuffer(frameControl, 
                                                cmd->apsFrame->clusterId,
                                                cmd->mfgCode,
                                                ZCL_CONFIGURE_REPORTING_RESPONSE_COMMAND_ID,
                                                "");

  // Each record in the command has at least a one-byte direction and a two-
  // byte attribute id.  Additional fields are present depending on the value
  // of the direction field.
  while (bufIndex + 3 < cmd->bufLen) {
    EmberAfAttributeId attributeId;
    EmberAfReportingDirection direction;
    EmberAfStatus status;

    direction = emberAfGetInt8u(cmd->buffer, bufIndex, cmd->bufLen);
    bufIndex++;
    attributeId = emberAfGetInt16u(cmd->buffer, bufIndex, cmd->bufLen);
    bufIndex += 2;

    emberAfReportingPrintln(" - direction:%x, attr:%2x", direction, attributeId);

    switch (direction) {
    case EMBER_ZCL_REPORTING_DIRECTION_REPORTED:
      {
        EmberAfAttributeType dataType;
        int16u minInterval, maxInterval;
        int32u reportableChange = 0;

        dataType = emberAfGetInt8u(cmd->buffer, bufIndex, cmd->bufLen);
        bufIndex++;
        minInterval = emberAfGetInt16u(cmd->buffer, bufIndex, cmd->bufLen);
        bufIndex += 2;
        maxInterval = emberAfGetInt16u(cmd->buffer, bufIndex, cmd->bufLen);
        bufIndex += 2;

        emberAfReportingPrintln("   type:%x, min:%2x, max:%2x",
                                dataType,
                                minInterval,
                                maxInterval);
        emberAfReportingFlush();

        if (emberAfGetAttributeAnalogOrDiscreteType(dataType)
            == EMBER_AF_DATA_TYPE_ANALOG) {
          int8u dataSize = emberAfGetDataSize(dataType);
          reportableChange = emberAfGetInt(cmd->buffer,
                                           bufIndex,
                                           cmd->bufLen,
                                           dataSize);

          emberAfReportingPrint("   change:");
          emberAfReportingPrintBuffer(cmd->buffer + bufIndex, dataSize, FALSE);
          emberAfReportingPrintln("");

          bufIndex += dataSize;
        }

        // Add a reporting entry for a reported attribute.  The reports will
        // be sent from us to the source of the Configure Reporting command.
        status = emAfPluginReportingConfigureReportedAttribute(cmd,
                                                               attributeId,
                                                               mask,
                                                               dataType,
                                                               minInterval,
                                                               maxInterval,
                                                               reportableChange);
        break;
      }
    case EMBER_ZCL_REPORTING_DIRECTION_RECEIVED:
      {
        int16u timeout = emberAfGetInt16u(cmd->buffer, bufIndex, cmd->bufLen);
        bufIndex += 2;

        emberAfReportingPrintln("   timeout:%2x", timeout);

        // Add a reporting entry from a received attribute.  The reports
        // will be sent to us from the source of the Configure Reporting
        // command.
        status = configureReceivedAttribute(cmd,
                                            attributeId,
                                            mask,
                                            timeout);
        break;
      }
    default:
      // This will abort the processing (see below).
      status = EMBER_ZCL_STATUS_INVALID_FIELD;
      break;
    }

    // If a report cannot be configured, the status, direction, and
    // attribute are added to the response.  If the failure was due to an
    // invalid field, we have to abort after this record because we don't
    // know how to interpret the rest of the data in the request.
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
      emberAfPutInt8uInResp(status);
      emberAfPutInt8uInResp(direction);
      emberAfPutInt16uInResp(attributeId);
      failures = TRUE;
      if (status == EMBER_ZCL_STATUS_INVALID_FIELD) {
        break;
      }
    }
  }

  // We just respond with SUCCESS if we made it through without failures.
  if (!failures) {
    emberAfPutInt8uInResp(EMBER_ZCL_STATUS_SUCCESS);
  }

  emberAfSendResponse();
  return TRUE;
}

boolean emberAfReadReportingConfigurationCommandCallback(const EmberAfClusterCommand *cmd)
{
  int16u bufIndex = cmd->payloadStartIndex;
  int8u frameControl, mask;

  emberAfReportingPrint("%p: ", "READ_RPT_CFG");
  emberAfReportingDebugExec(emberAfDecodeAndPrintCluster(cmd->apsFrame->clusterId));
  emberAfReportingPrintln("");
  emberAfReportingFlush();

  if (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER) {
    frameControl = (ZCL_PROFILE_WIDE_COMMAND
                    | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                    | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS);
    mask = CLUSTER_MASK_SERVER;
  } else {
    frameControl = (ZCL_PROFILE_WIDE_COMMAND
                    | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
                    | EMBER_AF_DEFAULT_RESPONSE_POLICY_REQUESTS);
    mask = CLUSTER_MASK_CLIENT;
  }

  // The manufacturer-specfic version of the fill API only creates a
  // manufacturer-specfic command if the manufacturer code is set.  For non-
  // manufacturer-specfic reports, the manufacturer code is unset, so we can
  // get away with using this API for both cases.
  emberAfFillExternalManufacturerSpecificBuffer(frameControl,
                                                cmd->apsFrame->clusterId,
                                                cmd->mfgCode,
                                                ZCL_READ_REPORTING_CONFIGURATION_RESPONSE_COMMAND_ID,
                                                "");

  // Each record in the command has a one-byte direction and a two-byte
  // attribute id.
  while (bufIndex + 3 <= cmd->bufLen) {
    EmberAfAttributeId attributeId;
    EmberAfAttributeMetadata *metadata;
    EmberAfPluginReportingEntry entry;
    EmberAfReportingDirection direction;
    int8u i;
    boolean found = FALSE;

    direction = emberAfGetInt8u(cmd->buffer, bufIndex, cmd->bufLen);
    bufIndex++;
    attributeId = emberAfGetInt16u(cmd->buffer, bufIndex, cmd->bufLen);
    bufIndex += 2;

    switch (direction) {
    case EMBER_ZCL_REPORTING_DIRECTION_REPORTED:
      metadata = emberAfLocateAttributeMetadata(cmd->apsFrame->destinationEndpoint,
                                                cmd->apsFrame->clusterId,
                                                attributeId,
                                                mask,
                                                cmd->mfgCode);
      if (metadata == NULL) {
        emberAfPutInt8uInResp(EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
        emberAfPutInt8uInResp(direction);
        emberAfPutInt16uInResp(attributeId);
        continue;
      }
      break;
    case EMBER_ZCL_REPORTING_DIRECTION_RECEIVED:
      break;
    default:
      emberAfPutInt8uInResp(EMBER_ZCL_STATUS_INVALID_FIELD);
      emberAfPutInt8uInResp(direction);
      emberAfPutInt16uInResp(attributeId);
      continue;
    }

    // 075123r03 seems to suggest that SUCCESS is returned even if reporting
    // isn't configured for the requested attribute.  The individual fields
    // of the response for this attribute get populated with defaults.
    for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
      emAfPluginReportingGetEntry(i, &entry);
      if (entry.direction == direction
          && entry.endpoint == cmd->apsFrame->destinationEndpoint
          && entry.clusterId == cmd->apsFrame->clusterId
          && entry.attributeId == attributeId
          && entry.mask == mask
          && entry.manufacturerCode == cmd->mfgCode
          && (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED
              || (entry.data.received.source == cmd->source
                  && entry.data.received.endpoint == cmd->apsFrame->sourceEndpoint))) {
        found = TRUE;
        break;
      }
    }
    emberAfPutInt8uInResp(EMBER_ZCL_STATUS_SUCCESS);
    emberAfPutInt8uInResp(direction);
    emberAfPutInt16uInResp(attributeId);
    switch (direction) {
      case EMBER_ZCL_REPORTING_DIRECTION_REPORTED:
        emberAfPutInt8uInResp(metadata->attributeType);
        emberAfPutInt16uInResp(found ? entry.data.reported.minInterval : 0xFFFF);
        emberAfPutInt16uInResp(found ? entry.data.reported.maxInterval : 0xFFFF);
        if (emberAfGetAttributeAnalogOrDiscreteType(metadata->attributeType)
            == EMBER_AF_DATA_TYPE_ANALOG) {
          putReportableChangeInResp(found ? &entry : NULL, metadata->attributeType);
        }
        break;
      case EMBER_ZCL_REPORTING_DIRECTION_RECEIVED:
        emberAfPutInt16uInResp(found ? entry.data.received.timeout : 0xFFFF);
        break;
    }
  }

  emberAfSendResponse();
  return TRUE;
}

EmberStatus emberAfClearReportTableCallback(void)
{
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    removeConfiguration(i);
  }
  emberEventControlSetInactive(emberAfPluginReportingTickEventControl);
  return EMBER_SUCCESS;
}

EmberStatus emAfPluginReportingRemoveEntry(int8u index)
{
  EmberStatus status = EMBER_INDEX_OUT_OF_RANGE;
  if (index < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE) {
    removeConfigurationAndScheduleTick(index);
    status = EMBER_SUCCESS;
  }
  return status;
}

void emberAfReportingAttributeChangeCallback(int8u endpoint,
                                             EmberAfClusterId clusterId,
                                             EmberAfAttributeId attributeId,
                                             int8u mask,
                                             int16u manufacturerCode,
                                             EmberAfAttributeType type,
                                             int8u *data)
{
  int32u currentTime = halCommonGetInt32uMillisecondTick();
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    EmberAfPluginReportingEntry entry;
    emAfPluginReportingGetEntry(i, &entry);
    if (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED
        && entry.endpoint == endpoint
        && entry.clusterId == clusterId
        && entry.attributeId == attributeId
        && entry.mask == mask
        && entry.manufacturerCode == manufacturerCode) {
      // If we are reporting this particular attribute, we only care whether
      // the new value meets the reportable change criteria.  If it does, we
      // mark the entry as ready to report and reschedule the tick.  Whether
      // the tick will be scheduled for immediate or delayed execution depends
      // on the minimum reporting interval.  This is handled in the scheduler.
      int32u difference = emberAfGetDifference(data,
                                               volatileData[i].lastReportValue,
                                               emberAfGetDataSize(type));
      int8u analogOrDiscrete = emberAfGetAttributeAnalogOrDiscreteType(type);
      if ((analogOrDiscrete == EMBER_AF_DATA_TYPE_DISCRETE && difference != 0)
          || (analogOrDiscrete == EMBER_AF_DATA_TYPE_ANALOG
              && entry.data.reported.reportableChange <= difference)) {
        volatileData[i].reportableChange = TRUE;
        scheduleTick();
      }
      break;
    }
  }
}

static void scheduleTick(void)
{
  int32u delay = MAX_INT32U_VALUE;
  int32u currentTime = halCommonGetInt32uMillisecondTick();
  int8u i;
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    EmberAfPluginReportingEntry entry;
    emAfPluginReportingGetEntry(i, &entry);
    if (entry.endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID
        && entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED) {
      int32u minInterval = (entry.data.reported.minInterval
                            * MILLISECOND_TICKS_PER_SECOND);
      int32u elapsed = currentTime - volatileData[i].lastReportTime;
      int32u remaining = MAX_INT32U_VALUE;
      if (volatileData[i].reportableChange) {
        remaining = (minInterval < elapsed ? 0 : minInterval - elapsed);
      } else if (entry.data.reported.maxInterval != 0x0000) {
        remaining = (entry.data.reported.maxInterval
                     * MILLISECOND_TICKS_PER_SECOND
                     - elapsed);
      }
      if (remaining < delay) {
        delay = remaining;
      }
    }
  }
  if (delay != MAX_INT32U_VALUE) {
    emberAfDebugPrintln("sched report event for: 0x%4x", delay);
    emberAfEventControlSetDelay(&emberAfPluginReportingTickEventControl, delay);
  } else {
    emberAfDebugPrintln("deactivate report event");
    emberEventControlSetInactive(emberAfPluginReportingTickEventControl);
  }
}

static void removeConfiguration(int8u index)
{
  EmberAfPluginReportingEntry entry;
  emAfPluginReportingGetEntry(index, &entry);
  entry.endpoint = EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID;
  emAfPluginReportingSetEntry(index, &entry);
  emberAfPluginReportingConfiguredCallback(&entry);
}

static void removeConfigurationAndScheduleTick(int8u index)
{
  removeConfiguration(index);
  scheduleTick();
}

EmberAfStatus emAfPluginReportingConfigureReportedAttribute(const EmberAfClusterCommand *cmd,
                                                            EmberAfAttributeId attributeId,
                                                            int8u mask,
                                                            EmberAfAttributeType dataType,
                                                            int16u minInterval,
                                                            int16u maxInterval,
                                                            int32u reportableChange)
{
  EmberAfAttributeMetadata *metadata;
  EmberAfPluginReportingEntry entry;
  EmberAfStatus status;
  int8u i, index = NULL_INDEX;
  boolean initialize = TRUE;


  // Verify that we support the attribute and that the data type matches.
  metadata = emberAfLocateAttributeMetadata(cmd->apsFrame->destinationEndpoint,
                                            cmd->apsFrame->clusterId,
                                            attributeId,
                                            mask,
                                            cmd->mfgCode);
  if (metadata == NULL) {
    return EMBER_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
  } else if (dataType != metadata->attributeType) {
    return EMBER_ZCL_STATUS_INVALID_DATA_TYPE;
  }

  // Verify the minimum and maximum intervals make sense.
  if (maxInterval != 0 && maxInterval < minInterval) {
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }

  // Check the table for an entry that matches this request and also watch for
  // empty slots along the way.  If a report exists, it will be overwritten
  // with the new configuration.  Otherwise, a new entry will be created and
  // initialized.
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    emAfPluginReportingGetEntry(i, &entry);
    if (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED
        && entry.endpoint == cmd->apsFrame->destinationEndpoint
        && entry.clusterId == cmd->apsFrame->clusterId
        && entry.attributeId == attributeId
        && entry.mask == mask
        && entry.manufacturerCode == cmd->mfgCode) {
      initialize = FALSE;
      index = i;
      break;
    } else if (entry.endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID
               && index == NULL_INDEX) {
      index = i;
    }
  }

  // If the maximum reporting interval is 0xFFFF, the device shall not issue
  // reports for the attribute and the configuration information for that
  // attribute need not be maintained.
  if (maxInterval == 0xFFFF) {
    if (!initialize) {
      removeConfigurationAndScheduleTick(index);
    }
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  if (index == NULL_INDEX) {
    return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
  } else if (initialize) {
    entry.direction = EMBER_ZCL_REPORTING_DIRECTION_REPORTED;
    entry.endpoint = cmd->apsFrame->destinationEndpoint;
    entry.clusterId = cmd->apsFrame->clusterId;
    entry.attributeId = attributeId;
    entry.mask = mask;
    entry.manufacturerCode = cmd->mfgCode;
    volatileData[index].lastReportTime = halCommonGetInt32uMillisecondTick();
    volatileData[index].lastReportValue = 0;
  }

  // For new or updated entries, set the intervals and reportable change.
  // Updated entries will retain all other settings configured previously.
  entry.data.reported.minInterval = minInterval;
  entry.data.reported.maxInterval = maxInterval;
  entry.data.reported.reportableChange = reportableChange;

  // Give the application a chance to review the configuration that we have
  // been building up.  If the application rejects it, we just do not save the
  // record.  If we were supposed to add a new configuration, it will not be
  // created.  If we were supposed to update an existing configuration, we will
  // keep the old one and just discard any changes.  So, in either case, life
  // continues unchanged if the application rejects the configuration.
  status = emberAfPluginReportingConfiguredCallback(&entry);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emAfPluginReportingSetEntry(index, &entry);
    scheduleTick();
  }
  return status;
}

static EmberAfStatus configureReceivedAttribute(const EmberAfClusterCommand *cmd,
                                                EmberAfAttributeId attributeId,
                                                int8u mask,
                                                int16u timeout)
{
  EmberAfPluginReportingEntry entry;
  EmberAfStatus status;
  int8u i, index = NULL_INDEX;
  boolean initialize = TRUE;

  // Check the table for an entry that matches this request and also watch for
  // empty slots along the way.  If a report exists, it will be overwritten
  // with the new configuration.  Otherwise, a new entry will be created and
  // initialized.
  for (i = 0; i < EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE; i++) {
    emAfPluginReportingGetEntry(i, &entry);
    if (entry.direction == EMBER_ZCL_REPORTING_DIRECTION_RECEIVED
        && entry.endpoint == cmd->apsFrame->destinationEndpoint
        && entry.clusterId == cmd->apsFrame->clusterId
        && entry.attributeId == attributeId
        && entry.mask == mask
        && entry.manufacturerCode == cmd->mfgCode
        && entry.data.received.source == cmd->source
        && entry.data.received.endpoint == cmd->apsFrame->sourceEndpoint) {
      initialize = FALSE;
      index = i;
      break;
    } else if (entry.endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID
               && index == NULL_INDEX) {
      index = i;
    }
  }

  if (index == NULL_INDEX) {
    return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
  } else if (initialize) {
    entry.direction = EMBER_ZCL_REPORTING_DIRECTION_RECEIVED;
    entry.endpoint = cmd->apsFrame->destinationEndpoint;
    entry.clusterId = cmd->apsFrame->clusterId;
    entry.attributeId = attributeId;
    entry.mask = mask;
    entry.manufacturerCode = cmd->mfgCode;
    entry.data.received.source = cmd->source;
    entry.data.received.endpoint = cmd->apsFrame->sourceEndpoint;
  }

  // For new or updated entries, set the timeout.  Updated entries will retain
  // all other settings configured previously.
  entry.data.received.timeout = timeout;

  // Give the application a chance to review the configuration that we have
  // been building up.  If the application rejects it, we just do not save the
  // record.  If we were supposed to add a new configuration, it will not be
  // created.  If we were supposed to update an existing configuration, we will
  // keep the old one and just discard any changes.  So, in either case, life
  // continues unchanged if the application rejects the configuration.  If the
  // application accepts the change, the tick does not have to be rescheduled
  // here because we don't do anything with received reports.
  status = emberAfPluginReportingConfiguredCallback(&entry);
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    emAfPluginReportingSetEntry(index, &entry);
  }
  return status;
}

static void putReportableChangeInResp(const EmberAfPluginReportingEntry *entry,
                                      EmberAfAttributeType dataType)
{
  int8u bytes = emberAfGetDataSize(dataType);
  if (entry == NULL) { // default, 0xFF...UL or 0x80...L
    for (; bytes > 0; bytes--) {
      int8u b = 0xFF;
      if (emberAfIsTypeSigned(dataType)) {
        b = (bytes == 1 ? 0x80 : 0x00);
      }
      emberAfPutInt8uInResp(b);
    }
  } else { // reportable change value
    int32u value = entry->data.reported.reportableChange;
    for (; bytes > 0; bytes--) {
      int8u b = BYTE_0(value);
      emberAfPutInt8uInResp(b);
      value >>= 8;
    }
  }
}
