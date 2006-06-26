/*
 * include/asm-arm/arch-ixp23xx/ixp23xx.h
 *
 * Register definitions for IXP23XX
 *
 * Copyright (C) 2003-2005 Intel Corporation.
 * Copyright (C) 2005 MontaVista Software, Inc.
 *
 * Maintainer: Deepak Saxena <dsaxena@plexity.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARCH_IXP23XX_H
#define __ASM_ARCH_IXP23XX_H

/*
 * IXP2300 linux memory map:
 *
 * virt		phys		size
 * fffd0000	a0000000	64K		XSI2CPP_CSR
 * fffc0000	c4000000	4K		EXP_CFG
 * fff00000	c8000000	64K		PERIPHERAL
 * fe000000	1c0000000	16M		CAP_CSR
 * fd000000	1c8000000	16M		MSF_CSR
 * fb000000			16M		---
 * fa000000	1d8000000	32M		PCI_IO
 * f8000000	1da000000	32M		PCI_CFG
 * f6000000	1de000000	32M		PCI_CREG
 * f4000000			32M		---
 * f0000000	1e0000000	64M		PCI_MEM
 * e[c-f]000000					per-platform mappings
 */


/****************************************************************************
 * Static mappings.
 ****************************************************************************/
#define IXP23XX_XSI2CPP_CSR_PHYS	0xa0000000
#define IXP23XX_XSI2CPP_CSR_VIRT	0xfffd0000
#define IXP23XX_XSI2CPP_CSR_SIZE	0x00010000

#define IXP23XX_EXP_CFG_PHYS		0xc4000000
#define IXP23XX_EXP_CFG_VIRT		0xfffc0000
#define IXP23XX_EXP_CFG_SIZE		0x00001000

#define IXP23XX_PERIPHERAL_PHYS		0xc8000000
#define IXP23XX_PERIPHERAL_VIRT		0xfff00000
#define IXP23XX_PERIPHERAL_SIZE		0x00010000

#define IXP23XX_CAP_CSR_PHYS		0x1c0000000ULL
#define IXP23XX_CAP_CSR_VIRT		0xfe000000
#define IXP23XX_CAP_CSR_SIZE		0x01000000

#define IXP23XX_MSF_CSR_PHYS		0x1c8000000ULL
#define IXP23XX_MSF_CSR_VIRT		0xfd000000
#define IXP23XX_MSF_CSR_SIZE		0x01000000

#define IXP23XX_PCI_IO_PHYS		0x1d8000000ULL
#define IXP23XX_PCI_IO_VIRT		0xfa000000
#define IXP23XX_PCI_IO_SIZE		0x02000000

#define IXP23XX_PCI_CFG_PHYS		0x1da000000ULL
#define IXP23XX_PCI_CFG_VIRT		0xf8000000
#define IXP23XX_PCI_CFG_SIZE		0x02000000
#define IXP23XX_PCI_CFG0_VIRT		IXP23XX_PCI_CFG_VIRT
#define IXP23XX_PCI_CFG1_VIRT		(IXP23XX_PCI_CFG_VIRT + 0x01000000)

#define IXP23XX_PCI_CREG_PHYS		0x1de000000ULL
#define IXP23XX_PCI_CREG_VIRT		0xf6000000
#define IXP23XX_PCI_CREG_SIZE		0x02000000
#define IXP23XX_PCI_CSR_VIRT		(IXP23XX_PCI_CREG_VIRT + 0x01000000)

#define IXP23XX_PCI_MEM_START		0xe0000000
#define IXP23XX_PCI_MEM_PHYS		0x1e0000000ULL
#define IXP23XX_PCI_MEM_VIRT		0xf0000000
#define IXP23XX_PCI_MEM_SIZE		0x04000000


/****************************************************************************
 * XSI2CPP CSRs.
 ****************************************************************************/
