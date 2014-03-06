/*
 * phy-ti-pipe3 - PIPE3 PHY driver.
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/phy/phy.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/phy/omap_control_phy.h>
#include <linux/of_platform.h>

#define	PLL_STATUS		0x00000004
#define	PLL_GO			0x00000008
#define	PLL_CONFIGURATION1	0x0000000C
#define	PLL_CONFIGURATION2	0x00000010
#define	PLL_CONFIGURATION3	0x00000014
#define	PLL_CONFIGURATION4	0x00000020

#define	PLL_REGM_MASK		0x001FFE00
#define	PLL_REGM_SHIFT		0x9
#define	PLL_REGM_F_MASK		0x0003FFFF
#define	PLL_REGM_F_SHIFT	0x0
#define	PLL_REGN_MASK		0x000001FE
#define	PLL_REGN_SHIFT		0x1
#define	PLL_SELFREQDCO_MASK	0x0000000E
#define	PLL_SELFREQDCO_SHIFT	0x1
#define	PLL_SD_MASK		0x0003FC00
#define	PLL_SD_SHIFT		0x9
#define	SET_PLL_GO		0x1
#define	PLL_TICOPWDN		0x10000
#define	PLL_LOCK		0x2
#define	PLL_IDLE		0x1

/*
 * This is an Empirical value that works, need to confirm the actual
 * value required for the PIPE3PHY_PLL_CONFIGURATION2.PLL_IDLE status
 * to be correctly reflected in the PIPE3PHY_PLL_STATUS register.
 */
# define PLL_IDLE_TIME  100;

struct pipe3_dpll_params {
	u16	m;
	u8	n;
	u8	freq:3;
	u8	sd;
	u32	mf;
};

struct ti_pipe3 {
	void __iomem		*pll_ctrl_base;
	struct device		*dev;
	struct device		*control_dev;
	struct clk		*wkupclk;
	struct clk		*sys_clk;
	struct clk		*optclk;
};

struct pipe3_dpll_map {
	unsigned long rate;
	struct pipe3_dpll_params params;
};

static struct pipe3_dpll_map dpll_map[] = {
	{12000000, {1250, 5, 4, 20, 0} },	/* 12 MHz */
	{16800000, {3125, 20, 4, 20, 0} },	/* 16.8 MHz */
	{19200000, {1172, 8, 4, 20, 65537} },	/* 19.2 MHz */
	{20000000, {1000, 7, 4, 10, 0} },	/* 20 MHz */
	{26000000, {1250, 12, 4, 20, 0} },	/* 26 MHz */
	{38400000, {3125, 47, 4, 20, 92843} },	/* 38.4 MHz */
};

static inline u32 ti_pipe3_readl(void __iomem *addr, unsigned offset)
{
	return __raw_readl(addr + offset);
}

static inline void ti_pipe3_writel(void __iomem *addr, unsigned offset,
	u32 data)
{
	__raw_writel(data, addr + offset);
}

static struct pipe3_dpll_params *ti_pipe3_get_dpll_params(unsigned long rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpll_map); i++) {
		if (rate == dpll_map[i].rate)
			return &dpll_map[i].params;
	}

	return NULL;
}

static int ti_pipe3_power_off(struct phy *x)
{
	struct ti_pipe3 *phy = phy_get_drvdata(x);
	int val;
	int timeout = PLL_IDLE_TIME;

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
	val |= PLL_IDLE;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

	do {
		val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_STATUS);
		if (val & PLL_TICOPWDN)
			break;
		udelay(5);
	} while (--timeout);

	if (!timeout) {
		dev_err(phy->dev, "power off failed\n");
		return -EBUSY;
	}

	omap_control_phy_power(phy->control_dev, 0);

	return 0;
}

static int ti_pipe3_power_on(struct phy *x)
{
	struct ti_pipe3 *phy = phy_get_drvdata(x);
	int val;
	int timeout = PLL_IDLE_TIME;

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
	val &= ~PLL_IDLE;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

	do {
		val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_STATUS);
		if (!(val & PLL_TICOPWDN))
			break;
		udelay(5);
	} while (--timeout);

	if (!timeout) {
		dev_err(phy->dev, "power on failed\n");
		return -EBUSY;
	}

	return 0;
}

static void ti_pipe3_dpll_relock(struct ti_pipe3 *phy)
{
	u32		val;
	unsigned long	timeout;

	ti_pipe3_writel(phy->pll_ctrl_base, PLL_GO, SET_PLL_GO);

	timeout = jiffies + msecs_to_jiffies(20);
	do {
		val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_STATUS);
		if (val & PLL_LOCK)
			break;
	} while (!WARN_ON(time_after(jiffies, timeout)));
}

static int ti_pipe3_dpll_lock(struct ti_pipe3 *phy)
{
	u32			val;
	unsigned long		rate;
	struct pipe3_dpll_params *dpll_params;

	rate = clk_get_rate(phy->sys_clk);
	dpll_params = ti_pipe3_get_dpll_params(rate);
	if (!dpll_params) {
		dev_err(phy->dev,
			  "No DPLL configuration for %lu Hz SYS CLK\n", rate);
		return -EINVAL;
	}

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION1);
	val &= ~PLL_REGN_MASK;
	val |= dpll_params->n << PLL_REGN_SHIFT;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION1, val);

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
	val &= ~PLL_SELFREQDCO_MASK;
	val |= dpll_params->freq << PLL_SELFREQDCO_SHIFT;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION1);
	val &= ~PLL_REGM_MASK;
	val |= dpll_params->m << PLL_REGM_SHIFT;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION1, val);

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION4);
	val &= ~PLL_REGM_F_MASK;
	val |= dpll_params->mf << PLL_REGM_F_SHIFT;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION4, val);

	val = ti_pipe3_readl(phy->pll_ctrl_base, PLL_CONFIGURATION3);
	val &= ~PLL_SD_MASK;
	val |= dpll_params->sd << PLL_SD_SHIFT;
	ti_pipe3_writel(phy->pll_ctrl_base, PLL_CONFIGURATION3, val);

	ti_pipe3_dpll_relock(phy);

	return 0;
}

