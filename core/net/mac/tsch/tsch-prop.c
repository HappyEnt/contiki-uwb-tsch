/*
 * Copyright (c) 2014, SICS Swedish ICT.
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         ITSCH MAC localization manager.
 * \author
 *         Maximilien Charlier <maximilien.charlier@umons.ac.be>
 */


#include "contiki.h"
#include "linkaddr.h"
#include "list.h"
#include "net/mac/tsch/tsch-asn.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-prop.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "dev/radio.h"
#include "memb.h"
#include "lib/random.h"

#include "nrfx_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_stdint.h>

#include "dw1000-driver.h"
#include "dw1000-ranging-bias.h"
#include "tsch-slot-operation.h"

#if TSCH_MTM_LOCALISATION

#define TMP_DEBUG_MTM_LOCALISATION 0
#if TMP_DEBUG_MTM_LOCALISATION
#define _PRINTF(...) printf(__VA_ARGS__)
#else
#define _PRINTF(...)
#endif

// NRF SDK does not support long long, so we define here some macros to dissect the number into two parts
// has to be converted in the mind of the reader :)
#define LOG_LLU_MARK "0x%08x%08x"
#define LOG_LLU(bigint) (uint32_t)(((bigint) >> 32)), (uint32_t)((bigint) & 0xFFFFFFFF)

#define SPEED_OF_LIGHT_M_PER_S 299702547.236
#define SPEED_OF_LIGHT_M_PER_UWB_TU ((SPEED_OF_LIGHT_M_PER_S * 1.0E-15) * 15650.0) // around 0.00469175196

struct ds_twr_ts
{
    uint64_t t_a1, r_b1, t_b1, r_a1, t_a2, r_b2, t_b2, r_a2;
};

#define WITH_PASSIVE_TDOA 1
#if WITH_PASSIVE_TDOA
// this structure tracks timestamps for a passive tdoa measurement to
// two neighbors A and B. Hereby we virtually identify A as being in the initiator role
// and B taking the responder hole. See paper for more details on this measurement method.
// with L we denote ourself as the passive listener.

struct mtm_pas_tdoa
{
    struct mtm_pas_tdoa *next;

    // For internal mangement we will identify with node A the node that initiated a transfer
    // and with node B the responder. Off course both nodes take either role, but
    // to not mix up the timestamps we will always use the same notation.
    ranging_addr_t A_addr; 
    ranging_addr_t B_addr;

    struct distance_measurement last_measurement;
    
    struct ds_twr_ts ds_ts;
    uint64_t r_l1, r_l2, r_l3, r_l4;
};

int32_t mtm_compute_tdoa(struct mtm_pas_tdoa *ts);

#endif

enum mtm_neighbor_type  {
    MTM_DIRECT_NEIGHBOR,
    MTM_TWO_HOP_NEIGHBOR,
};

enum mtm_node_role {
    MTM_PASSIVE,
    MTM_ACTIVE,
    MTM_ANCHOR // an anchor is a active participant that never loses its slot 
};

#define MAX_MISSED_OBSERVATIONS 5 // after not observing a neighbor for 5 rounds we will consider him a goner

struct mtm_neighbor {
    struct mtm_neighbor *next;

    ranging_addr_t neighbor_addr;
    struct ds_twr_ts ts;
    
    struct distance_measurement last_measurement;

    enum mtm_neighbor_type type;

    uint8_t missed_observations, observed_this_round, timeslot_offset;
};

void mtm_compute_dstwr(struct tsch_asn_t *asn, struct mtm_neighbor *mtm_n);
/* float calculate_propagation_time_alternative(struct ds_twr_ts *ts); */
int32_t calculate_propagation_time_alternative(struct ds_twr_ts *ts);

// in the following we use the value -1 for uninitialized timestamps
struct mtm_rx_queue_item {
    struct mtm_rx_queue_item *next;
    ranging_addr_t neighbor_addr;
    uint64_t rx_timestamp;
    uint8_t timeslot_offset;
};

struct mtm_timestamp_assoc {
    struct mtm_timestamp_assoc *next;
    struct tsch_asn_t asn;
    uint64_t timestamp;
};

LIST(ranging_neighbor_list);
LIST(rx_send_queue);

#if WITH_PASSIVE_TDOA
LIST(pas_tdoa_list);
#endif

MEMB(mtm_prop_neighbor_memb, struct mtm_neighbor, TSCH_MTM_PROP_MAX_MEASUREMENT);
MEMB(mtm_prop_rx_memb, struct mtm_rx_queue_item, TSCH_MTM_PROP_MAX_NEIGHBORS);
MEMB(mtm_pas_tdoa_memb, struct mtm_pas_tdoa, TSCH_MTM_PROP_MAX_NEIGHBORS*TSCH_MTM_PROP_MAX_NEIGHBORS);


// this will be used to return a array of timestamps that we read from the user
static struct mtm_packet_timestamp return_timestamps[TSCH_MTM_PROP_MAX_NEIGHBORS];

static enum mtm_node_role mtm_current_node_role = MTM_PASSIVE;
static uint8_t mtm_active_our_timeslot = 0;
static uint8_t mtm_active_missed_observations = 0;


// protos
void print_tdoa_timestamps(struct mtm_pas_tdoa *ts);
void print_ds_twr_timestamps(struct ds_twr_ts *ts);
void print_mtm_neighbors();
void notify_user_process_new_measurement(struct distance_measurement *measurement);