#define IXP23XX_XSI2CPP_REG(x)		((volatile unsigned long *)(IXP23XX_XSI2CPP_CSR_VIRT + (x)))
#define IXP23XX_CPP2XSI_CURR_XFER_REG3	IXP23XX_XSI2CPP_REG(0xf8)
#define IXP23XX_CPP2XSI_ADDR_31		(1 << 19)
#define IXP23XX_CPP2XSI_PSH_OFF		(1 << 20)
#define IXP23XX_CPP2XSI_COH_OFF		(1 << 21)


/****************************************************************************
 * Expansion Bus Config.
 ****************************************************************************/
#define IXP23XX_EXP_CFG_REG(x)		((volatile unsigned long *)(IXP23XX_EXP_CFG_VIRT + (x)))
#define IXP23XX_EXP_CS0			IXP23XX_EXP_CFG_REG(0x00)
#define IXP23XX_EXP_CS1			IXP23XX_EXP_CFG_REG(0x04)
#define IXP23XX_EXP_CS2			IXP23XX_EXP_CFG_REG(0x08)
#define IXP23XX_EXP_CS3			IXP23XX_EXP_CFG_REG(0x0c)
#define IXP23XX_EXP_CS4			IXP23XX_EXP_CFG_REG(0x10)
#define IXP23XX_EXP_CS5			IXP23XX_EXP_CFG_REG(0x14)
#define IXP23XX_EXP_CS6			IXP23XX_EXP_CFG_REG(0x18)
#define IXP23XX_EXP_CS7			IXP23XX_EXP_CFG_REG(0x1c)
#define IXP23XX_FLASH_WRITABLE		(0x2)
#define IXP23XX_FLASH_BUS8		(0x1)

#define IXP23XX_EXP_CFG0		IXP23XX_EXP_CFG_REG(0x20)
#define IXP23XX_EXP_CFG1		IXP23XX_EXP_CFG_REG(0x24)
#define IXP23XX_EXP_CFG0_MEM_MAP		(1 << 31)
#define IXP23XX_EXP_CFG0_XSCALE_SPEED_SEL 	(3 << 22)
#define IXP23XX_EXP_CFG0_XSCALE_SPEED_EN	(1 << 21)
#define IXP23XX_EXP_CFG0_CPP_SPEED_SEL		(3 << 19)
#define IXP23XX_EXP_CFG0_CPP_SPEED_EN		(1 << 18)
#define IXP23XX_EXP_CFG0_PCI_SWIN		(3 << 16)
#define IXP23XX_EXP_CFG0_PCI_DWIN		(3 << 14)
#define IXP23XX_EXP_CFG0_PCI33_MODE		(1 << 13)
#define IXP23XX_EXP_CFG0_QDR_SPEED_SEL		(1 << 12)
#define IXP23XX_EXP_CFG0_CPP_DIV_SEL		(1 << 5)
#define IXP23XX_EXP_CFG0_XSI_NOT_PRES		(1 << 4)
#define IXP23XX_EXP_CFG0_PROM_BOOT		(1 << 3)
#define IXP23XX_EXP_CFG0_PCI_ARB		(1 << 2)
#define IXP23XX_EXP_CFG0_PCI_HOST		(1 << 1)
#define IXP23XX_EXP_CFG0_FLASH_WIDTH		(1 << 0)

#define IXP23XX_EXP_UNIT_FUSE		IXP23XX_EXP_CFG_REG(0x28)
#define IXP23XX_EXP_MSF_MUX		IXP23XX_EXP_CFG_REG(0x30)
#define IXP23XX_EXP_CFG_FUSE		IXP23XX_EXP_CFG_REG(0x34)

#define IXP23XX_EXP_BUS_PHYS		0x90000000
#define IXP23XX_EXP_BUS_WINDOW_SIZE	0x01000000

