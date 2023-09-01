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
#include "drand.h"
#endif
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
PROCESS(TSCH_PROP_PROCESS, "TSCH localization User Process");


AUTOSTART_PROCESSES(&node_process, &sensors_process);

#if WITH_LOC_RIME

static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  printf("broadcast message received from %d.%d: '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

#endif

/*---------------------------------------------------------------------------*/
static void
print_network_status(void)
{
  int i;
  uint8_t state;
#if LOC_WITH_RPL
  uip_ds6_defrt_t *default_route;  
#if RPL_WITH_STORING
  uip_ds6_route_t *route;
#endif /* RPL_WITH_STORING */
#if RPL_WITH_NON_STORING
  rpl_ns_node_t *link;
#endif /* RPL_WITH_NON_STORING */

  PRINTF("--- Network status ---\n");

  /* Our IPv6 addresses */
  PRINTF("- Server IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINTF("-- ");
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
    }
  }

  /* Our default route */
  PRINTF("- Default route:\n");
  default_route = uip_ds6_defrt_lookup(uip_ds6_defrt_choose());
  if(default_route != NULL) {
    PRINTF("-- ");
    PRINT6ADDR(&default_route->ipaddr);
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)default_route->lifetime.interval);
  } else {
    PRINTF("-- None\n");
  }

#if RPL_WITH_NON_STORING
  /* Our routing links */
  PRINTF("- Routing links (%u in total):\n", rpl_ns_num_nodes());
  link = rpl_ns_node_head();
  while(link != NULL) {
    uip_ipaddr_t child_ipaddr;
    uip_ipaddr_t parent_ipaddr;
    rpl_ns_get_node_global_addr(&child_ipaddr, link);
    rpl_ns_get_node_global_addr(&parent_ipaddr, link->parent);
    PRINTF("-- ");
    PRINT6ADDR(&child_ipaddr);
    if(link->parent == NULL) {
      memset(&parent_ipaddr, 0, sizeof(parent_ipaddr));
      PRINTF(" --- DODAG root ");
    } else {
      PRINTF(" to ");
      PRINT6ADDR(&parent_ipaddr);
    }
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)link->lifetime);
    link = rpl_ns_node_next(link);
  }
#endif
  /* print rpl neighbor list */
  PRINTF("- RPL neighbor list:\n");
  rpl_print_neighbor_list();
#endif /* LOC_WITH_RPL */
  
  printf("\n-TSCH neighbor list:\n");
  print_tsch_neighbor_list();
  
  printf("----------------------\n");
}
/*---------------------------------------------------------------------------*/

static void
net_init(uint8_t is_coordinator)
{
  uip_ipaddr_t global_ipaddr;


  if(is_coordinator) { /* We are RPL root. Will be set automatically
                     as TSCH pan coordinator via the tsch-rpl module */
#if LOC_WITH_RPL      

#else
      tsch_set_coordinator(1); // This would otherwise be done by tsch-rpl
#endif
  }

  NETSTACK_MAC.on();
}


#define LOG_CONFIG_ITEM(NAME) do { printf(";; %s = %d\n", #NAME, NAME); } while(0)

// useful for evaluation, print configuration into log, prefix with ;; for easy parsing
static void print_configuration() {
    // use macro instead
    LOG_CONFIG_ITEM(TSCH_CONF_DEFAULT_TIMESLOT_LENGTH);
    LOG_CONFIG_ITEM(TSCH_CONF_EB_PERIOD);
    LOG_CONFIG_ITEM(TSCH_CONF_MAX_EB_PERIOD);
    LOG_CONFIG_ITEM(TSCH_CONF_AUTOSELECT_TIME_SOURCE);
    LOG_CONFIG_ITEM(LOC_WITH_RPL);        
    LOG_CONFIG_ITEM(PACKETBUF_SIZE);
    
    printf(";; SCHEDULE_USED = CUSTOM\n");
}

void output_range_via_serial_snprintf(uint8_t addr_short, float range) {
    // use uart0_writeb(char byte) to write range
    char buffer[20];

    int length = snprintf(buffer, 20, "TW, %u,"NRF_LOG_FLOAT_MARKER "\n", addr_short, NRF_LOG_FLOAT( range ) );

    for (int i = 0; i < length; i++) {
        uart0_writeb(buffer[i]);
    }
}

void output_tdoa_via_serial(ranging_addr_t node1_addr, ranging_addr_t node2_addr, float dist) {
    // use uart0_writeb(char byte) to write range
    char buffer[30];

    int length = snprintf(buffer, 30, "TD, %u, %u, " NRF_LOG_FLOAT_MARKER "\n", node1_addr, node2_addr, NRF_LOG_FLOAT(dist));

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
        
        float dist = time_to_dist(m->time);
        if(m->type == TWR) {
            ranging_addr_t addr_short = m->addr_B;
            output_range_via_serial_snprintf(addr_short, dist);
        } else if (m->type == TDOA) {
            /* printf("TDOA between %u and %u is %d\n", m->addr_A, m->addr_B, m->time); */
            output_tdoa_via_serial(m->addr_A, m->addr_B, dist);
        }
    }
  }

  PROCESS_END();
}

/* void initialize_udp() { */
/*     server_conn = udp_new(NULL, UIP_HTONS(UDP_CLIENT_PORT), NULL); */
/*     if(server_conn == NULL) { */
/*         PRINTF("No UDP connection available, exiting the process!\n"); */
/*         PROCESS_EXIT(); */
/*     } */
/*     udp_bind(server_conn, UIP_HTONS(UDP_SERVER_PORT)); */
/* } */

/* void net_reinit() { */
/*     NETSTACK_MAC.off(false); */
/*     NETSTACK_MAC.init(); */
/*     NETSTACK_MAC.on();     */
/* } */

