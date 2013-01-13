/*
 * Copyright (c) 2009 NVIDIA Corporation.
 * Copyright (C) 2011 NEC Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/time.h>
#include <linux/rtc.h>
#include "nvodm_pmu_tps6586x_rtc.h"
#include "nvodm_pmu_tps6586x_i2c.h"
#include "tps6586x_reg.h"
#include "pmu_hal.h"

/* NOTES:
 * The PMU has two alarm counters, but both counters' compare limit are
 * same : when prescaler is enabled,
 *  ALARM1 is compared to RTC[23:0]
 *  ALARM2 is compared to RTC[22:7]
 * So it is not reasonable to use ALARM2... and RTC[23:0] duration is
 * about 4 hours, we must use multiple matching if alarm duration is longer
 * than 4 hours...!
 *
 * This code assumes that a pre-scaler is enabled.
 */

#define DBG	0

// macro OFFSET_BASE_YEAR if 1, uses epoch as reference year instead of 1970
// This is because RTC in PMU TPS6586x can store duration of 34 years,
// else we cannot retain date beyond 2004
#define OFFSET_BASE_YEAR 1

#if OFFSET_BASE_YEAR
#define EPOCH_START	2009
#define EPOCH_END	2037

static unsigned long get_epoch_start(void)
{
    static unsigned long epoch_start = 0;
    if (!epoch_start)
        epoch_start = mktime(EPOCH_START,1,1,0,0,0);
    return epoch_start;
}

static unsigned long get_epoch_end(void)
{
    static unsigned long epoch_end = 0;
    if (!epoch_end)
        epoch_end = mktime(EPOCH_END,12,31,23,59,59);
    return epoch_end;
}
#endif

/* FIXME: these parameters should be keeped in device structure. */
static NvBool rtc_alarm_active = NV_FALSE;      /* RTC_ALARM_ACTIVE */
static NvBool bRtcNotInitialized = NV_TRUE;
static NvU32  g_alarm_count;
static NvU32  g_alarm_overflow;		/* only for debug */

#define ALARM_NEAR_FULL	(1)		/* 250 msec */

/* Read RTC count register */
NvBool
Tps6586xRtcCountRead(
    NvOdmPmuDeviceHandle hDevice,
    NvU32* Count)
{
    NvU32 ReadBuffer[2];

    // 1) The I2C address pointer must not be left pointing in the range 0xC6 to 0xCA
    // 2) The maximum time for the address pointer to be in this range is 1ms
    // 3) Always read RTC_ALARM2 in the following order to prevent the address pointer
    // from stopping at 0xC6: RTC_ALARM2_LO, then RTC_ALARM2_HI

    if (Tps6586xRtcWasStartUpFromNoPower(hDevice) && bRtcNotInitialized)
    {
        Tps6586xRtcCountWrite(hDevice, 0);
        *Count = 0;
    }
    else
    {
        // The unit of the RTC count is second!!! 1024 tick = 1s.
        // Read all 40 bit and right move 10 = Read the hightest 32bit and right move 2
#if 0
        Tps6586xI2cRead32(hDevice, TPS6586x_RC6_RTC_COUNT4, &ReadBuffer[0]);

        Tps6586xI2cRead8(hDevice, TPS6586x_RCA_RTC_COUNT0, &ReadBuffer[1]);

        Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer[1]);

        // return second
        *Count = ReadBuffer[0]>>2;
#else
        NvU8 buf[5], buf2[5];
        const int retry = 10;
        int n;
        for (n = 0; n < retry; ++n) {
            Tps6586xI2cRead40(hDevice, TPS6586x_RC6_RTC_COUNT4, buf);
            Tps6586xI2cRead40(hDevice, TPS6586x_RC6_RTC_COUNT4, buf2);
            if (buf[0] == buf2[0] && buf[1] == buf2[1] &&
                buf[2] == buf2[2] && buf[3] == buf2[3]) {
                break;
            }
            NvOdmOsPrintf("%s: RTC read retry[%d] %08x %08x\n", __FUNCTION__, n,
                          buf [0] << 24 | buf [1] << 16 | buf [2] << 8 | buf [3],
                          buf2[0] << 24 | buf2[1] << 16 | buf2[2] << 8 | buf2[3]);
        }
        *Count = (buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0) >> 2;
#endif
    }