static int ti_pipe3_init(struct phy *x)
{
	struct ti_pipe3 *phy = phy_get_drvdata(x);
	int ret;

	ret = ti_pipe3_dpll_lock(phy);
	if (ret)
		return ret;

	omap_control_phy_power(phy->control_dev, 1);

	return 0;
}

static struct phy_ops ops = {
	.init		= ti_pipe3_init,
	.power_on	= ti_pipe3_power_on,
	.power_off	= ti_pipe3_power_off,
	.owner		= THIS_MODULE,
};

static int ti_pipe3_probe(struct platform_device *pdev)
{
	struct ti_pipe3 *phy;
	struct phy *generic_phy;
	struct phy_provider *phy_provider;
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;
	struct device_node *control_node;
	struct platform_device *control_pdev;

	if (!node)
		return -EINVAL;

	phy = devm_kzalloc(&pdev->dev, sizeof(*phy), GFP_KERNEL);
	if (!phy) {
		dev_err(&pdev->dev, "unable to alloc mem for TI PIPE3 PHY\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pll_ctrl");
	phy->pll_ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(phy->pll_ctrl_base))
		return PTR_ERR(phy->pll_ctrl_base);

	phy->dev		= &pdev->dev;

	phy->wkupclk = devm_clk_get(phy->dev, "usb_phy_cm_clk32k");
	if (IS_ERR(phy->wkupclk)) {
		dev_err(&pdev->dev, "unable to get usb_phy_cm_clk32k\n");
		return PTR_ERR(phy->wkupclk);
	}
	clk_prepare(phy->wkupclk);

	phy->optclk = devm_clk_get(phy->dev, "usb_otg_ss_refclk960m");
	if (IS_ERR(phy->optclk)) {
		dev_err(&pdev->dev, "unable to get usb_otg_ss_refclk960m\n");
		return PTR_ERR(phy->optclk);
	}
	clk_prepare(phy->optclk);

	phy->sys_clk = devm_clk_get(phy->dev, "sys_clkin");
	if (IS_ERR(phy->sys_clk)) {
		pr_err("%s: unable to get sys_clkin\n", __func__);
		return -EINVAL;
	}

	control_node = of_parse_phandle(node, "ctrl-module", 0);
	if (!control_node) {
		dev_err(&pdev->dev, "Failed to get control device phandle\n");
		return -EINVAL;
	}

	control_pdev = of_find_device_by_node(control_node);
	if (!control_pdev) {
		dev_err(&pdev->dev, "Failed to get control device\n");
		return -EINVAL;
	}

	phy->control_dev = &control_pdev->dev;

	omap_control_phy_power(phy->control_dev, 0);

	platform_set_drvdata(pdev, phy);
	pm_runtime_enable(phy->dev);

	generic_phy = devm_phy_create(phy->dev, &ops, NULL);
	if (IS_ERR(generic_phy))
		return PTR_ERR(generic_phy);

	phy_set_drvdata(generic_phy, phy);
	phy_provider = devm_of_phy_provider_register(phy->dev,
			of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	pm_runtime_get(&pdev->dev);

	return 0;
}

static int ti_pipe3_remove(struct platform_device *pdev)
{
	struct ti_pipe3 *phy = platform_get_drvdata(pdev);

	clk_unprepare(phy->wkupclk);
	clk_unprepare(phy->optclk);
	if (!pm_runtime_suspended(&pdev->dev))
		pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static int ti_pipe3_runtime_suspend(struct device *dev)
{
	struct ti_pipe3	*phy = dev_get_drvdata(dev);

	clk_disable(phy->wkupclk);
	clk_disable(phy->optclk);

	return 0;
}

static int ti_pipe3_runtime_resume(struct device *dev)
{
	u32 ret = 0;
	struct ti_pipe3	*phy = dev_get_drvdata(dev);

	ret = clk_enable(phy->optclk);
	if (ret) {
		dev_err(phy->dev, "Failed to enable optclk %d\n", ret);
		goto err1;
	}

	ret = clk_enable(phy->wkupclk);
	if (ret) {
		dev_err(phy->dev, "Failed to enable wkupclk %d\n", ret);
		goto err2;
	}

	return 0;

err2:
	clk_disable(phy->optclk);

err1:
	return ret;
}

static const struct dev_pm_ops ti_pipe3_pm_ops = {
	SET_RUNTIME_PM_OPS(ti_pipe3_runtime_suspend,
			   ti_pipe3_runtime_resume, NULL)
};

#define DEV_PM_OPS     (&ti_pipe3_pm_ops)
#else
#define DEV_PM_OPS     NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id ti_pipe3_id_table[] = {
	{ .compatible = "ti,phy-usb3" },
	{ .compatible = "ti,omap-usb3" },
	{}
};
MODULE_DEVICE_TABLE(of, ti_pipe3_id_table);
#endif

static struct platform_driver ti_pipe3_driver = {
	.probe		= ti_pipe3_probe,
	.remove		= ti_pipe3_remove,
	.driver		= {
		.name	= "ti-pipe3",
		.owner	= THIS_MODULE,
		.pm	= DEV_PM_OPS,
		.of_match_table = of_match_ptr(ti_pipe3_id_table),
	},
};

module_platform_driver(ti_pipe3_driver);

MODULE_ALIAS("platform: ti_pipe3");
MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TI PIPE3 phy driver");
MODULE_LICENSE("GPL v2");
