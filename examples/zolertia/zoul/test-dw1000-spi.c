/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "dev/dw1000-arch.h"
#include "dw1000.h"
#include "dw1000-const.h"
#include "dw1000-driver.h"
#include "assert.h"

#include <stdio.h> /* For printf() */

/*---------------------------------------------------------------------------*/
extern void dw1000_arch_init(void);
extern void dwm1000_arch_spi_deselect(void);
extern void dwm1000_arch_spi_select(void);
extern int dwm1000_arch_spi_rw_byte(uint8_t c);
extern int dwm1000_arch_spi_rw(uint8_t *inbuf, const uint8_t *write_buf, uint16_t len);
extern void print_u8_Array_inHex(char *string, uint8_t *array, uint32_t arrayLength);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS(dwm1000_spi_test_process, "DWM1000 SPI process");
AUTOSTART_PROCESSES(&dwm1000_spi_test_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(dwm1000_spi_test_process, ev, data)
{
  static struct etimer timer;
  static int csn = 0;

  PROCESS_BEGIN();
  // dw1000_driver_init();
  // dw1000_arch_init();
  assert(0xDECA0130 == dw_read_reg_32(DW_REG_DEV_ID, DW_LEN_DEV_ID));


  etimer_set(&timer, CLOCK_SECOND * 5);
  while (1) {
    PROCESS_WAIT_EVENT();
    if (ev == PROCESS_EVENT_TIMER) {

      printf("CSN %d\n", csn);
      // if(csn == 1){
      //   dwm1000_arch_spi_select();
      //   csn = 0;
      // }
      // else{
      //   dwm1000_arch_spi_deselect();
      //   csn = 1;
      // }


      // uint8_t c = 0x55;
      // dwm1000_arch_spi_select();
      // dwm1000_arch_spi_rw_byte(c);
      // dwm1000_arch_spi_deselect();


      uint8_t c = 0x0;
      uint8_t tempRead1[4];
      tempRead1[0] = 0;
      tempRead1[1] = 0;
      tempRead1[2] = 0;
      tempRead1[3] = 0;
      dwm1000_arch_spi_select();
      dwm1000_arch_spi_rw_byte(c);
      dwm1000_arch_spi_rw(tempRead1, NULL, 4);
      dwm1000_arch_spi_deselect();

      /* make test-dw1000-spi.upload BOARD=remote-dw1000 TARGET=zoul PORT=/dev/ttyUSB0 */
      /* make login TARGET=zoul */
      // uint8_t tempRead1[8];
      // dw_read_reg(DW_REG_DEV_ID, DW_LEN_DEV_ID, tempRead1);

      /* 0xDECA0130 */
      print_u8_Array_inHex("REG ID 0xDECA0130:", tempRead1, 4);


      tempRead1[0] = 0;
      tempRead1[1] = 0;
      tempRead1[2] = 0;
      tempRead1[3] = 0;
      uint8_t tempWrite1[2] = {0x40, 0x02};
      dwm1000_arch_spi_select();
      dwm1000_arch_spi_rw(NULL, tempWrite1, 2);
      dwm1000_arch_spi_rw(tempRead1, NULL, 2);
      dwm1000_arch_spi_deselect();

      print_u8_Array_inHex("tempWrite1 0x4002:", tempWrite1, 2);
      print_u8_Array_inHex("REG ID 0xDECA:", tempRead1, 2);
      printf("read %8X\n", (int) dw_read_reg_32(DW_REG_DEV_ID, 4));


      etimer_reset(&timer);
    }
  }

  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
