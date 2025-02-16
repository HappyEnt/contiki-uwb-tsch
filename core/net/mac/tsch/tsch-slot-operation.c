/*
 * Copyright (c) 2015, SICS Swedish ICT.
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
 *         TSCH slot operation implementation, running from interrupt.
 * \author
 *         Simon Duquennoy <simonduq@sics.se>
 *         Beshr Al Nahas <beshr@sics.se>
 *         Atis Elsts <atis.elsts@bristol.ac.uk>
 *
 */

#include "contiki.h"
#include "dev/radio.h"
#include "dw1000.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/mac/framer-802154.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-slot-operation.h"
#include "net/mac/tsch/tsch-queue.h"
#include "net/mac/tsch/tsch-private.h"
#include "net/mac/tsch/tsch-log.h"
#include "net/mac/tsch/tsch-packet.h"
#include "net/mac/tsch/tsch-prop.h"
#include "net/mac/tsch/tsch-security.h"
#include "net/mac/tsch/tsch-adaptive-timesync.h"
#include "tsch-schedule.h"
#include "watchdog.h"
#include "random.h"

/* we do not want to keep these file included. */
// #include "dw1000.h"
// #include "dw1000-driver.h"
// #include "dw1000-const.h"

#include "decadriver/deca_device_api.h"
#include <stdint.h>


#if WITH_DWM_DEBUG_GPIO
#include "debug-gpio.h"
#define TOGGLE_DEBUG_GPIO() debug_toggle_pin();
#define QUICK_TOGGLE_GPIO() do { \
  TOGGLE_DEBUG_GPIO(); \
  TOGGLE_DEBUG_GPIO(); \
} while(0)

#else
#define TOGGLE_DEBUG_GPIO()
#define QUICK_TOGGLE_GPIO()
#endif

#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64
#include "lib/simEnvChange.h"
#include "sys/cooja_mt.h"
#endif /* CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64 */

/* Just for the debugging of TSCH */
// #define DEBUG_GPIO_TSCH 1

/* only enable the TX slot end port, other info comes from dw1000-driver.c */
// #define DEBUG_SYNC_LOGICAL 1

// TODO remove all this after development has concluded
#define TMP_DEBUG_MTM_LOCALISATION 0
#if TMP_DEBUG_MTM_LOCALISATION
#define _PRINTF(...) printf(__VA_ARGS__)
#else
#define _PRINTF(...)
#endif


#define PRINTADDR(addr) printf(" %02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7])
#ifdef DEBUG_GPIO_TSCH
  #include "sys/clock.h"
  #include "dev/gpio.h"

  #undef UART_WRITE
  #define UART_WRITE 1

  #ifdef DEBUG_SYNC_LOGICAL
    /* We use the logical analyser on 3 pin (PD2 (RX), PD0 (TX) and PA2 (slot end))
      to see if the synchronisation work correctly.
      In this file we will define the TSCH_DEBUG_SLOT_END macro and enable the other macro in the driver of the transceiver. */
    #define TSCH_DEBUG_RX_EVENT()
    #define TSCH_DEBUG_TX_EVENT()
    #define TSCH_DEBUG_SLOT_END()
    #define DWM1000_PA2_PORT           GPIO_A_NUM
    #define DWM1000_PA2_PIN            2
    #define TSCH_DEBUG_SLOT_START() do { \
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(DWM1000_PA2_PORT), GPIO_PIN_MASK(DWM1000_PA2_PIN)); \
        clock_delay_usec(1); \
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PA2_PORT), GPIO_PIN_MASK(DWM1000_PA2_PIN)); \
    } while(0)

    #define DWM1000_PB3_PORT           GPIO_A_NUM
    #define DWM1000_PB3_PIN            6
    #define PB3_TRIGGER() do { \
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN)); \
        clock_delay_usec(1); \
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN)); \
    } while(0)

    #define TSCH_DEBUG_INIT() do {\
      GPIO_SOFTWARE_CONTROL(GPIO_PORT_TO_BASE(DWM1000_PA2_PORT), GPIO_PIN_MASK(DWM1000_PA2_PIN)); \
      GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(DWM1000_PA2_PORT), GPIO_PIN_MASK(DWM1000_PA2_PIN)); \
      GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PA2_PORT), GPIO_PIN_MASK(DWM1000_PA2_PIN)); \
      GPIO_SOFTWARE_CONTROL(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN)); \
      GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN)); \
      GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN)); \
    } while(0)
  #else/* if not def DEBUG_SYNC_LOGICAL */
    #define DWM1000_PD2_PORT           GPIO_D_NUM
    #define DWM1000_PD2_PIN            2
    #define PD2_TRIGGER() do { \
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(DWM1000_PD2_PORT), GPIO_PIN_MASK(DWM1000_PD2_PIN)); \
        clock_delay_usec(1); \
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PD2_PORT), GPIO_PIN_MASK(DWM1000_PD2_PIN)); \
    } while(0)
    #define DWM1000_PD0_PORT           GPIO_D_NUM
    #define DWM1000_PD0_PIN            0
    #define PD0_TRIGGER() do { \
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(DWM1000_PD0_PORT), GPIO_PIN_MASK(DWM1000_PD0_PIN)); \
        clock_delay_usec(1); \
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PD0_PORT), GPIO_PIN_MASK(DWM1000_PD0_PIN)); \
    } while(0)
    /* First PD2, after PD0 */
    #define TSCH_DEBUG_INIT() do {\
      GPIO_SOFTWARE_CONTROL(GPIO_PORT_TO_BASE(DWM1000_PD2_PORT), GPIO_PIN_MASK(DWM1000_PD2_PIN)); \
      GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(DWM1000_PD2_PORT), GPIO_PIN_MASK(DWM1000_PD2_PIN)); \
      GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PD2_PORT), GPIO_PIN_MASK(DWM1000_PD2_PIN)); \
      GPIO_SOFTWARE_CONTROL(GPIO_PORT_TO_BASE(DWM1000_PD0_PORT), GPIO_PIN_MASK(DWM1000_PD0_PIN)); \
      GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(DWM1000_PD0_PORT), GPIO_PIN_MASK(DWM1000_PD0_PIN)); \
      GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PD0_PORT), GPIO_PIN_MASK(DWM1000_PD0_PIN)); \
    } while(0)

    #define TSCH_DEBUG_RX_EVENT() do {\
      PD2_TRIGGER();\
    } while(0)
    #define TSCH_DEBUG_TX_EVENT() do {\
      PD2_TRIGGER();\
      PD2_TRIGGER();\
    } while(0)
    #define TSCH_DEBUG_SLOT_START() do {\
      PD0_TRIGGER();\
    } while(0)
    #define TSCH_DEBUG_SLOT_END() do {\
      PD0_TRIGGER();\
    } while(0)
    #define PB3_TRIGGER()
  #endif /* DEBUG_SYNC_LOGICAL */
#endif /* DEBUG_GPIO_TSCH */

#if TSCH_CHORUS
  #undef UART_WRITE
  #define UART_WRITE 1
  #include "dw1000-driver.h" /* US_TO_RADIO */
  #include "dw1000-arch.h" /* US_TO_RADIO */
  #include "dw1000-util.h"


#endif /* TSCH_CHORUS */

#define UART_WRITE 1
#ifdef UART_WRITE
  #include "dev/uart0.h"
  #define write_byte(b) uart0_writeb(b);
#endif /* UART_WRITE */
/* END : Just for the debugging of TSCH */

#if TSCH_LOG_LEVEL >= 1
#define DEBUG DEBUG_PRINT
#else /* TSCH_LOG_LEVEL */
#define DEBUG DEBUG_NONE
#endif /* TSCH_LOG_LEVEL */
#include "net/net-debug.h"

/* TSCH debug macros, i.e. to set LEDs or GPIOs on various TSCH
 * timeslot events */
#ifndef TSCH_DEBUG_INIT
#define TSCH_DEBUG_INIT()
#endif
#ifndef TSCH_DEBUG_INTERRUPT
#define TSCH_DEBUG_INTERRUPT()
#endif
#ifndef TSCH_DEBUG_RX_EVENT
#define TSCH_DEBUG_RX_EVENT()
#endif
#ifndef TSCH_DEBUG_TX_EVENT
#define TSCH_DEBUG_TX_EVENT()
#endif
#ifndef TSCH_DEBUG_SLOT_START
#define TSCH_DEBUG_SLOT_START()
#endif
#ifndef TSCH_DEBUG_SLOT_END
#define TSCH_DEBUG_SLOT_END()
#endif

/* #define TSCH_DEBUG_TX_EVENT() printf("TSCH: TX\n") */
/* #define TSCH_DEBUG_RX_EVENT() printf("TSCH: RX\n") */
/* #define TSCH_DEBUG_SLOT_END() printf("TSCH: slot end\n") */

/* Check if TSCH_MAX_INCOMING_PACKETS is power of two */
#if (TSCH_MAX_INCOMING_PACKETS & (TSCH_MAX_INCOMING_PACKETS - 1)) != 0
#error TSCH_MAX_INCOMING_PACKETS must be power of two
#endif

/* Check if TSCH_DEQUEUED_ARRAY_SIZE is power of two and greater or equal to QUEUEBUF_NUM */
#if TSCH_DEQUEUED_ARRAY_SIZE < QUEUEBUF_NUM
#error TSCH_DEQUEUED_ARRAY_SIZE must be greater or equal to QUEUEBUF_NUM
#endif
#if (TSCH_DEQUEUED_ARRAY_SIZE & (TSCH_DEQUEUED_ARRAY_SIZE - 1)) != 0
#error TSCH_DEQUEUED_ARRAY_SIZE must be power of two
#endif

/* Truncate received drift correction information to maximum half
 * of the guard time (one fourth of TSCH_DEFAULT_TS_RX_WAIT) */
#define SYNC_IE_BOUND ((int32_t)US_TO_RTIMERTICKS(TSCH_DEFAULT_TS_RX_WAIT / 4))

/* By default: check that rtimer runs at >=32kHz and use a guard time of 10us */
#if RTIMER_SECOND < (32 * 1024)
#error "TSCH: RTIMER_SECOND < (32 * 1024)"
#endif
#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64
/* Use 0 usec guard time for Cooja Mote with a 1 MHz Rtimer*/
#define RTIMER_GUARD 0u
#elif RTIMER_SECOND >= 200000
#define RTIMER_GUARD (RTIMER_SECOND / 100000)
#else
#define RTIMER_GUARD 2u
#endif

enum tsch_radio_state_on_cmd {
  TSCH_RADIO_CMD_ON_START_OF_TIMESLOT,
  TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT,
  TSCH_RADIO_CMD_ON_FORCE,
};

enum tsch_radio_state_off_cmd {
  TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT,
  TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT,
  TSCH_RADIO_CMD_OFF_FORCE,
};

/* A ringbuf storing outgoing packets after they were dequeued.
 * Will be processed layer by tsch_tx_process_pending */
struct ringbufindex dequeued_ringbuf;
struct tsch_packet *dequeued_array[TSCH_DEQUEUED_ARRAY_SIZE];
/* A ringbuf storing incoming packets.
 * Will be processed layer by tsch_rx_process_pending */
struct ringbufindex input_ringbuf;
struct input_packet input_array[TSCH_MAX_INCOMING_PACKETS];

/* Last time we received Sync-IE (ACK or data packet from a time source) */
static struct tsch_asn_t last_sync_asn;

/* A global lock for manipulating data structures safely from outside of interrupt */
static volatile int tsch_locked = 0;
/* As long as this is set, skip all slot operation */
static volatile int tsch_lock_requested = 0;

/* Last estimated drift in RTIMER ticks
 * (Sky: 1 tick = 30.517578125 usec exactly) */
static int32_t drift_correction = 0;
/* Is drift correction used? (Can be true even if drift_correction == 0) */
static uint8_t is_drift_correction_used;

/* The neighbor last used as our time source */
struct tsch_neighbor *last_timesource_neighbor = NULL;

/* Used from tsch_slot_operation and sub-protothreads */
static rtimer_clock_t volatile current_slot_start;

/* Are we currently inside a slot? */
static volatile int tsch_in_slot_operation = 0;

/* If we are inside a slot, this tells the current channel */
static uint8_t current_channel;

/* Info about the link, packet and neighbor of
 * the current (or next) slot */
struct tsch_link *current_link = NULL;
/* A backup link with Rx flag, overlapping with current_link.
 * If the current link is Tx-only and the Tx queue
 * is empty while executing the link, fallback to the backup link. */
static struct tsch_link *backup_link = NULL;
static struct tsch_packet *current_packet = NULL;
static struct tsch_neighbor *current_neighbor = NULL;

/* Protothread for association */
PT_THREAD(tsch_scan(struct pt *pt));
/* Protothread for slot operation, called from rtimer interrupt
 * and scheduled from tsch_schedule_slot_operation */
static PT_THREAD(tsch_slot_operation(struct rtimer *t, void *ptr));
static struct pt slot_operation_pt;
/* Sub-protothreads of tsch_slot_operation */
static PT_THREAD(tsch_tx_slot(struct pt *pt, struct rtimer *t));
static PT_THREAD(tsch_rx_slot(struct pt *pt, struct rtimer *t));
#if TSCH_LOCALISATION
  static PT_THREAD(tsch_tx_prop_slot(struct pt *pt, struct rtimer *t));
  static PT_THREAD(tsch_rx_prop_slot(struct pt *pt, struct rtimer *t));
#endif /* TSCH_LOCALISATION*/

#if TSCH_MTM_LOCALISATION
  static PT_THREAD(tsch_mtm_tx_slot(struct pt *pt, struct rtimer *t));
  static PT_THREAD(tsch_mtm_rx_slot(struct pt *pt, struct rtimer *t));
#endif

#if TSCH_CHORUS
  #if TSCH_CHORUS_NODE_TYPE == CHORUS_INITIATOR_NODE
  static PT_THREAD(tsch_chorus_initiator_slot(struct pt *pt, struct rtimer *t));
  #elif TSCH_CHORUS_NODE_TYPE == CHORUS_MOBILE_NODE
  static PT_THREAD(tsch_chorus_mobile_slot(struct pt *pt, struct rtimer *t));
  #elif TSCH_CHORUS_NODE_TYPE == CHORUS_ANCHORS_NODE
  static PT_THREAD(tsch_chorus_anchor_slot(struct pt *pt, struct rtimer *t));
  #endif /* TSCH_CHORUS_NODE_TYPE */
#endif /* TSCH_CHORUS*/

/*---------------------------------------------------------------------------*/
/* TSCH locking system. TSCH is locked during slot operations */

/* Is TSCH locked? */
int
tsch_is_locked(void)
{
  return tsch_locked;
}