// for all timestamps check overflow condition. The internal clock of the decawave transceiver will overflow roughly every 17 seconds
// therefore sometimes a timestamp which should be bigger, will be smaller than a preceding one. DW_TIMESTAMP_MAX_VALUE
// this function expects that the user passes in high_ts the timestamp that is supposed to be the larger value of
// the given interval.
static inline int64_t interval_correct_overflow(uint64_t high_ts, uint64_t low_ts) {
    // cast numbers to int
    int64_t high_ts_i = (int64_t) high_ts;
    int64_t low_ts_i = (int64_t) low_ts;
    if (high_ts_i < low_ts_i) {
        printf("interval_correct_overflow: high_ts < low_ts\n");
        return (DW_TIMESTAMP_MAX_VALUE - low_ts_i) + high_ts_i;
    }

    return high_ts_i - low_ts_i;
}

void init_ds_twr_struct(struct ds_twr_ts *ds) {
    ds->r_a1 = UINT64_MAX;
    ds->r_a2 = UINT64_MAX;
    ds->r_b1 = UINT64_MAX;
    ds->r_b2 = UINT64_MAX;
    ds->t_a1 = UINT64_MAX;
    ds->t_a2 = UINT64_MAX;
    ds->t_b1 = UINT64_MAX;
    ds->t_b2 = UINT64_MAX;
}

#if WITH_PASSIVE_TDOA
void init_tdoa_struct(struct mtm_pas_tdoa *tdoa) {
    tdoa->r_l1 = UINT64_MAX;
    tdoa->r_l2 = UINT64_MAX;
    tdoa->r_l3 = UINT64_MAX;
    tdoa->r_l4 = UINT64_MAX;    
    init_ds_twr_struct(&tdoa->ds_ts);
}

int pas_tdoa_all_initialized(struct mtm_pas_tdoa *tdoa) {
    if (tdoa->r_l1 == UINT64_MAX) return 0;
    if (tdoa->r_l2 == UINT64_MAX) return 0;
    if (tdoa->r_l3 == UINT64_MAX) return 0;
    if (tdoa->r_l4 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.r_a1 == UINT64_MAX) return 0;
    /* if (tdoa->ds_ts.r_a2 == UINT64_MAX) return 0; */
    if (tdoa->ds_ts.r_b1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.r_b2 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.t_a1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.t_a2 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.t_b1 == UINT64_MAX) return 0;
    /* if (tdoa->ds_ts.t_b2 == UINT64_MAX) return 0; */
    return 1;
}

#endif

void print_mtm_neighbors() {
    printf("-------MTM NEIGHBORS-------\n");
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        printf("%d, type: %d, timeslot %d, missed_observations: %d \n", n->neighbor_addr, n->type, n->timeslot_offset, n->missed_observations);
    }
}


void mtm_scheduling_decision() {
    uint8_t occupied_timeslots[7] = {0x00};

    //iterate over links an mark all links that are not of type MTM_PROP as unavailable
    
    struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
    if(sf_eb != NULL) {
        struct tsch_link *l = NULL;
        for(l = list_head(sf_eb->links_list); l != NULL; l = list_item_next(l)) {
            if (l->link_type != LINK_TYPE_PROP_MTM) {
                occupied_timeslots[l->timeslot] = 1;
            }
        }
    }

    /* occupied_timeslots[0] = 1; */
    printf("scheduling_decision, current Role: %d, with slot: %d\n", mtm_current_node_role, mtm_active_our_timeslot);
    
    // iterate over neighbor table
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        // if we have a two hop neighbor and we have a direct/indirect neighbor and we didn't observe it for some time we remove it

        if(!n->observed_this_round) {
            n->missed_observations++;
        }
        
        if (n->missed_observations >= MAX_MISSED_OBSERVATIONS) {
            // remove from list
            list_remove(ranging_neighbor_list, n);
            // free memory
            memb_free(&mtm_prop_neighbor_memb, n);
        } else {
            // entry still valid, mark timeslot as occupied
            occupied_timeslots[n->timeslot_offset] = 1;
        }
    }

    // in case that we already have a timeslot, skip
    if (mtm_current_node_role == MTM_ACTIVE || mtm_current_node_role == MTM_ANCHOR) {
        return;
    }

    // if there is a free timeslot we will take it greedily
    printf("mtm_scheduling_decision: checking for free timeslots\n");
    for (int i = 0; i < 7; i++) {
        if (!occupied_timeslots[i]) {
            mtm_active_our_timeslot = i; // we have to skip the first slot
            mtm_current_node_role = MTM_ACTIVE;            
            mtm_active_missed_observations = 0;
            // we found a free timeslot, lets create a tx link here for ourselves

            printf("taking slot %d\n", mtm_active_our_timeslot);
            
            struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);
            if(sf_eb != NULL) {
                struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, i);
                l->link_options = LINK_OPTION_TX;
            }
            
            break;
        }
    }
}

void mtm_indirect_observed_node(ranging_addr_t neighbor, uint8_t timeslot_offset) {
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (n->neighbor_addr == neighbor) {
            break;
        }
    }

    if (n != NULL) {
        // in case that the entry is a of MTM_DIRECT_TYPE we will do nothing
        if (n->type == MTM_TWO_HOP_NEIGHBOR) {
            n->missed_observations = 0;
            n->observed_this_round = 1;            
        }
    } else {
        // create new neighbor
        struct mtm_neighbor *new_neighbor = memb_alloc(&mtm_prop_neighbor_memb);
        if (new_neighbor != NULL) {
            new_neighbor->neighbor_addr = neighbor;
            new_neighbor->type = MTM_TWO_HOP_NEIGHBOR;
            new_neighbor->missed_observations = 0;
            new_neighbor->observed_this_round = 1;
            new_neighbor->timeslot_offset = timeslot_offset;
            list_add(ranging_neighbor_list, new_neighbor);
        }
    }
}

