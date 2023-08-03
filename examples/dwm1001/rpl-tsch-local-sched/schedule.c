#include "schedule.h"
#include "tsch-schedule.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-schedule.h"

void load_schedule(struct schedule_link *links, size_t num_links) {
    // first create new slotframe
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(2, 20);

    for (size_t i = 0; i < num_links; i++) {
        struct schedule_link *link = &links[i];
        tsch_schedule_add_link(
            sf_eb,
            link->link_options,
            link->link_type,
            link->addr,
            link->timeslot_offset,
            link->channel_offset);
    }
}


void schedule_callback_routing_child_added(const linkaddr_t *addr) {
    printf("schedule_callback_routing_child_added\n");
    printf("Installing ranging schedule for %02x:%02x\n", addr->u8[0], addr->u8[1]);
    // range every 10 slots
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(1, 10);

    // add one LINK_TYPE_PROP link
    // depending if our address ends on 0xFE we are sender
    // or receiver
    if (addr->u8[LINKADDR_SIZE-1] == 0xFE) {
        tsch_schedule_add_link(
            sf_eb,
            LINK_OPTION_TX,
            LINK_TYPE_PROP,
            addr,
            0,
            0);
    } else {
        tsch_schedule_add_link(
            sf_eb,
            LINK_OPTION_RX,
            LINK_TYPE_PROP,
            addr,
            0,
            0);
    }
}

void schedule_callback_routing_child_removed(const linkaddr_t *addr) {
    printf("schedule_callback_routing_child_removed\n");
}
