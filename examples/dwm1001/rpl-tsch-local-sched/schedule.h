#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-schedule.h"

struct schedule_link {
    const linkaddr_t *addr;
    uint16_t timeslot_offset;
    uint16_t channel_offset;
    uint8_t link_options;
    enum link_type link_type;
};

void load_schedule(struct schedule_link *links, size_t num_links);


void schedule_callback_routing_child_added(const linkaddr_t *addr);
void schedule_callback_routing_child_removed(const linkaddr_t *addr);

#endif // SCHEDULE_H

