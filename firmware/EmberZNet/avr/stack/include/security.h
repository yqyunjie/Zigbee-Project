/**
 * @file security.h
 * @brief EmberZNet security API.
 * See @ref security for documentation.
 *
 * <!--Copyright 2004-2007 by Ember Corporation. All rights reserved.    *80*-->
 */

/**
 * @addtogroup security
 *
 * This file describes the functions necessary to manage security for a
 * regular device.  There are three major modes for security and applications
 * should link in the appropriate library:
 * - Residential security uses only network keys.  This is the only supported
 *     option for ZigBee 2006 devices.
 * - Standard security uses network keys with optional link keys.  Ember
 *     strongly recommends using Link Keys.  It is possible for 2006 devices
 *     to run on a network that uses Standard Security.
 * - High Security uses network keys and requires link keys derived via SKKE.
 *     Devices that do not support link keys (i.e. 2006 devices) are not 
 *     allowed.  High Security also uses Entity Authentication to synchronize
 *     frame counters between neighboring devices and children.
 *
 * See security.h for source code.
 * @{
 */

#include "stack/include/trust-center.h"

/** @brief This function sets the initial security state that will be used by 
 * the device when it forms or joins a network.  If security is enabled then
 * this function <b>must</b> be called prior to forming or joining the network.
 * It must also be called if the device left the network and wishes to form 
 * or join another network.
 *
 * The call may be used by either the Trust Center or non Trust Center devices,
 * the options that are set are different depending on which role the device
 * will assume.  See the ::EmberInitialSecurityState structure for more
 * explanation about the various security settings.
 *
 * The function will return ::EMBER_SECURITY_CONFIGURATION_INVALID
 * in the following cases:
 *   <ul>
 *   <li>Distributed Trust Center Mode was enabled with Hashed Link Keys.</li>
 *   <li>High Security was specified.</li>
 * </ul>
 *
 * @param state The security configuration to be set.
 *
 * @return An ::EmberStatus value.  ::EMBER_SUCCESS if the security
 *   state has been set successfully.  ::EMBER_INVALID_CALL if the device
 *   is not in the ::EMBER_NO_NETWORK state.  The value 
 *   ::EMBER_SECURITY_CONFIGURATION_INVALID is returned if the combination
 *   of security parameters is not valid.  ::EMBER_KEY_INVALID is returned
 *   if a reserved or invalid key value was passed in the key structure for
 *   one of the keys.
 */
EmberStatus emberSetInitialSecurityState(EmberInitialSecurityState* state);


/**
 * @brief A non-Trust Center Device configuration bitmask example.
 *    There is no Preconfigured Link Key, so the NWK key is expected to
 *    be sent in-the-clear.  The device will request a Trust Center Link key
 *    after getting the Network Key.  
 */
#define EMBER_JOIN_NO_PRECONFIG_KEY_BITMASK \
  ( EMBER_STANDARD_SECURITY_MODE            \
    | EMBER_GET_LINK_KEY_WHEN_JOINING )

/**
 * @brief A non-Trust Center device configuration bitmask example.
 *   The device has a Preconfigured Link Key and expects to receive
 *   a NWK Key encrypted at the APS Layer.  A NWK key sent in-the-clear
 *   will be rejected.
 */
#define EMBER_JOIN_PRECOFIG_KEY_BITMASK \
  ( EMBER_STANDARD_SECURITY_MODE        \
    | EMBER_HAVE_PRECONFIGURED_KEY      \
    | EMBER_REQUIRE_ENCRYPTED_KEY )


/** @brief Gets the security state that is being used by a device joined
 *  into the Network.
 *
 * @param state A pointer to an ::EmberRunningSecurityState value into which 
 * the security configuration will be copied.
 *
 * @return ::EMBER_SUCCESS if the security settings were successfully retrieved.
 *   ::EMBER_NOT_JOINED if the device is not currently joined in the network.
 */
EmberStatus emberGetCurrentSecurityState(EmberCurrentSecurityState* state);

