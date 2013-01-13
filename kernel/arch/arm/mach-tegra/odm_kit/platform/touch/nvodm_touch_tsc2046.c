/*
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
#include <linux/spi/spi.h>

#include "nvodm_touch_int.h"
#include "nvodm_services.h"
#include "nvodm_touch_tsc2046.h"
#include "nvodm_query_discovery.h"
#include "tpk_reg.h"


//for compass reset pin, 
static NvOdmServicesGpioHandle s_hGpio = NULL;
static NvOdmGpioPinHandle s_hResetGpioPin = NULL;
//end 

//median filter,
#define X                   0
#define Y                   1
#define COORDS_NUM          2   //x,y
#define SAMPLE_SET_NUM      50
#define FILTER_ABOVE        10 
#define FILTER_BELOW        5 
#define FILTER_THRESHOLD    12  // error*COORDS_NUM = 6*2
#define IGNORE_MAX_DATA     9999
#define SAMPLE_CAL_SET_NUM  30
int gFifo[COORDS_NUM][SAMPLE_SET_NUM];
int gSort[COORDS_NUM][SAMPLE_SET_NUM];
int gLast_issued[COORDS_NUM];
int gPos = 0;
int gGetDoneNums = 0;
//end 


#define USE_TOUCH_SPI       1

#define TPK_SPI_SPEED_KHZ                          100  //100 kHz
#define TPK_LOW_SAMPLE_RATE                        0    //40 reports per-second
#define TPK_HIGH_SAMPLE_RATE                       1    //80 reports per-second
#define TPK_MAX_READ_BYTES                         16
#define TPK_MAX_PACKET_SIZE                        8
#define TPK_CHECK_ERRORS                           0
#define TPK_BENCHMARK_SAMPLE                       0
#define TPK_REPORT_WAR_SUCCESS                     0
#define TPK_REPORT_2ND_FINGER_DATA                 0
#define TPK_DUMP_REGISTER                          0
#define TPK_SCREEN_ANGLE                           0   //0=Landscape, 1=Portrait
#define TPK_QUERY_SENSOR_RESOLUTION                0    //units per millimeter
#define TPK_SET_MAX_POSITION                       0
#define TPK_PAGE_CHANGE_DELAY                      0 //This should be unnecessary
#define TPK_POR_DELAY                              100 //Dealy after Power-On Reset


#define TPK_ADL340_WAR                             0
/* WAR for spurious zero reports from panel: verify zero
   fingers sample data by waiting one refresh interval and
   retrying reading data */
#define TPK_SPURIOUS_ZERO_WAR                      1

#define TSC_TOUCH_DEVICE_GUID NV_ODM_GUID('t','s','c','_','2','0','4','6')

#define TPK_GetData(dev, Command) TPK_SPIGetData(dev, Command)

#define TPK_DEBOUNCE_TIME_MS 0

//for Pressure measurement
#define RESISTANCE_X_PLATE	866
#define RESISTANCE_Y_PLATE	313
#define CALCULATE_UNIT		100
#define PRESSURE_MAX_BOUND	100000

typedef struct TPK_TouchDeviceRec
{
    NvOdmTouchDevice OdmTouch;
    NvOdmTouchCapabilities Caps;

    NvOdmServicesSpiHandle hOdmSpi;

    NvOdmServicesGpioHandle hGpio;
    NvOdmServicesPmuHandle hPmu;
    NvOdmGpioPinHandle hPin;
    NvOdmServicesGpioIntrHandle hGpioIntr;
    NvOdmOsSemaphoreHandle hIntSema;
    NvBool PrevFingers;
    NvU32 DeviceAddr;
    NvU32 SampleRate;
    NvU32 SleepMode;
    NvBool PowerOn;
    NvU32 VddId;    
    NvU32 ChipRevisionId; //Id=0x01:TPK chip on Concorde1
                          //id=0x02:TPK chip with updated firmware on Concorde2

    NvU32 SPIClockSpeedKHz;

} TPK_TouchDevice;

