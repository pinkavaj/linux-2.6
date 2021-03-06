/* Machine specific code for the Acer n30, Acer N35, Navman PiN 570,
 * Yakumo AlphaX and Airis NC05 PDAs.
 *
 * Copyright (c) 2003-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Copyright (c) 2005-2008 Christer Weinigel <christer@weinigel.se>
 *
 * There is a wiki with more information about the n30 port at
 * http://handhelds.org/moin/moin.cgi/AcerN30Documentation .
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/delay.h>
#include <linux/gpio_keys.h>
#include <linux/memblock.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/serial_core.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/mmc/host.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <mach/fb.h>
#include <mach/h1940.h>
#include <mach/leds-gpio.h>
#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/map.h>

#include <plat/iic.h>
#include <plat/regs-serial.h>

#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/devs.h>
#include <plat/mci.h>
#include <plat/nand.h>
#include <plat/s3c2410.h>
#include <plat/ts.h>
#include <plat/udc.h>

#include <video/platform_lcd.h>

#define N35_SUSPEND_RESUMEAT (S3C2410_SDRAM_PA + 0x201000)

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>

struct mtd_partition n35_default_nand_part[] = {
	/* Original partitioning by manufacturer
	 *
	 * 0x0     - 0x3fff  NBoot
	 * 0x4000  - 0x4fff  TOC
	 * 0x8000  - 0x27fff Eboot
	 * 0x28000 - 0x2bfff partition table
	 * 0x2c000 -         data ...
	 * FIXME: incomplete
	 * */
	{
		.name   = "bootloader",
		.offset = 0,
		.size   = 0x28000,
		.mask_flags = MTD_WRITEABLE,
	},
	{
		.name   = "partition",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x4000,
	},
	{
		.name   = "kernel",
		.offset = MTDPART_OFS_APPEND,
		.size   = 0x180000,
	},
	{
		.name   = "rootfs",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL,
	},
};

static struct s3c2410_nand_set n35_nand_sets[] = {
	{
		.name       = "chip0",
		.nr_chips   = 1,
		.nr_partitions  = ARRAY_SIZE(n35_default_nand_part),
		.partitions = n35_default_nand_part,
	},
};

static struct s3c2410_platform_nand n35_nand_info = {
	.tacls      = 25,
	.twrph0     = 55,
	.twrph1     = 25,
	.nr_sets    = ARRAY_SIZE(n35_nand_sets),
	.sets       = n35_nand_sets,
	.ignore_unset_ecc = 1,
};
#endif

static struct map_desc n30_iodesc[] __initdata = {
	/* nothing here yet */
};

#define UCON	(S3C2410_UCON_DEFAULT | \
		 S3C2443_UCON_RXERR_IRQEN)

#define ULCON	(S3C2410_LCON_CS8 | \
		 S3C2410_LCON_PNONE)

#define UFCON	(S3C2410_UFCON_FIFOMODE | \
		 S3C2410_UFCON_RXTRIG8 | \
		 S3C2410_UFCON_TXTRIG4)

static struct s3c2410_uartcfg n30_uartcfgs[] = {
	/* Normal serial port */
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
	/* IR port */
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.uart_flags  = UPF_CONS_FLOW,
		.ucon	     = UCON,
		.ulcon	     = ULCON | S3C2410_LCON_IRM,
		.ufcon	     = UFCON,
	},
	/* On the N30 the bluetooth controller is connected here.
	 * On the N35 and variants the GPS receiver is connected here. */
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = UCON,
		.ulcon	     = ULCON,
		.ufcon	     = UFCON,
	},
};

static struct s3c2410_udc_mach_info n30_udc_cfg __initdata = {
	.vbus_pin		= S3C2410_GPG(1),
	.vbus_pin_inverted	= 0,
	.pullup_pin		= S3C2410_GPB(3),
};

static struct s3c2410_ts_mach_info n30_ts_cfg __initdata = {
	.delay = 16383,
	.presc = 49,
	.oversampling_shift = 3,
	.cfg_gpio = s3c24xx_ts_cfg_gpio,
};

