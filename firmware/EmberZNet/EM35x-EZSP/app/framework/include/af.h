/**
 * @file af.h
 * @brief The master include file for the Ember ApplicationFramework  API.
 *
 * <!-- Author(s): Ezra Hale, ezra@ember.com,        -->
 * <!--            Timotej Ecimovic, timotej@ember.com -->
 *
 * <!--Copyright 2004-2010 by Ember Corporation. All rights reserved.    *80*-->
 */

/**
 * @addtogroup af Application Framework V2 API Reference
 * This documentation describes the application programming interface (API)
 * for the Ember Application Framework V2.
 * The file af.h is the master include file for the Application Framework V2 modules.
 * @{
 */


#ifndef __AF_API__
#define __AF_API__

// Micro and compiler specific typedefs and macros
#include PLATFORM_HEADER

#ifndef CONFIGURATION_HEADER
  #define CONFIGURATION_HEADER "app/framework/util/config.h"
#endif
#include CONFIGURATION_HEADER

#ifdef EZSP_HOST
  // Includes needed for ember related functions for the EZSP host
  #include "stack/include/error.h"
  #include "stack/include/ember-types.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/ezsp-utils.h"
  #include "app/util/ezsp/serial-interface.h"
#else
  // Includes needed for ember related functions for the EM250
  #include "stack/include/ember.h"
#endif // EZSP_HOST

// HAL - hardware abstraction layer
#include "hal/hal.h" 
#include "app/util/serial/serial.h"  // Serial utility APIs

#include "stack/include/event.h"
#include "stack/include/error.h"
#include "af-types.h"
#include "app/framework/util/print.h"
#include "af-structs.h"
#include "attribute-id.h"
#include "att-storage.h"
#include "attribute-type.h"
#include "call-command-handler.h"
#include "callback.h"
#include "hal-callback.h"
#include "client-command-macro.h"
#include "cluster-id.h"
#include "command-id.h"
#include "debug-printing.h"
#include "enums.h"
#include "print-cluster.h"
#include "stack-handlers.h"
#include "app/framework/util/client-api.h"

/** @name Attribute Storage */
// @{

/**
 * @brief locate attribute metadata
 *
 * Function returns pointer to the attribute metadata structure,
 * or NULL if attribute was not found.
 *
 * @param endpoint Zigbee endpoint number.
 * @param cluster Cluster ID of the sought cluster.
 * @param attribute Attribute ID of the sought attribute.
 * @param mask CLUSTER_MASK_SERVER or CLUSTER_MASK_CLIENT
 *
 * @return Returns pointer to the attribute metadata location.
 */
EmberAfAttributeMetadata *emberAfLocateAttributeMetadata(int8u endpoint,
                                                         EmberAfClusterId cluster,
                                                         EmberAfAttributeId attribute,
                                                         int8u mask,
                                                         int16u manufacturerCode);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Returns TRUE if the attribute exists. */
  boolean emberAfContainsAttribute(int8u endpoint, 
                                   EmberAfClusterId clusterId,
                                   EmberAfAttributeId attributeId,
                                   int8u mask,
                                   int16u manufacturerCode);
#else
  #define emberAfContainsAttribute(endpoint, clusterId, attributeId, mask, manufacturerCode) \
    (emberAfLocateAttributeMetadata(endpoint, clusterId, attributeId, mask, manufacturerCode) != NULL)
#endif

// Returns true if attribute exists


/**
 * @brief Returns TRUE If endpoint contains cluster.
 *
 * This function returns true regardless of whether
 * the endpoint contains server, client or both.
 */
boolean emberAfContainsCluster(int8u endpoint, EmberAfClusterId clusterId);

/**
 * @brief Returns TRUE If endpoint contains cluster server.
 *
 * This function returns true if
 * the endpoint contains server of a given cluster.
 */
boolean emberAfContainsServer(int8u endpoint, EmberAfClusterId clusterId);

/**
 * @brief Returns TRUE If endpoint contains cluster client.
 *
 * This function returns true if
 * the endpoint contains client of a given cluster.
 */
boolean emberAfContainsClient(int8u endpoint, EmberAfClusterId clusterId);

/**
 * @brief write an attribute, performing all the checks.
 *
 * This function will attempt to write the attribute value from
 * the provided pointer. This function will only check that the
 * attribute exists. If it does it will write the value into
 * the attribute table for the given attribute.
 *
 * This funciton will not check to see if the attribute is
 * writable since the read only / writable characteristic
 * of an attribute only pertains to external devices writing
 * over the air. Because this function is being called locally
 * it assumes that the device knows what it is doing and has permission
 * to perform the given operation.
 *
 * @see emberAfWriteClientAttribute, emberAfWriteServerAttribute, 
 *      emberAfWriteManufacturerSpecificClientAttribute, 
 *      emberAfWriteManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfWriteAttribute(int8u endpoint,
                                    EmberAfClusterId cluster,
                                    EmberAfAttributeId attributeID,
                                    int8u mask,
                                    int8u* dataPtr,
                                    int8u dataType);

/**
 * @brief write a cluster server attribute.
 *
 * This function is the same as emberAfWriteAttribute
 * except that it saves having to pass the cluster mask.
 * this is useful for code savings since write attribute
 * is used frequently throughout the framework
 *
 * @see emberAfWriteClientAttribute, 
 *      emberAfWriteManufacturerSpecificClientAttribute, 
 *      emberAfWriteManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfWriteServerAttribute(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u* dataPtr,
                                          int8u dataType);

/**
 * @brief write an cluster client attribute.
 *
 * This function is the same as emberAfWriteAttribute
 * except that it saves having to pass the cluster mask.
 * this is useful for code savings since write attribute
 * is used frequently throughout the framework
 *
 * @see emberAfWriteServerAttribute, 
 *      emberAfWriteManufacturerSpecificClientAttribute, 
 *      emberAfWriteManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfWriteClientAttribute(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u* dataPtr,
                                          int8u dataType);

/**
 * @brief write an manufacturer specific server attribute.
 *
 * This function is the same as emberAfWriteAttribute
 * except that it saves having to pass the cluster mask.
 * this is useful for code savings since write attribute
 * is used frequently throughout the framework
 *
 * @see emberAfWriteClientAttribute, emberAfWriteServerAttribute, 
 *      emberAfWriteManufacturerSpecificClientAttribute 
 */
EmberAfStatus emberAfWriteManufacturerSpecificServerAttribute(int8u endpoint,
                                                              EmberAfClusterId cluster,
                                                              EmberAfAttributeId attributeID,
                                                              int16u manufacturerCode,
                                                              int8u* dataPtr,
                                                              int8u dataType);

/**
 * @brief write an manufacturer specific client attribute.
 *
 * This function is the same as emberAfWriteAttribute
 * except that it saves having to pass the cluster mask.
 * this is useful for code savings since write attribute
 * is used frequently throughout the framework
 *
 * @see emberAfWriteClientAttribute, emberAfWriteServerAttribute, 
 *      emberAfWriteManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfWriteManufacturerSpecificClientAttribute(int8u endpoint,
                                                              EmberAfClusterId cluster,
                                                              EmberAfAttributeId attributeID,
                                                              int16u manufacturerCode,
                                                              int8u* dataPtr,
                                                              int8u dataType);


/**
 * @brief Function that test the success of attribute write.
 *
 * This function returns success if attribute write would be succesfull.
 * It does not actuall write anything, just validates for read-only and
 * data-type.
 *
 * @param endpoint Zigbee endpoint number
 * @param cluster Cluster ID of the sought cluster.
 * @param attribute Attribute ID of the sought attribute.
 * @param mask CLUSTER_MASK_SERVER or CLUSTER_MASK_CLIENT
 * @param buffer Location where attribute will be written from.
 * @param dataType ZCL attribute type.
 */