static const NvOdmTouchCapabilities TPK_Capabilities =
{
    0, //IsMultiTouchSupported
    1, //MaxNumberOfFingerCoordReported;
    0, //IsRelativeDataSupported
    1, //MaxNumberOfRelativeCoordReported
    1, //MaxNumberOfWidthReported
    1, //MaxNumberOfPressureReported
    (NvU32)NvOdmTouchGesture_Not_Supported, //Gesture
    1, //IsWidthSupported
    1, //IsPressureSupported
    1, //IsFingersSupported
    0, //XMinPosition
    0, //YMinPosition
    0, //XMaxPosition
    0, //YMaxPosition
#if TPK_SCREEN_ANGLE
    (NvU32)NvOdmTouchOrientation_H_FLIP // Orientation 4 inch tpk panel
#else
    (NvU32)(NvOdmTouchOrientation_H_FLIP | NvOdmTouchOrientation_V_FLIP)
//    (NvU32)(NvOdmTouchOrientation_XY_SWAP | NvOdmTouchOrientation_H_FLIP | NvOdmTouchOrientation_V_FLIP)
#endif
};


#if TPK_ADL340_WAR
// Dummy write accelerometer in order to workaround a HW bug of ADL340
// Will Remove it once we use new accelerometer
static NvBool NvAccDummyI2CSetRegs(TPK_TouchDevice* hTouch)
{
    NvOdmI2cTransactionInfo TransactionInfo;
    NvU8 arr[2];
    NvOdmI2cStatus Error;    

    arr[0] = 0x0;
    arr[1] = 0x0;

    TransactionInfo.Address = 0x3A;
    TransactionInfo.Buf = arr;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    // Write dummy data to accelerometer
    Error = NvOdmI2cTransaction(hTouch->hOdmI2c,
                                &TransactionInfo,
                                1,
                                hTouch->I2cClockSpeedKHz,
                                TPK_I2C_TIMEOUT);

    if (Error != NvOdmI2cStatus_Success)
    {
        //NvOdmOsDebugPrintf("error!\r\n");
        return NV_FALSE;
    }                        
    //NvOdmOsDebugPrintf("dummy!\r\n");
    return NV_TRUE;
}
#endif


static NvU32 TPK_SPIGetData (TPK_TouchDevice* hTouch, NvU8 uData)
{
    #define DummyBit    0x0
    NvU8 pSendData[8], pRecData[8], i;

    for(i=0; i<8; i++)
      pRecData[i] = 0;  

    pSendData[0] = (uData >> 5) & 0x7;
    pSendData[1] = (uData >> 2) & 0x7;
    pSendData[2] = (((uData)& 0x3) << 1) | DummyBit;
    pSendData[3] = 0;
    pSendData[4] = 0;
    pSendData[5] = 0;
    pSendData[6] = 0;
    pSendData[7] = 0;


    NvOdmSpiTransaction(hTouch->hOdmSpi,
                        hTouch->DeviceAddr,
                        hTouch->SPIClockSpeedKHz,
                        pRecData, pSendData, 8, 3);

    return ((pRecData[3] & 0x7) << 9) | ((pRecData[4] & 0x7) << 6) | ((pRecData[5] & 0x7) << 3) | (pRecData[6] & 0x7);
}


static NvBool TPK_SetPage (TPK_TouchDevice* hTouch, NvU8 page)
{

    return NV_TRUE;
}

#if TPK_CHECK_ERRORS
static void TPK_CheckError (TPK_TouchDevice* hTouch)
{
   
}
#endif

static NvBool TPK_Configure (TPK_TouchDevice* hTouch)
{
    hTouch->SleepMode = 0x0;
    hTouch->SampleRate = 0; /* this forces register write */
    return TPK_SetSampleRate(&hTouch->OdmTouch, TPK_HIGH_SAMPLE_RATE);
}

//for median filter,
NvBool ResetFilterConfig(void)
{
    int i = 0;
    int j = 0;

    for(i=0;i<COORDS_NUM;i++)
    {
        for( j=0; j<SAMPLE_SET_NUM; j++ )
        {
            gFifo[i][j] = IGNORE_MAX_DATA;
            gSort[i][j] = IGNORE_MAX_DATA;
        }
        gLast_issued[i] = 1;
    }

    gPos = 0;
    gGetDoneNums = 0;


    return NV_TRUE; 
} 

