/*
 * Copyright (c) 2015, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file
 *         Random scheduler based on MTM link statistics
 *
 * \author El Pato
 */

#include "apps/mqtt/mqtt.h"
#include "contiki.h"
#include "rand-sched.h"
#include "linkaddr.h"
#include "net/packetbuf.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/rpl/rpl-private.h"
#include "net/rime/rime.h"
#include "tsch-private.h"

#include "net/rime/neighbor-discovery.h"
#include "net/rime/unicast.h"
#include "tsch-schedule.h"
#include "tsch-slot-operation.h"
#include "tsch.h"
#include <stdint.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define LOG_LLU_MARK "0x%08x%08x"
#define LOG_LLU(bigint) (uint32_t)(((bigint) >> 32)), (uint32_t)((bigint) & 0xFFFFFFFF)

#include "lib/random.h"
#include "lib/list.h"

#include "tsch-prop.h"

PROCESS(rand_sched_process, "RANDSCHED");

/* #define RAND_SCHED_MAX_NEIGHBORS 40 */

#define MAX_LAST_SEEN_INTERVAL CLOCK_SECOND*3

static struct etimer rand_schedule_phase_timer;
/* static struct etimer decision_evaluation_timer; */
static struct etimer initial_wait_timer;
static struct ctimer start_neighbor_discovery_timer;

static struct neighbor_discovery_conn neighbor_discovery;

#define MTM_START_SLOT 2

// create memb block for occupancy map

enum mtm_node_role {
    MTM_PASSIVE,
    MTM_ACTIVE,
    MTM_ANCHOR // an anchor is a active participant that never loses its slot 
};

static struct rand_schedule_node_state {
    uint8_t node_is_fixed;        
    clock_time_t picked_timeslot_at;
    uint8_t max_slots;
} node_state = {
    .node_is_fixed = 0,
    .picked_timeslot_at = 0,
    .max_slots = 0
};


struct drand_neighbor_state {
    struct drand_neighbor_state *next;
    
    /* linkaddr_t neighbor_addr; */
    ranging_addr_t addr;
    uint8_t has_timeslot, chosen_timeslot;

    uint64_t prev_total_found_ours;

    clock_time_t last_seen;
};

LIST(direct_neighbor_list);
MEMB(direct_neighbor_memb, struct drand_neighbor_state, 10);

static void remove_transmit_link(uint8_t timeslot);
static void sched_neighbor_discovery_stop();
static uint8_t check_have_timeslot();
static void save_alive_neighbors_timestamp_counters();
    
static uint8_t neighbor_discovery_running = 0;


// the higher the time we own the timeslot already is, the lower the probability should be
// that we throw our pick away. The highest probability is hereby 3/4 to keep the timeslot
// at a period of 10 seconds
static uint8_t backoff_lottery(clock_time_t time_since_pick) {
    
    uint8_t backoff = 0;
    if(time_since_pick > CLOCK_SECOND*10) {
        backoff = 1;
    } else if(time_since_pick > CLOCK_SECOND*5) {
        backoff = 2;
    } else if(time_since_pick > CLOCK_SECOND*2) {
        backoff = 3;
    } else if(time_since_pick > CLOCK_SECOND) {
        backoff = 4;
    } else {
        backoff = 5;
    }

    return random_rand() % backoff;
}
    
static void discovery_recv(struct neighbor_discovery_conn *c,
    const linkaddr_t *from, uint16_t val) {
    clock_time_t now = clock_time();
    uint8_t our_timeslot = check_have_timeslot();

    printf("recv, %u\n", from->u8[LINKADDR_SIZE-1]);

    if(!our_timeslot || node_state.node_is_fixed)
        return;

    // Alternative Idea: When we get a neighbor message, and we are an active node, and we did not
    // observe the neighbor through mtm, than we directly know something is wrong and we backoff
    // from our decision
    struct mtm_neighbor *n = NULL;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        if (n->neighbor_addr == from->u8[LINKADDR_SIZE-1]) {
            break;
        }
    }

    if (n == NULL || ((now - n->last_observed_direct) > MAX_LAST_SEEN_INTERVAL)) {
        /* || val == our_timeslot */
        printf("boff, %u\n", from->u8[LINKADDR_SIZE-1]);
        remove_transmit_link(our_timeslot);
        sched_neighbor_discovery_stop();
        neighbor_discovery_set_val(&neighbor_discovery, 0);
        /* tsch_set_send_beacons(0); */
    }
}

void rand_set_timeslot_fixed(uint8_t enable) {
    node_state.node_is_fixed = enable;
}


static void discovery_sent(struct neighbor_discovery_conn *c) {
    printf("snt\n");
}

