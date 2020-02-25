/*
 * Copyright (c) 2015, Zolertia - http://www.zolertia.com
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
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * \addtogroup zoul
 * @{
 *
 * \defgroup zoul-platforms Zolertia platforms based on the Zoul core module
 *
 * The Zoul allows a fast reuse and easy integration to most applications and
 * products.  Its small size and module format eases to place in different PCB
 * designs and to integrate in existing products.  The Zoul-based platforms
 * share most of the Zoul core implementation.
 *
 * \file
 *  Configuration for the Zoul-based platforms
 */
#ifndef CONTIKI_CONF_H_
#define CONTIKI_CONF_H_

#include <stdint.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
/* Include Project Specific conf */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */
/*---------------------------------------------------------------------------*/
/**
 * \name Compiler configuration and platform-specific type definitions
 *
 * Those values are not meant to be modified by the user
 * @{
 */
#define CLOCK_CONF_SECOND 128


/* CPU clock Speed */
//#define SYS_CTRL_CONF_SYS_DIV SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ


/* Compiler configurations */
#define CCIF
#define CLIF

/* Platform typedefs */
typedef uint32_t clock_time_t;
typedef uint32_t uip_stats_t;

/*
 * rtimer.h typedefs rtimer_clock_t as unsigned short. We need to define
 * RTIMER_CLOCK_DIFF to override this
 */
typedef uint32_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a, b)     ((int32_t)((a) - (b)))
/** @} */
/*---------------------------------------------------------------------------*/
/* Configuration for IEEE 802.15.4 NB radio */
#ifndef RADIO_DELAY_BEFORE_TX
/* 352us from calling transmit() until the SFD byte has been sent */
#define RADIO_DELAY_BEFORE_TX     ((unsigned)US_TO_RTIMERTICKS(352))
#endif

#ifndef RADIO_DELAY_BEFORE_RX
/* 192us as in datasheet but ACKs are not always received, so adjusted to 250us */
#define RADIO_DELAY_BEFORE_RX     ((unsigned)US_TO_RTIMERTICKS(250))
#endif

#ifndef RADIO_DELAY_BEFORE_DETECT
#define RADIO_DELAY_BEFORE_DETECT 0
#endif
/*---------------------------------------------------------------------------*/
/**
 * \name Serial Boot Loader Backdoor configuration
 *
 * @{
 */
#ifndef FLASH_CCA_CONF_BOOTLDR_BACKDOOR
#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR 1 /**<Enable the boot loader backdoor */
#endif

#ifndef FLASH_CCA_CONF_BOOTLDR_BACKDOOR_PORT_A_PIN
#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR_PORT_A_PIN 3 /**< Pin PA_3 (user button), activates the boot loader */
#endif

