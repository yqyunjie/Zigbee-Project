// *******************************************************************
// * load-control-event-table.h
// *
// * The Demand Response Load Control Event Table is responsible 
// * for keeping track of all load control events scheduled
// * by the Energy Service Provider. This module provides
// * interfaces used to schedule and inform load shedding
// * devices of scheduled events.
// *
// * Any code that uses this event table is responsible for
// * providing four things:
// *   1. frequent calls to eventTableTick(), one per millisecond
// *      will do. These calls are used to drive the 
// *      table's timing mechanism.
// *   2. A way to get the real time by implementing 
// *      getCurrentTime(int32u *currentTime);
// *   3. An implementation of eventAction which
// *      will be called whenever event status changes
// * 
// * The load control event table expects that currentTime, startTime
// * and randomization are provided in seconds. And that duration is 
// * provided in minutes. 
// *
// * The implementing code is responsible for over the
// * air communication based on event status changes
// * reported through the eventAction interface
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef __LOAD_CONTROL_EVENT_TABLE_H__
#define __LOAD_CONTROL_EVENT_TABLE_H__

// include global header for public LoadControlEvent struct
#include "../../include/af.h"

#define RANDOMIZE_START_TIME_FLAG     1
#define RANDOMIZE_END_TIME_FLAG       2

#define CANCEL_WITH_RANDOMIZATION        1

// Table entry status
enum {
  ENTRY_VOID,
  ENTRY_SCHEDULED,
  ENTRY_STARTED,
  ENTRY_IS_SUPERSEDED_EVENT,
  ENTRY_IS_CANCELLED_EVENT
};

enum {
  EVENT_OPT_FLAG_OPT_IN                           = 0x01,
  EVENT_OPT_FLAG_PARTIAL                          = 0x02
};


// EVENT TABLE API

void afLoadControlEventTableInit(int8u endpoint);


/** 
 * This function is used to schedule events in the
 * load control event table. The interface expects that
 * the user will populate a LoadControlEvent with all the
 * necessary fields and will pass a pointer to this event.
 * The passed event will be copied into the event table so it does
 * not need to survive the call to scheduleEvent.
 *
 * A call to this function always generates an event response OTA
 *
 */
void emAfScheduleLoadControlEvent(int8u endpoint,
                                  EmberAfLoadControlEvent *event);

/**
 * Tells the Event table when a tick has taken place. This
 * function should be called by the cluster that uses the 
 * event table.
 **/
void emAfLoadControlEventTableTick(int8u endpoint);

/**
 * Used to cancel all events in the event table
 *
 * @return A boolean value indicating that a response was
 * generated for this action.
 **/
boolean emAfCancelAllLoadControlEvents(int8u endpoint,
                                       int8u cancelControl);

/**
 * Used to cancel an event in the event table
 *
 * A call to this function always generates an event response OTA.
 **/
void emAfCancelLoadControlEvent(int8u endpoint,
                                int32u eventId,
                                int8u cancelControl,
                                int32u effectiveTime);

/**
 * Used to schedule a call to cancel an event. Takes a
 * LoadControlEvent struct which is used to encapsulate
 * the necessary information for the scheduled cancel.
 * 
 * The scheduleCancelEvent function only uses three fields
 * from the LoadControlEvent struct, eventId, startTime,
 * and eventControl, as follows:
 *
 * eventId: The eventId of the event to cancel.
 *
 * startTime: The starttime of the event should 
 * be the same as the effective time of the 
 * cancel and will be used to schedule the 
 * call to cancel.
 *
 * eventControl: The eventControl of the event should
 * be the same as the cancelControl passed when sending the 
 * initial cancel event call.
 *
 * NOTE: A call to cancelAllEvents() will wipe out
 * this scheduled cancel as well as all other events which
 * should be ok, since the event scheduled to cancel would
 * be affected as well by the cancel all call anyway.
 **/
void afScheduleCancelEvent(EmberAfLoadControlEvent *e);

/**
 * An interface for opting in and out of an event
 */
void emAfLoadControlEventOptInOrOut(int8u endpoint, 
                                    int32u eventId, 
                                    boolean optIn);

// The module using this table is responsible for providing the following
// functions.
/**
 * Called by the event table when an events status has
 * changed. This handle should be used to inform the 
 * ESP(s) and or react to scheduled events. The event
 * table will take care of clearing itself if the event
 * is completed.
 **/
void emberAfEventAction(EmberAfLoadControlEvent *event,
                        int8u eventStatus,
                        int8u sequenceNumber,
                        int8u esiIndex);

/** A routine that prints a message indicating
 *  an attempt to append a signature to the event status message
 *  has failed.
 **/
void emAfNoteSignatureFailure(void);

/** Prints the load control event table
**/
void emAfLoadControlEventTablePrint(int8u endpoint);

/**
 * Initialization function.
 */
void emAfLoadControlEventTableInit(int8u endpoint);

/*
 * Clear the event table.
 */
void emAfLoadControlEventTableClear(int8u endpoint);

#endif //__LOAD_CONTROL_EVENT_TABLE_H__