/* Lock TSCH (no slot operation) */
int
tsch_get_lock(void)
{
  if(!tsch_locked) {
    rtimer_clock_t busy_wait_time;
    int busy_wait = 0; /* Flag used for logging purposes */
    /* Make sure no new slot operation will start */
    tsch_lock_requested = 1;
    /* Wait for the end of current slot operation. */
    if(tsch_in_slot_operation) {
      busy_wait = 1;
      busy_wait_time = RTIMER_NOW();
      #ifdef DEBUG_GPIO_TSCH
        write_byte((uint8_t) '-');
        write_byte((uint8_t) 'P');
        write_byte((uint8_t) ':');
        write_byte((uint8_t) 'T'); //TSCH
        write_byte((uint8_t) (9+7));
        for(int i = 0; i < 4 ; i++){
          write_byte((uint8_t) ((uint8_t*)&busy_wait_time)[i]);
        }
        write_byte((uint8_t) 6);
        write_byte((uint8_t) 'L');
        write_byte((uint8_t) 'O');
        write_byte((uint8_t) 'C');
        write_byte((uint8_t) 'K');
        write_byte((uint8_t) 'E');
        write_byte((uint8_t) 'D');
        write_byte((uint8_t) '\n');
      #endif /* DEBUG_GPIO_TSCH */

      while(tsch_in_slot_operation) {
#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64
        simProcessRunValue = 1;
        cooja_mt_yield();
#endif /* CONTIKI_TARGET_COOJA || CONTIKI_TARGET_COOJA_IP64 */

        watchdog_periodic();
      }
      busy_wait_time = RTIMER_NOW() - busy_wait_time;
    }
    if(!tsch_locked) {
      /* Take the lock if it is free */
      tsch_locked = 1;
      tsch_lock_requested = 0;
      if(busy_wait) {
        /* Issue a log whenever we had to busy wait until getting the lock */
        TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "!get lock delay %u", (unsigned)busy_wait_time);
        );
      }
      return 1;
    }
  }
  TSCH_LOG_ADD(tsch_log_message,
      snprintf(log->message, sizeof(log->message),
                      "!failed to lock");
          );
  return 0;
}

/* Release TSCH lock */
void
tsch_release_lock(void)
{
  tsch_locked = 0;
}

/*---------------------------------------------------------------------------*/
/* Channel hopping utility functions */

/* Return channel from ASN and channel offset */
uint8_t
tsch_calculate_channel(struct tsch_asn_t *asn, uint8_t channel_offset)
{
  uint16_t index_of_0 = TSCH_ASN_MOD(*asn, tsch_hopping_sequence_length);
  uint16_t index_of_offset = (index_of_0 + channel_offset) % tsch_hopping_sequence_length.val;
  return tsch_hopping_sequence[index_of_offset];
}
/*---------------------------------------------------------------------------*/
/* Timing utility functions */

/* Checks if the current time has passed a ref time + offset. Assumes
 * a single overflow and ref time prior to now. */
static uint8_t
check_timer_miss(rtimer_clock_t ref_time, rtimer_clock_t offset, rtimer_clock_t now)
{
  rtimer_clock_t target = ref_time + offset;
  int now_has_overflowed = now < ref_time;
  int target_has_overflowed = target < ref_time;

  if(now_has_overflowed == target_has_overflowed) {
    /* Both or none have overflowed, just compare now to the target */
    return target <= now;
  } else {
    /* Either now or target of overflowed.
     * If it is now, then it has passed the target.
     * If it is target, then we haven't reached it yet.
     *  */
    return now_has_overflowed;
  }
}
/*---------------------------------------------------------------------------*/
/* Schedule a wakeup at a specified offset from a reference time.
 * Provides basic protection against missed deadlines and timer overflows
 * A return value of zero signals a missed deadline: no rtimer was scheduled. */
static uint8_t
tsch_schedule_slot_operation(struct rtimer *tm, rtimer_clock_t ref_time, rtimer_clock_t offset, const char *str)
{
  rtimer_clock_t now = RTIMER_NOW();
  int r;
  /* Subtract RTIMER_GUARD before checking for deadline miss
   * because we can not schedule rtimer less than RTIMER_GUARD in the future */
  int missed = check_timer_miss(ref_time, offset - RTIMER_GUARD, now);

  _PRINTF("tsch_schedule_slot_operation: now=%lu, ref_time=%lu, offset=%lu\n", now, ref_time, offset);

  if(missed) {
    TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                    "!dl-miss %s %d %d",
                        str, (int)(now-ref_time), (int)offset);
    );

    return 0;
  }
  ref_time += offset;
  r = rtimer_set(tm, ref_time, 1, (void (*)(struct rtimer *, void *))tsch_slot_operation, NULL);
  if(r != RTIMER_OK) {
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
/* Schedule slot operation conditionally, and YIELD if success only.
 * Always attempt to schedule RTIMER_GUARD before the target to make sure to wake up
 * ahead of time and then busy wait to exactly hit the target. */
#define TSCH_SCHEDULE_AND_YIELD(pt, tm, ref_time, offset, str) \
  do { \
    if(tsch_schedule_slot_operation(tm, ref_time, offset - RTIMER_GUARD, str)) { \
       PT_YIELD(pt); \
    } \
    watchdog_periodic(); \
    BUSYWAIT_UNTIL_ABS(0, ref_time, offset); \
  } while(0);
/*---------------------------------------------------------------------------*/
/* Schedule slot operation conditionally, and YIELD if success only.
 * Always attempt to schedule RTIMER_GUARD before the target to make sure to wake up
 * ahead of time and then busy wait to exactly hit the target. */
#define TSCH_WAIT(pt, tm, ref_time, offset, str) \
  do { \
    watchdog_periodic(); \
    BUSYWAIT_UNTIL_ABS(0, ref_time, offset); \
  } while(0);
  // TSCH_SCHEDULE_AND_YIELD(pt, tm, ref_time, offset, str)
/*---------------------------------------------------------------------------*/
/* Get EB, broadcast or unicast packet to be sent, and target neighbor. */
static struct tsch_packet *
get_packet_and_neighbor_for_link(struct tsch_link *link, struct tsch_neighbor **target_neighbor)
{
  struct tsch_packet *p = NULL;
  struct tsch_neighbor *n = NULL;

  /* Is this a Tx link? */
  if(link->link_options & LINK_OPTION_TX) {
    /* is it for advertisement of EB? */
    if(link->link_type == LINK_TYPE_ADVERTISING || link->link_type == LINK_TYPE_ADVERTISING_ONLY) {
      /* fetch EB packets */
      n = n_eb;
      p = tsch_queue_get_packet_for_nbr(n, link);
    }
    if(link->link_type != LINK_TYPE_ADVERTISING_ONLY) {
      /* NORMAL link or no EB to send, pick a data packet */
      if(p == NULL) {
        /* Get neighbor queue associated to the link and get packet from it */
        n = tsch_queue_get_nbr(&link->addr);
        p = tsch_queue_get_packet_for_nbr(n, link);
        /* if it is a broadcast slot and there were no broadcast packets, pick any unicast packet */
        if(p == NULL && n == n_broadcast) {
          p = tsch_queue_get_unicast_packet_for_any(&n, link);
        }
      }
    }
  }
  /* return nbr (by reference) */
  if(target_neighbor != NULL) {
    *target_neighbor = n;
  }

  return p;
}
/*---------------------------------------------------------------------------*/
/* Post TX: Update neighbor state after a transmission */
static int
update_neighbor_state(struct tsch_neighbor *n, struct tsch_packet *p,
                      struct tsch_link *link, uint8_t mac_tx_status)
{
  int in_queue = 1;
  int is_shared_link = link->link_options & LINK_OPTION_SHARED;
  int is_unicast = !n->is_broadcast;

  if(mac_tx_status == MAC_TX_OK) {
    /* Successful transmission */
    tsch_queue_remove_packet_from_queue(n);
    in_queue = 0;

    /* Update CSMA state in the unicast case */
    if(is_unicast) {
      if(is_shared_link || tsch_queue_is_empty(n)) {
        /* If this is a shared link, reset backoff on success.
         * Otherwise, do so only is the queue is empty */
        tsch_queue_backoff_reset(n);
      }
    }
  } else {
    /* Failed transmission */
    if(p->transmissions >= TSCH_MAC_MAX_FRAME_RETRIES + 1) {
      /* Drop packet */
      tsch_queue_remove_packet_from_queue(n);
      in_queue = 0;
    }
    /* Update CSMA state in the unicast case */
    if(is_unicast) {
      /* Failures on dedicated (== non-shared) leave the backoff
       * window nor exponent unchanged */
      if(is_shared_link) {
        /* Shared link: increment backoff exponent, pick a new window */
        tsch_queue_backoff_inc(n);
      }
    }
  }

  return in_queue;
}
/*---------------------------------------------------------------------------*/
/**
 * This function turns on the radio. Its semantics is dependent on
 * the value of TSCH_RADIO_ON_DURING_TIMESLOT constant:
 * - if enabled, the radio is turned on at the start of the slot
 * - if disabled, the radio is turned on within the slot,
 *   directly before the packet Rx guard time and ACK Rx guard time.
 */
static void
tsch_radio_on(enum tsch_radio_state_on_cmd command)
{
  int do_it = 0;
  switch(command) {
  case TSCH_RADIO_CMD_ON_START_OF_TIMESLOT:
    if(TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT:
    if(!TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_ON_FORCE:
    do_it = 1;
    break;
  }
  if(do_it) {
    NETSTACK_RADIO.on();
  }
}
/*---------------------------------------------------------------------------*/
/**
 * This function turns off the radio. In the same way as for tsch_radio_on(),
 * it depends on the value of TSCH_RADIO_ON_DURING_TIMESLOT constant:
 * - if enabled, the radio is turned off at the end of the slot
 * - if disabled, the radio is turned off within the slot,
 *   directly after Tx'ing or Rx'ing a packet or Tx'ing an ACK.
 */
static void
tsch_radio_off(enum tsch_radio_state_off_cmd command)
{
  int do_it = 0;
  switch(command) {
  case TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT:
    if(TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT:
    if(!TSCH_RADIO_ON_DURING_TIMESLOT) {
      do_it = 1;
    }
    break;
  case TSCH_RADIO_CMD_OFF_FORCE:
    do_it = 1;
    break;
  }
  if(do_it) {
    NETSTACK_RADIO.off();
  }
}

/*---------------------------------------------------------------------------*/
/**
 * Tell if a slot is active or not
 * The slot is ative if we have a packet to send or
 * If the slot is a receive slot or
 * If the slot is a localisation slot in RX or
 * A localisation slot in TX with a unicast link. */
int
is_active_timeslot(struct tsch_packet *p, struct tsch_neighbor *n,
              struct tsch_link *link)
{
  return p != NULL ||
       (link->link_options & LINK_OPTION_RX) ||
       (link->link_options & LINK_OPTION_TX && link->link_type == LINK_TYPE_PROP) ||
       (link->link_options & LINK_OPTION_TX && link->link_type == LINK_TYPE_PROP_MTM) ||
       (link->link_options & LINK_OPTION_TX && link->link_type == LINK_TYPE_CHORUS);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(tsch_tx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * TX slot:
   * 1. Copy packet to radio buffer
   * 2. Perform CCA if enabled
   * 3. Sleep until it is time to transmit
   * 4. Wait for ACK if it is a unicast packet
   * 5. Extract drift if we received an E-ACK from a time source neighbor
   * 6. Update CSMA parameters according to TX status
   * 7. Schedule mac_call_sent_callback
   **/

  /* tx status */
  static uint8_t mac_tx_status;
  /* is the packet in its neighbor's queue? */
  uint8_t in_queue;
  static int dequeued_index;
  static int packet_ready = 1;

  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();

  /* First check if we have space to store a newly dequeued packet (in case of
   * successful Tx or Drop) */
  dequeued_index = ringbufindex_peek_put(&dequeued_ringbuf);
  if(dequeued_index != -1) {
    if(current_packet == NULL || current_packet->qb == NULL) {
      mac_tx_status = MAC_TX_ERR_FATAL;
    } else {
      /* packet payload */
      static void *packet;
#if LLSEC802154_ENABLED
      /* encrypted payload */
      static uint8_t encrypted_packet[TSCH_PACKET_MAX_LEN];
#endif /* LLSEC802154_ENABLED */
      /* packet payload length */
      static uint8_t packet_len;
      /* packet seqno */
      static uint8_t seqno;
      /* is this a broadcast packet? (wait for ack?) */
      static uint8_t is_broadcast;
      static rtimer_clock_t tx_start_time;

#if CCA_ENABLED
      static uint8_t cca_status;
#endif

      /* get payload */
      packet = queuebuf_dataptr(current_packet->qb);
      packet_len = queuebuf_datalen(current_packet->qb);
      /* is this a broadcast packet? (wait for ack?) */
      is_broadcast = current_neighbor->is_broadcast;
      /* read seqno from payload */
      seqno = ((uint8_t *)(packet))[2];
      /* if this is an EB, then update its Sync-IE */
      if(current_neighbor == n_eb) {
        packet_ready = tsch_packet_update_eb(packet, packet_len, current_packet->tsch_sync_ie_offset);
      } else {
        packet_ready = 1;
      }

#if LLSEC802154_ENABLED
      if(tsch_is_pan_secured) {
        /* If we are going to encrypt, we need to generate the output in a separate buffer and keep
         * the original untouched. This is to allow for future retransmissions. */
        int with_encryption = queuebuf_attr(current_packet->qb, PACKETBUF_ATTR_SECURITY_LEVEL) & 0x4;
        packet_len += tsch_security_secure_frame(packet, with_encryption ? encrypted_packet : packet, current_packet->header_len,
            packet_len - current_packet->header_len, &tsch_current_asn);
        if(with_encryption) {
          packet = encrypted_packet;
        }
      }
#endif /* LLSEC802154_ENABLED */


      /* prepare packet to send: copy to radio buffer */
      if(packet_ready && NETSTACK_RADIO.prepare(packet, packet_len) == 0) { /* 0 means success */
        static rtimer_clock_t tx_duration;


#if CCA_ENABLED
        cca_status = 1;
        /* delay before CCA */
        TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, TS_CCA_OFFSET, "cca");
        TSCH_DEBUG_TX_EVENT();
        tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
        /* CCA */
        BUSYWAIT_UNTIL_ABS(!(cca_status |= NETSTACK_RADIO.channel_clear()),
                           current_slot_start, TS_CCA_OFFSET + TS_CCA);
        TSCH_DEBUG_TX_EVENT();
        /* there is not enough time to turn radio off */
        /*  NETSTACK_RADIO.off(); */
        if(cca_status == 0) {
          mac_tx_status = MAC_TX_COLLISION;
        } else
#endif /* CCA_ENABLED */
        {
          // /* delay before TX */
          // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_tx_offset] - RADIO_DELAY_BEFORE_TX, "TxBeforeTx");
          BUSYWAIT_UNTIL_ABS(0, current_slot_start, tsch_timing[tsch_ts_tx_offset] - RADIO_DELAY_BEFORE_TX);
          TSCH_DEBUG_TX_EVENT();
          /* send packet already in radio tx buffer */
          mac_tx_status = NETSTACK_RADIO.transmit(packet_len);
          /* Save tx timestamp */
          tx_start_time = current_slot_start + tsch_timing[tsch_ts_tx_offset];
          /* calculate TX duration based on sent packet len */
          tx_duration = TSCH_PACKET_DURATION(packet_len);
          /* limit tx_time to its max value */
          tx_duration = MIN(tx_duration, tsch_timing[tsch_ts_max_tx]);
          /* turn tadio off -- will turn on again to wait for ACK if needed */
          tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

          if(mac_tx_status == RADIO_TX_OK) {
            if(!is_broadcast) {
              uint8_t ackbuf[TSCH_PACKET_MAX_LEN];
              int ack_len;
              int is_time_source;
              struct ieee802154_ies ack_ies;
              uint8_t ack_hdrlen;
              frame802154_t frame;

#if TSCH_HW_FRAME_FILTERING
              radio_value_t radio_rx_mode;
              /* Entering promiscuous mode so that the radio accepts the enhanced ACK */
              NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
              NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode & (~RADIO_RX_MODE_ADDRESS_FILTER));
#endif /* TSCH_HW_FRAME_FILTERING */

#if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
        /* At the end of the transmission, get an more accurate estimate of SFD sending time */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_TX_PACKET_TIMESTAMP, &tx_start_time, sizeof(rtimer_clock_t));
#endif


              /* Unicast: wait for ack after tx: sleep until ack time */
              TSCH_SCHEDULE_AND_YIELD(pt, t, tx_start_time,
                  tx_duration + tsch_timing[tsch_ts_rx_ack_delay] - RADIO_DELAY_BEFORE_RX, "TxBeforeAck");
              TSCH_DEBUG_TX_EVENT();

              tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
              /* Wait for ACK to come */
              BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),
                  tx_start_time, tx_duration + tsch_timing[tsch_ts_rx_ack_delay] + tsch_timing[tsch_ts_ack_wait] + RADIO_DELAY_BEFORE_DETECT);
              TSCH_DEBUG_RX_EVENT();

              if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
                /* Wait for ACK to finish */
                BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
                    tx_start_time, tx_duration + tsch_timing[tsch_ts_rx_ack_delay] + tsch_timing[tsch_ts_ack_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
                TSCH_DEBUG_TX_EVENT();
              }
              tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

#if TSCH_HW_FRAME_FILTERING
              /* Leaving promiscuous mode */
              NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
              NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode | RADIO_RX_MODE_ADDRESS_FILTER);
#endif /* TSCH_HW_FRAME_FILTERING */

              /* Read ack frame */
              ack_len = NETSTACK_RADIO.read((void *)ackbuf, sizeof(ackbuf));


              is_time_source = 0;
              /* The radio driver should return 0 if no valid packets are in the rx buffer */
              if(ack_len > 0) {
                is_time_source = current_neighbor != NULL && current_neighbor->is_time_source;
                if(tsch_packet_parse_eack(ackbuf, ack_len, seqno,
                    &frame, &ack_ies, &ack_hdrlen) == 0) {
                  ack_len = 0;
                }

#if LLSEC802154_ENABLED
                if(ack_len != 0) {
                  if(!tsch_security_parse_frame(ackbuf, ack_hdrlen, ack_len - ack_hdrlen - tsch_security_mic_len(&frame),
                      &frame, &current_neighbor->addr, &tsch_current_asn)) {
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                        "!failed to authenticate ACK"));
                    ack_len = 0;
                  }
                } else {
                  TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                      "!failed to parse ACK"));
                }
#endif /* LLSEC802154_ENABLED */
              }

              if(ack_len != 0) {
                if(is_time_source) {


                  int32_t eack_time_correction = US_TO_RTIMERTICKS(ack_ies.ie_time_correction);
                  int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
                  if(eack_time_correction > SYNC_IE_BOUND) {
                    drift_correction = SYNC_IE_BOUND;
                  } else if(eack_time_correction < -SYNC_IE_BOUND) {
                    drift_correction = -SYNC_IE_BOUND;
                  } else {
                    drift_correction = eack_time_correction;
                  }
                  if(drift_correction != eack_time_correction) {
                    TSCH_LOG_ADD(tsch_log_message,
                        snprintf(log->message, sizeof(log->message),
                            "!truncated dr %d %d", (int)eack_time_correction, (int)drift_correction);
                    );
                  }
                  is_drift_correction_used = 1;
                  tsch_timesync_update(current_neighbor, since_last_timesync, drift_correction);
                  /* Keep track of sync time */
                  last_sync_asn = tsch_current_asn;
                  tsch_schedule_keepalive();


                }
                mac_tx_status = MAC_TX_OK;
              } else {
                mac_tx_status = MAC_TX_NOACK;
              }
            } else {
              mac_tx_status = MAC_TX_OK;
            }
          } else {
            mac_tx_status = MAC_TX_ERR;
          }
        }

        /* printf("Packet send %d, length %d, dest 0X%02X, slot %lu\n",
          mac_tx_status, packet_len,
          current_neighbor->addr.u8[sizeof(current_neighbor->addr)-1],
          tsch_current_asn.ls4b % TSCH_SCHEDULE_DEFAULT_LENGTH);

        if(mac_tx_status != MAC_TX_OK){
          printf("OK %d; NOACK %d, ERR %d\n", MAC_TX_OK, MAC_TX_NOACK, MAC_TX_ERR);
          // print_frame(packet_len, packet);
        }
        */
      }
    }

    tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

    current_packet->transmissions++;
    current_packet->ret = mac_tx_status;

    /* Post TX: Update neighbor state */
    in_queue = update_neighbor_state(current_neighbor, current_packet, current_link, mac_tx_status);

    /* The packet was dequeued, add it to dequeued_ringbuf for later processing */
    if(in_queue == 0) {
      dequeued_array[dequeued_index] = current_packet;
      ringbufindex_put(&dequeued_ringbuf);
    }

    /* Log every tx attempt */
    TSCH_LOG_ADD(tsch_log_tx,
        log->tx.mac_tx_status = mac_tx_status;
    log->tx.num_tx = current_packet->transmissions;
    log->tx.datalen = queuebuf_datalen(current_packet->qb);
    log->tx.drift = drift_correction;
    log->tx.drift_used = is_drift_correction_used;
    log->tx.is_data = ((((uint8_t *)(queuebuf_dataptr(current_packet->qb)))[0]) & 7) == FRAME802154_DATAFRAME;
#if LLSEC802154_ENABLED
    log->tx.sec_level = queuebuf_attr(current_packet->qb, PACKETBUF_ATTR_SECURITY_LEVEL);
#else /* LLSEC802154_ENABLED */
    log->tx.sec_level = 0;
#endif /* LLSEC802154_ENABLED */
    log->tx.dest = TSCH_LOG_ID_FROM_LINKADDR(queuebuf_addr(current_packet->qb, PACKETBUF_ADDR_RECEIVER));
    );

    /* Poll process for later processing of packet sent events and logs */
    process_poll(&tsch_pending_events_process);

  }

  TSCH_DEBUG_TX_EVENT();

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

