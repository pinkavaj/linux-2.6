/*
 * arch/arm/mach-s3c2410/n35-gps.c
 * Copyright (c) Arnaud Patard <arnaud.patard@rtp-net.org>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    Acer n35 GPS "driver"
 *	    FIXME: Do proper up/down handling, now it only ses power on/off.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
//#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/gpio.h>

#include <mach/regs-gpio.h>
#include <mach/hardware.h>

#define DRV_NAME              "n35-gps"

struct n35_gps_data {
	int suspend_power_state;
};

static struct n35_gps_data n35_gps;

static int gps_power_get(void)
{
	return !(gpio_get_value(S3C2410_GPB(4)) || \
			gpio_get_value(S3C2410_GPG(11)));
}

static void gps_power_set(int power)
{
	gpio_set_value(S3C2410_GPB(4), power ? 0 : 1);
	gpio_set_value(S3C2410_GPG(11), power ? 0 : 1);
}

static ssize_t gps_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (gps_power_get())
		return sprintf(buf, "1\n");
	return sprintf(buf, "0\n");
}

static ssize_t gps_power_store(struct device *pdev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int power;

	power = simple_strtoul(buf, NULL, 0);
	gps_power_set(power);

	return size;
}

static DEVICE_ATTR(gps_power, 0664, gps_power_show, gps_power_store);

static int __devinit n35_gps_probe(struct platform_device *pdev)
{
	int ret;

	ret = gpio_request(S3C2410_GPB(4), "Acer n35 GPS module power");
	if (ret)
		goto ret_err;

	ret = gpio_request(S3C2410_GPG(11), "Acer n35 GPS antena power");
	if (ret)
		goto ret_err_gpg4;

	s3c_gpio_cfgpin(S3C2410_GPB(4), S3C2410_GPIO_OUTPUT);
	s3c_gpio_cfgpin(S3C2410_GPG(11), S3C2410_GPIO_OUTPUT);

	gps_power_set(0);

	ret = device_create_file(&pdev->dev, &dev_attr_gps_power);
	if (ret) {
		dev_err(&pdev->dev, "Failed to create GPS sysfs device\n");
		goto ret_err_gpg11;
	}

	return 0;
ret_err_gpg11:
	gpio_free(S3C2410_GPG(11));
ret_err_gpg4:
	gpio_free(S3C2410_GPB(4));
ret_err:
	return ret;
}

static int n35_gps_remove(struct platform_device *pdev)
{
	gps_power_set(0);

	gpio_free(S3C2410_GPG(11));
	gpio_free(S3C2410_GPB(4));

	device_remove_file(&pdev->dev, &dev_attr_gps_power);

	return 0;
}

static int n35_gps_suspend(struct platform_device *pdev, pm_message_t state)
{
	n35_gps.suspend_power_state = gps_power_get();
	gps_power_set(0);
	return 0;
}

static int n35_gps_resume(struct platform_device *pdev)
{
	gps_power_set(n35_gps.suspend_power_state);
	return 0;
}

static struct platform_driver n35_gps_driver = {
	.driver		= {
		.name	= DRV_NAME,
	},
	.probe		= n35_gps_probe,
	.remove		= n35_gps_remove,
	.resume		= n35_gps_resume,
	.suspend	= n35_gps_suspend,
};


static int __init n35_gps_init(void)
{
	return platform_driver_register(&n35_gps_driver);
}

static void __exit n35_gps_exit(void)
{
	platform_driver_unregister(&n35_gps_driver);
}

module_init(n35_gps_init);
module_exit(n35_gps_exit);

MODULE_AUTHOR("Jiri Pinkava <jiri.pinkava@vscht.cz>");
MODULE_DESCRIPTION("Driver for the Acer n35 GPS");
MODULE_LICENSE("GPL");
