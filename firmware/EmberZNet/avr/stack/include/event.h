/**
 * @file event.h
 * @brief Scheduling events for future execution.
 * See @ref event for documentation.
 *
 * <!--Copyright 2005 by Ember Corporation. All rights reserved.         *80*-->
 */

/**
 * @addtogroup event
 *
 * These macros implement an event abstraction that allows the application
 * to schedule code to run after some specified time interval.  An event
 * consists of a procedure to be called at some point in the future and
 * a control object that determines the procedure should be called.  Events
 * are also useful for when an ISR needs to initiate an action that should
 * run outside of ISR context.
 *
 * See event.h for source code.
 *
 * Note that while not required, it is recommended that the event-handling procedure 
 * explicitly define the recurrence of the next event, either by rescheduling
 * it via some kind of @e emberEventControlSetDelayXX() call or by deactivating it
 * via a call to ::emberEventControlSetInactive().
 * In cases where the handler does not explicitly reschedule or cancel the 
 * event, the default behavior of the event control system is to keep the 
 * event immediately active as if the handler function had called  
 * ::emberEventControlSetActive(someEvent) or
 * ::emberEventControlSetDelayMS(someEvent, EMBER_EVENT_ZERO_DELAY)
 *
 * Following are some brief usage examples.
 * @code
 * EmberEventControl delayEvent;
 * EmberEventControl signalEvent;
 * EmberEventControl periodicEvent;
 *
 * void delayEventHandler(void)
 * {
 *   // Disable this event until its next use.
 *   emberEventControlSetInactive(delayEvent);
 * }
 *
 * void signalEventHandler(void)
 * {
 *   // Disable this event until its next use.
 *   emberEventControlSetInactive(signalEvent);
 *
 *   // Sometimes we need to do something 100 ms later.
 *   if (somethingIsExpected)
 *     emberEventControlSetDelayMS(delayEvent, 100);
 * }
 *
 * void periodicEventHandler(void)
 * {
 *   emberEventControlSetDelayQS(periodicEvent, 4);
 * }
 *
 * void someIsr(void)
 * {
 *   // Set the signal event to run at the first opportunity.
 *   emberEventControlSetActive(signalEvent);
 * }
 *
 * // Put the controls and handlers in an array.  They will be run in
 * // this order.
 * EmberEventData events[] =
 *  {
 *    { &delayEvent,    delayEventHandler },
 *    { &signalEvent,   signalEentHandler },
 *    { &periodicEvent, periodicEventHandler },
 *    { NULL, NULL }                            // terminator
 *  };
 *
 * void main(void)
 * {
 *   // Cause the periodic event to occur once a second.
 *   emberEventControlSetDelayQS(periodicEvent, 4);
 *   
 *   while (TRUE) {
 *     emberRunEvents(events);
 *   }
 * }
 * @endcode
 *
 * @{
 */

// Controlling events

// Possible event status values.  Having zero as the 'inactive' value
// causes events to initially be inactive.
//
// These are 'binary' quarter seconds and minutes.  One 'quarter second'
// is 256 milliseconds and one 'minute' is 65536 (0x10000) milliseconds
// (64 * 4 'quarter seconds').  And the milliseconds aren't all that accurate
// to begin with.

#ifndef __EVENT_H__
#define __EVENT_H__

/** @brief Sets this ::EmberEventControl as inactive (no pending event).
 */

#define emberEventControlSetInactive(control)    \
 ((control).status = EMBER_EVENT_INACTIVE)

/** @brief Returns true if the event is active, false otherwise.
 */

#define emberEventControlGetActive(control) ((control).status)

/** @brief Sets this ::EmberEventControl to run at the next available
    opportunity.
 */

#define emberEventControlSetActive(control)        \
 ((control).status = EMBER_EVENT_ZERO_DELAY)

/** @brief Sets this ::EmberEventControl to run "delay" milliseconds in the future.
 */
#define emberEventControlSetDelayMS(control, delay)        \
 (emEventControlSetDelayMS(&control, (delay)))

/** @brief Sets this ::EmberEventControl to run "delay" milliseconds in the future.
 */
void emEventControlSetDelayMS(EmberEventControl*event, int16u delay);

/** @brief Sets this ::EmberEventControl to run "delay" quarter seconds 
 *  in the future. The 'quarter seconds' are actually 256 milliseconds long.
 */
#define emberEventControlSetDelayQS(control, delay)             \
 (emEventControlSetDelayQS(&control, (delay)))

/** @brief Sets this ::EmberEventControl to run "delay" quarter seconds 
 *  in the future. The 'quarter seconds' are actually 256 milliseconds long.
 */
void emEventControlSetDelayQS(EmberEventControl*event, int16u delay);

/** @brief Sets this ::EmberEventControl to run "delay" minutes in the future.
 *  The 'minutes' are actually 65536 (0x10000) milliseconds long.
 */
#define emberEventControlSetDelayMinutes(control, delay)        \
 (emEventControlSetDelayMinutes(&control, (delay)))

/** @brief Sets this ::EmberEventControl to run "delay" minutes in the future.
 *  The 'minutes' are actually 65536 (0x10000) milliseconds long.
 */
void emEventControlSetDelayMinutes(EmberEventControl*event, int16u delay);

// Running events

/** @brief An application typically creates an array of events
 * along with their handlers.
 *
 * The main loop passes the array to ::emberRunEvents() in order to call
 * the handlers of any events whose time has arrived.
 */
void emberRunEvents(EmberEventData *events);

/** @brief Returns the number of milliseconds before the next event
 *  is scheduled to expire, or maxMs if no event is scheduled to expire
 * within that time.
 */
int32u emberMsToNextEvent(EmberEventData *events, int32u maxMs);

#endif // __EVENT_H__

// @} END addtogroup


