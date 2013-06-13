// *******************************************************************
// * groups-server.c
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "groups-server-callback.h"

static boolean isGroupPresent(int8u endpoint, int16u groupId);

static boolean bindingGroupMatch(int8u endpoint,
                                 int16u groupId,
                                 EmberBindingTableEntry *entry);

static int8u findGroupIndex(int8u endpoint, int16u groupId);

void emberAfGroupsClusterServerInitCallback(int8u endpoint)
{
  // The high bit of Name Support indicates whether group names are supported.
  // Group names are not supported by this plugin.
  EmberAfStatus status;
  int8u nameSupport = (int8u) emberAfPluginGroupsServerGroupNamesSupportedCallback(endpoint);
  status = emberAfWriteAttribute(endpoint,
                                 ZCL_GROUPS_CLUSTER_ID,
                                 ZCL_GROUP_NAME_SUPPORT_ATTRIBUTE_ID,
                                 CLUSTER_MASK_SERVER,
                                 (int8u *)&nameSupport,
                                 ZCL_BITMAP8_ATTRIBUTE_TYPE);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfGroupsClusterPrintln("ERR: writing name support %x", status);
  }
}

// --------------------------
// Internal functions used to maintain the group table within the context 
// of the binding table.
//
// In the binding:
// The first two bytes of the identifier is set to the groupId
// The local endpoint is set to the endpoint that is mapped to this group
// --------------------------
static EmberAfStatus addEntryToGroupTable(int8u endpoint, int16u groupId, int8u *groupName)
{
  int8u i;

  // Check for duplicates.
  if (isGroupPresent(endpoint, groupId)) {
    return EMBER_ZCL_STATUS_DUPLICATE_EXISTS;
  }

  // Look for an empty binding slot.
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry binding;
    if (emberGetBinding(i, &binding) == EMBER_SUCCESS
        && binding.type == EMBER_UNUSED_BINDING) {
      EmberStatus status;
      binding.type = EMBER_MULTICAST_BINDING;
      binding.identifier[0] = LOW_BYTE(groupId);
      binding.identifier[1] = HIGH_BYTE(groupId);
      binding.local = endpoint;

      status = emberSetBinding(i, &binding);
      if (status == EMBER_SUCCESS) {
        // Set the group name, if supported
        emberAfPluginGroupsServerSetGroupNameCallback(endpoint,
                                                      groupId,
                                                      groupName);
        return EMBER_ZCL_STATUS_SUCCESS;
      } else {
        emberAfGroupsClusterPrintln("ERR: Failed to create binding (0x%x)",
                                    status);
      }
    }
  }
  emberAfGroupsClusterPrintln("ERR: Binding table is full");
  return EMBER_ZCL_STATUS_INSUFFICIENT_SPACE;
}

static EmberAfStatus removeEntryFromGroupTable(int8u endpoint, int16u groupId)
{
  if(isGroupPresent(endpoint, groupId)) {
    int8u bindingIndex = findGroupIndex(endpoint, groupId);
    EmberStatus status = emberDeleteBinding(bindingIndex);
    if (status == EMBER_SUCCESS) {
      int8u groupName[ZCL_GROUPS_CLUSTER_MAXIMUM_NAME_LENGTH + 1] = {0};
      emberAfPluginGroupsServerSetGroupNameCallback(endpoint,
                                                    groupId,
                                                    groupName);
      return EMBER_ZCL_STATUS_SUCCESS;
    } else {
      emberAfGroupsClusterPrintln("ERR: Failed to delete binding (0x%x)",
                                  status);
      return EMBER_ZCL_STATUS_FAILURE;
    }
  }
  return EMBER_ZCL_STATUS_NOT_FOUND;
}

boolean emberAfGroupsClusterAddGroupCallback(int16u groupId, int8u *groupName)
{
  EmberAfStatus status;

  emberAfGroupsClusterPrint("RX: AddGroup 0x%2x, \"", groupId);
  emberAfGroupsClusterPrintString(groupName);
  emberAfGroupsClusterPrintln("\"");

  status = addEntryToGroupTable(emberAfCurrentEndpoint(), groupId, groupName);

  // If this is a ZLL device, Add Group commands are only responded to when
  // they are addressed to a single device.
  if (emberIsZllNetwork()) {
    if (emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST
        && emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST_REPLY) {
      return TRUE;
    }
  }
  emberAfFillCommandGroupsClusterAddGroupResponse(status, groupId);
  emberAfSendResponse();
  return TRUE;
}

