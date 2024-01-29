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
#include "math.h"
#include "nrfx_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_stdint.h>

#include "dw1000-arch.h"
#include "dw1000-driver.h"
#include "dw1000-ranging-bias.h"
#include "deca_regs.h"
#include "deca_device_api.h"
#include "tsch-slot-operation.h"
#include "uart0.h"

#if TSCH_MTM_LOCALISATION

#define TMP_DEBUG_MTM_LOCALISATION 0
#if TMP_DEBUG_MTM_LOCALISATION
#define _PRINTF(...) printf(__VA_ARGS__)
#else
#define _PRINTF(...)
#endif

#define SPEED_OF_LIGHT_M_PER_S 299702547.236
#define SPEED_OF_LIGHT_M_PER_UWB_TU ((SPEED_OF_LIGHT_M_PER_S * 1.0E-15) * 15650.0) // around 0.00469175196

#if !defined(WITH_MTM_TDOA_REPLACE_AFTER_TIMEOUT) && !defined(WITH_MTM_TDOA_REPLACE_OLDEST)
#define WITH_MTM_TDOA_REPLACE_OLDEST 1
#endif


#if WITH_PASSIVE_TDOA
// this structure tracks timestamps for a passive tdoa measurement to
// two neighbors A and B. Hereby we virtually identify A as being in the initiator role
// and B taking the responder hole. See paper for more details on this measurement method.
// with L we denote ourself as the passive listener.

float mtm_compute_tdoa(struct mtm_pas_tdoa *ts);
#endif

int32_t correctedExpression(struct ds_twr_ts *ts);
void mtm_compute_dstwr(struct tsch_asn_t *asn, struct mtm_neighbor *mtm_n);
float calculate_propagation_time_alternative(struct ds_twr_ts *ts);

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

MEMB(mtm_prop_neighbor_memb, struct mtm_neighbor, TSCH_MTM_PROP_MAX_NEIGHBOR_ENTRIES);
MEMB(mtm_prop_rx_memb, struct mtm_rx_queue_item, TSCH_MTM_PROP_MAX_MEASUREMENT);

#if WITH_PASSIVE_TDOA
MEMB(mtm_pas_tdoa_memb, struct mtm_pas_tdoa, TSCH_MTM_MAX_TDOA_ENTRIES);
#endif

static uint64_t most_recent_tx_timestamp;
static uint32_t round_counter = 0;
static uint8_t our_tx_timeslot;
static uint8_t mtm_round_begin, mtm_round_end;
uint32_t mtm_successfull_frame_receptions, mtm_sucessfull_rounds, mtm_failed_frame_receptions, mtm_no_frame_detected;

static enum MTM_SLOT_END_TYPE slot_end_type;

#if WITH_MTM_BUS_BOARDING
static uint8_t bus_boarding_begin = 0; // will be constantly update to mark current timeslot from which to save timestamps


static uint8_t bus_boarding_timeslot_active(uint8_t timeslot);
static void bus_boarding_update();
#endif

// this will be used to return a array of timestamps that we read from the user
static struct mtm_packet_timestamp return_timestamps[TSCH_MTM_PROP_MAX_NEIGHBORS];

list_t tsch_prop_get_neighbor_list() {
    return ranging_neighbor_list;
}

#if WITH_PASSIVE_TDOA
list_t tsch_prop_get_tdoa_list() {
    return pas_tdoa_list;
}
#endif

void mtm_reset_rx_queue() {
    // free all elements in queue
    struct mtm_rx_queue_item *item;
    while((item = list_pop(rx_send_queue)) != NULL) {
        memb_free(&mtm_prop_rx_memb, item);
    }

    most_recent_tx_timestamp = UINT64_MAX;
}

void mtm_reset() {
    // clear all tables
    mtm_reset_rx_queue();

#if WITH_PASSIVE_TDOA
    struct mtm_pas_tdoa *item;
    while((item = list_pop(pas_tdoa_list)) != NULL) {
        memb_free(&mtm_pas_tdoa_memb, item);
    }
#endif

    struct mtm_neighbor *n;
    while((n = list_pop(ranging_neighbor_list)) != NULL) {
        memb_free(&mtm_prop_neighbor_memb, n);
    }

    mtm_successfull_frame_receptions = 0;
    mtm_failed_frame_receptions = 0;
    mtm_no_frame_detected = 0;
    mtm_sucessfull_rounds = 0;

#if WITH_MTM_BUS_BOARDING
    bus_boarding_begin = 0;
#endif
}

// protos
#if WITH_PASSIVE_TDOA
void print_tdoa_timestamps(struct mtm_pas_tdoa *ts);
#endif

