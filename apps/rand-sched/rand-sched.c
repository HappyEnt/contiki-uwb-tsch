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
 *         DRAND: drand protocol for automatic conflict free TDMA scheduling build on top of RIME stack
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

/* #define DEBUG_DRAND_UART 1 */
/* #if DEBUG_DRAND_UART */
/* #include "dbg-io/dbg.h" */
/* #undef PRINTF */
/* #define PRINTF(...) do { char buf[128]; sprintf(buf, __VA_ARGS__); dbg_send_bytes(buf, strlen(buf)); } while(0) */
/* #else */
#define PRINTF(...) printf(__VA_ARGS__)
/* #endif */

#define LOG_LLU_MARK "0x%08x%08x"
#define LOG_LLU(bigint) (uint32_t)(((bigint) >> 32)), (uint32_t)((bigint) & 0xFFFFFFFF)

#include "lib/random.h"
#include "lib/list.h"

#include "tsch-prop.h"

PROCESS(rand_sched_process, "RANDSCHED");

#define RAND_SCHED_MAX_NEIGHBORS 40

#define MAX_LAST_SEEN_INTERVAL 2*CLOCK_SECOND

static struct etimer rand_schedule_phase_timer;
/* static struct etimer decision_evaluation_timer; */
static struct etimer initial_wait_timer;

static struct neighbor_discovery_conn neighbor_discovery;

#define MTM_START_SLOT 2

// create memb block for occupancy map

enum mtm_node_role {
    MTM_PASSIVE,
    MTM_ACTIVE,
    MTM_ANCHOR // an anchor is a active participant that never loses its slot 
};

static struct rand_schedule_node_state {
    uint8_t mtm_round_first;

    uint8_t max_slots;
} node_state = {
    0x00
};


struct drand_neighbor_state {
    struct drand_neighbor_state *next;
    
    linkaddr_t neighbor_addr;
    uint8_t has_timeslot, chosen_timeslot;

    uint64_t prev_total_found_ours;

    clock_time_t last_seen;
};

LIST(direct_neighbor_list);
MEMB(direct_neighbor_memb, struct drand_neighbor_state, RAND_SCHED_MAX_NEIGHBORS);

static enum mtm_node_role mtm_current_node_role = MTM_PASSIVE;



static void discovery_recv(struct neighbor_discovery_conn *c,
    const linkaddr_t *from, uint16_t val) {
    /* printf("discovered %d.%d val %d\n", from->u8[0], from->u8[1], val); */

    // add entry to drand_neighbor_list
    // look whether there is an entry already in our table
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(direct_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        if(linkaddr_cmp(&neighbor->neighbor_addr, from)) {
            break;
        }
    }

    if(neighbor == NULL) {
        // no entry found, create new one
        neighbor = memb_alloc(&direct_neighbor_memb);
        if(neighbor == NULL) {
            printf("could not allocate memory for neighbor\n");
            return;
        }
        linkaddr_copy(&neighbor->neighbor_addr, from);
        neighbor->has_timeslot = 0;
        neighbor->prev_total_found_ours = 0;
        
        list_add(direct_neighbor_list, neighbor);
    }

    neighbor->last_seen = clock_time();

    if(val != 0) {
        neighbor->has_timeslot = 1;
        neighbor->chosen_timeslot = val;
    } else {
        neighbor->has_timeslot = 0;
        neighbor->chosen_timeslot = 0;
    }
}

static void discovery_sent(struct neighbor_discovery_conn *c) {
    /* printf("sent neighbor discovery thingy\n"); */
}

static const struct neighbor_discovery_callbacks neighbor_discovery_calls = { .recv = discovery_recv, .sent = discovery_sent };



static void sched_neighbor_discovery_start() {
  neighbor_discovery_open(&neighbor_discovery,
      42,
      CLOCK_SECOND/2,
      CLOCK_SECOND/8,
      CLOCK_SECOND*2,
      &neighbor_discovery_calls);
  neighbor_discovery_start(&neighbor_discovery, 0);
}

