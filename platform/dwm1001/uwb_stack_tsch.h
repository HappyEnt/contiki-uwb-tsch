#ifndef TSCH_UWB_CONF_H
#define TSCH_UWB_CONF_H

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

#if RADIO_DRIVER_UWB

  #include "dev/dw1000/dw1000.h"

  #define UWB_SMART_TX_POWER 1
  /* configuration of the DW1000 radio driver */
  #undef NETSTACK_CONF_RADIO
  #define NETSTACK_CONF_RADIO         dw1000_driver
  #define DW1000_IEEE802154_EXTENDED  0

  #if DW1000_IEEE802154_EXTENDED
    #define PACKETBUF_CONF_SIZE       266
  #endif

  #ifndef DW1000_CHANNEL
  #define DW1000_CHANNEL              0
  #endif
  #ifndef DW1000_DATA_RATE
  #define DW1000_DATA_RATE            DW_DATA_RATE_110_KBPS
  #endif
  #ifndef DW1000_PRF
  #define DW1000_PRF                  DW_PRF_16_MHZ
  #endif
  #ifndef DW1000_TSCH
  #define DW1000_TSCH                 1
  #endif

#define RADIO_DELAY_MEASUREMENT 0


  #define DW1000_ARCH_CONF_DMA        1

#if DW1000_DATA_RATE == DW_DATA_RATE_6800_KBPS
  #define DW1000_PREAMBLE             DW_PREAMBLE_LENGTH_128
  /* #define DW1000_PREAMBLE             DW_PREAMBLE_LENGTH_256 */
  /* UWB_T_SHR = (Preamble lenght + 16) at 6.8 mbps */
  #define UWB_T_SHR                  ((uint16_t) (128+16))
  /* #define UWB_T_SHR                  ((uint16_t) (256+16)) */
  /* #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   2500 */
  #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   5000
  /* #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   10000 */
  /* #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   25000 */

#elif DW1000_DATA_RATE == DW_DATA_RATE_850_KBPS
  #define DW1000_PREAMBLE             DW_PREAMBLE_LENGTH_512
  /* UWB_T_SHR = (Preamble lenght + 16) at 850 mbps */
  #define UWB_T_SHR                  ((uint16_t) (512+16))
  #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   7500
  /* #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   10000 */

#else /* DW1000_DATA_RATE == DW_DATA_RATE_110_KBPS */
  #define DW1000_PREAMBLE             DW_PREAMBLE_LENGTH_1024
  /* UWB_T_SHR = (Preamble lenght + 64) at 110 kbps */
  #define UWB_T_SHR                  ((uint16_t) (1024+64))

  #define TSCH_CONF_DEFAULT_TIMESLOT_LENGTH   25000
#endif /* DW1000_DATA_RATE */
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
  /* #undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE */
  /* #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){3, 7} // this configuration uses only UWB Channel 5 with PRF 64 and PRF 16 */
  /* #define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN  2 */

#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){7} // this configuration uses only UWB Channel 5 with PRF 64 and PRF 16
/* #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){7} // this configuration uses only UWB Channel 5 with PRF 64 and PRF 16 */
#define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN    1

#define TSCH_CONF_DEFAULT_RANGE_HOPPING_SEQUENCE  (uint8_t[]){7} // this configuration uses only UWB Channel 5 with PRF 64 and PRF 16
#define TSCH_CONF_HOPPING_RANGE_SEQUENCE_MAX_LEN   1

/* #define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  (uint8_t[]){1, 7, 6, 5, 0, 4, 2, 3, 9, 10, 8, 11} */
/* #define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN  12 */


// NOTE if we use only a single channel here, the tsch join process won't change channels
// for some reason this does result in beacons not being received anymore, if the receiver
// did not receive the beacon directly at turn on
#define TSCH_CONF_JOIN_HOPPING_SEQUENCE     (uint8_t[]){3, 7}

/* Chorus Config : */
#ifdef CHORUS_CONFIG
#undef TSCH_CONF_DEFAULT_HOPPING_SEQUENCE
#undef TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN
#define TSCH_CONF_DEFAULT_HOPPING_SEQUENCE  TSCH_CHORUS_HOPPING_SEQUENCE
#define TSCH_CONF_HOPPING_SEQUENCE_MAX_LEN  TSCH_CHORUS_HOPPING_SEQUENCE_MAX_LEN
#endif /* CHORUS_CONFIG */



  /* #undef TSCH_CONF_RADIO_ON_DURING_TIMESLOT */
  /* #define TSCH_CONF_RADIO_ON_DURING_TIMESLOT 1 */

  // #define TSCH_CONF_MAX_INCOMING_PACKETS      8


  /* We use the SFD timestamp gived by the DW1000 transceiver to do the synchronization */
  #define TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS 1

  /* EB period is 3.42 seconds */
  /* #define TSCH_CONF_EB_PERIOD ((CLOCK_SECOND)/100) */

  /*Time to desynch assuming a drift of 40 PPM (80 PPM between two nodes) and guard time of +/-0.5ms: 6.25s. */
  // #define TSCH_CONF_KEEPALIVE_TIMEOUT (10*CLOCK_SECOND)
  // #define TSCH_CONF_MAX_EB_PERIOD (10*CLOCK_SECOND)
  // #define TSCH_CONF_MAX_KEEPALIVE_TIMEOUT (20*CLOCK_SECOND)
#ifndef TSCH_CONF_SLEEP
  #define TSCH_CONF_SLEEP 0
#endif

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


#endif /* __PROJECT_CONF_H__ */
