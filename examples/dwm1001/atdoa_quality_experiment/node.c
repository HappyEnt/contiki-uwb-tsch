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

#include "contiki.h"
#include "linkaddr.h"
#include "node-id.h"
#include "project-conf.h"
#include "tsch-schedule.h"
#include "dev/serial-line.h"

#include <stdint.h>

#include "net/mac/tsch/tsch.h"
#include "net/netstack.h"

#include "tsch-prop.h"
#include <stdio.h>
#include "button-sensor.h"

#include "lib/random.h"
#include "dev/uart0.h"
#include "nrfx_log.h"

#include "leds.h"



/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
PROCESS(TSCH_PROP_PROCESS, "TSCH localization User Process");

AUTOSTART_PROCESSES(&node_process, &sensors_process);

/*---------------------------------------------------------------------------*/

static void
net_init(uint8_t is_coordinator)
{
  uip_ipaddr_t global_ipaddr;

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

void output_range_via_serial_snprintf(uint8_t addr_short, float range) {
    // use uart0_writeb(char byte) to write range
    char buffer[60];

    // we output data in units of cm
    int length = snprintf(buffer, 60, "TW, %u,"NRF_LOG_FLOAT_MARKER "\n", addr_short, NRF_LOG_FLOAT( range * 100) );

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void uart_write_link_addr() {
    char buffer[50];
    int length = snprintf(buffer, 50, "TA, %u, %u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void output_tdoa_via_serial(ranging_addr_t node1_addr, ranging_addr_t node2_addr, float dist) {
    // use uart0_writeb(char byte) to write range
    char buffer[70];

    int length = snprintf(buffer, 70, "TD, %u, %u, " NRF_LOG_FLOAT_MARKER "\n", node1_addr, node2_addr, NRF_LOG_FLOAT(dist*100));

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
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
#if WITH_UART_OUTPUT_RANGE
        float dist = time_to_dist(m->time);
        if(m->type == TWR) {
            ranging_addr_t addr_short = m->addr_B;
            output_range_via_serial_snprintf(addr_short, dist);
        } else if (m->type == TDOA) {
            /* printf("TDOA between %u and %u is %d\n", m->addr_A, m->addr_B, m->time); */
            output_tdoa_via_serial(m->addr_A, m->addr_B, dist);
        }
#endif
    }
  }

  PROCESS_END();
}

const linkaddr_t node_0_ll = { { 64, 29 } }; //dwm1001-29 */
const linkaddr_t node_1_ll = { { 66, 33 } }; //dwm1001-33 */
const linkaddr_t node_2_ll =  { { 89, 26 } }; // dwm1001-26 */

/* const linkaddr_t node_6_ll = { {  61, 196 } }; // dwm1001-7 */
/* const linkaddr_t node_7_ll = { {  106, 6 } }; // dwm1001-8 */

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer et;

  PROCESS_BEGIN();

#define CONFIG_WAIT_TIME 1
  etimer_set(&et, CLOCK_SECOND * CONFIG_WAIT_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  print_configuration();
  
  // wait again two second
  printf("=wait 1 seconds=\n");

    struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(0, 4);

    if (sf_eb != NULL) {
        // initial shared slot
        tsch_schedule_add_link(sf_eb,
            LINK_OPTION_TX | LINK_OPTION_RX | LINK_OPTION_SHARED | LINK_OPTION_TIME_KEEPING,
            LINK_TYPE_ADVERTISING, &tsch_broadcast_address, 0, 0);  
        if(linkaddr_cmp(&node_0_ll, &linkaddr_node_addr)) {
            printf("=node type 0 initiator=\n");
            tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 1, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 2, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 3, 0);
            net_init(1);                  
        } else if(linkaddr_cmp(&node_1_ll, &linkaddr_node_addr)) {
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 1, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 2, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 3, 0);
            net_init(0);      
        } else if(linkaddr_cmp(&node_2_ll, &linkaddr_node_addr)) {
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 1, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 2, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_TX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 3, 0);
            net_init(0);      
        } else {
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 1, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 2, 0);
            tsch_schedule_add_link(sf_eb, LINK_OPTION_RX, LINK_TYPE_PROP_MTM, &tsch_broadcast_address, 3, 0);
            net_init(0);
        }
    }
    
    mtm_set_round_end(3);    
  
  etimer_set(&et, CLOCK_SECOND * 1);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  etimer_set(&et, CLOCK_SECOND);
  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et)
          || (ev == serial_line_event_message)
          );

      if(ev == serial_line_event_message) {
          char *serial_data = (char *)data;
          printf("received line: %s\n", serial_data);

          if(serial_data[0] == 's') {
              // read number, skip one space
              int distance;
              sscanf(serial_data, "s %d", &distance);

              // minimum slot distance is 1
              if(distance > UINT8_MAX || distance < 1) {
                  printf("invalid distance\n");
              }
              
              /* generate_slotframe_with_distance(distance); */
          }
      }

      // if tsch_is_associated add for each neighbor a ranging slot
      if(tsch_is_associated) {
          leds_toggle(LEDS_3);

          printf("tschass, 1\n");
      } else {
          printf("tschass, 0\n");          
      }
      
      uart_write_link_addr();

      if(etimer_expired(&et)) {
          etimer_reset(&et);
      }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