/* It would be possible to register GPG1 as interrupt resource EINT9
 * with the pda_power code, but since GPC7 cannot have an interrupt
 * connected to it, the pda_power driver will use polling anyway.  In
 * addition to that, since the USB driver already requests EINT9, the
 * request_irq in pda_power fails.  So don't do that. */

/*
static struct resource n30_power_resource[] = {
	{
		.name = "usb",
		.start = IRQ_EINT9,
		.end   = IRQ_EINT9,
		.flags = IORESOURCE_IRQ,
	},
};
*/

static int n30_is_ac_online(void)
{
	return gpio_get_value(S3C2410_GPC(7));
}

static int n30_is_usb_online(void)
{
	return gpio_get_value(S3C2410_GPG(1));
}

static char *n30_power_supplied_to[] = {
	"battery",
};

static struct pda_power_pdata n30_power_pdata = {
	.is_ac_online = n30_is_ac_online,
	.is_usb_online = n30_is_usb_online,
	.supplied_to = n30_power_supplied_to,
	.num_supplicants = ARRAY_SIZE(n30_power_supplied_to),
};

static struct platform_device n30_power = {
	.name		= "pda-power",
	.id		= -1,
	.dev		= {
		.platform_data	= &n30_power_pdata,
	},
/*
	.num_resources	= ARRAY_SIZE(n30_power_resource),
	.resource	= n30_power_resource,
*/
};

static struct platform_device n30_battery = {
	.name		= "n30-battery",
	.id		= -1,
};

static struct gpio_keys_button n30_buttons[] = {
	{
		.gpio		= S3C2410_GPF(0),
		.code		= KEY_POWER,
		.desc		= "Power",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(9),
		.code		= KEY_UP,
		.desc		= "Thumbwheel Up",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(8),
		.code		= KEY_DOWN,
		.desc		= "Thumbwheel Down",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(7),
		.code		= KEY_ENTER,
		.desc		= "Thumbwheel Press",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(7),
		.code		= KEY_HOMEPAGE,
		.desc		= "Home",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(6),
		.code		= KEY_CALENDAR,
		.desc		= "Calendar",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(5),
		.code		= KEY_ADDRESSBOOK,
		.desc		= "Contacts",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(4),
		.code		= KEY_MAIL,
		.desc		= "Mail",
		.active_low	= 0,
	},
};

static struct gpio_keys_platform_data n30_button_data = {
	.buttons	= n30_buttons,
	.nbuttons	= ARRAY_SIZE(n30_buttons),
};

static struct platform_device n30_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &n30_button_data,
	}
};

static struct gpio_keys_button n35_buttons[] = {
	{
		.gpio		= S3C2410_GPF(0),
		.code		= KEY_POWER,
		.desc		= "Power",
		.active_low	= 0,
		.wakeup		= 1,
	},
	{
		.gpio		= S3C2410_GPG(9),
		.code		= KEY_UP,
		.desc		= "Joystick Up",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(8),
		.code		= KEY_DOWN,
		.desc		= "Joystick Down",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(6),
		.code		= KEY_LEFT,
		.desc		= "Joystick Left",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(5),
		.code		= KEY_RIGHT,
		.desc		= "Joystick Right",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(7),
		.code		= KEY_ENTER,
		.desc		= "Joystick Press",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(7),
		.code		= KEY_HOMEPAGE,
		.desc		= "Home",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(6),
		.code		= KEY_CALENDAR,
		.desc		= "Calendar",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(5),
		.code		= KEY_ADDRESSBOOK,
		.desc		= "Contacts",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(4),
		.code		= KEY_MAIL,
		.desc		= "Mail",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPF(3),
		.code		= SW_RADIO,
		.desc		= "GPS Antenna",
		.active_low	= 0,
	},
	{
		.gpio		= S3C2410_GPG(2),
		.code		= SW_HEADPHONE_INSERT,
		.desc		= "Headphone",
		.active_low	= 0,
	},
};

static struct gpio_keys_platform_data n35_button_data = {
	.buttons	= n35_buttons,
	.nbuttons	= ARRAY_SIZE(n35_buttons),
};

static struct platform_device n35_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &n35_button_data,
	}
};

