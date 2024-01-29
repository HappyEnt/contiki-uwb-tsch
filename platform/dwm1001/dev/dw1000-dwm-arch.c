#include "deca_device_api.h"
#include "contiki.h"

#include "contiki-net.h"

#include "dw1000.h"
#include "dw1000-arch.h"
#include "dw1000-const.h"

#include "d3s_dw1000-arch.h"

#include <stdio.h>
#include <string.h>

// we are lazy so we just wrap the d3s stuff here
/*---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*/
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

/*---------------------------------------------------------------------------*/
void
dw1000_arch_gpio8_disable_irq(void)
{
    dw1000_disable_interrupt();
}

/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize the architecture specific part of the DW1000
 **/
void dw1000_arch_init()
{

  printf("dw1000_arch_init\n");
  d3s_dw1000_arch_init();

  // read antenna delay from otp memory. These devices come factory pre-calibrated.  antenna delays
  // in OTP memory are vendor specific and are therefore not set as part of the driver interface
#if DWM1001_LOAD_OTP_ANTENNA_DELAY
  uint16_t antenna_delay_prf_anchor = (uint16_t) ((dw_read_otp_32(0x01C) & 0xFFFF0000)  >> 8);
  uint16_t antenna_delay_prf_tag = (uint16_t) (dw_read_otp_32(0x01C) & 0x0000FFFF);

  printf("antenna_delay_prf_anchor: %d\n", antenna_delay_prf_anchor);
  printf("antenna_delay_prf_tag: %d\n", antenna_delay_prf_tag);

  // set tag delay as for both prf64 and prf16
  NETSTACK_RADIO.set_object(RADIO_LOC_ANTENNA_DELAY_PRF_64, &antenna_delay_prf_tag, sizeof(uint16_t));
  NETSTACK_RADIO.set_object(RADIO_LOC_ANTENNA_DELAY_PRF_16, &antenna_delay_prf_tag, sizeof(uint16_t));
#endif

  /* Performe a wake up in case of the node was in deepsleept before being restarted */
  NETSTACK_RADIO.set_value(RADIO_SLEEP_STATE, RADIO_REQUEST_WAKEUP);
  dw1000_us_delay(4000);
}

/**
 * \brief     Wait a delay in microsecond.
 *
 * \param ms  The delay in microsecond.
 **/
void dw1000_us_delay(int us){
  clock_delay_usec(us);
}

/**
 * Change the SPI frequency to freq.
 * If freq is bigger than the maximum SPI frequency value of the embedded
 * system set this maximum value.
 **/
void dw1000_arch_spi_set_clock_freq(uint32_t freq){
    /* printf("setting spi clock freq to %d\n", freq); */
    if(freq == DW_SPI_CLOCK_FREQ_IDLE_STATE) {
        dw1000_spi_set_fast_rate();
    } else {
        dw1000_spi_set_slow_rate();
    }
}

/**
 * Configure the embedeed system to allow the transceiver to go in
 * DEEPSLEEP state.
 * Disable SPICLK and SPIMISO to avoid leakeage current.
 **/
void dw1000_arch_init_deepsleep(void){
    // TODO
}


/**
 * Used in DEEPSLEEP state.
 * You need to drive HIGH the port WAKEUP or LOW the CSN port for min 500µs
 * to wakeup the transceiver.
 * You can check if thew transceiver have wakeup by reading the GPIO8 state
 * GPIO8 is drive HIGH when the transceiver comes in the IDLE state
 * It take approx 3ms to go out of the deepsleep state.
 **/
void dw1000_arch_wake_up(dw1000_pin_state state) {
    if(state == DW1000_PIN_ENABLE) {
        nrf_gpio_pin_clear(DW1000_SPI_CS_PIN);
    } else {
        nrf_gpio_pin_set(DW1000_SPI_CS_PIN);
    }
}

/**
 * Configure the embedded system to interact with the transceiver.
 * Enable SPICLK and SPIMISO.
 **/
void dw1000_arch_restore_idle_state(void){
    // TODO
}
