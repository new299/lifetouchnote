/*
 * drivers/input/misc/nvec-btns.c
 *
 * Input driver for buttons and switches connected to an NvEc compliant
 * embedded controller
 *
 * Copyright (c) 2009, NVIDIA Corporation.
 * Copyright (C) 2011 NEC Corporation
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

#include <linux/module.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

#include "nvos.h"
#include "nvec.h"
#include "nvodm_services.h"
#include "nvodm_button.h"
#include "nvec_device.h"
#undef CONFIG_HAS_EARLYSUSPEND
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define DRIVER_DESC "NvEc button driver"
#define DRIVER_LICENSE "GPL"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define TEGRA_GPIO_PA0	(('a'-'a')*8 + 0)
#define EC_INT_2_HOST	TEGRA_GPIO_PA0
#define OPEN	0
#define CLOSE	1
struct nvec_button
{
	struct device			*dev;
	struct input_dev		*input_dev;
	struct task_struct		*task;
	char				name[128];
	int				powerdown;
	NvEcHandle			hNvec;
//	NvEcEventRegistrationHandle	hEvent;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
int InEarlySuspend = 0;
#endif

static int nvec_button_recv(void *arg)
{
	struct input_dev *input_dev = (struct input_dev *)arg;
	struct nvec_button *btn = input_get_drvdata(input_dev);
	unsigned int PinNum = 0;
	NvU8 PinState = 0;

	while (!btn->powerdown) {
		NvOdmButtonGetData(&PinNum, &PinState, 0);

		dev_info(btn->dev, "got pin%d level %s\n", PinNum, (PinState & 0x1) ? "high" : "low");
		/* pin state high = lid open */
		if (PinState & 0x1)
			PinState = OPEN;
		else
			PinState = CLOSE;
		dev_info(btn->dev, "LID %s\n", (PinState & 0x1) ? "CLOSE" : "OPEN");
#ifdef CONFIG_HAS_EARLYSUSPEND
	       //printk("*********** 1. %s, %d **********",__FUNCTION__, InEarlySuspend);
		if( InEarlySuspend && PinState == CLOSE) continue;
#endif
 
		input_report_switch(input_dev, SW_LID, PinState);
		input_sync(input_dev);
		
	}

	return 0;
}

static int nvec_button_open(struct input_dev *idev)
{
	struct nvec_button *btn = input_get_drvdata(idev);

	dev_dbg(btn->dev, "input device file is opened.\n");
	if (NvOdmButtonEnableInterrupt() == NV_TRUE)
		return 0;
	else {
		dev_err(btn->dev, "%s() failed to enable interrupt.\n", __func__);
		return -1;
	}
}

static void nvec_button_close(struct input_dev *idev)
{
	struct nvec_button *btn = input_get_drvdata(idev);

	dev_dbg(btn->dev, "device file is closed.\n");
	NvOdmButtonDisableInterrupt();
	return;
}

static irqreturn_t nvec_button_irq(int irq, void *priv)
{
	struct device *dev = (struct device *)priv;
	dev_dbg(dev, "got wakeup interrupt\n");
	return IRQ_HANDLED;
}

static int __devinit nvec_button_probe(struct nvec_device *pdev)
{
	struct nvec_button *btn;
	struct input_dev *input_dev;
	int error;
	NvError nverr;

	btn = kzalloc(sizeof(struct nvec_button), GFP_KERNEL);
	if (IS_ERR(btn)) {
		dev_err(&pdev->dev, "allocate memory fail!\n");
		error = -ENOMEM;
		goto fail_mem;
	}
	btn->dev = &pdev->dev;
	input_dev = input_allocate_device();
	if (IS_ERR(btn) || IS_ERR(input_dev)) {
		error = -ENOMEM;
		goto fail;
	}

	btn->input_dev = input_dev;
	input_set_drvdata(input_dev, btn);
	nvec_set_drvdata(pdev, input_dev);

	if (!NvOdmButtonInit()) {
		error = -ENODEV;
		dev_err(btn->dev, "NvOdmButtonInit failed.\n");
		goto fail_button_init;
	}

	btn->task = kthread_create(nvec_button_recv, input_dev,
		"nvec_button_thread");
	if (IS_ERR(btn->task)) {
		error = PTR_ERR(btn->task);
		goto fail_thread_create;
	}
	wake_up_process(btn->task);

	if (!strlen(btn->name))
		snprintf(btn->name, sizeof(btn->name),
			 "nvec button");

	input_dev->name = btn->name;
	input_dev->open = nvec_button_open;
	input_dev->close = nvec_button_close;
	
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_POWER, input_dev->keybit);

	set_bit(EV_SW, input_dev->evbit);
	set_bit(SW_LID, input_dev->swbit);


	/* get EC handle */
	nverr = NvEcOpen(&btn->hNvec, 0 /* instance */);
	if (nverr != NvError_Success) {
		error = -ENODEV;
		goto fail_input_register;
	}

	error = input_register_device(btn->input_dev);
	if (error)
		goto fail_input_register;

	
	gpio_request(EC_INT_2_HOST, "ec_int_2_host");
	gpio_direction_input(EC_INT_2_HOST);
	error = request_irq(gpio_to_irq(EC_INT_2_HOST), 
			nvec_button_irq, 
			IRQF_TRIGGER_FALLING | IRQF_SHARED, 
			"nvec_btn", btn->dev);
	if (error) {
		dev_err(btn->dev, "failed to request_irq, returns error code = %d\n", error);
	}
	return 0;

