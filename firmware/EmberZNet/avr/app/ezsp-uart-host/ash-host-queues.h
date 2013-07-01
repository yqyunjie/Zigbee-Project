/** @file ash-host-queues.h
 * @brief Header for ASH host queue functions
 *
 * See @ref ash_util for documentation.
 *
 * <!-- Copyright 2006 by Ember Corporation. All rights reserved.-->   
 */

/** @addtogroup ash_util
 *
 * See ash-host-queues.h.
 *
 *@{
 */

#ifndef __ASH_HOST_QUEUE_H__
#define __ASH_HOST_QUEUE_H__

/** @brief The number of transmit buffers must be set to the number of receive buffers
* -- to hold the immediate ACKs sent for each callabck frame received --
* plus 3 buffers for the retransmit queue and one each for an automatic ACK
* (due to data flow control) and a command.
*/
#define TX_POOL_BUFFERS   (EZSP_HOST_ASH_RX_POOL_SIZE + 5)

/** @brief Define the limits used to decide if the host will hold off the ncp from
* sending normal priority frames.
*/
#define RX_FREE_LWM 8
#define RX_FREE_HWM 12

/** @brief Buffer to hold a DATA frame.
*/
typedef struct ashBuffer {
  struct ashBuffer *link; 
  int8u len; 
  int8u data[ASH_MAX_DATA_FIELD_LEN];
} AshBuffer;

/** @brief Simple queue (singly-linked list)
*/
typedef struct {
  AshBuffer *tail;
} AshQueue;

/** @brief Simple free list (singly-linked list)
*/
typedef struct {
  AshBuffer *link;
} AshFreeList;

/** @brief Initializes all queues and free lists. 
 *  All receive buffers are put into rxFree, and rxQueue is empty.
 *  All transmit buffers are put into txFree, and txQueue and reTxQueue are
 *  empty.
 */
void ashInitQueues(void);

/** @brief  Add a buffer to the free list.
 *
 * @param list    pointer to the free list 
 * @param buffer  pointer to the buffer
 */
void ashFreeBuffer(AshFreeList *list, AshBuffer *buffer);

/** @brief  Get a buffer from the free list.
 *
 * @param list    pointer to the free list 
 * 
 * @return        pointer to the buffer allocated, NULL if free list was empty
 */
AshBuffer *ashAllocBuffer(AshFreeList *list);

/** @brief  Remove the buffer at the head of a queue. The queue must not
 *  be empty.
 *
 * @param queue   pointer to the queue
 *
 * @return        pointer to the buffer that had been the head of the queue
 */
AshBuffer *ashRemoveQueueHead(AshQueue *queue);

/** @brief  Get a pointer to the buffer at the head of the queue. The
 *  queue must not be empty.
 *
 * @param queue   pointer to the queue
 *
 * @return        pointer to the buffer at the nead of the queue
 */
AshBuffer *ashQueueHead(AshQueue *queue);

/** @brief  Get a pointer to the Nth entry in a queue. The tail is entry
 *  number 1, and if the queue has N entries, the head is entry number N.
 *  The queue must not be empty.
 *
 * @param queue   pointer to the queue
 * @param n       number of the entry to which a pointer will be returned
 *
 * @return        pointer to the Nth queue entry
 */
AshBuffer *ashQueueNthEntry(AshQueue *queue, int8u n);

/** @brief  Get a pointer to the queue entry before (closer to the tail)
 *  than the specified entry. 
 *  If the entry specified is the tail, NULL is returned.
 *  If the entry specifed is NULL, a pointer to the head is returned.
 *
 * @param queue   pointer to the queue
 * @param buffer  pointer to the buffer whose predecessor is wanted
 *
 * @return        pointer to the buffer before that specifed, or NULL if none
 */
AshBuffer *ashQueuePrecedingEntry(AshQueue *queue, AshBuffer *buffer);

/** @brief  Removes the buffer from the queue, and returns a pointer to
 * its predecssor, if there is one, otherwise it returns NULL.
 *
 * @param queue   pointer to the queue
 * @param buffer  pointer to the buffer to be removed
 *
 * @return        pointer to the buffer before that removed, or NULL if none
 */
AshBuffer *ashRemoveQueueEntry(AshQueue *queue, AshBuffer *buffer);

/** @brief  Returns the number of entries in the queue.
 *
 * @param queue   pointer to the queue
 *
 * @return        number of entries in the queue
 */
int8u ashQueueLength(AshQueue *queue);


/** @brief  Returns the number of entries in the free list.
 *
 * @param list    pointer to the free list
 *  
 * @return        number of entries in the free list
 */
int8u ashFreeListLength(AshFreeList *list);

/** @brief Add a buffer to the tail of the queue.
 *
 * @param queue   pointer to the queue
 * @param buffer  pointer to the buffer
 */
void ashAddQueueTail(AshQueue *queue, AshBuffer *buffer);

/** @brief  Returns TRUE if the queue is empty.
 *
 * @param queue   pointer to the queue
 *
 * @return        TRUE if the queue is empty
 */
boolean ashQueueIsEmpty(AshQueue *queue);

extern AshQueue txQueue;
extern AshQueue reTxQueue;
extern AshQueue rxQueue;
extern AshFreeList txFree;
extern AshFreeList rxFree;

#endif //__ASH_HOST_QUEUE_H___

/** @} // END addtogroup
 */
