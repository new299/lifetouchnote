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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>

#include <asm/gpio.h>

#include <linux/kthread.h>


//for control EC
#include "nvec.h"

static NvEcHandle     ghEc = NULL;
//static NvEcRequest  gEcRequest = {0};
//static NvEcResponse gEcResponse = {0};
//end for EC


struct nvec_led_data {
	struct led_classdev cdev;
	//unsigned gpio;
	//struct work_struct work;
	//u8 active_low;
    int brightness;
    int blink_mode;
	int	testMode_OnOff; //
};

static void nvec_led_work(struct work_struct *work)
{
	//struct nvec_led_data	*led_dat =
	//	container_of(work, struct nvec_led_data, work);

	//gpio_set_value_cansleep(led_dat->gpio, led_dat->new_level);
}

static void nvec_brightness_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);

#if 0
    led_cdev->brightness = value;
    led_cdev->blink_mode = BLINK_MODE_OFF;

    NvError NvStatus = NvError_Force32;
    NvEcRequest  EcRequest = {0};
    NvEcResponse EcResponse = {0};
    
    //printk("led======================>nvec_brightness_set value=%d\n", value);

    if ( ghEc )
    {
            EcRequest.PacketType = NvEcPacketType_Request;
            EcRequest.RequestType = NvEcRequestResponseType_Led;
            EcRequest.RequestSubtype = NvEcLedSubtype_Mode_OnOff;

            EcRequest.NumPayloadBytes = 1;
            EcRequest.Payload[0] = value;

                
            NvStatus = NvEcSendRequest( ghEc, &EcRequest, &EcResponse,
                     sizeof(EcRequest), sizeof(EcResponse));
            if (NvStatus != NvSuccess)
            {
                printk("led======================>NvEcSendRequest eror\n");
            }

            if ( EcResponse.Status != NvEcStatus_Success)
            {
                printk("led======================>EcResponse eror\n");
                NvStatus = NvError_Force32;
            }
    }
    else    
    {
        printk("led======================>NVEC handle error\n");
    }

    if (NvStatus == NvSuccess)
    {
        led_dat->brightness = value;
        led_dat->blink_mode = BLINK_MODE_OFF;
    }
#endif

}

static enum led_brightness nvec_brightness_get(struct led_classdev *led_cdev)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);
	//int level;

    return led_dat->brightness; 
}




////////////blink
static void nvec_blink_mode_set(struct led_classdev *led_cdev,
	enum led_blink_mode value)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);
	int level;

    led_cdev->blink_mode = value;
    led_cdev->brightness = LED_OFF;

    NvError NvStatus = NvError_Force32;
    NvEcRequest  EcRequest = {0};
    NvEcResponse EcResponse = {0};

    
    //printk("led======================>nvec_blink_mode_set value=%d\n", value);
    
    if ( value == BLINK_MODE_ON1_OFF1 )
		level = BLINK_MODE_ON1_OFF1;
	else if( value == BLINK_MODE_ON1_OFF2 )
		level = BLINK_MODE_ON1_OFF2;
    else  if( value == BLINK_MODE_OFF )
        level = BLINK_MODE_OFF;
    else
        return;

    if ( ghEc )
    {
            EcRequest.PacketType = NvEcPacketType_Request;
            EcRequest.RequestType = NvEcRequestResponseType_Led;
            EcRequest.RequestSubtype = NvEcLedSubtype_Mode_Blink;

            EcRequest.NumPayloadBytes = 2;

            if( level == BLINK_MODE_ON1_OFF1 )
            {
                EcRequest.Payload[0] = 0x01;
                EcRequest.Payload[1] = 0x01;
            }
            else if( level == BLINK_MODE_ON1_OFF2 )
            {
                EcRequest.Payload[0] = 0x01;
                EcRequest.Payload[1] = 0x02;
            }
            else
            {
                EcRequest.Payload[0] = 0x00;
                EcRequest.Payload[1] = 0x00;
            }

                
            NvStatus = NvEcSendRequest( ghEc, &EcRequest, &EcResponse,
                     sizeof(EcRequest), sizeof(EcResponse));
            if (NvStatus != NvSuccess)
            {
                printk("led nvec_blink_mode_set======================>NvEcSendRequest eror\n");
            }

            if ( EcResponse.Status != NvEcStatus_Success)
            {
                printk("led nvec_blink_mode_set======================>EcResponse eror\n");
                NvStatus = NvError_Force32;
            }
    }
    else    
    {
        printk("led nvec_blink_mode_set======================>NVEC handle error\n");
    }


    if (NvStatus == NvSuccess)
    {
        led_dat->blink_mode = level;
        led_dat->brightness = LED_OFF;
    }
 
}


