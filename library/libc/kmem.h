#pragma once
#include "egos.h"

/**
 * basic malloc implementation:
 * 
 * The malloc library manages a big chunk of memory, and partitions it into regions
 * 
 * Memory: [<Region 1><Region 2>...<Region N>]
 * 
 * Region: <metadata | data>
 * 
 * The malloc library uses the metadata associated with each region to specify
 *  1. How many bytes are in the data portion of the region (size)
 *  2. The address of another memory region that can be allocated (next)
 * 
 * The metadata of each region constructs an in-memory free list. The object
 * containing the address of the free-list head is in some memory completely
 * separate from what a process can allocate.
 * 
 * When a process wants to allocate some number of bytes of memory
 * (through malloc(size)), malloc will use a allocation policy (best-fit, first-fit) 
 * to find a memory region suitable for the allocation of `size` bytes. After finding
 * the memory region, malloc will split the region (unless the region is exactly 
 * `size` bytes), and push one partition of the split memory back onto the free list.
 * malloc will return a pointer to the data portion of the alloc'd memory region.
 * 
 * When a process wants to free a previously malloc'd region of memory
 * (through free(ptr)), the address specified by `ptr` is located at the data
 * portion of a memory region. The pointer is shifted down to the base of the
 * memory region, and pushed back onto the free list.
 */

 #define EGOSNULL (void*)0

/* the metadata of a memory region */
typedef struct memregion_info {
    uint size;
    struct memregion_info *next;
} *memregion_info_t;

void *egosalloc(uint size); // same as malloc
void *egozalloc(uint size); // almost same as calloc
void egosfree(void *ptr);