EmberAfStatus emberAfVerifyAttributeWrite(int8u endpoint,
                                          EmberAfClusterId cluster,
                                          EmberAfAttributeId attributeID,
                                          int8u mask,
                                          int16u manufacturerCode,
                                          int8u* buffer,
                                          int8u dataType);

/**
 * @brief Read the attribute value, performing all the checks.
 *
 * This function will attempt to read the attribute and store
 * it into the pointer. It will also read the data type.
 * Both dataPtr and dataType may be NULL, signifying that either
 * value or type is not desired.
 *
 * @see emberAfReadClientAttribute, emberAfReadServerAttribute, 
 *      emberAfReadManufacturerSpecificClientAttribute, 
 *      emberAfReadManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfReadAttribute(int8u endpoint,
                                   EmberAfClusterId cluster,
                                   EmberAfAttributeId attributeID,
                                   int8u mask,
                                   int8u* dataPtr,
                                   int8u readLength,
                                   int8u* dataType);

/**
 * @brief Read the server attribute value, performing all the checks.
 *
 * This function will attempt to read the attribute and store
 * it into the pointer. It will also read the data type.
 * Both dataPtr and dataType may be NULL, signifying that either
 * value or type is not desired.
 *
 * @see emberAfReadClientAttribute, 
 *      emberAfReadManufacturerSpecificClientAttribute, 
 *      emberAfReadManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfReadServerAttribute(int8u endpoint,
                                         EmberAfClusterId cluster,
                                         EmberAfAttributeId attributeID,
                                         int8u* dataPtr,
                                         int8u readLength);

/**
 * @brief Read the client attribute value, performing all the checks.
 *
 * This function will attempt to read the attribute and store
 * it into the pointer. It will also read the data type.
 * Both dataPtr and dataType may be NULL, signifying that either
 * value or type is not desired.
 *
 * @see emberAfReadServerAttribute, 
 *      emberAfReadManufacturerSpecificClientAttribute, 
 *      emberAfReadManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfReadClientAttribute(int8u endpoint,
                                         EmberAfClusterId cluster,
                                         EmberAfAttributeId attributeID,
                                         int8u* dataPtr,
                                         int8u readLength);

/**
 * @brief Read the server attribute value, performing all the checks.
 *
 * This function will attempt to read the attribute and store
 * it into the pointer. It will also read the data type.
 * Both dataPtr and dataType may be NULL, signifying that either
 * value or type is not desired.
 *
 * @see emberAfReadClientAttribute, emberAfReadServerAttribute, 
 *      emberAfReadManufacturerSpecificClientAttribute
 */
EmberAfStatus emberAfReadManufacturerSpecificServerAttribute(int8u endpoint,
                                                             EmberAfClusterId cluster,
                                                             EmberAfAttributeId attributeID,
                                                             int16u manufacturerCode,
                                                             int8u* dataPtr,
                                                             int8u readLength);

/**
 * @brief Read the client attribute value, performing all the checks.
 *
 * This function will attempt to read the attribute and store
 * it into the pointer. It will also read the data type.
 * Both dataPtr and dataType may be NULL, signifying that either
 * value or type is not desired.
 *
 * @see emberAfReadClientAttribute, emberAfReadServerAttribute, 
 *      emberAfReadManufacturerSpecificServerAttribute
 */
EmberAfStatus emberAfReadManufacturerSpecificClientAttribute(int8u endpoint,
                                                             EmberAfClusterId cluster,
                                                             EmberAfAttributeId attributeID,
                                                             int16u manufacturerCode,
                                                             int8u* dataPtr,
                                                             int8u readLength);


/**
 * @brief this function returns the size of the ZCL data in bytes.
 *
 * @param dataType Zcl data type
 * @return size in bytes or 0 if invalid data type
 */
int8u emberAfGetDataSize(int8u dataType);

/**
 * @brief macro that returns true if the cluster is in the manufacturer specific range
 *
 * @param cluster EmberAfCluster* to consider
 */
#define emberAfClusterIsManufacturerSpecific(cluster) ((cluster)->clusterId >= 0xFC00)


/**
 * @brief macro that returns true if attribute is read only.
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeIsReadOnly(metadata) (((metadata)->mask & ATTRIBUTE_MASK_WRITABLE) == 0)

/**
 * @brief macro that returns true if this is a client attribute, and false if it is server
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeIsClient(metadata) (((metadata)->mask & ATTRIBUTE_MASK_CLIENT) != 0)

/**
 * @brief macro that returns true if attribute is saved to token.
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeIsTokenized(metadata) (((metadata)->mask & ATTRIBUTE_MASK_TOKENIZE) != 0)

/**
 * @brief macro that returns true if attribute is a singleton
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeIsSingleton(metadata) (((metadata)->mask & ATTRIBUTE_MASK_SINGLETON) != 0)

/**
 * @brief macro that returns true if attribute is manufacturer specific
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeIsManufacturerSpecific(metadata) (((metadata)->mask & ATTRIBUTE_MASK_MANUFACTURER_SPECIFIC) != 0)


/**
 * @brief macro that returns size of attribute in bytes.
 *
 * @param metadata EmberAfAttributeMetadata* to consider.
 */
#define emberAfAttributeSize(metadata) ((metadata)->size)

// master array of all defined endpoints
extern EmberAfDefinedEndpoint emAfEndpoints[];

// Master array of all defined networks.
extern EmberAfNetwork emAfNetworks[];

// The current network.
extern EmberAfNetwork *emAfCurrentNetwork;

/**
 * @brief Macro that takes index of endpoint, and returns Zigbee endpoint
 */
int8u emberAfEndpointFromIndex(int8u index);

/**
 * Returns the index of a given endpoint
 */
int8u emberAfIndexFromEndpoint(int8u endpoint);

/**
 * Returns the endpoint index within a given cluster (Client-side)
 */
int8u emberAfFindClusterClientEndpointIndex(int8u endpoint, EmberAfClusterId clusterId);

/**
 * Returns the endpoint index within a given cluster (Server-side)
 */
int8u emberAfFindClusterServerEndpointIndex(int8u endpoint, EmberAfClusterId clusterId);

/**
 * @brief Macro that takes index of endpoint, and returns profile Id for it
 */
#define emberAfProfileIdFromIndex(index) (emAfEndpoints[(index)].profileId)

/**
 * @brief Macro that takes index of endpoint, and returns device Id for it
 */
#define emberAfDeviceIdFromIndex(index) (emAfEndpoints[(index)].deviceId)

/**
 * @brief Macro that takes index of endpoint, and returns device version for it
 */
#define emberAfDeviceVersionFromIndex(index) (emAfEndpoints[(index)].deviceVersion)

/**
 * @brief Macro that takes index of endpoint, and returns network index for it
 */
#define emberAfNetworkIndexFromEndpointIndex(index) (emAfEndpoints[(index)].networkIndex)

/** Returns the network index of a given endpoint. */
int8u emberAfNetworkIndexFromEndpoint(int8u endpoint);

/**
 * @brief Macro that returns primary profile ID.
 *
 * Primary profile is the profile of a primary endpoint as defined
 * in AppBuilder.
 */
