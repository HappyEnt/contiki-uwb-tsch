 /*
 * Copyright 2013 (c) DecaWave Ltd, Dublin, Ireland.
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
 *
 */
#include <stdio.h>
#include <stdlib.h>

#include "dw1000.h"
#include "dw1000-driver.h"
#include "cc.h"

#include "dw1000-ranging-bias.h"

/* number of ranging bias correction for each PRF */
#define NUM_16M_OFFSET    (37)
#define NUM_16M_OFFSETWB  (68)
#define NUM_64M_OFFSET    (26)
#define NUM_64M_OFFSETWB  (59)
#define NUM_CH_SUPPORTED   8

/* Only channels 1,2,3 and 5 are in the narrow band tables */
const uint8_t chan_idxnb[NUM_CH_SUPPORTED] = {0, 0, 1, 2, 0, 3, 0, 0};
/* 0nly channels 4 and 7 are in in the wide band tables */
const uint8_t chan_idxwb[NUM_CH_SUPPORTED] = {0, 0, 0, 0, 0, 0, 0, 1};
/*-----------------------------------------------------------------------------
 *  Range Bias Correction TABLES of range values in integer units of 25 cm,
 *  for 8-bit unsigned storage, MUST END IN 255 !!!!!!
 *----------------------------------------------------------------------------*/

/* offsets to nearest centimeter for index 0, all rest are +1 cm per value */
#define CM_OFFSET_16M_NB  (-23) /* for normal band channels at 16 MHz PRF */
#define CM_OFFSET_16M_WB  (-28) /* for wider  band channels at 16 MHz PRF */
#define CM_OFFSET_64M_NB  (-17) /* for normal band channels at 64 MHz PRF */
#define CM_OFFSET_64M_WB  (-30) /* for wider  band channels at 64 MHz PRF */


#define CENTIMETER_TO_DWTIME 2.13139451293f

/*-----------------------------------------------------------------------------
 * range25cm16PRFnb: Range Bias Correction table for narrow band channels at
 *  16 MHz PRF, NB: !!!! each MUST END IN 255 !!!!
 *----------------------------------------------------------------------------*/


const uint8_t range25cm16PRFnb[4][NUM_16M_OFFSET] =
{
    /* ch 1 - range25cm16PRFnb */
    {
           1,
           3,
           4,
           5,
           7,
           9,
          11,
          12,
          13,
          15,
          18,
          20,
          23,
          25,
          28,
          30,
          33,
          36,
          40,
          43,
          47,
          50,
          54,
          58,
          63,
          66,
          71,
          76,
          82,
          89,
          98,
         109,
         127,
         155,
         222,
         255,
         255
    },

    /* ch 2 - range25cm16PRFnb */
    {
           1,
           2,
           4,
           5,
           6,
           8,
           9,
          10,
          12,
          13,
          15,
          18,
          20,
          22,
          24,
          27,
          29,
          32,
          35,
          38,
          41,
          44,
          47,
          51,
          55,
          58,
          62,
          66,
          71,
          78,
          85,
          96,
         111,
         135,
         194,
         240,
         255
    },

    /* ch 3 - range25cm16PRFnb */
    {
           1,
           2,
           3,
           4,
           5,
           7,
           8,
           9,
          10,
          12,
          14,
          16,
          18,
          20,
          22,
          24,
          26,
          28,
          31,
          33,
          36,
          39,
          42,
          45,
          49,
          52,
          55,
          59,
          63,
          69,
          76,
          85,
          98,
         120,
         173,
         213,
         255
    },

    /* ch 5 - range25cm16PRFnb */
    {
           1,
           1,
           2,
           3,
           4,
           5,
           6,
           6,
           7,
           8,
           9,
          11,
          12,
          14,
          15,
          16,
          18,
          20,
          21,
          23,
          25,
          27,
          29,
          31,
          34,
          36,
          38,
          41,
          44,
          48,
          53,
          59,
          68,
          83,
         120,
         148,
         255
    }
}; /* end range25cm16PRFnb */


/*-----------------------------------------------------------------------------
 * range25cm16PRFwb: Range Bias Correction table for wide band channels at
 *  16 MHz PRF, NB: !!!! each MUST END IN 255 !!!!
 *---------------------------------------------------------------------------*/