#define IXP23XX_EXP_BUS_CS0_BASE	(IXP23XX_EXP_BUS_PHYS + 0x00000000)
#define IXP23XX_EXP_BUS_CS1_BASE	(IXP23XX_EXP_BUS_PHYS + 0x01000000)
#define IXP23XX_EXP_BUS_CS2_BASE	(IXP23XX_EXP_BUS_PHYS + 0x02000000)
#define IXP23XX_EXP_BUS_CS3_BASE	(IXP23XX_EXP_BUS_PHYS + 0x03000000)
#define IXP23XX_EXP_BUS_CS4_BASE	(IXP23XX_EXP_BUS_PHYS + 0x04000000)
#define IXP23XX_EXP_BUS_CS5_BASE	(IXP23XX_EXP_BUS_PHYS + 0x05000000)
#define IXP23XX_EXP_BUS_CS6_BASE	(IXP23XX_EXP_BUS_PHYS + 0x06000000)
#define IXP23XX_EXP_BUS_CS7_BASE	(IXP23XX_EXP_BUS_PHYS + 0x07000000)


/****************************************************************************
 * Peripherals.
 ****************************************************************************/
#define IXP23XX_UART1_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x0000)
#define IXP23XX_UART2_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x1000)
#define IXP23XX_PMU_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x2000)
#define IXP23XX_INTC_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x3000)
#define IXP23XX_GPIO_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x4000)
#define IXP23XX_TIMER_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x5000)
#define IXP23XX_NPE0_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x6000)
#define IXP23XX_DSR_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x7000)
#define IXP23XX_NPE1_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x8000)
#define IXP23XX_ETH0_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0x9000)
#define IXP23XX_ETH1_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0xA000)
#define IXP23XX_GIG0_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0xB000)
#define IXP23XX_GIG1_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0xC000)
#define IXP23XX_DDRS_VIRT		(IXP23XX_PERIPHERAL_VIRT + 0xD000)

#define IXP23XX_UART1_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x0000)
#define IXP23XX_UART2_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x1000)
#define IXP23XX_PMU_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x2000)
#define IXP23XX_INTC_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x3000)
#define IXP23XX_GPIO_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x4000)
#define IXP23XX_TIMER_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x5000)
#define IXP23XX_NPE0_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x6000)
#define IXP23XX_DSR_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x7000)
#define IXP23XX_NPE1_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x8000)
#define IXP23XX_ETH0_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0x9000)
#define IXP23XX_ETH1_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0xA000)
#define IXP23XX_GIG0_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0xB000)
#define IXP23XX_GIG1_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0xC000)
#define IXP23XX_DDRS_PHYS		(IXP23XX_PERIPHERAL_PHYS + 0xD000)


/****************************************************************************
 * Interrupt controller.
 ****************************************************************************/
#define IXP23XX_INTC_REG(x)		 ((volatile unsigned long *)(IXP23XX_INTC_VIRT + (x)))
#define IXP23XX_INTR_ST1		IXP23XX_INTC_REG(0x00)
#define IXP23XX_INTR_ST2		IXP23XX_INTC_REG(0x04)
#define IXP23XX_INTR_ST3		IXP23XX_INTC_REG(0x08)
#define IXP23XX_INTR_ST4		IXP23XX_INTC_REG(0x0c)
#define IXP23XX_INTR_EN1		IXP23XX_INTC_REG(0x10)
#define IXP23XX_INTR_EN2		IXP23XX_INTC_REG(0x14)
#define IXP23XX_INTR_EN3		IXP23XX_INTC_REG(0x18)
#define IXP23XX_INTR_EN4		IXP23XX_INTC_REG(0x1c)
#define IXP23XX_INTR_SEL1		IXP23XX_INTC_REG(0x20)
#define IXP23XX_INTR_SEL2		IXP23XX_INTC_REG(0x24)
#define IXP23XX_INTR_SEL3		IXP23XX_INTC_REG(0x28)
#define IXP23XX_INTR_SEL4		IXP23XX_INTC_REG(0x2c)
#define IXP23XX_INTR_IRQ_ST1		IXP23XX_INTC_REG(0x30)
#define IXP23XX_INTR_IRQ_ST2		IXP23XX_INTC_REG(0x34)
#define IXP23XX_INTR_IRQ_ST3		IXP23XX_INTC_REG(0x38)
#define IXP23XX_INTR_IRQ_ST4		IXP23XX_INTC_REG(0x3c)
#define IXP23XX_INTR_IRQ_ENC_ST_OFFSET	0x54