void mtm_direct_observed_node(ranging_addr_t node, uint8_t timeslot_offset) {
  // we don't pass this outside so we will just add it here for now
  // iterate over neighbor table
  struct mtm_neighbor *n = NULL;
  for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
      if (n->neighbor_addr == node) {
          break;
      }
  }

  if (n != NULL) {
      // set neighbor type to direct neighbor
      n->type = MTM_DIRECT_NEIGHBOR;
      n->missed_observations = 0;
      n->observed_this_round = 1;
      n->timeslot_offset = timeslot_offset;
  }
}

void mtm_update_schedule() {
    print_mtm_neighbors();    
    mtm_scheduling_decision();

    if (mtm_current_node_role == MTM_ACTIVE && mtm_active_missed_observations > 50) {
        mtm_current_node_role = MTM_PASSIVE;
        mtm_active_missed_observations = 0;
        printf("resetting\n");

        // install rx link again
        tsch_get_lock();
        struct tsch_slotframe *sf_eb = tsch_schedule_get_slotframe_by_handle(0);            

        if(sf_eb != NULL) {
            struct tsch_link *l = tsch_schedule_get_link_by_timeslot(sf_eb, mtm_active_our_timeslot);
            l->link_options = LINK_OPTION_RX;
        }

        tsch_release_lock();
    }
}

void mtm_end_of_round() {
    // increase missed observations for all nodes that we did not observe this round
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (!n->observed_this_round) {
            n->missed_observations++;
        }
    }
}

void mtm_init(uint8_t is_coordinator) {
  uint16_t timeslot_amount = 7;
  
  struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, timeslot_amount);
  tsch_schedule_add_link(
          sf_eb,
          LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
          LINK_TYPE_ADVERTISING,
          &tsch_broadcast_address,
          0,
          0);
    // if our node_role is role_6dr we will take the first TX slot



    tsch_schedule_add_link(
            sf_eb,
            is_coordinator ? LINK_OPTION_TX : LINK_OPTION_RX,
            LINK_TYPE_PROP_MTM,
            &tsch_broadcast_address,
            1,
            0
        );

    // initialize all other links as RX PROP MTM
    for(int i = 2; i < timeslot_amount; i++) {
            tsch_schedule_add_link(
                sf_eb,
                LINK_OPTION_RX,
                LINK_TYPE_PROP_MTM,
                &tsch_broadcast_address,
                i,
                0);
    }


    if(is_coordinator) {
        mtm_current_node_role = MTM_ANCHOR;
        mtm_active_our_timeslot = 1;
    } else {
        mtm_current_node_role = MTM_PASSIVE;
    }
}


void add_to_direct_observed_rx_to_queue(uint64_t rx_timestamp, uint8_t neighbor, uint8_t timeslot_offset) {
    // first update our own outgoing rx queue

    struct mtm_rx_queue_item *t = NULL;
    
    if (list_length (rx_send_queue) >= TSCH_MTM_PROP_MAX_NEIGHBORS) {
        printf("MTM: RX queue full, dropping received timestamp\n");
        return;
    }

    // since we are space constrained if there is already a timestamp for the neighbor, we will replace that entry in our queue
    for(t = list_head(rx_send_queue); t != NULL; t = list_item_next(t)) {
        if (t->neighbor_addr == neighbor) {
            break;
        }
    }

    if (t != NULL) {
        // replace entry
        t->rx_timestamp = rx_timestamp;
        t->neighbor_addr = neighbor;
    } else {
        t = memb_alloc(&mtm_prop_rx_memb);

        if (t != NULL) {
            t->rx_timestamp = rx_timestamp;
            t->neighbor_addr = neighbor;
            t->timeslot_offset = timeslot_offset;
            list_add(rx_send_queue, t);
        } else {
            printf("MTM: Could not allocate memory for reception timestamp\n");
            return;
        }
    }
}