const uint8_t range25cm16PRFwb[2][NUM_16M_OFFSETWB] =
{
    /* ch 4 - range25cm16PRFwb */
    {
           7,
           7,
           8,
           9,
           9,
          10,
          11,
          11,
          12,
          13,
          14,
          15,
          16,
          17,
          18,
          19,
          20,
          21,
          22,
          23,
          24,
          26,
          27,
          28,
          30,
          31,
          32,
          34,
          36,
          38,
          40,
          42,
          44,
          46,
          48,
          50,
          52,
          55,
          57,
          59,
          61,
          63,
          66,
          68,
          71,
          74,
          78,
          81,
          85,
          89,
          94,
          99,
         104,
         110,
         116,
         123,
         130,
         139,
         150,
         164,
         182,
         207,
         238,
         255,
         255,
         255,
         255,
         255
    },

    /* ch 7 - range25cm16PRFwb */
    {
           4,
           5,
           5,
           5,
           6,
           6,
           7,
           7,
           7,
           8,
           9,
           9,
          10,
          10,
          11,
          11,
          12,
          13,
          13,
          14,
          15,
          16,
          17,
          17,
          18,
          19,
          20,
          21,
          22,
          23,
          25,
          26,
          27,
          29,
          30,
          31,
          32,
          34,
          35,
          36,
          38,
          39,
          40,
          42,
          44,
          46,
          48,
          50,
          52,
          55,
          58,
          61,
          64,
          68,
          72,
          75,
          80,
          85,
          92,
         101,
         112,
         127,
         147,
         168,
         182,
         194,
         205,
         255
    }
}; /* end range25cm16PRFwb */

/*------------------------------------------------------------------------------
 * range25cm64PRFnb: Range Bias Correction table for narrow band channels at 64 MHz PRF, NB: !!!! each MUST END IN 255 !!!!
 *----------------------------------------------------------------------------*/

const uint8_t range25cm64PRFnb[4][NUM_64M_OFFSET] =
{
    /* ch 1 - range25cm64PRFnb */
    {
           1,
           2,
           2,
           3,
           4,
           5,
           7,
          10,
          13,
          16,
          19,
          22,
          24,
          27,
          30,
          32,
          35,
          38,
          43,
          48,
          56,
          78,
         101,
         120,
         157,
         255
    },

    /* ch 2 - range25cm64PRFnb */
    {
           1,
           2,
           2,
           3,
           4,
           4,
           6,
           9,
          12,
          14,
          17,
          19,
          21,
          24,
          26,
          28,
          31,
          33,
          37,
          42,
          49,
          68,
          89,
         105,
         138,
         255
    },

    /* ch 3 - range25cm64PRFnb */
    {
           1,
           1,
           2,
           3,
           3,
           4,
           5,
           8,
          10,
          13,
          15,
          17,
          19,
          21,
          23,
          25,
          27,
          30,
          33,
          37,
          44,
          60,
          79,
          93,
         122,
         255
    },

    /* ch 5 - range25cm64PRFnb */
    {
           1,
           1,
           1,
           2,
           2,
           3,
           4,
           6,
           7,
           9,
          10,
          12,
          13,
          15,
          16,
          17,
          19,
          21,
          23,
          26,
          30,
          42,
          55,
          65,
          85,
         255
    }
}; /* end range25cm64PRFnb */

/*-----------------------------------------------------------------------------
 * range25cm64PRFwb: Range Bias Correction table for wide band channels at
 *  64 MHz PRF, NB: !!!! each MUST END IN 255 !!!!
 *----------------------------------------------------------------------------*/

const uint8_t range25cm64PRFwb[2][NUM_64M_OFFSETWB] =
{
    /* ch 4 - range25cm64PRFwb */
    {
           7,
           8,
           8,
           9,
           9,
          10,
          11,
          12,
          13,
          13,
          14,
          15,
          16,
          16,
          17,
          18,
          19,
          19,
          20,
          21,
          22,
          24,
          25,
          27,
          28,
          29,
          30,
          32,
          33,
          34,
          35,
          37,
          39,
          41,
          43,
          45,
          48,
          50,
          53,
          56,
          60,
          64,
          68,
          74,
          81,
          89,
          98,
         109,
         122,
         136,
         146,
         154,
         162,
         178,
         220,
         249,
         255,
         255,
         255
    },

    /* ch 7 - range25cm64PRFwb */
    {
           4,
           5,
           5,
           5,
           6,
           6,
           7,
           7,
           8,
           8,
           9,
           9,
          10,
          10,
          10,
          11,
          11,
          12,
          13,
          13,
          14,
          15,
          16,
          16,
          17,
          18,
          19,
          19,
          20,
          21,
          22,
          23,
          24,
          25,
          26,
          28,
          29,
          31,
          33,
          35,
          37,
          39,
          42,
          46,
          50,
          54,
          60,
          67,
          75,
          83,
          90,
          95,
         100,
         110,
         135,
         153,
         172,
         192,
         255
    }
}; /* end range25cm64PRFwb */


/**
 * \brief This function is used to return the range bias correction
 *        need for TWR with DW1000 units.
 *
 * \param[in]  channel   Specifies the operating channel
 *                        (e.g. 1, 2, 3, 4, 5, 6 or 7)
 * \param[in]  range  The calculated distance before correction
 *                      (in DecaWave time unit)
 * \param[in]  prf    This is the PRF e.g. DW_PRF_16_MHZ or DW_PRF_64_MHZ
 *
 * \return The correction needed in DecaWave time unit.
 *          The final ranging value can be compute has follow: range - output
 */