static const struct neighbor_discovery_callbacks neighbor_discovery_calls = { .recv = discovery_recv, .sent = discovery_sent };


static void sched_neighbor_discovery_start() {
  uint8_t our_timeslot = check_have_timeslot();
  neighbor_discovery_open(&neighbor_discovery,
      42,
      CLOCK_SECOND*2,
      CLOCK_SECOND,
      CLOCK_SECOND*4,
      &neighbor_discovery_calls);
  neighbor_discovery_start(&neighbor_discovery, our_timeslot);
  neighbor_discovery_running = 1;
  ctimer_stop(&start_neighbor_discovery_timer);
}

static void sched_neighbor_discovery_stop() {
    neighbor_discovery_close(&neighbor_discovery);
    neighbor_discovery_running = 0;
}

// round starts at timeslot 2
#define MTM_ROUND_START 2

void rand_sched_init(uint8_t max_mtm_slots) {

    node_state.max_slots = max_mtm_slots;
    
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, node_state.max_slots + MTM_ROUND_START + 1);
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        0,
        0);
    
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING_ONLY,
        &tsch_broadcast_address,
        1,
        0);

    // initialize all other links as RX PROP MTM
    for(int i = MTM_START_SLOT; i < MTM_START_SLOT + node_state.max_slots; i++) {
        tsch_schedule_add_link(
            sf_eb,
            LINK_OPTION_RX,
            LINK_TYPE_PROP_MTM,
            &tsch_broadcast_address,
            i,
            0);
    }

    mtm_set_round_end(MTM_START_SLOT + node_state.max_slots - 1);

    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        MTM_ROUND_START + node_state.max_slots,
        0);

    
    process_start(&rand_sched_process, NULL);
}

/*  static uint8_t rule_enough_neighbors(uint8_t min_neighbors) { */
/*     // if we don't have enough neighbors (as measured by our mtm table) we don't extend */

/*     struct mtm_neighbor *n = NULL; */
/*     uint8_t num_neighbors = 0; */
    
/*     for (n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) { */
/*         if((clock_time() - n->last_observed) > CLOCK_SECOND*2) { */
/*             continue; */
/*         } */
/*         num_neighbors++; */
/*     } */
    
/*     return num_neighbors >= min_neighbors; */
/* } */

/* static uint8_t rule_maximum_direct_neighbors(uint8_t max_capacity) { */
/*     struct mtm_neighbor *n = NULL; */
/*     uint8_t num_neighbors = 0; */
    
/*     for (n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) { */
/*         if((clock_time() - n->last_observed) > CLOCK_SECOND*2) { */
/*             continue; */
/*         } */
/*         num_neighbors++; */
/*     } */
    
/*     return num_neighbors <= max_capacity; */
/* } */

/* static uint8_T rule_neighbor_capacity(uint8_t max_capacity) { */
/*     // if we have too many neighbors (as measured by our mtm table) we don't extend */
/* } */

// returns a free timeslot, or 0 if no free timeslot exists.  Note that timeslot 0 is always our
// shared timeslot, so it may never be associated for mtm usage
static uint8_t choose_free_timeslot() {
    clock_time_t now = clock_time();
    // pick randomly free slot from occupied_slots
    uint8_t occupied_slots[node_state.max_slots+MTM_ROUND_START];

    memset(occupied_slots, 0, sizeof(occupied_slots));

    // mark all timeslots below MTM_ROUND_START as occupied
    for (uint8_t i = 0; i < MTM_ROUND_START; i++) {
        occupied_slots[i] = 1;
    }
    
    /* // iterate over tsch_prop_get_neighbor_list(); */
    struct mtm_neighbor *n = NULL;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        if((now - n->last_observed_direct) > CLOCK_SECOND*5 && (now - n->last_observed_indirect) > CLOCK_SECOND*5) {
            continue;
        }
        occupied_slots[n->observed_timeslot] = 1;
    }

    // print occupancy map
    printf("occupancy map: ");
    for (uint8_t i = 0; i < node_state.max_slots+MTM_ROUND_START; i++) {
        printf("%d", occupied_slots[i]);
    }
    printf("\n");

    uint8_t free_slot_count = 0;
    for (uint8_t i = 0; i < node_state.max_slots+MTM_ROUND_START; i++) {
        if (occupied_slots[i] == 0) {
            free_slot_count++;
        }
    }

    if(free_slot_count == 0) {
        return 0;
    }

    // pick random number
    uint8_t random_number = random_rand() % free_slot_count;

    // return slot number
    uint8_t free_slot_number = 0;
    for (uint8_t i = MTM_ROUND_START; i < node_state.max_slots+MTM_ROUND_START; i++) {
        if (occupied_slots[i] == 0) {
            if (free_slot_number == random_number) {
                return i;
            }
            free_slot_number++;
        }
    }

    return 0;
}