void filter_fifo_insert(int *p, int sample, int pos)
{
    int i =0;

    //insert to fifo
    if( pos < SAMPLE_SET_NUM )
    {
        p[pos] = sample;
    }
    else if( pos == SAMPLE_SET_NUM )//fifo full
    {
        //remove oldest one
        for(i=0; i < pos-1 ; i++)
        {
            p[i] = p[i+1];
        }
        p[pos-1] = sample;
    }
}

void filter_sort_insert(int *p, int sample, int count)
{
    int i = 0;
    int j = 0;

    for(i=0; i<count; i++)
    {
        if( sample < p[i] )
        {
            for( j=count; j>i; j--)
            {
                if( j != SAMPLE_SET_NUM  )
                    p[j] = p[j-1];
            }
            p[i] = sample;
            return;
        }
    }

    //no bigger than sample
    p[count] = sample;

}

void filter_sort_del(int *p, int sample, int count)
{
    int i = 0;

    for(i=0; i<count; i++)
    {
        if( p[i] == sample )
        {
            //re-stort
            for( ; i<count-1; i++ )
                p[i] = p[i+1];

            //set last one item to max to ready to del 
            p[i] = IGNORE_MAX_DATA;   

            return;
        }
    }
}
//end 

static NvBool TPK_GetSample (TPK_TouchDevice* hTouch, NvOdmTouchCoordinateInfo* coord)
{

    int i = 0;
    int x,y;
    int midpos = 0;
    int xsum = 0;
    int ysum = 0;
    int movement = 1;
    int sampleSetsStart = 0, sampleSetsEnd = 0;
    int realSampleCalSetNum = 0;
	int z1=0,z2=0;
	unsigned long pressure=0,temp=0,temp2=0;


    //start median filter
    if( gPos == 0 )
    {
        //get enough sets data
        for(i=0; i<SAMPLE_SET_NUM; i++)
        {
			//just get pressure data
			z1 = TPK_SPIGetData(hTouch, 0xB3);
			z2 = TPK_SPIGetData(hTouch, 0xC3);
			if( z1==0 || z2==0 ) 
			{
				coord->fingerstate = NvOdmTouchSampleIgnore;
				return NV_TRUE; 
			}
			x = TPK_SPIGetData(hTouch, 0xD3);
			y = TPK_SPIGetData(hTouch, 0x93);
			//if get 0 then user no more touch, here not enough data sets
			if( x==0 || y==0 ) 
			{
				coord->fingerstate = NvOdmTouchSampleIgnore;
				return NV_TRUE; 
			}

			//pressure = (CALCULATE_UNIT*RESISTANCE_X_PLATE*x/4096)*((z2/z1)-1);
			temp = (unsigned long)21*x*z2/z1;
			temp2 = (unsigned long)21*x;
			pressure = temp - temp2;
			if( pressure > PRESSURE_MAX_BOUND )
			{
				coord->fingerstate = NvOdmTouchSampleIgnore;
				return NV_TRUE;
			}


            //insert to fifo
            filter_fifo_insert( &gFifo[X][0], x, i );
            filter_fifo_insert( &gFifo[Y][0], y, i );

            //insert to sort
            filter_sort_insert( &gSort[X][0], x, i);
            filter_sort_insert( &gSort[Y][0], y, i);

            //printk("loop %d [ %d %d %d %d %d %d %d] %d\n", i, gSort[X][0], gSort[X][1], gSort[X][2], gSort[X][3], gSort[X][4], gSort[X][5], gSort[X][6],x);

        }

        gPos = SAMPLE_SET_NUM;

    }
    else if( gPos == SAMPLE_SET_NUM )
    {
        //just get pressure data
		z1 = TPK_SPIGetData(hTouch, 0xB3);
		z2 = TPK_SPIGetData(hTouch, 0xC3);
		if( z1==0 || z2==0) 
		{
			coord->fingerstate = NvOdmTouchSampleIgnore;
			return NV_TRUE; 
		}
		x = TPK_SPIGetData(hTouch, 0xD3);
		y = TPK_SPIGetData(hTouch, 0x93);
		//if get 0 then user out of range,
		if( x==0 || y==0 ) 
		{
			coord->fingerstate = NvOdmTouchSampleIgnore;
			return NV_TRUE; 
		}

		//pressure = (CALCULATE_UNIT*RESISTANCE_X_PLATE*x/4096)*((z2/z1)-1);
		temp = (unsigned long)21*x*z2/z1;
		temp2 = (unsigned long)21*x;
		pressure = temp - temp2;
		if( pressure > PRESSURE_MAX_BOUND )
		{
			coord->fingerstate = NvOdmTouchSampleIgnore;
			return NV_TRUE;
		}


        //del one of sort
        filter_sort_del( &gSort[X][0], gFifo[X][0], gPos );
        filter_sort_del( &gSort[Y][0], gFifo[Y][0], gPos );

        //insert to fifo
        filter_fifo_insert( &gFifo[X][0], x, gPos );
        filter_fifo_insert( &gFifo[Y][0], y, gPos );

        //insert to sort
        filter_sort_insert( &gSort[X][0], x, gPos);
        filter_sort_insert( &gSort[Y][0], y, gPos);


        //printk("insert [ %d %d %d %d %d %d %d ] %d\n", gSort[X][0], gSort[X][1], gSort[X][2], gSort[X][3], gSort[X][4], gSort[X][5], gSort[X][6], x);
    }


    if( gGetDoneNums > 0 )  //need more datas
    {
        gGetDoneNums--;
        return NV_FALSE;
    }
    else
    {

        midpos = SAMPLE_SET_NUM/2;

        
        //sum of middle value
        sampleSetsStart = midpos - (SAMPLE_CAL_SET_NUM/2);
        sampleSetsEnd = midpos + (SAMPLE_CAL_SET_NUM/2);
        if( sampleSetsStart < 0 )   
            sampleSetsStart = 0;
        if( sampleSetsEnd > SAMPLE_SET_NUM )
            sampleSetsEnd = SAMPLE_SET_NUM ;

        // it should be realSampleCalSetNum ==  SAMPLE_CAL_SET_NUM, if SAMPLE_SET_NUM big enough
        realSampleCalSetNum = sampleSetsEnd - sampleSetsStart;

        for( i=sampleSetsStart ; i<sampleSetsEnd ; i++ )
        {
            xsum += gSort[X][i];
            ysum += gSort[Y][i];
        }
                

        //calculate movment
        movement += abs( gLast_issued[X] - xsum);
        movement += abs( gLast_issued[Y] - ysum);

        //printk("movement = %d \n",movement);

        if( movement >  FILTER_THRESHOLD )  //move fast
            gGetDoneNums = FILTER_ABOVE;
        else
            gGetDoneNums = FILTER_BELOW;

        gLast_issued[X] = xsum;
        gLast_issued[Y] = ysum;


        x = xsum/realSampleCalSetNum;
        y = ysum/realSampleCalSetNum;


        coord->fingerstate = NvOdmTouchSampleValidFlag;
        coord->additionalInfo.Gesture = NvOdmTouchGesture_Tap;
        coord->additionalInfo.Fingers = 1;

        //report x,y to up level
        coord->fingerstate |= NvOdmTouchSampleDownFlag;

        coord->xcoord =
        coord->additionalInfo.multi_XYCoords[0][0] = x;

        coord->ycoord =
        coord->additionalInfo.multi_XYCoords[0][1] = y;

        coord->additionalInfo.width[0] = 12;
        coord->additionalInfo.Pressure[0] = 1;

    }
    //end median filter


    return NV_TRUE;

}