void debug_output_ds_twr_timestamps(struct ds_twr_ts *ts, ranging_addr_t neighbor_addr);
void print_mtm_neighbors();
void notify_user_process_new_measurement(struct distance_measurement *measurement);

// for all timestamps check overflow condition. The internal clock of the decawave transceiver will overflow roughly every 17 seconds
// therefore sometimes a timestamp which should be bigger, will be smaller than a preceding one. DW_TIMESTAMP_MAX_VALUE
// this function expects that the user passes in high_ts the timestamp that is supposed to be the larger value of
// the given interval.
static inline int64_t interval_correct_overflow(uint64_t high_ts, uint64_t low_ts) {

    /* return (uint64_t)(high_ts - low_ts)&0xFFFFFFFFFF;  */
// cast numbers to int
    int64_t high_ts_i = (int64_t) high_ts;
    int64_t low_ts_i = (int64_t) low_ts;
    if (high_ts_i < low_ts_i) {
        _PRINTF("interval_correct_overflow: high_ts < low_ts\n");
        return DW_TIMESTAMP_MAX_VALUE - low_ts_i + high_ts_i + 1;
    }

    return high_ts_i - low_ts_i;
}

void set_mtm_tx_slot(uint8_t timeslot) {
    our_tx_timeslot = timeslot;
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
    tdoa->closed_round = 0;
    init_ds_twr_struct(&tdoa->ds_ts);
}

int pas_tdoa_all_initialized(struct mtm_pas_tdoa *tdoa) {
    if (tdoa->r_l1 == UINT64_MAX) return 0;
    if (tdoa->r_l2 == UINT64_MAX) return 0;
    if (tdoa->r_l3 == UINT64_MAX) return 0;

    if (tdoa->ds_ts.t_a1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.r_b1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.t_b1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.r_a1 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.t_a2 == UINT64_MAX) return 0;
    if (tdoa->ds_ts.r_b2 == UINT64_MAX) return 0;

    return 1;
}

#endif

void print_mtm_neighbors() {
    _PRINTF("-------MTM NEIGHBORS-------\n");
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        _PRINTF("%d, type: %d, timeslot %d, missed_observations: %d \n", n->neighbor_addr, n->type, n->timeslot_offset, n->missed_observations);
    }
}

void mtm_indirect_observed_node(ranging_addr_t neighbor, uint8_t timeslot_offset) {
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (n->neighbor_addr == neighbor) {
            break;
        }
    }

    if (n == NULL) {
        // create new neighbor
        n = memb_alloc(&mtm_prop_neighbor_memb);
        if (n != NULL) {
            n->neighbor_addr = neighbor;
            n->type = MTM_TWO_HOP_NEIGHBOR;
            list_add(ranging_neighbor_list, n);
        }
    }

    if(n != NULL) { // this check is necessary as memory allocation might fail
        n->last_observed_indirect = clock_time();
        n->observed_timeslot = timeslot_offset;
    }
}

void mtm_direct_observed_node(ranging_addr_t node, uint8_t timeslot_offset) {
    /* printf("mtm_direct_observed_node: %d, %d\n", node, timeslot_offset); */
  // we don't pass this outside so we will just add it here for now
  // iterate over neighbor table
  struct mtm_neighbor *n = NULL;
  for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
      if (n->neighbor_addr == node) {
          break;
      }
  }

  if(n == NULL) {
        // create new neighbor
      n = memb_alloc(&mtm_prop_neighbor_memb);
      if (n != NULL) {
          list_add(ranging_neighbor_list, n);
      } else {
          // in this case we don't have any more memory, so we replace the oldest existing entry
          struct mtm_neighbor *oldest = NULL;
          struct mtm_neighbor *curr = NULL;
          for(curr = list_head(ranging_neighbor_list); curr != NULL; curr = list_item_next(curr)) {
              if (oldest == NULL) {
                  oldest = curr;
              } else {
                  if (curr->last_observed_direct < oldest->last_observed_direct) {
                      oldest = curr;
                  }
              }
          }

          // remove all pass tdoa entries with n as address
#if WITH_PASSIVE_TDOA
          struct mtm_pas_tdoa *tdoa = NULL;
            for(tdoa = list_head(pas_tdoa_list); tdoa != NULL; tdoa = list_item_next(tdoa)) {
                if (tdoa->A_addr == oldest->neighbor_addr || tdoa->B_addr == oldest->neighbor_addr) {
                    list_remove(pas_tdoa_list, tdoa);
                    memb_free(&mtm_pas_tdoa_memb, tdoa);
                }
            }
#endif

          n = oldest;
      }

      n->total_found_ours_counter = 0;
      init_ds_twr_struct(&n->ts);
  }

  if (n != NULL) {
      // set neighbor type to direct neighbor
      n->neighbor_addr = node;
      n->type = MTM_DIRECT_NEIGHBOR;
      n->observed_timeslot = timeslot_offset;
      n->last_observed_direct = clock_time();
  }
}