boolean emberAfGroupsClusterViewGroupCallback(int16u groupId)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_NOT_FOUND;
  int8u groupName[ZCL_GROUPS_CLUSTER_MAXIMUM_NAME_LENGTH + 1] = {0};

  // Get the group name, if supported
  emberAfPluginGroupsServerGetGroupNameCallback(emberAfCurrentEndpoint(), 
                                                groupId, 
                                                groupName);

  emberAfGroupsClusterPrintln("RX: ViewGroup 0x%2x", groupId);

  // If this is a ZLL device, View Group commands can only be addressed to a
  // single device.
  if (emberIsZllNetwork()) {
    if (emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST
        && emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST_REPLY) {
      return TRUE;
    }
  }

  if(isGroupPresent(emberAfCurrentEndpoint(), groupId)) {
    status = EMBER_ZCL_STATUS_SUCCESS;
  }

  emberAfFillCommandGroupsClusterViewGroupResponse(status, groupId, groupName);
  emberAfSendResponse();
  return TRUE;
}

boolean emberAfGroupsClusterGetGroupMembershipCallback(int8u groupCount,
                                                       int8u *groupList)
{
  int8u i, j;
  int8u count = 0;
  int8u list[EMBER_BINDING_TABLE_SIZE << 1];
  int8u listLen = 0;

  emberAfGroupsClusterPrint("RX: GetGroupMembership 0x%x,", groupCount);
  for (i = 0; i < groupCount; i++) {
    emberAfGroupsClusterPrint(" [0x%2x]",
                              emberAfGetInt16u(groupList + (i << 1), 0, 2));
  }
  emberAfGroupsClusterPrintln("");

  // If this is a ZLL device, Get Group Membership commands can only be
  // addressed to a single device.
  if (emberIsZllNetwork()) {
    if (emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST
        && emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST_REPLY) {
      return TRUE;
    }
  }

  // When Group Count is zero, respond with a list of all active groups.
  // Otherwise, respond with a list of matches.
  if (groupCount == 0) {
    for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
      EmberBindingTableEntry entry;
      emberGetBinding(i, &entry);
      if (entry.type == EMBER_MULTICAST_BINDING) {
        if (entry.local == emberAfCurrentEndpoint()) {
          list[listLen]     = entry.identifier[0];
          list[listLen + 1] = entry.identifier[1];
          listLen += 2;
          count++;
        }
      }
    }
  } else {
    for (i = 0; i < groupCount; i++) {
      int16u groupId = emberAfGetInt16u(groupList + (i << 1), 0, 2);
      for (j = 0; j < EMBER_BINDING_TABLE_SIZE; j++) {
        EmberBindingTableEntry entry;
        emberGetBinding(j, &entry);
        if (entry.type == EMBER_MULTICAST_BINDING) {
          if (entry.local == emberAfCurrentEndpoint()
              && entry.identifier[0] == LOW_BYTE(groupId)
              && entry.identifier[1] == HIGH_BYTE(groupId)) {
            list[listLen]     = entry.identifier[0];
            list[listLen + 1] = entry.identifier[1];
            listLen += 2;
            count++;
          }
        }
      }
    }
  }

  // Only send a response if the Group Count was zero or if one or more active
  // groups matched.  Otherwise, a Default Response is sent.
  if (groupCount == 0 || count != 0) {
    // A capacity of 0xFF means that it is unknown if any further groups may be
    // added.  Each group requires a binding and, because the binding table is
    // used for other purposes besides groups, we can't be sure what the
    // capacity will be in the future.
    emberAfFillCommandGroupsClusterGetGroupMembershipResponse(0xFF, // capacity
                                                              count,
                                                              list,
                                                              listLen);
    emberAfSendResponse();
  } else {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_NOT_FOUND);
  }
  return TRUE;
}

boolean emberAfGroupsClusterRemoveGroupCallback(int16u groupId)
{
  EmberAfStatus status;

  emberAfGroupsClusterPrintln("RX: RemoveGroup 0x%2x", groupId);

  status = removeEntryFromGroupTable(emberAfCurrentEndpoint(),
                                     groupId);

  // If this is a ZLL device, Remove Group commands are only responded to when
  // they are addressed to a single device.
  if (emberIsZllNetwork()) {
    if (emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST
        && emberAfCurrentCommand()->type != EMBER_INCOMING_UNICAST_REPLY) {
      return TRUE;
    }
  }

  emberAfScenesClusterRemoveScenesInGroupCallback(emberAfCurrentEndpoint(),
                                                  groupId);

  emberAfFillCommandGroupsClusterRemoveGroupResponse(status, groupId);
  emberAfSendResponse();
  return TRUE;
}

