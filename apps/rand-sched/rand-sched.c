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

/* #define DEBUG DEBUG_PRINT */
/* #include "net/ip/uip-debug.h" */

#define DEBUG_RAND 0
#if DEBUG_define
#RAND PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define LOG_LLU_MARK "0x%08x%08x"
#define LOG_LLU(bigint) (uint32_t)(((bigint) >> 32)), (uint32_t)((bigint) & 0xFFFFFFFF)

#include "lib/random.h"
#include "lib/list.h"

#include "tsch-prop.h"

PROCESS(rand_sched_process, "RANDSCHED");
PROCESS(TSCH_MTM_SLOT_END_PROCESS, "TSCH MTM round end Process");

#define MAX_LAST_SEEN_INTERVAL CLOCK_SECOND

static struct etimer rand_schedule_phase_timer;
/* static struct etimer decision_evaluation_timer; */
static struct etimer initial_wait_timer;
static struct ctimer start_neighbor_discovery_timer;

#define RAND_SCHED_WITH_BACKOFF_TIMER 1
#define RAND_SCHED_MIN_JITTER (CLOCK_SECOND*5)
#define RAND_SCHED_INITIAL_BACKOFF_TIME CLOCK_SECOND
#define RAND_SCHED_MAX_BACKOFF_TIME     (8 * CLOCK_SECOND)
#define RAND_SCHED_WITH_RANK 0
#define WITH_NEIGHBOR_TABLE_OCCUPANCY 0

#if RAND_SCHED_WITH_BACKOFF_TIMER
static struct ctimer backoff_timer;
static struct ctimer backoff_reset_timer;
#endif

static struct neighbor_discovery_conn neighbor_discovery;

// create memb block for occupancy map

static struct rand_schedule_node_state {
    uint8_t node_is_fixed, node_is_mobile;
    clock_time_t picked_timeslot_at;
    uint8_t max_slots;
    uint16_t join_prob;
    uint8_t neighbor_discovery_running;

    uint8_t rank; // only used when RAND_SCHED_WITH_RANK is enabled

    uint8_t backoff_active;
    clock_time_t current_backoff;
} node_state;

static void reset_rand_schedule_state () {
    node_state.neighbor_discovery_running = 0;
    node_state.node_is_fixed = 0;
    node_state.node_is_mobile = 0;
    node_state.picked_timeslot_at = 0;
    node_state.max_slots = 0;
    node_state.join_prob = 4;
    node_state.backoff_active = 0;
    node_state.rank = 0;
    node_state.current_backoff = RAND_SCHED_INITIAL_BACKOFF_TIME;
}

#if RAND_SCHED_WITH_BACKOFF_TIMER
void backoff_callback(void *ptr) {
    node_state.backoff_active = 0;
    ctimer_stop(&backoff_timer);
}

void reset_backoff() {
    node_state.backoff_active = 0;
    node_state.current_backoff = RAND_SCHED_INITIAL_BACKOFF_TIME;
}
#endif



struct rand_neighbor_state {
    struct rand_neighbor_state *next;
    
    linkaddr_t addr;
    uint8_t rank, timeslot;

    clock_time_t last_seen;
};

LIST(direct_neighbor_list);
MEMB(direct_neighbor_memb, struct rand_neighbor_state, 40);

