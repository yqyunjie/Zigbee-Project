/**
 * @file event.h
 * @brief Scheduling events for future execution.
 * See @ref event for documentation.
 *
 * <!--Copyright 2005 by Ember Corporation. All rights reserved.         *80*-->
 */

#ifdef XAP2B
  // The xap2b platform does not support processor idling
  #define EMBER_NO_IDLE_SUPPORT
#endif

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
 * The time units are all in 'binary'
 * One 'millisecond' is 1/1024 of a second.
 * One 'quarter second' is 256 milliseconds (which happens to work out to be
 *                                           the same as a real quarter second)
 * One 'minute' is 65536 (0x10000) milliseconds (64 * 4 'quarter seconds').  
 * The accuracy of the base 'millisecond' depends on your timer source.
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
#ifndef __EVENT_H__
#define __EVENT_H__

/** @brief Sets this ::EmberEventControl as inactive (no pending event).
 */
#define emberEventControlSetInactive(control)    \
  do { (control).status = EMBER_EVENT_INACTIVE; } while(0)

/** @brief Returns TRUE if the event is active, FALSE otherwise.
 */
#define emberEventControlGetActive(control)      \
  ((control).status != EMBER_EVENT_INACTIVE)

/** @brief Sets this ::EmberEventControl to run at the next available
    opportunity.
 */
#define emberEventControlSetActive(control)               \
  do { emEventControlSetActive(&(control)); } while(0)

/** @brief Sets this ::EmberEventControl to run at the next available
    opportunity.
 */
void emEventControlSetActive(EmberEventControl *event);

/** @brief Sets this ::EmberEventControl to run "delay" milliseconds in the future.
 */
#define emberEventControlSetDelayMS(control, delay)        \
  do { emEventControlSetDelayMS(&(control), (delay)); } while(0)

/** @brief Sets this ::EmberEventControl to run "delay" milliseconds in the future.
 */
void emEventControlSetDelayMS(EmberEventControl*event, int16u delay);

/** @brief Sets this ::EmberEventControl to run "delay" quarter seconds 
 *  in the future. The 'quarter seconds' are actually 256 milliseconds long.
 */
#define emberEventControlSetDelayQS(control, delay)             \
  do { emEventControlSetDelayQS(&(control), (delay)); } while(0)

/** @brief Sets this ::EmberEventControl to run "delay" quarter seconds 
 *  in the future. The 'quarter seconds' are actually 256 milliseconds long.
 */
void emEventControlSetDelayQS(EmberEventControl*event, int16u delay);

/** @brief Sets this ::EmberEventControl to run "delay" minutes in the future.
 *  The 'minutes' are actually 65536 (0x10000) milliseconds long.
 */
#define emberEventControlSetDelayMinutes(control, delay)        \
  do { emEventControlSetDelayMinutes(&(control), (delay)); } while(0)

/** @brief Sets this ::EmberEventControl to run "delay" minutes in the future.
 *  The 'minutes' are actually 65536 (0x10000) milliseconds long.
 */
void emEventControlSetDelayMinutes(EmberEventControl*event, int16u delay);

/** @brief Returns The amount of milliseconds remaining before the event is
 *  scheduled to run.  If the event is inactive, MAX_INT32U_VALUE is returned.
 */
#define emberEventControlGetRemainingMS(control)        \
  (emEventControlGetRemainingMS(&(control)))

/** @brief Returns The amount of milliseconds remaining before the event is
 *  scheduled to run.  If the event is inactive, MAX_INT32U_VALUE is returned.
 */
int32u emEventControlGetRemainingMS(EmberEventControl *event);


// Running events

/** @brief An application typically creates an array of events
 * along with their handlers.
 *
 * The main loop passes the array to ::emberRunEvents() in order to call
 * the handlers of any events whose time has arrived.
 */
void emberRunEvents(EmberEventData *events);

/** @brief If an application has initialized a task via emberTaskInit, to
 *  run the events associated with that task, it should could ::emberRunTask()
 *  instead of ::emberRunEvents().
 */
void emberRunTask(EmberTaskId taskid);

/** @brief Returns the number of milliseconds before the next event
 *  is scheduled to expire, or maxMs if no event is scheduled to expire
 *  within that time.  
 *  NOTE: If any events are modified within an interrupt, in order to guarantee
 *  the accuracy of this API, it must be called with interrupts disabled or 
 *  from within an ATOMIC() block.
 */
int32u emberMsToNextEvent(EmberEventData *events, int32u maxMs);

/** @brief This function does the same as emberMsToNextEvent() with the 
 *  following addition.  If the returnIndex is non-NULL, it will set the value
 *  pointed to by the pointer to be equal to the index of the event that is ready
 *  to fire next.  If no events are active, then it returns 0xFF.
 */
int32u emberMsToNextEventExtended(EmberEventData *events, int32u maxMs, int8u* returnIndex);

/** @brief Returns the number of milliseconds before the next stack event is
 * scheduled to expire.
 */
int32u emberMsToNextStackEvent(void);


/** @brief Initializes a task to be used for managing events and processor
 *  idling state.
 *  Returns the ::EmberTaskId which represents the newly created task
 */
EmberTaskId emberTaskInit(EmberEventData *events);

/** @brief Indicates that a task has nothing to do (unless any events are 
 *   pending) and that it would be safe to idle the CPU if all other tasks 
 *   also have nothing to do.
 *  This API should always be called with interrupts disabled.  It will forcibly
 *   re-enable interrupts before returning
 *  Returns TRUE if the processor was idled, FALSE if idling wasn't permitted
 *   because some other task has something to do.
 */
boolean emberMarkTaskIdle(EmberTaskId taskid);

#ifndef EMBER_NO_IDLE_SUPPORT
  /** @brief Call this to indicate that an application supports processor idling
   */
  #define emberTaskEnableIdling(allow)  \
    do { emTaskEnableIdling((allow)); } while(0)
   
  void emTaskEnableIdling(boolean allow);

  /** @brief Indicates that a task has something to do, so the CPU should not be 
   *   idled until emberMarkTaskIdle is next called on this task.
   */
  #define emberMarkTaskActive(taskid)  \
    do { emMarkTaskActive((taskid)); } while(0)

  void emMarkTaskActive(EmberTaskId taskid);
#else
  #define emberTaskEnableIdling(allow)  do {} while(0)
  #define emberMarkTaskActive(taskid)   do {} while(0)
#endif // EMBER_NO_IDLE_SUPPORT

#endif // __EVENT_H__

// @} END addtogroup


