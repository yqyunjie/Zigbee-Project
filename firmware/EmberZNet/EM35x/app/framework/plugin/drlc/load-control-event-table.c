// *******************************************************************
// * load-control-event-table.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// clusters specific header
#include "app/framework/include/af.h"
#include "../../util/common.h"
#include "load-control-event-table.h"

#include "app/framework/plugin/esi-management/esi-management.h"

static void emAfCallEventAction(EmberAfLoadControlEvent *event,
                                int8u eventStatus,
                                int8u sequenceNumber,
                                boolean replyToSingleEsi,
                                int8u esiIndex);

// This assumes that the Option Flag Enum uses only two bits
PGM int8u controlValueToStatusEnum[] = {
  EMBER_ZCL_AMI_EVENT_STATUS_EVENT_COMPLETED_NO_USER_PARTICIPATION_PREVIOUS_OPT_OUT, // ! EVENT_OPT_FLAG_PARTIAL
                                                                           // && ! EVENT_OPT_FLAG_OPT_IN
  EMBER_ZCL_AMI_EVENT_STATUS_EVENT_COMPLETED,                                        // ! EVENT_OPT_FLAG_PARTIAL
                                                                  // && EVENT_OPT_FLAG_OPT_IN
  EMBER_ZCL_AMI_EVENT_STATUS_EVENT_PARTIALLY_COMPLETED_WITH_USER_OPT_OUT,   // EVENT_OPT_FLAG_PARTIAL
                                                                  // && ! EVENT_OPT_FLAG_OPT_IN
  EMBER_ZCL_AMI_EVENT_STATUS_EVENT_PARTIALLY_COMPLETED_DUE_TO_USER_OPT_IN   // EVENT_OPT_FLAG_PARTIAL
                                                                  // && EVENT_OPT_FLAG_OPT_IN
};

typedef struct 
{
  EmberAfLoadControlEvent event;
  int8u entryStatus;
} LoadControlEventTableEntry;

static LoadControlEventTableEntry loadControlEventTable[EMBER_AF_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_CLIENT_ENDPOINT_COUNT][EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE];

static boolean overlapFound(EmberAfLoadControlEvent *newEvent,
                            EmberAfLoadControlEvent *existingEvent) {
  if (newEvent->startTime < (existingEvent->startTime + ((int32u)existingEvent->duration * 60)) &&
      existingEvent->startTime < (newEvent->startTime + ((int32u)newEvent->duration * 60)))
    return TRUE;
  else
    return FALSE;
}

static boolean entryIsScheduledOrStarted(int8u entryStatus) {
  if (entryStatus == ENTRY_SCHEDULED || entryStatus == ENTRY_STARTED)
    return TRUE;

  return FALSE;
}

static void initEventData(EmberAfLoadControlEvent *event) {
  MEMSET(event, 0, sizeof(EmberAfLoadControlEvent));
}

/**
 * Clears the table of any entries which pertain to a
 * specific eventId.
 **/
static void voidAllEntriesWithEventId(int8u endpoint,
                                      int32u eventId) {
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;

  if (ep == 0xFF) {
    return;
  }

  for (i=0;i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE;i++)
  {
    e = &loadControlEventTable[ep][i];
    if (e->event.eventId == eventId)
    {
      e->entryStatus = ENTRY_VOID;
    }
  }
}

/**
 * The callback function passed to the ESI management plugin. It handles
 * ESI entry deletions.
 */
static void esiDeletionCallback(int8u esiIndex) {
  int8u i, j;
  for (i = 0; i < EMBER_AF_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_CLIENT_ENDPOINT_COUNT; i++) {
    for (j = 0; j < EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; j++) {
      loadControlEventTable[i][j].event.esiBitmask &= ~BIT(esiIndex);
    }
  }
}

/**
 * The tick function simply checks entries in the table
 * and sends informational messages about event start
 * and event complete.
 */