/* This is the bluetooth LED on the device. */
static struct s3c24xx_led_platdata n30_blue_led_pdata = {
	.name		= "blue_led",
	.gpio		= S3C2410_GPG(6),
	.def_trigger	= "",
};

/* This is the blue LED on the device. Originally used to indicate GPS activity
 * by flashing. */
static struct s3c24xx_led_platdata n35_blue_led_pdata = {
	.name		= "blue_led",
	.gpio		= S3C2410_GPD(8),
	.def_trigger	= "",
};

/* This LED is driven by the battery microcontroller, and is blinking
 * red, blinking green or solid green when the battery is low,
 * charging or full respectively.  By driving GPD9 low, it's possible
 * to force the LED to blink red, so call that warning LED.  */
static struct s3c24xx_led_platdata n30_warning_led_pdata = {
	.name		= "warning_led",
	.flags          = S3C24XX_LEDF_ACTLOW,
	.gpio		= S3C2410_GPD(9),
	.def_trigger	= "",
};

static struct s3c24xx_led_platdata n35_warning_led_pdata = {
	.name		= "warning_led",
	.flags          = S3C24XX_LEDF_ACTLOW | S3C24XX_LEDF_TRISTATE,
	.gpio		= S3C2410_GPD(9),
	.def_trigger	= "",
};

static struct platform_device n30_blue_led = {
	.name		= "s3c24xx_led",
	.id		= 1,
	.dev		= {
		.platform_data	= &n30_blue_led_pdata,
	},
};

static struct platform_device n35_blue_led = {
	.name		= "s3c24xx_led",
	.id		= 1,
	.dev		= {
		.platform_data	= &n35_blue_led_pdata,
	},
};

static struct platform_device n30_warning_led = {
	.name		= "s3c24xx_led",
	.id		= 2,
	.dev		= {
		.platform_data	= &n30_warning_led_pdata,
	},
};

static struct platform_device n35_warning_led = {
	.name		= "s3c24xx_led",
	.id		= 2,
	.dev		= {
		.platform_data	= &n35_warning_led_pdata,
	},
};

static struct s3c2410fb_display n30_display __initdata = {
	.type		= S3C2410_LCDCON1_TFT,
	.width		= 240,
	.height		= 320,
	.pixclock	= 170000,

	.xres		= 240,
	.yres		= 320,
	.bpp		= 16,
	.left_margin	= 3,
	.right_margin	= 40,
	.hsync_len	= 40,
	.upper_margin	= 2,
	.lower_margin	= 3,
	.vsync_len	= 2,

	.lcdcon5 = S3C2410_LCDCON5_INVVLINE | S3C2410_LCDCON5_INVVFRAME,
};

static struct s3c2410fb_mach_info n30_fb_info __initdata = {
	.displays	= &n30_display,
	.num_displays	= 1,
	.default_display = 0,
	.lpcsel		= 0x06,

	.gpccon		= 0xaaa80208,
	.gpccon_mask	= 0xfffc030c,
	.gpcup		= 0x0000fe12,
	.gpcup_mask	= 0xfffffe12,
	.gpdcon		= 0xaa80aaa4,
	.gpdcon_mask	= 0xffc0fffc,
	.gpdup		= 0x0000f8fe,
	.gpdup_mask	= 0xfffff8fe,
};

static int n35_backlight_init(struct device *dev)
{
	int ret;

	ret = gpio_request(S3C2410_GPB(0), "Backlight PWM output");
	if (ret)
		goto request_gpb0_fail;
	ret = gpio_request(S3C2410_GPB(1), "Backlight power");
	if (ret)
		goto request_gpb1_fail;

	/* set GPB0 as output of PWM timer */
	gpio_set_value(S3C2410_GPB(0), 0);
	s3c_gpio_setpull(S3C2410_GPB(0), S3C_GPIO_PULL_DOWN);
	s3c_gpio_cfgpin(S3C2410_GPB(0), S3C2410_GPB0_TOUT0);

	/* backlight power */
	ret = gpio_direction_output(S3C2410_GPB(1), 1);
	if (ret)
		goto direction_gpb1_fail;
	s3c_gpio_setpull(S3C2410_GPB(1), S3C_GPIO_PULL_UP);

	return 0;

direction_gpb1_fail:
	gpio_free(S3C2410_GPB(1));
request_gpb1_fail:
	gpio_free(S3C2410_GPB(0));
request_gpb0_fail:
	return ret;
}

