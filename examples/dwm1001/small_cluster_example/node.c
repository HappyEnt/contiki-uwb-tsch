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
 */
/**
 * \file
 *         A RPL+TSCH node able to act as either a simple node (6ln),
 *         DAG Root (6dr) or DAG Root with security (6dr-sec)
 *         Press use button at startup to configure.
 *
 * \author Simon Duquennoy <simonduq@sics.se>
 */

#include "apps/rand-sched/rand-sched.h"
#include "contiki.h"
#include "linkaddr.h"
#include "node-id.h"
#include "project-conf.h"
#include "tsch-schedule.h"
#include "dev/serial-line.h"

#include "mtm_control.h"
#include <stdint.h>

#define LOC_WITH_RPL 0
#if LOC_WITH_RPL
#include "net/ipv6/uip-ds6-route.h"
#include "net/rpl/rpl.h"
#include "net/rpl/rpl-private.h"
#include "net/nbr-table.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
#endif

#include "net/mac/tsch/tsch.h"

#include "tsch-prop.h"
#include <stdio.h>

#include "nodes.h"

#include "lib/random.h"
#include "dev/uart0.h"
#include "nrfx_log.h"

#include "leds.h"

#include "button-sensor.h"

#define WITH_LOC_RIME 1
#if WITH_LOC_RIME
#include "net/netstack.h"
#include "net/rime/rime.h"
#endif
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
PROCESS(TSCH_PROP_PROCESS, "TSCH localization User Process");


AUTOSTART_PROCESSES(&node_process, &sensors_process);

static uint32_t measurement_count = 0;

static struct ctimer delayed_start_timer;

/*---------------------------------------------------------------------------*/

static void
net_init(uint8_t is_coordinator)
{
      
  if(is_coordinator) {
      tsch_set_coordinator(1); // This would otherwise be done by tsch-rpl
  }

  NETSTACK_MAC.on();
}


#define LOG_CONFIG_ITEM(NAME) do { printf(";; %s = %d\n", #NAME, NAME); } while(0)

// useful for evaluation, print configuration into log, prefix with ;; for easy parsing
static void print_configuration() {
    printf("=Configuration=\n");
    
    LOG_CONFIG_ITEM(TSCH_CONF_DEFAULT_TIMESLOT_LENGTH);
    LOG_CONFIG_ITEM(TSCH_CONF_EB_PERIOD);
    LOG_CONFIG_ITEM(TSCH_CONF_MAX_EB_PERIOD);
    LOG_CONFIG_ITEM(TSCH_CONF_AUTOSELECT_TIME_SOURCE);
    LOG_CONFIG_ITEM(LINKADDR_SIZE);
    LOG_CONFIG_ITEM(LOC_WITH_RPL);        
    LOG_CONFIG_ITEM(PACKETBUF_SIZE);
    
    printf(";; SCHEDULE_USED = CUSTOM\n");
}