#define emberAfPrimaryProfileId()       emberAfProfileIdFromIndex(0)

/**
 * @brief Macro that returns the primary endpoint.
 */
#define emberAfPrimaryEndpoint() (emAfEndpoints[0].endpoint)


/**
 * @brief Returns the number of endpoints.
 */
int8u emberAfEndpointCount(void);

/**
 * Data types are either analog or discrete. This makes a difference for
 * some of the ZCL global commands
 */
enum {
  EMBER_AF_DATA_TYPE_ANALOG     = 0,
  EMBER_AF_DATA_TYPE_DISCRETE   = 1,
  EMBER_AF_DATA_TYPE_NONE       = 2
};
/**
 * @brief Returns the type of the attribute, either ANALOG, DISCRETE or NONE
 */
int8u emberAfGetAttributeAnalogOrDiscreteType(int8u dataType);

/**
 *@brief Returns true if type is signed, false otherwise.
 */
boolean emberAfIsTypeSigned(int8u dataType);

/**
 * @brief Function that extracts a 32-bit integer from the message buffer
 */
int32u emberAfGetInt32u(const int8u* message, int16u currentIndex, int16u msgLen);

/**
 * @brief Function that extracts a 24-bit integer from the message buffer
 */
int32u emberAfGetInt24u(const int8u* message, int16u currentIndex, int16u msgLen);

/**
 * @brief Function that extracts a 16-bit integer from the message buffer
 */
int16u emberAfGetInt16u(const int8u* message, int16u currentIndex, int16u msgLen);
/**
 * @brief Function that extracts a ZCL string from the message buffer
 */
int8u* emberAfGetString(int8u* message, int16u currentIndex, int16u msgLen);
/**
 * @brief Function that extracts a ZCL long string from the message buffer
 */
int8u* emberAfGetLongString(int8u* message, int16u currentIndex, int16u msgLen);

/**
 * @brief Macro for consistency, that extracts single byte out of the message
 */
#define emberAfGetInt8u(message, currentIndex, msgLen) message[currentIndex]

/**
 * @brief Macro for consistency that copies an int8u from variable into buffer.
 */
#define emberAfCopyInt8u(data,index,x) (data[index] = (x))
/**
 * @brief function that copies an in16u value into a buffer
 */
void emberAfCopyInt16u(int8u *data, int16u index, int16u x);
/**
 * @brief function that copies an in24u value into a buffer
 */
void emberAfCopyInt24u(int8u *data, int16u index, int32u x);
/**
 * @brief function that copies an in32u value into a buffer
 */
void emberAfCopyInt32u(int8u *data, int16u index, int32u x);
/*
 * @brief Function that copies a ZigBee string into a buffer.  The size
 * parameter should indicate the maximum number of characters to copy to the
 * destination buffer not including the length byte.
 */
void emberAfCopyString(int8u *dest, int8u *src, int8u size);
/*
 * @brief Function that copies a ZigBee long string into a buffer.  The size
 * parameter should indicate the maximum number of characters to copy to the
 * destination buffer not including the length byte.
 */
void emberAfCopyLongString(int8u *dest, int8u *src, int16u size);
/*
 * @brief Function that determines the length of a ZigBee string.
 */
int8u emberAfStringLength(const int8u *buffer);
/*
 * @brief Function that determines the length of a ZigBee long string.
 */
int16u emberAfLongStringLength(const int8u *buffer);

/** @} END Attribute Storage */

/** @name Device Control */
// @{

/**
 * @brief Function that checks if endpoint is enabled.
 *
 * This function returns true if device at a given endpoint is
 * enabled. At startup all endpoints are enabled.
 *
 * @param endpoint Zigbee endpoint number
 */
boolean emberAfIsDeviceEnabled(int8u endpoint);

/**
 * @brief Function that checks if endpoint is identifying
 *
 * This function returns true if device at a given endpoint is
 * identifying.
 *
 * @param endpoint Zigbee endpoint number
 */
boolean emberAfIsDeviceIdentifying(int8u endpoint);

/**
 * @brief Function that enables or disables an endpoint.
 *
 * By calling this function, you turn off all processing of incoming traffic
 * for a given endpoint.
 *
 * @param endpoint Zigbee endpoint number
 */
void emberAfSetDeviceEnabled(int8u endpoint, boolean enabled);

/** @} END Device Control */

/** @name Miscellaneous */
// @{

/**
 * @brief Enable/disable endpoints
 */
boolean emberAfEndpointEnableDisable(int8u endpoint, boolean enable);


/**
 * @brief Determine if an endpoint at the specifid index is enabled or disabled
 */
boolean emberAfEndpointIndexIsEnabled(int8u index);


/**
 * @brief This indicates a new image verification is taking place.
 */
#define EMBER_AF_NEW_IMAGE_VERIFICATION TRUE

/**
 * @brief This indicates the continuation of an image verification already 
 * in progress.
 */
#define EMBER_AF_CONTINUE_IMAGE_VERIFY  FALSE


/**
 * @brief This variable defines an invalid image id.  It is used
 *   to determine if a returned EmberAfOtaImageId is valid or not.
 *   This is done by passing the data to the function
 *   emberAfIsOtaImageIdValid().
 */
extern PGM EmberAfOtaImageId emberAfInvalidImageId;


/**
 * @brief Returns true if a given ZCL data type is a string type.
 *
 * You should use this function if you need to perform a different
 * memory operation on a certain attribute because it is a string type.
 * Since ZCL strings carry length as the first byte(s), it is often required
 * to treat them differently than regular data types.
 *
 * @return true if data type is a string.
 */
boolean emberAfIsThisDataTypeAStringType(int8u dataType);

/** @brief Returns TRUE if the given attribute type is a string. */
boolean emberAfIsStringAttributeType(EmberAfAttributeType attributeType);

/** @brief Returns TRUE if the given attribute type is a long string. */
boolean emberAfIsLongStringAttributeType(EmberAfAttributeType attributeType);

/**
 * @brief The mask applied by ::emberAfNextSequence when generating ZCL
 * sequence numbers.
 */
#define EMBER_AF_ZCL_SEQUENCE_MASK 0x7F

/**
 * @brief Increments the ZCL sequence number and returns the value.
 *
 * ZCL messages have sequence numbers so that they can be matched up with other
 * messages in the transaction.  To avoid comflicts with sequece numbers
 * generated independently by the application, this API returns sequence
 * numbers with the high bit clear.  If the application generates its own
 * sequence numbers, it should use numbers with the high bit set.
 * 
 * @return The next ZCL sequence number.
 */
int8u emberAfNextSequence(void);

/** 
 * @brief Retreives the last sequence number that was used.
 *
 */
int8u emberAfGetLastSequenceNumber(void);


/**
 * @brief Simple integer comparison function.
 * Compares two values of a known length as integers.
 * The integers are in native endianess.
 *
 * @return -1 if val1 is smaller, 0 if they are the same or 1 if val2 is smaller.
 */
int8s emberAfCompareValues(int8u* val1, int8u* val2, int8u len);

/**
 * @brief populates the eui64 with the local eui64.
 */
void emberAfGetEui64(EmberEUI64 returnEui64);

/**
 * @brief Returns the node ID of the local node.
 */
EmberNodeId emberAfGetNodeId(void);

#if defined(DOXYGEN_SHOULD_SKIP_THIS) || defined(EZSP_HOST)
/**
 * @brief Generates a random key (link, network, or master).
 */