// Our implementation assumes there is just one ranging round per slotframe. Therefore
// we simply update the round counter at the end of the slotframe. We might want to change
// this is the future, as it might be useful to have many ranging rounds during a slotframe.
void mtm_slot_end_handler(uint16_t timeslot) {
    slot_end_type = MTM_SLOT_END;
    if(timeslot == mtm_round_end) {
        round_counter++;
#if WITH_MTM_BUS_BOARDING
        bus_boarding_update();
#endif
        slot_end_type = MTM_ROUND_END;
    }


#if WITH_MTM_SLOT_END_PROCESS
        process_post(&TSCH_MTM_SLOT_END_PROCESS, PROCESS_EVENT_MSG, &slot_end_type);
#endif

    uint32_t status = dwt_read32bitreg(SYS_STATUS_ID);
    if(status & SYS_STATUS_ALL_RX_TO || status & SYS_STATUS_ALL_RX_ERR) {
        /* printf("rxtout\n"); */
        /* dwt_rxreset(); */
        /* dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_INIT_STATE); */
        /* dw_soft_reset(); */
        /* dwt_softreset(); */
        /* dwt_softreset(); */
        dwt_forcetrxoff(); // Turn the RX off
        dwt_rxreset(); // Reset in case we we
        /* dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_IDLE_STATE); */
    }

    if (status & SYS_STATUS_RFPLL_LL || status & SYS_STATUS_CLKPLL_LL) {
        /* dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_INIT_STATE); */
        /* dwt_softreset(); */
        /* dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_IDLE_STATE); */
        /* printf("pll\n"); */
    }

}


// We require that the upper-layer marks which timeslot marks the round end
// see above comment why this might not be the best idea
void mtm_set_round_slots(uint8_t timeslot_begin, uint8_t timeslot_end) {
    mtm_round_begin = timeslot_begin;
    mtm_round_end = timeslot_end;
    round_counter = 0;
}


void add_to_direct_observed_rx_to_queue(uint64_t rx_timestamp, uint8_t neighbor, uint8_t timeslot_offset) {
    // first update our own outgoing rx queue

#if WITH_MTM_BUS_BOARDING
    if (!bus_boarding_timeslot_active(timeslot_offset)) {
        return;
    }
#endif

    struct mtm_rx_queue_item *t = NULL;

    if (list_length (rx_send_queue) > TSCH_MTM_PROP_MAX_MEASUREMENT) {
        _PRINTF("MTM: RX queue full, dropping received timestamp\n");

        while(list_length(rx_send_queue) > TSCH_MTM_PROP_MAX_MEASUREMENT) {
            t = list_pop(rx_send_queue);
            memb_free(&mtm_prop_rx_memb, t);
        }
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
        t->timeslot_offset = timeslot_offset;
    } else {
        t = memb_alloc(&mtm_prop_rx_memb);

        if (t != NULL) {
            t->rx_timestamp = rx_timestamp;
            t->neighbor_addr = neighbor;
            t->timeslot_offset = timeslot_offset;
            list_add(rx_send_queue, t);
        } else {
            _PRINTF("MTM: Could not allocate memory for reception timestamp\n");

            return;
        }
    }
}