static int n35_backlight_notify(struct device *dev, int brightness)
{
	/* power off backlight, values less than 14 are useless */
	gpio_set_value(S3C2410_GPB(1), brightness > 14 ? 1 : 0);
	return (brightness > 14 ? brightness : 0);
}

static void n35_backlight_exit(struct device *dev)
{
	gpio_direction_output(S3C2410_GPB(0), 0);
	gpio_free(S3C2410_GPB(0));

	gpio_set_value(S3C2410_GPB(1), 0);
	s3c_gpio_setpull(S3C2410_GPB(1), S3C_GPIO_PULL_UP);
	gpio_free(S3C2410_GPB(1));
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id         = 0,
	.max_brightness = 100,
	.dft_brightness = 50,
	.pwm_period_ns  = 3*1000*1000,
	.init           = n35_backlight_init,
	.notify         = n35_backlight_notify,
	.exit           = n35_backlight_exit,
};

static struct platform_device n35_backlight = {
	.name = "pwm-backlight",
	.dev  = {
		.parent = &s3c_device_timer[0].dev,
		.platform_data = &backlight_data,
	},
	.id   = -1,
};

static void n35_lcd_set_power(struct plat_lcd_data *pd, unsigned int power)
{
	if (power) {
		gpio_set_value(S3C2410_GPB(8), 1);
		s3c_gpio_setpull(S3C2410_GPB(8), S3C_GPIO_PULL_UP);

		s3c_gpio_cfgpin(S3C2410_GPB(9), S3C2410_GPIO_OUTPUT);
		gpio_set_value(S3C2410_GPB(9), 1);
		s3c_gpio_setpull(S3C2410_GPB(9), S3C_GPIO_PULL_UP);

		s3c_gpio_cfgpin(S3C2410_GPB(10), S3C2410_GPIO_OUTPUT);
		gpio_set_value(S3C2410_GPB(10), 1);
		s3c_gpio_setpull(S3C2410_GPB(10), S3C_GPIO_PULL_UP);
	} else {
		gpio_set_value(S3C2410_GPB(8), 0);
		s3c_gpio_setpull(S3C2410_GPB(8), S3C_GPIO_PULL_DOWN);

		s3c_gpio_cfgpin(S3C2410_GPB(9), S3C2410_GPIO_INPUT);
		s3c_gpio_setpull(S3C2410_GPB(9), S3C_GPIO_PULL_DOWN);

		s3c_gpio_cfgpin(S3C2410_GPB(10), S3C2410_GPIO_INPUT);
		s3c_gpio_setpull(S3C2410_GPB(10), S3C_GPIO_PULL_DOWN);
	}
}


static struct plat_lcd_data n35_lcd_power_data = {
	.set_power = n35_lcd_set_power,
};

static struct platform_device n35_lcd_powerdev = {
	.name	= "platform-lcd",
	.id	= -1,
	.dev	= {
		.parent		= &s3c_device_lcd.dev,
		.platform_data	= &n35_lcd_power_data,
	},
};

static void n30_sdi_set_power(unsigned char power_mode, unsigned short vdd)
{
	switch (power_mode) {
	case MMC_POWER_ON:
	case MMC_POWER_UP:
		gpio_set_value(S3C2410_GPG(4), 0);
		break;
	case MMC_POWER_OFF:
	default:
		gpio_set_value(S3C2410_GPG(4), 1);
		break;
	}
}

static struct s3c24xx_mci_pdata n30_mci_cfg __initdata = {
	.gpio_detect	= S3C2410_GPF(1),
	.gpio_wprotect  = S3C2410_GPG(10),
	.ocr_avail	= MMC_VDD_32_33,
	.set_power	= n30_sdi_set_power,
};

static struct platform_device n35_device_gps = {
	.name             = "n35-gps",
	.id               = -1,
};

