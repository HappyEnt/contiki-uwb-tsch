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

#ifndef __TSCH_LOC_H__
#define __TSCH_LOC_H__

/********** Includes **********/

#include "contiki.h"
#include "net/mac/tsch/tsch-asn.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-schedule.h"

/********** Propagation time Process *********/
PROCESS_NAME(TSCH_PROP_PROCESS);
PROCESS_NAME(TSCH_MTM_PROCESS);

/********** Type *********/

#if TSCH_MTM_LOCALISATION

// depends on frame length
#define TSCH_MTM_PROP_MAX_NEIGHBORS 15
// defines the maximum amount of measurements we will store
#define TSCH_MTM_PROP_MAX_MEASUREMENT 15


// define ranging_addr_t as uint8_t for now
typedef uint8_t ranging_addr_t;

enum measurement_type {
    TDOA,
    TWR
};

struct distance_measurement {
    enum measurement_type type;

    struct tsch_asn_t asn;

    /* In case that the measurement type is of type TWR, addr_a and addr_b denote the two nodes, for
     * which we calculated the time of flight. Note that in most cases addr_a will therefore be our
     * own address, but off course we are also able to calculate other distances by using the other
     * timestamps that we get through our ranging messages. In case that the measurement type is
     * TDOA, addr_a and addr_B will always be the two anchor nodes towards whom we estimate the time
     * difference of arrival.
     */
    ranging_addr_t addr_A;
    ranging_addr_t addr_B;
    
    float time;
    int32_t freq_offset;
};


// make public for now, should probably later be replaced with a better interface
struct ds_twr_ts
{
    uint64_t t_a1, r_b1, t_b1, r_a1, t_a2, r_b2, t_b2, r_a2;
};

struct mtm_pas_tdoa
{
    struct mtm_pas_tdoa *next;

    // For internal mangement we will identify with node A the node that initiated a transfer
    // and with node B the responder. Off course both nodes take either role, but
    // to not mix up the timestamps we will always use the same notation.
    ranging_addr_t A_addr; 
    ranging_addr_t B_addr;

    clock_time_t last_observed;

    struct distance_measurement last_measurement;

    // this information is used to detect when a round was successfully received
    // for this we must get a return message from B which was send in the same
    // round as the tx timestamp that we extract from A's message
    uint64_t most_recent_tx_A, rx_timestamp_L1, most_recent_tx_B, rx_timestamp_L2;
    uint8_t rx_timeslot_L1, rx_timeslot_L2; // stores in which round and timeslot we did receive the message we identify as initiator message at
    uint64_t rx_round_counter_L1, rx_round_counter_L2;
    uint8_t closed_round;

    struct ds_twr_ts ds_ts;
    uint64_t r_l1, r_l2, r_l3, r_l4;
};

enum mtm_neighbor_type  {
    MTM_DIRECT_NEIGHBOR,
    MTM_TWO_HOP_NEIGHBOR,
};

struct mtm_neighbor {
    struct mtm_neighbor *next;

    ranging_addr_t neighbor_addr;
    struct ds_twr_ts ts;
    
    struct distance_measurement last_measurement;

    enum mtm_neighbor_type type;

    clock_time_t last_observed_direct, last_observed_indirect;
    uint8_t observed_timeslot; // the timeslot we learned that the node sends in
    uint64_t total_found_ours_counter; // counter which tracks how often in total we found our timestamp
};

struct mtm_packet_timestamp {
    ranging_addr_t addr;
    uint64_t rx_timestamp;
};

void add_mtm_reception_timestamp(
    ranging_addr_t neighbor_addr,
    struct tsch_asn_t *asn,
    uint8_t timeslot,
    
    uint64_t rx_timestamp_A,    
    uint64_t tx_timestamp_B,
    
    struct mtm_packet_timestamp *rx_timestamps,
    uint8_t num_rx_timestamps
);

void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp);
void add_to_direct_observed_rx_to_queue(uint64_t rx_timestamp, uint8_t neighbor, uint8_t timeslot_offset);
void mtm_set_round_end(uint8_t timeslot);
void mtm_slot_end_handler(uint16_t timeslot);
void set_mtm_tx_slot(uint8_t timeslot);
void mtm_reset_rx_queue();
void mtm_reset();
list_t tsch_prop_get_neighbor_list();
list_t tsch_prop_get_tdoa_list();

// requires passing of the asn. Currently not used, but could in future be used when the duration
// between measurements increases. This would allow us to give a measurement of how outdated or bad
// the measurement is.

float time_to_dist(float tof);

/* tsch_prop_time is defined in tsch-queue.h to avoid loop in declaration. */
int tsch_packet_create_multiranging_packet(
    uint8_t *buf,
    int buf_size,
    const linkaddr_t *dest_addr,
    uint8_t seqno,
    uint64_t tx_timestamp);

int
tsch_packet_parse_multiranging_packet(
    uint8_t *buf,
    int buf_size,
    uint8_t timeslot_offset, // yes yes, should be uint16_t but for our design we assume short slotframes for the ranging
    frame802154_t *frame,
    // timestamps
    uint64_t *timestamp_tx_B,
    struct mtm_packet_timestamp **rx_timestamps,
    uint8_t *num_timestamps
    );
    
#endif // TSCH_MTM_LOCALISATION
/********** Functions *********/

#if TSCH_MTM_LOCALISATION
/* void  */
#endif

void update_neighbor_prop_time(struct tsch_neighbor *n, int32_t prop_time, struct tsch_asn_t * asn, uint8_t tsch_channel);

int32_t compute_prop_time(int32_t initiator_roundtrip,
      int32_t initiator_reply, int32_t replier_roundtrip,
      int32_t replier_reply);

/* int64_t compute_prop_time(int64_t initiator_roundtrip, int64_t initiator_reply, */
/*     int64_t replier_roundtrip, int64_t replier_reply); */

#endif /* __TSCH_LOC_H__ */
