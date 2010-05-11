/*
 * arch/sh/kernel/cpu/sh4a/clock-sh7786.c
 *
 * SH7786 support for the clock framework
 *
 *  Copyright (C) 2010  Paul Mundt
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <asm/clkdev.h>
#include <asm/clock.h>
#include <asm/freq.h>

/*
 * Default rate for the root input clock, reset this with clk_set_rate()
 * from the platform code.
 */
static struct clk extal_clk = {
	.name		= "extal",
	.id		= -1,
	.rate		= 33333333,
};

static unsigned long pll_recalc(struct clk *clk)
{
	int multiplier;

	/*
	 * Clock modes 0, 1, and 2 use an x64 multiplier against PLL1,
	 * while modes 3, 4, and 5 use an x32.
	 */
	multiplier = (sh_mv.mv_mode_pins() & 0xf) < 3 ? 64 : 32;

	return clk->parent->rate * multiplier;
}

static struct clk_ops pll_clk_ops = {
	.recalc		= pll_recalc,
};

static struct clk pll_clk = {
	.name		= "pll_clk",
	.id		= -1,
	.ops		= &pll_clk_ops,
	.parent		= &extal_clk,
	.flags		= CLK_ENABLE_ON_INIT,
};

static struct clk *clks[] = {
	&extal_clk,
	&pll_clk,
};

static unsigned int div2[] = { 1, 2, 4, 6, 8, 12, 16, 18,
			       24, 32, 36, 48 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors = div2,
	.nr_divisors = ARRAY_SIZE(div2),
};

static struct clk_div4_table div4_table = {
	.div_mult_table = &div4_div_mult_table,
};

enum { DIV4_I, DIV4_SH, DIV4_B, DIV4_DDR, DIV4_DU, DIV4_P, DIV4_NR };

#define DIV4(_str, _bit, _mask, _flags) \
  SH_CLK_DIV4(_str, &pll_clk, FRQMR1, _bit, _mask, _flags)

struct clk div4_clks[DIV4_NR] = {
	[DIV4_P] = DIV4("peripheral_clk", 0, 0x0b40, 0),
	[DIV4_DU] = DIV4("du_clk", 4, 0x0010, 0),
	[DIV4_DDR] = DIV4("ddr_clk", 12, 0x0002, CLK_ENABLE_ON_INIT),
	[DIV4_B] = DIV4("bus_clk", 16, 0x0360, CLK_ENABLE_ON_INIT),
	[DIV4_SH] = DIV4("shyway_clk", 20, 0x0002, CLK_ENABLE_ON_INIT),
	[DIV4_I] = DIV4("cpu_clk", 28, 0x0006, CLK_ENABLE_ON_INIT),
};

#define MSTPCR0		0xffc40030
#define MSTPCR1		0xffc40034

enum { MSTP029, MSTP028, MSTP027, MSTP026, MSTP025, MSTP024,
       MSTP023, MSTP022, MSTP021, MSTP020, MSTP017, MSTP016,
       MSTP015, MSTP014, MSTP011, MSTP010, MSTP009, MSTP008,
       MSTP005, MSTP004, MSTP002,
       MSTP112, MSTP110, MSTP109, MSTP108,
       MSTP105, MSTP104, MSTP103, MSTP102,
       MSTP_NR };

