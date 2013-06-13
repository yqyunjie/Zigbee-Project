// *******************************************************************
//  sp-database-util.c
//
//  Utility file for super parent application.
//
//  The file manages end devices database.  It receives information from the
//  gateway node.  Then it uses message type to determine the action to be
//  performed.  For example, when it receives a JOIN message, the module
//  performs a search through the database to determind if the child is already
//  there, the child's information will then be saved or updated.  When REPORT
//  message is received, the database first checks if the report is expected. 
//  If not, it will add the child who sends the report to the database if it is
//  not already there.  When TIMEOUT message is received, the database will 
//  increment the last three bit of the status bytes by one.
//
//  Note that database search can take a long time as database size grows. Each
//  user is responsible to manage his/her own database.  Ember will provide a
//  basic database management scheme but modification from user will be needed
//  to make the database module works best for his/her specific storage type.
//
//  In this sample database management utility, database module runs on Linux/
//  PC host machine to take advantage of available storage space and processing
//  power.  
//
//  Copyright 2008 by Ember Corporation. All rights reserved.               *80*
// *******************************************************************

#include "app/super-parent/sp-common.h"

// Allocate storage for end devices
#define maxChildren SP_MAX_END_DEVICES

// database utility functions
void dbAddChild(int8u *data);
int16u dbSearchForChild(spChildTableEntry *entryPtr);
boolean isEmptyChildEntry(spChildTableEntry *entryPtr);
boolean isReportExpected(EmberNodeId child);
void dbUpdateStatusByte(EmberNodeId childId, int8u action);

