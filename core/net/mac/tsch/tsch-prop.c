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
#include "net/mac/tsch/tsch-asn.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-prop.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "dev/radio.h"
#include "memb.h"

#include <stdio.h>
#include <string.h>


#if TSCH_MTM_LOCALISATION
LIST(measurement_list);
MEMB(mtm_prop_timestamp_memb, struct mtm_prop_timestamp, TSCH_MTM_PROP_MAX_MEASUREMENT);

void add_mtm_reception_timestamp(struct tsch_neighbor *n, uint64_t rx_timestamp) {
    struct mtm_prop_timestamp *t = NULL;

    t = memb_alloc(&mtm_prop_timestamp_memb);

    if (t != NULL) {
        list_add(measurement_list, t);
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
  memcpy(&buf[curr_len], &tx_timestamp, sizeof(uint64_t));
  curr_len = curr_len + sizeof(uint64_t);

  // iterate over list of measurements and add each timestamp to the packet
  /* struct mtm_prop_timestamp *t = NULL; */
  
  /* for(t = list_head(measurement_list); t != NULL; t = list_item_next(t)) { */
  /*     memcpy(&buf[curr_len], &t->rx_timestamp, sizeof(uint64_t)); */
  /*     curr_len = curr_len + sizeof(uint64_t); */
  /* } */

  /* // free all allocated memory for all measurements again */
  /* for(t = list_head(measurement_list); t != NULL; t = list_item_next(t)) { */
  /*     list_remove(measurement_list, t); */
  /*     memb_free(&mtm_prop_timestamp_memb, t); */
  /* } */
  
  return curr_len;
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

    while(1) {
      PROCESS_WAIT_EVENT();
      // printf("Got event number %d\n", ev);
      if(ev == PROCESS_EVENT_MSG){
        printf("New prop time %ld %u %lu %u\n", 
          ((struct tsch_neighbor *) data)->last_prop_time.prop_time, 
          ((struct tsch_neighbor *) data)->last_prop_time.asn.ms1b, /* most significant 1 byte */
          ((struct tsch_neighbor *) data)->last_prop_time.asn.ls4b, /* least significant 4 bytes */
          ((struct tsch_neighbor *) data)->last_prop_time.tsch_channel);
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
  n_prop_time.asn = * asn; /* Copy the 5 bytes pointed by the pointer * asn 
  to the n_prop_time.asn struct */
  n_prop_time.tsch_channel = tsch_channel;
  n->last_prop_time = n_prop_time;

  /* printf("TSCH-prop %ld %lu\n", 
          prop_time, 
          n_prop_time.asn.ls4b); */

  /* Send the PROCESS_EVENT_MSG event asynchronously to 
  "tsch_loc_operation", with a pointer to the tsch_neighbor. */
  process_post(&TSCH_PROP_PROCESS,
                PROCESS_EVENT_MSG, (void *) n);
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
  return (int32_t)(( ((int64_t) initiator_roundtrip * replier_roundtrip) 
                  - ((int64_t) initiator_reply * replier_reply) )
                /  ((int64_t) initiator_roundtrip 
                  +  replier_roundtrip 
                  +  initiator_reply 
                  +  replier_reply));
}