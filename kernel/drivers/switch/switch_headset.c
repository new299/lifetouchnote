/**************************************************
 * Copyright (C) 2011 NEC Corporation
 **************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/workqueue.h>

//for headphone detect interrupt,,20100303
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"


static NvOdmServicesGpioHandle s_hGpio = NULL;
static NvOdmGpioPinHandle s_hHeadphoneGpioPin = NULL;
static NvOdmServicesGpioIntrHandle s_hGpioIntr_Headphone = NULL;

static NvOdmOsThreadHandle g_hDetectThread = NULL;
static NvOdmOsSemaphoreHandle g_hDetectEventSema = NULL;
static int g_DetectThread_exit = 1;
static unsigned int gWaitTmes_msec = 800;

#define HEADSET_STATUS_REMOVE   0x00
#define HEADSET_STATUS_INSERT   0x01

#define HEADSET_STATUS_NONE         0x00
#define HEADSET_STATUS_NO_MIC       0x01
#define HEADSET_STATUS_ONLY_MIC     0x02    //no use now
#define HEADSET_STATUS_WITH_MIC     0x03



static int gHeadsetInsertStatus = HEADSET_STATUS_REMOVE;
static int gHeadsetStatus = HEADSET_STATUS_REMOVE;

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
    //unsigned pin;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
};


static void DetectThreadFun(void* arg)
{
    NvU32 value = 0;
    int tempHeadsetStatus = gHeadsetStatus; 

    struct gpio_switch_data* data = (struct gpio_switch_data*)arg;
    
    
    while( !g_DetectThread_exit )
    {       

        NvOdmOsSemaphoreWaitTimeout( g_hDetectEventSema , gWaitTmes_msec);

        //detect mic status here , but no use now
        //just assign headphone status
        if( gHeadsetInsertStatus == HEADSET_STATUS_INSERT )
        {      
                tempHeadsetStatus = HEADSET_STATUS_INSERT;
        }
        else
        {
                tempHeadsetStatus = HEADSET_STATUS_REMOVE;
        }


        //check if status change
        if( tempHeadsetStatus != gHeadsetStatus )
        {
            gHeadsetStatus = tempHeadsetStatus;
            switch_set_state(&data->sdev, gHeadsetStatus);
        }



    }//while( !g_micDetectThread_exit )
    
    return;
}

static void headset_switch_work(struct work_struct *work)
{ 
	//int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);

	//state = gpio_get_value(data->gpio);
    //detect headphone   

	//switch_set_state(&data->sdev, state);
}


static void headset_irq_handler(void* arg)
{
    NvU32 states;

    //printk("===============================> headset_irq_handler\n");

    NvOdmGpioGetState(s_hGpio,s_hHeadphoneGpioPin,&states);
    if( !states )
    {     
        gHeadsetInsertStatus = HEADSET_STATUS_REMOVE;
        NvOdmGpioConfig(s_hGpio,s_hHeadphoneGpioPin,NvOdmGpioPinMode_InputInterruptHigh );
    }
    else
    {        
        gHeadsetInsertStatus = HEADSET_STATUS_INSERT;
        NvOdmGpioConfig(s_hGpio,s_hHeadphoneGpioPin,NvOdmGpioPinMode_InputInterruptLow );
    }


	//schedule_work(&switch_data->work);
    if ( g_hDetectEventSema )
        NvOdmOsSemaphoreSignal( g_hDetectEventSema );


    if (s_hGpioIntr_Headphone)
        NvOdmGpioInterruptDone(s_hGpioIntr_Headphone);

	return;
}


static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
    int stateValue = 0;

    /*
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
    */

    stateValue = switch_get_state(sdev);
    return sprintf(buf, "%d\n", stateValue);
    
	return -1;
}

/*
static ssize_t	switch_headset_print_name(struct switch_dev *sdev, char *buf)
{
    struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);


    return sprintf(buf, "%s\n", test);
}
*/

static int headset_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_switch_data *switch_data;
	int ret = 0;
    NvU32 states;


	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
    //switch_data->pin = pdata->pin;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_headset_print_state;


    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;


    if( !s_hGpio )
    {
        s_hGpio = NvOdmGpioOpen();
        if( !s_hGpio )
        {
            goto err_request_gpio;
        }
    }    

    s_hHeadphoneGpioPin = NvOdmGpioAcquirePinHandle(s_hGpio, 'w'-'a', 2);
            //switch_data->gpio, switch_data->pin);

    NvOdmGpioConfig( s_hGpio, s_hHeadphoneGpioPin, NvOdmGpioPinMode_InputData);


	INIT_WORK(&switch_data->work, headset_switch_work);

    //create thread
    if( !g_hDetectEventSema )
        g_hDetectEventSema  = NvOdmOsSemaphoreCreate(0);
    g_DetectThread_exit = 0;
    if( !g_hDetectThread )
        g_hDetectThread = NvOdmOsThreadCreate( (NvOdmOsThreadFunction)DetectThreadFun,
                            (void*)switch_data);

    // headphone ISR      
    if (!NvOdmGpioInterruptRegister(
        s_hGpio, &s_hGpioIntr_Headphone, s_hHeadphoneGpioPin,
        NvOdmGpioPinMode_InputInterruptLow, headset_irq_handler, (void*)switch_data, 0))
    {
        return NV_FALSE;
    }
  

    //detect headphone right now
    NvOdmGpioGetState(s_hGpio,s_hHeadphoneGpioPin,&states);
    if( !states )
    {
            gHeadsetInsertStatus = HEADSET_STATUS_REMOVE;
            NvOdmGpioConfig(s_hGpio,s_hHeadphoneGpioPin,NvOdmGpioPinMode_InputInterruptHigh );
    }
    else
    {
            gHeadsetInsertStatus = HEADSET_STATUS_INSERT;
            NvOdmGpioConfig(s_hGpio,s_hHeadphoneGpioPin,NvOdmGpioPinMode_InputInterruptLow );               
    } 

	/* Perform initial detection */
	//headset_switch_work(&switch_data->work);


	return 0;


err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit headset_switch_remove(struct platform_device *pdev)
{

	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	

    //remove interrupt
    NvOdmGpioInterruptUnregister(
        s_hGpio, s_hHeadphoneGpioPin, s_hGpioIntr_Headphone);
    NvOdmGpioReleasePinHandle(s_hGpio, s_hHeadphoneGpioPin);
    s_hHeadphoneGpioPin = NULL;

    //remove thread
    //terminate thread 
    g_DetectThread_exit = 1;
    if ( g_hDetectEventSema )
        NvOdmOsSemaphoreSignal( g_hDetectEventSema );
    
    if( !g_hDetectThread )
        NvOdmOsThreadJoin( g_hDetectThread );
    g_hDetectThread = NULL;

    if ( g_hDetectEventSema )
    {
        NvOdmOsSemaphoreDestroy( g_hDetectEventSema );
        g_hDetectEventSema = 0;
    }

    switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);


	return 0;
}

static struct platform_driver headset_switch_driver = {
	.probe		= headset_switch_probe,
	.remove		= __devexit_p(headset_switch_remove),
	.driver		= {
		.name	= "headset-switch",
		.owner	= THIS_MODULE,
	},
};

static int __init headset_switch_init(void)
{
	return platform_driver_register(&headset_switch_driver);
}

static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&headset_switch_driver);
}

module_init(headset_switch_init);
module_exit(headset_switch_exit);

MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("Headset Switch driver");
MODULE_LICENSE("GPL");
