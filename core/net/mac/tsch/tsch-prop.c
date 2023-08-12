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
#include "net/mac/tsch/tsch-asn.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-prop.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "dev/radio.h"
#include "memb.h"


#include "nrfx_log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_stdint.h>

#include "dw1000-driver.h"
#include "dw1000-ranging-bias.h"

#if TSCH_MTM_LOCALISATION

// NRF SDK does not support uint64_t, so we define here some macros to dissect the number into two parts
#define LOG_UNSIGNED_LONG_LONG_MARKER "%u%u"
#define LOG_LLU(bigint) (uint32_t)(((bigint) >> 32) & 0xFFFFFFFF), (uint32_t)((bigint) & 0xFFFFFFFF)

struct ds_twr_ts
{
    uint64_t t_a1, r_b1, t_b1, r_a1, t_a2, r_b2, t_b2, r_a2;
};

float calculate_propagation_time_alternative(struct ds_twr_ts *ts);

struct mtm_neighbor {
    struct mtm_neighbor *next;
    linkaddr_t neighbor_addr;

    struct ds_twr_ts ts;
};

int64_t mtm_compute_propagation_time(struct tsch_asn_t *asn, struct mtm_neighbor *mtm_n);

// in the following we use the value -1 for uninitialized timestamps
struct mtm_rx_queue_item {
    struct mtm_rx_queue_item *next;
    linkaddr_t neighbor_addr;
    uint64_t rx_timestamp;
};

struct mtm_timestamp_assoc {
    struct mtm_timestamp_assoc *next;
    struct tsch_asn_t asn;
    uint64_t timestamp;
};

LIST(ranging_neighbor_list);
LIST(rx_send_queue);

MEMB(mtm_prop_neighbor_memb, struct mtm_neighbor, TSCH_MTM_PROP_MAX_MEASUREMENT);
MEMB(mtm_prop_rx_memb, struct mtm_rx_queue_item, TSCH_MTM_PROP_MAX_NEIGHBORS);

#define SPEED_OF_LIGHT_M_PER_S 299702547.236
#define SPEED_OF_LIGHT_M_PER_UWB_TU ((SPEED_OF_LIGHT_M_PER_S * 1.0E-15) * 15650.0) // around 0.00469175196

// for all timestamps check overflow condition. The internal clock of the decawave transceiver will overflow roughly every 17 seconds
// therefore sometimes a timestamp which should be bigger, will be smaller than a preceding one. DW_TIMESTAMP_MAX_VALUE
// this function expects that the user passes in high_ts the timestamp that is supposed to be the larger value of
// the given interval.
static inline int32_t interval_correct_overflow(uint64_t high_ts, uint64_t low_ts) {
    if (high_ts < low_ts) {
        return (DW_TIMESTAMP_MAX_VALUE - low_ts) + high_ts;
    }

    return high_ts - low_ts;
}

/* void add_mtm_reception_timestamp(struct tsch_neighbor *from_neighbor, uint64_t rx_timestamp_A, uint64_t rx_timestamp_B, uint64_t tx_timestamp_B) { */
/*     struct mtm_rx_queue_item *t = NULL; */

/*     // first update our own outgoing rx queue */
/*     if (list_length (rx_send_queue) >= TSCH_MTM_PROP_MAX_NEIGHBORS) { */
/*         printf("MTM: RX queue full, dropping received timestamp\n"); */
/*         return; */
/*     } */

/*     t = memb_alloc(&mtm_prop_rx_memb); */

/*     if (t != NULL) { */
/*         t->rx_timestamp = rx_timestamp_A; */
/*         t->neighbor = from_neighbor; */
/*         list_add(rx_send_queue, t); */
/*     } else { */
/*         printf("MTM: Could not allocate memory for reception timestamp\n"); */
/*         return; */
/*     } */

/*     // next update our neighbor table */
/*     struct mtm_neighbor *n = NULL; */
/*     for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) { */
/*         if (n->neighbor == from_neighbor) { */
/*             break; */
/*         } */
/*     } */

/*     // if no neighbor just break */
/*     if (n == NULL) { */
/*         printf("MTM: No neighbor found for reception timestamp\n"); */
/*         return; */
/*     } */

/*     n->ts.txB1 = tx_timestamp_B; */
/*     n->ts.rxA1 = rx_timestamp_A; */
/*     n->ts.rxB1 = rx_timestamp_B; */
/* } */