static struct platform_device *n30_devices[] __initdata = {
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c0,
	&s3c_device_iis,
	&s3c_device_ohci,
	&s3c_device_rtc,
	&s3c_device_usbgadget,
	&s3c_device_sdi,
	&s3c_device_adc,
	&s3c_device_ts,
	&n30_battery,
	&n30_power,
	&n30_button_device,
	&n30_blue_led,
	&n30_warning_led,
};

static struct platform_device *n35_devices[] __initdata = {
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c0,
	&s3c_device_iis,
	&s3c_device_rtc,
	&s3c_device_usbgadget,
	&s3c_device_sdi,
#ifdef CONFIG_MTD_PARTITIONS
	&s3c_device_nand,
#endif
	&s3c_device_adc,
	&s3c_device_ts,
	&s3c_device_timer[0],
	&n35_backlight,
	&n35_device_gps,
	&n35_lcd_powerdev,
	&n30_battery,
	&n35_button_device,
	&n35_blue_led,
	&n35_warning_led,
	&n30_power,
};

static struct s3c2410_platform_i2c __initdata n30_i2ccfg = {
	.flags		= 0,
	.slave_addr	= 0x10,
	.frequency	= 10*1000,
};

/* Lots of hardcoded stuff, but it sets up the hardware in a useful
 * state so that we can boot Linux directly from flash. */