EmberStatus emberAfGenerateRandomKey(EmberKeyData *result);
#else
  #define emberAfGenerateRandomKey(result) emberGenerateRandomKey(result)
#endif

/**
 * @brief Returns the PAN ID of the local node.
 */
EmberPanId emberAfGetPanId(void);

/*
 * @brief Returns a binding index that matches the current incoming message, if
 * known.
 */
int8u emberAfGetBindingIndex(void);

/*
 * @brief Returns an address index that matches the current incoming message,
 * if known.
 */
int8u emberAfGetAddressIndex(void);

/**
 * @brief Returns the current network parameters.
 */
EmberStatus emberAfGetNetworkParameters(EmberNodeType *nodeType,
                                        EmberNetworkParameters *parameters);

/**
 * @brief An App. Framework defined rejoin reason.  
 */
#define EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE      0xA0
#define EMBER_AF_REJOIN_DUE_TO_TC_KEEPALIVE_FAILURE 0xA1
#define EMBER_AF_REJOIN_DUE_TO_CLI_COMMAND          0xA2

#define EMBER_AF_REJOIN_FIRST_REASON                EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE
#define EMBER_AF_REJOIN_LAST_REASON                 EMBER_AF_REJOIN_DUE_TO_END_DEVICE_MOVE

/** @} END Miscellaneous */

/** @name Printing */
// @{

// Guaranteed print
/**
 * @brief Print that can't be turned off.
 *
 */
#define emberAfGuaranteedPrint(...)   emberAfPrint(0xFFFF, __VA_ARGS__)

/**
 * @brief Println that can't be turned off.
 */
#define emberAfGuaranteedPrintln(...) emberAfPrintln(0xFFFF, __VA_ARGS__)

/**
 * @brief Buffer print that can't be turned off.
 */
#define emberAfGuaranteedPrintBuffer(buffer,len,withSpace) emberAfPrintBuffer(0xFFFF,(buffer),(len),(withSpace))

/**
 * @brief String print that can't be turned off.
 */
#define emberAfGuaranteedPrintString(buffer) emberAfPrintString(0xFFFF,(buffer))

/**
 * @brief Long string print that can't be turned off.
 */
#define emberAfGuaranteedPrintLongString(buffer) emberAfPrintLongString(0xFFFF,(buffer))

/**
 * @brief Buffer flush for emberAfGuaranteedPrint(), emberAfGuaranteedPrintln(),
 * emberAfGuaranteedPrintBuffer(), and emberAfGuaranteedPrintString().
 */
#define emberAfGuaranteedFlush()      emberAfFlush(0xFFFF)

/**
 * @brief returns true if certain debug area is enabled.
 */
boolean emberAfPrintEnabled(int16u functionality);

/**
 * @brief Useful function to print a buffer
 */
void emberAfPrintBuffer(int16u area, const int8u *buffer, int16u bufferLen, boolean withSpaces);

/**
 * @brief Useful function to print character strings.  The first byte of the
 * buffer specifies the length of the string.
 */
void emberAfPrintString(int16u area, const int8u *buffer);

/**
 * @brief Useful function to print long character strings.  The first two bytes
 * of the buffer specify the length of the string.
 */
void emberAfPrintLongString(int16u area, const int8u *buffer);

/**
 * @brief prints the specified text if certain debug are is enabled
 * @param functionality: one of the EMBER_AF_PRINT_xxx macros as defined by AppBuilder
 * @param formatString: formatString for varargs
 */
void emberAfPrint(int16u functionality, PGM_P formatString, ...);

/**
 * @brief prints the specified text if certain debug are is enabled.
 * Print-out will include the newline character at the end.
 * @param functionality: one of the EMBER_AF_PRINT_xxx macros as defined by AppBuilder
 * @param formatString: formatString for varargs
 */
void emberAfPrintln(int16u functionality, PGM_P formatString, ...);

/**
 * @brief buffer flush
 */
void emberAfFlush(int16u functionality);

/**
 * @brief turns on debugging for certain functional area
 */
void emberAfPrintOn(int16u functionality);

/**
 * @brief turns off debugging for certain functional area
 */
void emberAfPrintOff(int16u functionality);

/** @brief turns on debugging for all functional areas
 */
void emberAfPrintAllOn(void);

/**
 * @brief turns off debugging for all functional areas
 */
void emberAfPrintAllOff(void);

/**
 * @brief prints current status of functional areas
 */
void emberAfPrintStatus(void);

/**
 * @brief prints eui64 stored in little endian format
 */
void emberAfPrintLittleEndianEui64(const EmberEUI64 eui64);


/**
 * @brief prints eui64 stored in big endian format
 */
void emberAfPrintBigEndianEui64(const EmberEUI64 eui64);

/**
 * @brief prints all message data in message format
 */
void emberAfPrintMessageData(int8u* data, int16u length);


/** @} END Printing */



/** @name Sleep Control */
//@{
#define MILLISECOND_TICKS_PER_QUARTERSECOND (MILLISECOND_TICKS_PER_SECOND >> 2)
#define MILLISECOND_TICKS_PER_MINUTE (60UL * MILLISECOND_TICKS_PER_SECOND)
#define MILLISECOND_TICKS_PER_HOUR   (60UL * MILLISECOND_TICKS_PER_MINUTE)

/**
 * @brief A function used to add a task to the task register.
 */
#define emberAfAddToCurrentAppTasks(x) \
  emberAfAddToCurrentAppTasksCallback(x)

/**
 * @brief A function used to remove a task from the task register.
 */
#define emberAfRemoveFromCurrentAppTasks(x) \
  emberAfRemoveFromCurrentAppTasksCallback(x)

/**
 * @brief A macro used to retrieve the bitmask of all application
 * frameowrk tasks currently in progress. This can be useful for debugging if
 * some task is holding the device out of hibernation.
 */
#define emberAfCurrentAppTasks() emberAfGetCurrentAppTasksCallback()

/**
 * @brief a function used to run the applcation framework's
 *        event mechanism. This function passes the application
 *        framework's event tables to the ember stack's event
 *        processing code.
 */
void emberAfRunEvents(void);

/**
 * @brief Friendly define for use in the scheduling or canceling client events
 * with emberAfScheduleClusterTick() and emberAfDeactivateClusterTick().
 */
#define EMBER_AF_CLIENT_CLUSTER_TICK TRUE

/**
* @brief Friendly define for use in the scheduling or canceling server events
* with emberAfScheduleClusterTick() and emberAfDeactivateClusterTick().
 */
#define EMBER_AF_SERVER_CLUSTER_TICK FALSE

/**
 * @brief This function is used to schedule a cluster related
 * event inside the application framework's event mechanism.
 * This function provides a wrapper for the ember stack
 * event mechanism which allows the cluster code
 * to access its events by their endpoint, cluster id and client/server
 * identity. The passed sleepControl value provides the same functionality
 * as adding an event to the current app tasks. By passing this value, the
 * cluster is able to indicate the priority of the event that is scheduled
 * and what type of sleeping the processor should be able to do while
 * the event being scheduled is active. The sleepControl value for each
 * active event is checked inside the emberAfOkToHibernate and emberAfOkToNap
 * functions.
 *
 * @param endpoint the endpoint of the event to be scheduled
 * @param clusterId the clusterId of the event to be scheduled
 * @param isClient TRUE if the event to be scheduled is associated with a
 *        client cluster, FALSE otherwise.
 * @param time the number of milliseconds till the event should be called.
 * @param sleepControl the priority of the event, what the processor should
 *        be allowed to do in terms of sleeping while the event is active.
 *
 * @return If the event in question was found in the event
 *         table and scheduled, this function will return EMBER_SUCCESS,
 *         If the event could not be found it will return EMBER_BAD_ARGUMENT
 *
 */