static void sched_neighbor_discovery_stop() {
    neighbor_discovery_close(&neighbor_discovery);    
}

void rand_sched_init(uint8_t max_mtm_slots) {

    node_state.max_slots = max_mtm_slots;
    
    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, node_state.max_slots + MTM_ROUND_START);
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING,
        &tsch_broadcast_address,
        0,
        0);
    
    tsch_schedule_add_link(
        sf_eb,
        LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
        LINK_TYPE_ADVERTISING_ONLY,
        &tsch_broadcast_address,
        1,
        0)    

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
    
    process_start(&rand_sched_process, NULL);
}

/* static uint8_t rule_enough_neighbors(uint8_t min_neighbors) { */
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
        if((clock_time() - n->last_observed) > CLOCK_SECOND*2) {
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
    for (uint8_t i = 0; i < node_state.max_slots+MTM_ROUND_START; i++) {
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
    return !(random_rand() % 6);
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
            struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
            if(sf_eb != NULL) {
                printf("Setting link to tx\n");
                struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot);
                if(l == NULL) {
                    printf("Could not find link\n");
                }
                l->link_options = LINK_OPTION_TX;
            }
        
        neighbor_discovery_set_val(&neighbor_discovery, timeslot);
    }
}

static void backoff_from_decision(uint8_t timeslot) {
    // reset link back to rx link
    /* if(tsch_get_lock()) { */
        struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
        if(sf_eb != NULL) {
            struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, timeslot);
            l->link_options = LINK_OPTION_RX;
        }
        /* tsch_release_lock(); */
    /* } */

}


// check whether timeslot was a good decision
static uint8_t good_decision(uint8_t timeslot) {
    printf("Checking whether our decisions were good \n");
    
    // first check for collisions
    uint8_t collisions = 0;
    // look through neighbor table if we find any collisions
    struct mtm_neighbor *n = NULL;
    for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
        printf("neighbor %u, timeslot %u\n", n->neighbor_addr, n->observed_timeslot);
        // outdated neighbor entries should not influence our decision
        if((clock_time() - n->last_observed) > CLOCK_SECOND*2) {
            continue;
        }


        if(n->observed_timeslot == timeslot) {
            collisions++;
            printf("Collision with node %u in timeslot %u\n", n->neighbor_addr, timeslot);
        }
    }

    if(collisions > 10) {
        printf("We had %u collisions\n", collisions);
        return 0;
    }


    // search matching entry in our rand-sched neighbor discovery service
    struct drand_neighbor_state *r = NULL;
    for(r = list_head(direct_neighbor_list); r != NULL; r = list_item_next(r)) {
        
        if((clock_time() - r->last_seen) > CLOCK_SECOND*10) {
            // probably outdated entry
            printf("long time no see, so we no care\n");
            continue;
        }

        /* if(!r->has_timeslot) { */
        /*     // probably outdated entry */
        /*     printf("no timeslot, so we no care\n"); */
        /*     continue; */
        /* } */
        
        // search matching neighbor entry in prop neighbor table
        struct mtm_neighbor *n = NULL;
        for(n = list_head(tsch_prop_get_neighbor_list()); n != NULL; n = list_item_next(n)) {
            if(n->neighbor_addr == r->neighbor_addr.u8[LINKADDR_SIZE-1]) {
                printf("found tsch prop entry \n");
                break;
            }
        }

        // this would be bad, if our neighbor discovery discovered a node that we did not observe during the ranging process
        // than this indicates that our slot assigment lead somewhere to collisions
        if (n == NULL) {
            printf("no tsch prop entry found for %u\n", r->neighbor_addr.u8[LINKADDR_SIZE-1]);
            return 0;
            /* continue; */
        }
        
        uint64_t new_since_last_check = n->total_found_ours_counter - r->prev_total_found_ours;
        r->prev_total_found_ours = n->total_found_ours_counter;

        // TODO should be calculated dynamically instead of being hardcoded
        printf(LOG_LLU_MARK " new since last check\n",  LOG_LLU(new_since_last_check));
        if (new_since_last_check < 3) {
            return 0;
        }
    }

    return 1;
}

