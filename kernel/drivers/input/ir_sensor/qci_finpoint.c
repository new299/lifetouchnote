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
#include "qci_finpoint.h"

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/ptrace.h>
#include <linux/i2c.h>
#include <asm/io.h>
//#include <mach/vreg.h> // Sam ---

// Sam +++
// Sam ---

//QCI Jacky Hsu, 20090929, modify start{
#include <linux/kobject.h>
//}end, QCI Jacky Hsu

//sort sulution in B stage,for IR button pull high,
#include "nvrm_structure.h"
#include "nvodm_query_pins_ap20.h"
#include "nvodm_query_pins.h"
//end 

#include "nvodm_services.h" //add by 

MODULE_VERSION("0.6 ["__DATE__"]");
MODULE_AUTHOR("Quanta Computer Inc. APBU");
MODULE_DESCRIPTION("Quanta Computer Inc. FINPOINT driver");
MODULE_SUPPORTED_DEVICE("IR SENSOR");
MODULE_ALIAS("finpoint");
MODULE_LICENSE("GPL");

#define DEBUG_MODE

#if defined(DEBUG_MODE)
#define dprintk printk
#else
#define dprintk(args...)
#endif

static unsigned long debugCounter = 0;

struct finpoint_data {
	struct i2c_client *i2c;
	struct input_dev* input_dev;	
	struct mutex finpoint_dd_lock;
	int init;
	int irq;	
	struct work_struct workqueue;	
#if SENSOR_BUTTON_EVENT
	int irqDome;
	struct work_struct workqueueDome;
#endif		
	wait_queue_head_t task_wait;
};

#define FINPOINT_DEVICE_NAME "finpoint"
#define FINPOINT_DOME_NAME "finpoint_dome"
#define FINPOINT_DEVICE_ID 0x83
#define IR_GUID   NV_ODM_GUID('i','r','s','e','n','s','o','r')

static ssize_t finpoint_show_attr(struct device *dev, struct device_attribute *attr, const char *buf);
static ssize_t finpoint_set_attr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static DEVICE_ATTR(parameter, S_IRUSR | S_IWUSR, finpoint_show_attr, finpoint_set_attr);

static void finpoint_work_f(struct work_struct *work);
static irqreturn_t finpoint_irq(int irq, void *dev_id);
static int finpoint_parse_data(struct finpoint_data* dd, u8* sensorData, int length);
#if SENSOR_BUTTON_EVENT
static void finpoint_work_f_dome(struct work_struct *work);
static irqreturn_t finpoint_irq_dome(int irq, void *dev_id);
#endif

static int __devinit finpoint_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static int __devexit finpoint_remove(struct i2c_client *client);

static int finpoint_init_client(struct i2c_client *client);
static int finpoint_init_sensor(struct i2c_client *client);
static int finpoint_power(int enable);

#if SENSOR_DATA_FILTER
static unsigned X_PIX_FILTER_VALUE = 25;
static unsigned X_SQUAL_FILTER_VALUE = 39;
static unsigned X_SHUTTER_UP_FILTER_VALUE = 200;
static unsigned X_SHUTTER_DW_FILTER_VALUE = 80;
static unsigned Y_PIX_FILTER_VALUE = 25;
static unsigned Y_SQUAL_FILTER_VALUE = 33;
static unsigned Y_SHUTTER_UP_FILTER_VALUE = 180;
static unsigned Y_SHUTTER_DW_FILTER_VALUE = 75;
#endif
static unsigned DELAY_FILTER_VALUE = 8;
#if 0
static int MAX_X_VALUE = 2;
static int MAX_Y_VALUE = 2;
#else
static int MAX_X_VALUE = 2;
static int MAX_Y_VALUE = 1;
#endif
static int debug_message_L1 = 0;
static int debug_message_L2 = 0;
static int debug_message_L3 = 0;

#if 0
static int speed_mode=3;
static int speed_mode_x=0;
static int speed_mode_y=0;
static int speed_mode_x_max=10;
static int speed_mode_y_max=16;
static int speed_mode_send=0;
#else
static int speed_mode=1;
static int speed_mode_x=0;
static int speed_mode_y=0;
static int speed_mode_x_max=20;
static int speed_mode_y_max=10;
static int speed_mode_send=0;
#endif

struct timespec time1;
struct timespec time2;
static int move_step=0;
static int move_step_old_x = 0;
static int move_step_old_y = 0;
static int move_step_x = 0;
static int move_step_y = 0;
static int move_step_sync;
static int move_step_change = 0;
static int move_step_min=3;
static int move_step_max=12;

// QCI Sam +++
NvOdmGpioPinHandle hDomePin = NULL;
NvOdmGpioPinHandle hNrstPin = NULL;
NvOdmGpioPinHandle hShtdwnPin = NULL;
NvOdmGpioPinHandle hMotionPin = NULL;
NvOdmServicesGpioHandle gGpio = NULL;

static int GPIO_IR_NRST = 156;
static int GPIO_IR_SHTDWN = 88;
static int GPIO_IR_DOME_SENSE = 89;
static int GPIO_IR_MOTION = 29;
// QCI Sam ---

