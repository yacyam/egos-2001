/**
 * pretty much a queue but used differently
 */

typedef struct list *list_t;

// return new list (or NULL if OOM)
list_t list_new();

// append item to front of list (LIFO)
int list_append(list_t list, void* item);
