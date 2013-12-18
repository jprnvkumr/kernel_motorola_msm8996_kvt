/* linux/arch/arm/mach-exynos4/include/mach/regs-clock.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4 - Clock register definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_REGS_CLOCK_H
#define __ASM_ARCH_REGS_CLOCK_H __FILE__

#include <plat/cpu.h>
#include <mach/map.h>

#define EXYNOS_CLKREG(x)			(S5P_VA_CMU + (x))

#define EXYNOS4_CLKDIV_LEFTBUS			EXYNOS_CLKREG(0x04500)
#define EXYNOS4_CLKDIV_STAT_LEFTBUS		EXYNOS_CLKREG(0x04600)

#define EXYNOS4_CLKDIV_RIGHTBUS			EXYNOS_CLKREG(0x08500)
#define EXYNOS4_CLKDIV_STAT_RIGHTBUS		EXYNOS_CLKREG(0x08600)

#define EXYNOS4_EPLL_LOCK			EXYNOS_CLKREG(0x0C010)
#define EXYNOS4_VPLL_LOCK			EXYNOS_CLKREG(0x0C020)

#define EXYNOS4_EPLL_CON0			EXYNOS_CLKREG(0x0C110)
#define EXYNOS4_EPLL_CON1			EXYNOS_CLKREG(0x0C114)
#define EXYNOS4_VPLL_CON0			EXYNOS_CLKREG(0x0C120)
#define EXYNOS4_VPLL_CON1			EXYNOS_CLKREG(0x0C124)

#define EXYNOS4_CLKSRC_MASK_TOP			EXYNOS_CLKREG(0x0C310)
#define EXYNOS4_CLKSRC_MASK_CAM			EXYNOS_CLKREG(0x0C320)
#define EXYNOS4_CLKSRC_MASK_TV			EXYNOS_CLKREG(0x0C324)
#define EXYNOS4_CLKSRC_MASK_LCD0		EXYNOS_CLKREG(0x0C334)
#define EXYNOS4_CLKSRC_MASK_MAUDIO		EXYNOS_CLKREG(0x0C33C)
#define EXYNOS4_CLKSRC_MASK_FSYS		EXYNOS_CLKREG(0x0C340)
#define EXYNOS4_CLKSRC_MASK_PERIL0		EXYNOS_CLKREG(0x0C350)
#define EXYNOS4_CLKSRC_MASK_PERIL1		EXYNOS_CLKREG(0x0C354)

#define EXYNOS4_CLKDIV_TOP			EXYNOS_CLKREG(0x0C510)
#define EXYNOS4_CLKDIV_CAM			EXYNOS_CLKREG(0x0C520)
#define EXYNOS4_CLKDIV_MFC			EXYNOS_CLKREG(0x0C528)

#define EXYNOS4_CLKDIV_STAT_TOP			EXYNOS_CLKREG(0x0C610)
#define EXYNOS4_CLKDIV_STAT_MFC			EXYNOS_CLKREG(0x0C628)

#define EXYNOS4210_CLKGATE_IP_IMAGE		EXYNOS_CLKREG(0x0C930)
#define EXYNOS4212_CLKGATE_IP_IMAGE		EXYNOS_CLKREG(0x04930)

#define EXYNOS4_CLKSRC_MASK_DMC			EXYNOS_CLKREG(0x10300)
#define EXYNOS4_CLKDIV_DMC0			EXYNOS_CLKREG(0x10500)
#define EXYNOS4_CLKDIV_DMC1			EXYNOS_CLKREG(0x10504)
#define EXYNOS4_CLKDIV_STAT_DMC0		EXYNOS_CLKREG(0x10600)
#define EXYNOS4_CLKDIV_STAT_DMC1		EXYNOS_CLKREG(0x10604)

#define EXYNOS4_DMC_PAUSE_CTRL			EXYNOS_CLKREG(0x11094)
#define EXYNOS4_DMC_PAUSE_ENABLE		(1 << 0)

#define EXYNOS4_CLKSRC_CPU			EXYNOS_CLKREG(0x14200)
#define EXYNOS4_CLKMUX_STATCPU			EXYNOS_CLKREG(0x14400)

#define EXYNOS4_CLKDIV_CPU			EXYNOS_CLKREG(0x14500)
#define EXYNOS4_CLKDIV_CPU1			EXYNOS_CLKREG(0x14504)
#define EXYNOS4_CLKDIV_STATCPU			EXYNOS_CLKREG(0x14600)
#define EXYNOS4_CLKDIV_STATCPU1			EXYNOS_CLKREG(0x14604)

#define EXYNOS4_EPLLCON0_LOCKED_SHIFT		(29)
#define EXYNOS4_VPLLCON0_LOCKED_SHIFT		(29)

#define EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT	(16)
#define EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK	(0x7 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT)

#define EXYNOS4_CLKDIV_DMC0_ACP_SHIFT		(0)
#define EXYNOS4_CLKDIV_DMC0_ACP_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_ACP_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_ACPPCLK_SHIFT	(4)
#define EXYNOS4_CLKDIV_DMC0_ACPPCLK_MASK	(0x7 << EXYNOS4_CLKDIV_DMC0_ACPPCLK_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DPHY_SHIFT		(8)
#define EXYNOS4_CLKDIV_DMC0_DPHY_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DPHY_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMC_SHIFT		(12)
#define EXYNOS4_CLKDIV_DMC0_DMC_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMC_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMCD_SHIFT		(16)
#define EXYNOS4_CLKDIV_DMC0_DMCD_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMCD_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_DMCP_SHIFT		(20)
#define EXYNOS4_CLKDIV_DMC0_DMCP_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_DMCP_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_COPY2_SHIFT		(24)
#define EXYNOS4_CLKDIV_DMC0_COPY2_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_COPY2_SHIFT)
#define EXYNOS4_CLKDIV_DMC0_CORETI_SHIFT	(28)
#define EXYNOS4_CLKDIV_DMC0_CORETI_MASK		(0x7 << EXYNOS4_CLKDIV_DMC0_CORETI_SHIFT)

#define EXYNOS4_CLKDIV_DMC1_G2D_ACP_SHIFT	(0)
#define EXYNOS4_CLKDIV_DMC1_G2D_ACP_MASK	(0xf << EXYNOS4_CLKDIV_DMC1_G2D_ACP_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_C2C_SHIFT		(4)
#define EXYNOS4_CLKDIV_DMC1_C2C_MASK		(0x7 << EXYNOS4_CLKDIV_DMC1_C2C_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_PWI_SHIFT		(8)
#define EXYNOS4_CLKDIV_DMC1_PWI_MASK		(0xf << EXYNOS4_CLKDIV_DMC1_PWI_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_C2CACLK_SHIFT	(12)
#define EXYNOS4_CLKDIV_DMC1_C2CACLK_MASK	(0x7 << EXYNOS4_CLKDIV_DMC1_C2CACLK_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_DVSEM_SHIFT		(16)
#define EXYNOS4_CLKDIV_DMC1_DVSEM_MASK		(0x7f << EXYNOS4_CLKDIV_DMC1_DVSEM_SHIFT)
#define EXYNOS4_CLKDIV_DMC1_DPM_SHIFT		(24)
#define EXYNOS4_CLKDIV_DMC1_DPM_MASK		(0x7f << EXYNOS4_CLKDIV_DMC1_DPM_SHIFT)

#define EXYNOS4_CLKDIV_MFC_SHIFT		(0)
#define EXYNOS4_CLKDIV_MFC_MASK			(0x7 << EXYNOS4_CLKDIV_MFC_SHIFT)

#define EXYNOS4_CLKDIV_TOP_ACLK200_SHIFT	(0)
#define EXYNOS4_CLKDIV_TOP_ACLK200_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK200_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT	(4)
#define EXYNOS4_CLKDIV_TOP_ACLK100_MASK		(0xF << EXYNOS4_CLKDIV_TOP_ACLK100_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT	(8)
#define EXYNOS4_CLKDIV_TOP_ACLK160_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK160_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK133_SHIFT	(12)
#define EXYNOS4_CLKDIV_TOP_ACLK133_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ACLK133_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ONENAND_SHIFT	(16)
#define EXYNOS4_CLKDIV_TOP_ONENAND_MASK		(0x7 << EXYNOS4_CLKDIV_TOP_ONENAND_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK266_GPS_SHIFT	(20)
#define EXYNOS4_CLKDIV_TOP_ACLK266_GPS_MASK	(0x7 << EXYNOS4_CLKDIV_TOP_ACLK266_GPS_SHIFT)
#define EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_SHIFT	(24)
#define EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_MASK	(0x7 << EXYNOS4_CLKDIV_TOP_ACLK400_MCUISP_SHIFT)

#define EXYNOS4_CLKDIV_BUS_GDLR_SHIFT		(0)
#define EXYNOS4_CLKDIV_BUS_GDLR_MASK		(0x7 << EXYNOS4_CLKDIV_BUS_GDLR_SHIFT)
#define EXYNOS4_CLKDIV_BUS_GPLR_SHIFT		(4)
#define EXYNOS4_CLKDIV_BUS_GPLR_MASK		(0x7 << EXYNOS4_CLKDIV_BUS_GPLR_SHIFT)

#define EXYNOS4_CLKDIV_CAM_FIMC0_SHIFT		(0)
#define EXYNOS4_CLKDIV_CAM_FIMC0_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC0_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC1_SHIFT		(4)
#define EXYNOS4_CLKDIV_CAM_FIMC1_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC1_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC2_SHIFT		(8)
#define EXYNOS4_CLKDIV_CAM_FIMC2_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC2_SHIFT)
#define EXYNOS4_CLKDIV_CAM_FIMC3_SHIFT		(12)
#define EXYNOS4_CLKDIV_CAM_FIMC3_MASK		(0xf << EXYNOS4_CLKDIV_CAM_FIMC3_SHIFT)

/* Only for EXYNOS4210 */

