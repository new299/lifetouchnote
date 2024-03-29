/*
 * Copyright (c) 2008-2009 NVIDIA Corporation.
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
 * @file          Nvodm_Sdio.c
 * @brief         <b>Sdio odm implementation</b>
 *
 * @Description : Implementation of the odm sdio API
 */
#include "nvodm_sdio.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"
#include "nvodm_pmu.h"
#include "nvos.h"

#include <linux/kernel.h>

#ifdef NV_DRIVER_DEBUG
    #define NV_DRIVER_TRACE(x) NvOdmOsDebugPrintf x
#else
    #define NV_DRIVER_TRACE(x)
#endif

#define WLAN_GUID   NV_ODM_GUID('s','d','i','o','w','l','a','n')

typedef struct NvOdmSdioRec
{
    // NvODM PMU device handle
    NvOdmServicesPmuHandle hPmu;
    // Gpio Handle
    NvOdmServicesGpioHandle hGpio;
    // Pin handle to Wlan Reset Gpio pin
    NvOdmGpioPinHandle hResetPin;
    // Pin handle to Wlan PWR GPIO Pin
    NvOdmGpioPinHandle hPwrPin;
    NvOdmPeripheralConnectivity *pConnectivity;
    // Power state
    NvBool PoweredOn;
    // Instance
    NvU32 Instance;
} NvOdmSdio;

NvOdmServicesPwmHandle s_hOdmPwm = NULL;


static void NvOdmSetPowerOnSdio(NvOdmSdioHandle pDevice, NvBool IsEnable);
static NvBool SdioOdmWlanSetPowerOn(NvOdmSdioHandle hOdmSdio, NvBool IsEnable);


static NvBool SdioOdmWlanSetPowerOn(NvOdmSdioHandle hOdmSdio, NvBool IsEnable)
{
#if 1
    if (IsEnable) 
    {
        // Wlan Power On Reset Sequence
        //NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hPwrPin, 0x0);      //PWD -> Low
        NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hResetPin, 0x0);    //RST -> Low
        NvOdmOsWaitUS(2000);
        //NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hPwrPin, 0x1);      //PWD -> High
        //NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hResetPin, 0x1);    //RST -> High      
     }
     else 
     {
         // Power Off sequence
         //NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hPwrPin, 0x0);     //PWD -> Low
        NvOdmGpioSetState(hOdmSdio->hGpio, hOdmSdio->hResetPin, 0x0);    //RST -> Low
     }
#endif
    return NV_TRUE;
}