#ifndef FLASH_CCA_CONF_BOOTLDR_BACKDOOR_ACTIVE_HIGH
#define FLASH_CCA_CONF_BOOTLDR_BACKDOOR_ACTIVE_HIGH 0 /**< A logic low level activates the boot loader */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name CC2538 System Control configuration
 *
 * @{
 */
#ifndef SYS_CTRL_CONF_OSC32K_USE_XTAL
#define SYS_CTRL_CONF_OSC32K_USE_XTAL   1 /**< Use the on-board 32.768-kHz crystal */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name CFS configuration
 *
 * @{
 */
#ifndef COFFEE_CONF_SIZE
#define COFFEE_CONF_SIZE            (4 * COFFEE_SECTOR_SIZE)
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Watchdog Timer configuration
 *
 * @{
 */
#ifndef WATCHDOG_CONF_ENABLE
#define WATCHDOG_CONF_ENABLE        1 /**< Enable the watchdog timer */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name USB 'core' configuration
 *
 * Those values are not meant to be modified by the user, except where stated
 * otherwise
 * @{
 */
#define CTRL_EP_SIZE                8
#define USB_EP1_SIZE               32
#define USB_EP2_SIZE               64
#define USB_EP3_SIZE               64
#define USB_ARCH_WRITE_NOTIFY       0

#ifndef USB_ARCH_CONF_DMA
#define USB_ARCH_CONF_DMA           1 /**< Change to Enable/Disable USB DMA */

#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Generic Configuration directives
 *
 * @{
 */
#ifndef ENERGEST_CONF_ON
#define ENERGEST_CONF_ON            0 /**< Energest Module */
#endif

#ifndef STARTUP_CONF_VERBOSE
#define STARTUP_CONF_VERBOSE        1 /**< Set to 0 to decrease startup verbosity */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name uDMA Configuration and channel allocations
 *
 * @{
 */
#define USB_ARCH_CONF_RX_DMA_CHAN   0 /**< USB -> RAM DMA channel */
#define USB_ARCH_CONF_TX_DMA_CHAN   1 /**< RAM -> USB DMA channel */
#define CC2538_RF_CONF_TX_DMA_CHAN  2 /**< RF -> RAM DMA channel */
#define CC2538_RF_CONF_RX_DMA_CHAN  3 /**< RAM -> RF DMA channel */
#define DW1000_CONF_RX_DMA_SPI_CHAN  10 /**< SPI -> RAM DMA channel */
#define DW1000_CONF_TX_DMA_SPI_CHAN  11 /**< RAM -> SPI DMA channel */
#define UDMA_CONF_MAX_CHANNEL       DW1000_CONF_TX_DMA_SPI_CHAN
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Character I/O Configuration
 *
 * @{
 */
#ifndef UART_CONF_ENABLE
#define UART_CONF_ENABLE            1 /**< Enable/Disable UART I/O */
#endif

#ifndef UART0_CONF_BAUD_RATE
#define UART0_CONF_BAUD_RATE   115200 /**< Default UART0 baud rate */
#endif

#ifndef UART1_CONF_BAUD_RATE
#define UART1_CONF_BAUD_RATE   115200 /**< Default UART1 baud rate */
#endif

#ifndef SLIP_ARCH_CONF_USB
#define SLIP_ARCH_CONF_USB          0 /**< SLIP over UART by default */
#endif

#ifndef DBG_CONF_USB
#define DBG_CONF_USB                0 /**< All debugging over UART by default */
#endif

#ifndef SERIAL_LINE_CONF_UART
#define SERIAL_LINE_CONF_UART       0 /**< UART to use with serial line */
#endif

#if !SLIP_ARCH_CONF_USB
#ifndef SLIP_ARCH_CONF_UART
#define SLIP_ARCH_CONF_UART         0 /**< UART to use with SLIP */
#endif
#endif

#if !DBG_CONF_USB
#ifndef DBG_CONF_UART
#define DBG_CONF_UART               0 /**< UART to use for debugging */
#endif
#endif

#ifndef UART1_CONF_UART
#define UART1_CONF_UART             0 /**< UART to use for examples relying on
                                           the uart1_* API */
#endif

/* Turn off example-provided putchars */
#define SLIP_BRIDGE_CONF_NO_PUTCHAR 1
#define SLIP_RADIO_CONF_NO_PUTCHAR  1

#ifndef SLIP_ARCH_CONF_ENABLED
/*
 * Determine whether we need SLIP
 * This will keep working while UIP_FALLBACK_INTERFACE and CMD_CONF_OUTPUT
 * keep using SLIP
 */
#if defined(UIP_FALLBACK_INTERFACE) || defined(CMD_CONF_OUTPUT)
#define SLIP_ARCH_CONF_ENABLED      1
#endif
#endif

/**
 * \brief Define this as 1 to build a headless node.
 *
 * The UART will not be initialised its clock will be gated, offering some
 * energy savings. The USB will not be initialised either
 */
#ifndef CC2538_CONF_QUIET
#define CC2538_CONF_QUIET           0
#endif

/* CC2538_CONF_QUIET is hard and overrides all other related defines */
#if CC2538_CONF_QUIET
#undef USB_SERIAL_CONF_ENABLE
#define USB_SERIAL_CONF_ENABLE      0

#undef UART_CONF_ENABLE
#define UART_CONF_ENABLE            0

#undef STARTUP_CONF_VERBOSE
#define STARTUP_CONF_VERBOSE        0
#endif /* CC2538_CONF_QUIET */

/**
 * \brief Enable the USB core only if we need it
 */
#ifndef USB_SERIAL_CONF_ENABLE
#define USB_SERIAL_CONF_ENABLE \
  ((SLIP_ARCH_CONF_USB & SLIP_ARCH_CONF_ENABLED) | \
   DBG_CONF_USB)
#endif

/*
 * If debugging and SLIP use the same peripheral, this will be 1. Don't modify
 * this
 */
#if SLIP_ARCH_CONF_ENABLED
#define DBG_CONF_SLIP_MUX (SLIP_ARCH_CONF_USB == DBG_CONF_USB && \
                           (SLIP_ARCH_CONF_USB || \
                            SLIP_ARCH_CONF_UART == DBG_CONF_UART))
#endif

/*
 * Automatic detection of whether a specific UART is in use
 */
#define UART_IN_USE_BY_SERIAL_LINE(u) (SERIAL_LINE_CONF_UART == (u))
#define UART_IN_USE_BY_SLIP(u)        (SLIP_ARCH_CONF_ENABLED && \
                                       !SLIP_ARCH_CONF_USB && \
                                       SLIP_ARCH_CONF_UART == (u))
#define UART_IN_USE_BY_DBG(u)         (!DBG_CONF_USB && DBG_CONF_UART == (u))
#define UART_IN_USE_BY_UART1(u)       (UART1_CONF_UART == (u))

#define UART_IN_USE(u) ( \
  UART_CONF_ENABLE && \
  (UART_IN_USE_BY_SERIAL_LINE(u) || \
   UART_IN_USE_BY_SLIP(u) || \
   UART_IN_USE_BY_DBG(u) || \
   UART_IN_USE_BY_UART1(u)) \
)
/** @} */
/*---------------------------------------------------------------------------*/
/* board.h assumes that basic configuration is done */
#include "board.h"
/*---------------------------------------------------------------------------*/
/**
 * \name Network Stack Configuration
 *
 * @{
 */
#ifndef NETSTACK_CONF_NETWORK
#if NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_NETWORK sicslowpan_driver
#else
#define NETSTACK_CONF_NETWORK rime_driver
#endif /* NETSTACK_CONF_WITH_IPV6 */
#endif /* NETSTACK_CONF_NETWORK */

#ifndef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC     csma_driver
#endif

#ifndef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC     contikimac_driver
#endif

/* Configure NullRDC for when it's selected */
#define NULLRDC_CONF_802154_AUTOACK             1
#define NULLRDC_CONF_802154_AUTOACK_HW			    1

/* Configure ContikiMAC for when it's selected */
#define CONTIKIMAC_CONF_WITH_PHASE_OPTIMIZATION 0
#define WITH_FAST_SLEEP                         1

#ifndef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE    8
#endif

#ifndef NETSTACK_CONF_FRAMER
#if NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_FRAMER  framer_802154
#else /* NETSTACK_CONF_WITH_IPV6 */
#define NETSTACK_CONF_FRAMER  contikimac_framer
#endif /* NETSTACK_CONF_WITH_IPV6 */
#endif /* NETSTACK_CONF_FRAMER */

#if CC1200_CONF_SUBGHZ_50KBPS_MODE
#define NETSTACK_CONF_RADIO                                 cc1200_driver
#define CC1200_CONF_RF_CFG                                  cc1200_802154g_863_870_fsk_50kbps
#define ANTENNA_SW_SELECT_DEF_CONF                          ANTENNA_SW_SELECT_SUBGHZ
#define CC1200_CONF_USE_GPIO2                               0
#define CC1200_CONF_USE_RX_WATCHDOG                         0

#define NULLRDC_CONF_ACK_WAIT_TIME                          (RTIMER_SECOND / 200)
#define NULLRDC_CONF_AFTER_ACK_DETECTED_WAIT_TIME           (RTIMER_SECOND / 1500)
#define NULLRDC_CONF_802154_AUTOACK                         1
#define NULLRDC_CONF_802154_AUTOACK_HW                      1
#define NULLRDC_CONF_SEND_802154_ACK                        0

#define CONTIKIMAC_CONF_CCA_CHECK_TIME                      (RTIMER_ARCH_SECOND / 800)
#define CONTIKIMAC_CONF_CCA_SLEEP_TIME                      (RTIMER_ARCH_SECOND / 120)
#define CONTIKIMAC_CONF_LISTEN_TIME_AFTER_PACKET_DETECTED   (RTIMER_ARCH_SECOND / 8)
#define CONTIKIMAC_CONF_AFTER_ACK_DETECTED_WAIT_TIME        (RTIMER_SECOND / 300)
#define CONTIKIMAC_CONF_INTER_PACKET_INTERVAL               (RTIMER_SECOND / 200)
#endif

/* This can be overriden to use the cc1200_driver instead */
#ifndef NETSTACK_CONF_RADIO
#define NETSTACK_CONF_RADIO         cc2538_rf_driver
#endif

/*
 * RE-Mote specific:
 * If dual RF enabled, we set the RF switch to enable the CC1200 and use 2.4GHz
 * on the available uFl/chip antenna (not mounted as default).  In contiki main
 * platform routine we set the right antenna depending on NETSTACK_CONF_RADIO,
 * but as changing the RF antenna also implies enabling/disabling the CC1200,
 * is better to start off with the right configuration
 */
#if REMOTE_DUAL_RF_ENABLED
#define ANTENNA_SW_SELECT_DEFAULT ANTENNA_SW_SELECT_SUBGHZ
#else /* REMOTE_DUAL_RF_ENABLED */
#ifndef ANTENNA_SW_SELECT_DEF_CONF
#define ANTENNA_SW_SELECT_DEFAULT ANTENNA_SW_SELECT_2_4GHZ
#else /* ANTENNA_SW_SELECT_DEF_CONF */
#define ANTENNA_SW_SELECT_DEFAULT ANTENNA_SW_SELECT_DEF_CONF
#endif /* ANTENNA_SW_SELECT_DEF_CONF */
#endif /* REMOTE_DUAL_RF_ENABLED */

#if RADIO_DRIVER_UWB

  /* configuration of the DW1000 radio driver */
  #undef NETSTACK_CONF_RADIO
  #define NETSTACK_CONF_RADIO         dw1000_driver
  #define DW1000_IEEE802154_EXTENDED  0

  #if DW1000_IEEE802154_EXTENDED
    #define PACKETBUF_CONF_SIZE       266
  #endif

  #define DW1000_CHANNEL              0
  #define DW1000_DATA_RATE            DW_DATA_RATE_6800_KBPS
  #define DW1000_PREAMBLE             DW_PREAMBLE_LENGTH_256
  #define DW1000_PRF                  DW_PRF_16_MHZ
  #define DW1000_TSCH                 1

// #define RADIO_DELAY_MEASUREMENT 1


  #define DW1000_ARCH_CONF_DMA        1


  /* Preamble transmission + SFD symbols
    1 µs by preamble symbols and 1 µs by SFD symbols
    The length of the SFD is 64 symbols at 110 kbps and 
    16 symbols for all other bit rates 
    (according to the transceiver configuration)*/
  #define UWB_T_SHR                  ((uint16_t) (256+16))

  /* time from calling transmit() until the SFD byte has been sent 
  Can be recomputed be adding the macro "RADIO_DELAY_MEASUREMENT" to 1
  in the radio driver and be calling NETSTACK_CONF_RADIO.transmit()
  Dependant of the configuration (DATA_RATE, PREAMBLE_LENGHT)*/
  #undef RADIO_DELAY_BEFORE_TX
  #define RADIO_DELAY_BEFORE_TX     ((uint16_t) US_TO_RTIMERTICKS(UWB_T_SHR+10))
  // #define RADIO_DELAY_BEFORE_TX     0

  /* the call of NETSTACK_CONF_RADIO.on take 53us, not dependent of the configuration.
  The radio is ready to receive 16 µs after this call. 
  Can be recomputed be adding the macro "RADIO_DELAY_MEASUREMENT" to 1
  in the radio driver. You need to call NETSTACK_CONF_RADIO.off() and after 
  NETSTACK_CONF_RADIO.on()*/
  #undef RADIO_DELAY_BEFORE_RX
  #define RADIO_DELAY_BEFORE_RX     ((unsigned) US_TO_RTIMERTICKS(10))
  // #define RADIO_DELAY_BEFORE_RX     0

  /* The delay between the reception of the SFD and the trigger by the radio */
  #undef RADIO_DELAY_BEFORE_DETECT
  #define RADIO_DELAY_BEFORE_DETECT          0

  // #define RADIO_DELAY_BEFORE_DETECT          ((unsigned)US_TO_RTIMERTICKS(122))

  /* TSCH channel hopping sequence, define for the UWB, in this case we have only 6 channels */
  /* We avoid to used the TSCH channel 2 and 5 that use physical channel 2 */
  #undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
  #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){ 7, 6, 4, 0, 2, 3}
  // #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){1, 7, 6, 5, 0, 4, 2, 3}
  // #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){7}
  // #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){ 3}
  #define TSCH_CONF_JOIN_HOPPING_SEQUENCE     TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
  // #define TSCH_CONF_JOIN_HOPPING_SEQUENCE     (uint8_t[]){ 3}
  #define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN  6
  // #define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN  8
  #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   5000

  // #undef TSCH_CONF_RADIO_ON_DURING_TIMESLOT
  // #define TSCH_CONF_RADIO_ON_DURING_TIMESLOT 1

  // #define TSCH_CONF_MAX_INCOMING_PACKETS      8


  /* Calculate packet tx/rx duration in RTIMER ticks based on sent
   * packet length in bytes with 802.15.4 UWB 110, 850 or 6810 kbps data rate.
   * One byte = 32us at 250 kbps.
   * One byte = 53.3us at 110 kbps.
   * One byte = 9.4us at 850 kbps.
   * One byte = 1.17us at 6810 kbps.
   * Add two bytes for CRC
   * Add 172 for the PHR transmission at 110 kbps
   * Add 22 for the PHR transmission at 850 kpbs or more
   * The SHR is not take into account here (see UWB_T_SHR)
   * The value do not need to be perfectly correct because
   * the value will by round in RTIMER tick*/
  #undef TSCH_PACKET_DURATION
  #define TSCH_PACKET_DURATION(len) (US_TO_RTIMERTICKS(22 + (117 * (len + 2))/100))
  
  /* We use the SFD timestamp gived by the DW1000 transceiver to do the synchronization */
  #define TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS 1

  /* EB period is 3.42 seconds */
  // #define TSCH_CONF_EB_PERIOD ((342*CLOCK_SECOND)/100)

  /*Time to desynch assuming a drift of 40 PPM (80 PPM between two nodes) and guard time of +/-0.5ms: 6.25s. */
  // #define TSCH_CONF_KEEPALIVE_TIMEOUT (10*CLOCK_SECOND)
  // #define TSCH_CONF_MAX_EB_PERIOD (10*CLOCK_SECOND)
  // #define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT (20*CLOCK_SECOND)
  #define TSCH_CONF_SLEEP 1

  /* Used to start the slot in advance to avoid miss deadline because of the slow processing speed */
  
  #if TSCH_CONF_SLEEP
  #define TSCH_CONF_SLOT_START_BEFOREHAND ((unsigned) US_TO_RTIMERTICKS(250+3400))
  #else
  #define TSCH_CONF_SLOT_START_BEFOREHAND ((unsigned) US_TO_RTIMERTICKS(250))
  #endif /* TSCH_CONF_SLEEP */

  /* change the clock of the CPU 32 MHZ in place of 16 */
  // #define SYS_CTRL_CONF_SYS_DIV SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

#endif /* RADIO_DRIVER_UWB */

/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name LPM configuration
 * @{
 */
#ifndef LPM_CONF_ENABLE
#define LPM_CONF_ENABLE       1 /**< Set to 0 to disable LPM entirely */
#endif

/**
 * \brief Maximum PM
 *
 * The SoC will never drop to a Power Mode deeper than the one specified here.
 * 0 for PM0, 1 for PM1 and 2 for PM2
 */
#ifndef LPM_CONF_MAX_PM
#define LPM_CONF_MAX_PM       2
#endif

#ifndef LPM_CONF_STATS
#define LPM_CONF_STATS        0 /**< Set to 1 to enable LPM-related stats */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name IEEE address configuration
 *
 * Used to generate our RIME & IPv6 address
 * @{
 */
/**
 * \brief Location of the IEEE address
 * 0 => Read from InfoPage,
 * 1 => Use a hardcoded address, configured by IEEE_ADDR_CONF_ADDRESS
 */
#ifndef IEEE_ADDR_CONF_HARDCODED
#define IEEE_ADDR_CONF_HARDCODED             0
#endif

/**
 * \brief The hardcoded IEEE address to be used when IEEE_ADDR_CONF_HARDCODED
 * is defined as 1
 */
#ifndef IEEE_ADDR_CONF_ADDRESS
#define IEEE_ADDR_CONF_ADDRESS { 0x00, 0x12, 0x4B, 0x00, 0x89, 0xAB, 0xCD, 0xEF }
#endif

/**
 * \brief Location of the IEEE address in the InfoPage when
 * IEEE_ADDR_CONF_HARDCODED is defined as 0
 * 0 => Use the primary address location
 * 1 => Use the secondary address location
 */
#ifndef IEEE_ADDR_CONF_USE_SECONDARY_LOCATION
#define IEEE_ADDR_CONF_USE_SECONDARY_LOCATION 1
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name RF configuration
 *
 * @{
 */
/* RF Config */
#ifndef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID           0xABCD
#endif

#ifdef RF_CHANNEL
#define CC2538_RF_CONF_CHANNEL      RF_CHANNEL
#endif

#ifndef CC2538_RF_CONF_CHANNEL
#define CC2538_RF_CONF_CHANNEL              26
#endif /* CC2538_RF_CONF_CHANNEL */

#ifndef CC2538_RF_CONF_AUTOACK
#define CC2538_RF_CONF_AUTOACK               1 /**< RF H/W generates ACKs */
#endif /* CC2538_CONF_AUTOACK */

#ifndef CC2538_RF_CONF_TX_USE_DMA
#define CC2538_RF_CONF_TX_USE_DMA            1 /**< RF TX over DMA */
#endif

#ifndef CC2538_RF_CONF_RX_USE_DMA
#define CC2538_RF_CONF_RX_USE_DMA            1 /**< RF RX over DMA */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name IPv6, RIME and network buffer configuration
 *
 * @{
 */

/* Don't let contiki-default-conf.h decide if we are an IPv6 build */
#ifndef NETSTACK_CONF_WITH_IPV6
#define NETSTACK_CONF_WITH_IPV6              0
#endif

#if NETSTACK_CONF_WITH_IPV6
/* Addresses, Sizes and Interfaces */
/* 8-byte addresses here, 2 otherwise */
#define LINKADDR_CONF_SIZE                   8
#define UIP_CONF_LL_802154                   1
#define UIP_CONF_LLH_LEN                     0
#define UIP_CONF_NETIF_MAX_ADDRESSES         3

/* TCP, UDP, ICMP */
#ifndef UIP_CONF_TCP
#define UIP_CONF_TCP                         1
#endif
#ifndef UIP_CONF_TCP_MSS
#define UIP_CONF_TCP_MSS                    64
#endif
#define UIP_CONF_UDP                         1
#define UIP_CONF_UDP_CHECKSUMS               1
#define UIP_CONF_ICMP6                       1

/* ND and Routing */
#ifndef UIP_CONF_ROUTER
#define UIP_CONF_ROUTER                      1
#endif

#define UIP_CONF_ND6_SEND_RA                 0
#define UIP_CONF_IP_FORWARD                  0
#define RPL_CONF_STATS                       0

#define UIP_CONF_ND6_REACHABLE_TIME     600000
#define UIP_CONF_ND6_RETRANS_TIMER       10000

#ifndef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS        16
#endif
#ifndef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES                 16
#endif

/* uIP */
#ifndef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE              1300
#endif

#define UIP_CONF_IPV6_QUEUE_PKT              0
#define UIP_CONF_IPV6_CHECKS                 1
#define UIP_CONF_IPV6_REASSEMBLY             0
#define UIP_CONF_MAX_LISTENPORTS             8

/* 6lowpan */
#define SICSLOWPAN_CONF_COMPRESSION          SICSLOWPAN_COMPRESSION_HC06
#ifndef SICSLOWPAN_CONF_COMPRESSION_THRESHOLD
#define SICSLOWPAN_CONF_COMPRESSION_THRESHOLD 63
#endif
#ifndef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG                 1
#endif
#define SICSLOWPAN_CONF_MAXAGE               8

/* Define our IPv6 prefixes/contexts here */
#define SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS    1
#ifndef SICSLOWPAN_CONF_ADDR_CONTEXT_0
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 { \
  addr_contexts[0].prefix[0] = UIP_DS6_DEFAULT_PREFIX_0; \
  addr_contexts[0].prefix[1] = UIP_DS6_DEFAULT_PREFIX_1; \
}
#endif

#define MAC_CONF_CHANNEL_CHECK_RATE          8

#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                    8
#endif
/*---------------------------------------------------------------------------*/
#else /* NETSTACK_CONF_WITH_IPV6 */
/* Network setup for non-IPv6 (rime). */
#define UIP_CONF_IP_FORWARD                  1

#ifndef UIP_CONF_BUFFER_SIZE
#define UIP_CONF_BUFFER_SIZE               108
#endif

#define RIME_CONF_NO_POLITE_ANNOUCEMENTS     0

#ifndef QUEUEBUF_CONF_NUM
#define QUEUEBUF_CONF_NUM                    8
#endif

#endif /* NETSTACK_CONF_WITH_IPV6 */
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Security
 *
 * @{
 */
#ifndef CRYPTO_CONF_INIT
#define CRYPTO_CONF_INIT        1 /**< Whether to init cryptoprocessor */
#endif

#ifndef AES_128_CONF
#define AES_128_CONF            cc2538_aes_128_driver /**< AES-128 driver */
#endif

#ifndef CCM_STAR_CONF
#define CCM_STAR_CONF           cc2538_ccm_star_driver /**< AES-CCM* driver */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name RTC
 *
 * @{
 */
#ifdef PLATFORM_HAS_RTC

#ifndef RTC_CONF_INIT
#define RTC_CONF_INIT   0 /**< Whether to initialize the RTC */
#endif

#ifndef RTC_CONF_SET_FROM_SYS
#define RTC_CONF_SET_FROM_SYS    0 /**< Whether to set the RTC from the build system */
#endif

#else
#undef RTC_CONF_INIT
#define RTC_CONF_INIT   0
#endif
/** @} */
/*---------------------------------------------------------------------------*/

#endif /* CONTIKI_CONF_H_ */

/** @} */