EmberStatus emberAfScheduleClusterTick(int8u endpoint,
                                       int16u clusterId,
                                       boolean isClient,
                                       int32u timeMs,
                                       EmberAfEventSleepControl sleepControl);

/**
 * @brief A function used to deactivate a cluster related event.
 * This function provides a wrapper for the ember stack's event mechanism
 * which allows an event to be accessed by its endpoint, cluster id and
 * client/server identity.
 *
 * @param endpoint the endpoint of the event to be deactivated.
 * @param clusterId the clusterId of the event to be deactivated.
 * @param isClient TRUE if the event to be deactivate is a client
 *        cluster, FALSE otherwise.
 *
 * @return If the event in question was found in the event table
 *         and scheduled, this function will return EMBER_SUCCESS,
 *         If the event could not be found it will return
 *         EMBER_BAD_ARGUMENT
 *
 */
EmberStatus emberAfDeactivateClusterTick(int8u endpoint,
                                         int16u clusterId,
                                         boolean isClient);

/**
 * @brief A convenience function used to schedule an event in
 * the Ember Event Control API depending on how large the passed
 * timeMs value is. The Ember Event Control API uses three functions
 * for the scheduling of events which indicate the time increment in
 * milliseconds, quarter seconds, or seconds. This function wraps that
 * API and provides a single function for the scheduling of all events
 * based on a passed int32u value indicating the number of milliseconds
 * until the next event. This function is used by both the cluster scheduling
 * as well as the application framework's own internal event scheduling
 *
 * @param eventControl A pointer to the ember event control value which
 *                     is referred to in the event table.
 * @param timeMs the number of milliseconds until the next event.
 *
 * @return If the passed milliseconds can be transformed into a proper
 *         event control time increment: milliseconds, quarter seconds or
 *         minutes, that can be represented in a int16u, this function will
 *         schedule the event and return EMBER_SUCCESS. Otherwise it will
 *         return EMBER_BAD_ARGUMENT.
 *
 */
EmberStatus emberAfEventControlSetDelay(EmberEventControl *eventControl, int32u timeMs);

/**
 * @brief Sets the ::EmberEventControl for the current network as inactive.
 */
void emberAfNetworkEventControlSetInactive(EmberEventControl *controls);
/**
 * @brief Returns TRUE if the event for the current number is active.
 */
boolean emberAfNetworkEventControlGetActive(EmberEventControl *controls);
/**
 * @brief Sets the ::EmberEventControl for the current network to run at the
 * next available opportunity.
 */
void emberAfNetworkEventControlSetActive(EmberEventControl *controls);
/**
 * @brief A convenience function that sets the ::EmberEventControl for the
 * current network to run "timeMs" milliseconds in the future using
 * ::emberAfEventControlSetDelay.
 */
EmberStatus emberAfNetworkEventControlSetDelay(EmberEventControl *controls, int32u timeMs);
/**
 * @brief Sets the ::EmberEventControl for the current network to run "delay"
 * milliseconds in the future.
 */
void emberAfNetworkEventControlSetDelayMS(EmberEventControl *controls, int16u delay);
/**
 * @brief Sets the ::EmberEventControl for the current network to run "delay"
 * quarter seconds in the future.
 */
void emberAfNetworkEventControlSetDelayQS(EmberEventControl *controls, int16u delay);
/**
 * @brief Sets the ::EmberEventControl for the current network to run "delay"
 * minutes in the future.
 */
void emberAfNetworkEventControlSetDelayMinutes(EmberEventControl *controls, int16u delay);

/**
 * @brief Sets the ::EmberEventControl for the specified endpoint as inactive.
 */
void emberAfEndpointEventControlSetInactive(EmberEventControl *controls, int8u endpoint);
/**
 * @brief Returns TRUE if the event for the current number is active.
 */
boolean emberAfEndpointEventControlGetActive(EmberEventControl *controls, int8u endpoint);
/**
 * @brief Sets the ::EmberEventControl for the specified endpoint to run at the
 * next available opportunity.
 */
void emberAfEndpointEventControlSetActive(EmberEventControl *controls, int8u endpoint);
/**
 * @brief A convenience function that sets the ::EmberEventControl for the
 * specified endpoint to run "timeMs" milliseconds in the future using
 * ::emberAfEventControlSetDelay.
 */
EmberStatus emberAfEndpointEventControlSetDelay(EmberEventControl *controls, int8u endpoint, int32u timeMs);
/**
 * @brief Sets the ::EmberEventControl for the specified endpoint to run "delay"
 * milliseconds in the future.
 */
void emberAfEndpointEventControlSetDelayMS(EmberEventControl *controls, int8u endpoint, int16u delay);
/**
 * @brief Sets the ::EmberEventControl for the specified endpoint to run "delay"
 * quarter seconds in the future.
 */
void emberAfEndpointEventControlSetDelayQS(EmberEventControl *controls, int8u endpoint, int16u delay);
/**
 * @brief Sets the ::EmberEventControl for the specified endpoint to run "delay"
 * minutes in the future.
 */
void emberAfEndpointEventControlSetDelayMinutes(EmberEventControl *controls, int8u endpoint, int16u delay);

/**
 * @brief A function used to retrieve the number of milliseconds until
 * the next event scheduled in the application framework's event
 * mechanism.
 * @param maxMs, the maximum number of milliseconds until the next
 *        event.
 * @return The number of milliseconds until the next event or
 * maxMs if no event is scheduled before then.
 */
int32u emberAfMsToNextEvent(int32u maxMs);

/** @brief This is the same as the function emberAfMsToNextEvent() with the 
 *  following addition.  If returnIndex is non-NULL it returns the index
 *  of the event that is ready to fire next.
 */
int32u emberAfMsToNextEventExtended(int32u maxMs, int8u* returnIndex);


/**
 * @brief A function used to retrieve the number of quarter seconds until
 * the next event scheduled in the application framework's event
 * mechanism. This function will round down and will return 0 if the
 * next event must fire within a quarter second.
 * @param maxQS, the maximum number of quarter seconds until the next
 *        event.
 * @return The number of quarter seconds until the next event or
 * maxQS if no event is scheduled before then.
 */
#define emberAfQSToNextEvent(maxQS)                                  \
  (emberAfMsToNextEvent(maxQS * MILLISECOND_TICKS_PER_QUARTERSECOND) \
   / MILLISECOND_TICKS_PER_QUARTERSECOND)

/**
 * @brief A function for retrieving the most restrictive sleep
 * control value for all scheduled events. This function is
 * used by emberAfOkToNap and emberAfOkToHibernate to makes sure
 * that there are no events scheduled which will keep the device
 * from hibernating or napping.
 * @return The most restrictive sleep control value for all
 *         scheduled events or the value returned by 
 *         emberAfGetDefaultSleepControl()
 *         if no events are currently scheduled. The default
 *         sleep control value is initialized to 
 *         EMBER_AF_OK_TO_HIBERNATE but can be changed at any
 *         time using the emberAfSetDefaultSleepControl() function.
 */
#define emberAfGetCurrentSleepControl() \
  emberAfGetCurrentSleepControlCallback()