NvOdmSdioHandle NvOdmSdioOpen(NvU32 Instance)
{
    static NvOdmSdio *pDevice = NULL;
    NvOdmServicesGpioHandle hGpioTemp = NULL;
    NvOdmPeripheralConnectivity *pConnectivity;
    NvU32 NumOfGuids = 1;
    NvU64 guid;
    NvU32 searchVals[2];
    const NvU32 *pOdmConfigs;
    NvU32 NumOdmConfigs;
    NvBool Status = NV_TRUE;
    NvU32 DutyCycle;
    NvU32 gRequestedFreqHzOrPeriod;
    NvU32 ReturnedFreq;
    const NvOdmPeripheralSearch searchAttrs[] =
    {
        NvOdmPeripheralSearch_IoModule,
        NvOdmPeripheralSearch_Instance,
    };
    
    searchVals[0] =  NvOdmIoModule_Sdio;
    searchVals[1] =  Instance;

    NvOdmQueryPinMux(NvOdmIoModule_Sdio, &pOdmConfigs, &NumOdmConfigs); 
    if (Instance >= NumOdmConfigs )
        return NULL;
    if( pOdmConfigs[Instance] == 0 )
        return NULL;

    NumOfGuids = NvOdmPeripheralEnumerate(
                                                    searchAttrs,
                                                    searchVals,
                                                    2,
                                                    &guid,
                                                    NumOfGuids);


    // Get the peripheral connectivity information
    pConnectivity = (NvOdmPeripheralConnectivity *)NvOdmPeripheralGetGuid(guid);
    if (pConnectivity == NULL)
        return NULL;

    pDevice = NvOdmOsAlloc(sizeof(NvOdmSdio));
    pDevice->hPmu = NULL;
    if(pDevice == NULL)
        return (pDevice);

    if (pDevice->hPmu == NULL)
    {
        pDevice->hPmu = NvOdmServicesPmuOpen();
        if(pDevice->hPmu == NULL)
        {
            NvOdmOsFree(pDevice);
            pDevice = NULL;
            return (NULL);
        }
    }

    pDevice->pConnectivity = pConnectivity;
    NvOdmSetPowerOnSdio(pDevice, NV_TRUE);

    if (pConnectivity->Guid == WLAN_GUID)
    {
        // Getting the OdmGpio Handle
        hGpioTemp = NvOdmGpioOpen();
        if (hGpioTemp == NULL)
        {
            NvOdmOsFree(pDevice);
            pDevice = NULL;
            return (pDevice);
        }
    
        // Search for the Vdd rail and set the proper volage to the rail.
/*
        if (pConnectivity->AddressList[1].Interface == NvOdmIoModule_Gpio)
        {
             // Acquiring Pin Handles for Power Pin
             pDevice->hPwrPin= NvOdmGpioAcquirePinHandle(hGpioTemp, 
                   pConnectivity->AddressList[1].Instance,
                   pConnectivity->AddressList[1].Address);
        }
*/// Sam ---         
        if (pConnectivity->AddressList[2].Interface == NvOdmIoModule_Gpio)
        {
             // Acquiring Pin Handles for Reset Pin
             pDevice->hResetPin= NvOdmGpioAcquirePinHandle(hGpioTemp, 
                   pConnectivity->AddressList[2].Instance,
                   pConnectivity->AddressList[2].Address);
        }

        // Setting the ON/OFF pin to output mode.
        //NvOdmGpioConfig(hGpioTemp, pDevice->hPwrPin, NvOdmGpioPinMode_Output);// Sam ---
        NvOdmGpioConfig(hGpioTemp, pDevice->hResetPin, NvOdmGpioPinMode_Output);

        // Setting the Output Pin to Low
        //NvOdmGpioSetState(hGpioTemp, pDevice->hPwrPin, 0x0); //Sam ---
        NvOdmGpioSetState(hGpioTemp, pDevice->hResetPin, 0x0);

        pDevice->hGpio = hGpioTemp;

        s_hOdmPwm = NvOdmPwmOpen();
        gRequestedFreqHzOrPeriod = 32000;	//32KHz
        DutyCycle = 3211264;

        NvOdmPwmConfig(s_hOdmPwm, NvOdmPwmOutputId_Blink, NvOdmPwmMode_Blink_32KHzClockOutput,
                        DutyCycle, &gRequestedFreqHzOrPeriod, &ReturnedFreq);

        Status = SdioOdmWlanSetPowerOn(pDevice, NV_TRUE);
        if (Status != NV_TRUE)
        {
            NvOdmOsFree(pDevice);
            pDevice = NULL;
            return (pDevice);
        }
    }
    pDevice->PoweredOn = NV_TRUE;
    pDevice->Instance = Instance;
    NV_DRIVER_TRACE(("Open SDIO%d", Instance));
    return pDevice;
}

void NvOdmSdioClose(NvOdmSdioHandle hOdmSdio)
{
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;

    NV_DRIVER_TRACE(("Close SDIO%d", hOdmSdio->Instance));

    pConnectivity = hOdmSdio->pConnectivity;
    if (pConnectivity->Guid == WLAN_GUID)
    {
        // Call Turn off power when close is Called
        (void)SdioOdmWlanSetPowerOn(hOdmSdio, NV_FALSE);

        //NvOdmGpioReleasePinHandle(hOdmSdio->hGpio, hOdmSdio->hPwrPin); //Sam ---
        NvOdmGpioReleasePinHandle(hOdmSdio->hGpio, hOdmSdio->hResetPin);    
        NvOdmGpioClose(hOdmSdio->hGpio);
    }
    NvOdmSetPowerOnSdio(hOdmSdio, NV_FALSE);
    if (hOdmSdio->hPmu != NULL)
    {
         NvOdmServicesPmuClose(hOdmSdio->hPmu);
    }
    NvOdmOsFree(hOdmSdio);
    hOdmSdio = NULL;
}