void add_mtm_reception_timestamp(linkaddr_t *neighbor_addr, struct tsch_asn_t *asn, uint64_t rx_timestamp_A, uint64_t rx_timestamp_B, uint64_t tx_timestamp_B) {
    struct mtm_rx_queue_item *t = NULL;

    printf("rx_timestamp_A %u%u, rx_timestamp_B %u%u, tx_timestamp_B %u%u\n",
        LOG_LLU(rx_timestamp_A), LOG_LLU(rx_timestamp_B), LOG_LLU(tx_timestamp_B)
        );

    // first update our own outgoing rx queue
    if (list_length (rx_send_queue) >= TSCH_MTM_PROP_MAX_NEIGHBORS) {
        printf("MTM: RX queue full, dropping received timestamp\n");
        return;
    }

    t = memb_alloc(&mtm_prop_rx_memb);

    if (t != NULL) {
        t->rx_timestamp = rx_timestamp_A;
        linkaddr_copy(&t->neighbor_addr, neighbor_addr);
        list_add(rx_send_queue, t);
    } else {
        printf("MTM: Could not allocate memory for reception timestamp\n");
        return;
    }

    // next update our neighbor table
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        if (linkaddr_cmp(&n->neighbor_addr, neighbor_addr)) {
            break;
        }
    }

    // if no neighbor just break
    if (n == NULL) {
        printf("MTM: No neighbor found for reception timestamp, Register new entry\n");
        // register neighbor in our ranging structure
        n = memb_alloc(&mtm_prop_neighbor_memb);
        if (n != NULL) {
            linkaddr_copy(&n->neighbor_addr, neighbor_addr);
            n->ts.r_a1 = UINT64_MAX;
            n->ts.t_b1 = UINT64_MAX;
            n->ts.r_a1 = UINT64_MAX;
            n->ts.t_a2 = UINT64_MAX;
            n->ts.r_b2 = UINT64_MAX;
            n->ts.t_b2 = UINT64_MAX;
            n->ts.r_a2 = UINT64_MAX;
            list_add(ranging_neighbor_list, n);
        } else {
            printf("MTM: Could not allocate memory for neighbor\n");
        }

        return;
    }

    // make coppy of timestamp
    struct ds_twr_ts ts = n->ts;

    // current round is always the one with the higher numbers

    n->ts.r_b1 = n->ts.r_b2;
    n->ts.t_b1 = n->ts.t_b2;
    n->ts.r_a1 = n->ts.r_a2;

    n->ts.r_b2 = rx_timestamp_B;
    n->ts.t_b2 = tx_timestamp_B;
    n->ts.r_a2 = rx_timestamp_A;

    mtm_compute_propagation_time(asn, n);

    /* n->ts.txB2 = tx_timestamp_B; */
    /* n->ts.rxB1 = rx_timestamp_B; */
    /* n->ts.rxA2 = rx_timestamp_A; */
}

void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp) {
    // Update mtm_neighbor list after our own transmission event
    struct mtm_neighbor *n = NULL;
    for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) {
        n->ts.t_a1 = n->ts.t_a2;
        n->ts.t_a2 = tx_timestamp;
    }
}


/* void add_mtm_transmission_timestamp(struct tsch_asn_t *asn, uint64_t tx_timestamp) { */

/*     // this is only temporarly, we will later keep our own neighbor table */
/*       struct tsch_neighbor *real_n = tsch_queue_get_real_neighbor_list_head(); */

/*       // Update mtm_neighbor list after our own transmission event */
/*       while(real_n != NULL) { */
/*           // check if neighbor has already a entry in our list */
/*           struct mtm_neighbor *n = NULL; */
/*           for(n = list_head(ranging_neighbor_list); n != NULL; n = list_item_next(n)) { */
/*               if (n->neighbor == real_n) { */
/*                   break; */
/*               } */
/*           } */

