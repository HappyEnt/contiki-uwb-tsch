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
#define TSCH_MTM_PROP_MAX_NEIGHBORS 20
// defines the maximum amount of measurements we will store
#define TSCH_MTM_PROP_MAX_MEASUREMENT 5

void add_mtm_reception_timestamp(linkaddr_t *neighbor_addr, struct tsch_asn_t *asn, uint64_t rx_timestamp_A, uint64_t rx_timestamp_B, uint64_t tx_timestamp_B);
void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp);
// requires passing of the asn. Currently not used, but could in future be used when the duration
// between measurements increases. This would allow us to give a measurement of how outdated or bad
// the measurement is.


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
    uint8_t seqno,
    frame802154_t *frame,
    // timestamps
    uint64_t *timestamp_rx_B,
    uint64_t *timestamp_tx_B
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
