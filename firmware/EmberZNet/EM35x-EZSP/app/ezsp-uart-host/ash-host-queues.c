/** @file ash-host-queues.c
 *  @brief  ASH protocol host queue functions
 * 
 * ASH Protocol Interface:
 * 
 * <!-- Copyright 2007 by Ember Corporation. All rights reserved.       *80*-->
 */

////////////////////////////////////////////////////////////////////////////////
//                                  CAUTION                                   //
//        Before these functions are used in a multi-threaded application,    //
//        they must be modified to create critical regions to prevent         //
//        data corruption that could result from overlapping accesses.        //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include PLATFORM_HEADER
#include <string.h>
#include "stack/include/ember-types.h"
#include "hal/micro/generic/ash-protocol.h"
#include "app/util/ezsp/ezsp-host-configuration-defaults.h"
#include "app/ezsp-uart-host/ash-host.h"
#include "app/ezsp-uart-host/ash-host-ui.h"
#include "app/ezsp-uart-host/ash-host-queues.h"

//------------------------------------------------------------------------------
// Preprocessor definitions

//------------------------------------------------------------------------------
// Global Variables

AshQueue txQueue;
AshQueue reTxQueue;
AshQueue rxQueue;
AshFreeList txFree;
AshFreeList rxFree;

//------------------------------------------------------------------------------
// Local Variables

static AshBuffer ashTxPool[TX_POOL_BUFFERS];
static AshBuffer ashRxPool[EZSP_HOST_ASH_RX_POOL_SIZE];

//------------------------------------------------------------------------------
// Forward Declarations

//#define ASH_QUEUE_TEST
#ifdef ASH_QUEUE_TEST
static void ashQueueTest(void);
static int32u ashInternalQueueTest(void);
#endif

//------------------------------------------------------------------------------
// Queue functions

// Initialize all queues to empty, and link all buffers into the free lists.
void ashInitQueues(void)
{
  AshBuffer *buffer;

  txQueue.tail = NULL;
  reTxQueue.tail = NULL;
  txFree.link = NULL;
  for (buffer = ashTxPool; buffer < &ashTxPool[TX_POOL_BUFFERS]; buffer++)
    ashFreeBuffer(&txFree, buffer);

  rxQueue.tail = NULL;
  rxFree.link = NULL;
  for (buffer = ashRxPool; 
       buffer < &ashRxPool[EZSP_HOST_ASH_RX_POOL_SIZE]; 
       buffer++)
    ashFreeBuffer(&rxFree, buffer);

#ifdef ASH_QUEUE_TEST
  ashQueueTest();
#endif

}

// Add a buffer to a free list
void ashFreeBuffer(AshFreeList *list, AshBuffer *buffer)
{
  if (buffer == NULL) {
    ashTraceEvent("Called ashFreeBuffer with NULL buffer\r\n");
    assert(FALSE);
  }
  buffer->link = list->link;
  list->link = buffer;
}

// Get a buffer from the free list
AshBuffer *ashAllocBuffer(AshFreeList *list)
{
  AshBuffer *buffer;

  buffer = list->link;
  if (buffer != NULL) {
    list->link = buffer->link;
    buffer->len = 0;
    memset(buffer->data, 0, ASH_MAX_DATA_FIELD_LEN);
  }
  return buffer;
}

// Remove the buffer at the head of a queue
AshBuffer *ashRemoveQueueHead(AshQueue *queue)
{
  AshBuffer *head, *prev;

  head = queue->tail;
  if (head == NULL) {
    ashTraceEvent("Tried to remove head from an empty queue\r\n");
    assert(FALSE);
  }
  if (head->link == NULL)
    queue->tail = NULL;
  else {
    do {
      prev = head;
      head = head->link;
    } while (head->link != NULL);
  prev->link = NULL;
  }
  return head;
}

// Get a pointer to the buffer at the head of a queue
AshBuffer *ashQueueHead(AshQueue *queue)
{
  AshBuffer *head;

  head = queue->tail;
  if (head == NULL) {
    ashTraceEvent("Tried to access head in an empty queue\r\n");
    assert(FALSE);
  }
  if(head != NULL) {
    while (head->link != NULL)
      head = head->link;
  }
  return head;
}

// Get a pointer to the Nth entry in a queue (the tail corresponds to N = 1)
AshBuffer *ashQueueNthEntry(AshQueue *queue, int8u n)
{
  AshBuffer *buffer;

  if (n == 0) {
    ashTraceEvent("Asked for 0th element in queue\r\n");
    assert(FALSE);
  }
  buffer = queue->tail;
  while (--n) {
    if (buffer == NULL) {
      ashTraceEvent("Less than N entries in queue\r\n");
      assert(FALSE);
    }
    buffer = buffer->link;
  } 
  return buffer;  
}

// Get a pointer to the entry before the specified entry (closer to the tail).
// If the buffer specified is NULL, the head entry is returned.
// If the buffer specified is the tail, NULL is returned;
AshBuffer *ashQueuePrecedingEntry(AshQueue *queue, AshBuffer *buffer)
{
  AshBuffer *ptr;

  ptr = queue->tail;
  if (ptr == buffer) {
    return NULL;
  } else {
    do {
      if (ptr->link == buffer) {      
        return ptr;
      }
      ptr = ptr->link;
    } while(ptr != NULL);
    ashTraceEvent("Buffer not in queue\r\n");
    assert(FALSE);
    return NULL;
  }
}

// Remove the specified entry from a queue, return a pointer to the preceding
// entry (if any).
AshBuffer *ashRemoveQueueEntry(AshQueue *queue, AshBuffer *buffer)
{
  AshBuffer *ptr;

  ptr = ashQueuePrecedingEntry(queue, buffer);
  if (ptr != NULL) {
    ptr->link = buffer->link;
  } else {
    queue->tail = buffer->link;
  }
  return ptr;
}