#ifdef DEBUG_GPIO_TSCH
  // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");

#endif /* DEBUG_GPIO_TSCH */

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(tsch_rx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * RX slot:
   * 1. Check if it is used for TIME_KEEPING
   * 2. Sleep and wake up just before expected RX time (with a guard time: TS_LONG_GT)
   * 3. Check for radio activity for the guard time: TS_LONG_GT
   * 4. Prepare and send ACK if needed
   * 5. Drift calculated in the ACK callback registered with the radio driver. Use it if receiving from a time source neighbor.
   **/

  struct tsch_neighbor *n;
  static linkaddr_t source_address;
  static linkaddr_t destination_address;
  static int16_t input_index;
  static int input_queue_drop = 0;

  PT_BEGIN(pt);
  TSCH_DEBUG_RX_EVENT();

  input_index = ringbufindex_peek_put(&input_ringbuf);
  if(input_index == -1) {
    input_queue_drop++;
  } else {
    static struct input_packet *current_input;
    /* Estimated drift based on RX time */
    static int32_t estimated_drift;
    /* Rx timestamps */
    static rtimer_clock_t rx_start_time;
    static rtimer_clock_t expected_rx_time;
    static rtimer_clock_t packet_duration;
    uint8_t packet_seen;

    expected_rx_time = current_slot_start + tsch_timing[tsch_ts_tx_offset];
    /* Default start time: expected Rx time */
    rx_start_time = expected_rx_time;

    current_input = &input_array[input_index];


    /* Wait before starting to listen */
    TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");
    TSCH_DEBUG_RX_EVENT();

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();
    if(!packet_seen) {
      /* Check if receiving within guard time */
      BUSYWAIT_UNTIL_ABS((packet_seen = NETSTACK_RADIO.receiving_packet()),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
    }
    if(!packet_seen) {
      /* no packets on air */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);
    } else {
      TSCH_DEBUG_RX_EVENT();
      /* Save packet timestamp */
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      /* Wait until packet is received, turn radio off */
      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + tsch_timing[tsch_ts_max_tx]);
      TSCH_DEBUG_RX_EVENT();
      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()) {
        static int frame_valid;
        static int header_len;
        static frame802154_t frame;
        radio_value_t radio_last_rssi;


        /* Read packet */
        current_input->len = NETSTACK_RADIO.read((void *)current_input->payload, TSCH_PACKET_MAX_LEN);
        NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_RSSI, &radio_last_rssi);
        current_input->rx_asn = tsch_current_asn;
        current_input->rssi = (signed)radio_last_rssi;
        current_input->channel = current_channel;
        header_len = frame802154_parse((uint8_t *)current_input->payload, current_input->len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

#if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
        /* At the end of the reception, get an more accurate estimate of SFD arrival time */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
#endif

        packet_duration = TSCH_PACKET_DURATION(current_input->len);


#if LLSEC802154_ENABLED
        /* Decrypt and verify incoming frame */
        if(frame_valid) {
          if(tsch_security_parse_frame(
               current_input->payload, header_len, current_input->len - header_len - tsch_security_mic_len(&frame),
               &frame, &source_address, &tsch_current_asn)) {
            current_input->len -= tsch_security_mic_len(&frame);
          } else {
            TSCH_LOG_ADD(tsch_log_message,
                snprintf(log->message, sizeof(log->message),
                "!failed to authenticate frame %u", current_input->len));
            frame_valid = 0;
          }
        } else {
          TSCH_LOG_ADD(tsch_log_message,
              snprintf(log->message, sizeof(log->message),
              "!failed to parse frame %u %u", header_len, current_input->len));
          frame_valid = 0;
        }
#endif /* LLSEC802154_ENABLED */

        if(frame_valid) {
          if(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)) {
            int do_nack = 0;
            estimated_drift = RTIMER_CLOCK_DIFF(expected_rx_time, rx_start_time);

#if TSCH_TIMESYNC_REMOVE_JITTER
            /* remove jitter due to measurement errors */
            if(ABS(estimated_drift) <= TSCH_TIMESYNC_MEASUREMENT_ERROR) {
              estimated_drift = 0;
            } else if(estimated_drift > 0) {
              estimated_drift -= TSCH_TIMESYNC_MEASUREMENT_ERROR;
            } else {
              estimated_drift += TSCH_TIMESYNC_MEASUREMENT_ERROR;
            }
#endif

#ifdef TSCH_CALLBACK_DO_NACK
            if(frame.fcf.ack_required) {
              do_nack = TSCH_CALLBACK_DO_NACK(current_link,
                  &source_address, &destination_address);
            }
#endif

            if(frame.fcf.ack_required) {
              static uint8_t ack_buf[TSCH_PACKET_MAX_LEN];
              static int ack_len;

              /* Build ACK frame */
              ack_len = tsch_packet_create_eack(ack_buf, sizeof(ack_buf),
                  &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack);

              if(ack_len > 0) {
#if LLSEC802154_ENABLED
                if(tsch_is_pan_secured) {
                  /* Secure ACK frame. There is only header and header IEs, therefore data len == 0. */
                  ack_len += tsch_security_secure_frame(ack_buf, ack_buf, ack_len, 0, &tsch_current_asn);
                }
#endif /* LLSEC802154_ENABLED */


                /* Copy to radio buffer */
                NETSTACK_RADIO.prepare((const void *)ack_buf, ack_len);


                /* Wait for time to ACK and transmit ACK */
                TSCH_SCHEDULE_AND_YIELD(pt, t, rx_start_time,
                                        packet_duration + tsch_timing[tsch_ts_tx_ack_delay] - RADIO_DELAY_BEFORE_TX, "RxBeforeAck");
                TSCH_DEBUG_RX_EVENT();
                NETSTACK_RADIO.transmit(ack_len);
                tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);
              }
            }


            /* If the sender is a time source, proceed to clock drift compensation */
            n = tsch_queue_get_nbr(&source_address);
            if(n != NULL && n->is_time_source) {
              int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
              /* Keep track of last sync time */
              last_sync_asn = tsch_current_asn;
              /* Save estimated drift */
              drift_correction = -estimated_drift;
              is_drift_correction_used = 1;
              tsch_timesync_update(n, since_last_timesync, -estimated_drift);
              tsch_schedule_keepalive();
            }


            /* Add current input to ringbuf */
            ringbufindex_put(&input_ringbuf);


            /* Log every reception */
            TSCH_LOG_ADD(tsch_log_rx,
              log->rx.src = TSCH_LOG_ID_FROM_LINKADDR((linkaddr_t*)&frame.src_addr);
              log->rx.is_unicast = frame.fcf.ack_required;
              log->rx.datalen = current_input->len;
              log->rx.drift = drift_correction;
              log->rx.drift_used = is_drift_correction_used;
              log->rx.is_data = frame.fcf.frame_type == FRAME802154_DATAFRAME;
              log->rx.sec_level = frame.aux_hdr.security_control.security_level;
              log->rx.estimated_drift = estimated_drift;
            );
          }

          /* Poll process for processing of pending input and logs */
          process_poll(&tsch_pending_events_process);
        }
      }

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);
    }

    if(input_queue_drop != 0) {
      TSCH_LOG_ADD(tsch_log_message,
          snprintf(log->message, sizeof(log->message),
              "!queue full skipped %u", input_queue_drop);
      );
      input_queue_drop = 0;
    }
  }

  TSCH_DEBUG_RX_EVENT();


  // printf("RX packet from 0X%02X, slot %lu\n",
  //   source_address.u8[sizeof(source_address)-1],
  //   tsch_current_asn.ls4b % TSCH_SCHEDULE_DEFAULT_LENGTH);


  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

#ifdef DEBUG_GPIO_TSCH
  // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");

#endif /* DEBUG_GPIO_TSCH */

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
/* Protothread for slot operation, called from rtimer interrupt
 * and scheduled from tsch_schedule_slot_operation */