void add_mtm_reception_timestamp(
    ranging_addr_t neighbor_addr,
    struct tsch_asn_t *asn, // TODO not used yet
    uint8_t timeslot,
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

    // will usually not happen
    if( n == NULL ) {
        return;
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
            _PRINTF("MTM: No pas_tdoa found for %u and %u, create new entry\n", rx_addr, m_addr);
            // create a new entry
            pas_tdoa = memb_alloc(&mtm_pas_tdoa_memb);
            if (pas_tdoa != NULL) {
                list_add(pas_tdoa_list, pas_tdoa);
            }
#if WITH_MTM_TDOA_REPLACE_OLDEST
            else {
                // else we replace the oldest entry

                struct mtm_pas_tdoa *oldest = NULL;
                struct mtm_pas_tdoa *curr = NULL;
                for(curr = list_head(pas_tdoa_list); curr != NULL; curr = list_item_next(curr)) {
                    if (oldest == NULL) {
                        oldest = curr;
                    } else {
                        if (curr->last_observed < oldest->last_observed) {
                            oldest = curr;
                        }
                    }
                }

                pas_tdoa = oldest;
            }
#elif WITH_MTM_TDOA_REPLACE_AFTER_TIMEOUT // only replace entries that are older than some threshold T_remove, otherwise keep entries
            else {
                clock_time_t now = clock_time();
                struct mtm_pas_tdoa *curr = NULL;
                for(curr = list_head(pas_tdoa_list); curr != NULL; curr = list_item_next(curr)) {
                    if(now - curr->last_observed > CLOCK_SECOND*15) {
                        pas_tdoa = curr;
                    }
                }
            }
#endif
            if(pas_tdoa != NULL) {
                pas_tdoa->B_addr = rx_addr;
                pas_tdoa->A_addr = m_addr;
                init_tdoa_struct(pas_tdoa);
            }
        }


        // here we do the handling in case we found A in the list as initator
        if(pas_tdoa != NULL) {
            if (pas_tdoa->A_addr == m_addr) {
                pas_tdoa->most_recent_tx_A = tx_timestamp_B;
                pas_tdoa->rx_round_counter_L1 = round_counter;
                pas_tdoa->rx_timeslot_L1 = timeslot;
                pas_tdoa->rx_timestamp_L1 = rx_timestamp_A;

                // check whether the other round concluded
                if (   (timeslot > pas_tdoa->rx_timeslot_L2 && round_counter == pas_tdoa->rx_round_counter_L2    )
                    || (timeslot < pas_tdoa->rx_timeslot_L2 && round_counter == pas_tdoa->rx_round_counter_L2 + 1)) {

                    pas_tdoa->r_l4 = pas_tdoa->rx_timestamp_L2;
                    pas_tdoa->ds_ts.t_b2 = pas_tdoa->most_recent_tx_B;
                    pas_tdoa->ds_ts.r_a2 = rx_timestamp;

                    // TODO we could in theory make another new distance estimation here, but we need to rewrite mtm_compute_tdoa
                    // so the timestamps for calculation are passed from outside

                    // push back the two rounds
                    pas_tdoa->r_l1 = pas_tdoa->r_l3;
                    pas_tdoa->r_l2 = pas_tdoa->r_l4;

                    pas_tdoa->ds_ts.t_b1 = pas_tdoa->ds_ts.t_b2;
                    pas_tdoa->ds_ts.t_a1 = pas_tdoa->ds_ts.t_a2;

                    pas_tdoa->ds_ts.r_b1 = pas_tdoa->ds_ts.r_b2;
                    pas_tdoa->ds_ts.r_a1 = pas_tdoa->ds_ts.r_a2;

                    // mark moved uninitialized
                    pas_tdoa->r_l3 = UINT64_MAX;
                    pas_tdoa->r_l4 = UINT64_MAX;
                    pas_tdoa->ds_ts.t_b2 = UINT64_MAX;
                    pas_tdoa->ds_ts.t_a2 = UINT64_MAX;
                    pas_tdoa->ds_ts.r_b2 = UINT64_MAX;
                    pas_tdoa->ds_ts.r_a2 = UINT64_MAX;

                    pas_tdoa->closed_round = 0;
                }
            } else if (pas_tdoa->B_addr == m_addr) {
                pas_tdoa->most_recent_tx_B = tx_timestamp_B;
                pas_tdoa->rx_round_counter_L2 = round_counter;
                pas_tdoa->rx_timeslot_L2 = timeslot;
                pas_tdoa->rx_timestamp_L2 = rx_timestamp_A;

                // check whether the round and timeslot is as expected. In case that timeslot >
                // rx_timeslot, we expect to be in the same round otherwise our round should be atmost
                // rx_timeslot + 1

                if (!pas_tdoa->closed_round && (  (timeslot > pas_tdoa->rx_timeslot_L1 && round_counter == pas_tdoa->rx_round_counter_L1    )
                        || (timeslot < pas_tdoa->rx_timeslot_L1 && round_counter == pas_tdoa->rx_round_counter_L1 + 1))) {
                    // Success! The round closes succesfully.
                    pas_tdoa->r_l3 = pas_tdoa->rx_timestamp_L1;
                    pas_tdoa->ds_ts.t_a2 = pas_tdoa->most_recent_tx_A;
                    pas_tdoa->ds_ts.r_b2 = rx_timestamp;

                    pas_tdoa->closed_round = 1; // we only want good rounds

                    pas_tdoa->last_observed = clock_time();

                    if(pas_tdoa_all_initialized(pas_tdoa)) {
                        // this message closes the round
                        float tdoa;

                        tdoa = mtm_compute_tdoa(pas_tdoa);

                        // update stored most recent measurement and notify user
                        pas_tdoa->last_measurement.type = TDOA;
                        pas_tdoa->last_measurement.addr_A = rx_addr;
                        pas_tdoa->last_measurement.addr_B = m_addr;
                        pas_tdoa->last_measurement.time = tdoa;
                        pas_tdoa->last_measurement.asn = *asn;

                        /* #define ERROR_MARGIN 5 */
                        /* if (time_to_dist(tdoa) < -ERROR_MARGIN || time_to_dist(tdoa) > ERROR_MARGIN) { */
                        /*     print_tdoa_timestamps(pas_tdoa); */
                        /* } */


                        notify_user_process_new_measurement(&pas_tdoa->last_measurement);
                    }
                }
            }
        }
    }

    #endif

    // next we extract information for two way ranging
    for(uint8_t i = 0; i < num_rx_timestamps; i++) {
        if(rx_timestamps[i].addr == linkaddr_node_addr.u8[LINKADDR_SIZE - 1]) {
                uint64_t rx_timestamp_B = rx_timestamps[i].rx_timestamp;
                // we got a return timestamp, so insert new round into our management structure
                // shift old most recent round to the back
                n->ts.t_a1 = n->ts.t_a2;
                n->ts.r_b1 = n->ts.r_b2;
                n->ts.t_b1 = n->ts.t_b2;
                n->ts.r_a1 = n->ts.r_a2;

                // insert new round that just concluded
                n->ts.t_a2 = most_recent_tx_timestamp;
                n->ts.r_b2 = rx_timestamp_B;
                n->ts.t_b2 = tx_timestamp_B;
                n->ts.r_a2 = rx_timestamp_A;

                mtm_compute_dstwr(asn, n);
                n->total_found_ours_counter++; // yippie
                mtm_sucessfull_rounds++;
        }
    }
}