static void remove_transmit_link(uint8_t timeslot);
static void sched_neighbor_discovery_stop();
static uint8_t check_have_timeslot();
static void save_alive_neighbors_timestamp_counters();
static void eval_print_rand_sched_status();
static void sched_neighbor_discovery_set_listen_only();

 static void observed_neighbor(const linkaddr_t *addr, uint8_t rank, uint8_t timeslot) {
    struct rand_neighbor_state *n = NULL;
    for(n = list_head(direct_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (linkaddr_cmp(&n->addr, addr)) {
            break;
        }
    }

    if(n == NULL) {
        n = memb_alloc(&direct_neighbor_memb);
        if(n == NULL) {
            PRINTF("direct_neighbor_memb full\n");
            return;
        }
        linkaddr_copy(&n->addr, addr);
        list_add(direct_neighbor_list, n);
    }
    
    n->timeslot = timeslot;
    n->rank = rank;
    n->last_seen = clock_time();
}


static uint8_t get_node_rank(const ranging_addr_t addr) {
    struct rand_neighbor_state *n = NULL;
    for(n = list_head(direct_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (n->addr.u8[LINKADDR_SIZE-1] == addr) {
            return n->rank;
        }
    }
    
    return 0;
}

void rand_sched_set_mobile(uint8_t enable) {
    node_state.node_is_mobile = enable;
}


void maybe_reroll_timeslot() {
    uint8_t our_timeslot = check_have_timeslot();
    clock_time_t now = clock_time();

    if(node_state.node_is_fixed)
        return;

    /* // iterate over tsch_prop_get_neighbor_list(); */
    struct mtm_neighbor *n = NULL;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        if((now - n->last_observed_direct) < MAX_LAST_SEEN_INTERVAL || (now - n->last_observed_indirect) < MAX_LAST_SEEN_INTERVAL) {
            if(n->observed_timeslot == our_timeslot) {
                PRINTF("rerolling timeslot\n");
                remove_transmit_link(our_timeslot);
                break;
            }
        }
    }
}

static void discovery_recv(struct neighbor_discovery_conn *c,
    const linkaddr_t *from, uint16_t val) {
    clock_time_t now = clock_time();
    uint8_t our_timeslot = check_have_timeslot();

    uint8_t other_rank = val & 0xFF;
    uint8_t other_timeslot = (val >> 8) & 0xFF;

    PRINTF("recv, %u\n", from->u8[LINKADDR_SIZE-1]);

    observed_neighbor(from, other_rank, other_timeslot);

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

    #define RAND_SCHED_NODE_ADDR_TIE_BREAKER 0
    #if RAND_SCHED_NODE_ADDR_TIE_BREAKER
    if(linkaddr_node_addr.u8[LINKADDR_SIZE-1] > from->u8[LINKADDR_SIZE-1]) {
        return;
    }
    #endif

    #if RAND_SCHED_WITH_RANK
    // we only expect to see nodes that outrank us, otherwise we don't care
    if(node_state.rank > other_rank) {
        return;
    }
    #endif

    if (n == NULL || ( ((now - n->last_observed_direct) > MAX_LAST_SEEN_INTERVAL) ) // && ((now - n->last_observed_indirect) > MAX_LAST_SEEN_INTERVAL))
        #if RAND_SCHED_WITH_RANK
        || (other_rank >= node_state.rank && our_timeslot == other_timeslot)
        #endif
        ) {
        
        sched_neighbor_discovery_set_listen_only();
        
        remove_transmit_link(our_timeslot);

        /* tsch_set_send_beacons(0); */

        node_state.backoff_active = 1;
        if(node_state.current_backoff < RAND_SCHED_MAX_BACKOFF_TIME) {
            node_state.current_backoff *= 2;
        }

        clock_time_t max_rand_jitter = node_state.current_backoff;
        if (node_state.current_backoff <= RAND_SCHED_MIN_JITTER) {
            max_rand_jitter = RAND_SCHED_MIN_JITTER;
        }

        clock_time_t random_offset = (random_rand() % (uint16_t) max_rand_jitter);
        ctimer_set(&backoff_timer, node_state.current_backoff + random_offset, backoff_callback, NULL);
        ctimer_stop(&backoff_reset_timer);
    }
}

void rand_set_timeslot_fixed(uint8_t enable) {
    node_state.node_is_fixed = enable;
}


static void discovery_sent(struct neighbor_discovery_conn *c) {
    if(!node_state.neighbor_discovery_running)
        printf("ERRRR\n");
}

static const struct neighbor_discovery_callbacks neighbor_discovery_calls = { .recv = discovery_recv, .sent = discovery_sent };

static void sched_neighbor_discovery_open() {
    if(node_state.neighbor_discovery_running) {
        return;
    }
    
    neighbor_discovery_open(&neighbor_discovery,
        42,
        CLOCK_SECOND/2,
        CLOCK_SECOND/2,
        CLOCK_SECOND*4,
        &neighbor_discovery_calls);

    node_state.neighbor_discovery_running = 1;
}

static void sched_neighbor_discovery_start() {
    uint8_t timeslot = check_have_timeslot();
    uint16_t value = node_state.rank | (timeslot << 8);
    
    sched_neighbor_discovery_open();

    neighbor_discovery_start(&neighbor_discovery, value);

    ctimer_stop(&start_neighbor_discovery_timer);
}

static void sched_neighbor_discovery_stop() {
    if(!node_state.neighbor_discovery_running) {
        return;
    }
    
    neighbor_discovery_close(&neighbor_discovery);
    node_state.neighbor_discovery_running = 0;
}


static void sched_neighbor_discovery_set_listen_only() {
    /* sched_neighbor_discovery_stop(); */
    /* sched_neighbor_discovery_open(); */

    sched_neighbor_discovery_stop();    
}

// round starts at timeslot 2
#define MTM_ROUND_START 2

void rand_sched_set_rank(uint8_t rank) {
    node_state.rank = rank;

    // if neighbor discovery is running update value
    if(node_state.neighbor_discovery_running) {
        uint8_t timeslot = check_have_timeslot();
        uint16_t value = node_state.rank | (timeslot << 8);
        neighbor_discovery_set_val(&neighbor_discovery, value);
    }
}

void rand_sched_init(uint8_t max_mtm_slots) {
    reset_rand_schedule_state();
    
    reset_backoff();

    sched_neighbor_discovery_set_listen_only();

    // clear all existing slotframes
    if (!tsch_schedule_remove_all_slotframes()) {
        return;
    }
    
    mtm_reset();

    node_state.max_slots = max_mtm_slots;
    
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, node_state.max_slots + MTM_ROUND_START + 2);
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING_ONLY,
        &tsch_broadcast_address,
        0,
        0);
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        1,
        0);    

    // initialize all other links as RX PROP MTM
    for(int i = MTM_ROUND_START; i < MTM_ROUND_START + node_state.max_slots; i++) {
        tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, i, 0);
    }

    mtm_set_round_slots(MTM_ROUND_START, MTM_ROUND_START + node_state.max_slots - 1);

    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED,
        LINK_TYPE_NORMAL,
        &tsch_broadcast_address,
        MTM_ROUND_START + node_state.max_slots,
        0);

        tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING_ONLY,
        &tsch_broadcast_address,
        MTM_ROUND_START + node_state.max_slots + 1,
        0);

}

