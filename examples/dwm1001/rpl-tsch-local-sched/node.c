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
#include "net/rpl/rpl.h"
#include "net/ipv6/uip-ds6-route.h"
#include "nbr-table.h"
#include "net/mac/tsch/tsch.h"
#include "net/rpl/rpl-private.h"
#include "tsch-prop.h"
#include <stdio.h>
#if WITH_ORCHESTRA
#include "orchestra.h"
#else
#include "schedule.h"
#endif

#include "dev/uart0.h"
#include "nrfx_log.h"

#include "leds.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define CONFIG_VIA_BUTTON PLATFORM_HAS_BUTTON
#if CONFIG_VIA_BUTTON
#include "button-sensor.h"
#endif /* CONFIG_VIA_BUTTON */

#define SPEED_OF_LIGHT_M_PER_S 299702547.236
#define SPEED_OF_LIGHT_M_PER_UWB_TU ((SPEED_OF_LIGHT_M_PER_S * 1.0E-15) * 15650.0) // around 0.00469175196

/*---------------------------------------------------------------------------*/
PROCESS(node_process, "RPL Node");
PROCESS(TSCH_PROP_PROCESS, "TSCH localization User Process");


#if CONFIG_VIA_BUTTON
AUTOSTART_PROCESSES(&node_process, &sensors_process);
#else /* CONFIG_VIA_BUTTON */
AUTOSTART_PROCESSES(&node_process);
#endif /* CONFIG_VIA_BUTTON */


/*---------------------------------------------------------------------------*/
static void
print_network_status(void)
{
  int i;
  uint8_t state;
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

#if RPL_WITH_STORING
  /* Our routing entries */
  PRINTF("- Routing entries (%u in total):\n", uip_ds6_route_num_routes());
  route = uip_ds6_route_head();
  while(route != NULL) {
    PRINTF("-- ");
    PRINT6ADDR(&route->ipaddr);
    PRINTF(" via ");
    PRINT6ADDR(uip_ds6_route_nexthop(route));
    PRINTF(" (lifetime: %lu seconds)\n", (unsigned long)route->state.lifetime);
    route = uip_ds6_route_next(route);
  }
#endif

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

  PRINTF("----------------------\n");
}
/*---------------------------------------------------------------------------*/
static void
net_init(uip_ipaddr_t *br_prefix)
{
  uip_ipaddr_t global_ipaddr;

  if(br_prefix) { /* We are RPL root. Will be set automatically
                     as TSCH pan coordinator via the tsch-rpl module */
    memcpy(&global_ipaddr, br_prefix, 16);
    uip_ds6_set_addr_iid(&global_ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&global_ipaddr, 0, ADDR_AUTOCONF);
    rpl_set_root(RPL_DEFAULT_INSTANCE, &global_ipaddr);
    rpl_set_prefix(rpl_get_any_dag(), br_prefix, 64);
    rpl_repair_root(RPL_DEFAULT_INSTANCE);
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

#if WITH_ORCHESTRA
    printf(";; SCHEDULE_USED = ORCHESTRA\n");
#else
    printf(";; SCHEDULE_USED = CUSTOM\n");
#endif

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

        /* printf("sending range to host, tof is: %u \n", n->tof); */
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


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  /* 3 possible roles:
   * - role_6ln: simple node, will join any network, secured or not
   * - role_6dr: DAG root, will advertise (unsecured) beacons
   * - role_6dr_sec: DAG root, will advertise secured beacons
   * */
  static int is_coordinator = 0;
  static enum { role_6ln, role_6dr, role_6dr_sec } node_role;
  node_role = role_6ln;

  int coordinator_candidate = 0;

#ifdef CONTIKI_TARGET_Z1
  /* Set node with MAC address c1:0c:00:00:00:00:01 as coordinator,
   * convenient in cooja for regression tests using z1 nodes
   * */
  extern unsigned char node_mac[8];
  unsigned char coordinator_mac[8] = { 0xc1, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

  coordinator_candidate = (memcmp(node_mac, coordinator_mac, 8) == 0);
#elif CONTIKI_TARGET_COOJA
  coordinator_candidate = (node_id == 1);
#elif CONTIKI_TARGET_DWM1001
  printf("linkaddr_node_addr.u8[LINKADDR_SIZE-1] = %d\n", linkaddr_node_addr.u8[LINKADDR_SIZE-1]);
  coordinator_candidate = linkaddr_cmp(&linkaddr_node_addr, &node_0_ll);
#endif

  if(coordinator_candidate) {
    if(LLSEC802154_ENABLED) {
      node_role = role_6dr_sec;
    } else {
      node_role = role_6dr;
    }
  } else {
    node_role = role_6ln;
  }

#define CONFIG_WAIT_TIME 5
  etimer_set(&et, CLOCK_SECOND * CONFIG_WAIT_TIME);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


  printf("Init: node starting with role %s\n",
         node_role == role_6ln ? "6ln" : (node_role == role_6dr) ? "6dr" : "6dr-sec");

  tsch_set_pan_secured(LLSEC802154_ENABLED && (node_role == role_6dr_sec));
  is_coordinator = node_role > role_6ln;

  if(is_coordinator) {
    uip_ipaddr_t prefix;
    uip_ip6addr(&prefix, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    net_init(&prefix);
  } else {
    net_init(NULL);
  }

#if WITH_ORCHESTRA
  orchestra_init();

  // we add one more slotframe for ranging exchange
#else
  // pass linkaddress
  init_custom_schedule();
#endif

  // delay 5 second before starting

  // print configuration
  printf("=print configuration=\n");
  print_configuration();
  // print current schedule


  // wait again two second
  printf("=wait 2 seconds=\n");
  etimer_set(&et, CLOCK_SECOND * 2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  tsch_schedule_print();

  /* struct tsch_slotframe *sf_eb = tsch_schedule_add_slotframe(4, 20); */

  /* Print out routing tables every minute */
  etimer_set(&et, CLOCK_SECOND*2);
  while(1) {
    /* print_network_status(); */
    PROCESS_YIELD_UNTIL(etimer_expired(&et));

    // if tsch_is_associated add for each neighbor a ranging slot
    if(tsch_is_associated) {
        rpl_print_neighbor_list();
        leds_toggle(LEDS_3);
        print_network_status();
    }

    etimer_reset(&et);
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
