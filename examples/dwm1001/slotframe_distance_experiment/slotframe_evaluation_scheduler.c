/**
 * \file
 *         DRAND: drand protocol for automatic conflict free TDMA scheduling build on top of RIME stack
 *
 * \author El Pato
 */

#include "apps/mqtt/mqtt.h"
#include "contiki.h"
#include "linkaddr.h"
#include "net/packetbuf.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/rpl/rpl-private.h"
#include "net/rime/rime.h"
#include "tsch-private.h"


#include "slotframe_evaluation_scheduler.h"

#include "net/rime/neighbor-discovery.h"
#include "net/rime/unicast.h"
#include "tsch-schedule.h"
#include "tsch-slot-operation.h"
#include "tsch.h"
#include "toulouse-nodes.h"

#include "tsch-prop.h"
#include <stdint.h>

PROCESS(slotframe_evaluation_scheduler_process, "slotframe_evaluation");

static uint8_t current_slot_distance = 0;
static uint8_t max_inter_slot_distance;
static uint8_t node_role;
static struct etimer slotframe_length_evaluation_timer;
static struct etimer initial_wait_timer;

static void (*slotframe_new_distance_callback)(uint8_t slot_distance);

void set_slotframe_new_distance_callback(void (*callback)(uint8_t slot_distance)) {
    slotframe_new_distance_callback = callback;
}


void generate_slotframe_with_distance(uint8_t slotframe_distance) {
    // find slotframe
    // first delete old slotframe

    if (slotframe_distance > 100) {
        printf("Slotframe distance too large\n");
        return;
    }

    /* struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0); */
    /* if(sf_eb != NULL) { */
    /*     while(tsch_is_locked()) { */
    /*     } */

    tsch_schedule_remove_all_slotframes();

    /*     printf("Removed old slotframe\n"); */
    /* } */

    /* sf_eb = tsch_schedule_add_slotframe(0, required_slotframe_length); */
    // make slotframe always fixed 200 slots long, so we get one measurement every second
    struct tsch_slotframe *sf_eb = NULL;
    uint16_t slotframe_length = 0;



#if WITH_SYMMETRIC_DELAYS
    if (slotframe_distance < 1 + WITH_EVAL_PROP_SLOT) {
        printf("Slotframe distance too small\n");
        return;
    }
    slotframe_length = 2 + (2*slotframe_distance);
#elif WITH_SHORT_REPLY_LONG_DELAY
    slotframe_length = slotframe_distance + 4;
#else
    slotframe_length = 100;
#endif
    sf_eb = tsch_schedule_add_slotframe(0, slotframe_length);

    if (sf_eb != NULL) {
        // initial shared slot

        // this is always present
        tsch_schedule_add_link(sf_eb,
            LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
            LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 0, 0);

        uint8_t round_start = 1;

        // first direction
        if(node_role == 0) {
            /* printf("Setting initiator\n"); */

#if WITH_EVAL_PROP_SLOT
            tsch_schedule_add_link(sf_eb,
                LINK_OPTION_TX,
                LINK_TYPE_PROP, &dwm1001_9_ll, round_start, 0);
            round_start = 2;
#endif

#if WITH_SYMMETRIC_DELAYS
            round_start += slotframe_distance-(1 + WITH_EVAL_PROP_SLOT);
#endif

#if WITH_INTERLEAVE_DSTWR
            for (int i = 2; i < slotframe_length; i += 3) {
                if( i != round_start && i != round_start + 1 + slotframe_distance) {
                    // every fourth iteration add a advertisment slot
                    tsch_schedule_add_link(sf_eb,
                        (i % 2 ? LINK_OPTION_RX : LINK_OPTION_TX),
                        LINK_TYPE_PROP, &dwm1001_9_ll, i, 0);
                    /* tsch_schedule_add_link(sf_eb, */
                    /*     LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING, */
                    /*     LINK_TYPE_ADVERTISING, &tsch_broadcast_address, i, 0); */

                    /* tsch_schedule_add_link(sf_eb, */
                    /*     (i % 2 ? LINK_OPTION_RX : LINK_OPTION_TX), */
                    /*     LINK_TYPE_PROP, &dwm1001_2_ll, i, 0); */
                }
            }
            /* if(slotframe_distance > 2) { */
            /*     tsch_schedule_add_link(sf_eb, */
            /*         LINK_OPTION_TX, */
            /*         LINK_TYPE_PROP, &dwm1001_2_ll, round_start + 1 + slotframe_distance - 1, 0); */
            /* } */
#endif
            printf("put %u %u\n", round_start, round_start + 1 + slotframe_distance);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start + 1 + slotframe_distance, 0);
        } else if (node_role == 1) {
            /* printf("Setting responder\n"); */

#if WITH_EVAL_PROP_SLOT
            tsch_schedule_add_link(sf_eb,
                LINK_OPTION_RX,
                LINK_TYPE_PROP, &dwm1001_10_ll, round_start, 0);
            round_start = 2;
#endif

#if WITH_SYMMETRIC_DELAYS
            round_start += slotframe_distance-(1 + WITH_EVAL_PROP_SLOT);
#endif


#if WITH_INTERLEAVE_DSTWR
            for (int i = 2; i < slotframe_length; i += 3) {
                if( i != round_start && i != round_start + 1 + slotframe_distance) {
                    tsch_schedule_add_link(sf_eb,
                        (i % 2 ? LINK_OPTION_TX : LINK_OPTION_RX),
                        LINK_TYPE_PROP, &dwm1001_10_ll, i, 0);

                    /* tsch_schedule_add_link(sf_eb, */
                    /*     LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING, */
                    /*     LINK_TYPE_ADVERTISING, &tsch_broadcast_address, i, 0); */

                }
            }
            /* if(slotframe_distance > 2) { */
            /*     tsch_schedule_add_link(sf_eb, */
            /*         LINK_OPTION_RX, */
            /*         LINK_TYPE_PROP, &dwm1001_1_ll, round_start + 1 + slotframe_distance - 1, 0); */
            /* }             */
#endif

            // back direction
            printf("put %u %u\n", round_start, round_start + 1 + slotframe_distance);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start + 1 + slotframe_distance, 0);
        } else {

#if WITH_EVAL_PROP_SLOT
            round_start = 2;
#endif


#if WITH_SYMMETRIC_DELAYS
            round_start += slotframe_distance-(1 + WITH_EVAL_PROP_SLOT);
#endif

            /* printf("Setting passive\n"); */
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, round_start + 1 + slotframe_distance, 0);
        }


        mtm_set_round_slots(round_start, round_start + 1 + slotframe_distance);

