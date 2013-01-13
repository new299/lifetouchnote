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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>   
#include <linux/sched.h>

#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#include "nvodm_services.h"
#include "nvrm_pinmux.h"
#include "nvrm_gpio.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"


#include <linux/kthread.h>

#define NV_DEBUG 0


static struct class *recovery_class;
struct device		*recovery_dev;


static struct task_struct *g_hRecoveryThread;
static int gRecoveryModeOn = 0;
static NvBool gRecoveryThreadOn = NV_FALSE;


static int RecoveryThreadFun(void* arg)
{
    //int larg = (int*)arg;
	int fd = -1;
	char cmdRecovery[] = {"boot-recovery"};
	int ret = 0;
	int pos = 0;
	char command[32];
	//mm_segment_t old_fs;
    
    while( gRecoveryThreadOn == NV_TRUE )
    {
		msleep_interruptible(1000);

		//printk("recovery =====>RecoveryThreadFun\n");

		if( gRecoveryModeOn )
		{	
			printk("recovery =====>enter recovery mode\n");

			//old_fs = get_fs();
			//set_fs(KERNEL_DS);
			fd = sys_open( "/dev/block/mmcblk3p3", O_RDWR, S_IRWXU);
			if( fd >= 0 )
			{
				memset( command , 0 , sizeof(command) );
				memcpy( command , cmdRecovery , sizeof(cmdRecovery) );

				pos = sys_lseek(fd, 2048, SEEK_SET );		
				ret = sys_write(fd, command, sizeof(command) );

				if(ret <= 0) 
				{	        	                
					printk("recovery mode======> msc write error\n");
				}
				else
					printk("recovery mode======> msc write ok\n");			
			
				sys_sync();
				sys_close(fd);			
			}
			else
			{
				printk("recovery mode======> msc file open error\n");
			}
			//set_fs(old_fs);

			gRecoveryModeOn = 0;
		} //if( gRecoveryModeOn )
	} //while( gRecoveryThreadOn == NV_TRUE )

	return 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
static ssize_t command_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	
	//NvOsDebugPrintf("nvecUpdate ========> nvecUpdateAttr_store %d, %s\n",size, buf);

	if( buf != NULL )
	{
		if( buf[0] == '@' )
		{
			gRecoveryModeOn = 1;
		}
	}

	return 1;
}
static ssize_t command_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "NULL\n");
}

static DEVICE_ATTR(command, 0644, command_show, command_store);

static int recovery_open(struct inode *inode, struct file *file)
{
	return 0;
}
static int recovery_close(struct inode * inode, struct file * file)
{
	return 0;
}
static int recovery_ioctl(struct inode * inode, struct file * file, unsigned int cmd, unsigned long arg)
{
	
	//copy_from_user(&brightness, (int*)arg, sizeof(int));		
	switch (cmd) 
	{
	default:
		break;
        }	
	
	return 0;
}

static ssize_t recovery_read(struct file *file, char *buf, size_t size, loff_t *loff)
{
    return 0;
};
static ssize_t recovery_write(struct file *file, const char *buf, size_t size, loff_t *loff)
{
    return 0;
};

struct file_operations recovery_fops = {
    	.owner	    = THIS_MODULE,
        .ioctl      = recovery_ioctl,
        .open       = recovery_open,
		.read       = recovery_read,
		.write      = recovery_write,
        .release    = recovery_close
};

static int recovery_probe(struct platform_device *pdev)
{
	int major = 0;
	int minor = 0;
	int ret;

	//create a char dev for application code access 	
	ret = register_chrdev(0, "recovery", &recovery_fops);
	if( ret > 0 )
	{
		major = ret;
	}


	// create a class connect to this module
	recovery_class = class_create(THIS_MODULE, "recovery");	
	
	// register a dev name in under "/dev" , for user to use
	recovery_dev = device_create(recovery_class, NULL,			   
			    MKDEV(major,minor),NULL,
			    "tool");


	device_create_file(recovery_dev, &dev_attr_command);

	gRecoveryThreadOn = NV_TRUE;
    g_hRecoveryThread = kthread_run(RecoveryThreadFun,&gRecoveryModeOn,"Recovery thread");
	wake_up_process(g_hRecoveryThread);

	return 0;
}

static int __devexit recovery_remove(struct platform_device *pdev)
{
	printk("recovery =====>recovery_remove\n");

	//kthread_stop(g_hRecoveryThread);
    gRecoveryThreadOn = NV_FALSE;

	device_remove_file(recovery_dev, &dev_attr_command);

	device_unregister(recovery_dev);

	class_destroy(recovery_class);

	return 0;
}



void recovery_shutdown(struct platform_device *pdev)
{
	printk("recovery =====>recovery_shutdown\n");

	//kthread_stop(g_hRecoveryThread);
    gRecoveryThreadOn = NV_FALSE;

	device_remove_file(recovery_dev, &dev_attr_command);

	device_unregister(recovery_dev);

	class_destroy(recovery_class);

}


static struct platform_device recovery_device = 
{
    .name = "recovery",
    .id   = -1,
};

static struct platform_driver recovery_driver = {
	.probe		= recovery_probe,
	.remove		= __devexit_p(recovery_remove),
	.shutdown	= recovery_shutdown,
	.driver		= {
		.name	= "recovery",
		.owner	= THIS_MODULE,
	},
};


static int __init qci_recovery_init(void)
{
	platform_device_register(&recovery_device);
	return platform_driver_register(&recovery_driver);
}

static void __exit qci_recovery_exit(void)
{
	platform_driver_unregister(&recovery_driver);
}

module_init(qci_recovery_init);
module_exit(qci_recovery_exit);

MODULE_AUTHOR("Quanta");
MODULE_DESCRIPTION("QCI recovery driver");
MODULE_LICENSE("GPL");