static void InitOdmTouch (NvOdmTouchDevice* Dev)
{
    Dev->Close              = TPK_Close;
    Dev->GetCapabilities    = TPK_GetCapabilities;
    Dev->ReadCoordinate     = TPK_ReadCoordinate;
    Dev->EnableInterrupt    = TPK_EnableInterrupt;
    Dev->HandleInterrupt    = TPK_HandleInterrupt;
    Dev->GetSampleRate      = TPK_GetSampleRate;
    Dev->SetSampleRate      = TPK_SetSampleRate;
    Dev->PowerControl       = TPK_PowerControl;
    Dev->PowerOnOff         = TPK_PowerOnOff;
    Dev->GetCalibrationData = TPK_GetCalibrationData;
    Dev->OutputDebugMessage = NV_FALSE;
}

static void TPK_GpioIsr(void *arg)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)arg;

    NVODMTOUCH_PRINTF(("JimDebug TPK_GpioIsr...\n"));
    /* Signal the touch thread to read the sample. After it is done reading the
     * sample it should re-enable the interrupt. */
    NvOdmOsSemaphoreSignal(hTouch->hIntSema);
}

NvBool TPK_ReadCoordinate (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo* coord)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;

#if TPK_BENCHMARK_SAMPLE    
    NvU32 time = NvOdmOsGetTimeMS();