int32_t
dw1000_getrangebias(uint8_t tsch_channel, uint16_t range)
{
  /* first get the lookup index that corresponds to given range for
   *  a particular channel at 16M PRF */
  uint8_t i = 0;
  uint8_t chanIdx = 0;
  int8_t cmoffseti = 0;  /* integer number of CM offset */
  int16_t offset = 0;  /* final offset result in metres */
  uint8_t prf = dw1000_get_tsch_channel_prf(tsch_channel);
  uint8_t channel = dw1000_get_tsch_channel_phy_channel(tsch_channel);

  /* NB: note we may get some small negitive values e.g. up to -50 cm. */

  /* convert range to integer number of 25cm values. */
  uint8_t rangeint25cm = (uint8_t) (range / 53);

  if (rangeint25cm > 255)
    /* make sure it matches largest value in table
        (all tables end in 255 !!!!) */
    rangeint25cm = 255;

  if (prf == DW_PRF_16_MHZ)
  {
    switch(channel)
    {
      case 4:
      case 7:
      {
        chanIdx = chan_idxwb[channel];
        /* find index in table corresponding to range*/
        while (rangeint25cm > range25cm16PRFwb[chanIdx][i]) i++;
        cmoffseti = i + CM_OFFSET_16M_WB; /* nearest centimeter correction */
      }
      break;
      default:
      {
        chanIdx = chan_idxnb[channel];
        /* find index in table corresponding to range */
        while (rangeint25cm > range25cm16PRFnb[chanIdx][i]) i++;
        cmoffseti = i + CM_OFFSET_16M_NB; /* nearest centimeter correction */
      }
    } /* end of switch */
  }
  else /* 64M PRF */
  {
    switch(channel)
    {
      case 4:
      case 7:
      {
        chanIdx = chan_idxwb[channel];
        /* find index in table corresponding to range */
        while (rangeint25cm > range25cm64PRFwb[chanIdx][i]) i++;
        cmoffseti = i + CM_OFFSET_64M_WB; /* nearest centimeter correction */
      }
      break;
      default:
      {
        chanIdx = chan_idxnb[channel];
        /* find index in table corresponding to range */
        while (rangeint25cm > range25cm64PRFnb[chanIdx][i]) i++;
        cmoffseti = i + CM_OFFSET_64M_NB; /* nearest centimeter correction */
      }
    } /* end of switch */
  } /* end else */

  /* offset result in centimetres */
  offset = (int32_t) ((double) CENTIMETER_TO_DWTIME * cmoffseti);

  return (offset);
}

#define RANGE_CORR_MAX_RSSI (-61)
#define RANGE_CORR_MIN_RSSI (-93)

static int8_t range_bias_by_rssi[RANGE_CORR_MAX_RSSI-RANGE_CORR_MIN_RSSI+1] = {
    -23, // -61dBm (-11 cm)
    -23, // -62dBm (-10.75 cm)
    -22, // -63dBm (-10.5 cm)
    -22, // -64dBm (-10.25 cm)
    -21, // -65dBm (-10.0 cm)
    -21, // -66dBm ( -9.65 cm)
    -20, // -67dBm (-9.3 cm)
    -19, // -68dBm (-8.75 cm)
    -17, // -69dBm (-8.2 cm)
    -16, // -70dBm (-7.55 cm)
    -15, // -71dBm (-6.9 cm)
    -13, // -72dBm (-6.0 cm)
    -11, // -73dBm (-5.1 cm)
    -8, // -74dBm (-3.9 cm)
    -6, // -75dBm (-2.7 cm)
    -3, // -76dBm (-1.35 cm)
    0, // -77dBm (0.0 cm)
    2, // -78dBm (1.05 cm)
    4, // -79dBm (2.1 cm)
    6, // -80dBm (2.8 cm)
    7, // -81dBm (3.5 cm)
    8, // -82dBm (3.85 cm)
    9, // -83dBm (4.2 cm)
    10, // -84dBm (4.55 cm)
    10, // -85dBm (4.9 cm)
    12, // -86dBm (5.55 cm)
    13, // -87dBm (6.2 cm)
    14, // -88dBm (6.65 cm)
    15, // -89dBm (7.1 cm)
    16, // -90dBm (7.35 cm)
    16, // -91dBm (7.6 cm)
    17, // -92dBm (7.85 cm)
    17, // -93dBm (8.1 cm)
};

int8_t dw_get_rssi_timestamp_bias(int8_t reception_rssi) {
    return range_bias_by_rssi[-(MAX(MIN(RANGE_CORR_MAX_RSSI, reception_rssi), RANGE_CORR_MIN_RSSI)-RANGE_CORR_MAX_RSSI)];
}