/**
 * @brief A function for setting the default sleep control
 *        value against which all scheduled event sleep control
 *        values will be evaluated. This can be used to keep
 *        a device awake for an extended period of time by setting
 *        the default to EMBER_AF_STAY_AWAKE and then resetting
 *        the value to EMBER_AF_OK_TO_HIBERNATE once the wake
 *        period is complete.
 */
#define  emberAfSetDefaultSleepControl(x) \
  emberAfSetDefaultSleepControlCallback(x)

/**
 * @brief A function used to retrive the default sleep control against
 *        which all event sleep control values are evaluated. The
 *        default sleep control value is initialized to 
 *        EMBER_AF_OK_TO_HIBERNATE but can be changed by the application
 *        at any time using the emberAfSetDefaultSleepControl() function.
 * @return The current default sleep control value.
 */
#define emberAfGetDefaultSleepControl() \
  emberAfGetDefaultSleepControlCallback()

/** @} END Sleep Control */


/** @name Time */
// @{

/**
 * @brief Retrieves current time.
 *
 * Convienience function for retrieving the current time.
 * If the time server cluster is implemented, then the time
 * is retrieved from that cluster's time attribute. Otherwise,
 * the emberAfGetCurrentTimeCallback is called. In the case of the
 * stub, the current hal time is returned.
 *
 * A real time is expected to in the ZigBee time format, the number
 * of seconds since the year 2000.
 */
int32u emberAfGetCurrentTime(void);

/**
 * @brief Sets current time.
 * Convenience function for setting the time to a value.
 * If the time server cluster is implemented on this device,
 * then this call will be passed along to the time cluster server
 * which will update the time. Otherwise the emberAfSetTimeCallback
 * is called, which in the case of the stub does nothing.
 *
 * @param utcTime: A ZigBee time, the number of seconds since the
 *                 year 2000.
 */
void emberAfSetTime(int32u utcTime);


/**
 * @brief Prints time.
 *
 * Convenience function for all clusters to print time.
 * This function expects to be passed a ZigBee time which
 * is the number of seconds since the year 2000. If
 * EMBER_AF_PRINT_CORE is defined, this function will print
 * a human readable time from the passed value. If not, this
 * function will print nothing.
 *
 * @param utcTime: A ZigBee time, the number of seconds since the
 *                 year 2000.
 */
void emberAfPrintTime(int32u utcTime);

/** @} END Time */


/** @name Messaging */
// @{

/**
 * @brief This function sends a ZCL response, based on the information
 * that is currently in the outgoing buffer. It is expected that a complete
 * ZCL message is present, including header.  The application may use
 * this method directly from within the message handling function
 * and associated callbacks.  However this will result in the 
 * response being sent before the APS Ack is sent which is not
 * ideal.
 * 
 * NOTE:  This will overwrite the ZCL sequence number of the message
 * to use the LAST received sequence number.
 */
EmberStatus emberAfSendResponse(void);

/**
 * @brief Sends multicast.
 */
EmberStatus emberAfSendMulticast(EmberMulticastId multicastId,
                                 EmberApsFrame *apsFrame,
                                 int16u messageLength,
                                 int8u* message);

/**
 * @brief Sends broadcast.
 */
EmberStatus emberAfSendBroadcast(EmberNodeId destination,
                                 EmberApsFrame *apsFrame,
                                 int16u messageLength,
                                 int8u* message);

/**
 * @brief Sends unicast.
 */
EmberStatus emberAfSendUnicast(EmberOutgoingMessageType type,
                               int16u indexOrDestination,
                               EmberApsFrame *apsFrame,
                               int16u messageLength,
                               int8u* message);

/**
 * @brief Unicasts the message to each remote node in the binding table that
 * matches the cluster and source endpoint in the APS frame.  Note: if the
 * binding table contains many matching entries, calling this API cause a
 * significant amount of network traffic.
 */
EmberStatus emberAfSendUnicastToBindings(EmberApsFrame *apsFrame,
                                         int16u messageLength,
                                         int8u* message);

/**
 * @brief Sends interpan message.
 */
EmberStatus emberAfSendInterPan(EmberPanId panId,
                                const EmberEUI64 destinationLongId,
                                EmberNodeId destinationShortId,
                                EmberMulticastId multicastId,
                                EmberAfClusterId clusterId,
                                EmberAfProfileId profileId,
                                int16u messageLength,
                                int8u* messageBytes);

/**
 * @brief Sends end device binding request.
 */
EmberStatus emberAfSendEndDeviceBind(int8u endpoint);

/**
 * @brief Sends the command prepared with emberAfFill.... macro.
 *
 * This function is used to send a command that was previously prepared
 * using the emberAfFill... macros from the client command API. It
 * will be sent as unicast to each remote node in the binding table that
 * matches the cluster and source endpoint in the APS frame.  Note: if the
 * binding table contains many matching entries, calling this API cause a
 * significant amount of network traffic.
 */
EmberStatus emberAfSendCommandUnicastToBindings(void);

/**
 * @brief Sends the command prepared with emberAfFill.... macro.
 *
 * This function is used to send a command that was previously prepared
 * using the emberAfFill... macros from the client command API. It
 * will be sent as multicast.
 */
EmberStatus emberAfSendCommandMulticast(EmberMulticastId multicastId);

/**
 * @brief Sends the command prepared with emberAfFill.... macro.
 *
 * This function is used to send a command that was previously prepared
 * using the emberAfFill... macros from the client command API.
 * It will be sent as unicast.
 */
EmberStatus emberAfSendCommandUnicast(EmberOutgoingMessageType type,
                                      int16u indexOrDestination);

/**
 * @brief Sends the command prepared with emberAfFill.... macro.
 *
 * This function is used to send a command that was previously prepared
 * using the emberAfFill... macros from the client command API.
 */
EmberStatus emberAfSendCommandBroadcast(EmberNodeId destination);

/**
 * @brief Sends the command prepared with emberAfFill.... macro.
 *
 * This function is used to send a command that was previously prepared
 * using the emberAfFill... macros from the client command API.
 * It will be sent via inter-PAN.  If destinationLongId is not NULL, the message
 * will be sent to that long address using long addressing mode; otherwise, the
 * message will be sent to destinationShortId using short address mode.  IF
 * multicastId is not zero, the message will be sent using multicast mode.
 */
EmberStatus emberAfSendCommandInterPan(EmberPanId panId,
                                       const EmberEUI64 destinationLongId,
                                       EmberNodeId destinationShortId,
                                       EmberMulticastId multicastId,
                                       EmberAfProfileId profileId);

/**
 * @brief Sends a default response to a cluster command.
 *
 * This function is used to prepare and send a default response to a cluster
 * command.
 *
 * @param cmd The cluster command to which to respond.
 * @param status Status code for the default response command.
 * @return An ::EmberStatus value that indicates the success or failure of
 * sending the response.
 */
EmberStatus emberAfSendDefaultResponse(const EmberAfClusterCommand *cmd,
                                       EmberAfStatus status);
                                       
/**
 * @brief Sends a default response to a cluster command using the 
 * current command.
 *
 * This function is used to prepare and send a default response to a cluster
 * command.
 *
 * @param status Status code for the default response command.
 * @return An ::EmberStatus value that indicates the success or failure of
 * sending the response.
 */
EmberStatus emberAfSendImmediateDefaultResponse(EmberAfStatus status);

