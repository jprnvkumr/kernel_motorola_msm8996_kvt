/*
 * OMAP2/3 CM module functions
 *
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>

#include <plat/common.h>

#include "cm.h"
#include "cm2xxx_3xxx.h"
#include "cm-regbits-24xx.h"
#include "cm-regbits-34xx.h"

static const u8 cm_idlest_offs[] = {
	CM_IDLEST1, CM_IDLEST2, OMAP2430_CM_IDLEST3
};

u32 cm_read_mod_reg(s16 module, u16 idx)
{
	return __raw_readl(cm_base + module + idx);
}

void cm_write_mod_reg(u32 val, s16 module, u16 idx)
{
	__raw_writel(val, cm_base + module + idx);
}

/* Read-modify-write a register in a CM module. Caller must lock */
u32 cm_rmw_mod_reg_bits(u32 mask, u32 bits, s16 module, s16 idx)
{
	u32 v;

	v = cm_read_mod_reg(module, idx);
	v &= ~mask;
	v |= bits;
	cm_write_mod_reg(v, module, idx);

	return v;
}

u32 cm_set_mod_reg_bits(u32 bits, s16 module, s16 idx)
{
	return cm_rmw_mod_reg_bits(bits, bits, module, idx);
}

u32 cm_clear_mod_reg_bits(u32 bits, s16 module, s16 idx)
{
	return cm_rmw_mod_reg_bits(bits, 0x0, module, idx);
}

/**
 * omap2_cm_wait_idlest_ready - wait for a module to leave idle or standby
 * @prcm_mod: PRCM module offset
 * @idlest_id: CM_IDLESTx register ID (i.e., x = 1, 2, 3)
 * @idlest_shift: shift of the bit in the CM_IDLEST* register to check
 *
 * XXX document
 */
int omap2_cm_wait_module_ready(s16 prcm_mod, u8 idlest_id, u8 idlest_shift)
{
	int ena = 0, i = 0;
	u8 cm_idlest_reg;
	u32 mask;

	if (!idlest_id || (idlest_id > ARRAY_SIZE(cm_idlest_offs)))
		return -EINVAL;

	cm_idlest_reg = cm_idlest_offs[idlest_id - 1];

	mask = 1 << idlest_shift;

	if (cpu_is_omap24xx())
		ena = mask;
	else if (cpu_is_omap34xx())
		ena = 0;
	else
		BUG();

	omap_test_timeout(((cm_read_mod_reg(prcm_mod, cm_idlest_reg) & mask) == ena),
			  MAX_MODULE_READY_TIME, i);

	return (i < MAX_MODULE_READY_TIME) ? 0 : -EBUSY;
}

/*
 * Context save/restore code - OMAP3 only
 */
#ifdef CONFIG_ARCH_OMAP3
struct omap3_cm_regs {
	u32 iva2_cm_clksel1;
	u32 iva2_cm_clksel2;
	u32 cm_sysconfig;
	u32 sgx_cm_clksel;
	u32 dss_cm_clksel;
	u32 cam_cm_clksel;
	u32 per_cm_clksel;
	u32 emu_cm_clksel;
	u32 emu_cm_clkstctrl;
	u32 pll_cm_autoidle2;
	u32 pll_cm_clksel4;
	u32 pll_cm_clksel5;
	u32 pll_cm_clken2;
	u32 cm_polctrl;
	u32 iva2_cm_fclken;
	u32 iva2_cm_clken_pll;
	u32 core_cm_fclken1;
	u32 core_cm_fclken3;
	u32 sgx_cm_fclken;
	u32 wkup_cm_fclken;
	u32 dss_cm_fclken;
	u32 cam_cm_fclken;
	u32 per_cm_fclken;
	u32 usbhost_cm_fclken;
	u32 core_cm_iclken1;
	u32 core_cm_iclken2;
	u32 core_cm_iclken3;
	u32 sgx_cm_iclken;
	u32 wkup_cm_iclken;
	u32 dss_cm_iclken;
	u32 cam_cm_iclken;
	u32 per_cm_iclken;
	u32 usbhost_cm_iclken;
	u32 iva2_cm_autoidle2;
	u32 mpu_cm_autoidle2;
	u32 iva2_cm_clkstctrl;
	u32 mpu_cm_clkstctrl;
	u32 core_cm_clkstctrl;
	u32 sgx_cm_clkstctrl;
	u32 dss_cm_clkstctrl;
	u32 cam_cm_clkstctrl;
	u32 per_cm_clkstctrl;
	u32 neon_cm_clkstctrl;
	u32 usbhost_cm_clkstctrl;
	u32 core_cm_autoidle1;
	u32 core_cm_autoidle2;
	u32 core_cm_autoidle3;
	u32 wkup_cm_autoidle;
	u32 dss_cm_autoidle;
	u32 cam_cm_autoidle;
	u32 per_cm_autoidle;
	u32 usbhost_cm_autoidle;
	u32 sgx_cm_sleepdep;
	u32 dss_cm_sleepdep;
	u32 cam_cm_sleepdep;
	u32 per_cm_sleepdep;
	u32 usbhost_cm_sleepdep;
	u32 cm_clkout_ctrl;
};