void rand_sched_start() {
#if WITH_MTM_SLOT_END_PROCESS
    process_start(&TSCH_MTM_SLOT_END_PROCESS, NULL);
#endif
   process_start(&rand_sched_process, NULL);

  // we start with an initial random delay
  node_state.backoff_active = 1;
  clock_time_t max_rand_jitter = node_state.current_backoff;
  if (node_state.current_backoff <= RAND_SCHED_MIN_JITTER) {
      max_rand_jitter = RAND_SCHED_MIN_JITTER;
  }

  clock_time_t random_offset = (random_rand() % (uint16_t) max_rand_jitter);
  ctimer_set(&backoff_timer, node_state.current_backoff + random_offset, backoff_callback, NULL);
}

void rand_sched_stop() {
    // stop neighbor discovery
    sched_neighbor_discovery_stop();

    // also ensure timer is not runnning
    ctimer_stop(&start_neighbor_discovery_timer);
    ctimer_stop(&backoff_timer);
    ctimer_stop(&backoff_reset_timer);    
    etimer_stop(&rand_schedule_phase_timer);
    
    process_exit(&rand_sched_process);

#if WITH_MTM_SLOT_END_PROCESS
    process_exit(&TSCH_MTM_SLOT_END_PROCESS);
#endif
}

// returns a free timeslot, or 0 if no free timeslot exists.  Note that timeslot 0 is always our
// shared timeslot, so it may never be associated for mtm usage
static uint8_t choose_free_timeslot() {
    clock_time_t now = clock_time();
    // pick randomly free slot from occupied_slots
    uint8_t occupied_slots[node_state.max_slots+MTM_ROUND_START];

    /* memset(occupied_slots, 0, node_state.max_slots+MTM_ROUND_START); */
    for (uint8_t i = 0; i < node_state.max_slots+MTM_ROUND_START; i++) {
        occupied_slots[i] = 0;
    }

    // mark all timeslots below MTM_ROUND_START as occupied
    for (uint8_t i = 0; i < MTM_ROUND_START; i++) {
        occupied_slots[i] = 1;
    }

    /* // iterate over tsch_prop_get_neighbor_list(); */
    struct mtm_neighbor *n = NULL;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        if((now - n->last_observed_direct) < MAX_LAST_SEEN_INTERVAL || (now - n->last_observed_indirect) < MAX_LAST_SEEN_INTERVAL) {
#if RAND_SCHED_WITH_RANK
            if(node_state.rank > get_node_rank(n->neighbor_addr)) {
                continue;
            }
#endif
            occupied_slots[n->observed_timeslot] = 1;            
        }
    }