/* static void merge_neighbor_information() { */
/*     // go through tsch_prop stats and augment our neighbor information with the information gathered */
/*     struct  */
/* } */

/* void mtm_scheduling_decision() { */
/*     uint8_t occupied_timeslots[MTM_MAX_RANGING_SLOTS+1]; */
/*     uint8_t directCount = 0, indirectCount = 0; // counters for direct and indirect taken slots */

/*     // initialize occupied timeslots to zero */
/*     memset(occupied_timeslots, 0, sizeof(occupied_timeslots)); */

/*     //iterate over links an mark all links that are not of type MTM_PROP as unavailable */
    
/*     struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0); */
/*     if(sf_eb != NULL) { */
/*         struct tsch_link *l = NULL; */
/*         for(l = list_head(sf_eb->links_list); l != NULL; l = list_item_next(l)) { */
/*             if (l->link_type != LINK_TYPE_PROP_MTM) { */
/*                 occupied_timeslots[l->timeslot] = 1; */
/*             } */
/*         } */
/*     } */

/*     /\* occupied_timeslots[0] = 1; *\/ */
/*     /\* printf("scheduling_decision, current Role: %d, with slot: %d\n", mtm_current_node_role, mtm_active_our_timeslot); *\/ */
    
/*     // iterate over neighbor table */
/*     struct mtm_neighbor *n = NULL; */
/*     for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) { */
/*         // if we have a two hop neighbor and we have a direct/indirect neighbor and we didn't observe it for some time we remove it */
/*         if (n->missed_observations >= MAX_MISSED_OBSERVATIONS) { */
/*             // remove from list */
/*             list_remove(ranging_neighbor_list, n); */
/*             // free memory */
/*             memb_free(&mtm_prop_neighbor_memb, n); */
/*         } else { */
/*             // entry still valid, mark timeslot as occupied */
/*             occupied_timeslots[n->timeslot_offset] = 1; */
/*             if(n->type == MTM_DIRECT_NEIGHBOR) { */
/*                 directCount++; */
/*             } else { */
/*                 indirectCount++; */
/*             } */
/*         } */
/*     } */

/*     // in case that we already have a timeslot, skip */
/*     if (mtm_current_node_role == MTM_ACTIVE || mtm_current_node_role == MTM_ANCHOR) { */
/*         return; */
/*     } */

/*     // otherwise check whether we are eligible to take a slot */
/*     if(directCount > MTM_MAX_DIRECT) { */
/*         return; */
/*     } */
    

/*     // if there is a free timeslot we will take it greedily */
/*     _PRINTF("mtm_scheduling_decision: checking for free timeslots\n"); */
/*     for (int i = 0; i < MTM_MAX_RANGING_SLOTS+1; i++) { */
/*         if (!occupied_timeslots[i]) { */
/*             mtm_active_our_timeslot = i; */
/*             mtm_current_node_role = MTM_ACTIVE;             */
/*             mtm_active_missed_observations = 0; */
/*             // we found a free timeslot, lets create a tx link here for ourselves */

/*             _PRINTF("taking slot %d\n", mtm_active_our_timeslot); */
            
/*             struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0); */
/*             if(sf_eb != NULL) { */
/*                 struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, i); */
/*                 l->link_options = LINK_OPTION_TX; */
/*             } */
            
/*             break; */
/*         } */
/*     } */
/* } */


/* void mtm_update_schedule() { */
/*     mtm_scheduling_decision(); */

/*     if (mtm_current_node_role == MTM_ACTIVE && mtm_active_missed_observations >= MAX_MISSED_OBSERVATIONS) { */
/*         mtm_current_node_role = MTM_PASSIVE; */
/*         mtm_active_missed_observations = 0; */
/*         _PRINTF("resetting\n"); */

/*         // install rx link again */
/*         struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);             */

/*         if(sf_eb != NULL) { */
/*             struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, mtm_active_our_timeslot); */
/*             l->link_options = LINK_OPTION_RX; */
/*         } */
/*     } */
/* } */

