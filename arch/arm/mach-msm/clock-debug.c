/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2010, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/clk.h>
#include "clock.h"
#include "clock-pcom.h"

static int clock_debug_rate_set(void *data, u64 val)
{
	struct clk *clock = data;
	int ret;

	/* Only increases to max rate will succeed, but that's actually good
	 * for debugging purposes so we don't check for error. */
	if (clock->flags & CLK_MAX)
		clk_set_max_rate(clock, val);
	if (clock->flags & CLK_MIN)
		ret = clk_set_min_rate(clock, val);
	else
		ret = clk_set_rate(clock, val);
	if (ret != 0)
		printk(KERN_ERR "clk_set%s_rate failed (%d)\n",
			(clock->flags & CLK_MIN) ? "_min" : "", ret);
	return ret;
}

static int clock_debug_rate_get(void *data, u64 *val)
{
	struct clk *clock = data;
	*val = clk_get_rate(clock);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_rate_fops, clock_debug_rate_get,
			clock_debug_rate_set, "%llu\n");

static int clock_debug_enable_set(void *data, u64 val)
{
	struct clk *clock = data;
	int rc = 0;

	if (val)
		rc = clock->ops->enable(clock->id);
	else
		clock->ops->disable(clock->id);

	return rc;
}

static int clock_debug_enable_get(void *data, u64 *val)
{
	struct clk *clock = data;

	*val = clock->ops->is_enabled(clock->id);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_enable_fops, clock_debug_enable_get,
			clock_debug_enable_set, "%llu\n");

static int clock_debug_local_get(void *data, u64 *val)
{
	struct clk *clock = data;

	*val = clock->ops != &clk_ops_pcom;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_local_fops, clock_debug_local_get,
			NULL, "%llu\n");

static struct dentry *dent_rate, *dent_enable, *dent_local;

int __init clock_debug_init(void)
{
	dent_rate = debugfs_create_dir("clk_rate", 0);
	if (!dent_rate)
		goto err;

	dent_enable = debugfs_create_dir("clk_enable", 0);
	if (!dent_enable)
		goto err;

	dent_local = debugfs_create_dir("clk_local", NULL);
	if (!dent_local)
		goto err;

	return 0;
err:
	debugfs_remove(dent_local);
	debugfs_remove(dent_enable);
	debugfs_remove(dent_rate);
	return -ENOMEM;
}

int __init clock_debug_add(struct clk *clock)
{
	char temp[50], *ptr;

	if (!dent_rate || !dent_enable || !dent_local)
		return -ENOMEM;

	strncpy(temp, clock->dbg_name, ARRAY_SIZE(temp)-1);
	for (ptr = temp; *ptr; ptr++)
		*ptr = tolower(*ptr);

	debugfs_create_file(temp, S_IRUGO | S_IWUSR, dent_rate,
			    clock, &clock_rate_fops);
	debugfs_create_file(temp, S_IRUGO | S_IWUSR, dent_enable,
			    clock, &clock_enable_fops);
	debugfs_create_file(temp, S_IRUGO, dent_local,
			    clock, &clock_local_fops);

	return 0;
}