#if RAND_SCHED_WITH_RANK
#if WITH_NEIGHBOR_TABLE_OCCUPANCY
    struct rand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(direct_neighbor_list); neighbor != NULL; neighbor = list_item_next(neighbor)) {
        if (neighbor->rank >= node_state.rank && (now - neighbor->last_seen) < CLOCK_SECOND*4) {
            occupied_slots[neighbor->timeslot] = 1;
        }
    }
#endif
#endif
    
    // print occupancy map
    PRINTF("occupancy map: ");
    for (uint8_t i = 0; i < node_state.max_slots+MTM_ROUND_START; i++) {
        PRINTF("%d", occupied_slots[i]);
    }
    PRINTF("\n");
    
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
    /* return !(random_rand() % node_state.join_prob); */
    return !(random_rand() % 2);
}

static uint8_t check_have_timeslot() {
    // go over slot frame
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);

    if(sf_eb == NULL) {
        return 0;        
    }

    
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
        PRINTF("Picked timeslot %d\n", timeslot);
        rand_sched_set_timeslot(timeslot);
    }
}

static void clear_neighbor_list() {
    // remove all entries from the neighbor direct list
    struct rand_neighbor_state *r = NULL;
    while((r = list_pop(direct_neighbor_list)) != NULL) {
        memb_free(&direct_neighbor_memb, r);
    }
}

uint8_t check_eligibility(clock_time_t now) {
    // first check whether we have one active neighbor at least
    struct mtm_neighbor *n = NULL;
    uint8_t eligible = 1;
    uint8_t num_active_neighbors = 0;
    
#if RAND_SCHED_RULES_WITH_PLANARITY_CHECK
    int16_t edge_count = 0, node_count = 1;
#endif
    
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        clock_time_t last_seen = now - n->last_observed_direct;
        if((last_seen) < MAX_LAST_SEEN_INTERVAL) {
            num_active_neighbors++;
            
#if RAND_SCHED_RULES_WITH_PLANARITY_CHECK            
            node_count++;
            edge_count++; // TODO  compare with other version without
#endif
            
        }
    }

#if RAND_SCHED_RULES_WITH_PLANARITY_CHECK
    struct mtm_pas_tdoa *pas = NULL;
    for(pas = list_head(tsch_prop_get_tdoa_list()); pas != NULL; pas = list_item_next(pas)) {
        clock_time_t last_seen = now - pas->last_observed;
        if(last_seen < MAX_LAST_SEEN_INTERVAL) {
            edge_count++;
        }
    }

    if( node_count >= 3 && edge_count > (3 * node_count - 6)) {
    /* if( node_count >= 3 && edge_count > (3 * node_count)) {         */
        eligible = 0;
    }

    /* if( edge_count > (3 * node_count)) { */
    /*     eligible = 0; */
    /* } */
#endif
    
    if(num_active_neighbors < RAND_SCHED_RULES_MIN_NEIGHBORS) {
        eligible = 0;
    }

    return eligible;
}

static void remove_transmit_link(uint8_t timeslot) {
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
    /* if(sf_eb != NULL) { */
    /*     printf("P: Setting link to rx\n"); */
    /*     struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot); */
    /*     l->link_options = LINK_OPTION_RX; */
    /* } */
    uint8_t success = tsch_schedule_remove_link_by_timeslot(sf_eb, timeslot);
    tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, timeslot, 0);
}

static void add_transmit_link(uint8_t timeslot) {
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
    uint8_t success = tsch_schedule_remove_link_by_timeslot(sf_eb, timeslot);
    tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, timeslot, 0);
}

