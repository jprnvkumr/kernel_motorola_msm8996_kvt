/*
 * Samsung Laptop driver
 *
 * Copyright (C) 2009,2011 Greg Kroah-Hartman (gregkh@suse.de)
 * Copyright (C) 2009,2011 Novell Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/dmi.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/acpi.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

/*
 * This driver is needed because a number of Samsung laptops do not hook
 * their control settings through ACPI.  So we have to poke around in the
 * BIOS to do things like brightness values, and "special" key controls.
 */

/*
 * We have 0 - 8 as valid brightness levels.  The specs say that level 0 should
 * be reserved by the BIOS (which really doesn't make much sense), we tell
 * userspace that the value is 0 - 7 and then just tell the hardware 1 - 8
 */
#define MAX_BRIGHT	0x07


#define SABI_IFACE_MAIN			0x00
#define SABI_IFACE_SUB			0x02
#define SABI_IFACE_COMPLETE		0x04
#define SABI_IFACE_DATA			0x05

/* Structure get/set data using sabi */
struct sabi_data {
	union {
		struct {
			u32 d0;
			u32 d1;
			u16 d2;
			u8  d3;
		};
		u8 data[11];
	};
};

struct sabi_header_offsets {
	u8 port;
	u8 re_mem;
	u8 iface_func;
	u8 en_mem;
	u8 data_offset;
	u8 data_segment;
};

struct sabi_commands {
	/*
	 * Brightness is 0 - 8, as described above.
	 * Value 0 is for the BIOS to use
	 */
	u16 get_brightness;
	u16 set_brightness;

	/*
	 * first byte:
	 * 0x00 - wireless is off
	 * 0x01 - wireless is on
	 * second byte:
	 * 0x02 - 3G is off
	 * 0x03 - 3G is on
	 * TODO, verify 3G is correct, that doesn't seem right...
	 */
	u16 get_wireless_button;
	u16 set_wireless_button;

	/* 0 is off, 1 is on */
	u16 get_backlight;
	u16 set_backlight;

	/*
	 * 0x80 or 0x00 - no action
	 * 0x81 - recovery key pressed
	 */
	u16 get_recovery_mode;
	u16 set_recovery_mode;

	/*
	 * on seclinux: 0 is low, 1 is high,
	 * on swsmi: 0 is normal, 1 is silent, 2 is turbo
	 */
	u16 get_performance_level;
	u16 set_performance_level;

	/* 0x80 is off, 0x81 is on */
	u16 get_battery_life_extender;
	u16 set_battery_life_extender;

	/*
	 * Tell the BIOS that Linux is running on this machine.
	 * 81 is on, 80 is off
	 */
	u16 set_linux;
};

struct sabi_performance_level {
	const char *name;
	u16 value;
};

struct sabi_config {
	const char *test_string;
	u16 main_function;
	const struct sabi_header_offsets header_offsets;
	const struct sabi_commands commands;
	const struct sabi_performance_level performance_levels[4];
	u8 min_brightness;
	u8 max_brightness;
};

static const struct sabi_config sabi_configs[] = {
	{
		.test_string = "SECLINUX",

		.main_function = 0x4c49,

		.header_offsets = {
			.port = 0x00,
			.re_mem = 0x02,
			.iface_func = 0x03,
			.en_mem = 0x04,
			.data_offset = 0x05,
			.data_segment = 0x07,
		},

		.commands = {
			.get_brightness = 0x00,
			.set_brightness = 0x01,

			.get_wireless_button = 0x02,
			.set_wireless_button = 0x03,

			.get_backlight = 0x04,
			.set_backlight = 0x05,

			.get_recovery_mode = 0x06,
			.set_recovery_mode = 0x07,

			.get_performance_level = 0x08,
			.set_performance_level = 0x09,

			.get_battery_life_extender = 0xFFFF,
			.set_battery_life_extender = 0xFFFF,

			.set_linux = 0x0a,
		},

		.performance_levels = {
			{
				.name = "silent",
				.value = 0,
			},
			{
				.name = "normal",
				.value = 1,
			},
			{ },
		},
		.min_brightness = 1,
		.max_brightness = 8,
	},
	{
		.test_string = "SwSmi@",

		.main_function = 0x5843,

		.header_offsets = {
			.port = 0x00,
			.re_mem = 0x04,
			.iface_func = 0x02,
			.en_mem = 0x03,
			.data_offset = 0x05,
			.data_segment = 0x07,
		},

		.commands = {
			.get_brightness = 0x10,
			.set_brightness = 0x11,

			.get_wireless_button = 0x12,
			.set_wireless_button = 0x13,

			.get_backlight = 0x2d,
			.set_backlight = 0x2e,

			.get_recovery_mode = 0xff,
			.set_recovery_mode = 0xff,

			.get_performance_level = 0x31,
			.set_performance_level = 0x32,

			.get_battery_life_extender = 0x65,
			.set_battery_life_extender = 0x66,

			.set_linux = 0xff,
		},

		.performance_levels = {
			{
				.name = "normal",
				.value = 0,
			},
			{
				.name = "silent",
				.value = 1,
			},
			{
				.name = "overclock",
				.value = 2,
			},
			{ },
		},
		.min_brightness = 0,
		.max_brightness = 8,
	},
	{ },
};