// for now the lottery is a coin toss
static uint8_t play_lottery() {
    return !(random_rand() % 20);
}

static uint8_t check_have_timeslot() {
    // go over slot frame
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);

    if(sf_eb == NULL)
        return 0;
    
    struct tsch_link *l = NULL;
    for(l = list_head(sf_eb->links_list); l != NULL; l = list_item_next(l)) {
        if (l->link_type == LINK_TYPE_PROP_MTM && (l->link_options & LINK_OPTION_TX)) {
            return l->timeslot;
        }
    }
    
    return 0;
}

static void handle_won_lottery() {
    // uniformly pick timeslot
    uint8_t timeslot = choose_free_timeslot();
    if(timeslot) {
        printf("Picked timeslot %d\n", timeslot);
        rand_sched_set_timeslot(timeslot);
    }
}

static struct drand_neighbor_state* get_or_create_neighbor_entry(ranging_addr_t addr) {
    struct drand_neighbor_state *r = NULL;
    for(r = list_head(direct_neighbor_list); r != NULL; r = list_item_next(r)) {
        if(r->addr == addr) {
            break;
        }
    }
    
    if (r == NULL) {
        // create entry
        r = memb_alloc(&direct_neighbor_memb);
        if(r == NULL) {
            printf("Could not allocate neighbor entry\n");
        } else {
            // add to list
            list_add(direct_neighbor_list, r);
            r->addr = addr;
        }
    }

    return r;
}

    
uint8_t mtm_neighbor_is_alive(struct mtm_neighbor *n) {
    clock_time_t now = clock_time();
    
    if((now - n->last_observed_direct) < CLOCK_SECOND) {
        return 1;
    }
    
    return 0;
}

static void clear_neighbor_list() {
    // remove all entries from the neighbor direct list
    struct drand_neighbor_state *r = NULL;
    while((r = list_pop(direct_neighbor_list)) != NULL) {
        memb_free(&direct_neighbor_memb, r);
    }
}

static void save_alive_neighbors_timestamp_counters() {
    struct mtm_neighbor *n = NULL;

    clear_neighbor_list();
    
    for (n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        if (mtm_neighbor_is_alive(n)) {
            struct drand_neighbor_state *r = get_or_create_neighbor_entry(n->neighbor_addr);
            if (r != NULL) {
                r->prev_total_found_ours = n->total_found_ours_counter;
            }
        }
    }
}

uint8_t check_planarity(clock_time_t now) {
}

uint8_t check_eligibility(clock_time_t now) {
    // first check whether we have one active neighbor at least
    struct mtm_neighbor *n = NULL;
    uint8_t num_active_neighbors = 0;
    int8_t edge_count = 0, node_count = 0;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        clock_time_t last_seen = now - n->last_observed_direct;
        if((last_seen) < MAX_LAST_SEEN_INTERVAL) {
            num_active_neighbors++;
            node_count++;
        }
    }

    struct mtm_pas_tdoa *pas = NULL;
    for(pas = list_head(tsch_prop_get_tdoa_list()); pas != NULL; pas = list_item_next(pas)) {
        clock_time_t last_seen = now - pas->last_observed;
        if(last_seen < MAX_LAST_SEEN_INTERVAL) {
            edge_count++;
        }
    }

    // use eulers formula to check for planarity
    return num_active_neighbors > 2 && !(edge_count > (3 * node_count - 6));
}

static void remove_transmit_link(uint8_t timeslot) {
    printf("r\n");
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
    if(sf_eb != NULL) {
        struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot);
        l->link_options = LINK_OPTION_RX;
    }
}

static void add_transmit_link(uint8_t timeslot) {
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
    if(sf_eb != NULL) {
        printf("Setting link to tx\n");
        struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot);
        if(l == NULL) {
            printf("Could not find link\n");
        }
        l->link_options = LINK_OPTION_TX;
    }
}

/* static void backoff_from_decision_probabilistic(uint8_t timeslot) { */
/* } */