void add_mtm_reception_timestamp(
    ranging_addr_t neighbor_addr,
    struct tsch_asn_t *asn,

    uint64_t rx_timestamp_A,    
    uint64_t tx_timestamp_B,
    
    struct mtm_packet_timestamp *rx_timestamps,
    uint8_t num_rx_timestamps
    )
{
    // next update our neighbor table
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (n->neighbor_addr == neighbor_addr) {
            break;
        }
    }

    // if no neighbor entry could be found we will create an entry for the neighbor
    if (n == NULL) {
        printf("MTM: No neighbor found for reception timestamp, Register new entry\n");
        // register neighbor in our ranging structure
        n = memb_alloc(&mtm_prop_neighbor_memb);
        if (n != NULL) {
            n->neighbor_addr =  neighbor_addr;
            init_ds_twr_struct(&n->ts);
            list_add(ranging_neighbor_list, n);
        } else {
            _PRINTF("MTM: Could not allocate memory for neighbor\n");
            return;            
        }
    }

    #if WITH_PASSIVE_TDOA
    // if passive tdoa is enabled we will update the passive tdoa structure for all neighbors in the message
    // that is even if we did not directly communicate with a pair of neighbors we can still derive a TDoA
    // estimate towards them using the method outlined in the paper

    
    // address of neighbor
    ranging_addr_t m_addr = n->neighbor_addr;
    
    for(uint8_t i = 0; i < num_rx_timestamps; i++) {
        ranging_addr_t rx_addr;
        uint64_t rx_timestamp;

        rx_addr = rx_timestamps[i].addr;
        rx_timestamp = rx_timestamps[i].rx_timestamp;
        // first we will handle the case where we imagine that the neighbor from which we received
        // the packet was the initiator in the TDoA scheme

        if(rx_addr == linkaddr_node_addr.u8[LINKADDR_SIZE - 1]) {
            continue;
        }

        struct mtm_pas_tdoa *pas_tdoa = NULL;

        // search for a pair with rx_addr and m_addr in the pas_tdoa_list
        for(pas_tdoa = list_head(pas_tdoa_list);
            pas_tdoa != NULL; pas_tdoa = list_item_next(pas_tdoa)) {
            if ((pas_tdoa->B_addr == rx_addr && pas_tdoa->A_addr == m_addr) ||
                (pas_tdoa->A_addr == rx_addr && pas_tdoa->B_addr == m_addr)) {
                break;
            }
        }

        if(pas_tdoa == NULL) {
            printf("MTM: No pas_tdoa found for %u and %u, create new entry\n", rx_addr, m_addr);
            // create a new entry
            pas_tdoa = memb_alloc(&mtm_pas_tdoa_memb);
            if (pas_tdoa != NULL) {
                init_tdoa_struct(pas_tdoa);
                // in case that we have not any entry yet, we will identify with A the initiator and B the responder
                pas_tdoa->B_addr = rx_addr;
                pas_tdoa->A_addr = m_addr;

                // add to pas_tdoa_list
                list_add(pas_tdoa_list, pas_tdoa);
            } else {
                _PRINTF("MTM: Could not allocate memory for pas_tdoa\n");
                return;
            }
        }

        // here we do the handling in case we found A in the list as initator
        if (pas_tdoa->A_addr == m_addr) {
            
            pas_tdoa->r_l1 = pas_tdoa->r_l3;
            pas_tdoa->r_l2 = pas_tdoa->r_l4;
            pas_tdoa->r_l4 = UINT64_MAX; // to be filled in by the responder message in the other case below
            
            pas_tdoa->r_l3 = rx_timestamp_A;
        
            pas_tdoa->ds_ts.t_a1 = pas_tdoa->ds_ts.t_a2;
            pas_tdoa->ds_ts.t_a2 = tx_timestamp_B;
        
            pas_tdoa->ds_ts.r_a1 = rx_timestamp;

            pas_tdoa->ds_ts.r_b1 = pas_tdoa->ds_ts.r_b2;
            pas_tdoa->ds_ts.r_b2 = UINT64_MAX; // to be filled in by the responder message in the other case below

            pas_tdoa->ds_ts.t_b1 = pas_tdoa->ds_ts.t_b2;
            pas_tdoa->ds_ts.t_b2 = UINT64_MAX; // to be filled in by the responder message in the other case below
            
        } else if (pas_tdoa->B_addr == m_addr) {
            // this message concludes the round, calculate new TDoA estimate
            int32_t tdoa;
            
            pas_tdoa->ds_ts.r_b2 = rx_timestamp;
            pas_tdoa->ds_ts.t_b2 = tx_timestamp_B;
            pas_tdoa->r_l4 = rx_timestamp_A;

            // check whether we have uninitialized entries

            if(pas_tdoa_all_initialized(pas_tdoa)) {
                tdoa = mtm_compute_tdoa(pas_tdoa);

                // update stored most recent measurement and notify user
                pas_tdoa->last_measurement.type = TDOA;
                pas_tdoa->last_measurement.addr_A = rx_addr;
                pas_tdoa->last_measurement.addr_B = m_addr;
                pas_tdoa->last_measurement.time = tdoa;
            
                notify_user_process_new_measurement(&pas_tdoa->last_measurement);                
            }
        }

        /* linkaddr_t *neighbor_addr, */
    }
    
    #endif

    // next we extract information for two way ranging
    int found_rx = 0;
    for(uint8_t i = 0; i < num_rx_timestamps; i++) {
        if(rx_timestamps[i].addr == linkaddr_node_addr.u8[LINKADDR_SIZE - 1]) {
                uint64_t rx_timestamp_B = rx_timestamps[i].rx_timestamp;
                
                // current round is always the one with the higher numbers
                n->ts.r_b1 = n->ts.r_b2;
                n->ts.t_b1 = n->ts.t_b2;
                n->ts.r_a1 = n->ts.r_a2;

                n->ts.r_b2 = rx_timestamp_B;
                n->ts.t_b2 = tx_timestamp_B;
                n->ts.r_a2 = rx_timestamp_A;

                mtm_compute_dstwr(asn, n);
                found_rx = 1;
        }
    }

    if (!found_rx) {
        // Here we could perform different strategies, what we do now is in case that if we don't find a reception timestmap
        // which was generated on device B in as a result of our transmission, then we just reset the whole structure
        // Another strategy could be leaving the partial good timestamps from the first transmissions and just delete
        // the last failed transmission timestmap t_a2.
        /* printf("MTM: Did not find our own rx_timestamp, reset ranging structure\n"); */
        init_ds_twr_struct(&n->ts);
    }
}

void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp) {
    // Update mtm_neighbor list after our own transmission event
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        n->ts.t_a1 = n->ts.t_a2;
        n->ts.t_a2 = tx_timestamp;
    }
}