/**
 * @brief Returns the maximum size of the payload that the Application
 * Support sub-layer will accept for the given message type, destination, and
 * APS frame.
 *
 * The size depends on multiple factors, including the security level in use
 * and additional information added to the message to support the various
 * options.
 *
 * @param type The outgoing message type.
 * @param indexOrDestination Depending on the message type, this is either the
 *  EmberNodeId of the destination, an index into the address table, an index
 *  into the binding table, the multicast identifier, or a broadcast address.
 * @param apsFrame The APS frame for the message.
 * @return The maximum APS payload length for the given message.
 */
int8u emberAfMaximumApsPayloadLength(EmberOutgoingMessageType type,
                                     int16u indexOrDestination,
                                     EmberApsFrame *apsFrame);

/**
 * @brief Access to client API aps frame.
 */
EmberApsFrame *emberAfGetCommandApsFrame(void);

/**
 * @brief Set the source and destination endpoints in the client API APS frame.
 */
void emberAfSetCommandEndpoints(int8u sourceEndpoint, int8u destinationEndpoint);

/**
 * @brief Friendly define for use in discovering client clusters with
 * ::emberAfFindDevicesByProfileAndCluster().
 */
#define EMBER_AF_CLIENT_CLUSTER_DISCOVERY FALSE

/**
 * @brief Friendly define for use in discovering server clusters with
 * ::emberAfFindDevicesByProfileAndCluster().
 */
#define EMBER_AF_SERVER_CLUSTER_DISCOVERY TRUE

/**
 * @brief Use this function to find devices in the network with endpoints
 *   matching a given profile ID and cluster ID in their descriptors.
 *   Target may either be a specific device, or the broadcast
 *   address EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS.
 *
 * With this function a service discovery is initiated and received
 * responses are returned by executing the callback function passed in.
 * For unicast discoveries, the callback will be executed only once.
 * Either the target will return a result or a timeout will occur.
 * For broadcast discoveries, the callback may be called multiple times
 * and after a period of time the discovery will be finished with a final
 * call to the callback.
 * 
 * @param target The destination node ID for the discovery; either a specific
 *  node's ID or EMBER_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS.
 * @param profileId The application profile for the cluster being discovered.
 * @param clusterId The cluster being discovered.
 * @param serverCluster EMBER_AF_SERVER_CLUSTER_DISCOVERY (TRUE) if discovering
 *  servers for the target cluster; EMBER_AF_CLIENT_CLUSTER_DISCOVERY (FALSE)
 *  if discovering clients for that cluster.
 * @param callback Function pointer for the callback function triggered when
 *  a match is discovered.  (For broadcast discoveries, this is called once per
 *  matching node, even if a node has multiple matching endpoints.)
 */
EmberStatus emberAfFindDevicesByProfileAndCluster(EmberNodeId target,
                                                  EmberAfProfileId profileId,
                                                  EmberAfClusterId clusterId,
                                                  boolean serverCluster,
                                                  EmberAfServiceDiscoveryCallback *callback);


/**
 * @brief Use this function to find all of the given in and out clusters
 *   implemented on a devices given endpoint. Target should only be the 
 *   short address of a specific device.
 * 
 * With this function a single service discovery is initiated and the response
 * is passed back to the passed callback.
 *
 * @param target The destination node ID for the discovery. This should be a
 *  specific node's ID and should not be a broadcast address.
 * @param targetEndpoint The endpoint to target with the discovery process.
 * @param callback Function pointer for the callback function triggered when
  *  the discovery is returned.
 */
EmberStatus emberAfFindClustersByDeviceAndEndpoint(EmberNodeId target,
                                                   int8u targetEndpoint,
                                                   EmberAfServiceDiscoveryCallback *callback);

/** 
 * @brief Use this function to initiate a discovery for the IEEE address
 *   of the specified node id.  This will send a unicast sent to the target
 *   node ID.
 */
EmberStatus emberAfFindIeeeAddress(EmberNodeId shortAddress,
                                   EmberAfServiceDiscoveryCallback *callback);

/**
 * @brief Use this function to initiate a discovery for the short ID of the
 *   specified long address.  This will send a broadcast to all
 *   rx-on-when-idle devices (non-sleepies).
 */
EmberStatus emberAfFindNodeId(EmberEUI64 longAddress,
                              EmberAfServiceDiscoveryCallback *callback);

/**
 * @brief Use this function to add an entry for a remote device to the address
 * table.
 *
 * If the EUI64 already exists in the address table, the index of the existing
 * entry will be returned.  Otherwise, a new entry will be created and the new
 * new index will be returned.  The framework will remember how many times the
 * the returned index has been referenced.  When the address table entry is no
 * longer needed, the application should remove its reference by calling
 * ::emberAfRemoveAddressTableEntry.
 *
 * @param longId The EUI64 of the remote device.
 * @param shortId The node id of the remote device or ::EMBER_UNKNOWN_NODE_ID
 * if the node id is currently unknown.
 * @return The index of the address table entry for this remove device or
 * ::EMBER_NULL_ADDRESS_TABLE_INDEX if an error occurred (e.g., the address
 * table is full).
 */
int8u emberAfAddAddressTableEntry(EmberEUI64 longId, EmberNodeId shortId);

/**
 * @brief Use this function to add an entry for a remote device to the address
 * table at a specific location.
 *
 * The framework will remember how many times an address table index has been
 * referenced through ::emberAfAddAddressTableEntry.  If the reference count
 * for the index passed to this function is not zero, the entry will be not
 * changed.   When the address table entry is no longer needed, the application
 * should remove its reference by calling ::emberAfRemoveAddressTableEntry.
 *
 * @param index The index of the address table entry.
 * @param longId The EUI64 of the remote device.
 * @param shortId The node id of the remote device or ::EMBER_UNKNOWN_NODE_ID
 * if the node id is currently unknown.
 * @return ::EMBER_SUCCESS if the address table entry was successfully set,
 * ::EMBER_ADDRESS_TABLE_ENTRY_IS_ACTIVE if any messages are being sent using
 * the existing entry at that index or the entry is still referenced in the
 * framework, or ::EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE if the index is out
 * of range.
 */
EmberStatus emberAfSetAddressTableEntry(int8u index,
                                        EmberEUI64 longId,
                                        EmberNodeId shortId);

/**
 * @brief Use this function to remove a specific entry from the address table.
 *
 * The framework will remember how many times an address table index has been
 * referenced through ::emberAfAddAddressTableEntry and
 * ::emberAfSetAddressTableEntry.  The address table entry at this index will
 * not actually be removed until its reference count reaches zero.
 *
 * @param index The index of the address table entry.
 * @return ::EMBER_SUCCESS if the address table entry was successfully removed
 * or ::EMBER_ADDRESS_TABLE_INDEX_OUT_OF_RANGE if the index is out of range.
 */
EmberStatus emberAfRemoveAddressTableEntry(int8u index);

/**
 * @brief Use this macro to retrive the current command. This
 * macro may only be used within the command parsing context. For instance
 * Any of the command handling callbacks may use this macro. If this macro
 * is used outside the command context, the returned EmberAfClusterCommand pointer
 * will be null.
 */
#define emberAfCurrentCommand() (emAfCurrentCommand)
extern EmberAfClusterCommand *emAfCurrentCommand;

/**
 * @brief returns the current endpoint that is being served.
 *
 * The purpose of this macro is mostly to access endpoint that
 * is being served in the command callbacks.
 */
#define emberAfCurrentEndpoint() (emberAfCurrentCommand()->apsFrame->destinationEndpoint)

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Use this function to initiate key establishment with a remote node.
 * ::emberAfKeyEstablishmentCallback will be called as events occur and when
 * key establishment completes.
 *
 * @param nodeId The node id of the remote device.
 * @param endpoint The endpoint on the remote device.
 * @return ::EMBER_SUCCESS if key establishment was initiated successfully
 */