void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp) {
    most_recent_tx_timestamp = tx_timestamp;
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

    printf("MTM: Initiator roundtrip: %" PRId64 "\n", initiator_roundtrip);
    printf("MTM: Initiator reply: %" PRId64 "\n", initiator_reply);
    printf("MTM: Replier roundtrip: %" PRId64 "\n", replier_roundtrip);
    printf("MTM: Replier reply: %" PRId64 "\n", replier_reply);
}

void debug_output_ds_twr_timestamps(struct ds_twr_ts *ts, ranging_addr_t neighbor_addr) {
    printf("tstx1, ");
    /* uint64_t t_a1, r_b1, t_b1, r_a1, t_a2, r_b2, t_b2, r_a2;     */
    printf("%" PRIu8 ", ", neighbor_addr);

    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->t_a1 >> 32), (uint32_t)(ts->t_a1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->r_b1 >> 32), (uint32_t)(ts->r_b1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->t_b1 >> 32), (uint32_t)(ts->t_b1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->r_a1 >> 32), (uint32_t)(ts->r_a1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->t_a2 >> 32), (uint32_t)(ts->t_a2));
    printf("%" PRIu32 ":%" PRIu32 " ", (uint32_t)(ts->r_b2 >> 32), (uint32_t)(ts->r_b2));

    printf("\n");
}


// WARNING might run into timing problems when using standard serial output. Use
// RTT if available.
#if WITH_PASSIVE_TDOA
void print_tdoa_timestamps(struct mtm_pas_tdoa *ts) {
    printf("tdoa, ");

    // Assuming the format of printing the timestamps is similar to the debug_output_ds_twr_timestamps function
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.t_a1 >> 32), (uint32_t)(ts->ds_ts.t_a1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.r_b1 >> 32), (uint32_t)(ts->ds_ts.r_b1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.t_b1 >> 32), (uint32_t)(ts->ds_ts.t_b1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.r_a1 >> 32), (uint32_t)(ts->ds_ts.r_a1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.t_a2 >> 32), (uint32_t)(ts->ds_ts.t_a2));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.r_b2 >> 32), (uint32_t)(ts->ds_ts.r_b2));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.t_b2 >> 32), (uint32_t)(ts->ds_ts.t_b2));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->ds_ts.r_a2 >> 32), (uint32_t)(ts->ds_ts.r_a2));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->r_l1 >> 32), (uint32_t)(ts->r_l1));
    printf("%" PRIu32 ":%" PRIu32 ", ", (uint32_t)(ts->r_l2 >> 32), (uint32_t)(ts->r_l2));
    printf("%" PRIu32 ":%" PRIu32 " ", (uint32_t)(ts->r_l3 >> 32), (uint32_t)(ts->r_l3));

    printf("\n");
}
#endif