/****************************************************************************
 * GPIO.
 ****************************************************************************/
#define IXP23XX_GPIO_REG(x)		((volatile unsigned long *)(IXP23XX_GPIO_VIRT + (x)))
#define IXP23XX_GPIO_GPOUTR		IXP23XX_GPIO_REG(0x00)
#define IXP23XX_GPIO_GPOER		IXP23XX_GPIO_REG(0x04)
#define IXP23XX_GPIO_GPINR		IXP23XX_GPIO_REG(0x08)
#define IXP23XX_GPIO_GPISR		IXP23XX_GPIO_REG(0x0c)
#define IXP23XX_GPIO_GPIT1R		IXP23XX_GPIO_REG(0x10)
#define IXP23XX_GPIO_GPIT2R		IXP23XX_GPIO_REG(0x14)
#define IXP23XX_GPIO_GPCLKR		IXP23XX_GPIO_REG(0x18)
#define IXP23XX_GPIO_GPDBSELR 		IXP23XX_GPIO_REG(0x1c)

#define IXP23XX_GPIO_STYLE_MASK		0x7
#define IXP23XX_GPIO_STYLE_ACTIVE_HIGH	0x0
#define IXP23XX_GPIO_STYLE_ACTIVE_LOW	0x1
#define IXP23XX_GPIO_STYLE_RISING_EDGE	0x2
#define IXP23XX_GPIO_STYLE_FALLING_EDGE	0x3
#define IXP23XX_GPIO_STYLE_TRANSITIONAL	0x4

#define IXP23XX_GPIO_STYLE_SIZE		3


/****************************************************************************
 * Timer.
 ****************************************************************************/
#define IXP23XX_TIMER_REG(x)		((volatile unsigned long *)(IXP23XX_TIMER_VIRT + (x)))
#define IXP23XX_TIMER_CONT		IXP23XX_TIMER_REG(0x00)
#define IXP23XX_TIMER1_TIMESTAMP	IXP23XX_TIMER_REG(0x04)
#define IXP23XX_TIMER1_RELOAD		IXP23XX_TIMER_REG(0x08)
#define IXP23XX_TIMER2_TIMESTAMP	IXP23XX_TIMER_REG(0x0c)
#define IXP23XX_TIMER2_RELOAD		IXP23XX_TIMER_REG(0x10)
#define IXP23XX_TIMER_WDOG		IXP23XX_TIMER_REG(0x14)
#define IXP23XX_TIMER_WDOG_EN		IXP23XX_TIMER_REG(0x18)
#define IXP23XX_TIMER_WDOG_KEY		IXP23XX_TIMER_REG(0x1c)
#define IXP23XX_TIMER_WDOG_KEY_MAGIC	0x482e
#define IXP23XX_TIMER_STATUS		IXP23XX_TIMER_REG(0x20)
#define IXP23XX_TIMER_SOFT_RESET	IXP23XX_TIMER_REG(0x24)
#define IXP23XX_TIMER_SOFT_RESET_EN	IXP23XX_TIMER_REG(0x28)

#define IXP23XX_TIMER_ENABLE		(1 << 0)
#define IXP23XX_TIMER_ONE_SHOT		(1 << 1)
/* Low order bits of reload value ignored */
#define IXP23XX_TIMER_RELOAD_MASK	(0x3)
#define IXP23XX_TIMER_DISABLED		(0x0)
#define IXP23XX_TIMER1_INT_PEND		(1 << 0)
#define IXP23XX_TIMER2_INT_PEND		(1 << 1)
#define IXP23XX_TIMER_STATUS_TS_PEND	(1 << 2)
#define IXP23XX_TIMER_STATUS_WDOG_PEND	(1 << 3)
#define IXP23XX_TIMER_STATUS_WARM_RESET	(1 << 4)


/****************************************************************************
 * CAP CSRs.
 ****************************************************************************/