#endif
//    NVODMTOUCH_PRINTF(("GpioIst+\n"));

#if TPK_CHECK_ERRORS
    TPK_CheckError(hTouch);
#endif

    for (;;)
    {
        if (TPK_GetSample(hTouch, coord))
        {
            //enable pen irq interrupts
            TPK_SPIGetData(hTouch, 0x80);
            break;
        }


    }

#if TPK_BENCHMARK_SAMPLE    
    NvOdmOsDebugPrintf("Touch sample time %d\n", NvOdmOsGetTimeMS() - time);
#endif
    
    return NV_TRUE;
}

void TPK_GetCapabilities (NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;
    *pCapabilities = hTouch->Caps;
}

NvBool TPK_PowerOnOff (NvOdmTouchDeviceHandle hDevice, NvBool OnOff)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;

    hTouch->hPmu = NvOdmServicesPmuOpen();

    if (!hTouch->hPmu)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmServicesPmuOpen Error \n"));
        return NV_FALSE;
    }
    
    if (OnOff != hTouch->PowerOn)
    {
        NvOdmServicesPmuVddRailCapabilities vddrailcap;
        NvU32 settletime;

        NvOdmServicesPmuGetCapabilities( hTouch->hPmu, hTouch->VddId, &vddrailcap);

        if(OnOff)
		{
            NvOdmServicesPmuSetVoltage( hTouch->hPmu, hTouch->VddId, vddrailcap.requestMilliVolts, &settletime);
		}
        else
		{
            NvOdmServicesPmuSetVoltage( hTouch->hPmu, hTouch->VddId, NVODM_VOLTAGE_OFF, &settletime);
		}

        if (settletime)
            NvOdmOsWaitUS(settletime); // wait to settle power

        hTouch->PowerOn = OnOff;

        if(OnOff)
            NvOdmOsSleepMS(TPK_POR_DELAY);
    }

    NvOdmServicesPmuClose(hTouch->hPmu);

    return NV_TRUE;
}