void emAfLoadControlEventTableTick(int8u endpoint) {
  int32u ct = emberAfGetCurrentTime();
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  int8u i;
  LoadControlEventTableEntry *e;

  if (ep == 0xFF) {
    return;
  }

  // Check for events that need to be run
  for( i=0; i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++ ) {
    e = &loadControlEventTable[ep][i];
    if (e->entryStatus == ENTRY_SCHEDULED)
    {
      if (e->event.startTime+e->event.startRand <= ct)
      {
        // Bug: 13546
        // When the event starts always send a Report Event status message.
        // If user opted-out, then send that status instead of event started.
        emAfCallEventAction(&(e->event), 
                            ((e->event.optionControl & EVENT_OPT_FLAG_OPT_IN)
                             ? EMBER_ZCL_AMI_EVENT_STATUS_EVENT_STARTED
                             : EMBER_ZCL_AMI_EVENT_STATUS_USER_HAS_CHOOSE_TO_OPT_OUT),
                            emberAfNextSequence(),
                            FALSE,
                            0);
        e->entryStatus = ENTRY_STARTED;
        return;
      }
    } else if (e->entryStatus == ENTRY_STARTED) {
      if ((e->event.startTime 
           + ((int32u)e->event.duration * 60) 
           + e->event.endRand) <= ct) {
        emAfCallEventAction(&(e->event), 
                            controlValueToStatusEnum[e->event.optionControl],
                            emberAfNextSequence(),
                            FALSE,
                            0);
        voidAllEntriesWithEventId(endpoint,
                                  e->event.eventId);
        return;
      }
    } else if (e->entryStatus == ENTRY_IS_SUPERSEDED_EVENT) {
      if (e->event.startTime <= ct) {
        emAfCallEventAction(&(e->event), 
                            EMBER_ZCL_AMI_EVENT_STATUS_THE_EVENT_HAS_BEEN_SUPERSEDED,
                            emberAfNextSequence(),
                            FALSE,
                            0);
        voidAllEntriesWithEventId(endpoint,
                                  e->event.eventId);
        return;
      }
    } else if (e->entryStatus == ENTRY_IS_CANCELLED_EVENT)
    {
      if (e->event.startTime <= ct) {
        emAfCallEventAction(&(e->event), 
                            EMBER_ZCL_AMI_EVENT_STATUS_THE_EVENT_HAS_BEEN_CANCELED, 
                            emberAfNextSequence(),
                            FALSE,
                            0);
        voidAllEntriesWithEventId(endpoint,
                                  e->event.eventId);
        return;
      }
    }
  }
}

/** 
 * This function is used to schedule events in the
 * load control event table.
 */
void emAfScheduleLoadControlEvent(int8u endpoint,
                                  EmberAfLoadControlEvent *newEvent) {
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;
  int32u ct = emberAfGetCurrentTime();
  EmberAfClusterCommand *curCommand = emberAfCurrentCommand();
  int8u esiIndex =
      emberAfPluginEsiManagementUpdateEsiAndGetIndex(curCommand);
      //emberAfPluginEsiManagementUpdateEsiAndGetIndex(emberAfCurrentCommand());

  if (ep == 0xFF) {
    return;
  }

  //validate starttime + duration
  if (newEvent->startTime == 0xffffffffUL
      || newEvent->duration > 0x5a0) {
    emAfCallEventAction(newEvent, 
                        EMBER_ZCL_AMI_EVENT_STATUS_LOAD_CONTROL_EVENT_COMMAND_REJECTED, 
                        emberAfCurrentCommand()->seqNum,
                        TRUE,
                        esiIndex);
    return;
  }

  //validate expiration
  if (ct > (newEvent->startTime + ((int32u)newEvent->duration * 60)))
  {
    emAfCallEventAction(newEvent, 
                        EMBER_ZCL_AMI_EVENT_STATUS_REJECTED_EVENT_EXPIRED, 
                        emberAfCurrentCommand()->seqNum,
                        TRUE,
                        esiIndex);
    return;
  }

  //validate event id
  for (i = 0; i < EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++) {
    e = &loadControlEventTable[ep][i];
    if (entryIsScheduledOrStarted(e->entryStatus) &&
        (e->event.eventId == newEvent->eventId))
    {
      // Bug 13805: from multi-ESI specs (5.7.3.5): When a device receives
      // duplicate events (same event ID) from multiple ESIs, it shall send an
      // event response to each ESI. Future duplicate events from the same
      // ESI(s) shall be either ignored by sending no response at all or with a
      // default response containing a success status code.
      //emberAfSendDefaultResponse(emberAfCurrentCommand(),
      //                           EMBER_ZCL_STATUS_DUPLICATE_EXISTS);

      // First time hearing this event from this ESI. If the ESI is present in
      // the table add the ESI to the event ESI bitmask and respond. If it is
      // a duplicate from the same ESI, we just ingore it.
      if ((e->event.esiBitmask & BIT(esiIndex)) == 0
          && esiIndex < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE) {
        e->event.esiBitmask |= BIT(esiIndex);
        emAfCallEventAction(&(e->event),
                            EMBER_ZCL_AMI_EVENT_STATUS_LOAD_CONTROL_EVENT_COMMAND_RX,
                            emberAfCurrentCommand()->seqNum,
                            TRUE,
                            esiIndex);
      }

      return;
    }
  }

  //locate empty table entry
  for (i = 0; i < EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++) {
    e = &loadControlEventTable[ep][i];
    if (e->entryStatus == ENTRY_VOID) {
      MEMCOPY(&(e->event), newEvent, sizeof(EmberAfLoadControlEvent));

      //check for supercession
      for (i=0;i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE;i++) {
        LoadControlEventTableEntry *currentEntry = &loadControlEventTable[ep][i];
        if (currentEntry->entryStatus == ENTRY_SCHEDULED ||
            currentEntry->entryStatus == ENTRY_STARTED) 
        {
          // If the event is superseded we need to let the application know
          // according to the following conditions interpreted from 075356r15
          // with help from NTS.
          //    1. If superseded event has not started, send superseded
          //       notification to application immediately.
          //    2. If superseded event HAS started, allow to run and send
          //       superseded message 1 second before new event starts. 
          //       (to do this we subtract 1 from new event start time to know
          //        when to notify the application that the current running
          //        event has been superseded.)
          if (overlapFound(newEvent, &(currentEntry->event))) 
          {
            if (currentEntry->entryStatus != ENTRY_STARTED)
              currentEntry->event.startTime = ct;
            else
              currentEntry->event.startTime = (newEvent->startTime + newEvent->startRand - 1);
            currentEntry->entryStatus = ENTRY_IS_SUPERSEDED_EVENT;
          }
        }
      }

      e->entryStatus = ENTRY_SCHEDULED;

      // If the ESI is in the table, we add it to the ESI bitmask of this event
      // and we respond.
      if (esiIndex < EMBER_AF_PLUGIN_ESI_MANAGEMENT_ESI_TABLE_SIZE) {
        e->event.esiBitmask = BIT(esiIndex);
        emAfCallEventAction(&(e->event),
                            EMBER_ZCL_AMI_EVENT_STATUS_LOAD_CONTROL_EVENT_COMMAND_RX,
                            emberAfCurrentCommand()->seqNum,
                            TRUE,
                            esiIndex);
      }

      return;
    }
  }

  // If we get here we have failed to schedule the event because we probably
  // don't have any room in the table and must reject scheduling. We reject but
  // others have chosen to bump the earliest event. There is an ongoing
  // discussion on this issue will be discussed for possible CCB.
  emAfCallEventAction(newEvent, 
                      EMBER_ZCL_AMI_EVENT_STATUS_LOAD_CONTROL_EVENT_COMMAND_REJECTED, 
                      emberAfCurrentCommand()->seqNum,
                      TRUE,
                      esiIndex);
}

