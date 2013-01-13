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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>

#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_gpio.h"


#define STATUS_ACTIVE	1
#define STATUS_RESET	2
#define STATUS_SUSPEND	3

static NvOdmServicesGpioHandle s_hGpio = NULL;
static NvOdmGpioPinHandle s_hResetGpioPin = NULL;

void ms3c_ControlStatus(int status)
{
    if( !s_hGpio )
        s_hGpio = NvOdmGpioOpen();

    if( s_hGpio )
    {
            //GPIO_PX1
            if( !s_hResetGpioPin )
                s_hResetGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,'x'-'a',1);

            
            if( s_hResetGpioPin )  
            {
                //active reset
                if( status == STATUS_ACTIVE)
				{
					printk("==============>Msensor_ACTIVE\r\n");
                    NvOdmGpioSetState(s_hGpio, s_hResetGpioPin, 0x1);
	                NvOdmGpioConfig( s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Output);
				}
                else if( status == STATUS_RESET) // reset
				{
                    NvOdmGpioSetState(s_hGpio, s_hResetGpioPin, 0x0);
	                NvOdmGpioConfig( s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Output);
				}
				else
				{
					printk("==============>Msensor_SUSPEND\r\n");
					NvOdmGpioSetState(s_hGpio, s_hResetGpioPin, 0x0);
					NvOdmGpioConfig(s_hGpio, s_hResetGpioPin, NvOdmGpioPinMode_Tristate);
				}


            }     
    }    	    

}


static int Msensor_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("==============>Msensor_suspend\r\n");

	ms3c_ControlStatus(STATUS_SUSPEND);

	return 0;
}

static int Msensor_resume(struct platform_device *pdev)
{
	printk("==============>Msensor_resume\r\n");

	ms3c_ControlStatus(STATUS_ACTIVE);

	return 0;
}

static int Msensor_probe(struct platform_device *pdev)
{

	int ret = 0;

	//printk("==============>Msensor_probe\r\n");

    if( !s_hGpio )
        s_hGpio = NvOdmGpioOpen();

	if( !s_hResetGpioPin )
                s_hResetGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio,'x'-'a',1);
	
	ms3c_ControlStatus(STATUS_ACTIVE);

	return ret;
}

static int __devexit Msensor_remove(struct platform_device *pdev)
{

	//printk("==============>Msensor_remove\r\n");
	
	ms3c_ControlStatus(STATUS_SUSPEND);

	if( s_hResetGpioPin )
		NvOdmGpioReleasePinHandle(s_hGpio, s_hResetGpioPin);
	if( s_hGpio )
		NvOdmGpioClose(s_hGpio);

	return 0;
}


static struct platform_driver Msensor_driver = {
	.probe		= Msensor_probe,
	.remove		= __devexit_p(Msensor_remove),
	.driver		= {
		.name	= "tegra_Msensor",
		.owner	= THIS_MODULE,
	},
	.suspend = Msensor_suspend,
	.resume = Msensor_resume,
};

static int __init Msensor_init(void)
{
	return platform_driver_register(&Msensor_driver);
}

static void __exit Msensor_exit(void)
{
	platform_driver_unregister(&Msensor_driver);
}

module_init(Msensor_init);
module_exit(Msensor_exit);

MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("M sensor driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:Msensor");

