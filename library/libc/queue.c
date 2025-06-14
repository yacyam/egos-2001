#include "egos.h"
#include "kmem.h"
#include "queue.h"

#define check(x) do { if(!(x)) { FATAL("Queue: !!check failure at line %d", __LINE__); } } while (0)

enum { ASSERT_ON, ASSERT_OFF } flag = ASSERT_ON;

/**
 * node: Structure that holds meta-data for a given item in the queue
 *
 * void  *item: Item inserted in the queue
 * node_t next: Pointer to next node in the queue (NULL if last node in queue)
 * enum { NODE_DUMMY, NODE_REAL } type:
 *  -> NODE_DUMMY: does not exist as an item inserted 
 *     into the queue by the user, and is purely used 
 *     to make queue operations cleaner. For example,
 *     queue_iterate does not apply the passed function to
 *     a dummy node's item.
 *  -> NODE_REAL: inserted by the user, and can be influenced
 *     by queue operations.
 */
typedef struct node {
    void  *item;
    struct node *next;
    enum { NODE_DUMMY, NODE_REAL } type; 
} *node_t;

/**
 * queue: Holds meta-data about the entire queue
 * 
 * node_t head, tail: Points to the head and tail of the queue.
 * In our implementation, both head and tail must be dummy nodes.
 * 
 * int size: Holds the number of REAL elements in the queue.
 */
typedef struct queue {
    node_t head, tail;
	int size;
} *queue_t;

/**
 * queue_invariants(queue_t queue): Asserts invariants that should hold about the
 * queue before and after every function (exceptions: queue_new and queue_free).
 * 
 * First: Head and Tail must always be valid DUMMY nodes, where the head always
 * has a non-NULL next pointer.
 *
 * Second: Size of queue must always be non-negative
 *
 * Third: Every node in-between must be REAL and have non-NULL next pointer
 */
void queue_invariants(queue_t queue) {
    if (flag == ASSERT_OFF) return;

    check(queue != EGOSNULL && queue->head != EGOSNULL && queue->tail != EGOSNULL);
    check(queue->size >= 0);

    check(queue->head->type == NODE_DUMMY && queue->tail->type == NODE_DUMMY);
    check(queue->head->next != EGOSNULL   && queue->tail->next == EGOSNULL);

    node_t node = queue->head->next;

    for (int i = 0; i < queue->size; i++) {
        check(node != EGOSNULL && node->type == NODE_REAL);
        node = node->next;
    }

    check(node == queue->tail);
}

queue_t queue_new() {
    /* Initialize memory for queue, and two dummy nodes */
    queue_t queue      = egozalloc(sizeof(*queue));
    node_t  dummy_head = egozalloc(sizeof(*dummy_head));
    node_t  dummy_tail = egozalloc(sizeof(*dummy_tail));

    /* Ran out of memory */
    if (queue == EGOSNULL || dummy_head == EGOSNULL || dummy_tail == EGOSNULL)
        return EGOSNULL;

    /* Initialize dummy nodes to point to one another */
    dummy_head->type = dummy_tail->type = NODE_DUMMY;
    dummy_head->next = dummy_tail;

    /* Initialize queue to place the corresponding dummy node at the head and tail */
    queue->head = dummy_head;
    queue->tail = dummy_tail;
    queue->size = 0;
    return queue;
}

int queue_push(queue_t queue, void* item) {
    queue_invariants(queue);

    node_t dummy_tail     = queue->tail;
    node_t dummy_tail_new = egozalloc(sizeof(*dummy_tail_new));

    /* Ran out of memory */
    if (dummy_tail_new == EGOSNULL) return -1;

    /* Current tail is updated to hold the enqueued item and point to the new dummy tail */
    dummy_tail->type = NODE_REAL;
    dummy_tail->item = item;
    dummy_tail->next = dummy_tail_new;

    dummy_tail_new->type = NODE_DUMMY;

    /* Queue metadata updated to hold new allocated tail */
    queue->tail = dummy_tail_new;
    queue->size++;
    queue_invariants(queue);
    return 0;
}

int queue_insert(queue_t queue, void* item) {
    queue_invariants(queue);

    node_t dummy_head     = queue->head;
    node_t dummy_head_new = egozalloc(sizeof(*dummy_head_new));

    /* Ran out of memory */
    if (dummy_head_new == EGOSNULL) return -1;

    /* Current head is updated to hold the enqueued item */
    dummy_head->type = NODE_REAL;
    dummy_head->item = item;
    
    /* New head is made into a DUMMY node and points to the old head */
    dummy_head_new->type = NODE_DUMMY;
    dummy_head_new->next = dummy_head;

    /* Queue metadata updated to hold new allocated head */
    queue->head = dummy_head_new;
    queue->size++;
    queue_invariants(queue);
    return 0;
}

int queue_pop(queue_t queue, void** pitem) {
    queue_invariants(queue);
    if (queue->size == 0) return -1;

    /* Get first real node from queue, place item from real node into [pitem] */
    node_t head_real  = queue->head->next;
    check(head_real != EGOSNULL && head_real->type == NODE_REAL);

    if (pitem != EGOSNULL) *pitem = head_real->item;

    /* Remove [head_real] from queue and free its associated memory */
    queue->head->next = head_real->next;
    egosfree(head_real);

    queue->size--;
    queue_invariants(queue);
    return 0;
}

void queue_iterate(const queue_t queue, queue_func_t f, void* context) {
    queue_invariants(queue);

    /* Begin iterating at first real node in list */
    node_t node = queue->head->next;
    for (int i = 0; i < queue->size; i++) {
        f(node->item, context);
        node = node->next;
    }

    queue_invariants(queue);
}

int queue_free(queue_t queue) {
    queue_invariants(queue);
    if (queue->size > 0) return -1;

    egosfree(queue->head);
    egosfree(queue->tail);
    egosfree(queue);
    return 0;
}

int queue_length(const queue_t queue) {
    queue_invariants(queue);
    return queue->size;
}

int queue_delete(queue_t queue, void* item) {
    queue_invariants(queue);

    /* Dummy nodes guarantee that a non-empty queue has node_prev, node_curr, and
     * node_next as non-NULL */
    node_t node_prev = queue->head;
    node_t node_curr = queue->head->next;
    node_t node_next = queue->head->next->next;

    /* Iterate through each node in queue in FIFO order to find [item] */
    for (int i = 0; i < queue->size; i++) {

        /* Once item is found, cut the node out of queue and free the node's memory */
        if (node_curr->item == item) {
            node_prev->next = node_next;
            queue->size--;
            egosfree(node_curr);
            queue_invariants(queue);
            return 0;
        }

        node_prev = node_prev->next;
        node_curr = node_curr->next;
        node_next = node_next->next;
    }

    queue_invariants(queue);
    return -1;
}
