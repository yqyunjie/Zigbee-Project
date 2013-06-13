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
 * This call <b>should not</b> be used when restoring prior network operation
 * from saved state via ::emberNetworkInit as this will cause saved 
 * security settings and keys table from the prior network to be erased,
 * resulting in improper keys and/or frame counter values being used, which will
 * prevent proper communication with other devices in the network.
 * Calling ::emberNetworkInit is sufficient to restore all saved security
 * settings after a reboot and re-enter the network.
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

/** @brief Sets the extended initial security bitmask.
 *
 * @param mask An object of type ::EmberExtendedSecurityBitmask that indicates
 * what the extended security bitmask should be set to.
 *
 * @return ::EMBER_SUCCESS if the security settings were successfully retrieved.
 *   ::EMBER_INVALID_CALL otherwise.
 */
EmberStatus emberSetExtendedSecurityBitmask(EmberExtendedSecurityBitmask mask);

/** @brief Gets the extended security bitmask that is being used by a device.
 *
 * @param mask A pointer to an ::EmberExtendedSecurityBitmask value into which
 * the extended security bitmask will be copied.
 *
 * @return ::EMBER_SUCCESS if the security settings were successfully retrieved.
 *   ::EMBER_INVALID_CALL otherwise.
 */
EmberStatus emberGetExtendedSecurityBitmask(EmberExtendedSecurityBitmask* mask);


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
#define EMBER_JOIN_PRECONFIG_KEY_BITMASK \
  ( EMBER_STANDARD_SECURITY_MODE        \
    | EMBER_HAVE_PRECONFIGURED_KEY      \
    | EMBER_REQUIRE_ENCRYPTED_KEY )


/** @brief Gets the security state that is being used by a device joined
 *  into the Network.
 *
 * @param state A pointer to an ::EmberCurrentSecurityState value into which 
 * the security configuration will be copied.
 *
 * @return ::EMBER_SUCCESS if the security settings were successfully retrieved.
 *   ::EMBER_NOT_JOINED if the device is not currently joined in the network.
 */
EmberStatus emberGetCurrentSecurityState(EmberCurrentSecurityState* state);

/** @brief Gets the specified key and its associated data.  This can retrieve
 *  the Trust Center Link Key, Current Network Key, or Next Network Key.
 *  On the 35x series chips, the data returned by this call is governed by the
 *  security policy set in the manufacturing token for TOKEN_MFG_SECURITY_CONFIG.
 *  See the API calls ::emberSetMfgSecurityConfig() and ::emberGetMfgSecurityConfig()
 *  for more information.  If the security policy is not set to
 *  ::EMBER_KEY_PERMISSIONS_READING_ALLOWED, then the actual encryption key
 *  value will not be returned.  Other meta-data about the key will be returned.
 *  The 2xx series chips have no such restrictions.
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
 *  On the 35x series chips, the data returned by this call is governed by the
 *  security policy set in the manufacturing token for TOKEN_MFG_SECURITY_CONFIG.
 *  See the API calls ::emberSetMfgSecurityConfig() and ::emberGetMfgSecurityConfig()
 *  for more information.  If the security policy is not set to
 *  ::EMBER_KEY_PERMISSIONS_READING_ALLOWED, then the actual encryption key
 *  value will not be returned.  Other meta-data about the key will be returned.
 *  The 2xx series chips have no such restrictions.
 *
 * @param index The index in the key table of the entry to get.
 * @param result A pointer to the location of an ::EmberKeyStruct that will 
 *   contain the results retrieved by the stack.
 * @return ::EMBER_TABLE_ENTRY_ERASED if the index is an erased key entry.
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
 * @return ::EMBER_SUCCESS if the index is valid and the key data was erased.
 *   ::EMBER_KEY_INVALID if the index is out of range for the size of
 *   the key table.
 */
#if defined DOXYGEN_SHOULD_SKIP_THIS
EmberStatus emberEraseKeyTableEntry(int8u index);
#else
#define emberEraseKeyTableEntry(index) \
  emSetKeyTableEntry(TRUE, (index), NULL, FALSE, NULL)
#endif


/**
 * @brief This function clears the key table of the current network.
 *
 * @return ::EMBER_SUCCESS if the key table was successfully cleared.
 *   ::EMBER_INVALID_CALL otherwise.
 */
EmberStatus emberClearKeyTable(void);

/** @brief This function suppresses normal write operations of the stack
 *    tokens.  This is only done in rare circumstances when the device already
 *    has network parameters but needs to conditionally rejoin a network in order to
 *    perform a security message exchange (i.e. key establishment).  If the
 *    network is not authenticated properly, it will need to forget any stack 
 *    data it used and return to the old network.  By suppressing writing
 *    of the stack tokens the device will not have stored any persistent data 
 *    about the network and a reboot will clear the RAM copies.  The Smart Energy 
 *    profile feature Trust Center Swap-out uses this in order to securely replace
 *    the Trust Center and re-authenticate to it.
 *
 * @return ::EMBER_SUCCESS if it could allocate temporary buffers to
 *    store network information.  ::EMBER_NO_BUFFERS otherwise.
 */
EmberStatus emberStopWritingStackTokens(void);

/** @brief This function will immediately write the value of stack tokens
 *    and then resume normal network operation by writing the stack tokens
 *    at appropriate intervals or changes in state.  It has no effect unless
 *    a previous call was made to ::emberStopWritingStackTokens().  
 *
 *  @return ::EMBER_SUCCESS if it had previously unwritten tokens from a call to
 *    ::emberStopWritingStackTokens() that it now wrote to the token system.
 *    ::EMBER_INVALID_CALL otherwise.
 */