static
PT_THREAD(tsch_slot_operation(struct rtimer *t, void *ptr))
{
  TSCH_DEBUG_INTERRUPT();
  PT_BEGIN(&slot_operation_pt);

  static rtimer_clock_t rtimer_now;

  /* Loop over all active slots */
  while(tsch_is_associated) {
      #ifdef DEBUG_SYNC_LOGICAL
        PB3_TRIGGER();
        GPIO_SET_PIN(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN));
      #endif /* DEBUG_SYNC_LOGICAL*/
    if(current_link == NULL || tsch_lock_requested) { /* Skip slot operation if there is no link
                                                          or if there is a pending request for getting the lock */
      /* Issue a log whenever skipping a slot */
      TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "!skipped slot %u %u %u",
                            tsch_locked,
                            tsch_lock_requested,
                            current_link == NULL);
      );
    } else {
      int is_active_slot;
      tsch_in_slot_operation = 1;
      /* Reset drift correction */
      drift_correction = 0;
      is_drift_correction_used = 0;
      /* Get a packet ready to be sent */
      current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
      /* There is no packet to send, and this link does not have Rx flag. Instead of doing
       * nothing, switch to the backup link (has Rx flag) if any. */
      if(current_packet == NULL && !((current_link->link_options & LINK_OPTION_RX) ||
       (current_link->link_type == LINK_TYPE_PROP) || (current_link->link_type == LINK_TYPE_PROP_MTM) ||
       (current_link->link_type == LINK_TYPE_CHORUS)) && backup_link != NULL) {
        current_link = backup_link;
        current_packet = get_packet_and_neighbor_for_link(current_link, &current_neighbor);
      }
      /* Active slot if we have a packet to send or it is a receive slot or a localisation slot */
      is_active_slot = is_active_timeslot(current_packet, current_neighbor, current_link);
      if(is_active_slot) {
        #if TSCH_SLEEP
          /* Wake up the transceiver*/
          TSCH_SCHEDULE_AND_YIELD(&slot_operation_pt, t, current_slot_start-TSCH_SLOT_START_BEFOREHAND, US_TO_RTIMERTICKS(400), "wait");

          radio_value_t radio_state = RADIO_RESULT_NOT_SUPPORTED;

          if(NETSTACK_RADIO.get_value(RADIO_SLEEP_STATE, &radio_state) == RADIO_RESULT_OK){
                              TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "radio_state %d %d ",radio_state,RADIO_SLEEP);
                          );
            if(radio_state == (radio_value_t) RADIO_SLEEP){
              // printf("radio state sleep\n");
              if(NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_REQUEST_WAKEUP) == RADIO_RESULT_OK){
                    TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "Wake up requested");
                          );
                    rtimer_now = RTIMER_NOW();
                    TSCH_LOG_ADD(tsch_log_message,
                          snprintf(log->message, sizeof(log->message),
                              "Wake up ref time %lu, offset %ld, now %lu",
                              current_slot_start-TSCH_SLOT_START_BEFOREHAND,
                              US_TO_RTIMERTICKS(3000),
                              rtimer_now);
                          );

                TSCH_SCHEDULE_AND_YIELD(&slot_operation_pt, t, current_slot_start- US_TO_RTIMERTICKS(150), 0, "wait");


                NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_IDLE);
              }
            }
          }
        #else
          /* printf("current_slot_start %lu\n", current_slot_start); */
          /* TSCH_SCHEDULE_AND_YIELD(&slot_operation_pt, t, current_slot_start- US_TO_RTIMERTICKS(150), 0, "wait"); */
        #endif /* TSCH_SLEEP */
        TSCH_WAIT(&slot_operation_pt, t, current_slot_start, 0, "wait");
        TSCH_DEBUG_SLOT_START();

        /* Hop channel */
        current_channel = tsch_calculate_channel(&tsch_current_asn, current_link->channel_offset);
        NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, current_channel);
        /* Turn the radio on already here if configured so; necessary for radios with slow startup */
        tsch_radio_on(TSCH_RADIO_CMD_ON_START_OF_TIMESLOT);
        /* Decide whether it is a TX/RX/IDLE or OFF slot */
        /* Actual slot operation */
#if TSCH_LOCALISATION
        /* Is a localization slot and this localisation timeslot are enable ? */
        if(current_link->link_type == LINK_TYPE_PROP && tsch_is_prop_measurement_enable()){
          if(current_link->link_options & LINK_OPTION_TX){
            /*
              if(current_packet != NULL){
                printf("tsch_tx_prop_slot we have a packet to send :( \n");
              } */
              static struct pt slot_tx_loc_pt;
              PT_SPAWN(&slot_operation_pt, &slot_tx_loc_pt, tsch_tx_prop_slot(&slot_tx_loc_pt, t));
          }
          else if(current_link->link_options & LINK_OPTION_RX){
              static struct pt slot_rx_loc_pt;
              PT_SPAWN(&slot_operation_pt, &slot_rx_loc_pt, tsch_rx_prop_slot(&slot_rx_loc_pt, t));
          }
        }
        else
#endif /* TSCH_LOCALISATION */
#if TSCH_MTM_LOCALISATION
        if(current_link->link_type == LINK_TYPE_PROP_MTM && tsch_is_prop_measurement_enable()) {
          if(current_link->link_options & LINK_OPTION_TX) {
              static struct pt slot_mtm_tx_mtm_pt;
              PT_SPAWN(&slot_operation_pt, &slot_mtm_tx_mtm_pt, tsch_mtm_tx_slot(&slot_mtm_tx_mtm_pt, t));
          }
          else if(current_link->link_options & LINK_OPTION_RX) {
              static struct pt slot_mtm_rx_mtm_pt;
              PT_SPAWN(&slot_operation_pt, &slot_mtm_rx_mtm_pt, tsch_mtm_rx_slot(&slot_mtm_rx_mtm_pt, t));
          }
        }
        else
#endif /* TSCH_MTM_LOCALISATION */
#if TSCH_CHORUS
        /* Is a localization slot and this localisation timeslot are enable ? */
        if(current_link->link_type == LINK_TYPE_CHORUS){
          if(current_link->link_options & LINK_OPTION_TX){
            /*
              if(current_packet != NULL){
                printf("tsch_tx_prop_slot we have a packet to send :( \n");
              } */

            #if TSCH_CHORUS_NODE_TYPE == CHORUS_INITIATOR_NODE
              static struct pt slot_tx_chorus_pt;
              PT_SPAWN(&slot_operation_pt, &slot_tx_chorus_pt, tsch_chorus_initiator_slot(&slot_tx_chorus_pt, t));
            #endif /* TSCH_CHORUS_NODE_TYPE*/
          }
          else if(current_link->link_options & LINK_OPTION_RX){
            #if TSCH_CHORUS_NODE_TYPE == CHORUS_MOBILE_NODE
              /* mobile node */
              static struct pt slot_rx_chorus_pt;
              PT_SPAWN(&slot_operation_pt, &slot_rx_chorus_pt, tsch_chorus_mobile_slot(&slot_rx_chorus_pt, t));
            #elif TSCH_CHORUS_NODE_TYPE == CHORUS_ANCHORS_NODE
              static struct pt slot_rx_chorus_pt;
              PT_SPAWN(&slot_operation_pt, &slot_rx_chorus_pt, tsch_chorus_anchor_slot(&slot_rx_chorus_pt, t));
            #endif /* TSCH_CHORUS_NODE_TYPE */
          }
        }
        else
#endif /* TSCH_CHORUS */
          if(current_packet != NULL) {
          /* We have something to transmit, do the following:
           * 1. send
           * 2. update_backoff_state(current_neighbor)
           * 3. post tx callback
           **/
          static struct pt slot_tx_pt;
          PT_SPAWN(&slot_operation_pt, &slot_tx_pt, tsch_tx_slot(&slot_tx_pt, t));
        } else {
          /* Listen */
          static struct pt slot_rx_pt;
          PT_SPAWN(&slot_operation_pt, &slot_rx_pt, tsch_rx_slot(&slot_rx_pt, t));
        }
      }
      TSCH_DEBUG_SLOT_END();
    }


      #ifdef DEBUG_SYNC_LOGICAL
        PB3_TRIGGER();
      #endif /* DEBUG_SYNC_LOGICAL*/
    /* End of slot operation, schedule next slot or resynchronize */

    /* Do we need to resynchronize? i.e., wait for EB again */
    if(!tsch_is_coordinator && (TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn) >
        (100 * TSCH_CLOCK_TO_SLOTS(TSCH_DESYNC_THRESHOLD / 100, tsch_timing[tsch_ts_timeslot_length])))) {
      TSCH_LOG_ADD(tsch_log_message,
            snprintf(log->message, sizeof(log->message),
                "! leaving the network, last sync %u",
                          (unsigned)TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn));
      );

    printf("! leaving the network, last sync %u\n",
                          (unsigned)TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn));

      last_timesource_neighbor = NULL;
      tsch_disassociate();
    } else {
      /* backup of drift correction for printing debug messages */
      /* int32_t drift_correction_backup = drift_correction; */
      uint16_t timeslot_diff = 0;
      rtimer_clock_t prev_slot_start;
      /* Time to next wake up */
      rtimer_clock_t time_to_next_active_slot;
      /* Schedule next wakeup skipping slots if missed deadline */
      do {

      #ifdef DEBUG_SYNC_LOGICAL
        PB3_TRIGGER();
      #endif /* DEBUG_SYNC_LOGICAL*/

        if(current_link != NULL
            && current_link->link_options & LINK_OPTION_TX
            && current_link->link_options & LINK_OPTION_SHARED) {
          /* Decrement the backoff window for all neighbors able to transmit over
           * this Tx, Shared link. */
          tsch_queue_update_all_backoff_windows(&current_link->addr);
        }

        /* Get next active link */
        current_link = tsch_schedule_get_next_active_link(&tsch_current_asn, &timeslot_diff, &backup_link);
        if(current_link == NULL) {
          /* There is no next link. Fall back to default
           * behavior: wake up at the next slot. */
          timeslot_diff = 1;
        }
        /* Update ASN */
        TSCH_ASN_INC(tsch_current_asn, timeslot_diff);
        /* Time to next wake up */
        time_to_next_active_slot = timeslot_diff * tsch_timing[tsch_ts_timeslot_length] + drift_correction;
        drift_correction = 0;
        is_drift_correction_used = 0;
        /* Update current slot start */
        prev_slot_start = current_slot_start;
        current_slot_start += time_to_next_active_slot;
        current_slot_start += tsch_timesync_adaptive_compensate(time_to_next_active_slot);

                rtimer_now = RTIMER_NOW();
                TSCH_LOG_ADD(tsch_log_message,
                      snprintf(log->message, sizeof(log->message),
                          "slot time %lu, offset %ld, now %lu",
                          prev_slot_start,
                          time_to_next_active_slot-TSCH_SLOT_START_BEFOREHAND,
                          rtimer_now);
                      );

      } while(!tsch_schedule_slot_operation(t, prev_slot_start-TSCH_SLOT_START_BEFOREHAND, time_to_next_active_slot, "main"));
    }
      #ifdef DEBUG_SYNC_LOGICAL
        GPIO_CLR_PIN(GPIO_PORT_TO_BASE(DWM1000_PB3_PORT), GPIO_PIN_MASK(DWM1000_PB3_PIN));
      #endif /* DEBUG_SYNC_LOGICAL */
    tsch_in_slot_operation = 0;
    PT_YIELD(&slot_operation_pt);
  }
  PT_END(&slot_operation_pt);
}
/*---------------------------------------------------------------------------*/
#if TSCH_LOCALISATION
static
PT_THREAD(tsch_tx_prop_slot(struct pt *pt, struct rtimer *t))
{
  // printf("tsch_tx_prop_slot\n");
  /**
   * TX loc slot:
   * Double Sided Two Way Ranging
   * msg1  TX >> RX       step1    TX ok = step 2
   * msg2  TX << RX       step3
   * msg3  TX >> RX       step4    TX ok = step5
   * mqg4  RX << TX       step6
   *
   * 1. Copy packet (msg1) to the radio buffer
   * 2. Sleep until it is time to transmit msg1
   * 3. Transmit the msg1
   * 4. Save the transceiver TX time (msg1)
   * 5. Schedule a delayed reception (msg2)
   * 6. Wait until we received the response (msg2).
   * 7. Read the msg2
   * 8. Save the transceiver RX time (msg2)
   * 9. Prepare a response (msg3)
   * 10. Schedule a delayed transmission (msg3)
   * 11. Wait the end of the msg3.
   * 12. Save the TX time (msg3)
   * 13. Shedule a delayed reception (msg4)
   * 14. WAIT msg4
   * 15. Read the received packet (msg4):
   *     The packet contain informations to compute the ranging.
   * 16. Update Ranging parameters according to TX status
   * 17. Schedule mac_call_sent_callback
   **/

  /* tx status */
  static uint8_t mac_tx_status = MAC_TX_ERR;

  static uint8_t mac_status;

  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();

  /* First check if we have space to store a newly dequeued packet (in case of
   * successful Tx or Drop) */

  /* packet payload */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  /* packet payload length */
  static uint8_t packet_len;
  /* packet seqno */
  static uint8_t seqno;
  static rtimer_clock_t tx_start_time;

  // static uint16_t loc_reply_delay;

  /* timestamp of transmitted messages */
  static uint64_t timestamp_tx_m1, timestamp_rx_m2, timestamp_tx_m3;
  static uint8_t step = 0;

  seqno = 20;

  /* create payload */
  packet_len = tsch_packet_create_eack(packet_buf, TSCH_PACKET_MAX_LEN, &(current_link->addr), seqno, 0, 1);
  current_neighbor = tsch_queue_add_nbr(&(current_link->addr));

  /* Copy packet (msg1) to the radio buffer*/
  if(NETSTACK_RADIO.prepare(packet_buf, packet_len) == 0) { /* 0 means success */
    step = 1;
    static rtimer_clock_t tx_duration;
    /* Sleep until it is time to transmit msg1 */
    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_tx_offset] - RADIO_DELAY_BEFORE_TX, "msg1TX");


    TSCH_DEBUG_TX_EVENT();
    /* Transmit the msg1 */
    mac_status = NETSTACK_RADIO.transmit(packet_len);

    /* turn tadio off -- will turn on again to wait for ACK if needed */
    tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

    /* Save tx timestamp */
    tx_start_time = current_slot_start + tsch_timing[tsch_ts_loc_tx_offset];
    /* calculate TX duration based on sent packet len */
    tx_duration = TSCH_PACKET_DURATION(packet_len);
    /* limit tx_time to its max value */
    tx_duration = MIN(tx_duration, tsch_timing[tsch_ts_max_ack]);

    if(mac_status == RADIO_TX_OK) {
      // printf("tsch_tx_prop_slot mac_tx_status == RADIO_TX_OK\n");
      step = 2;
      rtimer_clock_t ack_start_time;
      int is_time_source;
      struct ieee802154_ies ack_ies;
      uint8_t ack_hdrlen;
      frame802154_t frame;
      // uint16_t rx_timeout_us;


      // rx_timeout_us = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_max_ack]+tsch_timing[tsch_ts_loc_uwb_t_shr])+TSCH_PACKET_DURATION_US(packet_len);
      // NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

      // /* Schedule a delayed reception (msg2)  */
      // loc_reply_delay = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_rx_reply_time])+TSCH_PACKET_DURATION_US(packet_len);
      // NETSTACK_RADIO.set_object(RADIO_LOC_RX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));


      #if TSCH_HW_FRAME_FILTERING
        radio_value_t radio_rx_mode;
        /* Entering promiscuous mode so that the radio accepts the enhanced ACK */
        NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
        NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode & (~RADIO_RX_MODE_ADDRESS_FILTER));
      #endif /* TSCH_HW_FRAME_FILTERING */

      /* Save the transceiver TX time (msg1) */
      #if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
        /* At the end of the transmission, get an more accurate estimate of SFD sending time */
        NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_TX_PACKET_TIMESTAMP, &tx_start_time, sizeof(rtimer_clock_t));
      #endif
      NETSTACK_RADIO.get_object(RADIO_LOC_LAST_TX_TIMESPTAMP, &timestamp_tx_m1, sizeof(uint64_t));

      /* Wait until we received the response (msg2) */
      TSCH_WAIT(pt, t, tx_start_time,
        tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] - RADIO_DELAY_BEFORE_RX, "waitRxMsg2");

      tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);


      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),tx_start_time,
        tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);

      ack_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
        /* Wait for ACK to finish */
        BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
            tx_start_time, tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
        TSCH_DEBUG_TX_EVENT();
      }

      /* turn tadio off -- will turn on needed */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()){
          // printf("rx message 2\n");
        step = 3;
        /*receive msg2 */
        #if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
          /* At the end of the reception, get an more accurate estimate of SFD receive time */
          NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &ack_start_time, sizeof(rtimer_clock_t));
        #endif
        /* Save the transceiver RX time (msg2) */
        NETSTACK_RADIO.get_object(RADIO_LOC_LAST_RX_TIMESPTAMP, &timestamp_rx_m2, sizeof(uint64_t));

        /* Read the msg2 */
        packet_len = NETSTACK_RADIO.read((void *) packet_buf, sizeof(packet_buf));

        is_time_source = 0;
        /* The radio driver should return 0 if no valid packets are in the rx buffer */
        if(packet_len > 0) {
          step = 4;

          is_time_source = current_neighbor != NULL && current_neighbor->is_time_source;
          if(tsch_packet_parse_eack(packet_buf, packet_len, seqno,
              &frame, &ack_ies, &ack_hdrlen) == 0) {
            packet_len = 0;
          }

          if(is_time_source) {
            int32_t eack_time_correction = US_TO_RTIMERTICKS(ack_ies.ie_time_correction);
            int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
            if(eack_time_correction > SYNC_IE_BOUND) {
              drift_correction = SYNC_IE_BOUND;
            } else if(eack_time_correction < -SYNC_IE_BOUND) {
              drift_correction = -SYNC_IE_BOUND;
            } else {
              drift_correction = eack_time_correction;
            }
            if(drift_correction != eack_time_correction) {
              TSCH_LOG_ADD(tsch_log_message,
                  snprintf(log->message, sizeof(log->message),
                      "!truncated dr %d %d", (int)eack_time_correction, (int)drift_correction);
              );
            }
            is_drift_correction_used = 1;
            tsch_timesync_update(current_neighbor, since_last_timesync, drift_correction);
            /* Keep track of sync time */
            last_sync_asn = tsch_current_asn;
            tsch_schedule_keepalive();
          }

          /* Schedule a delayed transmission (msg3) */
          // loc_reply_delay = (uint16_t) (RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_tx_reply_time])+TSCH_PACKET_DURATION_US(packet_len));
          // NETSTACK_RADIO.set_object(RADIO_LOC_TX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));


          tx_duration = TSCH_PACKET_DURATION(packet_len);
          /* Prepare a response (msg3) */
          packet_len = tsch_packet_create_ack(packet_buf, TSCH_PACKET_MAX_LEN, seqno);
          NETSTACK_RADIO.prepare(packet_buf, packet_len);

          /* Wait to transmit ms3 */

           TSCH_WAIT(pt, t, ack_start_time,
              tx_duration + tsch_timing[tsch_ts_loc_tx_reply_time] - RADIO_DELAY_BEFORE_TX, "txMsg3");

          /* Wait the end of the msg3. */
          /* send the message, the function wait the end of the transmition */
          mac_status = NETSTACK_RADIO.transmit(packet_len);
          // printf("mac_tx_status msg3 %d error %d\n", mac_tx_status, RADIO_TX_ERR);
          if(mac_status == RADIO_TX_OK){
            step = 5;
            // printf("tx msg3\n");
            /* Save the TX time (msg3)*/
            NETSTACK_RADIO.get_object(RADIO_LOC_LAST_TX_TIMESPTAMP, &timestamp_tx_m3, sizeof(uint64_t));

            /* At the end of the transmission, get an more accurate estimate of SFD sending time */
            NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_TX_PACKET_TIMESTAMP, &tx_start_time, sizeof(rtimer_clock_t));

            /* Shedule a delayed reception (msg4) */
            // loc_reply_delay = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_rx_reply_time])+TSCH_PACKET_DURATION_US(packet_len);
            // NETSTACK_RADIO.set_object(RADIO_LOC_RX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));


            /* Unicast: wait for ack after tx: sleep until ack time */
            TSCH_WAIT(pt, t, tx_start_time,
                tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] - RADIO_DELAY_BEFORE_RX, "waitMsg4");

            tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
            TSCH_DEBUG_TX_EVENT();

            /* Wait for msg4 to come */
            BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),
                tx_start_time, tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
            TSCH_DEBUG_TX_EVENT();

            if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
              /* Wait for ACK to finish */
              BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
                  tx_start_time, tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
              TSCH_DEBUG_TX_EVENT();
            }
            tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

            if(NETSTACK_RADIO.pending_packet()){
              step = 6;
              // printf("TX loc read last message\n");
              /* Read the received packet (msg4):
              *     The packet contain informations to compute the ranging. */
              packet_len = NETSTACK_RADIO.read((void *)packet_buf, sizeof(packet_buf));
              uint32_t replier_reply, replier_roundtrip;

              tsch_packet_parse_ranging_packet(packet_buf, packet_len, seqno, &frame,
                &replier_reply, &replier_roundtrip);
              // print_frame(packet_len, packet_buf);


              // printf("initiator_roundtrip %lld\n initiator_reply %lld\n replier_roundtrip%lu\n replier_reply%lu\n",
              //   timestamp_rx_m2-timestamp_tx_m1,
              //   timestamp_tx_m3-timestamp_rx_m2,
              //   replier_roundtrip, replier_reply);

              /* need to compute the distance and then send it to the upper layer */
              int32_t prop_time = compute_prop_time(timestamp_rx_m2-timestamp_tx_m1,
                timestamp_tx_m3-timestamp_rx_m2,
                replier_roundtrip, replier_reply);

              update_neighbor_prop_time(current_neighbor, prop_time, &tsch_current_asn, current_channel);
              // printf("update_neighbor_prop_time \n");
              mac_tx_status = MAC_TX_OK;

              // printf("TX localisation %ld ASN %u %lu channel %u\n", prop_time, tsch_current_asn.ms1b, tsch_current_asn.ls4b, current_channel);
            }
          }
        }
      } else {
        mac_tx_status = MAC_TX_NOACK;
      }
    } else {
      mac_tx_status = MAC_TX_ERR;
    }
    if(mac_tx_status != MAC_TX_OK){
      _PRINTF("TX localisation error at step %u ASN %u %u channel %u\n", step, tsch_current_asn.ms1b, tsch_current_asn.ls4b, current_channel);
    }

    tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

    uint16_t rx_timeout_us;
    rx_timeout_us = 0;
    NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));
  }

  TSCH_DEBUG_TX_EVENT();
  // printf("tsch_tx_prop_slot\n");

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  #ifdef DEBUG_GPIO_TSCH
    // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");
  #endif /* DEBUG_GPIO_TSCH */

  PT_END(pt);
}