static struct omap3_cm_regs cm_context;

void omap3_cm_save_context(void)
{
	cm_context.iva2_cm_clksel1 =
		cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_CLKSEL1);
	cm_context.iva2_cm_clksel2 =
		cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_CLKSEL2);
	cm_context.cm_sysconfig = __raw_readl(OMAP3430_CM_SYSCONFIG);
	cm_context.sgx_cm_clksel =
		cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_CLKSEL);
	cm_context.dss_cm_clksel =
		cm_read_mod_reg(OMAP3430_DSS_MOD, CM_CLKSEL);
	cm_context.cam_cm_clksel =
		cm_read_mod_reg(OMAP3430_CAM_MOD, CM_CLKSEL);
	cm_context.per_cm_clksel =
		cm_read_mod_reg(OMAP3430_PER_MOD, CM_CLKSEL);
	cm_context.emu_cm_clksel =
		cm_read_mod_reg(OMAP3430_EMU_MOD, CM_CLKSEL1);
	cm_context.emu_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_EMU_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.pll_cm_autoidle2 =
		cm_read_mod_reg(PLL_MOD, CM_AUTOIDLE2);
	cm_context.pll_cm_clksel4 =
		cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKSEL4);
	cm_context.pll_cm_clksel5 =
		cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKSEL5);
	cm_context.pll_cm_clken2 =
		cm_read_mod_reg(PLL_MOD, OMAP3430ES2_CM_CLKEN2);
	cm_context.cm_polctrl = __raw_readl(OMAP3430_CM_POLCTRL);
	cm_context.iva2_cm_fclken =
		cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_FCLKEN);
	cm_context.iva2_cm_clken_pll = cm_read_mod_reg(OMAP3430_IVA2_MOD,
						       OMAP3430_CM_CLKEN_PLL);
	cm_context.core_cm_fclken1 =
		cm_read_mod_reg(CORE_MOD, CM_FCLKEN1);
	cm_context.core_cm_fclken3 =
		cm_read_mod_reg(CORE_MOD, OMAP3430ES2_CM_FCLKEN3);
	cm_context.sgx_cm_fclken =
		cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_FCLKEN);
	cm_context.wkup_cm_fclken =
		cm_read_mod_reg(WKUP_MOD, CM_FCLKEN);
	cm_context.dss_cm_fclken =
		cm_read_mod_reg(OMAP3430_DSS_MOD, CM_FCLKEN);
	cm_context.cam_cm_fclken =
		cm_read_mod_reg(OMAP3430_CAM_MOD, CM_FCLKEN);
	cm_context.per_cm_fclken =
		cm_read_mod_reg(OMAP3430_PER_MOD, CM_FCLKEN);
	cm_context.usbhost_cm_fclken =
		cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_FCLKEN);
	cm_context.core_cm_iclken1 =
		cm_read_mod_reg(CORE_MOD, CM_ICLKEN1);
	cm_context.core_cm_iclken2 =
		cm_read_mod_reg(CORE_MOD, CM_ICLKEN2);
	cm_context.core_cm_iclken3 =
		cm_read_mod_reg(CORE_MOD, CM_ICLKEN3);
	cm_context.sgx_cm_iclken =
		cm_read_mod_reg(OMAP3430ES2_SGX_MOD, CM_ICLKEN);
	cm_context.wkup_cm_iclken =
		cm_read_mod_reg(WKUP_MOD, CM_ICLKEN);
	cm_context.dss_cm_iclken =
		cm_read_mod_reg(OMAP3430_DSS_MOD, CM_ICLKEN);
	cm_context.cam_cm_iclken =
		cm_read_mod_reg(OMAP3430_CAM_MOD, CM_ICLKEN);
	cm_context.per_cm_iclken =
		cm_read_mod_reg(OMAP3430_PER_MOD, CM_ICLKEN);
	cm_context.usbhost_cm_iclken =
		cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_ICLKEN);
	cm_context.iva2_cm_autoidle2 =
		cm_read_mod_reg(OMAP3430_IVA2_MOD, CM_AUTOIDLE2);
	cm_context.mpu_cm_autoidle2 =
		cm_read_mod_reg(MPU_MOD, CM_AUTOIDLE2);
	cm_context.iva2_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_IVA2_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.mpu_cm_clkstctrl =
		cm_read_mod_reg(MPU_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.core_cm_clkstctrl =
		cm_read_mod_reg(CORE_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.sgx_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430ES2_SGX_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.dss_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_DSS_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.cam_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_CAM_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.per_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_PER_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.neon_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430_NEON_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.usbhost_cm_clkstctrl =
		cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, OMAP2_CM_CLKSTCTRL);
	cm_context.core_cm_autoidle1 =
		cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE1);
	cm_context.core_cm_autoidle2 =
		cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE2);
	cm_context.core_cm_autoidle3 =
		cm_read_mod_reg(CORE_MOD, CM_AUTOIDLE3);
	cm_context.wkup_cm_autoidle =
		cm_read_mod_reg(WKUP_MOD, CM_AUTOIDLE);
	cm_context.dss_cm_autoidle =
		cm_read_mod_reg(OMAP3430_DSS_MOD, CM_AUTOIDLE);
	cm_context.cam_cm_autoidle =
		cm_read_mod_reg(OMAP3430_CAM_MOD, CM_AUTOIDLE);
	cm_context.per_cm_autoidle =
		cm_read_mod_reg(OMAP3430_PER_MOD, CM_AUTOIDLE);
	cm_context.usbhost_cm_autoidle =
		cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, CM_AUTOIDLE);
	cm_context.sgx_cm_sleepdep =
		cm_read_mod_reg(OMAP3430ES2_SGX_MOD, OMAP3430_CM_SLEEPDEP);
	cm_context.dss_cm_sleepdep =
		cm_read_mod_reg(OMAP3430_DSS_MOD, OMAP3430_CM_SLEEPDEP);
	cm_context.cam_cm_sleepdep =
		cm_read_mod_reg(OMAP3430_CAM_MOD, OMAP3430_CM_SLEEPDEP);
	cm_context.per_cm_sleepdep =
		cm_read_mod_reg(OMAP3430_PER_MOD, OMAP3430_CM_SLEEPDEP);
	cm_context.usbhost_cm_sleepdep =
		cm_read_mod_reg(OMAP3430ES2_USBHOST_MOD, OMAP3430_CM_SLEEPDEP);
	cm_context.cm_clkout_ctrl =
		cm_read_mod_reg(OMAP3430_CCR_MOD, OMAP3_CM_CLKOUT_CTRL_OFFSET);
}

