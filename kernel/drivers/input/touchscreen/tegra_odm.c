/*
 * drivers/input/touchscreen/tegra_odm.c
 *
 * Touchscreen class input driver for platforms using NVIDIA's Tegra ODM kit
 * driver interface
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

#define NV_DEBUG 0

#include <linux/module.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/freezer.h>

#include <nvodm_services.h>
#include <nvodm_touch.h>

#define TOOL_PRESSURE	100

#define TOOL_WIDTH_MAX		8

/* Ratio TOOL_WIDTH_FINGER / TOOL_WIDTH_MAX must be < 0.6f to be considered
 * "finger" press.
 */
#define TOOL_WIDTH_FINGER	1

/* Ratio TOOL_WIDTH_CHEEK / TOOL_WIDTH_MAX must be > 0.6f to be considered
 * "cheek" press.  (Cheek presses are ignored in phone mode, under assumption
 * that user accidentally pressed cheek against touch screen.)
 */

/* the kernel supports 5 fingers only as of now */
#define MAX_FINGERS	5

struct tegra_touch_driver_data
{
	struct input_dev	*input_dev;
	struct task_struct	*task;
	NvOdmOsSemaphoreHandle	semaphore;
	NvOdmTouchDeviceHandle	hTouchDevice;
	NvBool			bPollingMode;
	NvU32			pollingIntervalMS;
	NvOdmTouchCapabilities	caps;
	NvU32			MaxX;
	NvU32			MinX;
	NvU32			MaxY;
	NvU32			MinY;
	int			shutdown;
	struct early_suspend	early_suspend;
	NvBool bIsSuspended;
};

#define NVODM_TOUCH_NAME "nvodm_touch"

#define swapv(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)

#ifdef CONFIG_HAS_EARLYSUSPEND
static void tegra_touch_early_suspend(struct early_suspend *es)
{
	struct tegra_touch_driver_data *touch;
	touch = container_of(es, struct tegra_touch_driver_data, early_suspend);
	if (touch && touch->hTouchDevice) {
		if (!touch->bIsSuspended) {
			NvOdmTouchPowerOnOff(touch->hTouchDevice, NV_FALSE);
			touch->bIsSuspended = NV_TRUE;
		}
	}
	else {
		pr_err("tegra_touch_early_suspend: NULL handles passed\n");
	}
}

static void tegra_touch_late_resume(struct early_suspend *es)
{
	struct tegra_touch_driver_data *touch;
	touch = container_of(es, struct tegra_touch_driver_data, early_suspend);
	if (touch && touch->hTouchDevice) {
		if (touch->bIsSuspended) {
			NvOdmTouchPowerOnOff(touch->hTouchDevice, NV_TRUE);
			touch->bIsSuspended = NV_FALSE;
		}
	}
	else {
		pr_err("tegra_touch_late_resume: NULL handles passed\n");
	}
}

#endif

static int tegra_touch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_touch_driver_data *touch = platform_get_drvdata(pdev);
	if (touch && touch->hTouchDevice) {
		if (!touch->bIsSuspended) {
			NvOdmTouchPowerOnOff(touch->hTouchDevice, NV_FALSE);
			touch->bIsSuspended = NV_TRUE;
			return 0;
		}
		else {
			// device is already suspended
			return 0;
		}
	}
	pr_err("tegra_touch_suspend: NULL handles passed\n");
	return -1;
}

static int tegra_touch_resume(struct platform_device *pdev)
{
	struct tegra_touch_driver_data *touch = platform_get_drvdata(pdev);
	if (touch && touch->hTouchDevice) {
		if (touch->bIsSuspended) {
			NvOdmTouchPowerOnOff(touch->hTouchDevice, NV_TRUE);
			touch->bIsSuspended = NV_FALSE;
			return 0;
		}
		else {
			return 0;
		}
	}
	pr_err("tegra_touch_resume: NULL handles passed\n");
	return -1;
}

