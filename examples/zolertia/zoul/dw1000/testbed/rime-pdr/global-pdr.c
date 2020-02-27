/*
 * Copyright (c) 2016, Inria.
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
 *         Send a broacast message in each slotframe.
 * \author
 *         Maximilien Charlier
 *
 */

#include <stdio.h>
#include "contiki.h"
#include "contiki-conf.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
#include "net/mac/tsch/tsch.h"

/* containt def of tsch_schedule_get_slotframe_duration */
#include "net/mac/tsch/tsch-schedule.h" 

#include "examples/zolertia/zoul/dw1000/testbed/shedule-testbed.h"

#include "dev/uart.h"
#include "dev/serial-line.h"

#define write_byte(b) uart_write_byte(DBG_CONF_UART, b)

#define PRINT_BYTE 0

#if !PRINT_BYTE
  #define PRINTF(...) printf(__VA_ARGS__)
#else /* !PRINT_BYTE */
  #define PRINTF(...) do {} while(0)
#endif /* PRINT_BYTE */

const linkaddr_t coordinator_addr =    { { 0X00, 0X01 } };


/*---------------------------------------------------------------------------*/
PROCESS(global_pdr_process, "Global connectivity");
AUTOSTART_PROCESSES(&global_pdr_process);

/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
   #if PRINT_BYTE
    /* print R: _NODEADDR_
    */
    printf("-R:");
    write_byte(from->u8[1]);
    write_byte(from->u8[0]);
    write_byte((uint8_t) '\n');
  #else /* PRINT_BYTE */  
    printf("R: 0X%02X%02X \n", from->u8[0], from->u8[1]);
  #endif /* PRINT_BYTE */
}
/*---------------------------------------------------------------------------*/
static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
  const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(linkaddr_cmp(dest, &linkaddr_null)) {
    return;
  }
  #if PRINT_BYTE
    /* print S: _NODEADDR_STATUS_NUM-TX_
    */
    printf("-S:");
    write_byte(dest->u8[1]);
    write_byte(dest->u8[0]);
    write_byte(status);
    write_byte(num_tx);
    write_byte((uint8_t) '\n');
  #else /* PRINT_BYTE */  
    printf("S: 0X%02X%02X stat %d tx %d \n", 
      dest->u8[0], dest->u8[1], status, num_tx);
  #endif /* PRINT_BYTE */
}
/*---------------------------------------------------------------------------*/
static const struct unicast_callbacks unicast_callbacks = {recv_uc, sent_uc};
static struct unicast_conn uc;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(global_pdr_process, ev, data)
{  
  static struct etimer et;
  
  static linkaddr_t node_1_address = { { 0x00, 0X01} };
  static linkaddr_t node_2_address = { { 0x00, 0X02} };
  static linkaddr_t node_3_address = { { 0x00, 0X03} };
  static linkaddr_t node_4_address = { { 0x00, 0X04} };
  static linkaddr_t node_5_address = { { 0x00, 0X05} };
  static linkaddr_t node_6_address = { { 0x00, 0X06} };
  static linkaddr_t node_7_address = { { 0x00, 0X07} };
  static linkaddr_t node_8_address = { { 0x00, 0X08} };
  static linkaddr_t node_9_address = { { 0x00, 0X09} };
  static linkaddr_t node_10_address = { { 0x00, 0X0A} };
  static linkaddr_t node_11_address = { { 0x00, 0X0B} };
  static linkaddr_t node_12_address = { { 0x00, 0X0C} };
  static linkaddr_t node_13_address = { { 0x00, 0X0D} };
  static linkaddr_t node_14_address = { { 0x00, 0X0E} };
  static linkaddr_t node_15_address = { { 0x00, 0X0F} };
  static linkaddr_t node_16_address = { { 0x00, 0X10} };

  static linkaddr_t* all_addr[16] = {
    &node_1_address,
    &node_2_address,
    &node_3_address,
    &node_4_address,
    &node_5_address,
    &node_6_address,
    &node_7_address,
    &node_8_address,
    &node_9_address,
    &node_10_address,
    &node_11_address,
    &node_12_address,
    &node_13_address,
    &node_14_address,
    &node_15_address,
    &node_16_address
  };

  PROCESS_EXITHANDLER(unicast_close(&uc);)

  PROCESS_BEGIN();

  tsch_schedule_fullmesh_data();

  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));

  NETSTACK_MAC.on();

  unicast_open(&uc, 146, &unicast_callbacks);

  /* Delay 50 seconds */
  etimer_set(&et, CLOCK_SECOND * 150);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    for (int i = 0; i < 16; i++){
      packetbuf_copyfrom("Hello", 5);
      if(!linkaddr_cmp(all_addr[i], &linkaddr_node_addr)) {
        unicast_send(&uc, all_addr[i]);
      }
    }

    /* Delay 1 slotframe */
    etimer_set(&et, (CLOCK_SECOND * tsch_schedule_get_slotframe_duration())/RTIMER_SECOND);

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/