/*
 * samsung-laptop/    - debugfs root directory
 *   f0000_segment    - dump f0000 segment
 *   command          - current command
 *   data             - current data
 *   d0, d1, d2, d3   - data fields
 *   call             - call SABI using command and data
 *
 * This allow to call arbitrary sabi commands wihout
 * modifying the driver at all.
 * For example, setting the keyboard backlight brightness to 5
 *
 *  echo 0x78 > command
 *  echo 0x0582 > d0
 *  echo 0 > d1
 *  echo 0 > d2
 *  echo 0 > d3
 *  cat call
 */

struct samsung_laptop_debug {
	struct dentry *root;
	struct sabi_data data;
	u16 command;

	struct debugfs_blob_wrapper f0000_wrapper;
	struct debugfs_blob_wrapper data_wrapper;
};

struct samsung_laptop {
	const struct sabi_config *config;

	void __iomem *sabi;
	void __iomem *sabi_iface;
	void __iomem *f0000_segment;

	struct mutex sabi_mutex;

	struct platform_device *platform_device;
	struct backlight_device *backlight_device;
	struct rfkill *rfk;

	struct samsung_laptop_debug debug;

	bool handle_backlight;
	bool has_stepping_quirk;
};



static bool force;
module_param(force, bool, 0);
MODULE_PARM_DESC(force,
		"Disable the DMI check and forces the driver to be loaded");

static bool debug;
module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");

static int sabi_command(struct samsung_laptop *samsung, u16 command,
			struct sabi_data *in,
			struct sabi_data *out)
{
	const struct sabi_config *config = samsung->config;
	int ret = 0;
	u16 port = readw(samsung->sabi + config->header_offsets.port);
	u8 complete, iface_data;

	mutex_lock(&samsung->sabi_mutex);

	if (debug) {
		if (in)
			pr_info("SABI 0x%04x {0x%08x, 0x%08x, 0x%04x, 0x%02x}",
				command, in->d0, in->d1, in->d2, in->d3);
		else
			pr_info("SABI 0x%04x", command);
	}

	/* enable memory to be able to write to it */
	outb(readb(samsung->sabi + config->header_offsets.en_mem), port);

	/* write out the command */
	writew(config->main_function, samsung->sabi_iface + SABI_IFACE_MAIN);
	writew(command, samsung->sabi_iface + SABI_IFACE_SUB);
	writeb(0, samsung->sabi_iface + SABI_IFACE_COMPLETE);
	if (in) {
		writel(in->d0, samsung->sabi_iface + SABI_IFACE_DATA);
		writel(in->d1, samsung->sabi_iface + SABI_IFACE_DATA + 4);
		writew(in->d2, samsung->sabi_iface + SABI_IFACE_DATA + 8);
		writeb(in->d3, samsung->sabi_iface + SABI_IFACE_DATA + 10);
	}
	outb(readb(samsung->sabi + config->header_offsets.iface_func), port);

	/* write protect memory to make it safe */
	outb(readb(samsung->sabi + config->header_offsets.re_mem), port);