/** @brief Gets the specified key and its associated data.  This can retrieve
 *  the Trust Center Link Key, Current Network Key, or Next Network Key.
 *
 *  @param type The Type of key to get (e.g. Trust Center Link or Network).
 *  @param keyStruct A pointer to the ::EmberKeyStruct data structure that will
 *     be populated with the pertinent information.
 *  
 *  @return ::EMBER_SUCCESS if the key was retrieved successfully.
 *    ::EMBER_INVALID_CALL if an attempt was made to retrieve an
 *    ::EMBER_APPLICATION_LINK_KEY or ::EMBER_APPLICATION_MASTER_KEY.
 */
EmberStatus emberGetKey(EmberKeyType type,
                        EmberKeyStruct* keyStruct);


/** @brief Returns TRUE if a link key is available for securing messages
 * sent to the remote device.
 *
 * @param remoteDevice The long address of a some other device in the network.
 * @return boolean Returns TRUE if a link key is available.
 */
boolean emberHaveLinkKey(EmberEUI64 remoteDevice);

#ifndef DOXYGEN_SHOULD_SKIP_THIS
extern int8u emberKeyTableSize;
extern int32u* emIncomingApsFrameCounters;
#endif

/** @brief Generates a Random Key (link, network, or master) and returns
 * the result. 
 *
 * It copies the key into the array that \c result points to.  
 * This is an time-expensive operation as it needs to obtain truly
 * random numbers.
 *
 * @param keyAddress  A pointer to the location in which to copy the
 * generated key.
 *
 * @return ::EMBER_SUCCESS on success, ::EMBER_INSUFFICIENT_RANDOM_DATA
 *   on failure.
 */
EmberStatus emberGenerateRandomKey(EmberKeyData* keyAddress);

/** @brief A callback to inform the application that the Network Key
  * has been updated and the node has been switched over to use the new
  * key.  The actual key being used is not passed up, but the sequence number
  * is. 
  *
  * @param sequenceNumber  The sequence number of the new network key.
  */
void emberSwitchNetworkKeyHandler(int8u sequenceNumber);

/** @brief A function to request a Link Key from the
 *  Trust Center with another device device on the Network (which could
 *  be the Trust Center).  A Link Key with the Trust Center is possible
 *  but the requesting device cannot be the Trust Center.
 *  Link Keys are optional in ZigBee Standard Security and thus the stack
 *  cannot know whether the other device supports them.
 *
 *  If the partner device is the Trust Center, then only that device
 *  needs to request the key.  The Trust Center will immediately respond to
 *  those requests and send the key back to the device.
 *
 *  If the partner device is not the Trust Center, then both devices
 *  must request an Application Link Key with each other.  The requests
 *  will be sent to the Trust Center for it to answer.  The Trust
 *  Center will store the first request and wait ::EMBER_REQUEST_KEY_TIMEOUT
 *  for the second request to be received.  The Trust Center only supports one
 *  outstanding Application key request at a time and therefore will ignore 
 *  other requests that are not associated with the first request.  
 *
 *  Sleepy devices should poll at a higher rate until a response
 *  is received or the request times out.
 *
 *  The success or failure of the request is returned via
 *  ::emberZigbeeKeyEstablishmentHandler(...)
 *
 * @param partner The IEEE address of the partner device.  If NULL is passed
 *    in then the Trust Center IEEE Address is assumed.
 * @return EMBER_SUCCESS if the call succeeds, or EMBER_NO_BUFFERS.
 */
EmberStatus emberRequestLinkKey(EmberEUI64 partner);

/** @brief A callback to the application to notify it of the status
 *  of the request for a Link Key.  The application should define
 *  EMBER_APPLICATION_HAS_ZIGBEE_KEY_ESTABLISHMENT_HANDLER in order to implement
 *  its own handler.
 *
 * @param partner The IEEE address of the partner device.  Or all zeros if
 *   the Key establishment failed.
 * @param status The status of the key establishment.
 */
void emberZigbeeKeyEstablishmentHandler(EmberEUI64 partner, EmberKeyStatus status);


