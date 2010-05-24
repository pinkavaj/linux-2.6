/* Acer n30 battery driver.
 *
 * The Acer n30 family has a battery controller which sits on the I2C
 * bus and which can report the battery charge in percent.  Two GPIO
 * pins tells if the PDA is being powered from a charger or via USB.
 *
 * Copyright (c) 2008 Christer Weinigel <christer@weinigel.se>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>

#include <asm/mach-types.h>

static int n30_battery_get_capacity(struct device *dev)
{
	u8 value;
	struct i2c_adapter *adap;
	struct i2c_msg msg[] = {
		{ .addr = 0x0b, .flags = I2C_M_RD, .buf = &value, .len = 1 }
	};
	int capacity = -EIO;

	adap = i2c_get_adapter(0);
	if (adap != NULL) {
		if (i2c_transfer(adap, msg, 1) == 1)
			capacity = value;
		else
			dev_err(dev, "failed to read capacity\n");
		i2c_put_adapter(adap);
	} else
		dev_err(dev, "failed to get i2c adapter\n");

	return capacity;
}

static int n30_battery_get_property(struct power_supply *psy,
				    enum power_supply_property psp,
				    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = n30_battery_get_capacity(psy->dev);
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property n30_battery_props[] = {
	POWER_SUPPLY_PROP_CAPACITY,
};

static struct power_supply n30_battery = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = n30_battery_props,
	.num_properties = ARRAY_SIZE(n30_battery_props),
	.get_property = n30_battery_get_property,
	.use_for_apm = 1,
};

static int __devinit n30_battery_probe(struct platform_device *pdev)
{
	int ret;

	dev_info(&pdev->dev, "Acer n30 battery driver\n");

	ret = power_supply_register(&pdev->dev, &n30_battery);
	if (ret)
		return ret;

	return 0;
}

static int n30_battery_remove(struct platform_device *pdev)
{
	power_supply_unregister(&n30_battery);
	return 0;
}

static struct platform_driver n30_battery_driver = {
	.driver = {
		.name = "n30-battery",
	},
	.probe = n30_battery_probe,
	.remove = n30_battery_remove,
};

static int __init n30_battery_init(void)
{
	return platform_driver_register(&n30_battery_driver);
}

static void __exit n30_battery_exit(void)
{
	platform_driver_unregister(&n30_battery_driver);
}

module_init(n30_battery_init);
module_exit(n30_battery_exit);

MODULE_AUTHOR("Christer Weinigel <christer@weinigel.se>");
MODULE_DESCRIPTION("Acer n30 battery driver");
MODULE_LICENSE("GPL");