static struct clk mstp_clks[MSTP_NR] = {
	/* MSTPCR0 */
	[MSTP029] = SH_CLK_MSTP32("sci_fck", 5, &div4_clks[DIV4_P], MSTPCR0, 29, 0),
	[MSTP028] = SH_CLK_MSTP32("sci_fck", 4, &div4_clks[DIV4_P], MSTPCR0, 28, 0),
	[MSTP027] = SH_CLK_MSTP32("sci_fck", 3, &div4_clks[DIV4_P], MSTPCR0, 27, 0),
	[MSTP026] = SH_CLK_MSTP32("sci_fck", 2, &div4_clks[DIV4_P], MSTPCR0, 26, 0),
	[MSTP025] = SH_CLK_MSTP32("sci_fck", 1, &div4_clks[DIV4_P], MSTPCR0, 25, 0),
	[MSTP024] = SH_CLK_MSTP32("sci_fck", 0, &div4_clks[DIV4_P], MSTPCR0, 24, 0),
	[MSTP023] = SH_CLK_MSTP32("ssi_fck", 3, &div4_clks[DIV4_P], MSTPCR0, 23, 0),
	[MSTP022] = SH_CLK_MSTP32("ssi_fck", 2, &div4_clks[DIV4_P], MSTPCR0, 22, 0),
	[MSTP021] = SH_CLK_MSTP32("ssi_fck", 1, &div4_clks[DIV4_P], MSTPCR0, 21, 0),
	[MSTP020] = SH_CLK_MSTP32("ssi_fck", 0, &div4_clks[DIV4_P], MSTPCR0, 20, 0),
	[MSTP017] = SH_CLK_MSTP32("hac_fck", 1, &div4_clks[DIV4_P], MSTPCR0, 17, 0),
	[MSTP016] = SH_CLK_MSTP32("hac_fck", 0, &div4_clks[DIV4_P], MSTPCR0, 16, 0),
	[MSTP015] = SH_CLK_MSTP32("i2c_fck", 1, &div4_clks[DIV4_P], MSTPCR0, 15, 0),
	[MSTP014] = SH_CLK_MSTP32("i2c_fck", 0, &div4_clks[DIV4_P], MSTPCR0, 14, 0),
	[MSTP011] = SH_CLK_MSTP32("tmu9_11_fck", -1, &div4_clks[DIV4_P], MSTPCR0, 11, 0),
	[MSTP010] = SH_CLK_MSTP32("tmu678_fck", -1, &div4_clks[DIV4_P], MSTPCR0, 10, 0),
	[MSTP009] = SH_CLK_MSTP32("tmu345_fck", -1, &div4_clks[DIV4_P], MSTPCR0, 9, 0),
	[MSTP008] = SH_CLK_MSTP32("tmu012_fck", -1, &div4_clks[DIV4_P], MSTPCR0, 8, 0),
	[MSTP005] = SH_CLK_MSTP32("sdif_fck", 1, &div4_clks[DIV4_P], MSTPCR0, 5, 0),
	[MSTP004] = SH_CLK_MSTP32("sdif_fck", 0, &div4_clks[DIV4_P], MSTPCR0, 4, 0),
	[MSTP002] = SH_CLK_MSTP32("hspi_fck", -1, &div4_clks[DIV4_P], MSTPCR0, 2, 0),

	/* MSTPCR1 */
	[MSTP112] = SH_CLK_MSTP32("usb_fck", -1, NULL, MSTPCR1, 12, 0),
	[MSTP110] = SH_CLK_MSTP32("pcie_fck", 2, NULL, MSTPCR1, 10, 0),
	[MSTP109] = SH_CLK_MSTP32("pcie_fck", 1, NULL, MSTPCR1, 9, 0),
	[MSTP108] = SH_CLK_MSTP32("pcie_fck", 0, NULL, MSTPCR1, 8, 0),
	[MSTP105] = SH_CLK_MSTP32("dmac_11_6_fck", -1, NULL, MSTPCR1, 5, 0),
	[MSTP104] = SH_CLK_MSTP32("dmac_5_0_fck", -1, NULL, MSTPCR1, 4, 0),
	[MSTP103] = SH_CLK_MSTP32("du_fck", -1, NULL, MSTPCR1, 3, 0),
	[MSTP102] = SH_CLK_MSTP32("ether_fck", -1, NULL, MSTPCR1, 2, 0),
};

static struct clk_lookup lookups[] = {
	{
		/* TMU0 */
		.dev_id		= "sh_tmu.0",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP008],
	}, {
		/* TMU1 */
		.dev_id		= "sh_tmu.1",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP008],
	}, {
		/* TMU2 */
		.dev_id		= "sh_tmu.2",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP008],
	}, {
		/* TMU3 */
		.dev_id		= "sh_tmu.3",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP009],
	}, {
		/* TMU4 */
		.dev_id		= "sh_tmu.4",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP009],
	}, {
		/* TMU5 */
		.dev_id		= "sh_tmu.5",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP009],
	}, {
		/* TMU6 */
		.dev_id		= "sh_tmu.6",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP010],
	}, {
		/* TMU7 */
		.dev_id		= "sh_tmu.7",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP010],
	}, {
		/* TMU8 */
		.dev_id		= "sh_tmu.8",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP010],
	}, {
		/* TMU9 */
		.dev_id		= "sh_tmu.9",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP011],
	}, {
		/* TMU10 */
		.dev_id		= "sh_tmu.10",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP011],
	}, {
		/* TMU11 */
		.dev_id		= "sh_tmu.11",
		.con_id		= "tmu_fck",
		.clk		= &mstp_clks[MSTP011],
	}
};

int __init arch_clk_init(void)
{
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(clks); i++)
		ret |= clk_register(clks[i]);
	for (i = 0; i < ARRAY_SIZE(lookups); i++)
		clkdev_add(&lookups[i]);

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, ARRAY_SIZE(div4_clks),
					   &div4_table);
	if (!ret)
		ret = sh_clk_mstp32_register(mstp_clks, MSTP_NR);

	return ret;
}