boolean emberAfGroupsClusterRemoveAllGroupsCallback(void)
{
  int8u i, endpoint = emberAfCurrentEndpoint();
  boolean success = TRUE;

  emberAfGroupsClusterPrintln("RX: RemoveAllGroups");

  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry binding;
    if(emberGetBinding(i, &binding) == EMBER_SUCCESS) {
      if (binding.type == EMBER_MULTICAST_BINDING
          && endpoint == binding.local) {
        EmberStatus status = emberDeleteBinding(i);
        if (status != EMBER_SUCCESS) {
          success = FALSE;
          emberAfGroupsClusterPrintln("ERR: Failed to delete binding (0x%x)",
                                      status);
        }
        else {
          int8u groupName[ZCL_GROUPS_CLUSTER_MAXIMUM_NAME_LENGTH + 1] = {0};
          int16u groupId = HIGH_LOW_TO_INT(binding.identifier[1], 
                                           binding.identifier[0]);
          emberAfPluginGroupsServerSetGroupNameCallback(endpoint, 
                                                        groupId, 
                                                        groupName);
          success = TRUE && success;
        }
      }
    }
  }

  emberAfScenesClusterRemoveScenesInGroupCallback(emberAfCurrentEndpoint(),
                                                  ZCL_SCENES_GLOBAL_SCENE_GROUP_ID);

  emberAfSendImmediateDefaultResponse((success
                              ? EMBER_ZCL_STATUS_SUCCESS
                              : EMBER_ZCL_STATUS_FAILURE));
  return TRUE;
}

boolean emberAfGroupsClusterAddGroupIfIdentifyingCallback(int16u groupId,
                                                          int8u *groupName)
{
  EmberAfStatus status;

  emberAfGroupsClusterPrint("RX: AddGroupIfIdentifying 0x%2x, \"", groupId);
  emberAfGroupsClusterPrintString(groupName);
  emberAfGroupsClusterPrintln("\"");

  if (!emberAfIsDeviceIdentifying(emberAfCurrentEndpoint())) {
    status = EMBER_ZCL_STATUS_FAILURE;
  } else {
    status = addEntryToGroupTable(emberAfCurrentEndpoint(),
                                  groupId, 
                                  groupName);
  }

  emberAfSendImmediateDefaultResponse(status);
  return TRUE;
}

boolean emberAfGroupsClusterEndpointInGroupCallback(int8u endpoint,
                                                    int16u groupId)
{
  return isGroupPresent(endpoint, groupId);
}

void emberAfGroupsClusterClearGroupTableCallback(int8u endpoint)
{
  int8u i, networkIndex = emberGetCurrentNetwork();
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry binding;
    if (emberGetBinding(i, &binding) == EMBER_SUCCESS
        && binding.type == EMBER_MULTICAST_BINDING
        && (endpoint == binding.local
            || (endpoint == EMBER_BROADCAST_ENDPOINT
                && networkIndex == binding.networkIndex))) {
      EmberStatus status = emberDeleteBinding(i);
      if (status != EMBER_SUCCESS) {
        emberAfGroupsClusterPrintln("ERR: Failed to delete binding (0x%x)",
                                    status);
      }
    }
  }
}

static boolean isGroupPresent(int8u endpoint, int16u groupId)
{
  int8u i;

  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry binding;
    if (emberGetBinding(i, &binding) == EMBER_SUCCESS) {
      if (bindingGroupMatch(endpoint, groupId, &binding)) {
        return TRUE;
      }
    }
  }
  
  return FALSE;
}

static boolean bindingGroupMatch(int8u endpoint,
                                 int16u groupId,
                                 EmberBindingTableEntry *entry)
{
  return (entry->type == EMBER_MULTICAST_BINDING
          && entry->identifier[0] == LOW_BYTE(groupId)
          && entry->identifier[1] == HIGH_BYTE(groupId)
          && entry->local == endpoint);
}

static int8u findGroupIndex(int8u endpoint, int16u groupId)
{
  int8u i;
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    EmberBindingTableEntry entry;
    emberGetBinding(i, &entry);
    if(bindingGroupMatch(endpoint, groupId, &entry)) {
      return i;
    }
  }
  return EMBER_AF_GROUP_TABLE_NULL_INDEX;
}