static enum led_blink_mode nvec_blink_mode_get(struct led_classdev *led_cdev)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);

    //printk("led======================>nvec_blink_mode_get\n");

    return led_dat->blink_mode; 
}


//for led trigger timer use,but just let us know delay_on ,delay_off values.we don't need to do flash,
static int nvec_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	//struct nvec_led_data *led_dat =
	//	container_of(led_cdev, struct nvec_led_data, cdev);

    //printk("led======================>nvec_blink_set\n");

	return 1;
}


//for MTP
static void nvec_test_mode_set(struct led_classdev *led_cdev, enum led_test_mode on_off)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);
	int level = 0;
    NvError NvStatus = NvError_Force32;
    NvEcRequest  EcRequest = {0};
    NvEcResponse EcResponse = {0};

    
    //printk("led======================>nvec_blink_mode_set value=%d\n", value);

    led_cdev->brightness = LED_OFF;
    led_cdev->blink_mode = BLINK_MODE_OFF;

    
    if ( on_off == TEST_MODE_ON )
		level = TEST_MODE_ON;
	else if( on_off == TEST_MODE_OFF )
		level = TEST_MODE_OFF;
    else
        return;


    if ( ghEc )
    {
            EcRequest.PacketType = NvEcPacketType_Request;
            EcRequest.RequestType = NvEcRequestResponseType_Led;
            EcRequest.RequestSubtype = NvEcLedSubtype_Mode_Test;

            EcRequest.NumPayloadBytes = 1;
			EcRequest.Payload[0] = level & 0xff;
            
                
            NvStatus = NvEcSendRequest( ghEc, &EcRequest, &EcResponse,
                     sizeof(EcRequest), sizeof(EcResponse));
            if (NvStatus != NvSuccess)
            {
                printk("led nvec_test_mode_set======================>NvEcSendRequest eror\n");
            }

            if ( EcResponse.Status != NvEcStatus_Success)
            {
                printk("led nvec_test_mode_set======================>EcResponse eror\n");
                NvStatus = NvError_Force32;
            }
    }
    else    
    {
        printk("led nvec_test_mode_set======================>NVEC handle error\n");
    }


    if (NvStatus == NvSuccess)
    {
        led_dat->testMode_OnOff = level;
        led_dat->brightness = LED_OFF;
        led_dat->blink_mode = BLINK_MODE_OFF;
    }
	
 
}


static enum led_test_mode nvec_test_mode_get(struct led_classdev *led_cdev)
{
	struct nvec_led_data *led_dat =
		container_of(led_cdev, struct nvec_led_data, cdev);

    //printk("led======================>nvec_blink_mode_get\n");

    return led_dat->testMode_OnOff; 
}


///////////////////////////////////////////////////////////////////////////////////