#define EXYNOS4210_CLKSRC_MASK_LCD1		EXYNOS_CLKREG(0x0C338)

/* Only for EXYNOS4212 */

#define EXYNOS4_CLKDIV_CAM1			EXYNOS_CLKREG(0x0C568)

#define EXYNOS4_CLKDIV_STAT_CAM1		EXYNOS_CLKREG(0x0C668)

#define EXYNOS4_CLKDIV_CAM1_JPEG_SHIFT		(0)
#define EXYNOS4_CLKDIV_CAM1_JPEG_MASK		(0xf << EXYNOS4_CLKDIV_CAM1_JPEG_SHIFT)

/* For EXYNOS5250 */

#define EXYNOS5_APLL_LOCK			EXYNOS_CLKREG(0x00000)
#define EXYNOS5_APLL_CON0			EXYNOS_CLKREG(0x00100)
#define EXYNOS5_CLKMUX_STATCPU			EXYNOS_CLKREG(0x00400)
#define EXYNOS5_CLKDIV_CPU0			EXYNOS_CLKREG(0x00500)
#define EXYNOS5_CLKDIV_CPU1			EXYNOS_CLKREG(0x00504)
#define EXYNOS5_CLKDIV_STATCPU0			EXYNOS_CLKREG(0x00600)
#define EXYNOS5_CLKDIV_STATCPU1			EXYNOS_CLKREG(0x00604)

