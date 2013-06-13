// *****************************************************************************
// * esi-management.h
// *
// * It implements and manages the ESI table. The ESI table is shared among
// *   other plugins.
// *
// * Copyright 2011 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#ifndef EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE
#define EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE 3
#endif //EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE

#ifndef EMBER_AF_PLUGIN_ESI_MANAGEMENT_MIN_ERASING_AGE
#define EMBER_AF_PLUGIN_ESI_MANAGEMENT_MIN_ERASING_AGE 3
#endif //EMBER_AF_PLUGIN_ESI_MANAGEMENT_MIN_ERASING_AGE

#ifndef EMBER_AF_PLUGIN_ESI_MANAGEMENT_PLUGIN_CALLBACK_TABLE_SIZE
#define EMBER_AF_PLUGIN_ESI_MANAGEMENT_PLUGIN_CALLBACK_TABLE_SIZE 5
#endif // EMBER_AF_PLUGIN_ESI_MANAGEMENT_PLUGIN_CALLBACK_TABLE_SIZE

typedef struct {
  EmberEUI64 eui64;
  EmberNodeId nodeId;
  int8u networkIndex;
  int8u endpoint;
  int8u age; // Number of discovery cycles the ESI has not been discovered.
} EmberAfPluginEsiManagementEsiEntry;

typedef void(*EmberAfEsiManagementDeletionCallback)(int8u);

/**
 * It allows to search in the ESI table by the pair node (short id, endopoint).
 *
 * It returns a pointer to the entry if a matching entry was found, otherwise it
 * returns NULL.
 */
EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByShortIdAndEndpoint(EmberNodeId shortId,
                                                                                         int8u endpoint);

/**
 * It allows to search in the ESI table by the pair node (long id, endopoint).
 *
 * It returns a pointer to the entry if a matching entry was found, otherwise it
 * returns NULL.
 */
EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByLongIdAndEndpoint(EmberEUI64 longId,
                                                                                        int8u endpoint);

/**
 * It allows to retrieve the index of an entry that matches the passed short ID
 * and endpoint.
 *
 * It returns the index of the matching entry if a matching entry was found,
 * otherwise it returns 0xFF.
 */
int8u emberAfPluginEsiManagementIndexLookUpByShortIdAndEndpoint(EmberNodeId shortId,
                                                               int8u endpoint);

/**
 * It allows to retrieve the index of an entry that matches the passed long ID
 * and endpoint.
 *
 * It returns the index of the matching entry if a matching entry was found,
 * otherwise it returns 0xFF.
 */
int8u emberAfPluginEsiManagementIndexLookUpByLongIdAndEndpoint(EmberEUI64 longId,
                                                               int8u endpoint);

/**
 * It allows to search in the ESI table by the table index.
 *
 * It returns a pointer to the ESI entry stored at the index passed as
 * parameter.
 */
EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementEsiLookUpByIndex(int8u index);

/**
 * This function can be used as an iterator to go through the entries in the
 * table that are within a certain age threshold.
 *
 * If the passed pointer is NULL, it returns the first active entry with age
 * lesser or equal than the passed age parameter (if any). Otherwise it returns
 * the next active entry that satisfy the age requirement. If the are no entries
 * after the passed entry that satisfy the age requirement, it returns NULL.
 */
EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementGetNextEntry(EmberAfPluginEsiManagementEsiEntry* entry,
                                                                           int8u age);

/**
 * This function allows to obtain a free entry in the ESI table. It is the
 * requester responsibility to properly set all the fields in the obtained free
 * entry such as nodeId, age, etc. in order to avoid inconsistencies in the
 * table.
 *
 * Returns a free entry (if any), otherwise it clears the oldest entry whose age
 * is at least EMBER_AF_PLUGIN_ESI_MANAGEMENT_MIN_ERASING_AGE (if any) and
 * returns it, otherwise it returns NULL.
 */
EmberAfPluginEsiManagementEsiEntry* emberAfPluginEsiManagementGetFreeEntry(void);

/**
 * This function deletes the entry indicated by the parameter 'index' from the
 * ESI table.
 */
void emberAfPluginEsiManagementDeleteEntry(int8u index);

/**
 * This function increases the age of all the active entries in the table. A
 * non-active entry is an entry whose short ID is set to EMBER_NULL_NODE_ID.
 */
void emberAfPluginEsiManagementAgeAllEntries(void);

/**
 * This function clears the ESI table, i.e., it sets the short ID of each entry
 * to EMBER_NULL_NODE_ID.
 */
void emberAfPluginEsiManagementClearTable(void);

/**
 * This function allows a plugin to subscribe to ESI entries deletion
 * announcements, by passing its own deletion callback function. Upon an entry
 * deletion, all the deletion callback function are called passing the index
 * of the deleted entry.
 *
 * It returns TRUE if the subscription was successful, FALSE otherwise.
 */
boolean emberAfPluginEsiManagementSubscribeToDeletionAnnouncements(EmberAfEsiManagementDeletionCallback callback);

/**
 * This function performs the following steps:
 *  - Search for the source node of the passed command in the ESI table.
 *  - Adds a new entry in the ESI table if the source node is not present in the
 *    ESI table yet, or updates the current entry if needed.
 *
 *  @return The index of the source node of the passed command in the ESI
 *  table, or it returns 0xFF if the ESI was not present in the table and a new
 *  entry could not be added since the table was full.
 **/
int8u emberAfPluginEsiManagementUpdateEsiAndGetIndex(const EmberAfClusterCommand *cmd);