void print_ds_twr_durations(struct ds_twr_ts *ts) {
    int64_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply;

    initiator_roundtrip = interval_correct_overflow(ts->r_a1,  ts->t_a1);
    // ts->txA2 - ts->rxA1;
    initiator_reply = interval_correct_overflow(ts->t_a2, ts->r_a1);
    // ts->rxB2 - ts->txB1;
    replier_roundtrip = interval_correct_overflow(ts->r_b2, ts->t_b1);
    // ts->txB1 - ts->rxB1;
    replier_reply = interval_correct_overflow(ts->t_b1, ts->r_b1);

    printf("MTM: Initiator roundtrip: %d\n", initiator_roundtrip);
    printf("MTM: Initiator reply: %d\n", initiator_reply);
    printf("MTM: Replier roundtrip: %d\n", replier_roundtrip);
    printf("MTM: Replier reply: %d\n", replier_reply);
}

void print_ds_twr_timestamps(struct ds_twr_ts *ts) {
    printf("t_a1: " LOG_LLU_MARK "\n", LOG_LLU(ts->t_a1));
    printf("r_a1: " LOG_LLU_MARK "\n", LOG_LLU(ts->r_a1));
    printf("t_a2: " LOG_LLU_MARK "\n", LOG_LLU(ts->t_a2));
    printf("r_a2: " LOG_LLU_MARK "\n", LOG_LLU(ts->r_a2));

    printf("r_b1: " LOG_LLU_MARK "\n", LOG_LLU(ts->r_b1));
    printf("t_b1: " LOG_LLU_MARK "\n", LOG_LLU(ts->t_b1));
    printf("r_b2: " LOG_LLU_MARK "\n", LOG_LLU(ts->r_b2));
    printf("t_b2: " LOG_LLU_MARK "\n", LOG_LLU(ts->t_b2));
}

void print_tdoa_timestamps(struct mtm_pas_tdoa *ts) {
    printf(" t_a1: " LOG_LLU_MARK " r_b1: " LOG_LLU_MARK " t_b1: " LOG_LLU_MARK
            " r_a1: " LOG_LLU_MARK " t_a2: " LOG_LLU_MARK " r_b2: " LOG_LLU_MARK
            " t_b2: " LOG_LLU_MARK " r_a2: " LOG_LLU_MARK " r_l1: " LOG_LLU_MARK
        " r_l2: " LOG_LLU_MARK " r_l2: " LOG_LLU_MARK "\n",
        LOG_LLU(ts->ds_ts.t_a1), LOG_LLU(ts->ds_ts.r_b1), LOG_LLU(ts->ds_ts.t_b1),
        LOG_LLU(ts->ds_ts.r_a1), LOG_LLU(ts->ds_ts.t_a2), LOG_LLU(ts->ds_ts.r_b2),
        LOG_LLU(ts->ds_ts.t_b2), LOG_LLU(ts->ds_ts.r_a2), LOG_LLU(ts->r_l1), LOG_LLU(ts->r_l2),
        LOG_LLU(ts->r_l3));
}

void notify_user_process_new_measurement(struct distance_measurement *measurement) {
  /* Send the PROCESS_EVENT_MSG event asynchronously to
  "tsch_loc_operation", with a pointer to the tsch_neighbor. */
  process_post(&TSCH_PROP_PROCESS,
      PROCESS_EVENT_MSG, (void *) measurement);
}

void mtm_compute_dstwr(struct tsch_asn_t *asn, struct mtm_neighbor *mtm_n) {
    // first check whether neighbor has a valid entry in our list and none of the timestamps are uninitialized, i.e., of value UINT64_MAX;

    if (mtm_n == NULL) {
        _PRINTF("MTM: Neighbor not found in list\n");
        return;
    }

    if (mtm_n->ts.t_a1 == UINT64_MAX || mtm_n->ts.r_b1 == UINT64_MAX
        || mtm_n->ts.t_b1 == UINT64_MAX || mtm_n->ts.r_a1 == UINT64_MAX
        || mtm_n->ts.t_a2 == UINT64_MAX || mtm_n->ts.r_b2 == UINT64_MAX
        || mtm_n->ts.t_b2 == UINT64_MAX || mtm_n->ts.r_a2 == UINT64_MAX
        ) {
        printf("MTM: Neighbor has uninitialized timestamps\n");

        return;
    }

    int32_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply;

    /* Finally calculate round trip times */

    /* initiator_roundtrip = interval_correct_overflow(mtm_n->ts.r_a1,  mtm_n->ts.t_a1); */
    /* initiator_reply = interval_correct_overflow(mtm_n->ts.t_a2, mtm_n->ts.r_a1); */
    /* replier_roundtrip = interval_correct_overflow(mtm_n->ts.r_b2, mtm_n->ts.t_b1); */
    /* replier_reply = interval_correct_overflow(mtm_n->ts.t_b1, mtm_n->ts.r_b1); */

    /* int32_t prop_time  = compute_prop_time(initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply); */
    int32_t prop_time  = calculate_propagation_time_alternative(&(mtm_n->ts));   
    /* float prop_time = calculate_propagation_time_alternative(&(mtm_n->ts)); */
    float range = time_to_dist(prop_time);

    // bias correction in centimeters
    /* bias_correction = dw1000_getrangebias(n->last_prop_time.tsch_channel, range); */
    _PRINTF("bias uncorrected range to %u: " NRF_LOG_FLOAT_MARKER "\n", mtm_n->neighbor_addr, NRF_LOG_FLOAT(range));
    _PRINTF("prop time to %u: %d\n", mtm_n->neighbor_addr.u8[LINKADDR_SIZE-1], prop_time);
    // call into existing methods for passing data to user

    // TODO Added for debugging remove again
    if (range < -10 || range > 10) {
        print_ds_twr_timestamps(&mtm_n->ts);        
        print_ds_twr_durations(&mtm_n->ts);
    }
    
    // update stored measurement for node
    mtm_n->last_measurement.type = TWR;
    mtm_n->last_measurement.addr_A = linkaddr_node_addr.u8[LINKADDR_SIZE-1];
    mtm_n->last_measurement.addr_B = mtm_n->neighbor_addr;
    mtm_n->last_measurement.time = prop_time;
    
    notify_user_process_new_measurement(&mtm_n->last_measurement);

    /* update_neighbor_prop_time(n, prop_time, asn, UINT8_MAX); */
}