//QCI Sam +++
static int finpoint_gpio_cfg()
{
    NvOdmPeripheralConnectivity *pConnectivity;
    NvU64 guid;

    // Get the peripheral connectivity information
    pConnectivity = NvOdmPeripheralGetGuid(NV_ODM_GUID('i','r','s','e','n','s','o','r'));
    if (pConnectivity == NULL)
        printk(KERN_ERR "[ finpoint ]========> pConnectivity == NULL!!\n");

    if (pConnectivity->Guid == IR_GUID)
    {
        // Getting the OdmGpio Handle
        gGpio = NvOdmGpioOpen();
        if (gGpio == NULL)
            printk(KERN_ERR "[ finpoint ] ========> Failed to open Nvodm GPIO handle\n");

        if (pConnectivity->AddressList[0].Interface == NvOdmIoModule_Gpio)
             // Acquiring Pin Handles for Dome Pin
             hDomePin= NvOdmGpioAcquirePinHandle(gGpio, pConnectivity->AddressList[0].Instance, pConnectivity->AddressList[0].Address);
        if (pConnectivity->AddressList[1].Interface == NvOdmIoModule_Gpio)
             // Acquiring Pin Handles for NRST Pin
             hNrstPin= NvOdmGpioAcquirePinHandle(gGpio, pConnectivity->AddressList[1].Instance, pConnectivity->AddressList[1].Address);
        if (pConnectivity->AddressList[2].Interface == NvOdmIoModule_Gpio)
             // Acquiring Pin Handles for SHTDWN Pin
             hShtdwnPin= NvOdmGpioAcquirePinHandle(gGpio, pConnectivity->AddressList[2].Instance, pConnectivity->AddressList[2].Address);
        if (pConnectivity->AddressList[3].Interface == NvOdmIoModule_Gpio)
             // Acquiring Pin Handles for MOTION Pin
             hMotionPin= NvOdmGpioAcquirePinHandle(gGpio, pConnectivity->AddressList[3].Instance, pConnectivity->AddressList[3].Address);

        if(hDomePin == NULL || hNrstPin == NULL || hShtdwnPin == NULL || hMotionPin == NULL)
                printk(KERN_ERR "[ finpoint ] ============> Failed to request GPIO!!!\n");

        NvOdmGpioConfig(gGpio, hNrstPin, NvOdmGpioPinMode_Output);
        NvOdmGpioConfig(gGpio, hShtdwnPin, NvOdmGpioPinMode_Output);

        NvOdmGpioSetState(gGpio, hNrstPin, 0x0);
        NvOdmGpioSetState(gGpio, hShtdwnPin, 0x0);
    }
        return 0;
}
//QCI Sam ---


static ssize_t finpoint_show_attr(struct device *dev, struct device_attribute *attr, const char *buf)
{					
#if SENSOR_DATA_FILTER		
	return sprintf(buf, "XS=%d, XP=%d, XH=%d, XL=%d\nYS=%d, YP=%d, YH=%d, YL=%d\nx=%d, y=%d, d=%d\nDML1=%d, DML2=%d, DML3=%d\nSpeedMode=%d, sx=%d, sy=%d, smin=%d, smax=%d\n", 
				X_SQUAL_FILTER_VALUE, 
				X_PIX_FILTER_VALUE, 
				X_SHUTTER_UP_FILTER_VALUE, 
				X_SHUTTER_DW_FILTER_VALUE,
				Y_SQUAL_FILTER_VALUE, 
				Y_PIX_FILTER_VALUE, 
				Y_SHUTTER_UP_FILTER_VALUE, 
				Y_SHUTTER_DW_FILTER_VALUE,
				MAX_X_VALUE, 
				MAX_Y_VALUE, 
				DELAY_FILTER_VALUE,
				debug_message_L1,
				debug_message_L2,
				debug_message_L3,
				speed_mode,
				speed_mode_x_max,
				speed_mode_y_max,
				move_step_min,
				move_step_max);
#else
	return sprintf(buf, "x=%d, y=%d, d=%d\nDML1=%d, DML2=%d\nSpeedMode=%d, sx=%d, sy=%d, smin=%d, smax=%d\n", 
				MAX_X_VALUE, 
				MAX_Y_VALUE, 
				DELAY_FILTER_VALUE,
				debug_message_L1,
				debug_message_L2,
				speed_mode,
				speed_mode_x_max,
				speed_mode_y_max,
				move_step_min,
				move_step_max);
#endif				
}

