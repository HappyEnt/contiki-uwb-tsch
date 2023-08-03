#ifndef __TSCH_MTM_H__
#define __TSCH_MTM_H__

#include "contiki.h"

struct rx_stack {
    struct rx_stack *next;

    uint64_t rx_time;
    struct tsch_neighbor *n;
};

#endif // __TSCH_MTM_H__