// We put functionality regarding package creation here for now. This allows the measurement_list to
// remain inside this functional unit and not leak out.
int tsch_packet_create_multiranging_packet(
    uint8_t *buf,
    int buf_size,
    const linkaddr_t *dest_addr,
    uint8_t seqno,
    uint64_t tx_timestamp
    )
{
  uint8_t curr_len = 0;
  frame802154_t p;

  memset(&p, 0, sizeof(p));
  p.fcf.frame_type = FRAME802154_DATAFRAME;
  p.fcf.frame_version = FRAME802154_IEEE802154E_2012;
  p.fcf.ie_list_present = 0;
  /* Compression unset. According to IEEE802.15.4e-2012:
   * - if no address is present: elide PAN ID
   * - if at least one address is present: include exactly one PAN ID (dest by default) */
  p.fcf.panid_compression = 0;
  p.dest_pid = IEEE802154_PANID;
  p.seq = seqno;

  if(dest_addr != NULL) {
    p.fcf.dest_addr_mode = LINKADDR_SIZE > 2 ? FRAME802154_LONGADDRMODE : FRAME802154_SHORTADDRMODE;;
    linkaddr_copy((linkaddr_t *)&p.dest_addr, dest_addr);
  }

  p.fcf.src_addr_mode = LINKADDR_SIZE > 2 ? FRAME802154_LONGADDRMODE : FRAME802154_SHORTADDRMODE;;
  p.src_pid = IEEE802154_PANID;
  linkaddr_copy((linkaddr_t *)&p.src_addr, &linkaddr_node_addr);

#if LLSEC802154_ENABLED
  if(tsch_is_pan_secured) {
    p.fcf.security_enabled = 1;
    p.aux_hdr.security_control.security_level = TSCH_SECURITY_KEY_SEC_LEVEL_ACK;
    p.aux_hdr.security_control.key_id_mode = FRAME802154_1_BYTE_KEY_ID_MODE;
    p.aux_hdr.security_control.frame_counter_suppression = 1;
    p.aux_hdr.security_control.frame_counter_size = 1;
    p.aux_hdr.key_index = TSCH_SECURITY_KEY_INDEX_ACK;
  }
#endif /* LLSEC802154_ENABLED */

  if((curr_len = frame802154_create(&p, buf)) == 0) {
    return 0;
  }

  // add our candidate bitmask
  /* memcpy(&buf[curr_len], candidate_bitmask, sizeof(candidate_bitmask)); */
  /* curr_len = curr_len + sizeof(candidate_bitmask); */

  // add tx_timestamp
  memcpy(&buf[curr_len], &tx_timestamp, sizeof(uint64_t));
  curr_len = curr_len + sizeof(uint64_t);

  // iterate over list of measurements and add each timestamp to the packet
  struct mtm_rx_queue_item *t = NULL;

  // write amount of measurements
  uint8_t amount_of_measurements = list_length(rx_send_queue);

  memcpy(&buf[curr_len], &amount_of_measurements, sizeof(uint8_t));
  curr_len = curr_len + sizeof(uint8_t);

  for(t = list_head(rx_send_queue); t != NULL; t = list_item_next(t)) {
      memcpy(&buf[curr_len], &t->neighbor_addr, sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&buf[curr_len], &t->timeslot_offset, sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&buf[curr_len], &t->rx_timestamp, sizeof(uint64_t));
      curr_len = curr_len + sizeof(uint64_t);

      _PRINTF("put %d -> %u%u \n", t->neighbor_addr, (uint32_t)(t->rx_timestamp >> 32), (uint32_t)(t->rx_timestamp & 0xFFFFFFFF));
  }

  // free all allocated memory for all measurements again
  while((t = list_pop(rx_send_queue)) != NULL) {
      memb_free(&mtm_prop_rx_memb, t);
  }

  return curr_len;
}

