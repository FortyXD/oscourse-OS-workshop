#ifndef JOS_INC_FIFO_H
#define JOS_INC_FIFO_H

#include <inc/assert.h>
#include <inc/mmu.h>
#include <inc/types.h>

struct Fifo {
    volatile uint32_t lock;
    volatile uint32_t readers;
    volatile uint32_t writers;
    off_t rpos;
    off_t wpos;
    uint8_t buf[PAGE_SIZE - (3 * sizeof(uint32_t)) - (2 * sizeof(off_t))];
};

static_assert(sizeof(struct Fifo) == PAGE_SIZE, "Fifo must fit in one page");

#endif /* JOS_INC_FIFO_H */