static int nvec_led_probe(struct platform_device *pdev)
{
	struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_led *cur_led;
	struct nvec_led_data *leds_data, *led_dat;
	int i, ret = 0;
    NvError NvStatus = NvError_Success;
    

	if (!pdata)
		return -EBUSY;

	leds_data = kzalloc(sizeof(struct nvec_led_data) * pdata->num_leds,
				GFP_KERNEL);
	if (!leds_data)
		return -ENOMEM;

	for (i = 0; i < pdata->num_leds; i++) {
		cur_led = &pdata->leds[i];
		led_dat = &leds_data[i];


	        led_dat->brightness = LED_OFF;
        	led_dat->blink_mode = BLINK_MODE_OFF;
		led_dat->testMode_OnOff = TEST_MODE_OFF;

		led_dat->cdev.name = cur_led->name;
		led_dat->cdev.default_trigger = cur_led->default_trigger;

		led_dat->cdev.blink_set = nvec_blink_set;
		led_dat->cdev.brightness_set = nvec_brightness_set;
	        led_dat->cdev.brightness_get = nvec_brightness_get;
	
        	led_dat->cdev.blink_mode_set = nvec_blink_mode_set;
	        led_dat->cdev.blink_mode_get = nvec_blink_mode_get;
	        led_dat->cdev.blink_mode     = led_dat->blink_mode;

		led_dat->cdev.test_mode_set = nvec_test_mode_set;
	        led_dat->cdev.test_mode_get = nvec_test_mode_get;
		led_dat->cdev.testMode_OnOff = led_dat->testMode_OnOff;

		led_dat->cdev.brightness = led_dat->brightness;
        led_dat->cdev.blink_mode = led_dat->blink_mode;
        led_dat->cdev.testMode_OnOff = led_dat->testMode_OnOff;
		led_dat->cdev.flags |= (LED_CORE_SUSPENDRESUME);


		//INIT_WORK(&led_dat->work, nvec_led_work);

		ret = led_classdev_register(&pdev->dev, &led_dat->cdev);
		if (ret < 0) {
			goto err;
		}
	}

	platform_set_drvdata(pdev, leds_data);

    //for EC
    if( !ghEc )
        NvStatus = NvEcOpen( &ghEc, 0 /* instance */); 
    if (NvStatus != NvError_Success)
    {
        printk("led driver NvEcOpen error\n");
    }
    //end for EC

	return 0;

err:
	if (i > 0) {
		for (i = i - 1; i >= 0; i--) {
			led_classdev_unregister(&leds_data[i].cdev);
			//cancel_work_sync(&leds_data[i].work);
		}
	}

	kfree(leds_data);

	return ret;
}

static int __devexit nvec_led_remove(struct platform_device *pdev)
{
	int i;
	struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
	struct nvec_led_data *leds_data;

    //printk("led======================>nvec_led_remove\n");

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
		led_classdev_unregister(&leds_data[i].cdev);
		//cancel_work_sync(&leds_data[i].work);
	}

	kfree(leds_data);

    if( ghEc )
        NvEcClose( ghEc );
	return 0;
}

static void nvec_led_shutdown(struct platform_device *pdev)
{
	int i;
	struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
	struct nvec_led_data *leds_data;

    //printk("led======================>nvec_led_shutdown\n");

	leds_data = platform_get_drvdata(pdev);

	for (i = 0; i < pdata->num_leds; i++) {
        nvec_blink_mode_set(&leds_data[i].cdev,BLINK_MODE_OFF);
		led_classdev_unregister(&leds_data[i].cdev);
	}

	kfree(leds_data);

    if( ghEc )
        NvEcClose( ghEc );
	return 0;
}

static struct platform_driver nvec_led_driver = {
	.probe		= nvec_led_probe,
	.remove		= __devexit_p(nvec_led_remove),
    .shutdown	= nvec_led_shutdown,
	.driver		= {
		.name	= "nvec_leds",
		.owner	= THIS_MODULE,
	},
};

static int __init nvec_led_init(void)
{
	return platform_driver_register(&nvec_led_driver);
}

static void __exit nvec_led_exit(void)
{
	platform_driver_unregister(&nvec_led_driver);
}

module_init(nvec_led_init);
module_exit(nvec_led_exit);

MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("NVEC LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:leds-nvec");