void node_set_mobile() {
    printf("setting node as mobile\n");
    #if LOC_WITH_RPL
    rpl_set_mode(RPL_MODE_LEAF); // node is reachable, but does not forward packets for others
    #endif
    net_init(0);
    mtm_init(0);
}

void node_set_anchor() {
    printf("setting node as anchor\n");
    #if LOC_WITH_RPL    
    rpl_set_mode(RPL_MODE_MESH);
    #endif
    net_init(0);
    mtm_init(0);
    mtm_set_node_anchor(true);

    drand_init(32);
}

void node_set_root() {
    printf("setting node as root\n");
    #if LOC_WITH_RPL        
    rpl_set_mode(RPL_MODE_MESH);
    uip_ipaddr_t prefix;
    uip_ip6addr(&prefix, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    memcpy(&global_ipaddr, br_prefix, 16);
    uip_ds6_set_addr_iid(&global_ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&global_ipaddr, 0, ADDR_AUTOCONF);
    rpl_set_root(RPL_DEFAULT_INSTANCE, &global_ipaddr);
    rpl_set_prefix(rpl_get_any_dag(), br_prefix, 64);
    rpl_repair_root(RPL_DEFAULT_INSTANCE);    
    #endif

    net_init(1);
    mtm_init(1);
    mtm_set_node_anchor(true);

    drand_init(32);
}

void turn_on_led_for_role(enum node_role role) {
    leds_off(LEDS_ALL);
    switch(role) {
    case MOBILE:
        leds_on(LEDS_ORANGE);
        break;
    case ANCHOR:
        leds_on(LEDS_GREEN);
        break;
    case ROOT:
        leds_on(LEDS_BLUE);
        break;
    }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer et;
  static struct etimer node_role_timer;
  static struct etimer network_status_timer;
  static struct etimer broadcast_timer;
  static uint8_t anchorChoseRole = 0, manual_chose_role = 0;

  PROCESS_BEGIN();

  /* 3 possible roles:
   * - role_6ln: simple node, will join any network, secured or not
   * - role_6dr: DAG root, will advertise (unsecured) beacons
   * - role_6dr_sec: DAG root, will advertise secured beacons
   * */
  /* static enum { role_6ln, role_6dr } node_role; */
  static enum node_role current_node_role = MOBILE;
  leds_on(LEDS_ORANGE);
  
#define CONFIG_WAIT_TIME 1
  etimer_set(&et, CLOCK_SECOND * CONFIG_WAIT_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  // print configuration
  printf("=print configuration=\n");
  print_configuration();
  // print current schedule

  // wait again two second
  printf("=wait 1 seconds=\n");
  etimer_set(&et, CLOCK_SECOND * 1);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  SENSORS_ACTIVATE(button_sensor);

  etimer_set(&node_role_timer, CLOCK_SECOND*10);
  etimer_set(&network_status_timer, CLOCK_SECOND);

#if WITH_LOC_RIME
  broadcast_open(&broadcast, 129, &broadcast_call);  
  etimer_set(&broadcast_timer, CLOCK_SECOND * 4 + (random_rand() % (CLOCK_SECOND * 4)));
#endif

  /* struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(4, 20); */
  static uint16_t skip_rounds = 0;
  /* Print out routing tables every minute */
  etimer_set(&et, CLOCK_SECOND/4);
  while(1) {

      PROCESS_WAIT_EVENT_UNTIL(
          (ev == sensors_event && data == &button_sensor)
          || etimer_expired(&et) || etimer_expired(&node_role_timer)
          || etimer_expired(&network_status_timer)
          || etimer_expired(&broadcast_timer)
      );

      if (ev == sensors_event && data == &button_sensor) {
          int duration = button_sensor.value(BUTTON_SENSOR_VALUE_DURATION);
          int state = button_sensor.value(BUTTON_SENSOR_VALUE_STATE);
          
          if(state == BUTTON_SENSOR_VALUE_RELEASED) {
              manual_chose_role = 1;
              printf("button pressed, toggling node anchor state\n");

              current_node_role = (current_node_role + 1) % 3;
              
              turn_on_led_for_role(current_node_role);
          }
      }

      if (etimer_expired(&node_role_timer) && !anchorChoseRole) {
          enum node_role role = MOBILE;
          
          if(manual_chose_role) {
              role = current_node_role;
          } else { // else we will consult the node_roles in nodes.h
              role = get_role_for_node(&linkaddr_node_addr);
          }

          turn_on_led_for_role(role);          

          switch(role) {
          case MOBILE: {
              node_set_mobile();
              break;
          }
          case ANCHOR: {
              node_set_anchor();
              break;
          }
          case ROOT: {
              node_set_root();
              break;
          }
          }          

          anchorChoseRole = 1;
      }

      if(etimer_expired(&network_status_timer)) {
          /* printf("\n"); */
          /* print_network_status(); */
          /* printf("\n");               */
          etimer_reset(&network_status_timer);
      }

    // if tsch_is_associated add for each neighbor a ranging slot
      if(tsch_is_associated) {
          /* rpl_print_neighbor_list(); */
          leds_toggle(LEDS_3);
          /* print_network_status(); */

          if (skip_rounds > 5) {
              mtm_update_schedule();
          } else {
              skip_rounds++;            
          }

#if WITH_LOC_RIME
          if(etimer_expired(&broadcast_timer)) {
              /* printf("broadcasting\n"); */
              /* packetbuf_copyfrom("Hello", 6); */
              /* broadcast_send(&broadcast); */
              etimer_set(&broadcast_timer, CLOCK_SECOND * 4 + (random_rand() % (CLOCK_SECOND * 4)));
          }
#endif

      }

      if(etimer_expired(&et)) {
          etimer_reset(&et);
      }
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