	/* see if the command actually succeeded */
	complete = readb(samsung->sabi_iface + SABI_IFACE_COMPLETE);
	iface_data = readb(samsung->sabi_iface + SABI_IFACE_DATA);
	if (complete != 0xaa || iface_data == 0xff) {
		pr_warn("SABI command 0x%04x failed with"
			" completion flag 0x%02x and interface data 0x%02x",
			command, complete, iface_data);
		ret = -EINVAL;
		goto exit;
	}

	if (out) {
		out->d0 = readl(samsung->sabi_iface + SABI_IFACE_DATA);
		out->d1 = readl(samsung->sabi_iface + SABI_IFACE_DATA + 4);
		out->d2 = readw(samsung->sabi_iface + SABI_IFACE_DATA + 2);
		out->d3 = readb(samsung->sabi_iface + SABI_IFACE_DATA + 1);
	}

	if (debug && out) {
		pr_info("SABI {0x%08x, 0x%08x, 0x%04x, 0x%02x}",
			out->d0, out->d1, out->d2, out->d3);
	}

exit:
	mutex_unlock(&samsung->sabi_mutex);
	return ret;
}

/* simple wrappers usable with most commands */
static int sabi_set_commandb(struct samsung_laptop *samsung,
			     u16 command, u8 data)
{
	struct sabi_data in = { .d0 = 0, .d1 = 0, .d2 = 0, .d3 = 0 };

	in.data[0] = data;
	return sabi_command(samsung, command, &in, NULL);
}

static int read_brightness(struct samsung_laptop *samsung)
{
	const struct sabi_config *config = samsung->config;
	const struct sabi_commands *commands = &samsung->config->commands;
	struct sabi_data sretval;
	int user_brightness = 0;
	int retval;

	retval = sabi_command(samsung, commands->get_brightness,
			      NULL, &sretval);
	if (retval)
		return retval;

	user_brightness = sretval.data[0];
	if (user_brightness > config->min_brightness)
		user_brightness -= config->min_brightness;
	else
		user_brightness = 0;

	return user_brightness;
}

static void set_brightness(struct samsung_laptop *samsung, u8 user_brightness)
{
	const struct sabi_config *config = samsung->config;
	const struct sabi_commands *commands = &samsung->config->commands;
	u8 user_level = user_brightness + config->min_brightness;

	if (samsung->has_stepping_quirk && user_level != 0) {
		/*
		 * short circuit if the specified level is what's already set
		 * to prevent the screen from flickering needlessly
		 */
		if (user_brightness == read_brightness(samsung))
			return;

		sabi_set_commandb(samsung, commands->set_brightness, 0);
	}

	sabi_set_commandb(samsung, commands->set_brightness, user_level);
}

static int get_brightness(struct backlight_device *bd)
{
	struct samsung_laptop *samsung = bl_get_data(bd);

	return read_brightness(samsung);
}

static void check_for_stepping_quirk(struct samsung_laptop *samsung)
{
	int initial_level;
	int check_level;
	int orig_level = read_brightness(samsung);

	/*
	 * Some laptops exhibit the strange behaviour of stepping toward
	 * (rather than setting) the brightness except when changing to/from
	 * brightness level 0. This behaviour is checked for here and worked
	 * around in set_brightness.
	 */

	if (orig_level == 0)
		set_brightness(samsung, 1);

	initial_level = read_brightness(samsung);

	if (initial_level <= 2)
		check_level = initial_level + 2;
	else
		check_level = initial_level - 2;

	samsung->has_stepping_quirk = false;
	set_brightness(samsung, check_level);

	if (read_brightness(samsung) != check_level) {
		samsung->has_stepping_quirk = true;
		pr_info("enabled workaround for brightness stepping quirk\n");
	}

	set_brightness(samsung, orig_level);
}

static int update_status(struct backlight_device *bd)
{
	struct samsung_laptop *samsung = bl_get_data(bd);
	const struct sabi_commands *commands = &samsung->config->commands;

	set_brightness(samsung, bd->props.brightness);

	if (bd->props.power == FB_BLANK_UNBLANK)
		sabi_set_commandb(samsung, commands->set_backlight, 1);
	else
		sabi_set_commandb(samsung, commands->set_backlight, 0);

	return 0;
}

static const struct backlight_ops backlight_ops = {
	.get_brightness	= get_brightness,
	.update_status	= update_status,
};