// check whether timeslot was a good decision
static uint8_t good_decision(uint8_t timeslot) {
    printf("check dec\n");
    
    // first check for collisions
    /* uint8_t collisions = 0; */
    // look through neighbor table if we find any collisions
    /* struct mtm_neighbor *n = NULL; */
    /* for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) { */
    /*     printf("neighbor %u, timeslot %u\n", n->neighbor_addr, n->observed_timeslot); */
    /*     // outdated neighbor entries should not influence our decision */
    /*     if(n->last_observed_direct > node_state.picked_timeslot_at || n->last_observed_indirect > node_state.picked_timeslot_at) { */
    /*         if(n->observed_timeslot == timeslot) { */
    /*             collisions++; */
    /*             printf("Collision with node %u in timeslot %u\n", n->neighbor_addr, timeslot); */
    /*         } */
    /*     } */
    /* } */

    /* if(collisions > 10) { */
    /*     printf("We had %u collisions\n", collisions); */
    /*     return 0; */
    /* } */

/*     // search matching entry in our rand-sched neighbor discovery service */
    uint8_t found_in_all_active_neighbors = 0;
    struct drand_neighbor_state *r = NULL;
    for(r = list_head(direct_neighbor_list); r != NULL; r = list_item_next(r)) {
        struct mtm_neighbor *n = NULL;
        for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
            if(n->neighbor_addr == r->addr) {
                printf("found tsch prop entry \n");
                break;
            }
        }

        if (n != NULL) {
            if (n->total_found_ours_counter - r->prev_total_found_ours < 1) {
                found_in_all_active_neighbors = 0;
            }
        }
    }

    if (found_in_all_active_neighbors) {
        return 1;
    }

    return 0;
}


static void eval_print_rand_sched_status() {
    struct mtm_neighbor *mtm_neighbor = NULL;
    struct drand_neighbor_state *alive_neighbor = NULL;
    printf("tpnl, ");
    
    for(mtm_neighbor = list_head(tsch_prop_get_neighbor_list()); mtm_neighbor != NULL; mtm_neighbor = mtm_neighbor->next) {
        printf("%u, ", mtm_neighbor->neighbor_addr);
    } printf("\n");

    for(alive_neighbor = list_head(direct_neighbor_list); alive_neighbor != NULL; alive_neighbor = alive_neighbor->next) {
        printf("%u, ", alive_neighbor->addr);
    } printf("\n");

    printf("ndr, %u\n", neighbor_discovery_running);

    uint8_t timeslot = check_have_timeslot();

    printf("ts, %u\n", timeslot);
}



void rand_sched_set_timeslot(uint8_t timeslot) {
    // override existing timeslot
    uint8_t our_timeslot = check_have_timeslot();
    if(our_timeslot) {
        remove_transmit_link(our_timeslot);
        sched_neighbor_discovery_stop();
    }
    
    add_transmit_link(timeslot);
    
    neighbor_discovery_set_val(&neighbor_discovery, timeslot);
    tsch_set_send_beacons(1);
    node_state.picked_timeslot_at = clock_time();
    /* save_alive_neighbors_timestamp_counters(); */
    ctimer_set(&start_neighbor_discovery_timer, CLOCK_SECOND/2, &sched_neighbor_discovery_start, NULL);    
}

PROCESS_THREAD(rand_sched_process, ev, data)
{
  PROCESS_BEGIN();

  printf("starting rand_sched_process\n");
  
  /* etimer_set(&neighbor_discovery_phase_timer, CLOCK_SECOND * 40); */

  /* etimer_set(&initial_wait_timer, CLOCK_SECOND * 10); // wait 10 seconds before starting */
  /* etimer_set(&decision_evaluation_timer, CLOCK_SECOND/2); // every 250ms make a scheduling decision   */

  /* PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&initial_wait_timer)); */
  
  etimer_set(&rand_schedule_phase_timer, CLOCK_SECOND);
  
  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rand_schedule_phase_timer)
          /* || etimer_expired(&decision_evaluation_timer) */
          );

      if(etimer_expired(&rand_schedule_phase_timer)) {
          // play the lottery
          if(tsch_is_associated) {
              clock_time_t now = clock_time();              
              uint8_t our_timeslot = check_have_timeslot();
              eval_print_rand_sched_status();
          
              if(!our_timeslot && play_lottery() && check_eligibility(now)) {
                  printf("won\n");
                  handle_won_lottery();
              }
              
              /* else { */
              /*     if(!good_decision(our_timeslot)) { */
              /*         printf("bad\n"); */
              /*         remove_transmit_link(our_timeslot); */
                      
              /*         /\* sched_neighbor_discovery_stop(); *\/ */
              /*         /\* neighbor_discovery_set_val(&neighbor_discovery, 0); *\/ */
              /*     } */
              /*     save_alive_neighbors_timestamp_counters(); */
              /* } */
          }
          
          etimer_reset(&rand_schedule_phase_timer);                          
      }

      /* uint8_t our_timeslot = check_have_timeslot(); */
  }

  PROCESS_END();
}