#define EXYNOS5_PWR_CTRL1			EXYNOS_CLKREG(0x01020)
#define EXYNOS5_PWR_CTRL2			EXYNOS_CLKREG(0x01024)

#define PWR_CTRL1_CORE2_DOWN_RATIO		(7 << 28)
#define PWR_CTRL1_CORE1_DOWN_RATIO		(7 << 16)
#define PWR_CTRL1_DIV2_DOWN_EN			(1 << 9)
#define PWR_CTRL1_DIV1_DOWN_EN			(1 << 8)
#define PWR_CTRL1_USE_CORE1_WFE			(1 << 5)
#define PWR_CTRL1_USE_CORE0_WFE			(1 << 4)
#define PWR_CTRL1_USE_CORE1_WFI			(1 << 1)
#define PWR_CTRL1_USE_CORE0_WFI			(1 << 0)

#define PWR_CTRL2_DIV2_UP_EN			(1 << 25)
#define PWR_CTRL2_DIV1_UP_EN			(1 << 24)
#define PWR_CTRL2_DUR_STANDBY2_VAL		(1 << 16)
#define PWR_CTRL2_DUR_STANDBY1_VAL		(1 << 8)
#define PWR_CTRL2_CORE2_UP_RATIO		(1 << 4)
#define PWR_CTRL2_CORE1_UP_RATIO		(1 << 0)

#endif /* __ASM_ARCH_REGS_CLOCK_H */