static void eval_print_rand_sched_status() {
    uint8_t timeslot = check_have_timeslot();

    printf("ts, %u\n", timeslot);
}

void rand_sched_set_join_prob(uint16_t denominator) {
    node_state.join_prob = denominator;
}

void rand_sched_set_timeslot(uint8_t timeslot) {
    // override existing timeslot
    uint8_t our_timeslot = check_have_timeslot();
    if(our_timeslot) {
        remove_transmit_link(our_timeslot);
        sched_neighbor_discovery_set_listen_only();
    }
    
    mtm_reset_rx_queue();
    
    add_transmit_link(timeslot);
    
    node_state.picked_timeslot_at = clock_time();
    /* save_alive_neighbors_timestamp_counters(); */

    ctimer_set(&start_neighbor_discovery_timer, CLOCK_SECOND/3, &sched_neighbor_discovery_start, NULL);
    /* sched_neighbor_discovery_start(); */
}

#if WITH_MTM_SLOT_END_PROCESS
PROCESS_THREAD(TSCH_MTM_SLOT_END_PROCESS, ev, data)
{
  PROCESS_BEGIN();

  while(1) {
      PROCESS_YIELD();
      /* receive a new propagation time measurement */
      static enum MTM_SLOT_END_TYPE *slot_type = NULL;
      if(ev == PROCESS_EVENT_MSG) {
          slot_type = (enum MTM_SLOT_END_TYPE *) data;

          if (*slot_type == MTM_ROUND_END) {
              // play the lottery
              if(tsch_is_associated) {
                  clock_time_t now = clock_time();              
                  uint8_t our_timeslot = check_have_timeslot();


                  if(!our_timeslot
                      && !node_state.node_is_mobile
                      && !node_state.backoff_active
#if RAND_SCHED_WITH_LOTTERY
                      && play_lottery()
#endif
                      && check_eligibility(now)
                      ) {
                      PRINTF("won\n");
                      handle_won_lottery();

                      // set backoff_reset_timer to 5 seconds
                      ctimer_set(&backoff_reset_timer, CLOCK_SECOND * 4, &reset_backoff, NULL);
                  } else if(!node_state.backoff_active) {
                      node_state.backoff_active = 1;

                      clock_time_t max_rand_jitter = node_state.current_backoff;
                      
                      if (node_state.current_backoff <= RAND_SCHED_MIN_JITTER) {
                          max_rand_jitter = RAND_SCHED_MIN_JITTER;
                      }
                      
                      clock_time_t random_offset = (random_rand() % (uint16_t) max_rand_jitter);
                      ctimer_set(&backoff_timer, node_state.current_backoff + random_offset, backoff_callback, NULL);
                  }
                  eval_print_rand_sched_status();
              }
          }
      }
  }

  PROCESS_END();
}
#endif

#define RAND_SCHED_WITH_LOTTERY 0
PROCESS_THREAD(rand_sched_process, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("starting rand_sched_process\n");

  etimer_set(&rand_schedule_phase_timer, CLOCK_SECOND/2);

  

  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rand_schedule_phase_timer)
          /* || etimer_expired(&decision_evaluation_timer) */
          );

#if !WITH_MTM_SLOT_END_PROCESS
      if(etimer_expired(&rand_schedule_phase_timer)) {
          // play the lottery
          if(tsch_is_associated) {
              clock_time_t now = clock_time();              
              uint8_t our_timeslot = check_have_timeslot();

              if(!our_timeslot
                  && !node_state.node_is_mobile
                  && !node_state.backoff_active
                  #if RAND_SCHED_WITH_LOTTERY
                  && play_lottery()
                  #endif
                  && check_eligibility(now)
                  ) {
                  PRINTF("won\n");
                  handle_won_lottery();

                  // set backoff_reset_timer to 5 seconds
                  ctimer_set(&backoff_reset_timer, CLOCK_SECOND * 4, &reset_backoff, NULL);
              }
              /* else if (our_timeslot) { */
              /*     maybe_reroll_timeslot(); */
              /* } */
          }
          
          etimer_reset(&rand_schedule_phase_timer);                          
      }
#endif

      /* uint8_t our_timeslot = check_have_timeslot(); */
  }

  PROCESS_END();
}
