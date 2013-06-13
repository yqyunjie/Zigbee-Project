// *******************************************************************
// * time-server.c
// *
// * The Time server plugin is responsible for keeping track of the current
// * time.  All endpoints that implement the Time cluster server should use a
// * singleton time attribute.  Sleepy devices should not use this plugin as it
// * will prevent the device from sleeping for longer than one second.
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "time-server.h"

#ifdef ZCL_USING_TIME_CLUSTER_SERVER
#define INVALID_ENDPOINT 0xFF

static EmberAfStatus readTime(int8u endpoint, int32u *time);
static EmberAfStatus writeTime(int8u endpoint, int32u time);

static int8u singleton = INVALID_ENDPOINT;

#define emAfContainsTimeServerAttribute(endpoint,attribute) \
  emberAfContainsAttribute((endpoint),ZCL_TIME_CLUSTER_ID,(attribute),CLUSTER_MASK_SERVER,EMBER_AF_NULL_MANUFACTURER_CODE)

void emberAfTimeClusterServerInitCallback(int8u endpoint)
{
  EmberAfAttributeMetadata *metadata;
  EmberAfStatus status;
  int32u currentTime;
  int8u timeStatus = 0;

  metadata = emberAfLocateAttributeMetadata(endpoint,
                                            ZCL_TIME_CLUSTER_ID,
                                            ZCL_TIME_ATTRIBUTE_ID,
                                            CLUSTER_MASK_SERVER,
                                            EMBER_AF_NULL_MANUFACTURER_CODE);
  if (emberAfAttributeIsSingleton(metadata)) {
    if (singleton != INVALID_ENDPOINT) {
      return;
    }
    singleton = endpoint;
  }

  // Initialize the attribute with the real time, if it's available.
  currentTime = emberAfGetCurrentTimeCallback();
  if (currentTime != 0) {
    writeTime(endpoint, currentTime);
  }

#ifdef EMBER_AF_PLUGIN_TIME_SERVER_MASTER
  // The first bit of TimeStatus indicates whether the real time clock
  // corresponding to the Time attribute is internally set to the time
  // standard.
  timeStatus |= BIT(0);
#endif //EMBER_AF_PLUGIN_TIME_SERVER_MASTER

#ifdef EMBER_AF_PLUGIN_TIME_SERVER_MASTER_ZONE_DST
  // The third bit of TimeStatus indicates whether the TimeZone, DstStart,
  // DstEnd, and DstShift attributes are set internally to correct values for
  // the location of the clock.
  if (emAfContainsTimeServerAttribute(endpoint, ZCL_TIME_ZONE_ATTRIBUTE_ID)
      && emAfContainsTimeServerAttribute(endpoint, ZCL_DST_START_ATTRIBUTE_ID)
      && emAfContainsTimeServerAttribute(endpoint, ZCL_DST_END_ATTRIBUTE_ID)
      && emAfContainsTimeServerAttribute(endpoint, ZCL_DST_SHIFT_ATTRIBUTE_ID)) {
    timeStatus |= BIT(2);
  }
#endif //EMBER_AF_PLUGIN_TIME_SERVER_MASTER_ZONE_DST

#ifdef EMBER_AF_PLUGIN_TIME_SERVER_SUPERSEDING
  // Indicates that the time server should be considered as a more authoritative
  // time server.
  timeStatus |= BIT(3);
#endif //EMBER_AF_PLUGIN_TIME_SERVER_SUPERSEDING

  status = emberAfWriteAttribute(endpoint,
                                 ZCL_TIME_CLUSTER_ID,
                                 ZCL_TIME_STATUS_ATTRIBUTE_ID,
                                 CLUSTER_MASK_SERVER,
                                 (int8u *)&timeStatus,
                                 ZCL_BITMAP8_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfTimeClusterPrintln("ERR: writing time status %x", status);
  }

  // Ticks are scheduled for all endpoints that do not have a singleton time
  // attribute and for one of the endpoints with a singleton attribute.
  emberAfScheduleClusterTick(endpoint,
                             ZCL_TIME_CLUSTER_ID,
                             EMBER_AF_SERVER_CLUSTER_TICK,
                             MILLISECOND_TICKS_PER_SECOND,
                             EMBER_AF_OK_TO_HIBERNATE);
}