int
tsch_packet_parse_multiranging_packet(
    uint8_t *buf,
    int buf_size,
    uint8_t timeslot_offset,
    frame802154_t *frame,
    // timestamps
    uint64_t *timestamp_tx_B,
    struct mtm_packet_timestamp **rx_timestamps,
    uint8_t *num_timestamps
     )
{
  uint8_t curr_len = 0;
  int ret;
  linkaddr_t dest, src;


  if(frame == NULL || buf_size < 0) {
    return 0;
  }

  /* Parse 802.15.4-2006 frame, i.e. all fields before Information Elements */
  if((ret = frame802154_parse((uint8_t *)buf, buf_size, frame)) < 3) {
    return 0;
  }

  curr_len += ret;

  // printf("frame payload len %d\n", frame->payload_len);
  /* Check seqno */
  /* if(seqno != frame->seq) { */
  /*   return 0; */
  /* } */

  /* Check destination PAN ID */
  if(frame802154_check_dest_panid(frame) == 0) {
    return 0;
  }

  /* Check destination address (if any) */
  if(frame802154_extract_linkaddr(frame, &src, &dest) == 0 ||
     (!linkaddr_cmp(&dest, &linkaddr_node_addr)
      && !linkaddr_cmp(&dest, &linkaddr_null))) {
    return 0;
  }

  mtm_direct_observed_node( src.u8[LINKADDR_SIZE-1], timeslot_offset);
  
  /* curr_len = curr_len + sizeof(candidate_bitmask); */

  // if everything is okay we will extract the timestamps

  // extract timestamps
  uint64_t tx_timestamp;
  // extract rx timestamp amount
  uint8_t amount_of_measurements;
  uint8_t mtm_found_our_own;
  memcpy(&tx_timestamp, &buf[curr_len], sizeof(uint64_t));
  curr_len = curr_len + sizeof(uint64_t);
  memcpy(&amount_of_measurements, &buf[curr_len], sizeof(uint8_t));
  curr_len = curr_len + sizeof(uint8_t);
  for(int i = 0; i < amount_of_measurements; i++) {
      ranging_addr_t neighbor;
      uint8_t timeslot_offset;
      uint64_t rx_timestamp;

      memcpy(&neighbor, &buf[curr_len], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&timeslot_offset, &buf[curr_len], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);      
      memcpy(&rx_timestamp, &buf[curr_len], sizeof(uint64_t));
      curr_len = curr_len + sizeof(uint64_t);

      // update two_hop counter
      if(neighbor != linkaddr_node_addr.u8[LINKADDR_SIZE-1]) {
          mtm_indirect_observed_node(neighbor, timeslot_offset);
      } else {
          mtm_found_our_own = 1;
      }
      
      return_timestamps[i].addr = neighbor;
      return_timestamps[i].rx_timestamp = rx_timestamp;
  }

  if(mtm_current_node_role == MTM_ACTIVE && !mtm_found_our_own) {
      mtm_active_missed_observations++;

  }

  *num_timestamps = amount_of_measurements;
  *rx_timestamps = return_timestamps;
  *timestamp_tx_B = tx_timestamp;
  
  return curr_len;
}


int32_t fixed_precision_divide(int32_t a, int32_t b) {
    int64_t tmp = (int64_t)a << 32;
    return (int32_t)(tmp / b);
}

int32_t mtm_compute_tdoa(struct mtm_pas_tdoa *ts) {
    int64_t M_a, M_b, R_a, D_a, R_b, D_b, ToF_ab, TD;
    
    // we are in the lucky position that we have a fpu, so we will use them instead of fixed precision.
    double prop_drift_coff;
    
    // extract intervals from Message
    M_a = interval_correct_overflow( ts->r_l2 , ts->r_l1 );
    M_b = interval_correct_overflow( ts->r_l3 , ts->r_l2 );

    R_a = interval_correct_overflow(ts->ds_ts.r_a1 , ts->ds_ts.t_a1);
    D_a = interval_correct_overflow(ts->ds_ts.t_a2 , ts->ds_ts.r_a1);
    R_b = interval_correct_overflow(ts->ds_ts.r_b2 , ts->ds_ts.t_b1);
    D_b = interval_correct_overflow(ts->ds_ts.t_b1 , ts->ds_ts.r_b1);

    /* printf("M_a " LOG_LLU_MARK " M_b " LOG_LLU_MARK " R_a " LOG_LLU_MARK */
    /*        " D_a " LOG_LLU_MARK "\n", */
    /*        LOG_LLU(M_a), LOG_LLU(M_b), LOG_LLU(R_a), LOG_LLU(D_a)); */

    // M_a + M_b and R_a 
    prop_drift_coff = (double)((int64_t)M_a + M_b) / (double)((int64_t)R_a + D_a);

    ToF_ab = compute_prop_time(R_a, D_a, R_b, D_b);

    _PRINTF("prop_drift_coff " NRF_LOG_FLOAT_MARKER " ToF_ab %d \n", NRF_LOG_FLOAT((float)prop_drift_coff),
           ToF_ab);

    TD = prop_drift_coff * (R_a - ToF_ab) - M_a;

    // for loging convert TD to single 32 bit int, it shouuld fit easily if the calculations are right
    _PRINTF("TD %d\n", (int32_t) TD);
    
    return (int32_t) TD;
}

float time_to_dist(int32_t tof) {
    return (float)tof * SPEED_OF_LIGHT_M_PER_UWB_TU;
}

PROCESS(TSCH_MTM_PROCESS, "TSCH propagation time process");
  /*---------------------------------------------------------------------------*/
  /* Protothread for slot operation, called by update_neighbor_prop_time()
   * function. "data" is a struct tsch_neighbor pointer.*/
  PROCESS_THREAD(TSCH_MTM_PROCESS, ev, data)
  {
    PROCESS_BEGIN();

    while(1) {
      PROCESS_WAIT_EVENT();
      if(ev == PROCESS_EVENT_MSG) {
        printf("New prop time %ld %u %lu %u\n",
          ((struct tsch_neighbor *) data)->last_prop_time.prop_time,
          ((struct tsch_neighbor *) data)->last_prop_time.asn.ms1b, /* most significant 1 byte */
          ((struct tsch_neighbor *) data)->last_prop_time.asn.ls4b, /* least significant 4 bytes */
          ((struct tsch_neighbor *) data)->last_prop_time.tsch_channel);
      }
    }

    PROCESS_END();
  }

