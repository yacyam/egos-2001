/**
 * pretty much a queue but used differently
 */
#include "queue.h"

typedef queue_t list_t;
typedef queue_func_t list_func_t;

/**
 * return new list (or NULL if OOM)
 */ 
list_t list_new();

/**
 * append item to front of list (LIFO)
 */ 
int list_append(list_t list, void* item);

/*
 * Call f(item, context) for each item in list
 */
void list_iterate(const list_t list, list_func_t f, void* context);

/*
 * Return the number of items in the list.
 */
int list_length(const list_t list);

/*
 * Delete the first instance (the first to be popped) of the specified
 * item from the given list.
 * Return 0 if the item was deleted, or -1 otherwise (item wasn't found).
 */
int list_delete(list_t list, void* item);