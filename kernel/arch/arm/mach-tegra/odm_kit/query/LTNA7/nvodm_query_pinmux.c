/*
 * Copyright (c) 2009 NVIDIA Corporation.
 * Copyright (C) 2010 QCI
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *
 */

/*
 * This file implements the pin-mux configuration tables for each I/O module.
 */

// THESE SETTINGS ARE PLATFORM-SPECIFIC (not SOC-specific).
// PLATFORM = Harmony

#include "nvodm_query_pinmux.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_services.h"

#define NVODM_PINMUX_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const NvU32 s_NvOdmPinMuxConfig_Uart[] = {
    NvOdmUartPinMap_Config4,    // UART1, 2 lines
    NvOdmUartPinMap_Config2,    // UART2, 2 lines
    NvOdmUartPinMap_Config1,    // UART3, 4 lines
    0,    			// UART4
    0                           // UART5
};

static const NvU32 s_NvOdmPinMuxConfig_Spi[] = {
    NvOdmSpiPinMap_Config4, //for touch screen,
    0,
    0,
    0,
    0
};

static const NvU32 s_NvOdmPinMuxConfig_I2c[] = {
    NvOdmI2cPinMap_Config1,
    NvOdmI2cPinMap_Config1,
    NvOdmI2cPinMap_Config1
};

static const NvU32 s_NvOdmPinMuxConfig_I2cPmu[] = {
    NvOdmI2cPmuPinMap_Config1
};

// Sam modify 2010/5/20
static const NvU32 s_NvOdmPinMuxConfig_Sdio[] = {
    NvOdmSdioPinMap_Config1, // WIFI
    0,
    NvOdmSdioPinMap_Config2, //SD CARD
    NvOdmSdioPinMap_Config2, // SDIO4 (HSMMC enabled)
};

static const NvU32 s_NvOdmPinMuxConfig_Pwm[] = {
    //for backlight pwm,
    NvOdmPwmPinMap_Config1,
};
static const NvU32 s_NvOdmPinMuxConfig_Dap[] = {
    NvOdmDapPinMap_Config1, //for audio,
    0,
    0, 
    0,
    0
};

static const NvU32 s_NvOdmPinMuxConfig_ExternalClock[] = {
    NvOdmExternalClockPinMap_Config1,  
    0,
    NvOdmExternalClockPinMap_Config1
};

static const NvU32 s_NvOdmPinMuxConfig_Display[] = {
    NvOdmDisplayPinMap_Config1, //
    0  // Only 1 display is connected to the LCD pins
};

void
NvOdmQueryPinMux(
    NvOdmIoModule IoModule,
    const NvU32 **pPinMuxConfigTable,
    NvU32 *pCount)
{
    switch (IoModule)
    {
    case NvOdmIoModule_Display:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Display;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Display);
        break;

    case NvOdmIoModule_Dap:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Dap;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Dap);
        break;


    case NvOdmIoModule_I2c:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_I2c;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_I2c);
        break;

    case NvOdmIoModule_I2c_Pmu:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_I2cPmu;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_I2cPmu);
        break;

    case NvOdmIoModule_Sdio:
       *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Sdio;
       *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Sdio);
        break;

    case NvOdmIoModule_Spi:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Spi;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Spi);
        break;

    case NvOdmIoModule_Uart:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Uart;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Uart);
        break;

    case NvOdmIoModule_ExternalClock:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_ExternalClock;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_ExternalClock);
        break;

    case NvOdmIoModule_Pwm:
        *pPinMuxConfigTable = s_NvOdmPinMuxConfig_Pwm;
        *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmPinMuxConfig_Pwm);
        break;
    
    case NvOdmIoModule_Ulpi:
    case NvOdmIoModule_Kbd:
    case NvOdmIoModule_Spdif:
    case NvOdmIoModule_Tvo:
    case NvOdmIoModule_BacklightPwm:
    case NvOdmIoModule_Hdmi:
    case NvOdmIoModule_Hdcp:
    case NvOdmIoModule_Nand:
    case NvOdmIoModule_Twc:
    case NvOdmIoModule_Crt:
    case NvOdmIoModule_PciExpress:
    case NvOdmIoModule_Hsi:
    case NvOdmIoModule_Ata:
    case NvOdmIoModule_SyncNor:
    case NvOdmIoModule_Mio:
    case NvOdmIoModule_VideoInput:
    case NvOdmIoModule_OneWire:
        *pPinMuxConfigTable = NULL;
        *pCount = 0;
        break;

    default:
        *pCount = 0;
        break;
    }
}

static const NvU32 s_NvOdmClockLimit_Sdio[] = {
    24000,
    32000,
    50000,
    50000,
};

void
NvOdmQueryClockLimits(
    NvOdmIoModule IoModule,
    const NvU32 **pClockSpeedLimits,
    NvU32 *pCount)
{
    switch (IoModule)
    {
/*
        case NvOdmIoModule_Hsmmc:
            *pCount = 0;
            break;
*/

        case NvOdmIoModule_Sdio:
            *pClockSpeedLimits = s_NvOdmClockLimit_Sdio;
            *pCount = NVODM_PINMUX_ARRAY_SIZE(s_NvOdmClockLimit_Sdio);
            break;

        default:
            *pClockSpeedLimits = NULL;
            *pCount = 0;
            break;
    }
}

