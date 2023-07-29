#include "deca_device_api.h"
#include "contiki.h"

#include "contiki-net.h"

#include "dw1000.h"
#include "dw1000-arch.h"
#include "dw1000-const.h"

#include "d3s_dw1000-arch.h"

#include <stdio.h>
#include <string.h>


/* /\*---------------------------------------------------------------------------*\/ */
void dw_write_subreg(uint32_t reg_addr, uint16_t subreg_addr,
                      uint16_t subreg_len, const uint8_t *write_buf)
{
    // we just wrap the official decawave library here
    dwt_writetodevice(reg_addr, subreg_addr, subreg_len, write_buf);
}

void dw_read_subreg(uint32_t reg_addr, uint16_t subreg_addr,
                     uint16_t subreg_len, uint8_t *read_buf)
{
    // we just wrap the official decawave library here
    dwt_readfromdevice(reg_addr, subreg_addr, subreg_len, read_buf);
}

/* /\*---------------------------------------------------------------------------*\/ */
/* extern int dw1000_driver_interrupt(void); /\* declare in dw1000-driver.h *\/ */

/* /\*---------------------------------------------------------------------------*\/ */
/* /\* Dummy buffer for the SPI transaction *\/ */
/* uint8_t spi_dummy_buffer = 0; */
/* /\*---------------------------------------------------------------------------*\/ */
/* void */
/* dw1000_int_handler(uint8_t port, uint8_t pin) */
/* { */
/*   /\* To keep the gpio_register_callback happy *\/ */
/*   dw1000_driver_interrupt(); */
/* } */

/* /\*---------------------------------------------------------------------------*\/ */
void
dw1000_arch_gpio8_setup_irq(void)
{
    d3s_dw1000_arch_setup_irq();
}
/*---------------------------------------------------------------------------*/

void
dw1000_arch_gpio8_enable_irq(void)
{
    dw1000_enable_interrupt(1);
}

/* /\*---------------------------------------------------------------------------*\/ */
void
dw1000_arch_gpio8_disable_irq(void)
{
    dw1000_disable_interrupt();
}
/* /\*---------------------------------------------------------------------------*\/ */

/* int */
/* dw1000_arch_gpio8_read_pin(void) */
/* { */
/* } */
/* /\*---------------------------------------------------------------------------*\/ */
/* /\** Initialize the RESET PIN in INPUT and open drain *\/ */
/* void */
/* dw1000_arch_init_reset_pin(void) */
/* { */
/* } */

/* /\*---------------------------------------------------------------------------*\/ */
/* /\** */
/*  * \brief Initialize the architecture specific part of the DW1000 */
/*  **\/ */
void dw1000_arch_init()
{

  printf("dw1000_arch_init\n");
  d3s_dw1000_arch_init();

  /* Performe a wake up in case of the node was in deepsleept before being restarted */
  NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_REQUEST_WAKEUP);
  dw1000_us_delay(4000);
}

/* /\** */
/*  * \brief     Wait a delay in microsecond. */
/*  * */
/*  * \param ms  The delay in microsecond. */
/*  **\/ */
void dw1000_us_delay(int us){
  clock_delay_usec(us);
}

/* /\** */
/*  * Change the SPI frequency to freq. */
/*  * If freq is bigger than the maximum SPI frequency value of the embedded */
/*  * system set this maximum value. */
/*  **\/ */
void dw1000_arch_spi_set_clock_freq(uint32_t freq){
    printf("setting spi clock freq to %d\n", freq);
    if(freq == DW_SPI_CLOCK_FREQ_IDLE_STATE) {
        dw1000_spi_set_fast_rate();
    } else {
        dw1000_spi_set_slow_rate();
    }
}
/* /\* /\\** *\/ */
/* /\*  * Configure the embedeed system to allow the transceiver to go in *\/ */
/* /\*  * DEEPSLEEP state. *\/ */
/* /\*  * Disable SPICLK and SPIMISO to avoid leakeage current. *\/ */
/* /\*  **\\/ *\/ */
void dw1000_arch_init_deepsleep(void){
    // TODO
}


/* /\* /\\** *\/ */
/* /\*  * Used in DEEPSLEEP state. *\/ */
/* /\*  * You need to drive HIGH the port WAKEUP or LOW the CSN port for min 500µs *\/ */
/* /\*  * to wakeup the transceiver. *\/ */
/* /\*  * You can check if thew transceiver have wakeup by reading the GPIO8 state *\/ */
/* /\*  * GPIO8 is drive HIGH when the transceiver comes in the IDLE state *\/ */
/* /\*  * It take approx 3ms to go out of the deepsleep state. *\/ */
/* /\*  **\\/ *\/ */

void dw1000_arch_wake_up(dw1000_pin_state state){
    dw1000_arch_wakeup_nowait();
}

/* /\* /\\** *\/ */
/* /\*  * Configure the embedded system to interact with the transceiver. *\/ */
/* /\*  * Enable SPICLK and SPIMISO. *\/ */
/* /\*  **\\/ *\/ */
void dw1000_arch_restore_idle_state(void){
    // TODO
}


/* /\* /\\*---------------------------------------------------------------------------*\\/ *\/ */
/* /\* /\\** *\/ */
/* /\*  * \brief Read the 4096 bytes of the Accumulator CIR memory. *\/ */
/* /\*  * *\\/ *\/ */
/* /\* void dw_read_CIR(uint8_t * read_buf) *\/ */
/* /\* { *\/ */

/* /\*   // printf("dw_read_CIR \n"); *\/ */
/* /\*   uint8_t reg_addr = DW_REG_ACC_MEM; *\/ */
/* /\*   uint16_t subreg_len = DW_LEN_ACC_MEM, i; *\/ */

/* /\*   DW1000_SELECT(); *\/ */

/* /\*   /\\* We write the SPI commands on the SPI TX FIFO and we flush the SPI RX FIFO *\/ */
/* /\*     later (there are a delay between the reception of the byte on the physical *\/ */
/* /\*     SPI and the availability of the byte on the RX FIFO). *\\/ *\/ */
/* /\*   SPIX_BUF(DWM1000_SPI_INSTANCE) = reg_addr & 0x3F; *\/ */
/* /\*   DW1000_SPI_RX_FLUSH(1); *\/ */


/* /\*   SPIX_BUF(DWM1000_SPI_INSTANCE) = 0; *\/ */
/* /\*   SPIX_WAITFOREORx(DWM1000_SPI_INSTANCE); /\\* RX FIFO is not empty *\\/ *\/ */
/* /\*   SPIX_BUF(DWM1000_SPI_INSTANCE); *\/ */

/* /\*   for(i = 0; i < subreg_len; i++) { *\/ */
/* /\*     SPIX_BUF(DWM1000_SPI_INSTANCE) = 0; *\/ */

/* /\*     SPIX_WAITFOREORx(DWM1000_SPI_INSTANCE); /\\* RX FIFO is not empty *\\/ *\/ */
/* /\*     read_buf[i] = SPIX_BUF(DWM1000_SPI_INSTANCE); *\/ */

/* /\*     watchdog_periodic(); /\\* avoid watchdog timer to be reach *\\/ *\/ */
/* /\*   } *\/ */

/* /\*   DW1000_DESELECT(); *\/ */
/* /\* } *\/ */