#if OFFSET_BASE_YEAR
    *Count += (NvU32)get_epoch_start();
#endif

    return NV_TRUE;
}

/* Write RTC count register */

NvBool
Tps6586xRtcCountWrite(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 Count)
{
    NvU32 ReadBuffer = 0;
#if OFFSET_BASE_YEAR
    NvU32 epoch_start = get_epoch_start();
    if (Count < epoch_start)
    {
        // prevent setting date earlier than 'epoch'
        pr_warning("\n Date being set cannot be earlier than least "
            "year=%d. Setting as least year. ", (int)EPOCH_START);
        // base year seconds count is 0
        Count = 0;
    }
    else
        Count -= epoch_start;

    if (Count > get_epoch_end())
        Count = 0; //reset RTC count to 2010/01/01 00:00:00
#endif  /*OFFSET_BASE_YEAR */

    // Switch to 32KHz crystal oscillator
    // POR_SRC_SEL=1 and OSC_SRC_SEL=1
    Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer);
    ReadBuffer = ReadBuffer | 0xC0;
    Tps6586xI2cWrite8(hDevice, TPS6586x_RC0_RTC_CTRL, ReadBuffer);

    // To enable incrementing of the RTC_COUNT[39:0] from an initial value set by the host,
    // the RTC_ENABLE bit should be written to 1 only after the RTC_OUT voltage reaches
    // the operating range

    // Clear RTC_ENABLE before writing RTC_COUNT
    Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer);
    ReadBuffer = ReadBuffer & 0xDF;
    Tps6586xI2cWrite8(hDevice, TPS6586x_RC0_RTC_CTRL, ReadBuffer);

#if 0
    Tps6586xI2cWrite32(hDevice, TPS6586x_RC6_RTC_COUNT4, (Count<<2));
    Tps6586xI2cWrite8(hDevice,  TPS6586x_RCA_RTC_COUNT0, 0);
#else
    do {
        NvU8 buf[5], buf2[5];
        const int retry = 10;
        int n;
        Count <<= 2;
        buf[0] = (Count >> 24) & 0xff;
        buf[1] = (Count >> 16) & 0xff;
        buf[2] = (Count >>  8) & 0xff;
        buf[3] = (Count >>  0) & 0xff;
        buf[4] = 0;

        for (n = 0; n < retry; ++n) {
#if 1	/* PMU does not support i2c multibyte write */
            Tps6586xI2cWrite8(hDevice,  TPS6586x_RC6_RTC_COUNT4, buf[0]);
            Tps6586xI2cWrite8(hDevice,  TPS6586x_RC7_RTC_COUNT3, buf[1]);
            Tps6586xI2cWrite8(hDevice,  TPS6586x_RC8_RTC_COUNT2, buf[2]);
            Tps6586xI2cWrite8(hDevice,  TPS6586x_RC9_RTC_COUNT1, buf[3]);
            Tps6586xI2cWrite8(hDevice,  TPS6586x_RCA_RTC_COUNT0, buf[4]);
#else
            Tps6586xI2cWrite40(hDevice, TPS6586x_RC6_RTC_COUNT4, buf);
#endif
            Tps6586xI2cRead40(hDevice, TPS6586x_RC6_RTC_COUNT4, buf2);
            if (buf[0] == buf2[0] && buf[1] == buf2[1] && buf[2] == buf2[2] &&
                buf[3] == buf2[3] && buf[4] == buf2[4]) {
                break;
            }
            NvOdmOsPrintf("%s: verify RTC retry [%d] %08x %08x\n",
                          __FUNCTION__, n, Count,
                          buf2[0] << 24 | buf2[1] << 16 | buf2[2] << 8 | buf2[3]);

            NvOdmOsSleepMS(1);
        }
    } while (0);
