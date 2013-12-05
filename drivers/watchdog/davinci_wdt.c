/*
 * drivers/char/watchdog/davinci_wdt.c
 *
 * Watchdog driver for DaVinci DM644x/DM646x processors
 *
 * Copyright (C) 2006-2013 Texas Instruments.
 *
 * 2007 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>

#define MODULE_NAME "DAVINCI-WDT: "

#define DEFAULT_HEARTBEAT 60
#define MAX_HEARTBEAT     600	/* really the max margin is 264/27MHz*/

/* Timer register set definition */
#define PID12	(0x0)
#define EMUMGT	(0x4)
#define TIM12	(0x10)
#define TIM34	(0x14)
#define PRD12	(0x18)
#define PRD34	(0x1C)
#define TCR	(0x20)
#define TGCR	(0x24)
#define WDTCR	(0x28)

/* TCR bit definitions */
#define ENAMODE12_DISABLED	(0 << 6)
#define ENAMODE12_ONESHOT	(1 << 6)
#define ENAMODE12_PERIODIC	(2 << 6)

/* TGCR bit definitions */
#define TIM12RS_UNRESET		(1 << 0)
#define TIM34RS_UNRESET		(1 << 1)
#define TIMMODE_64BIT_WDOG      (2 << 2)

/* WDTCR bit definitions */
#define WDEN			(1 << 14)
#define WDFLAG			(1 << 15)
#define WDKEY_SEQ0		(0xa5c6 << 16)
#define WDKEY_SEQ1		(0xda7e << 16)

static int heartbeat;
static void __iomem	*wdt_base;
struct clk		*wdt_clk;
static struct watchdog_device	wdt_wdd;

static int davinci_wdt_start(struct watchdog_device *wdd)
{
	u32 tgcr;
	u32 timer_margin;
	unsigned long wdt_freq;

	wdt_freq = clk_get_rate(wdt_clk);

	/* disable, internal clock source */
	iowrite32(0, wdt_base + TCR);
	/* reset timer, set mode to 64-bit watchdog, and unreset */
	iowrite32(0, wdt_base + TGCR);
	tgcr = TIMMODE_64BIT_WDOG | TIM12RS_UNRESET | TIM34RS_UNRESET;
	iowrite32(tgcr, wdt_base + TGCR);
	/* clear counter regs */
	iowrite32(0, wdt_base + TIM12);
	iowrite32(0, wdt_base + TIM34);
	/* set timeout period */
	timer_margin = (((u64)wdd->timeout * wdt_freq) & 0xffffffff);
	iowrite32(timer_margin, wdt_base + PRD12);
	timer_margin = (((u64)wdd->timeout * wdt_freq) >> 32);
	iowrite32(timer_margin, wdt_base + PRD34);
	/* enable run continuously */
	iowrite32(ENAMODE12_PERIODIC, wdt_base + TCR);
	/* Once the WDT is in pre-active state write to
	 * TIM12, TIM34, PRD12, PRD34, TCR, TGCR, WDTCR are
	 * write protected (except for the WDKEY field)
	 */
	/* put watchdog in pre-active state */
	iowrite32(WDKEY_SEQ0 | WDEN, wdt_base + WDTCR);
	/* put watchdog in active state */
	iowrite32(WDKEY_SEQ1 | WDEN, wdt_base + WDTCR);
	return 0;
}

static int davinci_wdt_ping(struct watchdog_device *wdd)
{
	/* put watchdog in service state */
	iowrite32(WDKEY_SEQ0, wdt_base + WDTCR);
	/* put watchdog in active state */
	iowrite32(WDKEY_SEQ1, wdt_base + WDTCR);
	return 0;
}

static const struct watchdog_info davinci_wdt_info = {
	.options = WDIOF_KEEPALIVEPING,
	.identity = "DaVinci Watchdog",
};

static const struct watchdog_ops davinci_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= davinci_wdt_start,
	.stop		= davinci_wdt_ping,
	.ping		= davinci_wdt_ping,
};

static int davinci_wdt_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct resource  *wdt_mem;
	struct watchdog_device *wdd;

	wdt_clk = devm_clk_get(dev, NULL);
	if (WARN_ON(IS_ERR(wdt_clk)))
		return PTR_ERR(wdt_clk);

	clk_prepare_enable(wdt_clk);

	wdd			= &wdt_wdd;
	wdd->info		= &davinci_wdt_info;
	wdd->ops		= &davinci_wdt_ops;
	wdd->min_timeout	= 1;
	wdd->max_timeout	= MAX_HEARTBEAT;
	wdd->timeout		= DEFAULT_HEARTBEAT;

	watchdog_init_timeout(wdd, heartbeat, dev);

	dev_info(dev, "heartbeat %d sec\n", wdd->timeout);

	watchdog_set_nowayout(wdd, 1);

	wdt_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt_base = devm_ioremap_resource(dev, wdt_mem);
	if (IS_ERR(wdt_base))
		return PTR_ERR(wdt_base);

	ret = watchdog_register_device(wdd);
	if (ret < 0)
		dev_err(dev, "cannot register watchdog device\n");

	return ret;
}

static int davinci_wdt_remove(struct platform_device *pdev)
{
	watchdog_unregister_device(&wdt_wdd);
	clk_disable_unprepare(wdt_clk);

	return 0;
}

static const struct of_device_id davinci_wdt_of_match[] = {
	{ .compatible = "ti,davinci-wdt", },
	{},
};
MODULE_DEVICE_TABLE(of, davinci_wdt_of_match);

static struct platform_driver platform_wdt_driver = {
	.driver = {
		.name = "davinci-wdt",
		.owner	= THIS_MODULE,
		.of_match_table = davinci_wdt_of_match,
	},
	.probe = davinci_wdt_probe,
	.remove = davinci_wdt_remove,
};

module_platform_driver(platform_wdt_driver);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("DaVinci Watchdog Driver");

module_param(heartbeat, int, 0);
MODULE_PARM_DESC(heartbeat,
		 "Watchdog heartbeat period in seconds from 1 to "
		 __MODULE_STRING(MAX_HEARTBEAT) ", default "
		 __MODULE_STRING(DEFAULT_HEARTBEAT));

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:davinci-wdt");