/*           if (n == NULL) { */
/*               // neighbor not found, add it */
/*               n = memb_alloc(&mtm_prop_neighbor_memb); */
/*               if (n != NULL) { */
/*                   n->neighbor = real_n; */
/*                   n->ts.txA1 = tx_timestamp; */
/*                   n->ts.rxB1 = UINT64_MAX; */
/*                   n->ts.txB1 = UINT64_MAX; */
/*                   n->ts.rxA1 = UINT64_MAX; */
/*                   n->ts.txA2 = UINT64_MAX; */
/*                   n->ts.rxB2 = UINT64_MAX; */
/*                   list_add(ranging_neighbor_list, n); */
/*               } else { */
/*                   printf("MTM: Could not allocate memory for neighbor\n"); */
/*                   return; */
/*               } */
/*           } else { // update existing entry */
/*               // shift all */
/*               n->ts.txA2 = n->ts.txA1; */
/*               n->ts.txA1 = tx_timestamp; */
/*               n->ts.rxB2 = n->ts.rxB1; */
/*               // null remaining */
/*               n->ts.txB1 = UINT64_MAX; */
/*               n->ts.rxA1 = UINT64_MAX; */
/*               n->ts.rxB1 = UINT64_MAX; */

/*           } */

/*           real_n = list_item_next(real_n); */
/*       } */
/* } */

void print_ds_twr_durations(struct ds_twr_ts *ts) {
/*     int32_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply; */

/*     // mtm_n->ts->rxA1 - mtm_n->ts->txA1; */
/*     initiator_roundtrip = interval_correct_overflow(ts->rxB1,  ts->txB1); */
/*     // ts->txA2 - ts->rxA1; */
/*     initiator_reply = interval_correct_overflow(ts->txB2, ts->rxB1); */
/*     // ts->rxB2 - ts->txB1; */
/*     replier_roundtrip = interval_correct_overflow(ts->rxA2, ts->txA1); */
/*     // ts->txB1 - ts->rxB1; */
/*     replier_reply = interval_correct_overflow(ts->txA1, ts->rxA1); */

/*     printf("MTM: Initiator roundtrip: %d\n", initiator_roundtrip); */
/*     printf("MTM: Initiator reply: %d\n", initiator_reply); */
/*     printf("MTM: Replier roundtrip: %d\n", replier_roundtrip); */
/*     printf("MTM: Replier reply: %d\n", replier_reply); */
}

void print_ds_twr_timestamps(struct ds_twr_ts *ts) {
    /* printf("txB1: %u%u, rxA1: %u%u, txA1: %u%u, rxB1: %u%u, txB2: %u%u, rxA2: %u%u\n", */
    /*     (uint32_t) (ts->txB1 >> 32), (uint32_t) ts->txB1, (uint32_t) (ts->rxA1 >> 32), (uint32_t) ts->rxA1, */
    /*     (uint32_t) (ts->txA1 >> 32), (uint32_t) ts->txA1, (uint32_t) (ts->rxB1 >> 32), (uint32_t) ts->rxB1, */
    /*     (uint32_t) (ts->txB2 >> 32), (uint32_t) ts->txB2, (uint32_t) (ts->rxA2 >> 32), (uint32_t) ts->rxA2); */
}


