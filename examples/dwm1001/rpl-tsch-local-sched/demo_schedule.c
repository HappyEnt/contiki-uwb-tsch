#include "demo_schedule.h"

#include "contiki.h"
#include "memb.h"

#include "addr_map.h"

/* MEMB(schedule_memb, struct schedule_link, 1); */

struct schedule_link eb_shared_schedule[4] = {
    {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 0,
        .channel_offset = 0,
        .link_options = LINK_OPTION_TX | LINK_OPTION_TIME_KEEPING | LINK_OPTION_RX | LINK_OPTION_SHARED,
        .link_type = LINK_TYPE_ADVERTISING,
    },
    {
        .timeslot_offset = 1,
        .channel_offset = 0,
        .link_type = LINK_TYPE_NORMAL,
    },
    {
        .timeslot_offset = 2,
        .channel_offset = 0,
        .link_type = LINK_TYPE_NORMAL,
    },
    {
        .timeslot_offset = 3,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP,
    },
};

// *args: link_layer address
void init_custom_schedules(const linkaddr_t *addr) {
    // compare to our own address
    if (linkaddr_cmp(addr, &node_1_address)) {
        eb_shared_schedule[1].addr = &node_2_address;
        eb_shared_schedule[1].link_options = LINK_OPTION_TX;
        eb_shared_schedule[2].addr = &node_2_address;
        eb_shared_schedule[2].link_options = LINK_OPTION_RX;
        eb_shared_schedule[3].addr = &node_2_address;
        eb_shared_schedule[3].link_options = LINK_OPTION_TX;
    } else if (linkaddr_cmp(addr, &node_2_address)) {
        // if not equal, we are a leaf node
        // load the leaf schedule
        eb_shared_schedule[1].addr = &node_1_address;
        eb_shared_schedule[1].link_options = LINK_OPTION_RX;
        eb_shared_schedule[2].addr = &node_1_address;
        eb_shared_schedule[2].link_options = LINK_OPTION_TX;
        eb_shared_schedule[3].addr = &node_1_address;
        eb_shared_schedule[3].link_options = LINK_OPTION_RX;
    }
}