static ssize_t finpoint_set_attr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct finpoint_data *dd = i2c_get_clientdata(client);
	u8 rxBuf[2] = {0};
	u8 deviceIDAddr = 0x00;	
	//struct vreg *vreg; // Sam ---
        //int ret = 0;	// Sam ---
        int inputValue;
        u8  txBuf[2] = {0};
	
	if(count > 2)
	{  
	   if (strncmp(buf,"VER",3) == 0)
	   {
	   	i2c_master_send(client, &deviceIDAddr, 1);
		i2c_master_recv(client, rxBuf, 1);
        	printk(KERN_ERR "[finpoint_set_attr] device id = 0x%x\n",rxBuf[0]);
	   }
	   else if (strncmp(buf,"CHXY=",5) == 0)
	   {
	   	inputValue = simple_strtol(buf + 5, NULL, 10);
	   	rxBuf[0] = 0x77;
	   	rxBuf[1] = (u8)inputValue;
	   	if (i2c_master_send(client, &rxBuf[0], 2) == 2)
	   	{ 
	   	   printk(KERN_ERR "[finpoint_set_attr] write: addr[0x%x],data[0x%x] PASS\n",rxBuf[0],rxBuf[1]);	
	   	}
	   	else
	   	{
	   	   printk(KERN_ERR "[finpoint_set_attr] write: addr[0x%x],data[0x%x] FAIL\n",rxBuf[0],rxBuf[1]);	
	   	}
	   }
	   else if (strncmp(buf,"REG=",4) == 0)
	   {
	   	inputValue = simple_strtol(buf + 4, NULL, 10);
	   	rxBuf[0] = (u8)inputValue;
	   	rxBuf[1] = 0;
	   	if (i2c_smbus_read_i2c_block_data(client, rxBuf[0], 1, &rxBuf[1]) == 1)
	   	{ 
	   	   printk(KERN_ERR "[finpoint_set_attr] read: addr[0x%x],data[0x%x] PASS\n",rxBuf[0],rxBuf[1]);	
	   	}
	   	else
	   	{
	   	   printk(KERN_ERR "[finpoint_set_attr] read: addr[0x%x],data[0x%x] FAIL\n",rxBuf[0],rxBuf[1]);	
	   	}	   
	   }
	   else if (strncmp(buf,"INIT",4) == 0)
	   {	   	
	        if(finpoint_init_client(client))
	        {
	           printk("finpoint_init_client() failed\n");
	        }
	        else
	        {
	           printk("finpoint_init_client() pass\n");	
	           mutex_lock(&dd->finpoint_dd_lock);
	           dd->init = 1;
	           mutex_unlock(&dd->finpoint_dd_lock);	          	           	           	           
	        }	
	   }
	   else if (strncmp(buf,"PWON",4) == 0)
	   {	
                /*           
                printk("[qci_finpoint] POWER ON\n");
	        vreg= vreg_get(0, "rfrx2"); 
	        printk("[qci_finpoint] vreg_enable\n");       
	        ret = vreg_enable(vreg);        
	        if (ret) printk( KERN_ERR " ir-sensor: voltage enable failed\n");   
	        printk("[qci_finpoint] vreg_set_level\n");        
	        ret = vreg_set_level(vreg, 2800);       
	        if (ret) printk( KERN_ERR " ir-sersor: voltage level setup failed\n");
                */
	   }
	   else if (strncmp(buf,"PWOFF",5) == 0)
	   {
                /*
	   	printk("[qci_finpoint] POWER OFF\n");
	   	vreg = vreg_get(0, "rfrx2");  
	   	printk("[qci_finpoint] vreg_disable\n");       
		ret = vreg_disable(vreg);    
		if (ret) printk( KERN_ERR " ir-sensor: voltage disable failed\n");   
                */
	   }
	   else if (strncmp(buf,"GPIO_SHTDWN=",12) == 0)
	   {
	   	
	   	inputValue = simple_strtol(buf + 12, NULL, 10);
	   	
	   	if (inputValue == 0)
	   	{
	   	   printk("[qci_finpoint] GPIO_IR_SHTDWN=0\n");  
	   	   gpio_set_value(GPIO_IR_SHTDWN, 0);
	   	}
	   	else
	   	{
	   	   printk("[qci_finpoint] GPIO_IR_SHTDWN=1\n");  
	   	   gpio_set_value(GPIO_IR_SHTDWN, 1);	
	   	}	
	   }
	   else if (strncmp(buf,"GPIO_NRST=",10) == 0)
	   {	   
	   	inputValue = simple_strtol(buf + 10, NULL, 10);
	   	
	   	if (inputValue == 0)
	   	{
	   	   printk("[qci_finpoint] GPIO_IR_NRST=0\n");  
	   	   gpio_set_value(GPIO_IR_NRST, 0);
	   	}
	   	else
	   	{
	   	   printk("[qci_finpoint] GPIO_IR_NRST=1\n");  
	   	   gpio_set_value(GPIO_IR_NRST, 1);	
	   	}
	   	
	   }	   
	   else if (strncmp(buf,"IRQ_ON",6) == 0)
	   {
	   	enable_irq(dd->irq);	
	   }
	   else if (strncmp(buf,"IRQ_OFF",7) == 0)
	   {
	   	disable_irq(dd->irq);
	   }
	   else if (strncmp(buf,"DML1=",5) == 0)
           {
              debug_message_L1 = simple_strtol(buf + 5, NULL, 10);		
              if (debug_message_L1 < 0) debug_message_L1 = 0;
              if (debug_message_L1 > 1) debug_message_L1 = 1;	
	   }
	   else if (strncmp(buf,"DML2=",5) == 0)
	   {
	      debug_message_L2 = simple_strtol(buf + 5, NULL, 10);	
	      if (debug_message_L2 < 0) debug_message_L2 = 0;
              if (debug_message_L2 > 1) debug_message_L2 = 1;  
	   }
           else if (strncmp(buf,"DML3=",5) == 0)
	   {
	      debug_message_L3 = simple_strtol(buf + 5, NULL, 10);	
	      if (debug_message_L3 < 0) debug_message_L3 = 0;
              if (debug_message_L3 > 1) debug_message_L3 = 1;  
	   }
	   else if (strncmp(buf,"d=",2) == 0)
	      DELAY_FILTER_VALUE = simple_strtol(buf + 2, NULL, 10);   
	   else if (strncmp(buf,"x=",2) == 0)
	      MAX_X_VALUE = simple_strtol(buf + 2, NULL, 10);
	   else if (strncmp(buf,"y=",2) == 0)
	      MAX_Y_VALUE = simple_strtol(buf + 2, NULL, 10);      		
#if SENSOR_DATA_FILTER	
 	   else if (strncmp(buf,"XS=",3) == 0)
	      X_SQUAL_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10); 
	   else if (strncmp(buf,"XP=",3) == 0)
	      X_PIX_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10); 
	   else if (strncmp(buf,"XH=",3) == 0)
	      X_SHUTTER_UP_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10); 
	   else if (strncmp(buf,"XL=",3) == 0)
	      X_SHUTTER_DW_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10);  
	   else if (strncmp(buf,"YS=",3) == 0)
	      Y_SQUAL_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10); 
	   else if (strncmp(buf,"YP=",3) == 0)
	      Y_PIX_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10);   
	   else if (strncmp(buf,"YH=",3) == 0)
	      Y_SHUTTER_UP_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10);   
	   else if (strncmp(buf,"YL=",3) == 0)
	      Y_SHUTTER_DW_FILTER_VALUE = simple_strtol(buf + 3, NULL, 10);                
#endif	      
	   else if (strncmp(buf,"ym=",3) == 0)	     
	   {
	      input_report_rel(dd->input_dev, REL_Y, simple_strtol(buf + 3, NULL, 10));
	      input_sync(dd->input_dev);
	   }
	   else if (strncmp(buf,"xm=",3) == 0)	
	   {     
	      input_report_rel(dd->input_dev, REL_X, simple_strtol(buf + 3, NULL, 10));   
	      input_sync(dd->input_dev);
	   } 	   
	   else if (strncmp(buf,"sx=",3) == 0)	
	   {
	      speed_mode_x_max = simple_strtol(buf + 3, NULL, 10);
	   }
	   else if (strncmp(buf,"sy=",3) == 0)	
	   {
	      speed_mode_y_max = simple_strtol(buf + 3, NULL, 10);
	   } 
	   else if (strncmp(buf,"smin=",5) == 0)	
	   {
	      move_step_min = simple_strtol(buf + 5, NULL, 10);
	   }
	   else if (strncmp(buf,"smax=",5) == 0)	
	   {
	      move_step_max = simple_strtol(buf + 5, NULL, 10);
	   } 
	   else if (strncmp(buf,"SpeedMode=1",11) == 0)	
	   {
	      speed_mode = 1; 	  
	      MAX_X_VALUE = 2;
	      MAX_Y_VALUE = 2;
	      speed_mode_x = 0;
	      speed_mode_y = 0;
	      speed_mode_send=0;
	   } 
	   else if (strncmp(buf,"SpeedMode=2",11) == 0)	
	   {
	      speed_mode = 2; 
	      MAX_X_VALUE = 1;
	      MAX_Y_VALUE = 1;	  	
	   } 
	   else if (strncmp(buf,"SpeedMode=3",11) == 0)	
	   {
	      speed_mode = 3; 
	      MAX_X_VALUE = 2;
	      MAX_Y_VALUE = 2;	  	
	   } 
	   else if (strncmp(buf,"SpeedMode=4",11) == 0)	
	   {
	      speed_mode = 4; 
	      MAX_X_VALUE = 2;
	      MAX_Y_VALUE = 2;	  	
	   } 
	   else if (strncmp(buf,"SpeedMode=5",11) == 0)	
	   {
	      speed_mode = 5; 
	      MAX_X_VALUE = 2;
	      MAX_Y_VALUE = 2;	  	
	   } 
	}	

	return count;
}

