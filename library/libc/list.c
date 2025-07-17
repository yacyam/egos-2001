#include "kmem.h"
#include "queue.h"
#include "list.h"

list_t list_new() { return queue_new(); }

int list_append(list_t list, void *item) { 
    return queue_insert(list, item); 
}

int list_pop(list_t list, void **pitem) {
    return queue_pop(list, pitem);
}

void list_iterate(const list_t list, list_func_t f, void *context) { 
    queue_iterate(list, f, context); 
}

int list_length(const list_t list) { 
    return queue_length(list);
}

int list_delete(list_t list, void* item) {
    return queue_delete(list, item);
}