void omap3_cm_restore_context(void)
{
	cm_write_mod_reg(cm_context.iva2_cm_clksel1, OMAP3430_IVA2_MOD,
			 CM_CLKSEL1);
	cm_write_mod_reg(cm_context.iva2_cm_clksel2, OMAP3430_IVA2_MOD,
			 CM_CLKSEL2);
	__raw_writel(cm_context.cm_sysconfig, OMAP3430_CM_SYSCONFIG);
	cm_write_mod_reg(cm_context.sgx_cm_clksel, OMAP3430ES2_SGX_MOD,
			 CM_CLKSEL);
	cm_write_mod_reg(cm_context.dss_cm_clksel, OMAP3430_DSS_MOD,
			 CM_CLKSEL);
	cm_write_mod_reg(cm_context.cam_cm_clksel, OMAP3430_CAM_MOD,
			 CM_CLKSEL);
	cm_write_mod_reg(cm_context.per_cm_clksel, OMAP3430_PER_MOD,
			 CM_CLKSEL);
	cm_write_mod_reg(cm_context.emu_cm_clksel, OMAP3430_EMU_MOD,
			 CM_CLKSEL1);
	cm_write_mod_reg(cm_context.emu_cm_clkstctrl, OMAP3430_EMU_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.pll_cm_autoidle2, PLL_MOD,
			 CM_AUTOIDLE2);
	cm_write_mod_reg(cm_context.pll_cm_clksel4, PLL_MOD,
			 OMAP3430ES2_CM_CLKSEL4);
	cm_write_mod_reg(cm_context.pll_cm_clksel5, PLL_MOD,
			 OMAP3430ES2_CM_CLKSEL5);
	cm_write_mod_reg(cm_context.pll_cm_clken2, PLL_MOD,
			 OMAP3430ES2_CM_CLKEN2);
	__raw_writel(cm_context.cm_polctrl, OMAP3430_CM_POLCTRL);
	cm_write_mod_reg(cm_context.iva2_cm_fclken, OMAP3430_IVA2_MOD,
			 CM_FCLKEN);
	cm_write_mod_reg(cm_context.iva2_cm_clken_pll, OMAP3430_IVA2_MOD,
			 OMAP3430_CM_CLKEN_PLL);
	cm_write_mod_reg(cm_context.core_cm_fclken1, CORE_MOD, CM_FCLKEN1);
	cm_write_mod_reg(cm_context.core_cm_fclken3, CORE_MOD,
			 OMAP3430ES2_CM_FCLKEN3);
	cm_write_mod_reg(cm_context.sgx_cm_fclken, OMAP3430ES2_SGX_MOD,
			 CM_FCLKEN);
	cm_write_mod_reg(cm_context.wkup_cm_fclken, WKUP_MOD, CM_FCLKEN);
	cm_write_mod_reg(cm_context.dss_cm_fclken, OMAP3430_DSS_MOD,
			 CM_FCLKEN);
	cm_write_mod_reg(cm_context.cam_cm_fclken, OMAP3430_CAM_MOD,
			 CM_FCLKEN);
	cm_write_mod_reg(cm_context.per_cm_fclken, OMAP3430_PER_MOD,
			 CM_FCLKEN);
	cm_write_mod_reg(cm_context.usbhost_cm_fclken,
			 OMAP3430ES2_USBHOST_MOD, CM_FCLKEN);
	cm_write_mod_reg(cm_context.core_cm_iclken1, CORE_MOD, CM_ICLKEN1);
	cm_write_mod_reg(cm_context.core_cm_iclken2, CORE_MOD, CM_ICLKEN2);
	cm_write_mod_reg(cm_context.core_cm_iclken3, CORE_MOD, CM_ICLKEN3);
	cm_write_mod_reg(cm_context.sgx_cm_iclken, OMAP3430ES2_SGX_MOD,
			 CM_ICLKEN);
	cm_write_mod_reg(cm_context.wkup_cm_iclken, WKUP_MOD, CM_ICLKEN);
	cm_write_mod_reg(cm_context.dss_cm_iclken, OMAP3430_DSS_MOD,
			 CM_ICLKEN);
	cm_write_mod_reg(cm_context.cam_cm_iclken, OMAP3430_CAM_MOD,
			 CM_ICLKEN);
	cm_write_mod_reg(cm_context.per_cm_iclken, OMAP3430_PER_MOD,
			 CM_ICLKEN);
	cm_write_mod_reg(cm_context.usbhost_cm_iclken,
			 OMAP3430ES2_USBHOST_MOD, CM_ICLKEN);
	cm_write_mod_reg(cm_context.iva2_cm_autoidle2, OMAP3430_IVA2_MOD,
			 CM_AUTOIDLE2);
	cm_write_mod_reg(cm_context.mpu_cm_autoidle2, MPU_MOD, CM_AUTOIDLE2);
	cm_write_mod_reg(cm_context.iva2_cm_clkstctrl, OMAP3430_IVA2_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.mpu_cm_clkstctrl, MPU_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.core_cm_clkstctrl, CORE_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.sgx_cm_clkstctrl, OMAP3430ES2_SGX_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.dss_cm_clkstctrl, OMAP3430_DSS_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.cam_cm_clkstctrl, OMAP3430_CAM_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.per_cm_clkstctrl, OMAP3430_PER_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.neon_cm_clkstctrl, OMAP3430_NEON_MOD,
			 OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.usbhost_cm_clkstctrl,
			 OMAP3430ES2_USBHOST_MOD, OMAP2_CM_CLKSTCTRL);
	cm_write_mod_reg(cm_context.core_cm_autoidle1, CORE_MOD,
			 CM_AUTOIDLE1);
	cm_write_mod_reg(cm_context.core_cm_autoidle2, CORE_MOD,
			 CM_AUTOIDLE2);
	cm_write_mod_reg(cm_context.core_cm_autoidle3, CORE_MOD,
			 CM_AUTOIDLE3);
	cm_write_mod_reg(cm_context.wkup_cm_autoidle, WKUP_MOD, CM_AUTOIDLE);
	cm_write_mod_reg(cm_context.dss_cm_autoidle, OMAP3430_DSS_MOD,
			 CM_AUTOIDLE);
	cm_write_mod_reg(cm_context.cam_cm_autoidle, OMAP3430_CAM_MOD,
			 CM_AUTOIDLE);
	cm_write_mod_reg(cm_context.per_cm_autoidle, OMAP3430_PER_MOD,
			 CM_AUTOIDLE);
	cm_write_mod_reg(cm_context.usbhost_cm_autoidle,
			 OMAP3430ES2_USBHOST_MOD, CM_AUTOIDLE);
	cm_write_mod_reg(cm_context.sgx_cm_sleepdep, OMAP3430ES2_SGX_MOD,
			 OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(cm_context.dss_cm_sleepdep, OMAP3430_DSS_MOD,
			 OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(cm_context.cam_cm_sleepdep, OMAP3430_CAM_MOD,
			 OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(cm_context.per_cm_sleepdep, OMAP3430_PER_MOD,
			 OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(cm_context.usbhost_cm_sleepdep,
			 OMAP3430ES2_USBHOST_MOD, OMAP3430_CM_SLEEPDEP);
	cm_write_mod_reg(cm_context.cm_clkout_ctrl, OMAP3430_CCR_MOD,
			 OMAP3_CM_CLKOUT_CTRL_OFFSET);
}
#endif