#if SENSOR_BUTTON_EVENT
static void finpoint_work_f_dome(struct work_struct *work)
{
	if (debug_message_L1 == 1) printk("[finpoint_work_f_dome]+\n");

	struct finpoint_data*dd = container_of(work, struct finpoint_data, workqueueDome);
	int isPress = 0;	

	isPress = gpio_get_value(GPIO_IR_DOME_SENSE);

	mdelay(50);
	if (debug_message_L2 == 1) printk("[finpoint_work_f_dome] press: %d\n", !isPress);
	input_report_key(dd->input_dev, BTN_LEFT, isPress?0:1);
	input_report_key(dd->input_dev, BTN_MIDDLE, 0);
	input_report_key(dd->input_dev, BTN_RIGHT, 0);
	input_sync(dd->input_dev);
	//enable_irq(MSM_GPIO_TO_INT(GPIO_IR_DOME));
}
static irqreturn_t finpoint_irq_dome(int irq, void *dev_id)
{
	struct finpoint_data* dd = dev_id;

	//disable_irq(irq);
	if (debug_message_L1 == 1) dprintk("finpoint_irq_dome\n");
	schedule_work(&dd->workqueueDome);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t finpoint_irq(int irq, void *dev_id)
{
	struct finpoint_data *data = (struct finpoint_data *)dev_id;
	//disable_irq(irq);//Ryan +++
	schedule_work(&data->workqueue);
	return IRQ_HANDLED;
}

static void filter1(int *px, int *py)
{
    int x = *px;
    int y = *py;

    speed_mode_x = speed_mode_x + x;
    speed_mode_y = speed_mode_y + y;
    if (speed_mode_x >= speed_mode_x_max) {
        x = MAX_X_VALUE;
        speed_mode_x = 0;
    } else if (speed_mode_x <= -speed_mode_x_max) {
        x = -MAX_X_VALUE;
        speed_mode_x = 0;
    } else {
        x = 0;
    }

    if (speed_mode_y >= speed_mode_y_max) {
        y = MAX_Y_VALUE;
        speed_mode_y = 0;
    } else if (speed_mode_y <= -speed_mode_y_max) {
        y = -MAX_Y_VALUE;
        speed_mode_y = 0;
    }  else {
        y = 0;
    }

    *px = x;
    *py = y;
}

static void filter_data(int *px, int *py) {
    switch (speed_mode) {
    case 1:
        filter1(px, py);
        break;
    default:
        /* do nothing */
        break;
    }
}

static int finpoint_parse_data(struct finpoint_data* dd, u8* sensorData, int length)
{
	int x = 0;
	int y = 0;
	int x_source;
	int y_source;
	int xPosScal = 1;
	int xNegScal = 1;
	int yPosScal = 1;
	int yNegScal = 1;
	static int accX = 0;
	static int accY = 0;
	static MoveType lastMoveType = None;
	int pixMax = sensorData[PIX_MAX];
	int pixMin = sensorData[PIX_MIN];
	int pixSum = sensorData[PIX_SUM];
	int squal = sensorData[SQUAL];
	//int shutterUp = sensorData[SHT_UP];
	int shutterDown = sensorData[SHT_DW];
	char tmp[255];
	static long t1;
	static long t2;

	if (debug_message_L1 == 1) dprintk("[finpoint_parse_data]+\n");
	if (debug_message_L1 == 1) dprintk("motion: 0x%04x, x: %d, y: %d\n", sensorData[MOTION], sensorData[X], sensorData[Y]);
	//printk("motion: 0x%04x, x: %d, y: %d\n", sensorData[MOTION], sensorData[X], sensorData[Y]);//Ryan +++

	x = (sensorData[X] & 0x80)?(-1)* (128 + ~(sensorData[X] & 0x7f)):(sensorData[X]);
	y = (sensorData[Y] & 0x80)?(-1)* (128 + ~(sensorData[Y] & 0x7f)):(sensorData[Y]);

    x = 0 - x;
    y = 0 - y;

#if 1

    if(sensorData[MOTION] & 0x80)
    {
        u8 buf[] = {0x02, 0x01};
        filter_data(&x, &y);
        if (x != 0 || y != 0) {
            input_report_rel(dd->input_dev, REL_X, x);
            input_report_rel(dd->input_dev, REL_Y, y);
            input_sync(dd->input_dev);
        }
		i2c_master_send(dd->i2c, buf, 2);
    }

#else

	if(sensorData[MOTION] & 0x80)
	{
		if(length >= 3)
		{
			//static uint32_t jiffiesDiff = jiffies;

			if(x == 0 && y == 0)
			{
				//dprintk("[%d]0\n", debugCounter);
				return 1;
			}
			else
			{
				if(x > 0 && lastMoveType != XPos)
				{
					lastMoveType = XPos;

					return 1;
				}
				else if(x < 0 && lastMoveType != XNeg)
				{
					lastMoveType = XNeg;

					return 1;
				}
				else if(y > 0 && lastMoveType != YPos)
				{
					lastMoveType = YPos;

					return 1;
				}
				else if(y < 0 && lastMoveType != YNeg)
				{
					lastMoveType = YNeg;

					return 1;
				}

				debugCounter++;

				if(pixMax == pixMin)
				{
					// error data
					// clear data for motion pin
					u8 buf[] = {0x02, 0x01};

					i2c_master_send(dd->i2c, buf, 2);
					if (debug_message_L1 == 1) dprintk("[%ld]F\n", debugCounter);

					return 0; // change for hang of TCH
				}

#if SENSOR_DATA_FILTER
			if (speed_mode < 5)
			{	
				if(lastMoveType & 0x02)
				{
					if(shutterDown < Y_SHUTTER_DW_FILTER_VALUE || shutterDown > Y_SHUTTER_UP_FILTER_VALUE)
					{
						if (debug_message_L2 == 1) dprintk("[%ld][%d, %d]SHU: %d\n", debugCounter, x, y, shutterDown);

						return 1;
					}
					else if((pixSum - pixMin) < Y_PIX_FILTER_VALUE)
					{
						if (debug_message_L2 == 1) dprintk("[%ld][%d, %d]PIX: %d = %d-%d\n", debugCounter, x, y, (pixSum - pixMin), pixSum, pixMin);

						return 1;
					}
					/*else if(squal < Y_SQUAL_FILTER_VALUE)
					{
						dprintk("[%d][%d, %d]SQU: %d\n", debugCounter, x, y, squal);

						return 1;
					}*/
				}
				else
				{
					if(shutterDown < X_SHUTTER_DW_FILTER_VALUE || shutterDown > X_SHUTTER_UP_FILTER_VALUE)
					{
						if (debug_message_L2 == 1) dprintk("[%ld][%d, %d]SHU: %d\n", debugCounter, x, y, shutterDown);

						return 1;
					}
					else if((pixSum - pixMin) < X_PIX_FILTER_VALUE)
					{
						if (debug_message_L2 == 1) dprintk("[%ld][%d, %d]PIX: %d = %d-%d\n", debugCounter, x, y, (pixSum - pixMin), pixSum, pixMin);

						return 1;
					}
					/*else if(squal < X_SQUAL_FILTER_VALUE)
					{
						dprintk("[%d][%d, %d]SQU: %d\n", debugCounter, x, y, squal);

						return 1;
					}*/
				}
			}
#endif				

				x = (x > 0)?(x * xPosScal):(x * xNegScal);
				y = (y > 0)?(y * yPosScal):(y * yNegScal);
				x_source = x;
				y_source = y;

				if(x> MAX_X_VALUE)
				{
					x = MAX_X_VALUE;
				}
				if(x < -MAX_X_VALUE)
				{
					x = -MAX_X_VALUE;
				}
				if(y > MAX_Y_VALUE)
				{
					y = MAX_Y_VALUE;
				}
				if(y < -MAX_Y_VALUE)
				{
					y = -MAX_Y_VALUE;
				}		

				accX += x;
				accY += y;

				//dprintk("----[%d, %d]----\n", accX, accY);
				if (debug_message_L2 == 1) dprintk("[%ld][0x%02x][%d, %d]X=%d, Y=%d, SQ=%d, SHLW=%d, P=%d\n", 
								debugCounter,
								sensorData[MOTION], 
								x, 
								y, 
								x_source,
								y_source,
								squal, 
								shutterDown, 
								(pixSum - pixMin));
								
				if (speed_mode == 4 || speed_mode == 5)
				{
				   input_report_rel(dd->input_dev, REL_X, x);
				   input_report_rel(dd->input_dev, REL_Y, y);				   
				   input_sync(dd->input_dev);	
				}				
				else if (speed_mode == 3 || speed_mode == 2)
				{				
				   move_step_sync=1;							   
				   time2 = current_kernel_time();
				   sprintf(tmp,"%ld",time1);
				   t1 = simple_strtol(tmp,NULL,sizeof(tmp));
				   sprintf(tmp,"%ld",time2);
				   t2 = simple_strtol(tmp,NULL,sizeof(tmp));
				   
				   if (t2-t1 >= 1)
				   {
				      move_step = 0;
				      move_step_x = 0;
				      move_step_y = 0;
				      move_step_change=0;
				   }	
				   if (move_step != 0)
				   {			   
				      if (move_step_change == 0 && (x * move_step_old_x < 0 || y * move_step_old_y < 0))
				         move_step_change++;
				      if (move_step_change == 1 && (x * move_step_old_x >= 0 || y * move_step_old_y >= 0))
				         move_step_change++;
				      if (move_step_change == 2 && (x * move_step_old_x >= 0 || y * move_step_old_y >= 0))
				      {
				      	 move_step = 0;
				         move_step_x = 0;
				         move_step_y = 0;
				      }   
				      else   
				      	 move_step_change = 0;			         
				   }				  
				   if (move_step < 2)
				   {
				      move_step_x = move_step_x + x;
				      move_step_y = move_step_y + y;
				   }   
				   if (move_step == 0)
				   {				      
				      if (move_step_x > move_step_min || move_step_x < -move_step_min || move_step_y > move_step_min || move_step_y < -move_step_min)
				      {
				      	 move_step_sync = 0;
				      	 move_step = 1;
				      }	
				   }
				   else if (move_step == 1)
				   {				      
				      if (move_step_x > move_step_max || move_step_x < -move_step_max || move_step_y > move_step_max || move_step_y < -move_step_max)
				      	 move_step = 2; 
				      else
				         move_step_sync = 0;
				   }
				   
				   if (debug_message_L3 == 1) dprintk("------------------------------------------------\n");
				   if (debug_message_L3 == 1) dprintk("t2-t1=%ld, move_step=%d\nmove_step_x=%d, move_step_y=%d\nx[%d,%d], y=[%d,%d]\nmove_step_sync=%d\n\n",
				                                      t2-t1,
				                                      move_step,
				                                      move_step_x,
				                                      move_step_y,
				                                      move_step_old_x,
				                                      x,
				                                      move_step_old_y,
				                                      y,
				                                      move_step_sync);
				   
				   if (move_step_sync == 1)
				   {
				   input_report_rel(dd->input_dev, REL_X, x);
				   input_report_rel(dd->input_dev, REL_Y, y);
				   input_sync(dd->input_dev);
				}
				   time1=time2;
				   move_step_old_x = x;
				   move_step_old_y = y;
				}
				else if (speed_mode == 1)
				{
				   speed_mode_x = speed_mode_x + x;
				   speed_mode_y = speed_mode_y + y;
				   if (speed_mode_x >= speed_mode_x_max)
				   {
				      input_report_rel(dd->input_dev, REL_X, MAX_X_VALUE);	
				      speed_mode_x = 0;
				      speed_mode_send=1;
				   }
				   if (speed_mode_x <= (speed_mode_x_max * -1))
				   {
				      input_report_rel(dd->input_dev, REL_X, -MAX_X_VALUE);	
				      speed_mode_x = 0;
				      speed_mode_send=1;
				   }
				   if (speed_mode_y >= speed_mode_y_max)
				   {
				      input_report_rel(dd->input_dev, REL_Y, MAX_Y_VALUE);	
				      speed_mode_y = 0;
				      speed_mode_send=1;
				   } 
				   if (speed_mode_y <= (speed_mode_y_max * -1))
				   {
				      input_report_rel(dd->input_dev, REL_Y, -MAX_Y_VALUE);	
				      speed_mode_y = 0;
				      speed_mode_send=1;
				   } 
				   if (speed_mode_send == 1)
				   {
				      speed_mode_send = 0;
				      input_sync(dd->input_dev);	
				   }
				}
				mdelay(DELAY_FILTER_VALUE);
			}
		}

		return 1;
	}
	else
	{
		u8 buf[] = {0x02, 0x01};
		i2c_master_send(dd->i2c, buf, 2);
		return 0;
	}
#endif
	return 0;	
}

static void finpoint_work_f(struct work_struct *work)
{
	u8 dataAddr;
	int dataIndex;
	u8 sensorData[SENSOR_DATA_LEN] = {0};
	int rc;
	struct finpoint_data* dd = container_of(work, struct finpoint_data, workqueue);

	if (debug_message_L1 == 1) printk("[finpoint_work_f]+\n");		

#if 0
	if (dd->init == 0)
	{	   	
	   finpoint_power(0);
	   mdelay(200);   
	   finpoint_power(1);
	   rc = finpoint_init_client(dd->i2c);
	   if(rc)
	   {
	     printk("finpoint_init_client() failed\n");
	     goto ERROR2;
	   }
	   else
	   {
	      mutex_lock(&dd->finpoint_dd_lock);
	      dd->init = 1;
	      mutex_unlock(&dd->finpoint_dd_lock);		
	   }		
	}	
#endif	
	mutex_lock(&dd->finpoint_dd_lock);
	do
	{
		
		dataAddr = OFN_MOTION_REG | 0x80;
		dataIndex = 0;	        
		
		if ((rc = i2c_smbus_read_i2c_block_data(dd->i2c, dataAddr, SENSOR_DATA_LEN, &sensorData[0])) != SENSOR_DATA_LEN)
		{
		   if (debug_message_L1 == 1) 
		      printk("[do_finpoint_wq] i2c_smbus_read_i2c_block_data fail (%d)\n",rc);
		   dd->init = 0;   
		   goto ERROR1;
		}   
				
		if (debug_message_L1 == 1) 
		printk("[do_finpoint_wq]\n[MOTION]=0x%02x, [X]=0x%02x, [Y]=0x%02x, [SQUAL]=0x%02x\n[SHT_UP]=0x%02x, [SHT_DW]=0x%02x, [PIX_MAX]=0x%02x, [PIX_SUM]=0x%02x, [PIX_MIN]=0x%02x\n",
			sensorData[MOTION], sensorData[X],
			sensorData[Y], sensorData[SQUAL],
			sensorData[SHT_UP], sensorData[SHT_DW],
			sensorData[PIX_MAX], sensorData[PIX_SUM],
			sensorData[PIX_MIN]);
	}
	while(finpoint_parse_data(dd, sensorData, SENSOR_DATA_LEN));
ERROR1:
        mutex_unlock(&dd->finpoint_dd_lock);  
ERROR2:
	mutex_lock(&dd->finpoint_dd_lock);
	wake_up_interruptible_all(&dd->task_wait);
	mutex_unlock(&dd->finpoint_dd_lock);
}

static int finpoint_init_sensor(struct i2c_client *client)
{
	int ret;
	struct finpoint_data *data = i2c_get_clientdata(client);	
	
	u8 txBuf[] =
	{
		0x3A, 0x5A,
		0x60, 0x84,//0x94,enable mouse
		0x62, 0x12,
		0x63, 0x0E,
		0x77, 0x21,
	};
        int index;
	int i;
	int count = 0;

	if (data->input_dev == NULL)
	{
	   data->input_dev = input_allocate_device();
	   if(data->input_dev == NULL)
	   {
		ret = -ENOMEM;
		printk(KERN_ERR " [qci_finpoint] finpoint_init_sensor: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	   }
	   data->input_dev->name = "qci_finpoint";
	   data->input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	   data->input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);
	   data->input_dev->keybit[BIT_WORD(BTN_LEFT)] = BIT_MASK(BTN_LEFT) 
	                                              | BIT_MASK(BTN_MIDDLE)
				                      | BIT_MASK(BTN_RIGHT);

	   ret = input_register_device(data->input_dev);
	   if(ret)
	   {
		printk(KERN_ERR "[qci_finpoint] finpoint_init_sensor: Unable to register %s input device\n", data->input_dev->name);
		goto err_input_register_device_failed;
	   }
	}
	
	index=0;
	printk("[qci_finpoint] write address = 0x%02x , data = 0x%02x\n",txBuf[index],txBuf[index+1]);
	ret = i2c_master_send(client, &txBuf[index+=2], 2);
	mdelay(200);
	for (i=1;i<(sizeof(txBuf) / sizeof(txBuf[0]) / 2)&&ret==2;i++)
	{
	    printk("[qci_finpoint] write address = 0x%02x , data = 0x%02x\n",txBuf[index],txBuf[index+1]);
	    ret = i2c_master_send(client, &txBuf[index+=2], 2);	
	}	   
	
	if (ret!=2)
	   return ret;	

	return 0;
err_input_register_device_failed:
	input_free_device(data->input_dev);
err_input_dev_alloc_failed:
	kfree(data);
	printk("ret = %d\n",ret);
	return ret;

}


static int finpoint_init_client(struct i2c_client *client)
{
	u8 rxBuf[64] = {0};
	u8 deviceIDAddr = 0x00;	
	int confCount = 0;
	u8 readStartAddr;
	u8 confBuf[] =
	{
		0x60,	//0
		0x62,	//1
		0x63,
		0x64,
		0x65,
		0x66,
		0x67,
		0x68,
		0x69,
		0x6A,
		0x6B,
		0x6D,	//11	
		0x6E,	
		0x6F,
		0x70,
		0x71,		
		0x73,	//16
		0x74,
		0x75,
		0x77,	//19
	};	

	if (finpoint_init_sensor(client) != 0)
	   return -ENODEV;
	
	i2c_smbus_read_i2c_block_data(client, deviceIDAddr, 1, &rxBuf[0]);
	printk("[finpoint_init_panel]device id = 0x%x\n",rxBuf[0]);//Ryan +++
        printk(KERN_ERR "[finpoint_init_panel]device id = 0x%x\n",rxBuf[0]);
        
	if(rxBuf[0] == FINPOINT_DEVICE_ID)
	{
		i2c_smbus_read_i2c_block_data(client, confBuf[0], 1, &rxBuf[0]);
		
		readStartAddr = confBuf[1] | 0x80;
		i2c_smbus_read_i2c_block_data(client, readStartAddr, 10, &rxBuf[1]);
		
		readStartAddr = confBuf[11] | 0x80;
		i2c_smbus_read_i2c_block_data(client, readStartAddr, 5, &rxBuf[11]);
		
		readStartAddr = confBuf[16] | 0x80;
		i2c_smbus_read_i2c_block_data(client, readStartAddr, 3, &rxBuf[16]);
		
		i2c_smbus_read_i2c_block_data(client, confBuf[19], 1, &rxBuf[19]);
		
		for(confCount = 0; confCount < 20; confCount ++)
		{
		   printk("[0x%02x]=0x%02x\n",confBuf[confCount],rxBuf[confCount]);
		}
		
		return 0;
	}

	return -ENODEV;
}

static struct attribute *finpoint_attributes[] = {
	&dev_attr_parameter.attr,
	NULL
};

static const struct attribute_group finpoint_attr_group = {
	.attrs = finpoint_attributes,
};

static int __devinit finpoint_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct finpoint_data* data;
/*	
	printk("[qci_finpoint]__devinit finpoint_probe()\n");
	
	if (!i2c_check_functionality(adapter,
				     I2C_FUNC_SMBUS_I2C_BLOCK)) 
        {
	   dev_err(&client->dev, "i2c bus does not support the finpoint\n");
	   rc = -ENODEV;
	   goto ERROR0;
	}	

	if(NULL == strstr(adapter->name, "MSM I2C adapter"))//QCOM I2C hardware adapter QCOM I2C bit-banging adapter QCOM I2C chip-enable adapter
	{
    	   printk("adapter->name=%s\n", adapter->name);
	   rc = -ENODEV;
	   goto ERROR0;
	}		
*/

#if 0
    //for B stage issue of IR button and white panel, 
    NvOdmServicesGpioHandle hGpio = NULL;
    NvOdmGpioPinHandle hIDGpioPin = NULL;
    NvU32 states = 0;
    NvBool ifBstageBoard = NV_FALSE;
    NvRmDeviceHandle hNvRmHandle = NULL;
    NvU32 Count = 0, Offset = 0, Value = 0;
    NvOdmPinAttrib *pPinAttribTable = NULL;

    //short soultion in B stage, check id pin to sure be B stage, to set DTD pull high for IR button,
    hGpio = NvOdmGpioOpen();
    if( hGpio != NULL )
    {
        // GPIO_PBB4
        hIDGpioPin = NvOdmGpioAcquirePinHandle(hGpio, 27, 4);
        //read it status
        if( hIDGpioPin != NULL )
        {
            NvOdmGpioConfig( hGpio, hIDGpioPin, NvOdmGpioPinMode_InputData);
            NvOdmGpioGetState(hGpio,hIDGpioPin,&states);
        
            if( states )
            {
                ifBstageBoard = NV_TRUE;
            }
        }
        NvOdmGpioReleasePinHandle(hGpio, hIDGpioPin);       
        NvOdmGpioClose(hGpio);
    }
    
    if( ifBstageBoard )
    {
        NvRmOpen(&hNvRmHandle,0);
        if( hNvRmHandle != NULL )
        {
            Count = NvOdmQueryPinAttributes((const NvOdmPinAttrib **)&pPinAttribTable);
            for ( ; Count ; Count--, pPinAttribTable++)
            {
                if( pPinAttribTable->ConfigRegister == NvOdmPinRegister_Ap20_PullUpDown_A )
                {
                    Offset = NvOdmPinRegister_Ap20_PullUpDown_A & 0xFFFF;
                    Value = pPinAttribTable->Value;

                    //change DTD pull high for IR button
                    Value &= ~0x03000000;
                    Value |=  0x02000000;
                    NV_REGW(hNvRmHandle, NvRmModuleID_Misc, 0, Offset, Value);

                    break;
                }init_sen
            }

            NvRmClose(hNvRmHandle);
        }
    }
    //end 
#endif

    printk("XXXXXXXXXXXXXXX IR Probe XXXXXXXXXXXXXXX\n");

//QCI Ryan +++  // Sam
//	   finpoint_power(0);
//	   mdelay(200);   
//	   finpoint_power(1);
//QCI Ryan ---

    if(!(data = kzalloc(sizeof(struct finpoint_data), GFP_KERNEL)))
	{
    	   printk("kzalloc() failed\n");
	   rc = -ENOMEM;
	   goto ERROR0;
	}
    	
    init_waitqueue_head(&data->task_wait);
	INIT_WORK(&data->workqueue, (work_func_t)finpoint_work_f);
#if SENSOR_BUTTON_EVENT	
	INIT_WORK(&data->workqueueDome, (work_func_t)finpoint_work_f_dome);
#endif	
	
	data->i2c = client;
	i2c_set_clientdata(client, data);		
	
	mutex_init(&data->finpoint_dd_lock);
	
	data->input_dev = NULL;
	
	rc = device_create_file(&client->dev, &dev_attr_parameter);
	if (rc)
	{
           printk("device_create_file() failed(%d)\n",rc);
           goto ERROR1;		
	}
	mutex_lock(&data->finpoint_dd_lock);
	data->init = 0;
	mutex_unlock(&data->finpoint_dd_lock);

#if 0 //Ryan +++	
	rc = finpoint_init_client(client);
	if(rc)
	{
	  printk("finpoint_init_client() failed\n");
	  //goto ERROR1;
	}
	else
	{
	   mutex_lock(&data->finpoint_dd_lock);
	   data->init = 1;
	   mutex_unlock(&data->finpoint_dd_lock);	
	   printk("finpoint_init_client() success\n");//Ryan +++	
	}
#endif //Ryan ---
	gpio_request(GPIO_IR_MOTION, "qci_finpoint");
	data->irq = gpio_to_irq(GPIO_IR_MOTION);
	rc = request_irq(data->irq, &finpoint_irq, IRQF_TRIGGER_FALLING, FINPOINT_DEVICE_NAME, data);
	if (rc)
	{
           printk("request_irq() failed(%d)\n",rc);
           goto ERROR2;	
	}
//Ryan +++
        rc = finpoint_init_client(client);
	if(rc)
	{
	  printk("finpoint_init_client() failed\n");
	  //goto ERROR1;
	}
	else
	{
	   mutex_lock(&data->finpoint_dd_lock);
	   data->init = 1;
	   mutex_unlock(&data->finpoint_dd_lock);	
	   printk("finpoint_init_client() success\n");//Ryan +++	
	}
//Ryan ---
#if SENSOR_BUTTON_EVENT
	gpio_request(GPIO_IR_DOME_SENSE, "qci_finpoint_dome");
	data->irqDome = gpio_to_irq(GPIO_IR_DOME_SENSE);
	rc = request_irq(data->irqDome, &finpoint_irq_dome, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, FINPOINT_DOME_NAME, data);
	if (rc)
	{
           printk("request_irqDome() failed(%d)\n",rc);
           goto ERROR2;		
	}
#endif		
	time1 = current_kernel_time();
//QCI Ryan +++
        u8 buf[] = {0x02, 0x01};
	u8 dataAddr;
	int dataIndex;
	u8 sensorData[SENSOR_DATA_LEN] = {0};
	dataAddr = OFN_MOTION_REG | 0x80;  
	i2c_smbus_read_i2c_block_data(data->i2c, dataAddr, SENSOR_DATA_LEN, &sensorData[0]);       
	i2c_master_send(data->i2c, buf, 2);
//QCI Ryan ---
	return 0;
ERROR2:
	free_irq(data->irq, data);
#if SENSOR_BUTTON_EVENT
        free_irq(data->irqDome, data);
#endif  	

ERROR1:
        kfree(data);
        data = NULL;        
        gpio_free(GPIO_IR_MOTION); 
#if SENSOR_BUTTON_EVENT        
        gpio_to_irq(GPIO_IR_DOME_SENSE);  
#endif            

ERROR0:	
	return rc;
}

static int __devexit finpoint_remove(struct i2c_client *client)
{
	struct finpoint_data *data = i2c_get_clientdata(client);

	if(data->irq)
	{
	   free_irq(data->irq, data);
	   gpio_free(GPIO_IR_MOTION);
#if SENSOR_BUTTON_EVENT
	   free_irq(data->irqDome, data);
	   gpio_to_irq(GPIO_IR_DOME_SENSE); 
#endif	   
	}

	input_free_device(data->input_dev);
	input_unregister_device(data->input_dev);
	
	device_remove_file(&client->dev, &dev_attr_parameter);	

	kfree(data);
	
	return 0;
}

static int finpoint_power(int enable)
{	
	printk( KERN_ERR "[qci_finpoint] finpoint_power(%d)\n",enable);	
	if (enable == 0)
	{
           NvOdmGpioSetState(gGpio, hNrstPin, 0x0);
           NvOdmGpioSetState(gGpio, hShtdwnPin, 0x1);
	   return 0;	
	}
	else if (enable == 1)
	{
	   NvOdmGpioSetState(gGpio, hShtdwnPin, 0x1);
	   mdelay(200);
	   NvOdmGpioSetState(gGpio, hShtdwnPin, 0x0);
	   NvOdmGpioSetState(gGpio, hNrstPin, 0x0);
	   mdelay(200);
	   NvOdmGpioSetState(gGpio, hNrstPin, 0x1);
	   mdelay(200);	
	   return 0;	
	}
	else
	   return -1;
}

static int finpoint_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct finpoint_data *data = i2c_get_clientdata(client);
	
	printk( KERN_ERR ">>>>>>>>>>>>finpoint_suspend\n");	
	disable_irq(data->irq);
		
	finpoint_power(0);     
	
	mutex_lock(&data->finpoint_dd_lock);
	data->init = 0;
	mutex_unlock(&data->finpoint_dd_lock);
	
	return 0;
}