static void __init n30_hwinit(void)
{
	/* GPA0-11 special functions -- unknown what they do
	 * GPA12 N30 special function (nGCS1) -- unknown what it does
	 *       PiN output -- unknown function
	 *       N35 output -- turn on/off red led, 0 = normal operation,
	 *       1 = turn off red led at all. Note: GPD9 must be input or
	 *       output with value set to 1 before setting GPA12 to 0. */
	if (machine_is_n30())
		__raw_writel(0x007fffff, S3C2410_GPACON);
	if (machine_is_n35())
		__raw_writel(0x007fefff, S3C2410_GPACON);
	__raw_writel(0x00000000, S3C2410_GPADAT);

	/* GPB0 TOUT0 backlight level
	 * GPB1 output 1=backlight on
	 * GPB2 output IrDA enable 0=transceiver enabled, 1=disabled
	 * GPB3 output USB D+ pull up 0=disabled, 1=enabled
	 * GPB4 N30 output -- unknown function
	 *      N30/PiN GPS control 0=GPS enabled, 1=GPS disabled
	 * GPB5 output -- unknown function
	 * GPB6 input -- unknown function
	 * GPB7 output -- unknown function
	 * GPB8 output -- probably LCD driver enable
	 * GPB9 output -- probably LCD VSYNC driver enable
	 * GPB10 output -- probably LCD HSYNC driver enable
	 */
	__raw_writel(0x00154556, S3C2410_GPBCON);
	__raw_writel(0x00000750, S3C2410_GPBDAT);
	__raw_writel(0x00000073, S3C2410_GPBUP);

	/* GPC0 input RS232 DCD/DSR/RI
	 * GPC1 LCD
	 * GPC2 output RS232 DTR?
	 * GPC3 input RS232 DCD/DSR/RI
	 * GPC4 LCD
	 * GPC5 output 0=NAND write enabled, 1=NAND write protect
	 * GPC6 input -- unknown function
	 * GPC7 input charger status 0=charger connected
	 *      this input can be triggered by power on the USB device
	 *      port too, but will go back to disconnected soon after.
	 * GPC8 N30/N35 output -- unknown function, always driven to 1
	 *      PiN input -- unknown function, always read as 1
	 *      Make it an input with a pull up for all models.
	 * GPC9-15 LCD
	 */
	__raw_writel(0xaaa80618, S3C2410_GPCCON);
	__raw_writel(0x0000014c, S3C2410_GPCDAT);
	__raw_writel(0x0000fef2, S3C2410_GPCUP);

	/* GPD0 input -- unknown function
	 * GPD1-D7 LCD
	 * GPD8 N30 output -- unknown function
	 *      N35/PiN output 1=GPS LED on
	 * GPD9 output 0=power led blinks red, 1=normal power led function
	 * GPD10 output -- unknown function
	 * GPD11-15 LCD drivers
	 */
	__raw_writel(0xaa95aaa4, S3C2410_GPDCON);
	__raw_writel(0x00000601, S3C2410_GPDDAT);
	__raw_writel(0x0000fbfe, S3C2410_GPDUP);

	/* GPE0-4 I2S audio bus
	 * GPE5-10 SD/MMC bus
	 * E11-13 outputs -- unknown function, probably power management
	 * E14-15 I2C bus connected to the battery controller
	 */
	__raw_writel(0xa56aaaaa, S3C2410_GPECON);
	__raw_writel(0x0000efc5, S3C2410_GPEDAT);
	__raw_writel(0x0000f81f, S3C2410_GPEUP);

	/* GPF0  input 0=power button pressed
	 * GPF1  input SD/MMC switch 0=card present
	 * GPF2  N30 1=reset button pressed (inverted compared to the rest)
	 *	 N35/PiN 0=reset button pressed
	 * GPF3  N30/PiN input -- unknown function
	 *       N35 input GPS antenna position, 0=antenna closed, 1=open
	 * GPF4  input 0=button 4 pressed
	 * GPF5  input 0=button 3 pressed
	 * GPF6  input 0=button 2 pressed
	 * GPF7  input 0=button 1 pressed
	 */
	__raw_writel(0x0000aaaa, S3C2410_GPFCON);
	__raw_writel(0x00000000, S3C2410_GPFDAT);
	__raw_writel(0x000000ff, S3C2410_GPFUP);

	/* GPG0  input RS232 DCD/DSR/RI
	 * GPG1  input 1=USB gadget port has power from a host
	 * GPG2  N30 input -- unknown function
	 *       N35/PiN input 0=headphones plugged in, 1=not plugged in
	 * GPG3  N30 output -- unknown function
	 *       N35/PiN input with unknown function
	 * GPG4  N30 output 0=MMC enabled, 1=MMC disabled
	 * GPG5  N30 output 0=BlueTooth chip disabled, 1=enabled
	 *       N35/PiN input joystick right
	 * GPG6  N30 output 0=blue led on, 1=off
	 *       N35/PiN input joystick left
	 * GPG7  input 0=thumbwheel pressed
	 * GPG8  input 0=thumbwheel down
	 * GPG9  input 0=thumbwheel up
	 * GPG10 input SD/MMC write protect switch
	 * GPG11 N30 input -- unknown function
	 *       N35 output 0=GPS antenna powered, 1=not powered
	 *       PiN output -- unknown function
	 * GPG12-15 touch screen functions
	 *
	 * The pullups differ between the models, so enable all
	 * pullups that are enabled on any of the models.
	 */
	if (machine_is_n30())
		__raw_writel(0xff0a956a, S3C2410_GPGCON);
	if (machine_is_n35())
		__raw_writel(0xff4aa92a, S3C2410_GPGCON);
	__raw_writel(0x0000e800, S3C2410_GPGDAT);
	__raw_writel(0x0000f86f, S3C2410_GPGUP);

	/* GPH0/1/2/3 RS232 serial port
	 * GPH4/5 IrDA serial port
	 * GPH6/7  N30 BlueTooth serial port
	 *         N35/PiN GPS receiver
	 * GPH8 input -- unknown function
	 * GPH9 CLKOUT0 HCLK -- unknown use
	 * GPH10 CLKOUT1 FCLK -- unknown use
	 *
	 * The pull ups for H6/H7 are enabled on N30 but not on the
	 * N35/PiN.  I suppose is useful for a budget model of the N30
	 * with no bluetooh.  It doesn't hurt to have the pull ups
	 * enabled on the N35, so leave them enabled for all models.
	 */
	__raw_writel(0x0028aaaa, S3C2410_GPHCON);
	__raw_writel(0x000005ef, S3C2410_GPHDAT);
	__raw_writel(0x0000063f, S3C2410_GPHUP);
}

static void __init n30_map_io(void)
{
	s3c24xx_init_io(n30_iodesc, ARRAY_SIZE(n30_iodesc));
	n30_hwinit();
	s3c24xx_init_clocks(0);
	s3c24xx_init_uarts(n30_uartcfgs, ARRAY_SIZE(n30_uartcfgs));
}