static void print_rand_sched_status() {
    // nicely print current node status and status of neighbors
    PRINTF("N %u.%u: ", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);
    PRINTF("\n");

    printf("dpnl ");    
    struct drand_neighbor_state *neighbor = NULL;
    for(neighbor = list_head(direct_neighbor_list); neighbor != NULL; neighbor = neighbor->next) {
        /* PRINTF("Neighbor %u.%u, Status: ", neighbor->neighbor_addr.u8[0], neighbor->neighbor_addr.u8[1]); */

        /* printf("last_seen %u ", clock_time() - neighbor->last_seen); */
        /* printf("prev_total_found_ours 0x%08x%08x ", (uint32_t)(((neighbor->prev_total_found_ours) >> 32)), (uint32_t)((neighbor->prev_total_found_ours) & 0xFFFFFFFF)); */

        /* if(neighbor->has_timeslot) { */
        /*     printf("timeslot %u ", neighbor->chosen_timeslot); */
        /* } */

        printf("%u ", neighbor->neighbor_addr.u8[LINKADDR_SIZE-1]);
        
        /* watchdog_periodic(); */
    }
    
    printf("\n");    

    struct mtm_neighbor *mtm_neighbor = NULL;
    printf("tpnl ");
    for(mtm_neighbor = list_head(tsch_prop_get_neighbor_list()); mtm_neighbor != NULL; mtm_neighbor = mtm_neighbor->next) {
        /* PRINTF("Neighbor %u.%u, Status: ", neighbor->neighbor_addr.u8[0], neighbor->neighbor_addr.u8[1]); */

        /* printf("total_found_ours 0x%08x%08x ", (uint32_t)(((mtm_neighbor->total_found_ours_counter) >> 32)), (uint32_t)((mtm_neighbor->total_found_ours_counter) & 0xFFFFFFFF)); */
        printf("%u ", mtm_neighbor->neighbor_addr);
        
        /* watchdoc periodic */
        /* watchdog_periodic(); */
    }
    printf("\n");

    uint8_t timeslot = check_have_timeslot();
    /* if(timeslot) { */
    printf("ts %u\n", timeslot);
    /* } */
}

PROCESS_THREAD(rand_sched_process, ev, data)
{
  PROCESS_BEGIN();

  static uint8_t drand_running = 0;

  printf("starting rand_sched_process\n");
  
  /* etimer_set(&neighbor_discovery_phase_timer, CLOCK_SECOND * 40); */

  etimer_set(&initial_wait_timer, CLOCK_SECOND * 10); // wait 10 seconds before starting
  /* etimer_set(&decision_evaluation_timer, CLOCK_SECOND/2); // every 250ms make a scheduling decision   */

  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&initial_wait_timer));
  
  etimer_set(&rand_schedule_phase_timer, CLOCK_SECOND/2); // every 250ms make a scheduling decision
  
  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rand_schedule_phase_timer)
          /* || etimer_expired(&decision_evaluation_timer) */
          );

      if(etimer_expired(&rand_schedule_phase_timer)) {
          // play the lottery
          if(tsch_is_associated) {
              uint8_t our_timeslot = check_have_timeslot();
              print_rand_sched_status();
          
              if(!our_timeslot && play_lottery()) {
                  printf("Won lottery. Choosing free slot\n");
                  handle_won_lottery();
                  sched_neighbor_discovery_start();
              } else {
                  if(!good_decision(our_timeslot)) {
                      printf("Bad decision. Backing off\n");
                      backoff_from_decision(our_timeslot);
                      
                      sched_neighbor_discovery_stop();
                      neighbor_discovery_set_val(&neighbor_discovery, 0);                      
                  }
              }
          }
          
          etimer_reset(&rand_schedule_phase_timer);                          
      }

      
      /* if(etimer_expired(&decision_evaluation_timer)) { */
          uint8_t our_timeslot = check_have_timeslot();
 
          
          /* etimer_reset(&decision_evaluation_timer); */
      /* } */
  }

  PROCESS_END();
}
