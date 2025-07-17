#pragma once

#include "types.h"
#include "list.h"

#define PAGE_SIZE    4096
#define PAGE_NBITS   12
#define NUM_PAGES    64 // size of process' address space

#define PERMS_RWX  0b111
#define PERMS_RO   0b001
#define PERMS_RX   0b101
#define PERMS_RW   0b011
#define PERMS_NONE 0b000

// grab bit [b] from [x]
#define BIT(x, b) (((1 << b) & x))

// pseudo page table entry corresponds a page to a frame 
typedef struct _ppte {
    uint frame_num, perms;
    bool present;
} ppte;

// pseudo page table is a fixed collection of pptes.
// every process contains a pseudo page table.
typedef struct _pseudopgtbl {
    ppte tbl[NUM_PAGES];
} pseudopgtbl;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// a segment is an (address, size) pair associated with a maximum access
// permission allowed on any page in the segment.
// some examples include: code, data, heap, stack
typedef struct _segment {
    uint address, size, perms_max;
    int ino;
    uint offset;
} segment;

// a segment table is a collection of segments
// every process contains a segment table
typedef struct _segmenttbl {
    list_t segments;
} segmenttbl;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// a coremap entry contains metadata on a physical frame
typedef struct _coremap_entry {
    uint refcnt;
} coremap_entry;
