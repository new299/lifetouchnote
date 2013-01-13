/*
 * Copyright (c) 2006-2009 NVIDIA Corporation.
 * Copyright (c) 2010 QCI
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

#include "nvodm_vibrate.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvassert.h"
#include "nvodm_vibrate.h"

//#include "nvodm_query_gpio.h"
//#include "nvodm_query_pinmux.h"

#include "nvos.h"
#include <linux/platform_device.h>
// All unnecessary code taken from msm is masked using this macro.
#define DEBUG_TRACE_ENABLE 0
#if DEBUG_TRACE_ENABLE
#define DEBUG_VIBRATOR_TRACE(Format) \
                    do { \
                            pr_info Format; \
                    } while(0)
#else
#define DEBUG_VIBRATOR_TRACE(Format)
#endif

NvOdmServicesGpioHandle ghGpio=NULL;
NvOdmGpioPinHandle ghVibGpioPin=NULL; 
static NvOdmServicesI2cHandle s_hOdmI2c = NULL;

#define TPS658620_I2C_SPEED_KHZ  100
#define TPS658620_DEVICE_ADDR    0x68
#define TPS658620_PWM_ADDR       0x5B
#define TPS658620_PWM1_ADDR      0x5A
#define TPS658620_PWM2_ADDR      0x5C
       
/* OLED ssd1351 guid number */
#define VIBRATOR_GUID NV_ODM_GUID('v','i','b','r','a','t','o','r')

//static NvU8 mode = 1;// default mode prevent motor to damage.
//static NvBool VIBE_TPS658620_I2cWrite8(NvU8 Addr, NvU8 Data);


// mode 1 : set duty cycle for your ferquency
// mode 1 : set duty cycle for your ferquency
NvBool NvOdmVibSetIndex(NvOdmVibDeviceHandle hDevice, NvS32 Freq)
{
    NvU8 temp = 0;
    temp = (NvU8)Freq;
    if(temp < 0 || temp > 63)
    {
        DEBUG_VIBRATOR_TRACE(("[VIBE] : Invaid scope!\n"));
        return NV_FALSE;
    }
    temp += 192; // offset
    hDevice->CurrentIndex = temp;
    return NV_TRUE;
}


NvBool NvOdmVibOpen(NvOdmVibDeviceHandle* hDevice)
{
    NvOdmVibDeviceHandle tVib = NULL;
   
    tVib = kzalloc(sizeof(NvOdmVibrateDevice), GFP_KERNEL);

    if(tVib == NULL)
    {
        NvOdmOsFree(tVib);
        DEBUG_VIBRATOR_TRACE(("[VIBE] : kzalloc of tegra_touch_driver_data fail!\n"));
        return NV_FALSE;
    }
    else
    {
        *hDevice = tVib;
        tVib->CurrentIndex = 248;
        return NV_TRUE;
    }
}

void NvOdmVibClose(NvOdmVibDeviceHandle hDevice)
{
    if (!hDevice)
        return;
    NvOdmOsFree(hDevice);
}

NvBool NvOdmVibStart(NvOdmVibDeviceHandle hDevice)
{
 	NvOdmPeripheralConnectivity const *conn;
        NvU64 guid;
              
        /* get the main panel */
        guid = VIBRATOR_GUID;

        /* get the connectivity info */
        conn = NvOdmPeripheralGetGuid(guid);
        if(conn == NULL)
        {
                return NV_FALSE;       
        }
        
        /* acquire GPIO pins */
        ghGpio = NvOdmGpioOpen();
        if (ghGpio == NULL)
        {
                return NV_FALSE;
        }
        
        if (ghVibGpioPin == NULL)
        {
                /* Search for the GPIO pin */
                if (conn->AddressList[0].Interface == NvOdmIoModule_Gpio)
                {
                     ghVibGpioPin = NvOdmGpioAcquirePinHandle(ghGpio, 
                           conn->AddressList[0].Instance,
                           conn->AddressList[0].Address);
                }
        }
        
        if (ghVibGpioPin == NULL)
        {
                return NV_FALSE;
        }

        NvOdmGpioSetState(ghGpio, ghVibGpioPin, 0x1);
        NvOdmGpioConfig(ghGpio, ghVibGpioPin, NvOdmGpioPinMode_Output);

        VIBE_TPS658620_I2cWrite8(TPS658620_PWM1_ADDR, hDevice->CurrentIndex);
        return NV_TRUE;
}

NvBool NvOdmVibStop(NvOdmVibDeviceHandle hDevice)
{
    NvOdmGpioSetState(ghGpio, ghVibGpioPin, 0x0);
    NvOdmGpioConfig(ghGpio, ghVibGpioPin, NvOdmGpioPinMode_Output);

    VIBE_TPS658620_I2cWrite8(TPS658620_PWM1_ADDR, 0x00);
    return NV_TRUE;
}

NvBool VIBE_TPS658620_I2cWrite8(NvU8 Addr, NvU8 Data)
{
    NvBool RetVal = NV_TRUE;
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;    
    NvOdmI2cTransactionInfo TransactionInfo;
    NvU32 DeviceAddr = (NvU32)TPS658620_DEVICE_ADDR;
    NvU32 i = 0;

    NvOdmIoModule I2cModule = NvOdmIoModule_I2c;
    NvU32 I2cInstance = 0;
    NvU32 I2cAddress  = 0;

    const NvOdmPeripheralConnectivity *pConnectivity = 
                           NvOdmPeripheralGetGuid(NV_ODM_GUID('t','p','s','6','5','8','6','x'));

    for (i = 0; i < pConnectivity->NumAddress; i ++)
    {
        if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_I2c_Pmu)
        {
            I2cModule   = NvOdmIoModule_I2c_Pmu;
            I2cInstance = pConnectivity->AddressList[i].Instance;
            I2cAddress  = pConnectivity->AddressList[i].Address;
            break;
        }
    }


    s_hOdmI2c = NvOdmI2cOpen(I2cModule, I2cInstance);
    if (!s_hOdmI2c)
    {
        RetVal = NV_FALSE;
        goto VIBE_TPS658620_I2cWrite8_exit;
    }


    DEBUG_VIBRATOR_TRACE(("[VIBE] : Open vibrate I2C success!\n"));

    WriteBuffer[0] = Addr & 0xFF;   // PMU offset
    //modify by ,set DPWM_MODE=0, fixed freq=250Hz ,duty cycle=0~127
    //WriteBuffer[1] = Data & 0xFF;   // written data
    WriteBuffer[1] = Data & 0x7F;   // written data

    TransactionInfo.Address = DeviceAddr;
    TransactionInfo.Buf = WriteBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(s_hOdmI2c, &TransactionInfo, 1, 
                        TPS658620_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

    if (status == NvOdmI2cStatus_Success)
    {
        RetVal = NV_TRUE;
        DEBUG_VIBRATOR_TRACE(("[VIBE] : Write vibrate I2C success!\n"));
    }
    else
    {
        RetVal = NV_FALSE;
        goto VIBE_TPS658620_I2cWrite8_exit;
        DEBUG_VIBRATOR_TRACE(("[VIBE] : Write vibrate I2C fail!\n"));
    }

    NvOdmI2cClose(s_hOdmI2c);
    s_hOdmI2c = NULL;
    return RetVal;

VIBE_TPS658620_I2cWrite8_exit:
    DEBUG_VIBRATOR_TRACE(("[VIBE] : Open or Write vibrate I2C fail!\n"));
    NvOdmI2cClose(s_hOdmI2c);
    s_hOdmI2c = NULL;
    return RetVal;
}