static int rfkill_set(void *data, bool blocked)
{
	struct samsung_laptop *samsung = data;
	const struct sabi_commands *commands = &samsung->config->commands;

	/* Do something with blocked...*/
	/*
	 * blocked == false is on
	 * blocked == true is off
	 */
	if (blocked)
		sabi_set_commandb(samsung, commands->set_wireless_button, 0);
	else
		sabi_set_commandb(samsung, commands->set_wireless_button, 1);

	return 0;
}

static struct rfkill_ops rfkill_ops = {
	.set_block = rfkill_set,
};

static ssize_t get_performance_level(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct samsung_laptop *samsung = dev_get_drvdata(dev);
	const struct sabi_config *config = samsung->config;
	const struct sabi_commands *commands = &config->commands;
	struct sabi_data sretval;
	int retval;
	int i;

	/* Read the state */
	retval = sabi_command(samsung, commands->get_performance_level,
			      NULL, &sretval);
	if (retval)
		return retval;

	/* The logic is backwards, yeah, lots of fun... */
	for (i = 0; config->performance_levels[i].name; ++i) {
		if (sretval.data[0] == config->performance_levels[i].value)
			return sprintf(buf, "%s\n", config->performance_levels[i].name);
	}
	return sprintf(buf, "%s\n", "unknown");
}

static ssize_t set_performance_level(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct samsung_laptop *samsung = dev_get_drvdata(dev);
	const struct sabi_config *config = samsung->config;
	const struct sabi_commands *commands = &config->commands;
	int i;

	if (count < 1)
		return count;

	for (i = 0; config->performance_levels[i].name; ++i) {
		const struct sabi_performance_level *level =
			&config->performance_levels[i];
		if (!strncasecmp(level->name, buf, strlen(level->name))) {
			sabi_set_commandb(samsung,
					  commands->set_performance_level,
					  level->value);
			break;
		}
	}

	if (!config->performance_levels[i].name)
		return -EINVAL;

	return count;
}

static DEVICE_ATTR(performance_level, S_IWUSR | S_IRUGO,
		   get_performance_level, set_performance_level);

static int read_battery_life_extender(struct samsung_laptop *samsung)
{
	const struct sabi_commands *commands = &samsung->config->commands;
	struct sabi_data data;
	int retval;

	if (commands->get_battery_life_extender == 0xFFFF)
		return -ENODEV;

	memset(&data, 0, sizeof(data));
	data.data[0] = 0x80;
	retval = sabi_command(samsung, commands->get_battery_life_extender,
			      &data, &data);

	if (retval)
		return retval;

	if (data.data[0] != 0 && data.data[0] != 1)
		return -ENODEV;

	return data.data[0];
}

static int write_battery_life_extender(struct samsung_laptop *samsung,
				       int enabled)
{
	const struct sabi_commands *commands = &samsung->config->commands;
	struct sabi_data data;

	memset(&data, 0, sizeof(data));
	data.data[0] = 0x80 | enabled;
	return sabi_command(samsung, commands->set_battery_life_extender,
			    &data, NULL);
}

static ssize_t get_battery_life_extender(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct samsung_laptop *samsung = dev_get_drvdata(dev);
	int ret;

	ret = read_battery_life_extender(samsung);
	if (ret < 0)
		return ret;

	return sprintf(buf, "%d\n", ret);
}