static int tegra_touch_thread(void *pdata)
{
	struct tegra_touch_driver_data *touch =
		(struct tegra_touch_driver_data*)pdata;
	NvOdmTouchCoordinateInfo c = {0};
	NvU32 x, y;
	NvBool bKeepReadingSamples, bValidFlag = NV_FALSE;


	for (;;) {
		if (touch->bPollingMode)
			msleep(touch->pollingIntervalMS); 
		else
			NvOdmOsSemaphoreWait(touch->semaphore);

		bKeepReadingSamples = NV_TRUE;
		while (bKeepReadingSamples)
			{
			if (!NvOdmTouchReadCoordinate(touch->hTouchDevice, &c)){
				pr_err("Couldn't read touch sample\n");
				bKeepReadingSamples = NV_FALSE;
				continue;
			}

			if (c.fingerstate & NvOdmTouchSampleIgnore)
				goto DoneWithSample;

            x = c.xcoord;
            y = c.ycoord;

           /* transformation from touch to screen orientation */
            if (touch->caps.Orientation & NvOdmTouchOrientation_V_FLIP)
            {
                y = touch->caps.YMaxPosition + touch->caps.YMinPosition - y;
            }
            if (touch->caps.Orientation & NvOdmTouchOrientation_H_FLIP)
            {
                x = touch->caps.XMaxPosition + touch->caps.XMinPosition - x;
            }

            if (touch->caps.Orientation & NvOdmTouchOrientation_XY_SWAP)
            {
                swapv(x,y);
            }
             //printk(NVODM_TOUCH_NAME "minX=%d, maxX=%d, minY=%d, maxY=%d\n", touch->caps.XMinPosition, touch->caps.XMaxPosition, touch->caps.YMinPosition, touch->caps.YMaxPosition );

            if (c.fingerstate & NvOdmTouchSampleValidFlag)
            {
                input_report_abs(touch->input_dev, ABS_X, x);
                input_report_abs(touch->input_dev, ABS_Y, y);
                bValidFlag = NV_TRUE;
            }

            /*
            if( c.fingerstate & NvOdmTouchSampleDownFlag )
                printk("touch================>BTN down , x=%d , y=%d\n",x, y);
            else
                printk("touch================>BTN up , x=%d , y=%d\n",x, y);
            */

            /* Report down or up flag */
            input_report_key(touch->input_dev, BTN_TOUCH,
                (c.fingerstate & NvOdmTouchSampleDownFlag) ? 1 :0);
            input_sync(touch->input_dev);
            NvOdmOsSleepMS(1);

DoneWithSample:

            bKeepReadingSamples = NV_FALSE;

            if (!touch->bPollingMode && 
                !NvOdmTouchHandleInterrupt(touch->hTouchDevice))
            {
                /* Some more data to read keep going */
                bKeepReadingSamples = NV_TRUE;
            }
            else if (bValidFlag)
            {
                //printk("touch================>BTN up\n");
                input_report_key(touch->input_dev, BTN_TOUCH, 0);
                input_sync(touch->input_dev);
                NvOdmOsSleepMS(1);
            }

		}
	}

	return 0;
}

