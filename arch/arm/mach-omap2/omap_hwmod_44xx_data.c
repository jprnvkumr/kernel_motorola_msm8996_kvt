/*
 * Hardware modules present on the OMAP44xx chips
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Paul Walmsley
 * Benoit Cousson
 *
 * This file is automatically generated from the OMAP hardware databases.
 * We respectfully ask that any modifications to this file be coordinated
 * with the public linux-omap@vger.kernel.org mailing list and the
 * authors above to ensure that the autogeneration scripts are kept
 * up-to-date with the file contents.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>

#include <plat/omap_hwmod.h>
#include <plat/cpu.h>

#include "omap_hwmod_common_data.h"

#include "cm.h"
#include "prm-regbits-44xx.h"

/* Base offset for all OMAP4 interrupts external to MPUSS */
#define OMAP44XX_IRQ_GIC_START	32

/* Base offset for all OMAP4 dma requests */
#define OMAP44XX_DMA_REQ_START  1

/* Backward references (IPs with Bus Master capability) */
static struct omap_hwmod omap44xx_dmm_hwmod;
static struct omap_hwmod omap44xx_emif_fw_hwmod;
static struct omap_hwmod omap44xx_l3_instr_hwmod;
static struct omap_hwmod omap44xx_l3_main_1_hwmod;
static struct omap_hwmod omap44xx_l3_main_2_hwmod;
static struct omap_hwmod omap44xx_l3_main_3_hwmod;
static struct omap_hwmod omap44xx_l4_abe_hwmod;
static struct omap_hwmod omap44xx_l4_cfg_hwmod;
static struct omap_hwmod omap44xx_l4_per_hwmod;
static struct omap_hwmod omap44xx_l4_wkup_hwmod;
static struct omap_hwmod omap44xx_mpu_hwmod;
static struct omap_hwmod omap44xx_mpu_private_hwmod;

/*
 * Interconnects omap_hwmod structures
 * hwmods that compose the global OMAP interconnect
 */

/*
 * 'dmm' class
 * instance(s): dmm
 */
static struct omap_hwmod_class omap44xx_dmm_hwmod_class = {
	.name = "dmm",
};