EmberStatus emberStartWritingStackTokens(void);

/** @brief This function will determine whether stack tokens will be written
 *    to persistent storage when they change.  By default it is set to TRUE
 *    meaning the stack will update its internal tokens via HAL calls when the
 *    associated RAM values change.
 *
 * @return TRUE if the device will update the persistent storage for tokens
 *    when values in RAM change.  FALSE otherwise.
 */
boolean emberWritingStackTokensEnabled(void);

/** @brief This function performs APS encryption/decryption of messages
 *    directly.  Normally the stack handles all APS encryption/decryption
 *    automatically and there is no need to call this function.  If APS data
 *    is sent or received via some other means (such as over interpan) then
 *    APS encryption/decryption must be done manually.  If decryption
 *    is performed then the Auxiliary security header and MIC will be removed
 *    from the message.  If encrypting, then the auxiliary header and
 *    MIC will be added to the message.
 *    This is only available on SOC platforms.
 *
 * @param encrypt a boolean indicating whether perform encryption (TRUE) or
 *   decryption (FALSE).
 *
 * @param buffer An ::EmberMessageBuffer containing the APS frame to decrypt
 *   or encrypt.
 *
 * @param apsHeaderEndIndex The index into the buffer where the APS header ends.
 *   If encryption is being performed this should point to the APS payload, where
 *   an Auxiliary header will be inserted.  If decryption is being
 *   performed, this should point to the start of the Auxiliary header frame.
 *
 * @param remoteEui64 the ::EmberEUI64 of the remote device the message was
 *   received from (decryption) or being sent to (encryption).  
 *
 * @return ::EMBER_SUCCESS if encryption/decryption was performed successfully.
 *   An appropriate ::EmberStatus code on failure.
 */
EmberStatus emberApsCryptMessage(boolean encrypt,
                                 EmberMessageBuffer buffer,
                                 int8u apsHeaderEndIndex,
                                 EmberEUI64 remoteEui64);


/** @brief This function will retrieve the security configuration stored
 *    in manufacturing tokens.  It is only available on the 35x series.
 *    See ::emberSetMfgSecurityConfig() for more details.
 * 
 *  @param settings A pointer to the ::EmberMfgSecurityStruct variable
 *    that will contain the returned data.
 *
 *  @return ::EMBER_SUCCESS if the tokens were successfully read.
 *    ::EmberStatus error code otherwise. 
 */
EmberStatus emberGetMfgSecurityConfig(EmberMfgSecurityStruct* settings);


/** @brief This function will set the security configuration to be stored
 *    in manufacturing tokens.  It is only available on the 35x series.
 *    This API must be called with care.  Once set, a manufacturing token
 *    CANNOT BE UNSET without using the ISA3 tools and connecting the chip
 *    via JTAG.  Additionally, a chip with read protection enabled cannot have 
 *    its configuration changed without a full chip erase.  Thus this provides
 *    a way to disallow access to the keys at runtime that cannot be undone.
 *
 *    To call this API the magic number must be passed in corresponding to
 *    ::EMBER_MFG_SECURITY_CONFIG_MAGIC_NUMBER.  This prevents accidental
 *    calls to this function when ::emberGetMfgSecurityConfig() was actually
 *    intended.
 *
 *    This function will disable external access to the actual
 *    key data used for decryption/encryption outside the stack.   
 *    Attempts to call ::emberGetKey() or ::emberGetKeyTableEntry() will 
 *    return the meta-data (e.g. sequence number, associated EUI64, frame
 *    counters) but the key value may be modified, see below.
 *
 *    The stack always has access to the actual key data.
 *
 *    If the ::EmberKeySettings within the ::EmberMfgSecurityStruct are set to 
 *    ::EMBER_KEY_PERMISSIONS_NONE then the key value with be set to zero
 *    when ::emberGetKey() or ::emberGetKeyTableEntry() is called.
 *    If the ::EmberKeySettings within the ::EmberMfgSecurityStruct are set to
 *    ::EMBER_KEY_PERMISSIONS_HASHING_ALLOWED, then the AES-MMO hash of the key
 *    will replace the actual key data when calls to ::emberGetKey() or
 *    ::emberGetKeyTableEntry() are made.
 *    If the ::EmberKeySettings within the ::EmberMfgSecurityStruct are set to
 *    ::EMBER_KEY_PERMISSIONS_READING_ALLOWED, then the actual key data is
 *    returned.  This is the default value of the token.
 *
 *  @param magicNumber A 32-bit value that must correspond to
 *    ::EMBER_MFG_SECURITY_CONFIG_MAGIC_NUMBER, otherwise ::EMBER_INVALID_CALL
 *    will be returned.
 *
 *  @param settings The security settings that are intended to be set by the
 *    application and written to manufacturing token.
 *
 *  @return ::EMBER_BAD_ARGUMENT if the passed magic number is invalid. 
 *    ::EMBER_INVALID_CALL if the chip does not support writing MFG tokens (i.e. em2xx)
 *    ::EMBER_SECURITY_CONFIGURATION_INVALID if there was an attempt to write an
 *    unerased manufacturing token (i.e. the token has already been set).
 */
EmberStatus emberSetMfgSecurityConfig(int32u magicNumber,
                                      const EmberMfgSecurityStruct* settings);



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