static int finpoint_resume(struct i2c_client *client)
{
	struct finpoint_data *data = i2c_get_clientdata(client);
	int ret;
	
	printk( KERN_ERR "<<<<<<<<<<<finpoint_resume\n");
			     
	finpoint_power(1);
	ret = finpoint_init_sensor(client);
	if (ret == 0)
	{	  
	   mutex_lock(&data->finpoint_dd_lock);
	   data->init = 1;
	   mutex_unlock(&data->finpoint_dd_lock);

//QCI Ryan +++
        u8 buf[] = {0x02, 0x01};
	u8 dataAddr;
	int dataIndex;
	u8 sensorData[SENSOR_DATA_LEN] = {0};
	dataAddr = OFN_MOTION_REG | 0x80;  
	i2c_smbus_read_i2c_block_data(data->i2c, dataAddr, SENSOR_DATA_LEN, &sensorData[0]);       
	i2c_master_send(data->i2c, buf, 2);
//QCI Ryan ---

	}
	else   
	   printk("[finpoint_resume] finpoint_init_sensor fail!!"); 	
					         	
	enable_irq(data->irq);
		
	return ret;
}

static const struct i2c_device_id finpoint_id[] = {
	{ "finpoint", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, finpoint_id);

static struct i2c_driver finpoint_driver = {
	.driver = {
		.name = "finpoint",
	},
	.probe = finpoint_probe,
	.remove = finpoint_remove,
	.suspend = finpoint_suspend,
	.resume	= finpoint_resume,
	.id_table = finpoint_id,
};

static int __init finpoint_init(void)
{
        printk("[qci_finpoint]finpoint_init()\n");
        finpoint_gpio_cfg();
        finpoint_power(1);
	
	return i2c_add_driver(&finpoint_driver);
}

static void __exit finpoint_exit(void)
{
	finpoint_power(0);
	
	i2c_del_driver(&finpoint_driver);
}

module_init(finpoint_init);
module_exit(finpoint_exit);

