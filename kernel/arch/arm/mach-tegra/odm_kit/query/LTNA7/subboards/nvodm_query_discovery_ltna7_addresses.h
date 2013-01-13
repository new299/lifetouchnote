/*
 * Copyright (c) 2007-2009 NVIDIA Corporation.
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

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database NvOdmIoAddress entries
 *                 for the peripherals on E1162 module.
 */

#include "pmu/tps6586x/nvodm_pmu_tps6586x_supply_info_table.h"
#include "tmon/adt7461/nvodm_tmon_adt7461_channel.h"
#include "nvodm_tmon.h"
#include "../nvodm_query_kbc_gpio_def.h"


// RTC voltage rail
static const NvOdmIoAddress s_RtcAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO2 }  /* VDD_RTC -> LD02 */
};

// Core voltage rail
static const NvOdmIoAddress s_CoreAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_DCD0 }  /* VDD_CORE -> SM0 */
};

// CPU voltage rail
static const NvOdmIoAddress s_ffaCpuAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_DCD1 }  /* VDD_CPU -> SM1 */
};

// PLLA voltage rail
static const NvOdmIoAddress s_PllAAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDDPLLX_1V2 -> LDO1 */
};

// PLLM voltage rail
static const NvOdmIoAddress s_PllMAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDDPLLX_1V2 -> LDO1 */
};

// PLLP voltage rail
static const NvOdmIoAddress s_PllPAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDDPLLX_1V2 -> LDO1 */
};

// PLLC voltage rail
static const NvOdmIoAddress s_PllCAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDDPLLX_1V2 -> LDO1 */
};

// PLLU voltage rail
static const NvOdmIoAddress s_PllUAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDD_PLLU -> LDO1 */
};

// PLLU1 voltage rail
static const NvOdmIoAddress s_ffaPllU1Addresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDD_PLLU -> LDO1 */
};

// PLLS voltage rail
static const NvOdmIoAddress s_PllSAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* PLL_S -> LDO1 */
};

// OSC voltage rail
static const NvOdmIoAddress s_VddOscAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* AVDD_OSC -> LDO4 */
};

// PLLX voltage rail
static const NvOdmIoAddress s_PllXAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO1 } /* AVDDPLLX -> LDO1 */
};

// PLL_USB voltage rail
static const NvOdmIoAddress s_PllUsbAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO3 } /* AVDD_USB_PLL -> derived from LDO3 (VDD_3V3) */
};

// SYS IO voltage rail
static const NvOdmIoAddress s_VddSysAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_SYS -> LDO4 */
};

// USB voltage rail
static const NvOdmIoAddress s_VddUsbAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO3 } /* AVDD_USB -> derived from LDO3 (VDD_3V3) */
};

// MIPI voltage rail (DSI_CSI)
static const NvOdmIoAddress s_VddMipiAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, Ext_SWITCHPmuSupply_AVDD_DSI_CSI } /* AVDD_DSI_CSI -> VDD_1V2 */
};

// LCD voltage rail
static const NvOdmIoAddress s_VddLcdAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_LCD -> (LDO4PG) */
};

// Audio voltage rail
static const NvOdmIoAddress s_VddAudAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_AUDIO -> (LDO4PG) */
};

// DDR voltage rail
static const NvOdmIoAddress s_VddDdrAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 }  /* VDDIO_DDR -> (LDO4PG) */
};

// DDR_RX voltage rail
static const NvOdmIoAddress s_VddDdrRxAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO9 }  /* VDDIO_RX_DDR(2.7-3.3) -> LDO9 */
};

// NAND voltage rail
static const NvOdmIoAddress s_VddNandAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 }  /* VDDIO_NAND_1V8 -> derived from LDO4 (1V8_SUS) */
};

// UART voltage rail
static const NvOdmIoAddress s_VddUartAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_UART -> (LDO4PG) */
};

// SDIO voltage rail
static const NvOdmIoAddress s_VddSdioAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_SDIO -> derived from LDO4 (3V3_MAIN) */
};

// VI voltage rail
static const NvOdmIoAddress s_VddViAddresses[] = 
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_VI -> derived from LDO4 (3V3_MAIN) */
};

// BB voltage rail
static const NvOdmIoAddress s_VddBbAddresses[] = 
{
    // This is in the AON domain
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_BB -> (LDO4PG) */
};

// Super power voltage rail for the SOC
static const NvOdmIoAddress s_VddSocAddresses[]=
{
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_SoC } /* VDD SOC */
};

// PMU0
static const NvOdmIoAddress s_Pmu0Addresses[] = 
{
    { NvOdmIoModule_I2c_Pmu, 0x00, 0x68 },
};

