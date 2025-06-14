/**
 * basic malloc implementation
 */
#include "kmem.h"

#define MAGIC (void*)0x91531CCA // lets the library know the freelist hasn't been set up yet

/* in the data segment of the kernel */
memregion_info_t freelisthead = MAGIC;

/**
 * __memregion_split: Create a new region of `size` bytes inside the data portion
 * of the memory region `region`. Returns a pointer to the base of the newly constructed 
 * memory region. This function has no side effects on the freelisthead (but has side 
 * effects on `region`).
 * 
 * Visual depiction: 
 * Region: 
 * < metadata | data >
 * ->
 * < metadata | data_shunken >< metadata_new | data_new >
 * 
 * Where:
 * sizeof(data_new) == `size` and
 * sizeof(data) == sizeof(data_shrunken) + sizeof(metadata_new) + sizeof(data_new)
 */
memregion_info_t __memregion_split(memregion_info_t region, uint size) {
    if (region->size <= size + sizeof(struct memregion_info))
        FATAL("__memregion_split: do not have enough space to split region");
    
    memregion_info_t region_new = (memregion_info_t)(
        (uint)region + region->size - size // the metadata sizes cancel
    );

    region->size = region->size - size - sizeof(struct memregion_info);
    
    region_new->size = size;
    region_new->next = EGOSNULL;
    return region_new;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**
 * __freelist_setup: construct a single memory region for this library to allocate 
 * from, and point freelisthead to it.
 */
void __freelist_setup() {
    freelisthead = (memregion_info_t)HEAP_START;
    freelisthead->next = EGOSNULL;
    freelisthead->size = HEAP_END - HEAP_START - sizeof(struct memregion_info);
}

/**
 * __freelist_push: push `region` to the head of the free list.
 * TODO: first scan for merge?
 */
void __freelist_push(memregion_info_t region) {
    memregion_info_t freelisthead_old = freelisthead;
    freelisthead = region;
    region->next = freelisthead_old;
}

/**
 * __freelist_find(size) -> *memregion: Obtain a memory region
 * that can handle a data region of `size` bytes. Returns a pointer to the 
 * base of the region.
 */
memregion_info_t __freelist_find(uint size) {
    if (freelisthead == MAGIC)    FATAL("freelist_find: freelist uninitialized");
    if (freelisthead == EGOSNULL) FATAL("freelist_find: freelist empty");

    memregion_info_t *region_ptr = &freelisthead;
    while (*region_ptr != EGOSNULL) {
        memregion_info_t region = *region_ptr;

        // can only split a region if there is enough space for the metadata of the newly created region as well.
        // be careful about subtraction! (only do addition here)
        if (sizeof(struct memregion_info) + size < region->size) {
            return __memregion_split(region, size);
        }
        if (size <= region->size) {
            *region_ptr = region->next;
            region->next = EGOSNULL;
            return region;
        }

        region_ptr = &region->next;
    }

    FATAL("__frelist_find: could not find region of %x bytes", size);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void *egosalloc(uint size) {
    if (freelisthead == MAGIC) __freelist_setup();
    return (void*)((uint)__freelist_find(size) + sizeof(struct memregion_info));
}

void *egozalloc(uint size) {
    char *p = egosalloc(size);
    for (int i = 0; i < size; i++) p[i] = '\0';
    return p;
}

void egosfree(void *ptr) {
    __freelist_push((memregion_info_t)((uint)ptr - sizeof(struct memregion_info)));
}