void output_range_via_serial_snprintf(uint8_t addr_short, float range, struct tsch_asn_t asn) {
    // use uart0_writeb(char byte) to write range
    char buffer[80];

    int length = snprintf(buffer, 80, "TW, %u, %u, "NRF_LOG_FLOAT_MARKER "\n", addr_short, asn.ls4b, NRF_LOG_FLOAT( range * 100 ));

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void uart_write_link_addr() {
    char buffer[40];
    int length = snprintf(buffer, 40, "TA, %u, %u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void output_tdoa_via_serial(ranging_addr_t node1_addr, ranging_addr_t node2_addr, float dist, struct tsch_asn_t asn) {
    // use uart0_writeb(char byte) to write range
    char buffer[80];

    int length = snprintf(buffer, 80, "TD, %u, %u, %u, " NRF_LOG_FLOAT_MARKER "\n", node1_addr, node2_addr, asn.ls4b, NRF_LOG_FLOAT(dist * 100));

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void start_experiment() {
    printf("start experiment\n");
    rand_sched_start();
    ctimer_stop(&delayed_start_timer);
}


PROCESS_THREAD(TSCH_PROP_PROCESS, ev, data)
{
  PROCESS_BEGIN();

  printf("custom tsch_loc_operation handler start\n");

  static struct distance_measurement *m = NULL;

  while(1) {
    PROCESS_YIELD();
    /* receive a new propagation time measurement */

    if(ev == PROCESS_EVENT_MSG) {
        m = (struct distance_measurement *) data;
        measurement_count++;
#if WITH_UART_OUTPUT_RANGE
        float dist = time_to_dist(m->time);
        struct tsch_asn_t asn = m->asn;

#if WITH_UART_OUTPUT_SKIP > 0
        if(measurement_count % WITH_UART_OUTPUT_SKIP != 0) {
            continue;
        }
#endif
        if(m->type == TWR) {
            ranging_addr_t addr_short = m->addr_B;
            output_range_via_serial_snprintf(addr_short, dist, asn);
        } else if (m->type == TDOA) {
            /* printf("TDOA between %u and %u is %d\n", m->addr_A, m->addr_B, m->time); */
            output_tdoa_via_serial(m->addr_A, m->addr_B, dist, asn);
        }
#endif
    }
  }

  PROCESS_END();
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer et;
  static struct etimer network_status_timer;
  static struct node_role_entry *role;
  PROCESS_BEGIN();
  
  leds_on(LEDS_ORANGE);
  
#define CONFIG_WAIT_TIME 1
  etimer_set(&et, CLOCK_SECOND * CONFIG_WAIT_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


  print_configuration();


  // wait again two second
  printf("=wait 1 seconds=\n");
  etimer_set(&et, CLOCK_SECOND * 1);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  role = get_node_role_entry(&linkaddr_node_addr);
  switch(role->role) {
  case MOBILE: {
      net_init(0);
      tsch_set_send_beacons(0);
      rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);      
      break;
  }
  case ANCHOR: {
      /* node_set_anchor(); */
      net_init(0);
      tsch_set_send_beacons(0);
      if(role->send_beacons) {
          tsch_set_send_beacons(1);
      }
      
      if(role->fixed_timeslot > 1) {  // timeslots 0 and 1 are reserved
          tsch_set_send_beacons(1);
      }
      
      rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);      
      break;
  }
  case ROOT: {
      net_init(1);
      tsch_set_send_beacons(1);
      rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);
      break;
  }
  }

  etimer_set(&network_status_timer, CLOCK_SECOND);

  // nodes: 29 + 39 + 38 + 45 + 35 + 34 + 33 + 30 + 26 + 43 + 41
  // biggest clique: 29 +  39 +  30 +  38 +  25 +  26 +  34 +  35 +  43 +  41 +  14 +  13 +  33 +  45 +  1 +  17 +  2 +  5 +  21 +  19 +  6 +  10 +  9 +  18
  // NOte 42, 37 do not receive serial input

  /* Print out routing tables every minute */
  etimer_set(&et, CLOCK_SECOND/4);
  while(1) {

      PROCESS_WAIT_EVENT_UNTIL(
          (ev == serial_line_event_message)
          || etimer_expired(&et)
          || etimer_expired(&network_status_timer)
      );

      if(etimer_expired(&network_status_timer)) {
          uart_write_link_addr();
#if WITH_UART_OUTPUT_COUNTS
          printf("m, %u\n", measurement_count);
#endif
          
          etimer_reset(&network_status_timer);
      }

      if(ev == serial_line_event_message) {
          char *serial_data = (char *)data;
          /* printf("received line: %s\n", serial_data); */

          if(serial_data[0] == 't') {
              // read number, skip one space
              int timeslot;
              sscanf(serial_data, "t %d", &timeslot);

              if(timeslot > UINT8_MAX || timeslot < 0) {
                  printf("invalid timeslot\n");
              }
              rand_sched_set_timeslot((uint8_t) timeslot);
          } else if (serial_data[0] == 's') {
              role = get_node_role_entry(&linkaddr_node_addr);              
              switch(role->role) {
              case MOBILE: {
                  rand_sched_stop();
                  rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);
                  rand_set_timeslot_fixed(0);
                  break;
              }
              case ANCHOR: {
                  /* node_set_anchor(); */
                  rand_sched_stop();
                  rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);


                  break;
              }
              case ROOT: {
                  rand_sched_stop();
                  rand_sched_init(EXPERIMENT_MTM_ROUND_LENGTH);
                  
                  break;
              }
              }

              switch(role->role) {
              case MOBILE: {
                  break;
              }
              case ANCHOR: {
                  /* node_set_anchor(); */
                  if(role->fixed_timeslot > 1) {  // timeslots 0 and 1 are reserved
                      rand_set_timeslot_fixed(1);
                      rand_sched_set_timeslot(role->fixed_timeslot);
                  }
                  break;
              }
              case ROOT: {
                  rand_set_timeslot_fixed(1); // timeslots 0 and 1 are reserved
                  rand_sched_set_timeslot(role->fixed_timeslot);
                  
                  break;
              }
              }
              
              printf("ts, 0\n");
              
              ctimer_set(&delayed_start_timer, CLOCK_SECOND * 4, start_experiment, NULL);
          }
      }

      // if tsch_is_associated add for each neighbor a ranging slot
      /* if(tsch_is_associated) { */
      /*     printf("tschass 1\n"); */
      /* } else { */
      /*     printf("tschass 0\n"); */
      /* } */

      if(etimer_expired(&et)) {
          etimer_reset(&et);
      }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
