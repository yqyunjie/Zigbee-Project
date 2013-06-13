// File: af-event-host.c
// 
// Description: Functions dealing with events ported from the stack to host.
// 
// Copyright 2010 by Ember Corporation. All rights reserved.                *80*

#include PLATFORM_HEADER     // Micro and compiler specific typedefs and macros
#include "stack/include/ember-types.h"
#include "hal/hal.h"
#include "stack/include/event.h"


extern EmberTaskControl emTasks[];
extern PGM int8u emTaskCount;
static int8u emActiveTaskCount = 0;


void emEventControlSetActive(EmberEventControl *event)
{
  event->status = EMBER_EVENT_ZERO_DELAY;
}

void emEventControlSetDelayMS(EmberEventControl*event, int16u delay)
{
  event->timeToExecute = halCommonGetInt16uMillisecondTick() + delay;
  event->status = EMBER_EVENT_MS_TIME;
}

void emEventControlSetDelayQS(EmberEventControl*event, int16u delay)
{
  event->timeToExecute = halCommonGetInt16uQuarterSecondTick() + delay;
  event->status = EMBER_EVENT_QS_TIME;
}

void emEventControlSetDelayMinutes(EmberEventControl*event, int16u delay)
{
  event->timeToExecute =
    ((int16u) (halCommonGetInt32uMillisecondTick() >> 16)) + delay;
  event->status = EMBER_EVENT_MINUTE_TIME;
}

void emberRunTask(EmberTaskId taskid)
{
  EmberTaskControl *task = &(emTasks[taskid]);
  emberRunEvents(task->events);
}

void emberRunEvents(EmberEventData *events)
{
  int16u times[4];
  EmberEventData *nextEvent;
  int32u nowMS32 = halCommonGetInt32uMillisecondTick();

  times[EMBER_EVENT_MS_TIME]     = (int16u) nowMS32;
  times[EMBER_EVENT_QS_TIME]     = (int16u) (nowMS32 >> 8);
  times[EMBER_EVENT_MINUTE_TIME] = (int16u) (nowMS32 >> 16);

  for (nextEvent = events; ; nextEvent++) {
    EmberEventControl *control = nextEvent->control;
    if (control == NULL)
      break;
    if (control->status == EMBER_EVENT_ZERO_DELAY
        || (control->status != EMBER_EVENT_INACTIVE
            && (elapsedTimeInt16u(control->timeToExecute, 
                                  times[control->status]) <=
                (((int32u)MAX_INT16U_VALUE + 1) / 2)))) 
    {
      ((void (*)(EmberEventControl *))(nextEvent->handler))(control);
    }
  }
}

int32u emberMsToNextEventExtended(EmberEventData *events, int32u maxTime, int8u* returnIndex)
{
  int16u times[4];
  EmberEventData *nextEvent;
  int32u time = maxTime;
  int32u nowMS32 = halCommonGetInt32uMillisecondTick();
  int8u index = 0;
  if (returnIndex != NULL) {
    *returnIndex = 0xFF;
  }
  
  times[EMBER_EVENT_MS_TIME]     = (int16u) nowMS32;
  times[EMBER_EVENT_QS_TIME]     = (int16u) (nowMS32 >> 8);
  times[EMBER_EVENT_MINUTE_TIME] = (int16u) (nowMS32 >> 16);

  for (nextEvent = events; ; nextEvent++) {
    EmberEventControl *control = nextEvent->control;
    int8u eventStatus;
    int16u eventTime;
    int32u waitTime;

    if (control == NULL
        || time == 0)
      break;

    eventStatus = control->status;
    if (eventStatus != EMBER_EVENT_INACTIVE) {
      eventTime = control->timeToExecute;
      waitTime = elapsedTimeInt16u(times[eventStatus], eventTime);
      if (control->status == EMBER_EVENT_ZERO_DELAY
          || timeGTorEqualInt16u(times[eventStatus], eventTime)) {
        waitTime = 0;
      } else if (eventStatus == EMBER_EVENT_QS_TIME) {
        waitTime = ((waitTime << 8)
                    - (times[EMBER_EVENT_MS_TIME] & 0xFF));
      } else if (eventStatus == EMBER_EVENT_MINUTE_TIME) {
        waitTime = ((waitTime << 16)
                    - times[EMBER_EVENT_MS_TIME]);
      }
      if (waitTime < time) {
        time = waitTime;
        if (returnIndex != NULL) {
          *returnIndex = index;
        }
      }
    }
    index++;
  }
  return time;
}

int32u emberMsToNextEvent(EmberEventData *events, int32u maxMs)
{
  return emberMsToNextEventExtended(events, maxMs, NULL);
}

// Must be called on a task before it is possible that any of the tasks 
//  events may be altered.
EmberTaskId emberTaskInit(EmberEventData *events)
{
  EmberTaskControl *task;
  EmberTaskId id;
  
  // make sure we have an available task
  assert(emActiveTaskCount < emTaskCount);

  // allocate the next free task
  id = emActiveTaskCount++;
  
  task = &(emTasks[id]);
  task->events = events;
  
  return id;
}

boolean emberMarkTaskIdle(EmberTaskId taskid)
{
  INTERRUPTS_ON();
  return FALSE;
}


void emMarkTaskActive(EmberTaskId taskid)
{
  EmberTaskControl *task = &(emTasks[taskid]);
  task->busy = TRUE;
}

void emTaskEnableIdling(boolean allow) { } //stub