/*---------------------------------------------------------------------------*/
static
PT_THREAD(tsch_rx_prop_slot(struct pt *pt, struct rtimer *t))
{
  _PRINTF("tsch_rx_prop_slot\n");
  /**
   * RX loc slot:
   * Double Sided Two Way Ranging
   * msg1  TX >> RX => Received      step1
   * msg2  TX << RX => ACK send      step2
   * msg3  TX >> RX => ACK receive   step3
   * mqg4  RX << TX => TX a ranging packet (contain some informations for the ranging computation) step4
   *
   * 1. Check if it is used for TIME_KEEPING
   * 2. Sleep and wake up just before expected RX time (with a shorter guard time: tsch_ts_loc_rx_guard)
   * 3. Check for radio activity for the guard time: TS_LONG_GT
   * 4. Read the msg1
   * 5. Save the transceiver RX time (msg1)
   ----
   * 6. Prepare an ACK (msg2)
   * 7. Schedule a delayed transmition (msg2)
   * 8. Wait the end of the msg2.
   * 9. Save the TX time (msg2)
   ---
   * 10. Schedule a delayed reception (msg3)
   * 11. Wait until we received the response (msg3).
   * 12. Read the msg3
   * 12. Save the transceiver RX time (msg3)
   ----
   * 14. Prepare the  ranging packet (msg4)
   * 15. Schedule a delayed transmition (msg4)
   * 16. Wait the end of the msg4.
   ---
   * 17. Drift calculated in the ACK callback registered with the radio driver. Use it if receiving from a time source neighbor.
   * 18. Place the transceiver in sleep mode to save energy
   **/

  struct tsch_neighbor *n;
  static linkaddr_t source_address;
  static linkaddr_t destination_address;
  /* packet payload */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  /* packet payload length */
  static uint8_t packet_len;

  static uint8_t mac_tx_status;

  /* timestamp of transmitted messages */
  static uint64_t timestamp_rx_m1, timestamp_tx_m2, timestamp_rx_m3;
  // static uint16_t loc_reply_delay;

  PT_BEGIN(pt);
  TSCH_DEBUG_RX_EVENT();

    /* Estimated drift based on RX time */
    static int32_t estimated_drift;
    /* Rx timestamps */
    static rtimer_clock_t rx_start_time;
    static rtimer_clock_t expected_rx_time;
    static rtimer_clock_t packet_duration;
    uint8_t packet_seen, previews_packet_len;
    uint8_t step = 0;

    expected_rx_time = current_slot_start + tsch_timing[tsch_ts_loc_tx_offset];
    /* Default start time: expected Rx time */
    rx_start_time = expected_rx_time;

    // printf("CURRENT TIME vs rx_offset %lu %lu \n", RTIMER_NOW(), current_slot_start+ tsch_timing[tsch_ts_loc_rx_offset]);
    /* Wait before starting to listen */
    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

    // uint64_t sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

    if(!packet_seen) {
      /* Check if receiving within guard time */
      BUSYWAIT_UNTIL_ABS((packet_seen = NETSTACK_RADIO.receiving_packet()),
          current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);


      // sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

      packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();
    }
    if(!packet_seen) {
      /* no packets on air */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);
      printf("no packet seen rx loc slot\n");
    }
    else
    {
      step = 1;
      TSCH_DEBUG_RX_EVENT();
      /* Save packet timestamp */
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      /* Wait until packet is received or we stop receiving a packet (error in the crc or sfd), turn radio off */
      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet() || !NETSTACK_RADIO.receiving_packet(),
          rx_start_time + RADIO_DELAY_BEFORE_DETECT, tsch_timing[tsch_ts_loc_uwb_t_shr] + tsch_timing[tsch_ts_max_tx] +  RADIO_DELAY_BEFORE_DETECT);
      TSCH_DEBUG_RX_EVENT();

          // if(!NETSTACK_RADIO.pending_packet()){
          //   uint64_t sys_status = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);
          //   printf("frame lost rx loc slot\n");
          //   print_sys_status(sys_status);

          //   printf("prev sys-status\n");
          //   print_sys_status(sys_status_init);
          // }

      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()) {
        static int frame_valid;
        static int header_len;
        static frame802154_t frame;
        static uint8_t seqno;
        seqno = random_rand();
        // uint16_t rx_timeout_us;

        /* Read the msg1 */
        packet_len= NETSTACK_RADIO.read((void *) packet_buf, TSCH_PACKET_MAX_LEN);
        header_len = frame802154_parse((uint8_t *)packet_buf, packet_len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);
        #if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
          /* At the end of the reception, get an more accurate estimate of SFD arrival time */
          NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
        #endif

        /* Save the transceiver RX time (msg1) */
        NETSTACK_RADIO.get_object(RADIO_LOC_LAST_RX_TIMESPTAMP, &timestamp_rx_m1, sizeof(uint64_t));


        if(!frame_valid)
          printf("error invalid frame rx loc slot\n");

        if(frame_valid) {
          step=2;
          /* printf("rangin step 2\n"); */
          if(!(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)))
              printf("error invalid addr rx loc slot 0X%02X%02X  0X%02X%02X\n",
                destination_address.u8[0],destination_address.u8[1],
                linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

          if(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)) {
            int do_nack = 0;
            estimated_drift = RTIMER_CLOCK_DIFF(expected_rx_time, rx_start_time);

            #if TSCH_TIMESYNC_REMOVE_JITTER
              /* remove jitter due to measurement errors */
              if(ABS(estimated_drift) <= TSCH_TIMESYNC_MEASUREMENT_ERROR) {
                estimated_drift = 0;
              } else if(estimated_drift > 0) {
                estimated_drift -= TSCH_TIMESYNC_MEASUREMENT_ERROR;
              } else {
                estimated_drift += TSCH_TIMESYNC_MEASUREMENT_ERROR;
              }
            #endif

            /* compute the reply time based on the first packet lenght */
            // loc_reply_delay = (uint16_t) (RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_tx_reply_time]+TSCH_PACKET_DURATION(packet_len)));
            previews_packet_len = packet_len;
            /* Build ACK frame */
            packet_len = tsch_packet_create_eack(packet_buf, sizeof(packet_buf),
                &source_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack);

            if(packet_len <= 0)
              printf("error packet len frame rx loc slot\n");
           if(packet_len > 0) {
              // printf("prepare to transmit %d\n", loc_reply_delay);

              // rx_timeout_us = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_max_tx]+tsch_timing[tsch_ts_loc_uwb_t_shr]);
              // NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

              /* Schedule a delayed transmission (msg2) */
              /* we give the delay between the reception of the frame and the transmission of the ACK */
              // NETSTACK_RADIO.set_object(RADIO_LOC_TX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));

              /* Copy to radio buffer */
              NETSTACK_RADIO.prepare((const void *)packet_buf, packet_len);


              TSCH_WAIT(pt, t, rx_start_time+TSCH_PACKET_DURATION(previews_packet_len), tsch_timing[tsch_ts_loc_tx_reply_time]-RADIO_DELAY_BEFORE_TX, "msg2TX");
              TSCH_DEBUG_TX_EVENT();

              /* Wait to transmit msg2. */
              mac_tx_status = NETSTACK_RADIO.transmit(packet_len);

              // printf("rx loc slot send msg 2\n");
                // printf("ranging test2 TX mac statut %u, %u, %u, %u, %u\n", mac_tx_status, RADIO_TX_OK, RADIO_TX_ERR, RADIO_TX_COLLISION, RADIO_TX_NOACK);
              if(mac_tx_status == RADIO_TX_OK){
                /* printf("ranging test3\n"); */
                /* Save the TX time (msg2)*/
                NETSTACK_RADIO.get_object(RADIO_LOC_LAST_TX_TIMESPTAMP, &timestamp_tx_m2, sizeof(uint64_t));

                /* At the end of the transmission, get an more accurate estimate of SFD sending time */
                NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_TX_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));

                /* Shedule a delayed reception (msg3) */
                // loc_reply_delay = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_rx_reply_time])+TSCH_PACKET_DURATION_US(packet_len);
                // NETSTACK_RADIO.set_object(RADIO_LOC_RX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));
                // tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

                packet_duration = TSCH_PACKET_DURATION(packet_len);

                /* Unicast: wait for ack after tx: sleep until ack time */
                TSCH_WAIT(pt, t, rx_start_time, packet_duration + tsch_timing[tsch_ts_loc_rx_reply_time] - RADIO_DELAY_BEFORE_RX , "wait msg3");

                tsch_radio_on(TSCH_RADIO_CMD_ON_FORCE);
                /* WAIT msg3 */

                /* Wait for ACK to come */
                BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),
                    rx_start_time, packet_duration + tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
                TSCH_DEBUG_RX_EVENT();

                if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
                  /* Wait for ACK to finish */
                  BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
                      rx_start_time + packet_duration, tsch_timing[tsch_ts_loc_rx_reply_time] + tsch_timing[tsch_ts_loc_rx_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
                  TSCH_DEBUG_TX_EVENT();
                }
                tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

                if(NETSTACK_RADIO.pending_packet()){
                  /* printf("ranging tesp 4\n"); */
                  packet_len =  NETSTACK_RADIO.read((void *)packet_buf, TSCH_PACKET_MAX_LEN);
                  /* Save the transceiver RX time (msg3) */
                  NETSTACK_RADIO.get_object(RADIO_LOC_LAST_RX_TIMESPTAMP, &timestamp_rx_m3, sizeof(uint64_t));


                  NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));

                  uint32_t t_reply, t_round;
                  t_round = timestamp_rx_m3 - timestamp_tx_m2;/* time between msg 2 and 3 */
                  t_reply = timestamp_tx_m2 - timestamp_rx_m1;/* time between msg1 and msg2 */

                  packet_duration = TSCH_PACKET_DURATION(packet_len);

                  // loc_reply_delay = (uint16_t) (RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_tx_reply_time])+TSCH_PACKET_DURATION_US(packet_len));

                  packet_len = tsch_packet_create_ranging_packet(packet_buf, TSCH_PACKET_MAX_LEN, &destination_address, seqno, t_reply, t_round);

                  /* Schedule a delayed transmission (msg4) */
                  // NETSTACK_RADIO.set_object(RADIO_LOC_TX_DELAYED_US, &loc_reply_delay, sizeof(uint16_t));

                  /* Prepare the  ranging packet (msg4) */
                  NETSTACK_RADIO.prepare((const void *)packet_buf, packet_len);

                  TSCH_WAIT(pt, t, rx_start_time,
                    packet_duration + tsch_timing[tsch_ts_loc_tx_reply_time] - RADIO_DELAY_BEFORE_TX, "Wait");

                  /* waitForTxmsg4 the end of the msg4. */
                  mac_tx_status = NETSTACK_RADIO.transmit(packet_len);
                  // printf("replier_roundtrip %lu ack_len %d\n replier_reply %lu\n", t_round, ack_len, t_reply);
                  if(mac_tx_status== RADIO_TX_OK){
                    // printf("rangin OK\n");
                  }
                }
              } /* end TX msg2 OK */
            }


            /* If the sender is a time source, proceed to clock drift compensation */
            n = tsch_queue_get_nbr(&source_address);
            // n=NULL;
            if(n != NULL && n->is_time_source) {
              int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
              /* Keep track of last sync time */
              last_sync_asn = tsch_current_asn;
              /* Save estimated drift */
              drift_correction = -estimated_drift;
              is_drift_correction_used = 1;
              tsch_timesync_update(n, since_last_timesync, -estimated_drift);
              tsch_schedule_keepalive();
            }
          }
        } /* end frame valid (msg1) */
      } /* end pending frame (msg1) */

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);
    }

  /* disable RX timeout feature */
  uint16_t rx_timeout_us;
  rx_timeout_us = 0;
  NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  TSCH_DEBUG_RX_EVENT();

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  #ifdef DEBUG_GPIO_TSCH
    // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");

  #endif /* DEBUG_GPIO_TSCH */

  // printf("tsch_rx_prop_slot\n");

  PT_END(pt);
}
#endif /* TSCH_LOCALISATION */


