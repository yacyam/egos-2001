#pragma once

#include "types.h"
#include "list.h"

#define PAGE_SIZE    4096
#define PAGE_NBITS   12
#define NUM_PAGES    16 // size of process' address space
#define BLOCKS_PER_PAGE (PAGE_SIZE / BLOCK_SIZE)

#define PERMS_RWX  0b111
#define PERMS_RO   0b001
#define PERMS_RX   0b101
#define PERMS_RW   0b011
#define PERMS_NONE 0b000

#define EXCP_ID_FAULT_X 1
#define EXCP_ID_FAULT_R 5
#define EXCP_ID_FAULT_W 7

// grab bit [b] from [x]
#define BIT(x, b) (((1 << b) & x))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// a segment is an (address, size) pair associated with a maximum access
// permission allowed on any page in the segment.
// some examples include: code, data, heap, stack
typedef struct _segment {
    uint page_base, num_pages, perms_max;
    int in_file;
    uint ino, offset;
} segment;

// a segment table is a collection of segments
// every process contains a segment table
typedef struct _segmenttbl {
    list_t segments;
} segmenttbl;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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

typedef struct _ppagefault_reason {
    uint page_num, excp_id;
    segment *seg_containing_fault;
} ppagefault_reason;


static inline int _convert_excp_id_to_perms(uint excp_id) {
    switch (excp_id) {
    case EXCP_ID_FAULT_R: return 0b001;
    case EXCP_ID_FAULT_W: return 0b010;
    case EXCP_ID_FAULT_X: return 0b100;
    default: return -1;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// a coremap entry contains metadata on a physical frame
typedef struct _coremap_entry {
    uint refcnt;
    // what happens when the OS needs to forcefully reclaim a frame?
    // need more metadata to make reclamation efficient.
} coremap_entry;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define NUM_PMPSLOTS         NUM_PAGES
#define NUM_PMPSLOTS_PER_CFG 4
#define PMP_REGION_NAPOT     0b11000

#define PMP_34BIT_CONVERT(addr) (((addr) >> 2))

static inline void _pmp_addr_write(uint slot, uint val) {
    // the nasty table... :(
    switch (slot) {
        case 0: asm("csrw pmpaddr0, %0"::"r"(val)); break;
        case 1: asm("csrw pmpaddr1, %0"::"r"(val)); break;
        case 2: asm("csrw pmpaddr2, %0"::"r"(val)); break;
        case 3: asm("csrw pmpaddr3, %0"::"r"(val)); break;
        case 4: asm("csrw pmpaddr4, %0"::"r"(val)); break;
        case 5: asm("csrw pmpaddr5, %0"::"r"(val)); break;
        case 6: asm("csrw pmpaddr6, %0"::"r"(val)); break;
        case 7: asm("csrw pmpaddr7, %0"::"r"(val)); break;
        case 8: asm("csrw pmpaddr8, %0"::"r"(val)); break;
        case 9: asm("csrw pmpaddr9, %0"::"r"(val)); break;
        case 10: asm("csrw pmpaddr10, %0"::"r"(val)); break;
        case 11: asm("csrw pmpaddr11, %0"::"r"(val)); break;
        case 12: asm("csrw pmpaddr12, %0"::"r"(val)); break;
        case 13: asm("csrw pmpaddr13, %0"::"r"(val)); break;
        case 14: asm("csrw pmpaddr14, %0"::"r"(val)); break;
        case 15: asm("csrw pmpaddr15, %0"::"r"(val)); break;
    }
}

static inline void _pmp_cfg_set(uint slot, uint val) {
    #define BITLEN_SLOT 8
    uint shift_amt_slot = BITLEN_SLOT * (slot % NUM_PMPSLOTS_PER_CFG);
    switch (slot / NUM_PMPSLOTS_PER_CFG) {
        case 0: 
            asm("csrc pmpcfg0, %0"::"r"(0xFF << shift_amt_slot));
            asm("csrs pmpcfg0, %0"::"r"(val << shift_amt_slot)); 
            break;
        case 1:
            asm("csrc pmpcfg1, %0"::"r"(0xFF << shift_amt_slot));
            asm("csrs pmpcfg1, %0"::"r"(val << shift_amt_slot)); 
            break;
        case 2:
            asm("csrc pmpcfg2, %0"::"r"(0xFF << shift_amt_slot));
            asm("csrs pmpcfg2, %0"::"r"(val << shift_amt_slot)); 
            break;
        case 3: 
            asm("csrc pmpcfg3, %0"::"r"(0xFF << shift_amt_slot));
            asm("csrs pmpcfg3, %0"::"r"(val << shift_amt_slot)); 
            break;
    }
}

static inline uint _pmp_cfg_get(uint slot) {
    // TODO: improve this
    uint val = 0;
    switch (slot) {
        case 0:  case 1:  case 2:  case 3: asm("csrr %0, pmpcfg0":"=r"(val)); break;
        case 4:  case 5:  case 6:  case 7: asm("csrr %0, pmpcfg1":"=r"(val)); break;
        case 8:  case 9:  case 10: case 11: asm("csrr %0, pmpcfg2":"=r"(val)); break;
        case 12: case 13: case 14: case 15: asm("csrr %0, pmpcfg3":"=r"(val)); break;
    }
    return val;
}
