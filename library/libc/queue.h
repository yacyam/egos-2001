/*
 * Generic queue manipulation functions
 *
 * Except for queue_delete() and queue_iterate(), all operations
 * should be O(1), i.e., their execution time should not grow
 * with the length of the queue.
 */

/*
 * queue_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how queues are
 * represented.  They see and manipulate only queue_t's.
 */
typedef struct queue *queue_t;

/*
 * Return an empty queue.  Returns NULL on error (out of memory).
 */
queue_t queue_new();

/*
 * Enqueue an item.
 * Return 0 (success) or -1 (failure).  (A reason for failure may be
 * that there is not enough memory left.)
 */
int queue_enqueue(queue_t queue, void* item);

/*
 * Prepend an item to the beginning of the queue.  This should be returned
 * first on the next dequeue.
 * Return 0 (success) or -1 (failure).  (A reason for failure may be
 * that there is not enough memory left.)
 */
int queue_insert(queue_t queue, void* item);

/*
 * Dequeue and return the first item from the queue in *pitem, but only if
 * pitem != NULL.  If pitem == NULL, then just dequeue.
 * Return 0 (success) if queue is nonempty, or -1 (failure)
 * if queue is empty.
 */
int queue_dequeue(queue_t queue, void** pitem);

/*
 * Call f(item, context) for each item in queue.
 */
typedef void (*queue_func_t)(void* item, void* context);
void queue_iterate(const queue_t queue, queue_func_t f, void* context);

/*
 * Free the queue.  The queue must be empty.
 * Return 0 (success) or -1 (failure) if queue is non-empty.
 */
int queue_free(queue_t queue);

/*
 * Return the number of items in the queue.
 */
int queue_length(const queue_t queue);

/*
 * Delete the first instance (the first to be dequeued) of the specified
 * item from the given queue.
 * Return 0 if the item was deleted, or -1 otherwise (item wasn't found).
 */
int queue_delete(queue_t queue, void* item);