NvBool TPK_Open (NvOdmTouchDeviceHandle* hDevice)
{
    TPK_TouchDevice* hTouch;
    NvU32 i;
    NvU32 found = 0;
    NvU32 GpioPort = 0;
    NvU32 GpioPin = 0;
//    NvU32 irq;
    NvU32 SpiInstance = 0;


#if TPK_QUERY_SENSOR_RESOLUTION
    NvU8  sensorresolution = 0; //units per millimeter
#endif
#if TPK_SET_MAX_POSITION
    NvU32 SET_MAX_POSITION = 8191; //Max Position range from 0x0002 to 0x1fff
    NvU32 SENSOR_MAX_POSITION = 0;
#endif

    const NvOdmPeripheralConnectivity *pConnectivity = NULL;

    NVODMTOUCH_PRINTF(("<JimDebug>TPK_OPEN...1\n"));

    hTouch = NvOdmOsAlloc(sizeof(TPK_TouchDevice));
    if (!hTouch) return NV_FALSE;

    NVODMTOUCH_PRINTF(("<JimDebug>TPK_OPEN...2\n"));

    NvOdmOsMemset(hTouch, 0, sizeof(TPK_TouchDevice));

    /* set function pointers */
    InitOdmTouch(&hTouch->OdmTouch);

    pConnectivity = NvOdmPeripheralGetGuid(TSC_TOUCH_DEVICE_GUID);
    if (!pConnectivity)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : pConnectivity is NULL Error \n"));
        goto fail;
    }

    if (pConnectivity->Class != NvOdmPeripheralClass_HCI)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : didn't find any periperal in discovery query for touch device Error \n"));
        goto fail;
    }

    NVODMTOUCH_PRINTF(("pConnectivity->NumAddress = %d\n", pConnectivity->NumAddress));
    for (i = 0; i < pConnectivity->NumAddress; i++)
    {
        switch (pConnectivity->AddressList[i].Interface)
        {
            case NvOdmIoModule_Spi:
                hTouch->DeviceAddr = pConnectivity->AddressList[i].Address; // cs number
                SpiInstance = pConnectivity->AddressList[i].Instance;       //spi number
                NVODMTOUCH_PRINTF(("<JimDebug>NvOdmIoModule_Spi...cs=%d, Instance=%d\n", hTouch->DeviceAddr, SpiInstance));
                found |= 1;
                break;
            case NvOdmIoModule_I2c:
                /*
                hTouch->DeviceAddr = (pConnectivity->AddressList[i].Address << 1);
                I2cInstance = pConnectivity->AddressList[i].Instance;
                found |= 1;
                */
                break;
            case NvOdmIoModule_Gpio:
                NVODMTOUCH_PRINTF(("<JimDebug>NvOdmIoModule_Gpio...\n"));
                GpioPort = pConnectivity->AddressList[i].Instance;
                GpioPin = pConnectivity->AddressList[i].Address;
                found |= 2;
                break;
            case NvOdmIoModule_Vdd:
                hTouch->VddId = pConnectivity->AddressList[i].Address;
                found |= 4;
                break;
            default:
                break;
        }
    }

    NVODMTOUCH_PRINTF(("<JimDebug>found = %d\n", found));
    if ((found & 3) != 3)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : peripheral connectivity problem \n"));
        goto fail;
    }
/*
    if ((found & 4) != 0)
    {
        if (NV_FALSE == TPK_PowerOnOff(&hTouch->OdmTouch, 1))
            goto fail;            
    }
    else
    {
        hTouch->VddId = 0xFF; 
    }
*/

    hTouch->hOdmSpi = NvOdmSpiOpen(NvOdmIoModule_Spi, SpiInstance);
    if (!hTouch->hOdmSpi)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmSpiOpen Error \n"));
        goto fail;
    }


    hTouch->hGpio = NvOdmGpioOpen();

    if (!hTouch->hGpio)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : NvOdmGpioOpen Error \n"));
        goto fail;
    }

    hTouch->hPin = NvOdmGpioAcquirePinHandle(hTouch->hGpio, GpioPort, GpioPin);
    if (!hTouch->hPin)
    {
        NVODMTOUCH_PRINTF(("NvOdm Touch : Couldn't get GPIO pin \n"));
        goto fail;
    }

    NvOdmGpioConfig(hTouch->hGpio,
                    hTouch->hPin,
                    NvOdmGpioPinMode_InputData);

    /* set default capabilities */
    NvOdmOsMemcpy(&hTouch->Caps, &TPK_Capabilities, sizeof(NvOdmTouchCapabilities));

    /* set default SPI speed */
    hTouch->SPIClockSpeedKHz = TPK_SPI_SPEED_KHZ;


    hTouch->Caps.XMinPosition = 0;
    hTouch->Caps.YMinPosition = 0;
    hTouch->Caps.XMaxPosition = 4095;
    hTouch->Caps.YMaxPosition = 4095;


    /* configure panel */
    if (!TPK_Configure(hTouch)) goto fail;

    *hDevice = &hTouch->OdmTouch;
    return NV_TRUE;

 fail:
    TPK_Close(&hTouch->OdmTouch);
    return NV_FALSE;
}

NvBool TPK_EnableInterrupt (NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hIntSema)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;
    NvOdmTouchCoordinateInfo coord;

    NV_ASSERT(hIntSema);
    
    /* can only be initialized once */
    if (hTouch->hGpioIntr || hTouch->hIntSema)
        return NV_FALSE;

    NVODMTOUCH_PRINTF(("JimDebug TPK_EnableInterrupt...\n"));
    /* zero intr status */
    //(void)TPK_GetSample(hTouch, &coord);

    hTouch->hIntSema = hIntSema;

    if (NvOdmGpioInterruptRegister(hTouch->hGpio, &hTouch->hGpioIntr,
        hTouch->hPin, NvOdmGpioPinMode_InputInterruptLow, TPK_GpioIsr,
        (void*)hTouch, TPK_DEBOUNCE_TIME_MS) == NV_FALSE)
    {
        return NV_FALSE;
    }

    if (!hTouch->hGpioIntr)
        return NV_FALSE;
  
    ResetFilterConfig();

    NVODMTOUCH_PRINTF(("JimDebug TPK_EnableInterrupt...End\n"));
    return NV_TRUE;
}