/** @brief A function used to obtain data from the Key Table.  
 *
 * @param index The index in the key table of the entry to get.
 * @param result A pointer to the location of an ::EmberKeyStruct that will 
 *   contain the results retrieved by the stack.
 * @return ::EMBER_KEY_INVALID if the index is an erased key entry.
 *   ::EMBER_INDEX_OUT_OF_RANGE if the passed index is not valid.
 *   ::EMBER_SUCCESS on success.
 */
EmberStatus emberGetKeyTableEntry(int8u index, 
                                  EmberKeyStruct* result);

/** @brief A function to set an entry in the key table.
 *
 * @param index The index in the key table of the entry to set.
 * @param address The address of the partner device associated with the key.
 * @param keyData A pointer to the key data associated with the key entry.
 * @param linkKey A boolean indicating whether this is a Link or Master Key.
 *
 * @return ::EMBER_KEY_INVALID if the passed key data is using one of the
 *   reserved key values.  ::EMBER_INDEX_OUT_OF_RANGE if passed index is not
 *   valid.  ::EMBER_SUCCESS on success.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberSetKeyTableEntry(int8u index, 
                                  EmberEUI64 address,
                                  boolean linkKey,
                                  EmberKeyData* keyData);
#else
EmberStatus emSetKeyTableEntry(boolean erase,
                               int8u index, 
                               EmberEUI64 address,
                               boolean linkKey,
                               EmberKeyData* keyData);
#define emberSetKeyTableEntry(index, address, linkKey, keyData) \
  emSetKeyTableEntry(FALSE, index, address, linkKey, keyData)
#endif

/** @brief This function add a new entry in the key table or
 *  updates an existing entry with a new key.  It first searches
 *  the key table for an entry that has a matching EUI64.  If
 *  it does not find one it searches for the first free entry.
 *  If it is successful in either case, it sets the entry with
 *  the EUI64, key data, and flag that indicates if it is a Link
 *  or Master Key.  The Incoming Frame Counter for that key is
 *  also reset to 0.  If no existing entry was found, and there was
 *  not a free entry in the table, then the call will fail.
 *
 * @param address The IEEE Address of the partner device that shares
 *   the key.
 * @param linkKey A boolean indicating whether this is a Link or Master
 *   key.
 * @param keyData A pointer to the actual key data.
 * 
 * @return ::EMBER_TABLE_FULL if no free entry was found to add.
 *   ::EMBER_KEY_INVALID if the passed key was a reserved value.
 *   ::EMBER_KEY_TABLE_ADDRESS_NOT_VALID if the passed address is
 *   reserved or invalid.
 *   ::EMBER_SUCCESS on success.
 */ 
EmberStatus emberAddOrUpdateKeyTableEntry(EmberEUI64 address,
                                      boolean linkKey,
                                      EmberKeyData* keyData);

/** @brief A function to search the key table and find an entry
 *  matching the specified IEEE address and key type.
 *
 * @param address The IEEE Address of the partner device that shares
 *   the key.  To find the first empty entry pass in an address
 *   of all zeros.
 * @param linkKey A boolean indicating whether to search for an entry
 *   containing a Link or Master Key.
 * @return The index that matches the search criteria, or 0xFF if
 *   no matching entry was found.
 */
int8u emberFindKeyTableEntry(EmberEUI64 address, 
                             boolean linkKey);

/** @brief A function to clear a single entry in the key table.
 *
 * @param index The index in the key table of the entry to erase.
 *
 * @return TRUE if the index is valid and the data was erased.
 *   FALSE otherwise.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberEraseKeyTableEntry(int8u index);
#else
#define emberEraseKeyTableEntry(index) \
  emSetKeyTableEntry(TRUE, (index), NULL, FALSE, NULL)
#endif


// @} END addtogroup

/**
 * <!-- HIDDEN
 * @page 2p5_to_3p0
 * <hr>
 * The entire file security.h is new and is described in @ref security.
 * <ul>
 * <li> <b>New items</b>
 * <ul>
 * <li>
 * </ul>
 * <li> <b>Changed items</b>
 * <ul>
 * <li>
 * </ul>
 * <li> <b>Removed items</b>
 * <ul>
 * <li>
 * </ul>
 * </ul>
 * -->
 */