// database parameters
spChildTableEntry database[maxChildren];
int8u zeroEui64[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// two fingers to point at 1) current end device being queried and 2) last end
// device that failed to be queried.  This is to help minimizing database
// search time.
static int16u currentQuery = 0;
static int16u lastFailedQuery = 0;
// A window size of number of nodes to look back from currentQuery position
int16u numNodesToLook = 7;

// ----------------------------------------------------------
// Functions called by application

// Functions used to determine action to be performed by the database module
// regarding messages it receives.
void spDatabaseUtilForwardMessage(int8u *data, int8u length)
{
  EmberEUI64 childEui;
  
  // first byte of data is always message type
  int8u msgType = data[0];
  // next two bytes are child id
  EmberNodeId childId = HIGH_LOW_TO_INT(data[2], data[1]);
  // next eight bytes are child eui64
  MEMCOPY(childEui, &data[3], EUI64_SIZE);
  
  switch(msgType) {
    case SP_JOIN_MSG:
      // when receive a JOIN message, we will add the node into the database
      // if it's not already there.  Or update current node's information as
      // needed.
      emberSerialPrintf(APP_SERIAL, 
        "[databaseUtil] RX JOIN msg from %2x\r\n", childId);
      dbAddChild(&data[1]);
      break;

    case SP_REPORT_MSG:
      if(isReportExpected(childId)) {
        emberSerialPrintf(APP_SERIAL, 
        "[databaseUtil] RX REPORT msg from ");
        printEUI64(APP_SERIAL, (EmberEUI64*)childEui);
        emberSerialPrintf(APP_SERIAL, "\r\n");
      } else {
        // search the whole database for the node that we receives unexpected
        // report from.  If the node is not in the database, we may have missed
        // its JOIN message
        emberSerialPrintf(APP_SERIAL, 
        "[databaseUtil] RX unexpected REPORT from %2x\r\n", childId);
        dbAddChild(&data[1]);
      }
      break;

    case SP_TIMEOUT_MSG:
      // when receive a timeout message, the child's status byte is modified
      // accordingly.  We use the lower three bits in the status byte to keep
      // track of how many timeout message database has received for that 
      // particular child.  Customer can extend this status byte feature to,
      // for example, determine whether to remove the child from the database.
      emberSerialPrintf(APP_SERIAL, 
        "[databaseUtil] RX TIMEOUT msg for child %2x\r\n", childId);
      dbUpdateStatusByte(childId, 0);
      break;
  } // end switch statement
}

// Display current database's contents.
void spDatabaseUtilPrint(void)
{
  int16u i; 
  spChildTableEntry entry;                                     

  emberSerialPrintf(APP_SERIAL, 
    "idx  childId   childEui       parentId  status\r\n");
  for(i=0; i<maxChildren; ++i) {
    entry = database[i];
    emberSerialPrintf(APP_SERIAL, "%2x   %2x  %x%x%x%x%x%x%x%x   %2x    0x%x\r\n",
      i, entry.childId, entry.childEui[7], entry.childEui[6], entry.childEui[5], 
      entry.childEui[4], entry.childEui[3], entry.childEui[2],
      entry.childEui[1], entry.childEui[0], entry.parentId, entry.statusByte);
    emberSerialWaitSend(APP_SERIAL);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

// Provide end device's information to the gateway node in order for it to do
// the query process.
boolean spDatabaseUtilgetEnddeviceInfo(spChildTableEntry *entry)
{
  spChildTableEntry *tmp;
  
  if(currentQuery >= maxChildren) {
    emberSerialPrintf(APP_SERIAL, 
      "[databaseUtil] Invalid current query index %d\r\n", currentQuery);
    currentQuery = 0;
  }
  tmp = &(database[currentQuery]);
  if(isEmptyChildEntry(tmp)) {
    // If the entry is empty, then we are done.  Set the currentQuery pointer
    // back to the first end device in the database.
    currentQuery = 0;
    return FALSE;
  } else {
    entry->childId = tmp->childId;
    MEMCOPY(entry->childEui, tmp->childEui, EUI64_SIZE);
    entry->parentId = tmp->parentId;
    entry->statusByte = tmp->statusByte;
    ++currentQuery;
    return TRUE;
  }
}

// Search through the end device database using provided eui64 address and 
// populate the entry argument with the child's information.
boolean spDatabaseUtilgetEnddeviceInfoViaEui64(
                            spChildTableEntry *entry,
                            EmberEUI64 childEui)
{
  spChildTableEntry *tmp;
  int16u i;
  
  for(i=0; i<maxChildren; ++i) {
    tmp = &(database[i]);
    // Compare the last three bytes (manufacturer specific) with its own
    // eui64 and the other five bytes with the entry's childEui. 
    if(MEMCOMPARE(tmp->childEui, childEui, EUI64_SIZE) == 0) {
      entry->childId = tmp->childId;
      MEMCOPY(entry->childEui, tmp->childEui, EUI64_SIZE);
      entry->parentId = tmp->parentId;
      entry->statusByte = tmp->statusByte;
      return TRUE;
    }
  }
  return FALSE;
}

// ----------------------------------------------------------
// Database Utility Functions
// Search database for existing information before adding new information.
void dbAddChild(int8u *data)
{
  spChildTableEntry entry;
  int16u index;

  // extract data
  entry.childId = HIGH_LOW_TO_INT(data[1], data[0]);
  MEMCOPY(entry.childEui, &data[2], EUI64_SIZE);
  entry.parentId = HIGH_LOW_TO_INT(data[11], data[10]);
  entry.statusByte = 0;
  
  index = dbSearchForChild(&entry);
  if(index == 0xFFFF) {
    emberSerialPrintf(APP_SERIAL, 
      "[databaseUtil] database FULL!, cannot add child ");
    printEUI64(APP_SERIAL, (EmberEUI64*)entry.childEui);  
    emberSerialPrintf(APP_SERIAL, "\r\n");
  } else {
    database[index] = entry;
  }
}

// Search for given childId and eui64 in the database.  All field in the child
// entry is initialized to all 0x00's.  If it finds a free entry (ex. childId
// and childEui are all 0x00's), then it return the index of the free entry.
// If it cannot find a free entry, it returns 0xFFFF.  Note that this search
// function will also perform an id conflict detection while going through
// all the database to look for child with duplicate ids.  
int16u dbSearchForChild(spChildTableEntry *entryPtr)
{
  spChildTableEntry *tmp;
  int16u i;

  for(i=0; i<maxChildren; ++i) {
    tmp = &database[i];
    if(isEmptyChildEntry(tmp)) {
      // found free entry
      return i;
    } else {
      // id conflict detection:
      if((tmp->childId == entryPtr->childId) &&
        (MEMCOMPARE(tmp->childEui, entryPtr->childEui, EUI64_SIZE) != 0)) {
        emberSerialPrintf(APP_SERIAL, 
          "[databaseUtil] id conflict for short id %2x\r\n", tmp->childId);
        emberSerialPrintf(APP_SERIAL, "[databaseUtil] new eui64 ");
        printEUI64(APP_SERIAL, (EmberEUI64*)tmp->childEui); 
        emberSerialPrintf(APP_SERIAL, ", existing eui64 ");
        emberSerialPrintf(APP_SERIAL, "\r\n");
        printEUI64(APP_SERIAL, (EmberEUI64*)entryPtr->childEui); 
      } else if((tmp->childId != entryPtr->childId) &&
        (MEMCOMPARE(tmp->childEui, entryPtr->childEui, EUI64_SIZE) == 0)) {
        // child has left and joined back to the network.  update child entry
        // with new child id.
        return i;
      } else if((tmp->childId == entryPtr->childId) &&
        (MEMCOMPARE(tmp->childEui, entryPtr->childEui, EUI64_SIZE) == 0)) {
        // child has rejoined and we need to update its parent id
        return i;
      }
    }
  } // end for loop  
  return 0xFFFF;
}

// Check if given entry is empty, by check whether child id and child eui
// fields are all zero.
boolean isEmptyChildEntry(spChildTableEntry *entryPtr)
{
  if((entryPtr->childId == 0x0000) &&
        (MEMCOMPARE(entryPtr->childEui, zeroEui64, EUI64_SIZE) == 0)) {
    return TRUE;
  } else {
    return FALSE;
  }
}

// Look up if we expect to receive a report from the node, if so then we also
// set the status byte to a 'good' state (clear the missed message count)
boolean isReportExpected(EmberNodeId child)
{
  int16u start=0, index=0, i;
  spChildTableEntry *tmp;

  // check the position of the node we received the report from in reference
  // to the currentQuery
  start = (currentQuery + maxChildren - numNodesToLook)%maxChildren;
  for(i=0; i<=numNodesToLook; ++i) {
    index = (i+start)%maxChildren;
    tmp = &(database[index]);
    if(tmp->childId == child) {
      tmp->statusByte &= ~SP_STATUS_MISSED_MASK;
      return TRUE;
    }
  }

  // check if it's the node that has missed the query
  start = (lastFailedQuery + maxChildren - numNodesToLook)%maxChildren;
  for(i=0; i<=numNodesToLook; ++i) {
    index = (i+start)%maxChildren;
    tmp = &(database[index]);
    if(tmp->childId == child) {
      tmp->statusByte &= ~SP_STATUS_MISSED_MASK;
      return TRUE;
    }
  }
  return FALSE;
}

// When TIMEOUT message is received, we increment (the last three bits of) the
// status byte accordingly.
void dbUpdateStatusByte(EmberNodeId childId, int8u action)
{
  spChildTableEntry *tmp;
  int16u i;

  for(i=0; i<maxChildren; ++i) {
    tmp = &database[i];
    // found the child
    if(tmp->childId == childId) {
      switch(action) {
        case 0: // timeout message
          // check if the we have already missed maximum number of missed
          // packet count, which is 7 (SP_STATUS_MISSED_MASK) in this case,
          // since we only use the last three bit of the status byte to store
          // the missed packet count.  If we have already reached the max
          // value, then we will not increment the value.
          if((tmp->statusByte & SP_STATUS_MISSED_MASK) < SP_STATUS_MISSED_MASK) {
            tmp->statusByte = tmp->statusByte + 1;
          }
          // set last failed query index in case the child sends us report
          lastFailedQuery = i;
          break;
      }
      // leave the for loop
      break;
    } // end child id check
  } // end for loop  
}