void emberAfTimeClusterServerTickCallback(int8u endpoint)
{
  // Update the attribute with the real time if we have it; otherwise, just
  // increment the current the value.
  int32u currentTime = emberAfGetCurrentTimeCallback();
  if (currentTime == 0) {
    readTime(endpoint, &currentTime);
    currentTime++;
  }
  writeTime(endpoint, currentTime);

  // Reschedule the tick callback.
  emberAfScheduleClusterTick(endpoint,
                             ZCL_TIME_CLUSTER_ID,
                             EMBER_AF_SERVER_CLUSTER_TICK,
                             MILLISECOND_TICKS_PER_SECOND,
                             EMBER_AF_OK_TO_HIBERNATE);
}

int32u emAfTimeClusterServerGetCurrentTime(void)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_SUCCESS;
  int32u currentTime = emberAfGetCurrentTimeCallback();

  // If we don't have the current time, we have to try to get it from an
  // endpoint by rolling through all of them until one returns a time.
  if (currentTime == 0) {
    int8u i;
    for (i = 0; i < emberAfEndpointCount(); i++) {
      int8u endpoint = emberAfEndpointFromIndex(i);
      if (emAfContainsTimeServerAttribute(endpoint, ZCL_TIME_ATTRIBUTE_ID)) {
        status = readTime(endpoint, &currentTime);
        if (status == EMBER_ZCL_STATUS_SUCCESS) {
          break;
        }
      }
    }
  }

  return (status == EMBER_ZCL_STATUS_SUCCESS ? currentTime : 0);
}

void emAfTimeClusterServerSetCurrentTime(int32u utcTime)
{
  // Set the time on all endpoints that do not have a singleton time attribute
  // as well as on one of the endpoints with a singleton attribute.
  int8u i;
  for (i = 0; i < emberAfEndpointCount(); i++) {
    int8u endpoint = emberAfEndpointFromIndex(i);
    if (emAfContainsTimeServerAttribute(endpoint, ZCL_TIME_ATTRIBUTE_ID)) {
      EmberAfAttributeMetadata *metadata = emberAfLocateAttributeMetadata(endpoint,
                                                                          ZCL_TIME_CLUSTER_ID,
                                                                          ZCL_TIME_ATTRIBUTE_ID,
                                                                          CLUSTER_MASK_SERVER,
                                                                          EMBER_AF_NULL_MANUFACTURER_CODE);
      if (!emberAfAttributeIsSingleton(metadata) || singleton == endpoint) {
        writeTime(endpoint, utcTime);
      }
    }
  }
}

static EmberAfStatus readTime(int8u endpoint, int32u *time)
{
  EmberAfStatus status = emberAfReadAttribute(endpoint,
                                              ZCL_TIME_CLUSTER_ID,
                                              ZCL_TIME_ATTRIBUTE_ID,
                                              CLUSTER_MASK_SERVER,
                                              (int8u *)time,
                                              sizeof(*time),
                                              NULL); // data type
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TIME_CLUSTER)
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfTimeClusterPrintln("ERR: reading time %x", status);
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TIME_CLUSTER)
  return status;
}

static EmberAfStatus writeTime(int8u endpoint, int32u time)
{
  EmberAfStatus status = emberAfWriteAttribute(endpoint,
                                               ZCL_TIME_CLUSTER_ID,
                                               ZCL_TIME_ATTRIBUTE_ID,
                                               CLUSTER_MASK_SERVER,
                                               (int8u *)&time,
                                               ZCL_UTC_TIME_ATTRIBUTE_TYPE);
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TIME_CLUSTER)
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfTimeClusterPrintln("ERR: writing time %x", status);
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_TIME_CLUSTER)
  return status;
}
#endif //ZCL_USING_TIME_CLUSTER_SERVER