static void NvOdmSetPowerOnSdio(NvOdmSdioHandle pDevice,
                                                                NvBool IsEnable)
{
    NvU32 Index = 0;
    NvOdmServicesPmuVddRailCapabilities RailCaps;
    NvU32 SettlingTime = 0;
    const NvOdmPeripheralConnectivity *pConnectivity;

    pConnectivity = pDevice->pConnectivity;
    if (IsEnable) // Turn on Power
    {
        // Search for the Vdd rail and set the proper volage to the rail.
        for (Index = 0; Index < pConnectivity->NumAddress; ++Index)
        {
            if (pConnectivity->AddressList[Index].Interface == NvOdmIoModule_Vdd)
            {
                NvOdmServicesPmuGetCapabilities(pDevice->hPmu, pConnectivity->AddressList[Index].Address, &RailCaps);
                NvOdmServicesPmuSetVoltage(pDevice->hPmu, pConnectivity->AddressList[Index].Address,
                                RailCaps.requestMilliVolts, &SettlingTime);
                if (SettlingTime)
                {
                    NvOdmOsWaitUS(SettlingTime);
                }
            }
        }
    }
    else // Shutdown Power
    {
        // Search for the Vdd rail and power Off the module
        for (Index = 0; Index < pConnectivity->NumAddress; ++Index)
        {
            if (pConnectivity->AddressList[Index].Interface == NvOdmIoModule_Vdd)
            {
                NvOdmServicesPmuGetCapabilities(pDevice->hPmu, pConnectivity->AddressList[Index].Address, &RailCaps);
                NvOdmServicesPmuSetVoltage(pDevice->hPmu, pConnectivity->AddressList[Index].Address,
                                ODM_VOLTAGE_OFF, &SettlingTime);
                if (SettlingTime)
                {
                    NvOdmOsWaitUS(SettlingTime);
                }
            }
        }
    }
}

NvBool NvOdmSdioSuspend(NvOdmSdioHandle hOdmSdio)
{

    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvBool Status = NV_TRUE;

    if (!hOdmSdio->PoweredOn)
    {
        NV_DRIVER_TRACE(("SDIO%d already suspended", hOdmSdio->Instance));
        return NV_TRUE;
    }

    NV_DRIVER_TRACE(("Suspend SDIO%d", hOdmSdio->Instance));
    NvOdmSetPowerOnSdio(hOdmSdio, NV_FALSE);

    pConnectivity = hOdmSdio->pConnectivity;

    if (pConnectivity->Guid == WLAN_GUID)
    {
        // Turn off power
        Status = SdioOdmWlanSetPowerOn(hOdmSdio, NV_FALSE);
    }
    hOdmSdio->PoweredOn = NV_FALSE;
    return Status;

}

NvBool NvOdmSdioResume(NvOdmSdioHandle hOdmSdio)
{
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvBool Status = NV_TRUE;

    if (hOdmSdio->PoweredOn)
    {
        NV_DRIVER_TRACE(("SDIO%d already resumed", hOdmSdio->Instance));
        return NV_TRUE;
    }

    NvOdmSetPowerOnSdio(hOdmSdio, NV_TRUE);

    pConnectivity = hOdmSdio->pConnectivity;

    if (pConnectivity->Guid == WLAN_GUID)
    {
        // Turn on power
        Status = SdioOdmWlanSetPowerOn(hOdmSdio, NV_TRUE);
    }
    NV_DRIVER_TRACE(("Resume SDIO%d", hOdmSdio->Instance));
    hOdmSdio->PoweredOn = NV_TRUE;
    return Status;
}
