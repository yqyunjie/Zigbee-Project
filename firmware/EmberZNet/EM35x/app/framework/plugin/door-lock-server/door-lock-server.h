// *******************************************************************
// * door-lock-server.h
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// These are variable and should be defined by the application using
// this plugin.
#ifndef DOOR_LOCK_USER_TABLE_SIZE
  #define DOOR_LOCK_USER_TABLE_SIZE 4
#endif

#ifndef DOOR_LOCK_SCHEDULE_TABLE_SIZE
  #define DOOR_LOCK_SCHEDULE_TABLE_SIZE 4
#endif

#ifndef DOOR_LOCK_MAX_PIN_LENGTH
  #define DOOR_LOCK_MAX_PIN_LENGTH 8
#endif

#ifndef DOOR_LOCK_SERVER_ENDPOINT
  #define DOOR_LOCK_SERVER_ENDPOINT 1
#endif

typedef struct {
  EmberAfDoorLockUserStatus status;
  EmberAfDoorLockUserType type;
  int8u pinLength;
  int8u pin[DOOR_LOCK_MAX_PIN_LENGTH]; 
} EmberAfDoorLockUser;

typedef struct {
  int16u userID;
  int8u daysMask;
  int8u startHour;
  int8u startMinute;
  int8u stopHour;
  int8u stopMinute;
} EmberAfDoorLockScheduleEntry;