void emAfLoadControlEventOptInOrOut(int8u endpoint, 
                                    int32u eventId, 
                                    boolean optIn) {
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;

  if (ep == 0xFF) {
    return;
  }

  for (i=0;i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE;i++)
  {
    e = &loadControlEventTable[ep][i];
    if (entryIsScheduledOrStarted(e->entryStatus) &&
        e->event.eventId == eventId)
    {
      // used to find out if we have opted in our out of a running event
      boolean previousEventOption = (e->event.optionControl & EVENT_OPT_FLAG_OPT_IN);
      
      // set the event opt in flag
      e->event.optionControl = 
        (optIn 
         ? (e->event.optionControl | EVENT_OPT_FLAG_OPT_IN) 
         : (e->event.optionControl & ~EVENT_OPT_FLAG_OPT_IN));

      // if we have opted in or out of a running event we need to set the
      // partial flag.
      if ((previousEventOption != optIn) &&
           e->entryStatus == ENTRY_STARTED)
      {
        e->event.optionControl |= EVENT_OPT_FLAG_PARTIAL;
      }

      // Bug: 13546
      // SE 1.0 and 1.1 dictate that if the event has not yet started,
      // and the user opts-out then don't send a status message.
      // Effectively the event is not changing so don't bother
      // notifying the ESI.  When the event would normally start, 
      // the opt-out takes effect and that is when we send the opt-out
      // message.
      if (!(e->event.optionControl & ~EVENT_OPT_FLAG_OPT_IN
            && e->entryStatus == ENTRY_SCHEDULED)) {

        emAfCallEventAction(
                            &(e->event), 
                            (optIn 
                             ? EMBER_ZCL_AMI_EVENT_STATUS_USER_HAS_CHOOSE_TO_OPT_IN 
                             : EMBER_ZCL_AMI_EVENT_STATUS_USER_HAS_CHOOSE_TO_OPT_OUT), 
                            emberAfNextSequence(),
                            FALSE,
                            0);
      }
      return;
    }
  }
}

