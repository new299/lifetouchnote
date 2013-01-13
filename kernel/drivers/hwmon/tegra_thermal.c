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
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <mach/nvrm_linux.h>
#include <linux/err.h>
#include "nvodm_tmon.h"

struct adt7461_data {
        struct device           *hwmon_dev;
        struct attribute_group  attrs;
        struct mutex            lock;
};

struct tegra_thermal_driver_data
{
        NvS32     temp;
};

/*
* show thermal value in /sys/...
*/
static ssize_t show_temp(struct device *dev,
                             struct device_attribute *devattr,
                             char *buf)
{
        NvS32 TemperatureC = 0;
        NvOdmTmonDeviceHandle hOdmTcore;

        // Make sure TMON h/w is initialized
        hOdmTcore = NvOdmTmonDeviceOpen(NvOdmTmonZoneID_Core);
        if (hOdmTcore == NULL)
                return sprintf(buf, "0\n");
        
        if (NvOdmTmonTemperatureGet(hOdmTcore, &TemperatureC)) {
            NvOdmTmonDeviceClose(hOdmTcore);
            return sprintf(buf, "%d\n", TemperatureC);
        }

        // err to get thermal 
        NvOdmTmonDeviceClose(hOdmTcore);
        return sprintf(buf, "0\n");    
}

static SENSOR_DEVICE_ATTR(temp, S_IRUGO, show_temp, NULL, 0);

static struct attribute *adt7461_attr[] =
{
        &sensor_dev_attr_temp.dev_attr.attr,
        NULL
};
static int __init tegra_thermal_probe(struct platform_device *pdev)
{
        struct adt7461_data *data;
        int err;

        data = kzalloc(sizeof(struct adt7461_data), GFP_KERNEL);
        if (!data) {
                pr_err("tegra_thermal_probe: allocate mem failed\n");
                err = -ENOMEM;
                goto exit;
        }

        /* Register sysfs hooks */
        data->attrs.attrs = adt7461_attr;
        err = sysfs_create_group(&pdev->dev.kobj, &data->attrs);
        if (err) {
                pr_err("tegra_thermal_probe: sysfs_create_group failed\n");
                goto exit_free;
        }

        data->hwmon_dev = hwmon_device_register(&pdev->dev);
        if (IS_ERR(data->hwmon_dev)) {
                pr_err("tegra_thermal_probe: hwmon_device_register failed\n");
                err = PTR_ERR(data->hwmon_dev);
                goto exit_remove;
        }

        platform_set_drvdata(pdev, data);
        pr_err("tegra_thermal:registered\n");

        return 0;

exit_remove:
        sysfs_remove_group(&pdev->dev.kobj, &data->attrs);
exit_free:
        kfree(data);
exit:
        return err;

}

static int tegra_thermal_remove(struct platform_device *pdev)
{
        struct adt7461_data *data = platform_get_drvdata(pdev);

        hwmon_device_unregister(data->hwmon_dev);
        sysfs_remove_group(&pdev->dev.kobj, &data->attrs);
        kfree(data);
        return 0;
}

static struct platform_driver tegra_thermal_driver = {
        .probe      = tegra_thermal_probe,
        .remove     = tegra_thermal_remove,
        .driver     = {
                .name   = "tegra_thermal",
        },
};

static int __devinit tegra_thermal_init(void)
{
        return platform_driver_register(&tegra_thermal_driver);
}

static void __exit tegra_thermal_exit(void)
{
        platform_driver_unregister(&tegra_thermal_driver);
}

module_init(tegra_thermal_init);
module_exit(tegra_thermal_exit);

MODULE_DESCRIPTION("Tegra Thermal Driver");