#endif

    // Set RTC_ENABLE after writing RTC_COUNT
    Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &ReadBuffer);
    ReadBuffer = ReadBuffer | 0x20;
    Tps6586xI2cWrite8(hDevice, TPS6586x_RC0_RTC_CTRL, ReadBuffer);

    if (bRtcNotInitialized)
        bRtcNotInitialized = NV_FALSE;

    return NV_TRUE;
}

/* Read RTC alarm count register */

NvBool
Tps6586xRtcAlarmCountRead(
    NvOdmPmuDeviceHandle hDevice,
    NvU32* Count)
{
    if (rtc_alarm_active) {
        *Count = g_alarm_count;
        return NV_TRUE;
    } else {
        return NV_FALSE;
    }
}

void
Tps6586xRtcAlarmInterruptEnable(NvOdmPmuDeviceHandle dev, NvBool enabled)
{
    NvU32 temp;

    Tps6586xI2cRead8(dev, TPS6586x_RB4_INT_MASK5, &temp);
    if (enabled) {
        temp = temp & 0xEF;
    } else {
        temp = temp | 0x10;
    }
    Tps6586xI2cWrite8(dev, TPS6586x_RB4_INT_MASK5, temp);
}

static int
Tps6586xRtcSetupAlarmCounter(NvOdmPmuDeviceHandle dev)
{
    NvU32 rtcCount, diff, temp;	    /* unit of rtcCount is 250msec */
    NvU32 alarmCount = g_alarm_count;
    const NvU32 ALARM1_MAX = (1UL << 24);

#if OFFSET_BASE_YEAR
    NvU32 epoch_start = get_epoch_start();

    if (alarmCount < epoch_start) {
        NvOdmOsPrintf("%s: can not set alarm time before epoch: %u %u\n",
                      __FUNCTION__, alarmCount, epoch_start);
        return -1;
    }
    if (get_epoch_end() <= alarmCount) {
        NvOdmOsPrintf("%s: can not set alarm time after epoch end: %u %u\n",
                      __FUNCTION__, alarmCount, get_epoch_end());
        return -1;
    }
    alarmCount -= epoch_start;
#endif

    /* read RTC */
    Tps6586xI2cRead32(dev, TPS6586x_RC6_RTC_COUNT4, &rtcCount);
    Tps6586xI2cRead8(dev, TPS6586x_RCA_RTC_COUNT0, &temp);

    alarmCount <<= 2;	/* the unit of alarmCount becomes 250msec */
    diff = alarmCount - rtcCount;
    if (alarmCount <= rtcCount || diff < ALARM_NEAR_FULL) {
#if DBG
        NvOdmOsPrintf("%s: fail %u %u %u\n",
                      __FUNCTION__, alarmCount, rtcCount, diff);
#endif
        return -1;
    }

    diff <<= 8;		/* the unit of diff becomes same as one of PMU RTC */
    if (diff >= ALARM1_MAX) {
        /* multiple match */
#if DBG
        NvOdmOsPrintf("%s: multiple match %d\n",
                      __FUNCTION__, g_alarm_overflow);
#endif
        alarmCount = rtcCount - 1;
        ++g_alarm_overflow;
    }
    alarmCount = (alarmCount << 8) & (ALARM1_MAX - 1);

#if DBG
    NvOdmOsPrintf("RTC: %08x%02x\n", rtcCount, temp);
    NvOdmOsPrintf("ALM: 00%08x\n", alarmCount);
#endif

    /* FIXME: consider unexpected match while writing ALARM1 value */
    Tps6586xI2cWrite8(dev,
                      TPS6586x_RC3_RTC_ALARM1_LO,  ((alarmCount >>  0) & 0xFF));
    Tps6586xI2cWrite8(dev,
                      TPS6586x_RC2_RTC_ALARM1_MID, ((alarmCount >>  8) & 0xFF));
    Tps6586xI2cWrite8(dev,
                      TPS6586x_RC1_RTC_ALARM1_HI,  ((alarmCount >> 16) & 0xFF));

    return 0;
}