#endif

#ifndef TSCH_LOC_THREAD
  PROCESS(TSCH_PROP_PROCESS, "TSCH propagation time process");

  /*---------------------------------------------------------------------------*/
  /* Protothread for slot operation, called by update_neighbor_prop_time()
   * function. "data" is a struct tsch_neighbor pointer.*/
  PROCESS_THREAD(TSCH_PROP_PROCESS, ev, data)
  {
    PROCESS_BEGIN();

    // PROCESS_PAUSE();

    printf("tsch_loc_operation start\n");

    static int32_t bias_correction; // in centimeters?
    static float range;
    static struct tsch_neighbor *n;

    while(1) {
      PROCESS_WAIT_EVENT();
      // printf("Got event number %d\n", ev);
      if(ev == PROCESS_EVENT_MSG) {
          n = (struct tsch_neighbor *) data;

          range = tof_to_dist(n->last_prop_time.prop_time);
          // bias correction in centimeters
          bias_correction = dw1000_getrangebias(n->last_prop_time.tsch_channel, range);
          printf("bias corrected range: " NRF_LOG_FLOAT_MARKER "\n", NRF_LOG_FLOAT( range - (float) bias_correction * 0.01));

          printf("New prop time %ld %u %lu %u \r\n",
              n->last_prop_time.prop_time,
              n->last_prop_time.asn.ms1b, /* most significant 1 byte */
              n->last_prop_time.asn.ls4b, /* least significant 4 bytes */
              n->last_prop_time.tsch_channel);
      }
    }

    PROCESS_END();
  }
#endif /* TSCH_LOC_THREAD */

/*---------------------------------------------------------------------------*/
/* Update the propagation time between the node and his neighbor */
void
update_neighbor_prop_time(struct tsch_neighbor *n, int32_t prop_time,
                          struct tsch_asn_t * asn, uint8_t tsch_channel)
{
  struct tsch_prop_time n_prop_time;

  printf("updating neighbor with addr %u prop time %d\n", n->addr.u8[LINKADDR_SIZE-1], prop_time);

  n_prop_time.prop_time = prop_time;
  n_prop_time.asn = * asn; /* Copy the 5 bytes pointed by the pointer * asn to the n_prop_time.asn struct */
  n_prop_time.tsch_channel = tsch_channel;
  n->last_prop_time = n_prop_time;

  /* Send the PROCESS_EVENT_MSG event asynchronously to
  "tsch_loc_operation", with a pointer to the tsch_neighbor. */
  /* process_post(&TSCH_PROP_PROCESS, */
  /*               PROCESS_EVENT_MSG, (void *) n); */
}


/* float calculate_propagation_time_alternative(struct ds_twr_ts *ts) { */
/*     static float relative_drift_offset; */
/*     static uint64_t other_duration, own_duration; */
/*     static uint64_t round_duration_b, delay_duration_a; */
/*     static int64_t drift_offset_int, two_tof_int; */

/*     other_duration = ts->txB2 - ts->txB1; */
/*     own_duration = ts->rxA2 - ts->rxA1; */

/*     relative_drift_offset = (float) ((int64_t)own_duration - (int64_t) other_duration) / (float) other_duration; */

/*     round_duration_b = ts->rxB1 - ts->txB1; */
/*     delay_duration_a = ts->txA1 - ts->rxA1; */

/*     drift_offset_int = relative_drift_offset * (float) round_duration_b - relative_drift_offset * (float) delay_duration_a; */
/*     two_tof_int = (int64_t)round_duration_b - (int64_t)delay_duration_a + drift_offset_int; */

/*     return ((float)two_tof_int) * 0.5; */
/* } */

int32_t calculate_propagation_time_alternative(struct ds_twr_ts *ts) {
    int64_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply;
    
    initiator_roundtrip = interval_correct_overflow(ts->r_a1,  ts->t_a1);
    initiator_reply     = interval_correct_overflow(ts->t_a2,  ts->r_a1);
    replier_roundtrip   = interval_correct_overflow(ts->r_b2,  ts->t_b1);
    replier_reply       = interval_correct_overflow(ts->t_b1,  ts->r_b1);

    return (int32_t)(( ((int64_t) initiator_roundtrip * replier_roundtrip)
            - ((int64_t) initiator_reply * replier_reply))
        /
        ((int64_t) initiator_roundtrip
            +  replier_roundtrip
            +  initiator_reply
            +  replier_reply));
}

/*---------------------------------------------------------------------------*/
/**
 * Compute the propagation time using the Asymmetrical approach of Decawave
 * We use signed number to give the possibilities to have negative
 * propagation time in the case that the antenna delay was to hight
 * when we calibrate the nodes.
 *
 **/
int32_t
compute_prop_time(int32_t initiator_roundtrip, int32_t initiator_reply,
  int32_t replier_roundtrip, int32_t replier_reply) {

  /* printf("initiator_roundtrip: %d\n", initiator_roundtrip); */
  /* printf("initiator_reply: %d\n", initiator_reply); */
  /* printf("replier_roundtrip: %d\n", replier_roundtrip); */
  /* printf("replier_reply: %d\n", replier_reply); */

  return (int32_t)(( ((int64_t) initiator_roundtrip * replier_roundtrip)
                  - ((int64_t) initiator_reply * replier_reply))
                /
((int64_t) initiator_roundtrip
                  +  replier_roundtrip
                  +  initiator_reply
                  +  replier_reply));
}
