// *******************************************************************
// * groups-client.c
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "../../include/af.h"

boolean emberAfGroupsClusterAddGroupResponseCallback(int8u status,
                                                     int16u groupId)
{
  emberAfGroupsClusterPrintln("RX: AddGroupResponse 0x%x, 0x%2x",
                              status,
                              groupId);
  emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfGroupsClusterViewGroupResponseCallback(int8u status,
                                                      int16u groupId,
                                                      int8u* groupName)
{
  emberAfGroupsClusterPrint("RX: ViewGroupResponse 0x%x, 0x%2x, \"",
                            status,
                            groupId);
  emberAfGroupsClusterPrintString(groupName);
  emberAfGroupsClusterPrintln("\"");
   emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfGroupsClusterGetGroupMembershipResponseCallback(int8u capacity,
                                                               int8u groupCount,
                                                               int8u* groupList)
{
  int8u i;
  emberAfGroupsClusterPrint("RX: GetGroupMembershipResponse 0x%x, 0x%x,",
                            capacity,
                            groupCount);
  for (i = 0; i < groupCount; i++) {
    emberAfGroupsClusterPrint(" [0x%2x]",
                              emberAfGetInt16u(groupList + (i << 1), 0, 2));
  }
  emberAfGroupsClusterPrintln("");
   emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

boolean emberAfGroupsClusterRemoveGroupResponseCallback(int8u status,
                                                        int16u groupId)
{
  emberAfGroupsClusterPrintln("RX: RemoveGroupResponse 0x%x, 0x%2x",
                              status,
                              groupId);
   emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}