void
Tps6586xRtcAlarmInterrupt(NvOdmPmuDeviceHandle dev)
{
    struct rtc_time rtime;
    NvU32 rtc;

#if DBG
    NvOdmOsPrintf("%s enter\n", __FUNCTION__);
#endif
    Tps6586xRtcAlarmInterruptEnable(dev, NV_FALSE);
    if (Tps6586xRtcSetupAlarmCounter(dev) == 0) {
        /* continue alarm */
        Tps6586xRtcAlarmInterruptEnable(dev, NV_TRUE);
    } else {
#if DBG
        NvOdmOsPrintf("%s: spawn AlarmInterrupt\n", __FUNCTION__);
        rtc_time_to_tm(g_alarm_count, &rtime);
        NvOdmOsPrintf("alarm: %02d:%02d:%02d %02d/%02d/%04d\n",
                      rtime.tm_hour, rtime.tm_min, rtime.tm_sec,
                      rtime.tm_mon + 1, rtime.tm_mday, rtime.tm_year + 1900);

        Tps6586xRtcCountRead(dev, &rtc);
        rtc_time_to_tm(rtc, &rtime);
        NvOdmOsPrintf("rtc  : %02d:%02d:%02d %02d/%02d/%04d\n",
                      rtime.tm_hour, rtime.tm_min, rtime.tm_sec,
                      rtime.tm_mon + 1, rtime.tm_mday, rtime.tm_year + 1900);
#endif
        if (dev->pfnAlarmInterrupt)
            dev->pfnAlarmInterrupt(dev);
    }
}

/* Write RTC alarm count register */

NvBool
Tps6586xRtcAlarmCountWrite(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 Count)
{
#if DBG
    struct rtc_time rtime;
    rtc_time_to_tm(Count, &rtime);
    NvOdmOsPrintf("%s: alarm at %02d:%02d:%02d %02d/%02d/%04d\n",
                  __FUNCTION__,
                  rtime.tm_hour, rtime.tm_min, rtime.tm_sec,
                  rtime.tm_mon + 1, rtime.tm_mday, rtime.tm_year + 1900);
#endif

    if (rtc_alarm_active) {
#if DBG
        rtc_time_to_tm(g_alarm_count, &rtime);
        NvOdmOsPrintf("%s: overwrite: old alarm %02d:%02d:%02d %02d/%02d/%04d\n",
                      __FUNCTION__,
                      rtime.tm_hour, rtime.tm_min, rtime.tm_sec,
                      rtime.tm_mon + 1, rtime.tm_mday, rtime.tm_year + 1900);
#endif
        Tps6586xRtcAlarmInterruptEnable(hDevice, NV_FALSE);
    }

    g_alarm_count = Count;
    g_alarm_overflow = 0;

    if (Tps6586xRtcSetupAlarmCounter(hDevice) == 0) {
        Tps6586xRtcAlarmInterruptEnable(hDevice, NV_TRUE);
        rtc_alarm_active = NV_TRUE;
    } else {
        rtc_alarm_active = NV_FALSE;
    }

    return rtc_alarm_active;
}

/* Reads RTC alarm interrupt mask status */

NvBool
Tps6586xRtcIsAlarmIntEnabled(NvOdmPmuDeviceHandle hDevice)
{
    return NV_FALSE;
}

/* Enables / Disables the RTC alarm interrupt */

NvBool
Tps6586xRtcAlarmIntEnable(
    NvOdmPmuDeviceHandle hDevice,
    NvBool Enable)
{
    return NV_FALSE;
}

/* Checks if boot was from nopower / powered state */

NvBool
Tps6586xRtcWasStartUpFromNoPower(NvOdmPmuDeviceHandle hDevice)
{
    NvU32 Data = 0;

    if ((Tps6586xI2cRead8(hDevice, TPS6586x_RC0_RTC_CTRL, &Data)) == NV_TRUE)
    {
        return ((Data & 0x20)? NV_FALSE : NV_TRUE);
    }
    return NV_FALSE;
}