// interleave some rx slots, to wake up radio
/* #define WITH_ADD_ADDITIONAL_SHARED 1 */
/* #if WITH_ADD_ADDITIONAL_SHARED */
/*         // interleave some more ADVERTISING_ONLY Timeslots in case of long slotframes */
/*         int amount = (slotframe_distance - 1) > 10 ? 10 : (slotframe_distance - 1); */
/*         if(amount > 0) { */
/*             int spacing = (slotframe_distance - 1) / amount; */

/*             for (int i = 0; i < amount; i++) { */
/*                 tsch_schedule_add_link(sf_eb, */
/*                     LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING, */
/*                     LINK_TYPE_ADVERTISING, &tsch_broadcast_address, round_start + i*spacing, 0); */
/*                 tsch_schedule_add_link(sf_eb, */
/*                     LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING, */
/*                     LINK_TYPE_ADVERTISING, &tsch_broadcast_address, slotframe_distance + 1 + i*spacing, 0); */
/*             } */
/*         } */
/* #endif         */

        /* printf("Added new slotframe\n"); */

        if(slotframe_new_distance_callback != NULL) {
            slotframe_new_distance_callback(slotframe_distance);
        }
    }
}


/*
/* node_role = 0 => initiator
/* node_role = 1 => responder
/* node_role = 2 => passive listener (For TDOA)
 */
void slotframe_evaluation_length_scheduler_init(uint8_t max_distance, uint8_t role) {
    max_inter_slot_distance = max_distance;
    current_slot_distance = 1;
    node_role = role;


// we will control the experiment from the outside
#define AUTO_RUN_EXPERIMENT 0
#if AUTO_RUN_EXPERIMENT
    process_start(&slotframe_evaluation_scheduler_process, NULL);
#endif
}


PROCESS_THREAD(slotframe_evaluation_scheduler_process, ev, data)
{
  PROCESS_BEGIN();

  static uint8_t evaluation_running = 1;

  etimer_set(&initial_wait_timer, CLOCK_SECOND * 10); // wait 10 seconds before starting

  // generate one time a initial slotframe for some time so nodes can associate
  // after the next timer has triggered the same experiment will be run again for evaluation
  generate_slotframe_with_distance(current_slot_distance);

  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&initial_wait_timer));

  etimer_set(&slotframe_length_evaluation_timer, CLOCK_SECOND*60);

  while(evaluation_running) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&slotframe_length_evaluation_timer));

        if(etimer_expired(&slotframe_length_evaluation_timer)) {
            if(current_slot_distance > max_inter_slot_distance) {
                printf("CSFDFF\n"); // finished symbol
                evaluation_running = 0;
                break;
            } else {
                if(tsch_is_associated) {
                    printf("CSFD %c\n", current_slot_distance);

                    generate_slotframe_with_distance(current_slot_distance);

                    current_slot_distance++;
                }

                etimer_reset(&slotframe_length_evaluation_timer);
            }
        }
  }

  PROCESS_END();
}