fail_input_register:
	(void)kthread_stop(btn->task);
fail_thread_create:
	NvOdmButtonDeInit();
fail_button_init:
fail:
	NvEcClose(btn->hNvec);
	btn->hNvec = NULL;
	input_free_device(input_dev);
	kfree(btn);
fail_mem:
	return error;
}

static void nvec_button_remove(struct nvec_device *dev)
{
	struct input_dev *input_dev = nvec_get_drvdata(dev);
	struct nvec_button *btn = input_get_drvdata(input_dev);

	(void)kthread_stop(btn->task);
	NvOdmButtonDeInit();
	NvEcClose(btn->hNvec);
	btn->hNvec = NULL;
	btn->powerdown = 1;
	input_free_device(input_dev);
	gpio_free(EC_INT_2_HOST);
	kfree(btn);
}

static int nvec_button_suspend(struct nvec_device *pdev, pm_message_t state)
{
	struct input_dev *input_dev = nvec_get_drvdata(pdev);
	struct nvec_button *btn = input_get_drvdata(input_dev);

	if (!btn) {
		pr_err("%s: device handle is NULL\n", __func__);
		return -1;
	}
	return 0;
}

static int nvec_button_resume(struct nvec_device *pdev)
{
	struct input_dev *input_dev = nvec_get_drvdata(pdev);
	struct nvec_button *btn = input_get_drvdata(input_dev);

	if (!btn) {
		pr_err("%s: device handle is NULL\n", __func__);
		return -1;
	}
	return 0;
}

static struct nvec_driver nvec_button_driver = {
	.name		= "nvec_button",
	.probe		= nvec_button_probe,
	.remove		= nvec_button_remove,
	.suspend	= nvec_button_suspend,
	.resume		= nvec_button_resume,
};

static struct nvec_device nvec_button_device = {
	.name	= "nvec_button",
	.driver	= &nvec_button_driver,
};
#ifdef CONFIG_HAS_EARLYSUSPEND
static void suspend_to_display_off(struct early_suspend *h)
{
	        //printk("************** %s , %d************\n",__FUNCTION__,InEarlySuspend);
		        InEarlySuspend = 1;
}

static void suspend_to_display_on(struct early_suspend *h)
{
	        //printk("************** %s , %d************\n",__FUNCTION__,InEarlySuspend);
		        InEarlySuspend = 0;
}

static struct early_suspend display_power_state =
{
	        .suspend = suspend_to_display_off,
	        .resume = suspend_to_display_on,
	        .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN
};
#endif

static int __init nvec_button_init(void)
{
	int err;

	err = nvec_register_driver(&nvec_button_driver);
	if (err)
	{
		pr_err("**%s(): nvec_register_driver failed with err %d\n", __func__, err);
		return err;
	}

	err = nvec_register_device(&nvec_button_device);
	if (err)
	{
		pr_err("**%s(): nvec_register_device: failed with err %d\n", __func__, err);
		nvec_unregister_driver(&nvec_button_driver);
		return err;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&display_power_state);	
#endif	
	return 0;
}

static void __exit nvec_button_exit(void)
{
	nvec_unregister_device(&nvec_button_device);
	nvec_unregister_driver(&nvec_button_driver);
}

module_init(nvec_button_init);
module_exit(nvec_button_exit);