#if TSCH_MTM_LOCALISATION
static
PT_THREAD(tsch_mtm_tx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * MTM measurement slot:
   * Basically only broadcast one TX message for now
   * Optionally can be extended later by repeated
   * transmissions in order to derive more measurements.
   *
   **/

  /* tx status */
  static uint8_t mac_tx_status = MAC_TX_ERR;

  static uint8_t mac_status;

  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();
  QUICK_TOGGLE_GPIO();


  /* packet constructino */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  static uint8_t packet_len;
  static uint8_t seqno;

  #if MTM_SLOT_DURATIONS_EVAL
  static rtimer_clock_t slot_start_time, prepare_end_time, tx_start_time, tx_end_time, prop_handling_end_time;
  #endif

  static uint16_t mtm_delay_us;

  /* timestamp of transmitted messages */
  static uint64_t timestamp_tx;
  static uint16_t tx_antenna_delay;

#if MTM_SLOT_DURATIONS_EVAL
  slot_start_time = RTIMER_NOW();
#endif

  mtm_delay_us = RTIMERTICKS_TO_US(tsch_timing[tsch_ts_loc_tx_offset]);

#if TSCH_MTM_ADD_TX_JITTER
    mtm_delay_us += random_rand() % TSCH_MTM_ADD_TX_JITTER;
#endif

  /* printf("%u", RTIMERTICKS_TO_US(mtm_delay_us)); */

  /* Schedule a delayed transmission */
  NETSTACK_RADIO.set_object(RADIO_LOC_TX_DELAYED_US_MTM, &mtm_delay_us, sizeof(uint16_t));

  // calculate corrected timestamp
  NETSTACK_RADIO.get_object(RADIO_LOC_TX_ANTENNA_DELAY, &tx_antenna_delay, sizeof(uint16_t));

  timestamp_tx = dw_get_dx_timestamp() + tx_antenna_delay;
  /* timestamp_tx = (((uint64_t)(dw_get_dx_timestamp() & 0xFFFFFFFEUL)) << 8) + tx_antenna_delay; */
  /* create payload */
  packet_len = tsch_packet_create_multiranging_packet(packet_buf,
      TSCH_PACKET_MAX_LEN,
      &tsch_broadcast_address,
      seqno,
      timestamp_tx
  );


  current_neighbor = tsch_queue_add_nbr(&(current_link->addr));

  if(NETSTACK_RADIO.prepare(packet_buf, packet_len) == 0) { /* 0 means success */

#if MTM_SLOT_DURATIONS_EVAL
      prepare_end_time = RTIMER_NOW();
#endif

      QUICK_TOGGLE_GPIO();

    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_tx_offset] - RADIO_DELAY_BEFORE_TX, "msg1TX");

    TOGGLE_DEBUG_GPIO();

    TSCH_DEBUG_TX_EVENT();

#if MTM_SLOT_DURATIONS_EVAL
    tx_start_time = RTIMER_NOW();
#endif
    mac_status = NETSTACK_RADIO.transmit(packet_len);
#if MTM_SLOT_DURATIONS_EVAL
    tx_end_time = RTIMER_NOW();
#endif
    tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

    TOGGLE_DEBUG_GPIO();

    if(mac_status == RADIO_TX_OK) {
      // pass asn and timestamp
        add_mtm_transmission_timestamp(&tsch_current_asn, timestamp_tx);
    } else {
        add_mtm_transmission_timestamp(&tsch_current_asn, UINT64_MAX);
        _PRINTF("MTM: TX failed: 1\n");
    }

#if MTM_SLOT_DURATIONS_EVAL
    prop_handling_end_time = RTIMER_NOW();
#endif

    /* printf("ppl, %u\n", packet_len); */

    // last read out tx_timestamp and compare with the timestamp we set in the delayed send
    uint64_t reference_tx = 0x0;
    NETSTACK_RADIO.get_object(RADIO_LOC_LAST_TX_TIMESPTAMP, &reference_tx, sizeof(uint64_t));

#if MTM_SLOT_DURATIONS_EVAL
    /* printf("txd, %u, %u, %u, %u\n", */
    /*     RTIMERTICKS_TO_US(prepare_end_time) - RTIMERTICKS_TO_US(slot_start_time), RTIMERTICKS_TO_US(tx_start_time) - RTIMERTICKS_TO_US(prepare_end_time), RTIMERTICKS_TO_US(tx_end_time) - RTIMERTICKS_TO_US(tx_start_time), */
    /*     RTIMERTICKS_TO_US(prop_handling_end_time) - RTIMERTICKS_TO_US(tx_end_time)); */
#endif

    if(reference_tx != timestamp_tx) {
      printf("txmm\n");
    }
  } else {
     add_mtm_transmission_timestamp(&tsch_current_asn, UINT64_MAX);
     _PRINTF("MTM: TX failed: 2\n");
  }

  tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

  uint16_t rx_timeout_us;
  rx_timeout_us = 0;
  NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  mtm_slot_end_handler(current_link->timeslot);

  TSCH_DEBUG_TX_EVENT();

  #if TSCH_SLEEP
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP *///

  PT_END(pt);
}

static
PT_THREAD(tsch_mtm_rx_slot(struct pt *pt, struct rtimer *t))
{
  /**
   * MTM measurement slot:
   * Basically only listens for one TX message from the currently broadcasting node.
   * Optionally can be extended later by repeated transmissions in order to derive more measurements.
   *
   **/

  /* tx status */
  static uint8_t mac_tx_status = MAC_TX_ERR;
  static uint8_t mac_status;
  static struct tsch_neighbor *neighbor;

  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();
  QUICK_TOGGLE_GPIO();

  /* packet constructino */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  static uint8_t packet_len;
  /* static uint8_t seqno; */

  static rtimer_clock_t expected_rx_time, rx_start_time;
  static int32_t estimated_drift;

#if MTM_SLOT_DURATIONS_EVAL
  static rtimer_clock_t slot_start_time, eval_rx_start_time, eval_rx_end_time, prop_handling_end_time;
#endif

  /* timestamp of transmitted messages */
  /* We will denote as node A the virtual initiator node and with node B the virtual responder node */
  /* We will extract two timestamps, first the timestamp that was generated by the transmission that */
  /* we initiated when we took role of initiator and second the timestamp we generated upon taking */
  /* the role of the responder */
  static uint64_t timestamp_rx_A, timestamp_tx_B;
  static uint8_t packet_seen;

#if MTM_SLOT_DURATIONS_EVAL
    slot_start_time = RTIMER_NOW();
#endif


  expected_rx_time = current_slot_start + tsch_timing[tsch_ts_loc_rx_offset];

  /* TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen"); */
  TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");
  /* write_byte('r'); */


  TOGGLE_DEBUG_GPIO();

  tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

  packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

  if(!packet_seen) {
      /* Check if receiving within guard time */
      BUSYWAIT_UNTIL_ABS((packet_seen = NETSTACK_RADIO.receiving_packet()),
          current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);
  }

  TOGGLE_DEBUG_GPIO();

#if MTM_SLOT_DURATIONS_EVAL
  eval_rx_start_time = RTIMER_NOW();
#endif

  if(!packet_seen) {
      /* no packets on air */
      /* write_byte('s'); */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);
      _PRINTF("mtm missed\n");
      mtm_no_frame_detected++;
  } else {
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;
      TOGGLE_DEBUG_GPIO();

      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
          current_slot_start, tsch_timing[tsch_ts_rx_offset] + tsch_timing[tsch_ts_rx_wait] + tsch_timing[tsch_ts_max_tx]);

      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);
      TOGGLE_DEBUG_GPIO();


      if(NETSTACK_RADIO.pending_packet()) {
          static frame802154_t frame;
          static int header_len, frame_valid;
          static linkaddr_t source_address;
          static linkaddr_t destination_address;

          static struct mtm_packet_timestamp *rx_timestamps;
          static uint8_t num_rx_timestamps;

          timestamp_tx_B = 0;
          timestamp_rx_A = 0;

#if MTM_SLOT_DURATIONS_EVAL
          eval_rx_end_time = RTIMER_NOW();
#endif

          // get rx_timestamp
          NETSTACK_RADIO.get_object(RADIO_LOC_LAST_RX_TIMESPTAMP, &timestamp_rx_A, sizeof(uint64_t));

          packet_len = NETSTACK_RADIO.read((void *) packet_buf, TSCH_PACKET_MAX_LEN);
          header_len = frame802154_parse((uint8_t *)packet_buf, packet_len, &frame);

          frame_valid = header_len > 0 &&
              frame802154_check_dest_panid(&frame) &&
              frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

#if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
          /* At the end of the reception, get an more accurate estimate of SFD arrival time */
          NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
#endif

          // Ranging Related Management
          num_rx_timestamps = 0;
          rx_timestamps = NULL;

#if TSCH_MTM_REJECT_BY_FP_INDEX
          uint16_t fp_index = dw_get_fp_index();
          if((int32_t) 750 - ((int32_t) fp_index >> 6) > 40
              || (int32_t) 750 - ((int32_t) fp_index >> 6) < -40
              ) {
              frame_valid = 0;
          }
#endif


          // parse ranging packet
          if(frame_valid && tsch_packet_parse_multiranging_packet(packet_buf, packet_len, current_link->timeslot, &frame, &timestamp_tx_B, &rx_timestamps, &num_rx_timestamps)) {
              uint8_t neighbor = source_address.u8[LINKADDR_SIZE-1];
              uint8_t timeslot_offset = current_link->timeslot;
              add_to_direct_observed_rx_to_queue(timestamp_rx_A, neighbor, timeslot_offset);
              add_mtm_reception_timestamp(neighbor, &tsch_current_asn, timeslot_offset, timestamp_rx_A, timestamp_tx_B, rx_timestamps, num_rx_timestamps);
              mtm_successfull_frame_receptions++;
          } else {
              /* printf("mtm parse failed\n"); */
              mtm_failed_frame_receptions++;
          }

#if MTM_SLOT_DURATIONS_EVAL
          prop_handling_end_time = RTIMER_NOW();
#endif

#if MTM_SLOT_DURATIONS_EVAL
          printf("rxd, %u, %u, %u, %u, %u\n",
              RTIMERTICKS_TO_US(eval_rx_start_time) - RTIMERTICKS_TO_US(slot_start_time),       /// initial wait period
              RTIMERTICKS_TO_US(eval_rx_end_time) - RTIMERTICKS_TO_US(eval_rx_start_time),      // reception/transmission time
              RTIMERTICKS_TO_US(prop_handling_end_time) - RTIMERTICKS_TO_US(eval_rx_end_time),
              num_rx_timestamps,
              packet_len
              ); // processing time
#endif

          _PRINTF("--------------Finished----------------\n");

      } else {
          mtm_failed_frame_receptions++;
      }
  }
  tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

  uint16_t rx_timeout_us;
  rx_timeout_us = 0;
  NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  mtm_slot_end_handler(current_link->timeslot);

  #if TSCH_SLEEP
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  PT_END(pt);
}