static int __init tegra_touch_probe(struct platform_device *pdev)
{
	struct tegra_touch_driver_data *touch = NULL;
	struct input_dev *input_dev = NULL;
	int err, i = 0, offset = 0;
	NvOdmTouchCapabilities *caps;

	touch = kzalloc(sizeof(struct tegra_touch_driver_data), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (input_dev == NULL || touch == NULL) {
		input_free_device(input_dev);
		kfree(touch);
		err = -ENOMEM;
		pr_err("tegra_touch_probe: Failed to allocate input device\n");
		return err;
	}
	touch->semaphore = NvOdmOsSemaphoreCreate(0);
	if (!touch->semaphore) {
		err = -1;
		pr_err("tegra_touch_probe: Semaphore creation failed\n");
		goto err_semaphore_create_failed;
	}

	if (!NvOdmTouchDeviceOpen(&touch->hTouchDevice)) {
		err = -1;
		pr_err("tegra_touch_probe: NvOdmTouchDeviceOpen failed\n");
		goto err_open_failed;
	}
	touch->bPollingMode = NV_FALSE;
	if (!NvOdmTouchEnableInterrupt(touch->hTouchDevice, touch->semaphore)) {
		err = -1;
		pr_err("tegra_touch_probe: Interrupt failed, polling mode\n");
		touch->bPollingMode = NV_TRUE;
		touch->pollingIntervalMS = 10;
	}

	touch->task =
		kthread_create(tegra_touch_thread, touch, "tegra_touch_thread");

	if(touch->task == NULL) {
		err = -1;
		goto err_kthread_create_failed;
	}
	wake_up_process( touch->task );

	touch->input_dev = input_dev;
	touch->input_dev->name = NVODM_TOUCH_NAME;

	/* get hardware capabilities */
	NvOdmTouchDeviceGetCapabilities(touch->hTouchDevice, &touch->caps);
	caps = &touch->caps;

	/* Will generate sync at the end of all input */
	set_bit(EV_SYN, touch->input_dev->evbit);
	/* Event is key input type */
	set_bit(EV_KEY, touch->input_dev->evbit);
	/* Input values are in absoulte values */
	set_bit(EV_ABS, touch->input_dev->evbit);
	/* supported virtual keys */
	set_bit(BTN_TOUCH, touch->input_dev->keybit);
	for (i = 0; i < (caps->MaxNumberOfFingerCoordReported - 1); i++) {
		set_bit(BTN_2 + i, touch->input_dev->keybit);
	}

	//add by 
	if( caps->IsMultiTouchSupported )
	{
		/* expose multi-touch capabilities */
		set_bit(ABS_MT_TOUCH_MAJOR, touch->input_dev->keybit);
		set_bit(ABS_MT_POSITION_X, touch->input_dev->keybit);
		set_bit(ABS_MT_POSITION_Y, touch->input_dev->keybit);
		set_bit(ABS_X, touch->input_dev->keybit);
		set_bit(ABS_Y, touch->input_dev->keybit);
	}
	touch->bIsSuspended = NV_FALSE;

	if (caps->Orientation & NvOdmTouchOrientation_XY_SWAP) {
		touch->MaxY = caps->XMaxPosition;
		touch->MinY = caps->XMinPosition;
		touch->MaxX = caps->YMaxPosition;
		touch->MinX = caps->YMinPosition;

	} else {
		touch->MaxX = caps->XMaxPosition;
		touch->MinX = caps->XMinPosition;
		touch->MaxY = caps->YMaxPosition;
		touch->MinY = caps->YMinPosition;
	}

	input_set_abs_params(touch->input_dev, ABS_X, touch->MinX,
		touch->MaxX, 0, 0);
	input_set_abs_params(touch->input_dev, ABS_Y, touch->MinY,
		touch->MaxY, 0, 0);
	for (i = 0; i < caps->MaxNumberOfFingerCoordReported; i++) {
		input_set_abs_params(touch->input_dev, ABS_HAT0X + offset, touch->MinX,
			touch->MaxX, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_HAT0Y + offset, touch->MinY,
			touch->MaxY, 0, 0);
		offset += 2;
	}

	//add by 
	if( caps->IsMultiTouchSupported )
	{
		input_set_abs_params(touch->input_dev, ABS_MT_POSITION_X,
			touch->MinX, touch->MaxX, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_MT_POSITION_Y,
			touch->MinY, touch->MaxY, 0, 0);
	}
	//end 

	if (caps->IsPressureSupported) {
		input_set_abs_params(touch->input_dev, ABS_MT_TOUCH_MAJOR,
			0, caps->MaxNumberOfPressureReported, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_PRESSURE, 0,
			caps->MaxNumberOfPressureReported, 0, 0);
	}
	else {
		input_set_abs_params(touch->input_dev, ABS_MT_TOUCH_MAJOR,
			0, TOOL_PRESSURE, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_PRESSURE, 0,
			TOOL_PRESSURE, 0, 0);
	}
	if (caps->IsWidthSupported) {
		input_set_abs_params(touch->input_dev, ABS_TOOL_WIDTH, 0,
			caps->MaxNumberOfWidthReported, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_MT_WIDTH_MAJOR, 0,
			caps->MaxNumberOfWidthReported, 0, 0);
	}
	else {
		input_set_abs_params(touch->input_dev, ABS_TOOL_WIDTH, 0,
			TOOL_WIDTH_MAX, 0, 0);
		input_set_abs_params(touch->input_dev, ABS_MT_WIDTH_MAJOR, 0,
			TOOL_WIDTH_MAX, 0, 0);
	}

	platform_set_drvdata(pdev, touch);

	err = input_register_device(touch->input_dev);
	if (err)
	{
		pr_err("tegra_touch_probe: Unable to register input device\n");
		goto err_input_register_device_failed;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
        touch->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        touch->early_suspend.suspend = tegra_touch_early_suspend;
        touch->early_suspend.resume = tegra_touch_late_resume;
        register_early_suspend(&touch->early_suspend);
#endif
	printk(KERN_INFO NVODM_TOUCH_NAME 
		": Successfully registered the ODM touch driver %x\n", (NvU32)touch->hTouchDevice);
	return 0;

err_input_register_device_failed:
	NvOdmTouchDeviceClose(touch->hTouchDevice);
err_kthread_create_failed:
	/* FIXME How to destroy the thread? Maybe we should use workqueues? */
err_open_failed:
	NvOdmOsSemaphoreDestroy(touch->semaphore);
err_semaphore_create_failed:
	input_free_device(touch->input_dev);
	kfree(touch);
	return err;
}

static int tegra_touch_remove(struct platform_device *pdev)
{
	struct tegra_touch_driver_data *touch = platform_get_drvdata(pdev);

#ifdef CONFIG_HAS_EARLYSUSPEND
        unregister_early_suspend(&touch->early_suspend);
#endif
        touch->shutdown = 1;
	/* FIXME How to destroy the thread? Maybe we should use workqueues? */
	input_unregister_device(touch->input_dev);
	/* NvOsSemaphoreDestroy(touch->semaphore); */
	input_unregister_device(touch->input_dev);
	kfree(touch);
	return 0;
}

static struct platform_driver tegra_touch_driver = {
	.probe	  = tegra_touch_probe,
	.remove	 = tegra_touch_remove,
	.suspend = tegra_touch_suspend,
	.resume	 = tegra_touch_resume,
	.driver	 = {
		.name   = "tegra_touch",
	},
};

static int __devinit tegra_touch_init(void)
{
	return platform_driver_register(&tegra_touch_driver);
}

static void __exit tegra_touch_exit(void)
{
	platform_driver_unregister(&tegra_touch_driver);
}

module_init(tegra_touch_init);
module_exit(tegra_touch_exit);

MODULE_DESCRIPTION("Tegra ODM touch driver");