// computes propagation time from internal state kept for the passed tsch_neighbor
int64_t mtm_compute_propagation_time(struct tsch_asn_t *asn, struct mtm_neighbor *mtm_n) {
    // first check whether neighbor has a valid entry in our list and none of the timestamps are uninitialized, i.e., of value UINT64_MAX;

    if (mtm_n == NULL) {
        printf("MTM: Neighbor not found in list\n");
        return -1;
    }

    if (mtm_n->ts.t_a1 == UINT64_MAX || mtm_n->ts.r_b1 == UINT64_MAX
        || mtm_n->ts.t_b1 == UINT64_MAX || mtm_n->ts.r_a1 == UINT64_MAX
        || mtm_n->ts.t_a2 == UINT64_MAX || mtm_n->ts.r_b2 == UINT64_MAX
        || mtm_n->ts.t_b2 == UINT64_MAX || mtm_n->ts.r_a2 == UINT64_MAX
        ) {
        printf("MTM: Neighbor has uninitialized timestamps\n");
        return -1;
    }

    int32_t initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply;

    /* Finally calculate round trip times */

    initiator_roundtrip = interval_correct_overflow(mtm_n->ts.r_a1,  mtm_n->ts.t_a1);
    // mtm_n->ts.txA2 - mtm_n->ts.rxA1;
    initiator_reply = interval_correct_overflow(mtm_n->ts.t_a2, mtm_n->ts.r_a1);
    // mtm_n->ts.rxB2 - mtm_n->ts.txB1;
    replier_roundtrip = interval_correct_overflow(mtm_n->ts.r_b2, mtm_n->ts.t_b1);
    // mtm_n->ts.txB1 - mtm_n->ts.rxB1;
    replier_reply = interval_correct_overflow(mtm_n->ts.t_b1, mtm_n->ts.r_b1);


    int32_t prop_time  = compute_prop_time(initiator_roundtrip, initiator_reply, replier_roundtrip, replier_reply);
    /* float prop_time = calculate_propagation_time_alternative(&(mtm_n->ts)); */
    float range = prop_time * SPEED_OF_LIGHT_M_PER_UWB_TU;

    // bias correction in centimeters
    /* bias_correction = dw1000_getrangebias(n->last_prop_time.tsch_channel, range); */
    printf("bias uncorrected range to %u: " NRF_LOG_FLOAT_MARKER "\n", mtm_n->neighbor_addr.u8[LINKADDR_SIZE-1], NRF_LOG_FLOAT(range));
    printf("prop time to %u: %d\n", mtm_n->neighbor_addr.u8[LINKADDR_SIZE-1], prop_time);
    /* print_ds_twr_durations(&mtm_n->ts); */
    print_ds_twr_timestamps(&mtm_n->ts);
    // call into existing methods for passing data to user
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
      memcpy(&buf[curr_len], &t->neighbor_addr.u8[LINKADDR_SIZE-1], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&buf[curr_len], &t->rx_timestamp, sizeof(uint64_t));
      curr_len = curr_len + sizeof(uint64_t);

      printf("put %d -> %u%u \n", t->neighbor_addr.u8[LINKADDR_SIZE-1], (uint32_t)(t->rx_timestamp >> 32), (uint32_t)(t->rx_timestamp & 0xFFFFFFFF));
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
    uint8_t seqno,
    frame802154_t *frame,
    // timestamps
    uint64_t *timestamp_rx_B,
    uint64_t *timestamp_tx_B
     )
{
  uint8_t curr_len = 0;
  int ret;
  linkaddr_t dest;


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
  if(frame802154_extract_linkaddr(frame, NULL, &dest) == 0 ||
     (!linkaddr_cmp(&dest, &linkaddr_node_addr)
      && !linkaddr_cmp(&dest, &linkaddr_null))) {
    return 0;
  }

  // if everything is okay we will extract the timestamps

  // extract timestamps
  uint64_t tx_timestamp;
  // extract rx timestamp amount
  uint8_t amount_of_measurements;
  memcpy(&tx_timestamp, &buf[curr_len], sizeof(uint64_t));
  curr_len = curr_len + sizeof(uint64_t);
  memcpy(&amount_of_measurements, &buf[curr_len], sizeof(uint8_t));
  curr_len = curr_len + sizeof(uint8_t);
  for(int i = 0; i < amount_of_measurements; i++) {
      uint8_t neighbor;
      uint64_t rx_timestamp;

      memcpy(&neighbor, &buf[curr_len], sizeof(uint8_t));
      curr_len = curr_len + sizeof(uint8_t);
      memcpy(&rx_timestamp, &buf[curr_len], sizeof(uint64_t));
      curr_len = curr_len + sizeof(uint64_t);

      // check if the timestamps is for us
      if (neighbor == linkaddr_node_addr.u8[LINKADDR_SIZE-1]) {
          *timestamp_rx_B = rx_timestamp;
          printf("grab %u -> %u%u\n", neighbor, (uint32_t)(*timestamp_rx_B >> 32), (uint32_t)(*timestamp_rx_B & 0xFFFFFFFF));
      }
  }

  // set tx timestamp
  *timestamp_tx_B = tx_timestamp;
  return curr_len;
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

          range = n->last_prop_time.prop_time * SPEED_OF_LIGHT_M_PER_UWB_TU;
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


  n_prop_time.prop_time = prop_time;
  n_prop_time.asn = * asn; /* Copy the 5 bytes pointed by the pointer * asn to the n_prop_time.asn struct */
  n_prop_time.tsch_channel = tsch_channel;
  n->last_prop_time = n_prop_time;

  /* Send the PROCESS_EVENT_MSG event asynchronously to
  "tsch_loc_operation", with a pointer to the tsch_neighbor. */
  process_post(&TSCH_PROP_PROCESS,
                PROCESS_EVENT_MSG, (void *) n);
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