/* Acer N30/N35 need to reserve this memory to suspend */
static void __init n30_reserve(void)
{
       memblock_reserve(0x30201000, 0x1000);
}

/* GPB3 is the line that controls the pull-up for the USB D+ line */

static void __init n30_init(void)
{
	WARN_ON(gpio_request(S3C2410_GPG(4), "mmc power"));

	/* setup PM */

#ifdef CONFIG_PM_H1940
	memcpy(phys_to_virt(N35_SUSPEND_RESUMEAT), h1940_pm_return,
			h1940_pm_return_end - h1940_pm_return);
#endif
	s3c_pm_init();

	s3c24xx_fb_set_platdata(&n30_fb_info);
	s3c24xx_ts_set_platdata(&n30_ts_cfg);
	s3c24xx_udc_set_platdata(&n30_udc_cfg);
	s3c24xx_mci_set_platdata(&n30_mci_cfg);
	s3c_i2c0_set_platdata(&n30_i2ccfg);

	/* Turn off suspend on both USB ports, and switch the
	 * selectable USB port to USB device mode. */

	s3c2410_modify_misccr(S3C2410_MISCCR_USBHOST |
			      S3C2410_MISCCR_USBSUSPND0 |
			      S3C2410_MISCCR_USBSUSPND1, 0x0);

	if (machine_is_n30()) {
		/* Turn off suspend on both USB ports, and switch the
		 * selectable USB port to USB device mode. */
		s3c2410_modify_misccr(S3C2410_MISCCR_USBHOST |
				      S3C2410_MISCCR_USBSUSPND0 |
				      S3C2410_MISCCR_USBSUSPND1, 0x0);

		platform_add_devices(n30_devices, ARRAY_SIZE(n30_devices));
	}

	if (machine_is_n35()) {
		WARN_ON(gpio_request(S3C2410_GPB(8), "LCD driver"));
		WARN_ON(gpio_request(S3C2410_GPB(9), "LCD HSYNC"));
		WARN_ON(gpio_request(S3C2410_GPB(10), "LCD VSYNC"));
		s3c_gpio_cfgpin(S3C2410_GPB(8), S3C2410_GPIO_OUTPUT);

		/* Reset all locks - flash is and write protected but ready
                 * to unlock. */
		WARN_ON(gpio_request(S3C2410_GPC(5), "NAND write protection"));
		gpio_set_value(S3C2410_GPC(5), 1);
		udelay(10);
		gpio_set_value(S3C2410_GPC(5), 0);
		udelay(10);
		gpio_set_value(S3C2410_GPC(5), 1);

                s3c_nand_set_platdata(&n35_nand_info);

		/* Turn off suspend and switch the selectable USB port
		 * to USB device mode.  Turn on suspend for the host
		 * port since it is not connected on the N35.
		 *
		 * Actually, the host port is available at some pads
		 * on the back of the device, so it would actually be
		 * possible to add a USB device inside the N35 if you
		 * are willing to do some hardware modifications. */
		s3c2410_modify_misccr(S3C2410_MISCCR_USBHOST |
				      S3C2410_MISCCR_USBSUSPND0 |
				      S3C2410_MISCCR_USBSUSPND1,
				      S3C2410_MISCCR_USBSUSPND0);

		platform_add_devices(n35_devices, ARRAY_SIZE(n35_devices));
	}
}

MACHINE_START(N30, "Acer-N30")
	/* Maintainer: Christer Weinigel <christer@weinigel.se>,
				Ben Dooks <ben-linux@fluff.org>
	*/
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.timer		= &s3c24xx_timer,
	.init_machine	= n30_init,
	.init_irq	= s3c24xx_init_irq,
	.map_io		= n30_map_io,
	.reserve	= n30_reserve,
MACHINE_END

MACHINE_START(N35, "Acer-N35")
	/* Maintainer: Christer Weinigel <christer@weinigel.se>
	*/
	.boot_params	= S3C2410_SDRAM_PA + 0x100,
	.timer		= &s3c24xx_timer,
	.init_machine	= n30_init,
	.init_irq	= s3c24xx_init_irq,
	.map_io		= n30_map_io,
	.reserve	= n30_reserve,
MACHINE_END