/* dmm interface data */
/* l3_main_1 -> dmm */
static struct omap_hwmod_ocp_if omap44xx_l3_main_1__dmm = {
	.master		= &omap44xx_l3_main_1_hwmod,
	.slave		= &omap44xx_dmm_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mpu -> dmm */
static struct omap_hwmod_ocp_if omap44xx_mpu__dmm = {
	.master		= &omap44xx_mpu_hwmod,
	.slave		= &omap44xx_dmm_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* dmm slave ports */
static struct omap_hwmod_ocp_if *omap44xx_dmm_slaves[] = {
	&omap44xx_l3_main_1__dmm,
	&omap44xx_mpu__dmm,
};

static struct omap_hwmod_irq_info omap44xx_dmm_irqs[] = {
	{ .irq = 113 + OMAP44XX_IRQ_GIC_START },
};

static struct omap_hwmod omap44xx_dmm_hwmod = {
	.name		= "dmm",
	.class		= &omap44xx_dmm_hwmod_class,
	.slaves		= omap44xx_dmm_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_dmm_slaves),
	.mpu_irqs	= omap44xx_dmm_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap44xx_dmm_irqs),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/*
 * 'emif_fw' class
 * instance(s): emif_fw
 */
static struct omap_hwmod_class omap44xx_emif_fw_hwmod_class = {
	.name = "emif_fw",
};

/* emif_fw interface data */
/* dmm -> emif_fw */
static struct omap_hwmod_ocp_if omap44xx_dmm__emif_fw = {
	.master		= &omap44xx_dmm_hwmod,
	.slave		= &omap44xx_emif_fw_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_cfg -> emif_fw */
static struct omap_hwmod_ocp_if omap44xx_l4_cfg__emif_fw = {
	.master		= &omap44xx_l4_cfg_hwmod,
	.slave		= &omap44xx_emif_fw_hwmod,
	.clk		= "l4_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* emif_fw slave ports */
static struct omap_hwmod_ocp_if *omap44xx_emif_fw_slaves[] = {
	&omap44xx_dmm__emif_fw,
	&omap44xx_l4_cfg__emif_fw,
};

static struct omap_hwmod omap44xx_emif_fw_hwmod = {
	.name		= "emif_fw",
	.class		= &omap44xx_emif_fw_hwmod_class,
	.slaves		= omap44xx_emif_fw_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_emif_fw_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/*
 * 'l3' class
 * instance(s): l3_instr, l3_main_1, l3_main_2, l3_main_3
 */
static struct omap_hwmod_class omap44xx_l3_hwmod_class = {
	.name = "l3",
};

/* l3_instr interface data */
/* l3_main_3 -> l3_instr */
static struct omap_hwmod_ocp_if omap44xx_l3_main_3__l3_instr = {
	.master		= &omap44xx_l3_main_3_hwmod,
	.slave		= &omap44xx_l3_instr_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l3_instr slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l3_instr_slaves[] = {
	&omap44xx_l3_main_3__l3_instr,
};

static struct omap_hwmod omap44xx_l3_instr_hwmod = {
	.name		= "l3_instr",
	.class		= &omap44xx_l3_hwmod_class,
	.slaves		= omap44xx_l3_instr_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l3_instr_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l3_main_2 -> l3_main_1 */
static struct omap_hwmod_ocp_if omap44xx_l3_main_2__l3_main_1 = {
	.master		= &omap44xx_l3_main_2_hwmod,
	.slave		= &omap44xx_l3_main_1_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_cfg -> l3_main_1 */
static struct omap_hwmod_ocp_if omap44xx_l4_cfg__l3_main_1 = {
	.master		= &omap44xx_l4_cfg_hwmod,
	.slave		= &omap44xx_l3_main_1_hwmod,
	.clk		= "l4_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mpu -> l3_main_1 */
static struct omap_hwmod_ocp_if omap44xx_mpu__l3_main_1 = {
	.master		= &omap44xx_mpu_hwmod,
	.slave		= &omap44xx_l3_main_1_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l3_main_1 slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l3_main_1_slaves[] = {
	&omap44xx_l3_main_2__l3_main_1,
	&omap44xx_l4_cfg__l3_main_1,
	&omap44xx_mpu__l3_main_1,
};

static struct omap_hwmod omap44xx_l3_main_1_hwmod = {
	.name		= "l3_main_1",
	.class		= &omap44xx_l3_hwmod_class,
	.slaves		= omap44xx_l3_main_1_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l3_main_1_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l3_main_2 interface data */
/* l3_main_1 -> l3_main_2 */
static struct omap_hwmod_ocp_if omap44xx_l3_main_1__l3_main_2 = {
	.master		= &omap44xx_l3_main_1_hwmod,
	.slave		= &omap44xx_l3_main_2_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_cfg -> l3_main_2 */
static struct omap_hwmod_ocp_if omap44xx_l4_cfg__l3_main_2 = {
	.master		= &omap44xx_l4_cfg_hwmod,
	.slave		= &omap44xx_l3_main_2_hwmod,
	.clk		= "l4_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l3_main_2 slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l3_main_2_slaves[] = {
	&omap44xx_l3_main_1__l3_main_2,
	&omap44xx_l4_cfg__l3_main_2,
};

static struct omap_hwmod omap44xx_l3_main_2_hwmod = {
	.name		= "l3_main_2",
	.class		= &omap44xx_l3_hwmod_class,
	.slaves		= omap44xx_l3_main_2_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l3_main_2_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l3_main_3 interface data */
/* l3_main_1 -> l3_main_3 */
static struct omap_hwmod_ocp_if omap44xx_l3_main_1__l3_main_3 = {
	.master		= &omap44xx_l3_main_1_hwmod,
	.slave		= &omap44xx_l3_main_3_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l3_main_2 -> l3_main_3 */
static struct omap_hwmod_ocp_if omap44xx_l3_main_2__l3_main_3 = {
	.master		= &omap44xx_l3_main_2_hwmod,
	.slave		= &omap44xx_l3_main_3_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_cfg -> l3_main_3 */
static struct omap_hwmod_ocp_if omap44xx_l4_cfg__l3_main_3 = {
	.master		= &omap44xx_l4_cfg_hwmod,
	.slave		= &omap44xx_l3_main_3_hwmod,
	.clk		= "l4_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l3_main_3 slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l3_main_3_slaves[] = {
	&omap44xx_l3_main_1__l3_main_3,
	&omap44xx_l3_main_2__l3_main_3,
	&omap44xx_l4_cfg__l3_main_3,
};

static struct omap_hwmod omap44xx_l3_main_3_hwmod = {
	.name		= "l3_main_3",
	.class		= &omap44xx_l3_hwmod_class,
	.slaves		= omap44xx_l3_main_3_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l3_main_3_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/*
 * 'l4' class
 * instance(s): l4_abe, l4_cfg, l4_per, l4_wkup
 */
static struct omap_hwmod_class omap44xx_l4_hwmod_class = {
	.name = "l4",
};

/* l4_abe interface data */
/* l3_main_1 -> l4_abe */
static struct omap_hwmod_ocp_if omap44xx_l3_main_1__l4_abe = {
	.master		= &omap44xx_l3_main_1_hwmod,
	.slave		= &omap44xx_l4_abe_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mpu -> l4_abe */
static struct omap_hwmod_ocp_if omap44xx_mpu__l4_abe = {
	.master		= &omap44xx_mpu_hwmod,
	.slave		= &omap44xx_l4_abe_hwmod,
	.clk		= "ocp_abe_iclk",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_abe slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l4_abe_slaves[] = {
	&omap44xx_l3_main_1__l4_abe,
	&omap44xx_mpu__l4_abe,
};

static struct omap_hwmod omap44xx_l4_abe_hwmod = {
	.name		= "l4_abe",
	.class		= &omap44xx_l4_hwmod_class,
	.slaves		= omap44xx_l4_abe_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l4_abe_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l4_cfg interface data */
/* l3_main_1 -> l4_cfg */
static struct omap_hwmod_ocp_if omap44xx_l3_main_1__l4_cfg = {
	.master		= &omap44xx_l3_main_1_hwmod,
	.slave		= &omap44xx_l4_cfg_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_cfg slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l4_cfg_slaves[] = {
	&omap44xx_l3_main_1__l4_cfg,
};

static struct omap_hwmod omap44xx_l4_cfg_hwmod = {
	.name		= "l4_cfg",
	.class		= &omap44xx_l4_hwmod_class,
	.slaves		= omap44xx_l4_cfg_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l4_cfg_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l4_per interface data */
/* l3_main_2 -> l4_per */
static struct omap_hwmod_ocp_if omap44xx_l3_main_2__l4_per = {
	.master		= &omap44xx_l3_main_2_hwmod,
	.slave		= &omap44xx_l4_per_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_per slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l4_per_slaves[] = {
	&omap44xx_l3_main_2__l4_per,
};

static struct omap_hwmod omap44xx_l4_per_hwmod = {
	.name		= "l4_per",
	.class		= &omap44xx_l4_hwmod_class,
	.slaves		= omap44xx_l4_per_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l4_per_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/* l4_wkup interface data */
/* l4_cfg -> l4_wkup */
static struct omap_hwmod_ocp_if omap44xx_l4_cfg__l4_wkup = {
	.master		= &omap44xx_l4_cfg_hwmod,
	.slave		= &omap44xx_l4_wkup_hwmod,
	.clk		= "l4_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* l4_wkup slave ports */
static struct omap_hwmod_ocp_if *omap44xx_l4_wkup_slaves[] = {
	&omap44xx_l4_cfg__l4_wkup,
};

static struct omap_hwmod omap44xx_l4_wkup_hwmod = {
	.name		= "l4_wkup",
	.class		= &omap44xx_l4_hwmod_class,
	.slaves		= omap44xx_l4_wkup_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_l4_wkup_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/*
 * 'mpu_bus' class
 * instance(s): mpu_private
 */
static struct omap_hwmod_class omap44xx_mpu_bus_hwmod_class = {
	.name = "mpu_bus",
};

/* mpu_private interface data */
/* mpu -> mpu_private */
static struct omap_hwmod_ocp_if omap44xx_mpu__mpu_private = {
	.master		= &omap44xx_mpu_hwmod,
	.slave		= &omap44xx_mpu_private_hwmod,
	.clk		= "l3_div_ck",
	.user		= OCP_USER_MPU | OCP_USER_SDMA,
};

/* mpu_private slave ports */
static struct omap_hwmod_ocp_if *omap44xx_mpu_private_slaves[] = {
	&omap44xx_mpu__mpu_private,
};

static struct omap_hwmod omap44xx_mpu_private_hwmod = {
	.name		= "mpu_private",
	.class		= &omap44xx_mpu_bus_hwmod_class,
	.slaves		= omap44xx_mpu_private_slaves,
	.slaves_cnt	= ARRAY_SIZE(omap44xx_mpu_private_slaves),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

/*
 * 'mpu' class
 * mpu sub-system
 */

static struct omap_hwmod_class omap44xx_mpu_hwmod_class = {
	.name = "mpu",
};

/* mpu */
static struct omap_hwmod_irq_info omap44xx_mpu_irqs[] = {
	{ .name = "pl310", .irq = 0 + OMAP44XX_IRQ_GIC_START },
	{ .name = "cti0", .irq = 1 + OMAP44XX_IRQ_GIC_START },
	{ .name = "cti1", .irq = 2 + OMAP44XX_IRQ_GIC_START },
};

/* mpu master ports */
static struct omap_hwmod_ocp_if *omap44xx_mpu_masters[] = {
	&omap44xx_mpu__l3_main_1,
	&omap44xx_mpu__l4_abe,
	&omap44xx_mpu__dmm,
};

static struct omap_hwmod omap44xx_mpu_hwmod = {
	.name		= "mpu",
	.class		= &omap44xx_mpu_hwmod_class,
	.flags		= (HWMOD_INIT_NO_IDLE | HWMOD_INIT_NO_RESET),
	.mpu_irqs	= omap44xx_mpu_irqs,
	.mpu_irqs_cnt	= ARRAY_SIZE(omap44xx_mpu_irqs),
	.main_clk	= "dpll_mpu_m2_ck",
	.prcm = {
		.omap4 = {
			.clkctrl_reg = OMAP4430_CM_MPU_MPU_CLKCTRL,
		},
	},
	.masters	= omap44xx_mpu_masters,
	.masters_cnt	= ARRAY_SIZE(omap44xx_mpu_masters),
	.omap_chip	= OMAP_CHIP_INIT(CHIP_IS_OMAP4430),
};

static __initdata struct omap_hwmod *omap44xx_hwmods[] = {
	/* dmm class */
	&omap44xx_dmm_hwmod,
	/* emif_fw class */
	&omap44xx_emif_fw_hwmod,
	/* l3 class */
	&omap44xx_l3_instr_hwmod,
	&omap44xx_l3_main_1_hwmod,
	&omap44xx_l3_main_2_hwmod,
	&omap44xx_l3_main_3_hwmod,
	/* l4 class */
	&omap44xx_l4_abe_hwmod,
	&omap44xx_l4_cfg_hwmod,
	&omap44xx_l4_per_hwmod,
	&omap44xx_l4_wkup_hwmod,
	/* mpu_bus class */
	&omap44xx_mpu_private_hwmod,

	/* mpu class */
	&omap44xx_mpu_hwmod,
	NULL,
};

int __init omap44xx_hwmod_init(void)
{
	return omap_hwmod_init(omap44xx_hwmods);
}

