#include "kmem.h"
#include "queue.h"

typedef queue_t list_t;

list_t list_new() { return queue_new(); }

int list_append(list_t list, void *item) { return queue_insert(list, item); }
