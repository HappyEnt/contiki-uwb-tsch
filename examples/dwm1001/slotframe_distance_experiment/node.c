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

#include "slotframe_evaluation_scheduler.h"
#include "toulouse-nodes.h"

#include "dw1000-arch.h"
#include "decadriver/deca_device_api.h"
#include "decadriver/deca_regs.h"
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
PROCESS(TSCH_PROP_PROCESS, "TSCH localization User Process");

AUTOSTART_PROCESSES(&node_process, &sensors_process);

static uint8_t current_slot_distance = 0;

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
    LOG_CONFIG_ITEM(TSCH_CONF_SLEEP);    
    LOG_CONFIG_ITEM(LINKADDR_SIZE);
    LOG_CONFIG_ITEM(LOC_WITH_RPL);        
    LOG_CONFIG_ITEM(PACKETBUF_SIZE);
    
    printf(";; SCHEDULE_USED = CUSTOM\n");
}

void output_range_via_serial_snprintf(uint8_t addr_short, float range, int32_t freq_offset) {
    // use uart0_writeb(char byte) to write range
    char buffer[70];

    // we output data in units of cm
    /* int length = snprintf(buffer, 60, "TW, %u, %u, %d, "NRF_LOG_FLOAT_MARKER "\n", addr_short, current_slot_distance, freq_offset, NRF_LOG_FLOAT( range * 100 ) ); */
    /* int length = snprintf(buffer, 70, "TW, %u, %u, %d \n", addr_short, current_slot_distance, (int32_t) (range * 10000) ); */
    int length = snprintf(buffer, 70, "TW, %u, %u, %ld, %ld\n", addr_short, current_slot_distance, (int32_t) (range * 10000), (int32_t) freq_offset);

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void uart_write_link_addr() {
    char buffer[40];
    int length = snprintf(buffer, 50, "TA, %u, %u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1]);

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void output_tdoa_via_serial(ranging_addr_t node1_addr, ranging_addr_t node2_addr, float dist) {
    // use uart0_writeb(char byte) to write range
    char buffer[70];

    int length = snprintf(buffer, 70, "TD, %u, %u, %u, %ld\n", current_slot_distance, node1_addr, node2_addr, (int32_t) (dist * 10000));

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
            int32_t freq_offset = m->freq_offset;
            output_range_via_serial_snprintf(addr_short, dist, freq_offset);
        } else if (m->type == TDOA) {
            output_tdoa_via_serial(m->addr_A, m->addr_B, dist);
        }
#endif
    }
  }

  PROCESS_END();
}

void enable_all_clocks() {
    uint8 reg[2];    
    dwt_readfromdevice(PMSC_ID, PMSC_CTRL0_OFFSET, 2, reg);
    reg[0] = 0x00 ;
    reg[1] = reg[1] & 0xfe;
    dwt_writetodevice(PMSC_ID, PMSC_CTRL0_OFFSET, 1, &reg[0]);
    dwt_writetodevice(PMSC_ID, 0x1, 1, &reg[1]);            
}


void perform_soft_reset() {
    dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_INIT_STATE);    
    /* dw_soft_reset(); */
    dwt_softreset();
    /* enable_all_clocks(); */
    /* dwt_forcetrxoff(); // Turn the RX off */
    /* dwt_rxreset(); // Reset in case we we     */
    /* dwt_initialise(DWT_LOADNONE); // Reload the LDE microcode */
    dw1000_arch_spi_set_clock_freq(DW_SPI_CLOCK_FREQ_IDLE_STATE);    
}


void slotframe_new_distance_callback(uint8_t new_distance) {
    current_slot_distance = new_distance;
    /* printf("new slot distance %u\n", new_distance); */
}

#if TESTBED_LILLE
const linkaddr_t node_0_ll = { {  21, 215  } }; //dwm1001-1 */
const linkaddr_t node_1_ll = { {  23, 206  } }; //dwm1001-2 */
#elif TESTBED_TOULOUSE // using our custom linkaddr mapping
const linkaddr_t *node_0_ll = &dwm1001_10_ll;
const linkaddr_t *node_1_ll = &dwm1001_9_ll;
#endif

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

  set_slotframe_new_distance_callback(slotframe_new_distance_callback);
  
  if(linkaddr_cmp(node_0_ll, &linkaddr_node_addr)) {
      printf("=node type 0 initiator=\n");
      net_init(1);
      tsch_set_send_beacons(1);
      slotframe_evaluation_length_scheduler_init(100, 0);
  } else if(linkaddr_cmp(node_1_ll, &linkaddr_node_addr)) {
      printf("=node type 1 responder =\n");
      net_init(0);
      tsch_set_send_beacons(0);
      slotframe_evaluation_length_scheduler_init(100, 1);
  } else {
      printf("=node type 2 passive =\n");
      net_init(0);
      tsch_set_send_beacons(0);
      slotframe_evaluation_length_scheduler_init(100, 2);
  }
  
  etimer_set(&et, CLOCK_SECOND * 1);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  etimer_set(&et, CLOCK_SECOND*10);
  /* perform_soft_reset();   */
  while(1) {
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et)
          || (ev == serial_line_event_message)
          );

      if(ev == serial_line_event_message) {
          char *serial_data = (char *)data;
          /* printf("received line: %s\n", serial_data); */

          if(serial_data[0] == 's') {
              // read number, skip one space
              int distance;
              sscanf(serial_data, "s %d", &distance);

              // minimum slot distance is 1
              if(distance > UINT8_MAX || distance < 0) {
                  printf("invalid distance\n");
              }

              /* perform_soft_reset(); */
              
              generate_slotframe_with_distance(distance);
          } else if (serial_data[0] == 'r') {
              // perform soft reset
              printf("perform soft reset\n");
              perform_soft_reset();
          }
      }

      // if tsch_is_associated add for each neighbor a ranging slot
      /* if(tsch_is_associated) { */
      /*     leds_toggle(LEDS_3); */

      /*     printf("tschass 1\n"); */
      /* } else { */
      /*     printf("tschass 0\n");           */
      /* } */
      
      uart_write_link_addr();

      if(etimer_expired(&et)) {
          /* perform_soft_reset(); */
          etimer_reset(&et);
      }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