// SPI1 for Spi Touch panel 
static const NvOdmIoAddress s_SpiTouchPanelAddresses[] =
{
    { NvOdmIoModule_Spi, 0, 0 },                // SPI1 ,cs0
    { NvOdmIoModule_Gpio, (NvU32)'k'-'a', 5 },  // interrupt, Port k, Pin 5
};

// SPI1 for Spi Ethernet Kitl only
static const NvOdmIoAddress s_SpiEthernetAddresses[] =
{
    //mark it, it is for touch now,
    /*
    { NvOdmIoModule_Spi, 0, 0 },
    { NvOdmIoModule_Gpio, (NvU32)'c'-'a', 1 },  // DBQ_IRQ, Port C, Pin 1
    */
    { 0, 0, 0 },
};

// P1160 ULPI USB
static const NvOdmIoAddress s_UlpiUsbAddresses[] = 
{
    { NvOdmIoModule_ExternalClock, 1, 0 }, /* ULPI PHY Clock -> DAP_MCLK2 */
};

//  LVDS LCD Display
static const NvOdmIoAddress s_LvdsDisplayAddresses[] = 
{
    { NvOdmIoModule_Display, 0, 0 },
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4},     /* VDDIO_LCD (AON:VDD_1V8) */
//    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4},    /* VDD_LVDS (3V3_MAIN) */
};

// Sdio
static const NvOdmIoAddress s_SdioAddresses[] =
{
    { NvOdmIoModule_Sdio, 0x2,  0x0 },                      /* SD Memory on SD Bus */
    { NvOdmIoModule_Sdio, 0x3,  0x0 },                      /* SD Memory on SD Bus */
    { NvOdmIoModule_Vdd, 0x00, Ext_SWITCHPmuSupply_VDDIO_SD },   /* EN_VDDIO_SD */
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDIO_SDIO -> derived from LDO4 (3V3_MAIN) */
};

static const NvOdmIoAddress s_I2cSmbusAddresses[] = 
{
    { NvOdmIoModule_I2c, 2, 0x8A },
    { NvOdmIoModule_Gpio, 27, 1} //Port BB:01 is used on harmony. 
};

static const NvOdmIoAddress s_UsbMuxAddress[] = 
{
    {NvOdmIoModule_Usb, 1, 0}
};

static const NvOdmIoAddress s_QwertyKeyPad16x8Addresses[] =
{
    // instance = 1 indicates Column info.
    // instance = 0 indicates Row info.
    // address holds KBC pin number used for row/column.

    // All Row info has to be defined contiguously from 0 to max.
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow0}, // Row 0
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow1}, // Row 1
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow2}, // Row 2
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow3}, // Row 3
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow4}, // Row 4
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow5}, // Row 5
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow6}, // Row 6
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow7}, // Row 7
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow8}, // Row 8
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow9}, // Row 9
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow10}, // Row 10
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow11}, // Row 11
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow12}, // Row 12
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow13}, // Row 13
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow14}, // Row 14
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow15}, // Row 15
    
    // All Column info has to be defined contiguously from 0 to max.
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol0}, // Column 0
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol1}, // Column 1
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol2}, // Column 2
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol3}, // Column 3
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol4}, // Column 4
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol5}, // Column 5
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol6}, // Column 6
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol7}, // Column 7
};


static const NvOdmIoAddress s_Tmon0Addresses[] = 
{
    { NvOdmIoModule_I2c, 0x00, 0x98 },                      /* I2C bus */
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 },    /* TMON pwer rail -> LDO4 (3V3_MAIN) */
    { NvOdmIoModule_Gpio, (NvU32)'n'-'a', 6 },              /* GPIO Port N and Pin 6 */

    /* Temperature zone mapping */
    { NvOdmIoModule_Tsense, NvOdmTmonZoneID_Core, ADT7461ChannelID_Remote },   /* TSENSOR */
    { NvOdmIoModule_Tsense, NvOdmTmonZoneID_Ambient, ADT7461ChannelID_Local }, /* TSENSOR */
};