// Get the number of buffers in a queue
int8u ashQueueLength(AshQueue *queue)
{
  AshBuffer *head;
  int8u count;

  head = queue->tail;
  for (count = 0; head != NULL; count++)
    head = head->link;
  return count;
}

// Get the number of buffers in a free list
int8u ashFreeListLength(AshFreeList *list)
{
  AshBuffer *next;
  int8u count;

  next = list->link;
  for (count = 0; next != NULL; count++)
    next = next->link;
  return count;
}

// Add a buffer to the tail of a queue
void ashAddQueueTail(AshQueue *queue, AshBuffer *buffer)
{
  if (buffer == NULL) {
    ashTraceEvent("Called ashAddQueueTail with NULL buffer\r\n");
    assert(FALSE);
  }
  buffer->link = queue->tail;
  queue->tail = buffer;
}

// Return whether or not the queue is empty
boolean ashQueueIsEmpty(AshQueue *queue)
{
  return (queue->tail == NULL);
}

//------------------------------------------------------------------------------
// Test functions

#ifdef ASH_QUEUE_TEST

void ashQueueTest(void)
{
  static boolean alreadyRan = FALSE;
  int32u status;

  if (!alreadyRan) {
    alreadyRan = TRUE;
    status = ashInternalQueueTest();
    if (status) {
      printf("ASH queue test failed - error = %d\r\n", status);
    } else {
      printf("ASH queue test passed\r\n");
    }
    ashInitQueues();
  }
}

// Returns 0 if all tests pass, otherwise the number of the first test to fail.
static int32u ashInternalQueueTest(void)
{
  int8u i;
  AshBuffer *buf, *bufx;

  ashInitQueues();
  if (!ashQueueIsEmpty(&txQueue))
    return 10;
  if (!ashQueueIsEmpty(&reTxQueue))
    return 20;
  if (!ashQueueIsEmpty(&rxQueue))
    return 30;
  if (txFree.link == NULL)
    return 40;
  if (rxFree.link == NULL)
    return 50;
  if (ashFreeListLength(&txFree) != TX_POOL_BUFFERS)
    return 60;

  for (i = 1; ; i++) {
    buf = ashAllocBuffer(&txFree);
    if (buf == NULL) {
      if (i != TX_POOL_BUFFERS+1)
        return 70;
      break;
    }
    if (i > TX_POOL_BUFFERS)
      return 80;
    buf->len = i;
    ashAddQueueTail(&txQueue, buf);
  }
  if (ashQueueLength(&txQueue) != TX_POOL_BUFFERS)
    return 90;
  if (ashFreeListLength(&txFree) != 0)
    return 100;

  for (i = 1; i <= TX_POOL_BUFFERS; i++) {
    buf = ashQueueNthEntry(&txQueue, i);
    if (buf->len != TX_POOL_BUFFERS - i + 1)
      return 110;
  }

  for (i = 1; i <= TX_POOL_BUFFERS; i++) {
    buf = ashQueueHead(&txQueue);
    if (buf == NULL)
      return 120;
    if (buf->len != i)
      return 130;
    buf = ashRemoveQueueHead(&txQueue);
    if (buf == NULL)
      return 140;
    if (buf->len != i)
      return 150;

    ashFreeBuffer(&txFree, buf);
  }
  if (!ashQueueIsEmpty(&txQueue))
    return 170;

  buf = ashQueuePrecedingEntry(&txQueue, NULL);
  if (buf != NULL)
      return 180;

  for (i = 1; ; i++) {
    buf = ashAllocBuffer(&txFree);
    if (buf == NULL) {
      break;
    }
    buf->len = i;
    ashAddQueueTail(&txQueue, buf);
  }

  bufx = NULL;
  for (i = 1; i <= TX_POOL_BUFFERS; i++) {
    buf = ashQueuePrecedingEntry(&txQueue, bufx);
    bufx = buf;
    if (buf->len != i)
      return 190;
  }
  buf = ashQueuePrecedingEntry(&txQueue, bufx);
  if (buf != NULL)
    return 200;

  bufx = ashQueuePrecedingEntry(&txQueue, NULL);
  buf = ashRemoveQueueEntry(&txQueue, bufx);
  if (bufx->len != 1)
    return 210;
  if (buf->len != 2)
    return 220;
  if (ashQueueLength(&txQueue) != (TX_POOL_BUFFERS-1))
    return 230;
  ashFreeBuffer(&txFree, buf);
  bufx = buf;

  buf = ashQueuePrecedingEntry(&txQueue, bufx);
  if (buf->len != 3)
    return 240;
  bufx = buf;

  buf = ashRemoveQueueEntry(&txQueue, bufx);
  if (buf->len != 4)
    return 230;
  ashFreeBuffer(&txFree, buf);
  bufx = buf;

  txQueue.tail = NULL;
  txFree.link = NULL;
  for (buf = ashTxPool; buf < &ashTxPool[TX_POOL_BUFFERS]; buf++)
    ashFreeBuffer(&txFree, buf);
  for (i = 1; ; i++) {
    buf = ashAllocBuffer(&txFree);
    if (buf == NULL) {
      break;
    }
    buf->len = i;
    ashAddQueueTail(&txQueue, buf);
  }

  buf = ashRemoveQueueEntry(&txQueue, txQueue.tail);
  if (buf != NULL)
    return 240;
  if (ashQueueLength(&txQueue) != (TX_POOL_BUFFERS-1))
    return 250;

  return 0;
}
#endif // ifdef ASH_QUEUE_TEST