EmberStatus emberAfInitiateKeyEstablishment(EmberNodeId nodeId, int8u endpoint);

/** @brief Use this function to initiate key establishment wih a remote node on
 * a different PAN.  ::emberAfInterPanKeyEstablishmentCallback will be called
 * as events occur and when key establishment completes.
 *
 * @param panId The PAN id of the remote device.
 * @param eui64 The EUI64 of the remote device.
 * @return ::EMBER_SUCCESS if key establishment was initiated successfully
 */
EmberStatus emberAfInitiateInterPanKeyEstablishment(EmberPanId panId,
                                                    const EmberEUI64 eui64);

/** @brief Use this function to tell if the device is in the process of
 * performing key establishment.
 *
 * @return ::TRUE if key establishment is in progress.
 */
boolean emberAfPerformingKeyEstablishment(void);

/** @brief Use this function to initiate partner link key exchange with a
 * remote node.
 *
 * @param target The node id of the remote device.
 * @param endpoint The key establishment endpoint of the remote device.
 * @param callback The callback that should be called when the partner link
 * key exchange completes.
 * @return ::EMBER_SUCCESS if the partner link key exchange was initiated
 * successfully.
 */
EmberStatus emberAfInitiatePartnerLinkKeyExchange(EmberNodeId target,
                                                  int8u endpoint,
                                                  EmberAfPartnerLinkKeyExchangeCallback *callback);
#else
  #define emberAfInitiateKeyEstablishment(nodeId, endpoint) \
    emberAfInitiateKeyEstablishmentCallback(nodeId, endpoint)
  #define emberAfInitiateInterPanKeyEstablishment(panId, eui64) \
    emberAfInitiateInterPanKeyEstablishmentCallback(panId, eui64)
  #define emberAfPerformingKeyEstablishment() \
    emberAfPerformingKeyEstablishmentCallback()
  #define emberAfInitiatePartnerLinkKeyExchange(target, endpoint, callback) \
    emberAfInitiatePartnerLinkKeyExchangeCallback(target, endpoint, callback)
#endif

/** @brief Use this function to determine if the security profile of the
 * current network was set to Smart Energy.  The security profile is configured
 * in AppBuilder.
 @ return TRUE if the security profile is Smart Energy or FALSE otherwise.
 */
boolean emberAfIsCurrentSecurityProfileSmartEnergy(void);

/** @} END Messaging */


/** @name ZCL macros */
// @{
// Frame control fields (8 bits total)
// Bits 0 and 1 are Frame Type Sub-field
#define ZCL_FRAME_CONTROL_FRAME_TYPE_MASK     (BIT(0)|BIT(1))
#define ZCL_CLUSTER_SPECIFIC_COMMAND          BIT(0)
#define ZCL_PROFILE_WIDE_COMMAND              0
// Bit 2 is Manufacturer Specific Sub-field
#define ZCL_MANUFACTURER_SPECIFIC_MASK        BIT(2)
// Bit 3 is Direction Sub-field
#define ZCL_FRAME_CONTROL_DIRECTION_MASK      BIT(3)
#define ZCL_FRAME_CONTROL_SERVER_TO_CLIENT    BIT(3)
#define ZCL_FRAME_CONTROL_CLIENT_TO_SERVER    0
// Bit 4 is Disable Default Response Sub-field
#define ZCL_DISABLE_DEFAULT_RESPONSE_MASK     BIT(4)
// Bits 5 to 7 are reserved

// Packet must be at least 3 bytes for ZCL overhead.
//   Frame Control (1-byte)
//   Sequence Number (1-byte)
//   Command Id (1-byte)
#define EMBER_AF_ZCL_OVERHEAD                       3
#define EMBER_AF_ZCL_MANUFACTURER_SPECIFIC_OVERHEAD 5

/** @} END ZCL macros */


/** @name Network utility functions */
// ${

/** @brief Use this function to form a new network using the specified network
 * parameters.
 *
 * @param parameters Specification of the new network.
 * @return An ::EmberStatus value that indicates either the successful formation
 * of the new network or the reason that the network formation failed.
 */
EmberStatus emberAfFormNetwork(EmberNetworkParameters *parameters);

/** @brief Use this function to associate with the network using the specified
 * network parameters.
 *
 * @param parameters Specification of the network with which the node should
 * associate.
 * @return An ::EmberStatus value that indicates either that the association
 * process began successfully or the reason for failure.
 */
EmberStatus emberAfJoinNetwork(EmberNetworkParameters *parameters);

#ifdef DOXYGEN_SHOULD_SKIP_THIS
/** @brief Use this function to find an unused PAN id and form a new network.
 *
 * @return An ::EmberStatus value that indicates either the process begin
 * successfully or the reason for failure.
 */
EmberStatus emberAfFindUnusedPanIdAndForm(void);
/** @brief Use this function to find a joinable network and join it.
 *
 * @return An ::EmberStatus value that indicates either the process begin
 * successfully or the reason for failure.
 */
EmberStatus emberAfStartSearchForJoinableNetwork(void);
#else
  #define emberAfFindUnusedPanIdAndForm()        emberAfFindUnusedPanIdAndFormCallback()
  #define emberAfStartSearchForJoinableNetwork() emberAfStartSearchForJoinableNetworkCallback()
#endif

/** @brief Sets the current network to that of the given index and adds it to
 * the stack of networks maintained by the framework.  Every call to this API
 * must be paired with a subsequent call to ::emberAfPopNetworkIndex.
 */
EmberStatus emberAfPushNetworkIndex(int8u networkIndex);
/** @brief Sets the current network to the callback network and adds it to
 * the stack of networks maintained by the framework.  Every call to this API
 * must be paired with a subsequent call to ::emberAfPopNetworkIndex.
 */
EmberStatus emberAfPushCallbackNetworkIndex(void);
/** @brief Sets the current network to that of the given endpoint and adds it
 * to the stack of networks maintained by the framework.  Every call to this
 * API must be paired with a subsequent call to ::emberAfPopNetworkIndex.
 */
EmberStatus emberAfPushEndpointNetworkIndex(int8u endpoint);
/** @brief Removes the topmost network from the stack of networks maintained by
 * the framework and sets the current network to the new topmost network.
 * Every call to this API must be paired with a prior call to
 * ::emberAfPushNetworkIndex, ::emberAfPushCallbackNetworkIndex, or
  * ::emberAfPushEndpointNetworkIndex.
 */
EmberStatus emberAfPopNetworkIndex(void);
/** @brief Returns the primary endpoint of the given network index or 0xFF if
 * no endpoints belong to the network.
 */
int8u emberAfPrimaryEndpointForNetworkIndex(int8u networkIndex);
/** @brief Returns the primary endpoint of the current network index or 0xFF if
 * no endpoints belong to the current network.
 */
int8u emberAfPrimaryEndpointForCurrentNetworkIndex(void);
EmberStatus emAfInitializeNetworkIndexStack(void);
void emAfAssertNetworkIndexStackIsEmpty(void);

/** @} End network utility functions */

/** @} END addtogroup */

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
  #if defined(EMBER_TEST)
    #define EMBER_TEST_ASSERT(x) assert(x)
  #else
    #define EMBER_TEST_ASSERT(x)
  #endif
#endif


#endif // __AF_API__
