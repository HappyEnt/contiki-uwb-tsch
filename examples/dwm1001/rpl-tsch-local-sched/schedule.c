
/*
 * This file was automatically generated by the scheduler.
 *
 * Do not edit this file directly.
 *
 */

#include "schedule.h"

void init_custom_schedule()
{
    const struct my_slotframe *ms = NULL;

    printf("loading schedule for %d nodes\n", 4);

    for (size_t i = 0; i < 4; i++) {
        if (linkaddr_cmp(slotframes[i].hostAddress, &linkaddr_node_addr)) {
          ms = &slotframes[i];
          printf("found slotframe for %d\n", i);
        }
    }

    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, ms->length);

    for (size_t i = 0; i < ms->linkAmount; i++) {
        struct my_link *link = &ms->links[i];
        tsch_schedule_add_link(
            sf_eb,
            link->link_options,
            link->link_type,
            link->addr,
            link->timeslot_offset,
            link->channel_offset);
    }
}

/* Link addresses */

const linkaddr_t node_0_ll = { { 0xDE, 0xCA, 0x22, 0xB2, 0x6D, 0xFD, 0x0B, 0xFE } };
const linkaddr_t node_1_ll = { { 0xDE, 0xCA, 0x21, 0x64, 0xB0, 0x6F, 0x4B, 0xFB } };
const linkaddr_t node_2_ll = { { 0xDE, 0xCA, 0xFB, 0x15, 0x64, 0xC0, 0x87, 0xE9 } };
const linkaddr_t node_3_ll = { { 0xDE, 0xCA, 0x94, 0xB7, 0x2A, 0xCA, 0x34, 0xE6 } };

/* Schedule Set */
const struct my_slotframe slotframes[] =
{


{
  .hostAddress = &node_0_ll,
  .length = 25,
  .links = (struct my_link []) {
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 1,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 2,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 3,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 4,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 5,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 6,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 7,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 8,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 9,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 10,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 11,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 12,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 13,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 14,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 15,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 16,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 17,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 18,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 19,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 20,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 21,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 22,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 23,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 24,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 0,
        .channel_offset = 0,
        .link_type = LINK_TYPE_ADVERTISING,
        .link_options = LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING
        },
        
   },
   .linkAmount = 25
},

{
  .hostAddress = &node_1_ll,
  .length = 25,
  .links = (struct my_link []) {
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 2,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 1,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 3,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 4,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 6,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 5,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 7,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 8,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 10,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 9,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 11,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 12,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 14,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 13,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 15,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 16,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 18,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 17,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 19,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 20,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 22,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 21,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 23,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 24,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 0,
        .channel_offset = 0,
        .link_type = LINK_TYPE_ADVERTISING,
        .link_options = LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING
        },
        
   },
   .linkAmount = 25
},

{
  .hostAddress = &node_2_ll,
  .length = 25,
  .links = (struct my_link []) {
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 3,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 1,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 2,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 4,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 7,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 5,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 6,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 8,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 11,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 9,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 10,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 12,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 15,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 13,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 14,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 16,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 19,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 17,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 18,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 20,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 23,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 21,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 22,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_3_ll,
        .timeslot_offset = 24,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 0,
        .channel_offset = 0,
        .link_type = LINK_TYPE_ADVERTISING,
        .link_options = LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING
        },
        
   },
   .linkAmount = 25
},

{
  .hostAddress = &node_3_ll,
  .length = 25,
  .links = (struct my_link []) {
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 4,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 1,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 2,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 3,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 8,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 5,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 6,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 7,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 12,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 9,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 10,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 11,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 16,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 13,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 14,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 15,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 20,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 17,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 18,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 19,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 24,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_TX
        },
        
        {
        .addr = &node_0_ll,
        .timeslot_offset = 21,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_1_ll,
        .timeslot_offset = 22,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &node_2_ll,
        .timeslot_offset = 23,
        .channel_offset = 0,
        .link_type = LINK_TYPE_PROP_MTM,
        .link_options = LINK_OPTION_RX
        },
        
        {
        .addr = &tsch_broadcast_address,
        .timeslot_offset = 0,
        .channel_offset = 0,
        .link_type = LINK_TYPE_ADVERTISING,
        .link_options = LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING
        },
        
   },
   .linkAmount = 25
},

};