void emAfCancelLoadControlEvent(int8u endpoint,
                                int32u eventId,
                                int8u cancelControl,
                                int32u effectiveTime) {
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;
  EmberAfLoadControlEvent undefEvent;
  int32u cancelTime = 0;
  int8u esiIndex =
      emberAfPluginEsiManagementUpdateEsiAndGetIndex(emberAfCurrentCommand());

  if (ep == 0xFF) {
    return;
  }

  for (i=0;i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++)
  {
    e = &loadControlEventTable[ep][i];
    if (e->event.eventId == eventId &&
        e->entryStatus != ENTRY_VOID) {

      // Found the event, validate effective time
      if ((effectiveTime == 0xffffffffUL) ||
          (effectiveTime > (e->event.startTime + 
                            (((int32u) e->event.duration) * 60))))
      {
        emAfCallEventAction(&(e->event), 
          EMBER_ZCL_AMI_EVENT_STATUS_REJECTED_INVALID_CANCEL_COMMAND_INVALID_EFFECTIVE_TIME, 
          emberAfCurrentCommand()->seqNum,
          TRUE,
          esiIndex);
        return;
      }

      // We're good, Run the cancel
      if (cancelControl & CANCEL_WITH_RANDOMIZATION)
      {
        if (effectiveTime == 0) {
          cancelTime = emberAfGetCurrentTime();
        }
        cancelTime += e->event.endRand;
      } else {
        cancelTime = effectiveTime;
      }
      e->entryStatus = ENTRY_IS_CANCELLED_EVENT; //will generate message on next tick
      e->event.startTime = cancelTime;
      return;
    }
  }

  // If we get here, we have failed to find the event
  // requested to cancel, send a fail message.
  initEventData(&undefEvent);
  undefEvent.destinationEndpoint =
      emberAfCurrentCommand()->apsFrame->destinationEndpoint;
  undefEvent.eventId = eventId;
  emAfCallEventAction(&undefEvent,
                      EMBER_ZCL_AMI_EVENT_STATUS_REJECTED_INVALID_CANCEL_UNDEFINED_EVENT,
                      emberAfCurrentCommand()->seqNum,
                      TRUE,
                      esiIndex);
}

boolean emAfCancelAllLoadControlEvents(int8u endpoint,
                                       int8u cancelControl) 
{
  int8u i;
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;
  boolean generatedResponse = FALSE;

  if (ep == 0xFF) {
    return FALSE;
  }

  for (i=0;i<EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE;i++)
  {
    e = &loadControlEventTable[ep][i];
    if (e->entryStatus != ENTRY_VOID) {
      emAfCancelLoadControlEvent(endpoint, e->event.eventId, cancelControl, 0);
      generatedResponse = TRUE;
    }
  }
  return generatedResponse;
}

static void emAfCallEventAction(EmberAfLoadControlEvent *event,
                                int8u eventStatus,
                                int8u sequenceNumber,
                                boolean replyToSingleEsi,
                                int8u esiIndex)
{
  if (replyToSingleEsi) {
    EmberAfPluginEsiManagementEsiEntry* esiEntry =
        emberAfPluginEsiManagementEsiLookUpByIndex(esiIndex);
    if (esiEntry != NULL) {
      emberAfEventAction(event,
                         eventStatus,
                         sequenceNumber,
                         esiIndex); // send it to a specific ESI.
    }
    // Response intended for a single ESI. If it does not appear in the table,
    // nothing to do.
  } else {
    emberAfEventAction(event,
                       eventStatus,
                       sequenceNumber,
                       0xFF); // send it to all ESIs in the event bitmask.
  }
}

void emAfNoteSignatureFailure(void)
{
  emberAfDemandResponseLoadControlClusterPrintln("Failed to append signature to message.");
}

void emAfLoadControlEventTablePrint(int8u endpoint) 
{
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER)
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  LoadControlEventTableEntry *e;  
  int8u i;
 
  if (ep == 0xFF) {
    return;
  }

  emberAfDemandResponseLoadControlClusterPrintln("ind  st id       sta      dur  ec oc"); 
  emberAfDemandResponseLoadControlClusterFlush();
 
  for(i = 0; i < EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++) {
    e = &loadControlEventTable[ep][i];
    emberAfDemandResponseLoadControlClusterPrintln("[%x] %x %4x %4x %2x %x %x", 
                                              i, 
                                              e->entryStatus, 
                                              e->event.eventId, 
                                              e->event.startTime, 
                                              e->event.duration, 
                                              e->event.eventControl, 
                                              e->event.optionControl); 
    emberAfDemandResponseLoadControlClusterFlush();
  }
#endif //defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER)
}


void emAfLoadControlEventTableInit(int8u endpoint) {
  // Subscribe receive deletion announcements.
  emberAfPluginEsiManagementSubscribeToDeletionAnnouncements(esiDeletionCallback);

  emAfLoadControlEventTableClear(endpoint);
}

void emAfLoadControlEventTableClear(int8u endpoint)
{
  int8u ep = emberAfFindClusterClientEndpointIndex(endpoint, ZCL_DEMAND_RESPONSE_LOAD_CONTROL_CLUSTER_ID);
  int8u i;

  if (ep == 0xFF) {
    return;
  }

  for(i = 0; i < EMBER_AF_PLUGIN_DRLC_EVENT_TABLE_SIZE; i++) {
    MEMSET(&loadControlEventTable[ep][i], 0, sizeof(LoadControlEventTableEntry));
  }
}