// Bluetooth
static const NvOdmIoAddress s_p1162BluetoothAddresses[] =
{
    { NvOdmIoModule_Uart, 0x2,  0x0 },                  // FIXME: Is this used?
    { NvOdmIoModule_Gpio, (NvU32)'g'-'a', 6, 0},          /* BT_RST#: GPIO Port G and Pin 6 */
    { NvOdmIoModule_Gpio, (NvU32)'g'-'a', 5, 1},          /* BT_SHUTDOWN_N: GPIO Port G and Pin 5 */
    { NvOdmIoModule_Gpio, (NvU32)'p'-'a', 6, 2},          /* BT_RST# *DEPRECATED*: GPIO Port P and Pin 6 */
    { NvOdmIoModule_Gpio, (NvU32)'h'-'a', 6, 3},          /* BT_WAKEUP: GPIO Port H and Pin 6 */
    { NvOdmIoModule_Gpio, (NvU32)'u'-'a', 1, 4},          /* BT_WAKEUP *DEPRECATED*: GPIO Port U and Pin 1 */
    { NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDHOSTIF_BT -> LDO4 (AON:VDD_1V8) */
};

// Wlan
static const NvOdmIoAddress s_WlanAddresses[] =
{
    { NvOdmIoModule_Sdio, 0x0, 0x0 },                      /* WLAN is on SD Bus */
    { NvOdmIoModule_Gpio, 'u'-'a', 0x2 },                      /* GPIO Port K and Pin 5 - WIFI_PWR*/
    { NvOdmIoModule_Gpio, 'p'-'a', 0x5 },                      /* GPIO Port K and Pin 6 - WIFI_RST */
    { NvOdmIoModule_Vdd,  0x00, TPS6586xPmuSupply_LDO4 },  /* VDDIO_WLAN (AON:VDD_1V8) */
};

// Audio Codec , 
static const NvOdmIoAddress s_AudioCodecAddresses[] = 
{
    { NvOdmIoModule_ExternalClock, 0, 0 },       /* Codec MCLK -> APxx DAP_MCLK1 */
    { NvOdmIoModule_I2c, 0x01, 0x18 },           /* Codec I2C ->  APxx I2C, segment 0 */
    { NvOdmIoModule_Gpio, (NvU32)'a'-'a', 3 }, // codec's reset pin , GPIO_PA3

};

// Audio Codec on GEN1_I2C (I2C_1)
static const NvOdmIoAddress s_AudioCodecAddressesI2C_1[] = 
{
    { NvOdmIoModule_ExternalClock, 0, 0 },       /* Codec MCLK -> APxx DAP_MCLK1 */
    { NvOdmIoModule_I2c, 0x00, 0x34 },           /* Codec I2C ->  APxx PMU I2C, segment 0 */
                                                 /* Codec I2C address is 0x34 */
};

// TouchPanel
static const NvOdmIoAddress s_TouchPanelAddresses[] = 
{
    { NvOdmIoModule_I2c_Pmu, 0x00, 0x06 }, /* I2C address (7-bit) 0x03<<1=0x06(8-bit)  */
    { NvOdmIoModule_Gpio, (NvU32)'d'-'a', 0x02 }, /* GPIO Port D and Pin 2 */
};

static const NvOdmIoAddress s_AcceleroAddresses[] =
{
    { NvOdmIoModule_I2c_Pmu, 0x00, 0x70 }, /* I2C address (7-bit) 0x38<<1 = 0x70(8-bit) */
    { NvOdmIoModule_Gpio, (NvU32)'c'-'a', 0x03 }, /* Gpio port C and Pin 3 */
};

//  Vibrator module ,
static const NvOdmIoAddress s_VibratorAddresses[] =
{
    { NvOdmIoModule_Gpio, (NvU32)'v'-'a', 0x01 }, /* enable pin, GPIO Port V and Pin 1 */       
};

// IR sensor , QCI Sam
static const NvOdmIoAddress s_IRAddresses[] =
{
    { NvOdmIoModule_Gpio, (NvU32)'l'-'a', 0x01 }, // GPIO_IR_DOME_SENSE
    { NvOdmIoModule_Gpio, (NvU32)'t'-'a', 0x04 }, // GPIO_IR_NRST
    { NvOdmIoModule_Gpio, (NvU32)'l'-'a', 0x00 }, // GPIO_IR_SHTDWN
    { NvOdmIoModule_Gpio, (NvU32)'d'-'a', 0x05 }, // GPIO_IR_MOTION
};

static const NvOdmIoAddress s_GPSAddresses[] =
{
    { NvOdmIoModule_Uart, 0x1,  0x0 },                  // FIXME: Is this used?
    { NvOdmIoModule_Gpio, (NvU32)'t'-'a', 2, 0},          /* GPS_PWR_CTRL */
    //{ NvOdmIoModule_Vdd, 0x00, TPS6586xPmuSupply_LDO4 } /* VDDHOSTIF_BT -> LDO4 (AON:VDD_1V8) */
};