static ssize_t set_battery_life_extender(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct samsung_laptop *samsung = dev_get_drvdata(dev);
	int ret, value;

	if (!count || sscanf(buf, "%i", &value) != 1)
		return -EINVAL;

	ret = write_battery_life_extender(samsung, !!value);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(battery_life_extender, S_IWUSR | S_IRUGO,
		   get_battery_life_extender, set_battery_life_extender);

static struct attribute *platform_attributes[] = {
	&dev_attr_performance_level.attr,
	&dev_attr_battery_life_extender.attr,
	NULL
};

static int find_signature(void __iomem *memcheck, const char *testStr)
{
	int i = 0;
	int loca;

	for (loca = 0; loca < 0xffff; loca++) {
		char temp = readb(memcheck + loca);

		if (temp == testStr[i]) {
			if (i == strlen(testStr)-1)
				break;
			++i;
		} else {
			i = 0;
		}
	}
	return loca;
}

static void samsung_rfkill_exit(struct samsung_laptop *samsung)
{
	if (samsung->rfk) {
		rfkill_unregister(samsung->rfk);
		rfkill_destroy(samsung->rfk);
		samsung->rfk = NULL;
	}
}

static int __init samsung_rfkill_init(struct samsung_laptop *samsung)
{
	int retval;

	samsung->rfk = rfkill_alloc("samsung-wifi",
				    &samsung->platform_device->dev,
				    RFKILL_TYPE_WLAN,
				    &rfkill_ops, samsung);
	if (!samsung->rfk)
		return -ENOMEM;

	retval = rfkill_register(samsung->rfk);
	if (retval) {
		rfkill_destroy(samsung->rfk);
		samsung->rfk = NULL;
		return -ENODEV;
	}

	return 0;
}

static void samsung_backlight_exit(struct samsung_laptop *samsung)
{
	if (samsung->backlight_device) {
		backlight_device_unregister(samsung->backlight_device);
		samsung->backlight_device = NULL;
	}
}

static int __init samsung_backlight_init(struct samsung_laptop *samsung)
{
	struct backlight_device *bd;
	struct backlight_properties props;

	if (!samsung->handle_backlight)
		return 0;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_PLATFORM;
	props.max_brightness = samsung->config->max_brightness -
		samsung->config->min_brightness;

	bd = backlight_device_register("samsung",
				       &samsung->platform_device->dev,
				       samsung, &backlight_ops,
				       &props);
	if (IS_ERR(bd))
		return PTR_ERR(bd);

	samsung->backlight_device = bd;
	samsung->backlight_device->props.brightness = read_brightness(samsung);
	samsung->backlight_device->props.power = FB_BLANK_UNBLANK;
	backlight_update_status(samsung->backlight_device);

	return 0;
}

static mode_t samsung_sysfs_is_visible(struct kobject *kobj,
				       struct attribute *attr, int idx)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct platform_device *pdev = to_platform_device(dev);
	struct samsung_laptop *samsung = platform_get_drvdata(pdev);
	bool ok = true;

	if (attr == &dev_attr_performance_level.attr)
		ok = !!samsung->config->performance_levels[0].name;
	if (attr == &dev_attr_battery_life_extender.attr)
		ok = !!(read_battery_life_extender(samsung) >= 0);

	return ok ? attr->mode : 0;
}

static struct attribute_group platform_attribute_group = {
	.is_visible = samsung_sysfs_is_visible,
	.attrs = platform_attributes
};

static void samsung_sysfs_exit(struct samsung_laptop *samsung)
{
	struct platform_device *device = samsung->platform_device;

	sysfs_remove_group(&device->dev.kobj, &platform_attribute_group);
}

static int __init samsung_sysfs_init(struct samsung_laptop *samsung)
{
	struct platform_device *device = samsung->platform_device;

	return sysfs_create_group(&device->dev.kobj, &platform_attribute_group);

}

static int show_call(struct seq_file *m, void *data)
{
	struct samsung_laptop *samsung = m->private;
	struct sabi_data *sdata = &samsung->debug.data;
	int ret;

	seq_printf(m, "SABI 0x%04x {0x%08x, 0x%08x, 0x%04x, 0x%02x}\n",
		   samsung->debug.command,
		   sdata->d0, sdata->d1, sdata->d2, sdata->d3);

	ret = sabi_command(samsung, samsung->debug.command, sdata, sdata);

	if (ret) {
		seq_printf(m, "SABI command 0x%04x failed\n",
			   samsung->debug.command);
		return ret;
	}

	seq_printf(m, "SABI {0x%08x, 0x%08x, 0x%04x, 0x%02x}\n",
		   sdata->d0, sdata->d1, sdata->d2, sdata->d3);
	return 0;
}

static int samsung_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_call, inode->i_private);
}