#endif // TSCH_MTM_LOCALISATION



/*---------------------------------------------------------------------------*/
#if TSCH_CHORUS
static uint8_t cir[DW_LEN_ACC_MEM];

/* Read the Channel Inpulse Response of the
 * received message and write it on the serial line
 * Disable Accumulation memory ofter the read.
 * */
void tsch_chorus_output_anchors_cir(linkaddr_t* source_address){
  write_byte((uint8_t) '-');
  write_byte((uint8_t) 'R');
  write_byte((uint8_t) ':');
  write_byte(((uint8_t *)source_address)[6]);
  write_byte(((uint8_t *)source_address)[7]);
  write_byte((uint8_t) ':');
  uint16_t fp_index = dw_get_fp_index();
  write_byte((uint8_t) (fp_index >> 6) & 0XFF);
  write_byte((uint8_t) (fp_index >> (8+6)) & 0xFF );
  write_byte((uint8_t) (fp_index & 0x3F));
  // printf("fp_index %d %d\n", dw_is_lde_done(), fp_index>>6);
  write_byte((uint8_t) ':');


  uint16_t lde_threshold = dx_get_lde_threshold();
  write_byte((uint8_t) lde_threshold & 0xFF );
  write_byte((uint8_t) (lde_threshold >> 8) & 0XFF);

  write_byte((uint8_t) ':');

  /* Clear the memory (to check if data is write in the table later) */
  for( uint16_t i =0; i < DW_LEN_ACC_MEM; i++){
    cir[i] = 0;
  }

  /* read the ACC memory without using DMA */
  dw_read_CIR(cir);


  NETSTACK_RADIO.set_value(RADIO_ACCUMULATOR_MEMORY, RADIO_POWER_MODE_OFF);

  NETSTACK_RADIO.off();

  /* send to serial the contain of the ACC memory */
  for( uint16_t i =0; i < DW_LEN_ACC_MEM; i++){
    write_byte((uint8_t) cir[i]);
    watchdog_periodic(); /* avoid watchdog timer to be reach */
  }

  write_byte((uint8_t) '\n');
}

#if TSCH_CHORUS_NODE_TYPE == CHORUS_INITIATOR_NODE
static
PT_THREAD(tsch_chorus_initiator_slot(struct pt *pt, struct rtimer *t))
{
  // printf("tsch_tx_prop_slot\n");
  /**
   * Chorus Initiator TX Slot.
   * Step 1 Initiator Send a request.
   * Step 2 Initiator Receive the response and read the CIR
   *
   * 1. Copy packet (msg1) to the radio buffer
   * 2. Sleep until it is time to transmit msg1
   * 3. Transmit the msg1
   * 4. Save the transceiver TX time (msg1)
   * 5. Schedule a delayed reception (msg2)
   * 6. Wait until we received the response (msg2).
   * 7. Read the msg2
   * 8. Display the CIR
   * 17. Schedule mac_call_sent_callback
   **/

  /* tx status */
  static uint8_t mac_tx_status = MAC_TX_ERR;

  static uint8_t mac_status;

  static linkaddr_t destination_address;
  PT_BEGIN(pt);

  TSCH_DEBUG_TX_EVENT();

  /* First check if we have space to store a newly dequeued packet (in case of
   * successful Tx or Drop) */

  /* packet payload */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  /* packet payload length */
  static uint8_t packet_len;
  /* packet seqno */
  static uint8_t seqno;
  static rtimer_clock_t tx_start_time;

  seqno = 20;

  /* create payload */
  packet_len = tsch_packet_create_eack(packet_buf, TSCH_PACKET_MAX_LEN, &(current_link->addr), seqno, 0, 1);
  current_neighbor = tsch_queue_add_nbr(&(current_link->addr));

  /* Copy packet (msg1) to the radio buffer*/
  if(NETSTACK_RADIO.prepare(packet_buf, packet_len) == 0) { /* 0 means success */
    static rtimer_clock_t tx_duration;
    /* Sleep until it is time to transmit msg1 */
    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_tx_offset] - RADIO_DELAY_BEFORE_TX, "msg1TX");


    TSCH_DEBUG_TX_EVENT();
    /* Transmit the msg1 */
    mac_status = NETSTACK_RADIO.transmit(packet_len);
    // printf("Initiator TX status %d %d\n", mac_status, packet_len);
    // PRINTADDR(&(current_link->addr));

    /* turn tadio off -- will turn on again to wait for ACK if needed */
    tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

    /* Save tx timestamp */
    tx_start_time = current_slot_start + tsch_timing[tsch_ts_loc_tx_offset];
    /* calculate TX duration based on sent packet len */
    tx_duration = TSCH_PACKET_DURATION(packet_len);
    /* limit tx_time to its max value */
    tx_duration = MIN(tx_duration, tsch_timing[tsch_ts_max_ack]);

    if(mac_status == RADIO_TX_OK) {
      // printf("tsch_tx_prop_slot mac_tx_status == RADIO_TX_OK\n");

      rtimer_clock_t rx_timeout_us = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_max_ack]+tsch_timing[tsch_ts_loc_uwb_t_shr])+TSCH_PACKET_DURATION_US(packet_len);
      NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

      /* Schedule a delayed reception (anchors responses)  */
      uint16_t chorus_reply_time = (uint16_t) TSCH_CHORUS_REPLY_TIME - RTIMERTICKS_TO_US( tsch_timing[tsch_ts_loc_uwb_t_shr]);
      NETSTACK_RADIO.set_object(RADIO_LOC_RX_DELAYED_US, &chorus_reply_time, sizeof(uint16_t));


      #if TSCH_HW_FRAME_FILTERING
        radio_value_t radio_rx_mode;
        /* Entering promiscuous mode so that the radio accepts the enhanced ACK */
        NETSTACK_RADIO.get_value(RADIO_PARAM_RX_MODE, &radio_rx_mode);
        NETSTACK_RADIO.set_value(RADIO_PARAM_RX_MODE, radio_rx_mode & (~RADIO_RX_MODE_ADDRESS_FILTER));
      #endif /* TSCH_HW_FRAME_FILTERING */

      /* Wait until we received the response (msg2) */
      // TSCH_WAIT(pt, t, tx_start_time,
      //   tx_duration + tsch_timing[tsch_ts_loc_rx_reply_time] - RADIO_DELAY_BEFORE_RX, "waitRxMsg2");

      // tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);


      /* Enable CIR memory */
      NETSTACK_RADIO.set_value(RADIO_ACCUMULATOR_MEMORY, RADIO_POWER_MODE_ON);

      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),tx_start_time,
        tx_duration + US_TO_RTIMERTICKS(TSCH_CHORUS_REPLY_TIME) + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);

      if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
        /* Wait for ACK to finish */
        BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
            tx_start_time, tx_duration + US_TO_RTIMERTICKS(TSCH_CHORUS_REPLY_TIME) + tsch_timing[tsch_ts_loc_rx_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
        TSCH_DEBUG_TX_EVENT();
      }

      /* turn tadio off -- will turn on needed */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()){
        // printf("rx response message\n");
        frame802154_t frame;
        /* Read packet */
        packet_len = NETSTACK_RADIO.read((void *) packet_buf, sizeof(packet_buf));


        linkaddr_t source_address;
        frame802154_parse((uint8_t *) packet_buf, packet_len, &frame);
        frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

        /* Read the CIR, output it and disable the CIR memory */
        tsch_chorus_output_anchors_cir(&source_address);

        // printf("Source addr : ");
        // PRINTADDR(&source_address);
        // PRINTADDR(&destination_address);
        // printf("\n");

        // print_frame(packet_len, (uint8_t *) packet_buf);

        // dw_read_reg(DW_REG_RX_BUFFER, sizeof(packet_buf), (uint8_t *)packet_buf);
        // print_buf("  ", packet_buf, sizeof(packet_buf));
        mac_tx_status = MAC_TX_OK;
      } else {
        mac_tx_status = MAC_TX_NOACK;
      }
    } else {
      mac_tx_status = MAC_TX_ERR;

      printf("TSCH Initiator TX error\n");
    }
    if(mac_tx_status != MAC_TX_OK){
      printf("TSCH Initiator slot error\n");
    }
    tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);

    uint16_t rx_timeout_us;
    rx_timeout_us = 0;
    NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  }


  TSCH_DEBUG_TX_EVENT();
  // printf("tsch_tx_prop_slot\n");

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  #ifdef DEBUG_GPIO_TSCH
    // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");
  #endif /* DEBUG_GPIO_TSCH */

  PT_END(pt);
}
#endif /* TSCH_CHORUS_NODE_TYPE */
/*---------------------------------------------------------------------------*/

