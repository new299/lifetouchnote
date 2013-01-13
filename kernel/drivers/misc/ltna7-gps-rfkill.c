/*
 * GPS rfkill power control via GPIO
 * Modified by Quanta Computer, Inc from:
 *
 * GPS rfkill power control via GPIO
 *
 * Copyright (C) 2010 NVIDIA Corporation
 * Copyright (C) 2011 NEC Corporation
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/ltna7-gps-rfkill.h>

static int ltna7_gps_rfkill_set_power(void *data, bool blocked)
{
	struct platform_device *pdev = data;
	struct ltna7_gps_platform_data *plat = pdev->dev.platform_data;
	if (!blocked) {
		gpio_set_value(plat->gpio_pwr, 0);
		msleep(plat->delay);
		gpio_set_value(plat->gpio_pwr, 1);
	} else {
		gpio_set_value(plat->gpio_pwr, 0);
	}

	return 0;
}

static struct rfkill_ops ltna7_gps_rfkill_ops = {
	.set_block = ltna7_gps_rfkill_set_power,
};

static int ltna7_gps_rfkill_probe(struct platform_device *pdev)
{

	struct ltna7_gps_platform_data *plat = pdev->dev.platform_data;
	struct rfkill *rfkill;

	int rc;

	if (!plat) {
		dev_err(&pdev->dev, "no platform data\n");
		return -ENOSYS;
	}

	rc = gpio_request(plat->gpio_pwr, "ltna7_gps_ctl");
	if (rc < 0) {
		dev_err(&pdev->dev, "gpio_request failed\n");
		return rc;
	}

	rfkill = rfkill_alloc("ltna7-gps-rfkill", &pdev->dev,
			RFKILL_TYPE_GPS, &ltna7_gps_rfkill_ops, pdev);
	if (!rfkill) {
		rc = -ENOMEM;
		goto fail_gpio;
	}
	platform_set_drvdata(pdev, rfkill);
	gpio_direction_output(plat->gpio_pwr, 0);

	rc = rfkill_register(rfkill);
	if (rc < 0)
		goto fail_alloc;

	return 0;

fail_alloc:
	rfkill_destroy(rfkill);
fail_gpio:
	gpio_free(plat->gpio_pwr);
	return rc;
}

static int ltna7_gps_rfkill_remove(struct platform_device *pdev)
{
	struct ltna7_gps_platform_data *plat = pdev->dev.platform_data;
	struct rfkill *rfkill = platform_get_drvdata(pdev);

	rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	gpio_free(plat->gpio_pwr);
	return 0;
}

static int ltna7_gps_rfkill_suspend(struct platform_device *pdev)
{
	struct ltna7_gps_platform_data *plat = pdev->dev.platform_data;
	gpio_set_value(plat->gpio_pwr, 0);
	return 0;
}

static int ltna7_gps_rfkill_resume(struct platform_device *pdev)
{
	struct ltna7_gps_platform_data *plat = pdev->dev.platform_data;
	struct rfkill *rfkill = platform_get_drvdata(pdev);
	if (!rfkill_blocked(rfkill))
		gpio_set_value(plat->gpio_pwr, 1);
	return 0;
}

static struct platform_driver ltna7_gps_rfkill_driver = {
	.probe = ltna7_gps_rfkill_probe,
	.remove = ltna7_gps_rfkill_remove,
	.suspend = ltna7_gps_rfkill_suspend,
	.resume = ltna7_gps_rfkill_resume,
	.driver = {
		.name = "ltna7-gps-rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init ltna7_gps_rfkill_init(void)
{
	return platform_driver_register(&ltna7_gps_rfkill_driver);

}

static void __exit ltna7_gps_rfkill_exit(void)
{
	platform_driver_unregister(&ltna7_gps_rfkill_driver);
}

module_init(ltna7_gps_rfkill_init);
module_exit(ltna7_gps_rfkill_exit);

MODULE_DESCRIPTION("LTNA7 GPS rfkill");
MODULE_AUTHOR("QCI");
MODULE_LICENSE("GPL");