#define IXP23XX_GLOBAL_REG(x)		((volatile unsigned long *)(IXP23XX_CAP_CSR_VIRT + 0x4a00 + (x)))
#define IXP23XX_PRODUCT_ID		IXP23XX_GLOBAL_REG(0x00)
#define IXP23XX_MISC_CONTROL		IXP23XX_GLOBAL_REG(0x04)
#define IXP23XX_MSF_CLK_CNTRL		IXP23XX_GLOBAL_REG(0x08)
#define IXP23XX_RESET0			IXP23XX_GLOBAL_REG(0x0c)
#define IXP23XX_RESET1			IXP23XX_GLOBAL_REG(0x10)
#define IXP23XX_STRAP_OPTIONS		IXP23XX_GLOBAL_REG(0x18)

#define IXP23XX_ENABLE_WATCHDOG		(1 << 24)
#define IXP23XX_SHPC_INIT_COMP		(1 << 21)
#define IXP23XX_RST_ALL			(1 << 16)
#define IXP23XX_RESET_PCI		(1 << 2)
#define IXP23XX_PCI_UNIT_RESET		(1 << 1)
#define IXP23XX_XSCALE_RESET		(1 << 0)

#define IXP23XX_UENGINE_CSR_VIRT_BASE	(IXP23XX_CAP_CSR_VIRT + 0x18000)


/****************************************************************************
 * PCI CSRs.
 ****************************************************************************/
#define IXP23XX_PCI_CREG(x)		((volatile unsigned long *)(IXP23XX_PCI_CREG_VIRT + (x)))
#define IXP23XX_PCI_CMDSTAT		IXP23XX_PCI_CREG(0x04)
#define IXP23XX_PCI_SRAM_BAR		IXP23XX_PCI_CREG(0x14)
#define IXP23XX_PCI_SDRAM_BAR		IXP23XX_PCI_CREG(0x18)


#define IXP23XX_PCI_CSR(x)		((volatile unsigned long *)(IXP23XX_PCI_CREG_VIRT + 0x01000000 + (x)))
#define IXP23XX_PCI_OUT_INT_STATUS	IXP23XX_PCI_CSR(0x0030)
#define IXP23XX_PCI_OUT_INT_MASK	IXP23XX_PCI_CSR(0x0034)
#define IXP23XX_PCI_SRAM_BASE_ADDR_MASK IXP23XX_PCI_CSR(0x00fc)
#define IXP23XX_PCI_DRAM_BASE_ADDR_MASK IXP23XX_PCI_CSR(0x0100)
#define IXP23XX_PCI_CONTROL		IXP23XX_PCI_CSR(0x013c)
#define IXP23XX_PCI_ADDR_EXT		IXP23XX_PCI_CSR(0x0140)
#define IXP23XX_PCI_ME_PUSH_STATUS	IXP23XX_PCI_CSR(0x0148)
#define IXP23XX_PCI_ME_PUSH_EN		IXP23XX_PCI_CSR(0x014c)
#define IXP23XX_PCI_ERR_STATUS		IXP23XX_PCI_CSR(0x0150)
#define IXP23XX_PCI_ERROR_STATUS	IXP23XX_PCI_CSR(0x0150)
#define IXP23XX_PCI_ERR_ENABLE		IXP23XX_PCI_CSR(0x0154)
#define IXP23XX_PCI_XSCALE_INT_STATUS	IXP23XX_PCI_CSR(0x0158)
#define IXP23XX_PCI_XSCALE_INT_ENABLE	IXP23XX_PCI_CSR(0x015c)
#define IXP23XX_PCI_CPP_ADDR_BITS	IXP23XX_PCI_CSR(0x0160)


#ifndef __ASSEMBLY__
/*
 * Is system memory on the XSI or CPP bus?
 */
static inline unsigned ixp23xx_cpp_boot(void)
{
	return (*IXP23XX_EXP_CFG0 & IXP23XX_EXP_CFG0_XSI_NOT_PRES);
}
#endif


#endif