#if TSCH_CHORUS_NODE_TYPE == CHORUS_MOBILE_NODE
static
PT_THREAD(tsch_chorus_mobile_slot(struct pt *pt, struct rtimer *t))
{
  // printf("tsch_rx_prop_slot\n");
  /**
   * Chorus Mobile RX Slot.
   * Step 1 Receive Initiator request.
   * Step 2 Receive anchors responses and read the CIR
   *
   * 1. Check if it is used for TIME_KEEPING
   * 2. Sleep and wake up just before expected RX time (with a shorter guard time: tsch_ts_loc_rx_guard)
   * 3. Check for radio activity for the guard time: TS_LONG_GT
   * 4. Read the Initiator Request
   * 5. Correct Clock Drift
   ----
   * 10. Schedule a delayed reception (Anchors responses)
   * 11. Wait until we received the Anchors responses.
   * 12. Read the Anchors responses
   * 12. Show the CIR
   ---
   * 17. Drift calculated in the ACK callback registered with the radio driver. Use it if receiving from a time source neighbor.
   * 18. Place the transceiver in sleep mode to save energy
   **/

struct tsch_neighbor *n;
  static linkaddr_t source_address;
  static linkaddr_t destination_address;
  /* packet payload */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  /* packet payload length */
  static uint8_t packet_len;

  /* chorus reply time */
  static uint16_t reply_time;

  PT_BEGIN(pt);
  TSCH_DEBUG_RX_EVENT();

    /* Estimated drift based on RX time */
    static int32_t estimated_drift;
    /* Rx timestamps */
    static rtimer_clock_t rx_start_time;
    static rtimer_clock_t expected_rx_time;
    uint8_t packet_seen;

    expected_rx_time = current_slot_start + tsch_timing[tsch_ts_loc_tx_offset];
    /* Default start time: expected Rx time */
    rx_start_time = expected_rx_time;

    // printf("CURRENT TIME vs rx_offset %lu %lu \n", RTIMER_NOW(), current_slot_start+ tsch_timing[tsch_ts_loc_rx_offset]);
    /* Wait before starting to listen */
    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

    // uint64_t sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

    if(!packet_seen) {
      /* Check if receiving within guard time */
      BUSYWAIT_UNTIL_ABS((packet_seen = NETSTACK_RADIO.receiving_packet()),
          current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);


      // sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

      packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();
    }
    if(!packet_seen) {
      /* no packets on air */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);
      // printf("no packet seen rx loc slot\n");
    }
    else
    {
      TSCH_DEBUG_RX_EVENT();
      /* Save packet timestamp */
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      /* Wait until packet is received or we stop receiving a packet (error in the crc or sfd), turn radio off */
      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet() || !NETSTACK_RADIO.receiving_packet(),
          rx_start_time + RADIO_DELAY_BEFORE_DETECT, tsch_timing[tsch_ts_loc_uwb_t_shr] + tsch_timing[tsch_ts_max_tx] +  RADIO_DELAY_BEFORE_DETECT);
      TSCH_DEBUG_RX_EVENT();

          // if(!NETSTACK_RADIO.pending_packet()){
          //   uint64_t sys_status = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);
          //   printf("frame lost rx loc slot\n");
          //   print_sys_status(sys_status);

          //   printf("prev sys-status\n");
          //   print_sys_status(sys_status_init);
          // }

      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()) {
        static int frame_valid;
        static int header_len;
        static frame802154_t frame;

        /* Read Initiator request */
        packet_len= NETSTACK_RADIO.read((void *) packet_buf, TSCH_PACKET_MAX_LEN);
        header_len = frame802154_parse((uint8_t *)packet_buf, packet_len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);
        #if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
          /* At the end of the reception, get an more accurate estimate of SFD arrival time */
          NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
        #endif

        if(!frame_valid)
          printf("error invalid frame rx chorus mobile\n");

        if(frame_valid) {
          // printf("rangin step 2\n");
          if(!(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)))
              printf("error invalid addr rx loc slot 0X%02X%02X  0X%02X%02X\n",
                destination_address.u8[0],destination_address.u8[1],
                linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

          if(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)) {
            estimated_drift = RTIMER_CLOCK_DIFF(expected_rx_time, rx_start_time);

            #if TSCH_TIMESYNC_REMOVE_JITTER
              /* remove jitter due to measurement errors */
              if(ABS(estimated_drift) <= TSCH_TIMESYNC_MEASUREMENT_ERROR) {
                estimated_drift = 0;
              } else if(estimated_drift > 0) {
                estimated_drift -= TSCH_TIMESYNC_MEASUREMENT_ERROR;
              } else {
                estimated_drift += TSCH_TIMESYNC_MEASUREMENT_ERROR;
              }
            #endif

            /* compute the reply time based on the first packet lenght */
            reply_time = (uint16_t) TSCH_CHORUS_REPLY_TIME - RTIMERTICKS_TO_US( tsch_timing[tsch_ts_loc_uwb_t_shr]);

            rtimer_clock_t rx_timeout_us = (uint16_t) RTIMERTICKS_TO_US(tsch_timing[tsch_ts_max_tx]+tsch_timing[tsch_ts_loc_uwb_t_shr]);
            NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

            /* Schedule a delayed reception (anchors response) */
            /* we give the delay between the reception of the frame and the transmission of the ACK */
            NETSTACK_RADIO.set_object(RADIO_CHORUS_RX_DELAYED_US, &reply_time, sizeof(uint16_t));

            /* Enable Accumulator memory to be able to read the CIR after the reception of the response */
            NETSTACK_RADIO.set_value(RADIO_ACCUMULATOR_MEMORY, RADIO_POWER_MODE_ON);

            /* If the sender is a time source, proceed to clock drift compensation */
            n = tsch_queue_get_nbr(&source_address);
            if(n != NULL && n->is_time_source) {
              int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
              /* Keep track of last sync time */
              last_sync_asn = tsch_current_asn;
              /* Save estimated drift */
              drift_correction = -estimated_drift;
              is_drift_correction_used = 1;
              tsch_timesync_update(n, since_last_timesync, -estimated_drift);
              tsch_schedule_keepalive();
            }
                // tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

            /* Wait until we received the response (msg2) */
            TSCH_WAIT(pt, t, rx_start_time,
              TSCH_PACKET_DURATION(packet_len) +  US_TO_RTIMERTICKS(TSCH_CHORUS_REPLY_TIME) - RADIO_DELAY_BEFORE_RX, "waitRxMsg2");

            // tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);

            BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.receiving_packet(),rx_start_time,
              TSCH_PACKET_DURATION(packet_len) + US_TO_RTIMERTICKS(TSCH_CHORUS_REPLY_TIME) + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);

            if(NETSTACK_RADIO.receiving_packet() && !NETSTACK_RADIO.pending_packet()){
              /* Wait for ACK to finish */
              BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet(),
                  rx_start_time, TSCH_PACKET_DURATION(packet_len) + US_TO_RTIMERTICKS(TSCH_CHORUS_REPLY_TIME) + tsch_timing[tsch_ts_loc_rx_wait]+ tsch_timing[tsch_ts_max_ack] + RADIO_DELAY_BEFORE_DETECT);
              TSCH_DEBUG_TX_EVENT();
            }


            /* turn tadio off -- will turn on needed */
            tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);


            if(NETSTACK_RADIO.pending_packet()){
              // printf("rx response message\n");
              static frame802154_t frame;
              /* Read packet */
              packet_len = NETSTACK_RADIO.read((void *) packet_buf, sizeof(packet_buf));

              header_len = frame802154_parse((uint8_t *) packet_buf, packet_len, &frame);
              frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

              /* Read the CIR, output it and disable the CIR memory */
              tsch_chorus_output_anchors_cir(&source_address);
            }
          }
        } /* end frame valid (msg1) */
      } /* end pending frame (msg1) */

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);
    }

  /* disable RX timeout feature */
  uint16_t rx_timeout_us;
  rx_timeout_us = 0;
  NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  TSCH_DEBUG_RX_EVENT();

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  #ifdef DEBUG_GPIO_TSCH
    // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");

  #endif /* DEBUG_GPIO_TSCH */

  // printf("tsch_rx_prop_slot\n");

  PT_END(pt);
}
#endif /* TSCH_CHORUS_NODE_TYPE */
/*---------------------------------------------------------------------------*/
#if TSCH_CHORUS_NODE_TYPE == CHORUS_ANCHORS_NODE
static
PT_THREAD(tsch_chorus_anchor_slot(struct pt *pt, struct rtimer *t))
{
  // printf("tsch_chorus_anchor_slot\n");
  /**
   * Chorus Anchors Slot.
   * Step 1 Receive Initiator request.
   * Step 2 Transmit anchors responses
   *
   * 1. Check if it is used for TIME_KEEPING
   * 2. Sleep and wake up just before expected RX time (with a shorter guard time: tsch_ts_loc_rx_guard)
   * 3. Check for radio activity for the guard time: TS_LONG_GT
   * 4. Read the Initiator Request
   * 5. Correct Clock Drift
   ----
   * 10. Schedule a delayed transmission (Anchors responses)
   * 11. Wait until we transmit the Anchors responses.
   ---
   * 17. Drift calculated in the ACK callback registered with the radio driver. Use it if receiving from a time source neighbor.
   * 18. Place the transceiver in sleep mode to save energy
   **/

  struct tsch_neighbor *n;
  static linkaddr_t source_address;
  static linkaddr_t destination_address;
  /* packet payload */
  static uint8_t packet_buf[TSCH_PACKET_MAX_LEN];
  /* packet payload length */
  static uint8_t packet_len, response_len;

  PT_BEGIN(pt);
  TSCH_DEBUG_RX_EVENT();

    /* Estimated drift based on RX time */
    static int32_t estimated_drift;
    /* Rx timestamps */
    static rtimer_clock_t rx_start_time;
    static rtimer_clock_t expected_rx_time;
    uint8_t packet_seen;

    expected_rx_time = current_slot_start + tsch_timing[tsch_ts_loc_tx_offset];
    /* Default start time: expected Rx time */
    rx_start_time = expected_rx_time;

    // printf("CURRENT TIME vs rx_offset %lu %lu \n", RTIMER_NOW(), current_slot_start+ tsch_timing[tsch_ts_loc_rx_offset]);
    /* Wait before starting to listen */
    TSCH_WAIT(pt, t, current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] - RADIO_DELAY_BEFORE_RX, "RxBeforeListen");

    /* Start radio for at least guard time */
    tsch_radio_on(TSCH_RADIO_CMD_ON_WITHIN_TIMESLOT);
    packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();

    // uint64_t sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

    if(!packet_seen) {
      /* Check if receiving within guard time */
      BUSYWAIT_UNTIL_ABS((packet_seen = NETSTACK_RADIO.receiving_packet()),
          current_slot_start, tsch_timing[tsch_ts_loc_rx_offset] + tsch_timing[tsch_ts_loc_rx_wait] + RADIO_DELAY_BEFORE_DETECT);


      // sys_status_init = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);

      packet_seen = NETSTACK_RADIO.receiving_packet() || NETSTACK_RADIO.pending_packet();
    }
    if(!packet_seen) {
      /* no packets on air */
      tsch_radio_off(TSCH_RADIO_CMD_OFF_FORCE);
      // printf("no packet seen rx loc slot\n");
    }
    else
    {
      TSCH_DEBUG_RX_EVENT();
      /* Save packet timestamp */
      rx_start_time = RTIMER_NOW() - RADIO_DELAY_BEFORE_DETECT;

      /* Wait until packet is received or we stop receiving a packet (error in the crc or sfd), turn radio off */
      BUSYWAIT_UNTIL_ABS(NETSTACK_RADIO.pending_packet() || !NETSTACK_RADIO.receiving_packet(),
          rx_start_time + RADIO_DELAY_BEFORE_DETECT, tsch_timing[tsch_ts_loc_uwb_t_shr] + tsch_timing[tsch_ts_max_tx] +  RADIO_DELAY_BEFORE_DETECT);
      TSCH_DEBUG_RX_EVENT();

          // if(!NETSTACK_RADIO.pending_packet()){
          //   uint64_t sys_status = dw_read_reg_64(DW_REG_SYS_STATUS, DW_LEN_SYS_STATUS);
          //   printf("frame lost rx loc slot\n");
          //   print_sys_status(sys_status);

          //   printf("prev sys-status\n");
          //   print_sys_status(sys_status_init);
          // }

      tsch_radio_off(TSCH_RADIO_CMD_OFF_WITHIN_TIMESLOT);

      if(NETSTACK_RADIO.pending_packet()) {
        static int frame_valid;
        static int header_len;
        static frame802154_t frame;

        /* Read Initiator request */
        packet_len = NETSTACK_RADIO.read((void *) packet_buf, TSCH_PACKET_MAX_LEN);
        header_len = frame802154_parse((uint8_t *)packet_buf, packet_len, &frame);
        frame_valid = header_len > 0 &&
          frame802154_check_dest_panid(&frame) &&
          frame802154_extract_linkaddr(&frame, &source_address, &destination_address);
        #if TSCH_RESYNC_WITH_SFD_TIMESTAMPS
          /* At the end of the reception, get an more accurate estimate of SFD arrival time */
          NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &rx_start_time, sizeof(rtimer_clock_t));
        #endif

        if(!frame_valid)
          printf("error invalid frame rx loc slot\n");

        if(frame_valid) {
          // printf("rangin step 2\n");
          if(!(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)))
              printf("error invalid addr rx loc slot 0X%02X%02X  0X%02X%02X\n",
                destination_address.u8[0],destination_address.u8[1],
                linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

          if(linkaddr_cmp(&destination_address, &linkaddr_node_addr)
             || linkaddr_cmp(&destination_address, &linkaddr_null)) {
            estimated_drift = RTIMER_CLOCK_DIFF(expected_rx_time, rx_start_time);

            #if TSCH_TIMESYNC_REMOVE_JITTER
              /* remove jitter due to measurement errors */
              if(ABS(estimated_drift) <= TSCH_TIMESYNC_MEASUREMENT_ERROR) {
                estimated_drift = 0;
              } else if(estimated_drift > 0) {
                estimated_drift -= TSCH_TIMESYNC_MEASUREMENT_ERROR;
              } else {
                estimated_drift += TSCH_TIMESYNC_MEASUREMENT_ERROR;
              }
            #endif


            /* Schedule a delayed transmission (anchors response) */
            /* we give the delay between the reception of the frame and the transmission of the ACK */
            uint64_t delay_radio =  US_TO_RADIO(TSCH_CHORUS_REPLY_TIME) + (((NODEID - TSCH_CHORUS_FIRST_ANCHOR_ID)*(TSCH_CHORUS_TX_OFFSET/8)) << DW_TIMESTAMP_CLOCK_OFFSET);
            dw1000_schedule_tx_chorus(delay_radio);

            uint8_t do_nack = 0;
            linkaddr_copy(&destination_address, &tsch_broadcast_address);

            // Anchors response buffer
            static uint8_t anchor_resp_buff[TSCH_PACKET_MAX_LEN];
            response_len = tsch_packet_create_eack(anchor_resp_buff, sizeof(anchor_resp_buff), &destination_address, frame.seq, (int16_t)RTIMERTICKS_TO_US(estimated_drift), do_nack);
          // printf("Anchor response before upload to transceiver\n");
          // print_buf("  ", anchor_resp_buff, sizeof(anchor_resp_buff));

            if(response_len <= 0)
              printf("error creation of anchor response\n");
            if(response_len > 0) {
              // printf("prepare to transmit %d\n", loc_reply_delay);

              /* Copy to radio buffer */
              NETSTACK_RADIO.prepare((const void *)anchor_resp_buff, response_len);





              // TSCH_WAIT(pt, t, rx_start_time+TSCH_PACKET_DURATION(response_len), tsch_timing[tsch_ts_loc_tx_reply_time]-RADIO_DELAY_BEFORE_TX, "responseTX");
              // TSCH_DEBUG_TX_EVENT();

              /* Wait to transmit msg2. */
              if(!NETSTACK_RADIO.transmit(response_len)){
                printf("Error during transmission of the anchor response.\n");
              }
              printf("Delay %lld in ns : %lu\n", delay_radio, RADIO_TO_NS(delay_radio));


        // frame802154_parse((uint8_t *) anchor_resp_buff, response_len, &frame);
        // frame802154_extract_linkaddr(&frame, &source_address, &destination_address);

        // printf("Source addr : ");
        // PRINTADDR(&source_address);
        // PRINTADDR(&destination_address);
        // printf("\n");
        // print_frame(response_len, (uint8_t *) anchor_resp_buff);

        //   printf("Anchor response after upload to transceiver\n");
        //   static uint8_t read_anchor_resp_buff[TSCH_PACKET_MAX_LEN];

        // dw_read_reg(DW_REG_TX_BUFFER, sizeof(read_anchor_resp_buff), (uint8_t *)read_anchor_resp_buff);
        // print_buf("  ", read_anchor_resp_buff, sizeof(read_anchor_resp_buff));

            }
            /* If the sender is a time source, proceed to clock drift compensation */
            n = tsch_queue_get_nbr(&source_address);
            if(n != NULL && n->is_time_source) {
              int32_t since_last_timesync = TSCH_ASN_DIFF(tsch_current_asn, last_sync_asn);
              /* Keep track of last sync time */
              last_sync_asn = tsch_current_asn;
              /* Save estimated drift */
              drift_correction = -estimated_drift;
              is_drift_correction_used = 1;
              tsch_timesync_update(n, since_last_timesync, -estimated_drift);
              tsch_schedule_keepalive();
            }
          }
        } /* end frame valid (msg1) */
      } /* end pending frame (msg1) */

      tsch_radio_off(TSCH_RADIO_CMD_OFF_END_OF_TIMESLOT);
    }

  /* disable RX timeout feature */
  uint16_t rx_timeout_us;
  rx_timeout_us = 0;
  NETSTACK_RADIO.set_object(RADIO_RX_TIMEOUT_US, &rx_timeout_us, sizeof(uint16_t));

  TSCH_DEBUG_RX_EVENT();

  #if TSCH_SLEEP
    /* request to place the transceiver in SLEEP mode to reduce energy consumption */
    NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_SLEEP);
  #endif /* TSCH_SLEEP */

  #ifdef DEBUG_GPIO_TSCH
    // TSCH_SCHEDULE_AND_YIELD(pt, t, current_slot_start, tsch_timing[tsch_ts_timeslot_length], "Timeslot Lenght");

  #endif /* DEBUG_GPIO_TSCH */

  // printf("tsch_rx_prop_slot\n");

  PT_END(pt);
}
#endif /* TSCH_CHORUS_NODE_TYPE */
#endif /* TSCH_CHORUS */
/*---------------------------------------------------------------------------*/
/* Set global time before starting slot operation,
 * with a rtimer time and an ASN */
void
tsch_slot_operation_start(void)
{
  static struct rtimer slot_operation_timer;
  rtimer_clock_t time_to_next_active_slot;
  rtimer_clock_t prev_slot_start;
  TSCH_DEBUG_INIT();

  do {
    uint16_t timeslot_diff;

    _PRINTF("tsch_slot_operation_start\n");
    /* Get next active link */
    current_link = tsch_schedule_get_next_active_link(&tsch_current_asn, &timeslot_diff, &backup_link);
    if(current_link == NULL) {
      /* There is no next link. Fall back to default
       * behavior: wake up at the next slot. */
      timeslot_diff = 1;
    }
    /* Update ASN */
    TSCH_ASN_INC(tsch_current_asn, timeslot_diff);
    /* Time to next wake up */
    time_to_next_active_slot = timeslot_diff * tsch_timing[tsch_ts_timeslot_length];
    /* Update current slot start */
    prev_slot_start = current_slot_start;
    current_slot_start += time_to_next_active_slot;

    _PRINTF("current_slot_start : %lu\n", current_slot_start);
    _PRINTF("time_to_next_active_slot : %lu\n", time_to_next_active_slot);
    _PRINTF("prev_slot_start : %lu\n", prev_slot_start);

  } while(!tsch_schedule_slot_operation(&slot_operation_timer, prev_slot_start, time_to_next_active_slot, "association"));
}
/*---------------------------------------------------------------------------*/
/* Start actual slot operation */
void
tsch_slot_operation_sync(rtimer_clock_t next_slot_start,
    struct tsch_asn_t *next_slot_asn)
{
  current_slot_start = next_slot_start;
  tsch_current_asn = *next_slot_asn;
  last_sync_asn = tsch_current_asn;
  current_link = NULL;
}
/*---------------------------------------------------------------------------*/