void notify_user_process_new_measurement(struct distance_measurement *measurement) {
  /* Send the PROCESS_EVENT_MSG event asynchronously to
  "tsch_loc_operation", with a pointer to the tsch_neighbor. */
  process_post(&TSCH_PROP_PROCESS,
      PROCESS_EVENT_MSG, (void *) measurement);
}


// In the UWB Swarm Ranging Paper something called a Bus Boarding Scheme is proposed
#if WITH_MTM_BUS_BOARDING
// on end of current transmission call this function in order to update indice, from which receptions will be kept
void bus_boarding_update() {
    if(mtm_round_end - mtm_round_begin + 1 <= TSCH_MTM_PROP_MAX_MEASUREMENT) {
        bus_boarding_begin = 0;
    } else {
    // Shift the beginning of the active window by TSCH_MTM_PROP_MAX_MEASUREMENT and wrap around if necessary
        bus_boarding_begin = (bus_boarding_begin + TSCH_MTM_PROP_MAX_MEASUREMENT) % (mtm_round_end - mtm_round_begin + 1);
    }
}

uint8_t bus_boarding_timeslot_active(uint8_t timeslot) {

    if(mtm_round_end - mtm_round_begin + 1 <= TSCH_MTM_PROP_MAX_MEASUREMENT) {
        return 1;
    } else if (timeslot < mtm_round_begin || timeslot > mtm_round_end) {
        return 0;
    }

    uint8_t bus_boarding_end = (bus_boarding_begin + TSCH_MTM_PROP_MAX_MEASUREMENT - 1) % (mtm_round_end - mtm_round_begin + 1);

    // Check if the timeslot is within the active window
    if (bus_boarding_end > bus_boarding_begin) {
        return (timeslot - mtm_round_begin) >= bus_boarding_begin && (timeslot - mtm_round_begin) <= bus_boarding_end;
    } else {
        // This handles the wrap-around case where the window spans from end to beginning
        return (timeslot - mtm_round_begin) >= bus_boarding_begin || (timeslot - mtm_round_begin) <= bus_boarding_end;
    }
}


#endif

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
        _PRINTF("MTM: Neighbor has uninitialized timestamps\n");

        return;
    }

    /* int32_t prop_time  = compute_prop_time(initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply); */
    float prop_time  = calculate_propagation_time_alternative(&(mtm_n->ts));
    /* float prop_time  = (float) correctedExpression(&(mtm_n->ts)); */
    int32_t carrier_integrator = dwt_readcarrierintegrator();

#if MTM_EVAL_OUTPUT_TS
    debug_output_ds_twr_timestamps(&mtm_n->ts, mtm_n->neighbor_addr);
#endif

    // bias correction in centimeters
    /* bias_correction = dw1000_getrangebias(n->last_prop_time.tsch_channel, range); */
    _PRINTF("bias uncorrected range to %u: " NRF_LOG_FLOAT_MARKER "\n", mtm_n->neighbor_addr, NRF_LOG_FLOAT(range));
    _PRINTF("prop time to %u: %d\n", mtm_n->neighbor_addr, prop_time);
    // call into existing methods for passing data to user

    // update stored measurement for node
    mtm_n->last_measurement.type = TWR;
    mtm_n->last_measurement.addr_A = linkaddr_node_addr.u8[LINKADDR_SIZE-1];
    mtm_n->last_measurement.addr_B = mtm_n->neighbor_addr;
    mtm_n->last_measurement.time = prop_time;
    mtm_n->last_measurement.freq_offset = carrier_integrator;
    mtm_n->last_measurement.asn = *asn;

    notify_user_process_new_measurement(&mtm_n->last_measurement);
}


void packet_buf_copy_timestamp(uint64_t timestamp, uint8_t *buffer) {
    // Ensure the input buffer is not NULL
    if (!buffer) return;

    for (int i = 0; i < 5; ++i) {
        buffer[i] = (timestamp >> (8 * i)) & 0xFF;
    }
}