static const struct file_operations samsung_laptop_call_io_ops = {
	.owner = THIS_MODULE,
	.open = samsung_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void samsung_debugfs_exit(struct samsung_laptop *samsung)
{
	debugfs_remove_recursive(samsung->debug.root);
}

static int samsung_debugfs_init(struct samsung_laptop *samsung)
{
	struct dentry *dent;

	samsung->debug.root = debugfs_create_dir("samsung-laptop", NULL);
	if (!samsung->debug.root) {
		pr_err("failed to create debugfs directory");
		goto error_debugfs;
	}

	samsung->debug.f0000_wrapper.data = samsung->f0000_segment;
	samsung->debug.f0000_wrapper.size = 0xffff;

	samsung->debug.data_wrapper.data = &samsung->debug.data;
	samsung->debug.data_wrapper.size = sizeof(samsung->debug.data);

	dent = debugfs_create_u16("command", S_IRUGO | S_IWUSR,
				  samsung->debug.root, &samsung->debug.command);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_u32("d0", S_IRUGO | S_IWUSR, samsung->debug.root,
				  &samsung->debug.data.d0);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_u32("d1", S_IRUGO | S_IWUSR, samsung->debug.root,
				  &samsung->debug.data.d1);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_u16("d2", S_IRUGO | S_IWUSR, samsung->debug.root,
				  &samsung->debug.data.d2);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_u8("d3", S_IRUGO | S_IWUSR, samsung->debug.root,
				 &samsung->debug.data.d3);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_blob("data", S_IRUGO | S_IWUSR,
				   samsung->debug.root,
				   &samsung->debug.data_wrapper);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_blob("f0000_segment", S_IRUSR | S_IWUSR,
				   samsung->debug.root,
				   &samsung->debug.f0000_wrapper);
	if (!dent)
		goto error_debugfs;

	dent = debugfs_create_file("call", S_IFREG | S_IRUGO,
				   samsung->debug.root, samsung,
				   &samsung_laptop_call_io_ops);
	if (!dent)
		goto error_debugfs;

	return 0;

error_debugfs:
	samsung_debugfs_exit(samsung);
	return -ENOMEM;
}

static void samsung_sabi_exit(struct samsung_laptop *samsung)
{
	const struct sabi_config *config = samsung->config;

	/* Turn off "Linux" mode in the BIOS */
	if (config && config->commands.set_linux != 0xff)
		sabi_set_commandb(samsung, config->commands.set_linux, 0x80);

	if (samsung->sabi_iface) {
		iounmap(samsung->sabi_iface);
		samsung->sabi_iface = NULL;
	}
	if (samsung->f0000_segment) {
		iounmap(samsung->f0000_segment);
		samsung->f0000_segment = NULL;
	}

	samsung->config = NULL;
}

static __init void samsung_sabi_infos(struct samsung_laptop *samsung, int loca,
				      unsigned int ifaceP)
{
	const struct sabi_config *config = samsung->config;

	printk(KERN_DEBUG "This computer supports SABI==%x\n",
	       loca + 0xf0000 - 6);

	printk(KERN_DEBUG "SABI header:\n");
	printk(KERN_DEBUG " SMI Port Number = 0x%04x\n",
	       readw(samsung->sabi + config->header_offsets.port));
	printk(KERN_DEBUG " SMI Interface Function = 0x%02x\n",
	       readb(samsung->sabi + config->header_offsets.iface_func));
	printk(KERN_DEBUG " SMI enable memory buffer = 0x%02x\n",
	       readb(samsung->sabi + config->header_offsets.en_mem));
	printk(KERN_DEBUG " SMI restore memory buffer = 0x%02x\n",
	       readb(samsung->sabi + config->header_offsets.re_mem));
	printk(KERN_DEBUG " SABI data offset = 0x%04x\n",
	       readw(samsung->sabi + config->header_offsets.data_offset));
	printk(KERN_DEBUG " SABI data segment = 0x%04x\n",
	       readw(samsung->sabi + config->header_offsets.data_segment));

	printk(KERN_DEBUG " SABI pointer = 0x%08x\n", ifaceP);
}

static int __init samsung_sabi_init(struct samsung_laptop *samsung)
{
	const struct sabi_config *config = NULL;
	const struct sabi_commands *commands;
	unsigned int ifaceP;
	int ret = 0;
	int i;
	int loca;

	samsung->f0000_segment = ioremap_nocache(0xf0000, 0xffff);
	if (!samsung->f0000_segment) {
		pr_err("Can't map the segment at 0xf0000\n");
		ret = -EINVAL;
		goto exit;
	}

	/* Try to find one of the signatures in memory to find the header */
	for (i = 0; sabi_configs[i].test_string != 0; ++i) {
		samsung->config = &sabi_configs[i];
		loca = find_signature(samsung->f0000_segment,
				      samsung->config->test_string);
		if (loca != 0xffff)
			break;
	}

	if (loca == 0xffff) {
		pr_err("This computer does not support SABI\n");
		ret = -ENODEV;
		goto exit;
	}

	config = samsung->config;
	commands = &config->commands;

	/* point to the SMI port Number */
	loca += 1;
	samsung->sabi = (samsung->f0000_segment + loca);

	/* Get a pointer to the SABI Interface */
	ifaceP = (readw(samsung->sabi + config->header_offsets.data_segment) & 0x0ffff) << 4;
	ifaceP += readw(samsung->sabi + config->header_offsets.data_offset) & 0x0ffff;

	if (debug)
		samsung_sabi_infos(samsung, loca, ifaceP);

	samsung->sabi_iface = ioremap_nocache(ifaceP, 16);
	if (!samsung->sabi_iface) {
		pr_err("Can't remap %x\n", ifaceP);
		ret = -EINVAL;
		goto exit;
	}

	/* Turn on "Linux" mode in the BIOS */
	if (commands->set_linux != 0xff) {
		int retval = sabi_set_commandb(samsung,
					       commands->set_linux, 0x81);
		if (retval) {
			pr_warn("Linux mode was not set!\n");
			ret = -ENODEV;
			goto exit;
		}
	}

	/* Check for stepping quirk */
	if (samsung->handle_backlight)
		check_for_stepping_quirk(samsung);

exit:
	if (ret)
		samsung_sabi_exit(samsung);

	return ret;
}

static void samsung_platform_exit(struct samsung_laptop *samsung)
{
	if (samsung->platform_device) {
		platform_device_unregister(samsung->platform_device);
		samsung->platform_device = NULL;
	}
}

static int __init samsung_platform_init(struct samsung_laptop *samsung)
{
	struct platform_device *pdev;

	pdev = platform_device_register_simple("samsung", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	samsung->platform_device = pdev;
	platform_set_drvdata(samsung->platform_device, samsung);
	return 0;
}

static int __init dmi_check_cb(const struct dmi_system_id *id)
{
	pr_info("found laptop model '%s'\n", id->ident);
	return 1;
}

static struct dmi_system_id __initdata samsung_dmi_table[] = {
	{
		.ident = "N128",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N128"),
			DMI_MATCH(DMI_BOARD_NAME, "N128"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N130",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N130"),
			DMI_MATCH(DMI_BOARD_NAME, "N130"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N510",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N510"),
			DMI_MATCH(DMI_BOARD_NAME, "N510"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "X125",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X125"),
			DMI_MATCH(DMI_BOARD_NAME, "X125"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "X120/X170",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X120/X170"),
			DMI_MATCH(DMI_BOARD_NAME, "X120/X170"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "NC10",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "NC10"),
			DMI_MATCH(DMI_BOARD_NAME, "NC10"),
		},
		.callback = dmi_check_cb,
	},
		{
		.ident = "NP-Q45",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "SQ45S70S"),
			DMI_MATCH(DMI_BOARD_NAME, "SQ45S70S"),
		},
		.callback = dmi_check_cb,
		},
	{
		.ident = "X360",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X360"),
			DMI_MATCH(DMI_BOARD_NAME, "X360"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R410 Plus",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "R410P"),
			DMI_MATCH(DMI_BOARD_NAME, "R460"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R518",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "R518"),
			DMI_MATCH(DMI_BOARD_NAME, "R518"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R519/R719",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "R519/R719"),
			DMI_MATCH(DMI_BOARD_NAME, "R519/R719"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N150/N210/N220",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N150/N210/N220"),
			DMI_MATCH(DMI_BOARD_NAME, "N150/N210/N220"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N220",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N220"),
			DMI_MATCH(DMI_BOARD_NAME, "N220"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N150/N210/N220/N230",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N150/N210/N220/N230"),
			DMI_MATCH(DMI_BOARD_NAME, "N150/N210/N220/N230"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N150P/N210P/N220P",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N150P/N210P/N220P"),
			DMI_MATCH(DMI_BOARD_NAME, "N150P/N210P/N220P"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R700",
		.matches = {
		      DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		      DMI_MATCH(DMI_PRODUCT_NAME, "SR700"),
		      DMI_MATCH(DMI_BOARD_NAME, "SR700"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R530/R730",
		.matches = {
		      DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
		      DMI_MATCH(DMI_PRODUCT_NAME, "R530/R730"),
		      DMI_MATCH(DMI_BOARD_NAME, "R530/R730"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "NF110/NF210/NF310",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "NF110/NF210/NF310"),
			DMI_MATCH(DMI_BOARD_NAME, "NF110/NF210/NF310"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "N145P/N250P/N260P",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "N145P/N250P/N260P"),
			DMI_MATCH(DMI_BOARD_NAME, "N145P/N250P/N260P"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R70/R71",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR,
					"SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "R70/R71"),
			DMI_MATCH(DMI_BOARD_NAME, "R70/R71"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "P460",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "P460"),
			DMI_MATCH(DMI_BOARD_NAME, "P460"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "R528/R728",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "R528/R728"),
			DMI_MATCH(DMI_BOARD_NAME, "R528/R728"),
		},
		.callback = dmi_check_cb,
	},
	{
		.ident = "NC210/NC110",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "NC210/NC110"),
			DMI_MATCH(DMI_BOARD_NAME, "NC210/NC110"),
		},
		.callback = dmi_check_cb,
	},
		{
		.ident = "X520",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SAMSUNG ELECTRONICS CO., LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X520"),
			DMI_MATCH(DMI_BOARD_NAME, "X520"),
		},
		.callback = dmi_check_cb,
	},
	{ },
};
MODULE_DEVICE_TABLE(dmi, samsung_dmi_table);

static struct platform_device *samsung_platform_device;

static int __init samsung_init(void)
{
	struct samsung_laptop *samsung;
	int ret;

	if (!force && !dmi_check_system(samsung_dmi_table))
		return -ENODEV;

	samsung = kzalloc(sizeof(*samsung), GFP_KERNEL);
	if (!samsung)
		return -ENOMEM;

	mutex_init(&samsung->sabi_mutex);
	samsung->handle_backlight = true;

#ifdef CONFIG_ACPI
	/* Don't handle backlight here if the acpi video already handle it */
	if (acpi_video_backlight_support()) {
		pr_info("Backlight controlled by ACPI video driver\n");
		samsung->handle_backlight = false;
	}
#endif

	ret = samsung_platform_init(samsung);
	if (ret)
		goto error_platform;

	ret = samsung_sabi_init(samsung);
	if (ret)
		goto error_sabi;

	ret = samsung_sysfs_init(samsung);
	if (ret)
		goto error_sysfs;

	ret = samsung_backlight_init(samsung);
	if (ret)
		goto error_backlight;

	ret = samsung_rfkill_init(samsung);
	if (ret)
		goto error_rfkill;

	ret = samsung_debugfs_init(samsung);
	if (ret)
		goto error_debugfs;

	samsung_platform_device = samsung->platform_device;
	return ret;

error_debugfs:
	samsung_rfkill_exit(samsung);
error_rfkill:
	samsung_backlight_exit(samsung);
error_backlight:
	samsung_sysfs_exit(samsung);
error_sysfs:
	samsung_sabi_exit(samsung);
error_sabi:
	samsung_platform_exit(samsung);
error_platform:
	kfree(samsung);
	return ret;
}

static void __exit samsung_exit(void)
{
	struct samsung_laptop *samsung;

	samsung = platform_get_drvdata(samsung_platform_device);

	samsung_debugfs_exit(samsung);
	samsung_rfkill_exit(samsung);
	samsung_backlight_exit(samsung);
	samsung_sysfs_exit(samsung);
	samsung_sabi_exit(samsung);
	samsung_platform_exit(samsung);

	kfree(samsung);
	samsung_platform_device = NULL;
}

module_init(samsung_init);
module_exit(samsung_exit);

MODULE_AUTHOR("Greg Kroah-Hartman <gregkh@suse.de>");
MODULE_DESCRIPTION("Samsung Backlight driver");
MODULE_LICENSE("GPL");
