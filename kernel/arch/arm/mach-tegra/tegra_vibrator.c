/**************************************************
 * Copyright (C) 2011 NEC Corporation
 **************************************************/
/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>

#include <../drivers/staging/android/timed_output.h>

#include "mach/nvrm_linux.h"
#include "nvos.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_vibrate.h"


#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_services.h"

#define VIBRATOR_GUID NV_ODM_GUID('v','i','b','r','a','t','o','r')
NvOdmServicesGpioHandle hGpio=NULL;
NvOdmGpioPinHandle hVibGpioPin=NULL;

#define TPS658620_I2C_SPEED_KHZ  100
#define TPS658620_DEVICE_ADDR    0x68
#define TPS658620_PWM_ADDR       0x5B
#define TPS658620_PWM1_ADDR      0x5A
#define TPS658620_PWM2_ADDR      0x5C

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

#define MAX_DUTYCYCLE   0x0A
#define MIN_FREQUENCY   0x04
#define MAX_FREQUENCY   0xFA


static int s_Timeout;
static int g_index = 65;// default value


NvBool VibSetIndex(NvS32 Freq)
{
    //modify by , DPWM_MODE=0, freq=250Hz ,duty cycle=0~127
    /*
    NvU8 temp = 0;
    temp = (NvU8)Freq;
    if(temp < 0 || temp > 63)
    {
        DEBUG_VIBRATOR_TRACE(("[VIBE] : Invaid scope!\n"));
        return NV_FALSE;
    }
    temp += 192; // offset
    g_index = temp;
    */
    NvU8 temp = 0;
    temp = (NvU8)Freq;
    if(temp < 0 || temp > 127)
    {
        DEBUG_VIBRATOR_TRACE(("[VIBE] : Invaid scope!\n"));
        return NV_FALSE;
    }
    else
        g_index = temp;

    return NV_TRUE;
}

NvBool VibStart(void)
{
 	const NvOdmPeripheralConnectivity *conn;
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
        hGpio = NvOdmGpioOpen();
        if (hGpio == NULL)
        {
                return NV_FALSE;
        }
        
        if (hVibGpioPin == NULL)
        {
                /* Search for the GPIO pin */
                if (conn->AddressList[0].Interface == NvOdmIoModule_Gpio)
                {
                     hVibGpioPin = NvOdmGpioAcquirePinHandle(hGpio, 
                           conn->AddressList[0].Instance,
                           conn->AddressList[0].Address);
                }
        }
        
        if (hVibGpioPin == NULL)
        {
                return NV_FALSE;
        }

        NvOdmGpioSetState(hGpio, hVibGpioPin, 0x1);
        NvOdmGpioConfig(hGpio, hVibGpioPin, NvOdmGpioPinMode_Output);

        VIBE_TPS658620_I2cWrite8(TPS658620_PWM1_ADDR, g_index);

        return NV_TRUE;
}

NvBool VibStop(void)
{
    NvOdmGpioSetState(hGpio, hVibGpioPin, 0x0);
    NvOdmGpioConfig(hGpio, hVibGpioPin, NvOdmGpioPinMode_Output);

    VIBE_TPS658620_I2cWrite8(TPS658620_PWM1_ADDR, 0x00);

    return NV_TRUE;
}


static void vibrator_enable(struct timed_output_dev *dev, int value)
{
   s_Timeout = value;

   dev->vibrateTime = s_Timeout;
   DEBUG_VIBRATOR_TRACE(("vibrator_enable()++ %d  \n",value));
        
   if (value)
   {
        VibStart();
        msleep(value);
        VibStop();
   }
   else
  {
        VibStop();
  }
    DEBUG_VIBRATOR_TRACE(("vibrator_enable()--  \n"));
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
    DEBUG_VIBRATOR_TRACE(("vibrator_get_time()++  \n"));
    DEBUG_VIBRATOR_TRACE(("vibrator_get_time()--  \n"));
    return dev->vibrateTime;
}

static int vibrator_get_index(struct timed_output_dev *dev)
{
    DEBUG_VIBRATOR_TRACE(("vibrator_get_index()++  \n"));
    DEBUG_VIBRATOR_TRACE(("vibrator_get_index()--  \n"));
    return g_index;
}

static void set_index(struct timed_output_dev *dev, int value)
{
    
    DEBUG_VIBRATOR_TRACE(("Set set 4 ~ 22.2Hz by duty cycle...\n"));

    //modify by 
    VibSetIndex(value);

}

static struct timed_output_dev tegra_vibrator = {
    .name = "vibrator",
    .get_time = vibrator_get_time,
    .enable = vibrator_enable,
    .findex = set_index,
    .get_index = vibrator_get_index,
};

//////////////////////////////////////////////////////////////////////////////////////////////////////

static int __init tegra_vibe_probe(struct platform_device *pdev)
{
        //int err = 0;
        struct tps65862x_subdev_pdata *pdata;
        //struct als_data *data;
        
        pdata = pdev->dev.platform_data;
             

        s_Timeout = 0;
        tegra_vibrator.vibrateTime = s_Timeout;


        timed_output_dev_register(&tegra_vibrator);

	return 0;
}

static int tegra_vibe_remove(struct platform_device *pdev)
{
        VibStop();

        timed_output_dev_unregister(&tegra_vibrator);
        return 0;
}

static int tegra_vibe_suspend(struct platform_device *pdev, pm_message_t state)
{
        VibStop();
        return 0;
}

static struct platform_driver tegra_vibe_driver = {
	.probe = tegra_vibe_probe,
	.remove = tegra_vibe_remove,
    .driver = {
		   .name = "tegra_vibe",
		   .owner = THIS_MODULE,
            },
    .suspend = tegra_vibe_suspend,
};

static int __init init_tegra_vibrator(void)
{
        return platform_driver_register(&tegra_vibe_driver);
}

static void __exit exit_tegra_vibrator(void)
{
    DEBUG_VIBRATOR_TRACE(("Exiting tegra_vibrator \n"));

    timed_output_dev_unregister(&tegra_vibrator);

}

module_init(init_tegra_vibrator);
module_exit(exit_tegra_vibrator);

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_LICENSE("GPL");