void packet_buf_extract_timestamp(uint64_t *timestamp, uint8_t *buffer) {
    // Ensure the input buffer is not NULL
    if (!buffer) return;

    *timestamp = 0;
    for (int i = 0; i < 5; ++i) {
        *timestamp |= ((uint64_t) buffer[i]) << (8 * i);
    }
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

  // add tx_timestamp
  packet_buf_copy_timestamp(tx_timestamp, &buf[curr_len]);
  curr_len = curr_len + 5 * sizeof(uint8_t);

  // iterate over list of measurements and add each timestamp to the packet
  struct mtm_rx_queue_item *t = NULL;

  // write amount of measurements
  uint8_t amount_of_measurements = list_length(rx_send_queue);

  /* printf("s %d\n", amount_of_measurements); */

  memcpy(&buf[curr_len], &amount_of_measurements, sizeof(uint8_t));
  curr_len = curr_len + sizeof(uint8_t);

  // free all allocated memory for all measurements again
  while((t = list_pop(rx_send_queue)) != NULL) {
      memcpy(&buf[curr_len], &t->neighbor_addr, sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&buf[curr_len], &t->timeslot_offset, sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);

      packet_buf_copy_timestamp(t->rx_timestamp, &buf[curr_len]);
      curr_len = curr_len + 5*sizeof(uint8_t);

      memb_free(&mtm_prop_rx_memb, t);
  }


  uint32_t checksum = 0;
  // calculate checksum for buf
  for(int i = 0; i < curr_len; i++) {
      checksum += buf[i];
  }

  // append checksum to end
  memcpy(&buf[curr_len], &checksum, sizeof(uint32_t));
  curr_len = curr_len + sizeof(uint32_t);

#if WITH_DEV_FILL_MAC_PACKET
  return 120;
#endif


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

  mtm_direct_observed_node( src.u8[LINKADDR_SIZE-1], timeslot_offset );

  /* curr_len = curr_len + sizeof(candidate_bitmask); */

  // if everything is okay we will extract the timestamps

  // extract timestamps
  uint64_t tx_timestamp = 0;
  // extract rx timestamp amount
  uint8_t amount_of_measurements = 0;

  packet_buf_extract_timestamp(&tx_timestamp, &buf[curr_len]);
  curr_len = curr_len + 5 * sizeof(uint8_t);

  memcpy(&amount_of_measurements, &buf[curr_len], sizeof(uint8_t));

  /* printf("am, %d\n", amount_of_measurements); */

  curr_len = curr_len + sizeof(uint8_t);
  for(int i = 0; i < amount_of_measurements; i++) {
      ranging_addr_t neighbor;
      uint8_t timeslot_offset = 0;
      uint64_t rx_timestamp = 0;

      memcpy(&neighbor, &buf[curr_len], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);

      memcpy(&timeslot_offset, &buf[curr_len], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);

      packet_buf_extract_timestamp(&rx_timestamp, &buf[curr_len]);
      curr_len = curr_len + 5*sizeof(uint8_t);

      // update two_hop counter
      if(neighbor != linkaddr_node_addr.u8[LINKADDR_SIZE-1]) {
          mtm_indirect_observed_node(neighbor, timeslot_offset);
      }

      return_timestamps[i].addr = neighbor;
      return_timestamps[i].rx_timestamp = rx_timestamp;
  }

  uint32_t checksum_calculated = 0, checksum_packet = 0;
  // calculate checksum for buf
  for(int i = 0; i < curr_len; i++) {
      checksum_calculated += buf[i];
  }

  // extract checksum to end
  memcpy(&checksum_packet, &buf[curr_len], sizeof(uint32_t));
  curr_len = curr_len + sizeof(uint32_t);

  if(checksum_calculated != checksum_packet) {
      return 0;
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

#define TDOA_FLOAT double
#define PROP_FLOAT double
#if WITH_PASSIVE_TDOA
float mtm_compute_tdoa(struct mtm_pas_tdoa *ts) {
    int64_t M_a, M_b, R_a, D_a, R_b, D_b;

    // we are in the lucky position that we have a fpu, so we will use them instead of fixed precision.
    TDOA_FLOAT prop_drift_coff, ToF_ab, TD;

    // extract intervals from Message
    M_a = interval_correct_overflow(ts->r_l2 , ts->r_l1);
    M_b = interval_correct_overflow(ts->r_l3 , ts->r_l2);

    R_a = interval_correct_overflow(ts->ds_ts.r_a1 , ts->ds_ts.t_a1);
    D_a = interval_correct_overflow(ts->ds_ts.t_a2 , ts->ds_ts.r_a1);
    R_b = interval_correct_overflow(ts->ds_ts.r_b2 , ts->ds_ts.t_b1);
    D_b = interval_correct_overflow(ts->ds_ts.t_b1 , ts->ds_ts.r_b1);

    // M_a + M_b and R_a
    prop_drift_coff = (TDOA_FLOAT)((int64_t)M_a + M_b) / (TDOA_FLOAT)((int64_t)R_a + D_a);

    ToF_ab = calculate_propagation_time_alternative(&ts->ds_ts);

    /* TD = prop_drift_coff * (R_a - ToF_ab) - M_a; */
    TD = prop_drift_coff * ((TDOA_FLOAT)R_a - ToF_ab) - (TDOA_FLOAT)M_a;

    return TD;
}
#endif

float time_to_dist(float tof) {
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
  static uint32_t count = 0;

  /* printf("updating neighbor with addr %u prop time %d\n", n->addr.u8[LINKADDR_SIZE-1], prop_time); */

  n_prop_time.prop_time = prop_time;
  n_prop_time.asn = * asn; /* Copy the 5 bytes pointed by the pointer * asn to the n_prop_time.asn struct */
  n_prop_time.tsch_channel = tsch_channel;
  n->last_prop_time = n_prop_time;
  float range = time_to_dist(prop_time);

  count++;

  printf("DS, %" PRIu8 "," "%" PRId32 "\n", n->addr.u8[LINKADDR_SIZE-1],  (int32_t) (range * 100000));
}

// define macro define for used floating point type

int64_t partialMultiply(int64_t a, int64_t b) {
    int64_t k_value = 1LL << 32; // 2^32
    int64_t b_high = b / k_value;
    int64_t b_low = b % k_value;

    return (a * b_high << 32) + a * b_low;
}

int32_t correctedExpression(struct ds_twr_ts *ts) {
    int64_t initiator_roundtrip = interval_correct_overflow(ts->r_a1,  ts->t_a1);
    int64_t initiator_reply     = interval_correct_overflow(ts->t_a2,  ts->r_a1);
    int64_t replier_roundtrip   = interval_correct_overflow(ts->r_b2,  ts->t_b1);
    int64_t replier_reply       = interval_correct_overflow(ts->t_b1,  ts->r_b1);

    int64_t numerator_part1 = partialMultiply(initiator_roundtrip, replier_roundtrip);
    int64_t numerator_part2 = partialMultiply(initiator_reply, replier_reply);
    int64_t numerator = numerator_part1 - numerator_part2;

    int64_t denominator = initiator_roundtrip + replier_roundtrip + initiator_reply + replier_reply;

    return (int32_t)(numerator / denominator);
}


float calculate_propagation_time_alternative(struct ds_twr_ts *ts) {
    static PROP_FLOAT relative_drift_offset;
    static int64_t other_duration, own_duration;
    static int64_t round_duration_a, delay_duration_b;
    /* static PROP_FLOAT drift_offset_int, two_tof_int; */
    static int64_t drift_offset_int, two_tof_int;

    own_duration   = interval_correct_overflow(ts->t_a2, ts->t_a1);
    other_duration = interval_correct_overflow(ts->r_b2, ts->r_b1);

    // factor determining whether B's clock runs faster or slower measured from the perspective of our clock
    // a positive factor here means that the clock runs faster than ours
    /* relative_drift_offset = (float)((int64_t)own_duration-(int64_t)other_duration) / (float)(other_duration); */
    relative_drift_offset = (PROP_FLOAT)((int64_t)own_duration-(int64_t)other_duration) / (PROP_FLOAT)(other_duration);


    round_duration_a = interval_correct_overflow(ts->r_a1, ts->t_a1);
    delay_duration_b = interval_correct_overflow(ts->t_b1, ts->r_b1);

    /* drift_offset_int = -relative_drift_offset * (float) delay_duration_b; */
    drift_offset_int = round(-relative_drift_offset * (PROP_FLOAT) delay_duration_b);

    /* two_tof_int = (PROP_FLOAT)round_duration_a - (PROP_FLOAT)delay_duration_b + drift_offset_int; */
    two_tof_int = (int64_t)round_duration_a - (int64_t)delay_duration_b + drift_offset_int;

    return ((float) two_tof_int) * 0.5;
}


/* float calculate_propagation_time_alternative(struct ds_twr_ts *ts) { */
/*     int64_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply; */

/*     initiator_roundtrip = interval_correct_overflow(ts->r_a1,  ts->t_a1); */
/*     initiator_reply     = interval_correct_overflow(ts->t_a2,  ts->r_a1); */
/*     replier_roundtrip   = interval_correct_overflow(ts->r_b2,  ts->t_b1); */
/*     replier_reply       = interval_correct_overflow(ts->t_b1,  ts->r_b1); */

/*     return (float)((((int64_t) initiator_roundtrip * replier_roundtrip) */
/*             - ((int64_t) initiator_reply * replier_reply)) */
/*         / */
/*         ((int64_t) initiator_roundtrip */
/*             +  replier_roundtrip */
/*             +  initiator_reply */
/*             +  replier_reply)); */
/* } */

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