NvBool TPK_HandleInterrupt(NvOdmTouchDeviceHandle hDevice)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;
    NvU32 pinValue;
    
    NvOdmGpioGetState(hTouch->hGpio, hTouch->hPin, &pinValue);
    if (!pinValue)
    {
        //interrupt pin is still LOW, read data until interrupt pin is released.
        return NV_FALSE;
    }
    else
    {
        ResetFilterConfig();
        NvOdmGpioInterruptDone(hTouch->hGpioIntr);
    }
    
    return NV_TRUE;
}

NvBool TPK_GetSampleRate (NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;
    pTouchSampleRate->NvOdmTouchSampleRateHigh = 80;
    pTouchSampleRate->NvOdmTouchSampleRateLow = 40;
    pTouchSampleRate->NvOdmTouchCurrentSampleRate = (hTouch->SampleRate >> 1);
    return NV_TRUE;
}

NvBool TPK_SetSampleRate (NvOdmTouchDeviceHandle hDevice, NvU32 rate)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;

    if (rate != 0 && rate != 1)
        return NV_FALSE;
    
    rate = 1 << rate;
    
    if (hTouch->SampleRate == rate)
        return NV_TRUE;


    hTouch->SampleRate = rate;
    return NV_TRUE;
}


NvBool TPK_PowerControl (NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;
    NvU32 SleepMode;

    NV_ASSERT(hTouch->VddId != 0xFF);
    
    switch(mode)
    {
        case NvOdmTouch_PowerMode_0:
            SleepMode = 0x0;
            break;
        case NvOdmTouch_PowerMode_1:
        case NvOdmTouch_PowerMode_2:
        case NvOdmTouch_PowerMode_3:
            SleepMode = 0x03;
            break;
        default:
            return NV_FALSE;
    }

    if (hTouch->SleepMode == SleepMode)
        return NV_TRUE;


    hTouch->SleepMode = SleepMode;    
    return NV_TRUE;
}

NvBool TPK_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer)
{
#if TPK_SCREEN_ANGLE
    //Portrait
    static const NvS32 RawCoordBuffer[] = {2054, 3624, 3937, 809, 3832, 6546, 453, 6528, 231, 890};
#else
    //Landscape
    static NvS32 RawCoordBuffer[] = {2054, 3624, 3832, 6546, 453, 6528, 231, 890, 3937, 809};
#endif

    NVODMTOUCH_PRINTF(("JimDebug TPK_GetCalibrationData\n"));

    if (NumOfCalibrationData*2 != (sizeof(RawCoordBuffer)/sizeof(NvS32)))
    {
        NVODMTOUCH_PRINTF(("WARNING: number of calibration data isn't matched\n"));
        return NV_FALSE;
    }
    
    NvOdmOsMemcpy(pRawCoordBuffer, RawCoordBuffer, sizeof(RawCoordBuffer));

    return NV_TRUE;
}


void TPK_Close (NvOdmTouchDeviceHandle hDevice)
{
    TPK_TouchDevice* hTouch = (TPK_TouchDevice*)hDevice;

    if (!hTouch) return;
        
    if (hTouch->hGpio)
    {
        if (hTouch->hPin)
        {
            if (hTouch->hGpioIntr)
                NvOdmGpioInterruptUnregister(hTouch->hGpio, hTouch->hPin, hTouch->hGpioIntr);

            NvOdmGpioReleasePinHandle(hTouch->hGpio, hTouch->hPin);
        }

        NvOdmGpioClose(hTouch->hGpio);
    }

    if (hTouch->hOdmSpi)
        NvOdmSpiClose(hTouch->hOdmSpi);

    NvOdmOsFree(hTouch);
}


