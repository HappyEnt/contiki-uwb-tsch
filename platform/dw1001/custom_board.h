/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#ifndef DW1001_H
#define DW1001_H


// Internal Pin Mapping for DW1001
#define DW_IRQ 19
#define DW_SCK 16
#define DW_MOSI 20
#define DW_MISO 18
#define DW_SPI_CS 17
#define DW_RST 24
#define ACC_IRQ 25

// Used both externally and internally
#define I2C_SDA 29
#define I2C_SCL 28

// LEDs definitions for DW1001
#define LEDS_NUMBER    4

#define LED_START 14
#define LED_1          14 // Red
#define LED_2          22 // Red
#define LED_3          31 // Blue
#define LED_4          30 // Green

#define LEDS_LIST { LED_1, LED_2, LED_3, LED_4 }

#define BSP_LED_0      LED_1
#define BSP_LED_1      LED_2
#define BSP_LED_2      LED_3
#define BSP_LED_3      LED_4

#define BSP_LED_0_MASK (1<<BSP_LED_0)
#define BSP_LED_1_MASK (1<<BSP_LED_1)
#define BSP_LED_2_MASK (1<<BSP_LED_2)
#define BSP_LED_3_MASK (1<<BSP_LED_3)

#define LEDS_MASK      (BSP_LED_0_MASK | BSP_LED_1_MASK | BSP_LED_2_MASK | BSP_LED_3_MASK)
/* all LEDs are lit when GPIO is low */
#define LEDS_INV_MASK  LEDS_MASK

#define BUTTONS_NUMBER 2

// TODO how is this used? mappings to internal nrf gpios is not consecutive, does this even work?
#define BUTTON_1       2 // BT_WAKE_UP
#define BUTTON_2       21 // RESETn
#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_LIST { BUTTON_1, BUTTON_2 }

#define BSP_BUTTON_0   BUTTON_1
#define BSP_BUTTON_1   BUTTON_2

#define BSP_BUTTON_0_MASK (1<<BSP_BUTTON_0)
#define BSP_BUTTON_1_MASK (1<<BSP_BUTTON_1)

// TODO how is this mask used?
#define BUTTONS_MASK   (BSP_BUTTON_0_MASK | BSP_BUTTON_1_MASK)

#define RX_PIN_NUMBER  11
#define TX_PIN_NUMBER  5
#define CTS_PIN_NUMBER 0x00 // unused on dw1000 dev
#define RTS_PIN_NUMBER 0x00 // unused on dw1000 dev
#define HWFC           false

#define SPIS_MISO_PIN   7  // SPI MISO signal.
#define SPIS_MOSI_PIN   6  // SPI CSN signal.
#define SPIS_CLK_PIN    4  // SPI MOSI signal.
#define SPIS_CSN_PIN    3  // SPI SCK signal.

/* #define SPIM0_SCK_PIN   29  // SPI clock GPIO pin number. */
/* #define SPIM0_MOSI_PIN  25  // SPI Master Out Slave In GPIO pin number. */
/* #define SPIM0_MISO_PIN  28  // SPI Master In Slave Out GPIO pin number. */
/* #define SPIM0_SS_PIN    12  // SPI Slave Select GPIO pin number. */

/* #define SPIM1_SCK_PIN   2   // SPI clock GPIO pin number. */
/* #define SPIM1_MOSI_PIN  3   // SPI Master Out Slave In GPIO pin number. */
/* #define SPIM1_MISO_PIN  4   // SPI Master In Slave Out GPIO pin number. */
/* #define SPIM1_SS_PIN    5   // SPI Slave Select GPIO pin number. */

/* #define SPIM2_SCK_PIN   12  // SPI clock GPIO pin number. */
/* #define SPIM2_MOSI_PIN  13  // SPI Master Out Slave In GPIO pin number. */
/* #define SPIM2_MISO_PIN  14  // SPI Master In Slave Out GPIO pin number. */
/* #define SPIM2_SS_PIN    15  // SPI Slave Select GPIO pin number. */

// serialization APPLICATION board - temp. setup for running serialized MEMU tests
/* #define SER_APP_RX_PIN              31    // UART RX pin number. */
/* #define SER_APP_TX_PIN              30    // UART TX pin number. */
/* #define SER_APP_CTS_PIN             28    // UART Clear To Send pin number. */
/* #define SER_APP_RTS_PIN             29    // UART Request To Send pin number. */

/* #define SER_APP_SPIM0_SCK_PIN        2     // SPI clock GPIO pin number. */
/* #define SER_APP_SPIM0_MOSI_PIN       4     // SPI Master Out Slave In GPIO pin number */
/* #define SER_APP_SPIM0_MISO_PIN       3     // SPI Master In Slave Out GPIO pin number */
/* #define SER_APP_SPIM0_SS_PIN        31     // SPI Slave Select GPIO pin number */
/* #define SER_APP_SPIM0_RDY_PIN       29     // SPI READY GPIO pin number */
/* #define SER_APP_SPIM0_REQ_PIN       30     // SPI REQUEST GPIO pin number */

/* // serialization CONNECTIVITY board */
/* #define SER_CON_RX_PIN              13    // UART RX pin number. */
/* #define SER_CON_TX_PIN              12    // UART TX pin number. */
/* #define SER_CON_CTS_PIN             14    // UART Clear To Send pin number. Not used if HWFC is set to false. */
/* #define SER_CON_RTS_PIN             15    // UART Request To Send pin number. Not used if HWFC is set to false. */


/* #define SER_CON_SPIS_SCK_PIN        29    // SPI SCK signal. */
/* #define SER_CON_SPIS_MOSI_PIN       25    // SPI MOSI signal. */
/* #define SER_CON_SPIS_MISO_PIN       28    // SPI MISO signal. */
/* #define SER_CON_SPIS_CSN_PIN        12    // SPI CSN signal. */
/* #define SER_CON_SPIS_RDY_PIN        14    // SPI READY GPIO pin number. */
/* #define SER_CON_SPIS_REQ_PIN        13    // SPI REQUEST GPIO pin number. */

/* #define SER_CONN_CHIP_RESET_PIN     27    // Pin used to reset connectivity chip */

/* // Low frequency clock source to be used by the SoftDevice */
#define NRF_CLOCK_LFCLKSRC      NRF_CLOCK_LFCLKSRC_XTAL_20_PPM

#endif // DW1001_H
