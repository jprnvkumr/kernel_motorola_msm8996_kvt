/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for Alchemy Semiconductor's Au1k CPU.
 *
 * Copyright 2000-2001, 2006-2008 MontaVista Software Inc.
 * Author: MontaVista Software, Inc. <source@mvista.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

 /*
  * some definitions add by takuzo@sm.sony.co.jp and sato@sm.sony.co.jp
  */

#ifndef _AU1000_H_
#define _AU1000_H_


#ifndef _LANGUAGE_ASSEMBLY

#include <linux/delay.h>
#include <linux/types.h>

#include <linux/io.h>
#include <linux/irq.h>

/* cpu pipeline flush */
void static inline au_sync(void)
{
	__asm__ volatile ("sync");
}

void static inline au_sync_udelay(int us)
{
	__asm__ volatile ("sync");
	udelay(us);
}

void static inline au_sync_delay(int ms)
{
	__asm__ volatile ("sync");
	mdelay(ms);
}

void static inline au_writeb(u8 val, unsigned long reg)
{
	*(volatile u8 *)reg = val;
}

void static inline au_writew(u16 val, unsigned long reg)
{
	*(volatile u16 *)reg = val;
}

void static inline au_writel(u32 val, unsigned long reg)
{
	*(volatile u32 *)reg = val;
}

static inline u8 au_readb(unsigned long reg)
{
	return *(volatile u8 *)reg;
}

static inline u16 au_readw(unsigned long reg)
{
	return *(volatile u16 *)reg;
}

static inline u32 au_readl(unsigned long reg)
{
	return *(volatile u32 *)reg;
}

/* Early Au1000 have a write-only SYS_CPUPLL register. */
static inline int au1xxx_cpu_has_pll_wo(void)
{
	switch (read_c0_prid()) {
	case 0x00030100:	/* Au1000 DA */
	case 0x00030201:	/* Au1000 HA */
	case 0x00030202:	/* Au1000 HB */
		return 1;
	}
	return 0;
}

/* does CPU need CONFIG[OD] set to fix tons of errata? */
static inline int au1xxx_cpu_needs_config_od(void)
{
	/*
	 * c0_config.od (bit 19) was write only (and read as 0) on the
	 * early revisions of Alchemy SOCs.  It disables the bus trans-
	 * action overlapping and needs to be set to fix various errata.
	 */
	switch (read_c0_prid()) {
	case 0x00030100: /* Au1000 DA */
	case 0x00030201: /* Au1000 HA */
	case 0x00030202: /* Au1000 HB */
	case 0x01030200: /* Au1500 AB */
	/*
	 * Au1100/Au1200 errata actually keep silence about this bit,
	 * so we set it just in case for those revisions that require
	 * it to be set according to the (now gone) cpu_table.
	 */
	case 0x02030200: /* Au1100 AB */
	case 0x02030201: /* Au1100 BA */
	case 0x02030202: /* Au1100 BC */
	case 0x04030201: /* Au1200 AC */
		return 1;
	}
	return 0;
}

#define ALCHEMY_CPU_UNKNOWN	-1
#define ALCHEMY_CPU_AU1000	0
#define ALCHEMY_CPU_AU1500	1
#define ALCHEMY_CPU_AU1100	2
#define ALCHEMY_CPU_AU1550	3
#define ALCHEMY_CPU_AU1200	4

static inline int alchemy_get_cputype(void)
{
	switch (read_c0_prid() & 0xffff0000) {
	case 0x00030000:
		return ALCHEMY_CPU_AU1000;
		break;
	case 0x01030000:
		return ALCHEMY_CPU_AU1500;
		break;
	case 0x02030000:
		return ALCHEMY_CPU_AU1100;
		break;
	case 0x03030000:
		return ALCHEMY_CPU_AU1550;
		break;
	case 0x04030000:
	case 0x05030000:
		return ALCHEMY_CPU_AU1200;
		break;
	}

	return ALCHEMY_CPU_UNKNOWN;
}

/* return number of uarts on a given cputype */
static inline int alchemy_get_uarts(int type)
{
	switch (type) {
	case ALCHEMY_CPU_AU1000:
		return 4;
	case ALCHEMY_CPU_AU1500:
	case ALCHEMY_CPU_AU1200:
		return 2;
	case ALCHEMY_CPU_AU1100:
	case ALCHEMY_CPU_AU1550:
		return 3;
	}
	return 0;
}

/* enable an UART block if it isn't already */
static inline void alchemy_uart_enable(u32 uart_phys)
{
	void __iomem *addr = (void __iomem *)KSEG1ADDR(uart_phys);

	/* reset, enable clock, deassert reset */
	if ((__raw_readl(addr + 0x100) & 3) != 3) {
		__raw_writel(0, addr + 0x100);
		wmb();
		__raw_writel(1, addr + 0x100);
		wmb();
	}
	__raw_writel(3, addr + 0x100);
	wmb();
}

static inline void alchemy_uart_disable(u32 uart_phys)
{
	void __iomem *addr = (void __iomem *)KSEG1ADDR(uart_phys);
	__raw_writel(0, addr + 0x100);	/* UART_MOD_CNTRL */
	wmb();
}

static inline void alchemy_uart_putchar(u32 uart_phys, u8 c)
{
	void __iomem *base = (void __iomem *)KSEG1ADDR(uart_phys);
	int timeout, i;

	/* check LSR TX_EMPTY bit */
	timeout = 0xffffff;
	do {
		if (__raw_readl(base + 0x1c) & 0x20)
			break;
		/* slow down */
		for (i = 10000; i; i--)
			asm volatile ("nop");
	} while (--timeout);

	__raw_writel(c, base + 0x04);	/* tx */
	wmb();
}

/* return number of ethernet MACs on a given cputype */
static inline int alchemy_get_macs(int type)
{
	switch (type) {
	case ALCHEMY_CPU_AU1000:
	case ALCHEMY_CPU_AU1500:
	case ALCHEMY_CPU_AU1550:
		return 2;
	case ALCHEMY_CPU_AU1100:
		return 1;
	}
	return 0;
}

/* arch/mips/au1000/common/clocks.c */
extern void set_au1x00_speed(unsigned int new_freq);
extern unsigned int get_au1x00_speed(void);
extern void set_au1x00_uart_baud_base(unsigned long new_baud_base);
extern unsigned long get_au1x00_uart_baud_base(void);
extern unsigned long au1xxx_calc_clock(void);

/* PM: arch/mips/alchemy/common/sleeper.S, power.c, irq.c */
void alchemy_sleep_au1000(void);
void alchemy_sleep_au1550(void);
void au_sleep(void);

/* USB: drivers/usb/host/alchemy-common.c */
enum alchemy_usb_block {
	ALCHEMY_USB_OHCI0,
	ALCHEMY_USB_UDC0,
	ALCHEMY_USB_EHCI0,
	ALCHEMY_USB_OTG0,
};
int alchemy_usb_control(int block, int enable);


/* SOC Interrupt numbers */

#define AU1000_INTC0_INT_BASE	(MIPS_CPU_IRQ_BASE + 8)
#define AU1000_INTC0_INT_LAST	(AU1000_INTC0_INT_BASE + 31)
#define AU1000_INTC1_INT_BASE	(AU1000_INTC0_INT_LAST + 1)
#define AU1000_INTC1_INT_LAST	(AU1000_INTC1_INT_BASE + 31)
#define AU1000_MAX_INTR 	AU1000_INTC1_INT_LAST

enum soc_au1000_ints {
	AU1000_FIRST_INT	= AU1000_INTC0_INT_BASE,
	AU1000_UART0_INT	= AU1000_FIRST_INT,
	AU1000_UART1_INT,
	AU1000_UART2_INT,
	AU1000_UART3_INT,
	AU1000_SSI0_INT,
	AU1000_SSI1_INT,
	AU1000_DMA_INT_BASE,

	AU1000_TOY_INT		= AU1000_FIRST_INT + 14,
	AU1000_TOY_MATCH0_INT,
	AU1000_TOY_MATCH1_INT,
	AU1000_TOY_MATCH2_INT,
	AU1000_RTC_INT,
	AU1000_RTC_MATCH0_INT,
	AU1000_RTC_MATCH1_INT,
	AU1000_RTC_MATCH2_INT,
	AU1000_IRDA_TX_INT,
	AU1000_IRDA_RX_INT,
	AU1000_USB_DEV_REQ_INT,
	AU1000_USB_DEV_SUS_INT,
	AU1000_USB_HOST_INT,
	AU1000_ACSYNC_INT,
	AU1000_MAC0_DMA_INT,
	AU1000_MAC1_DMA_INT,
	AU1000_I2S_UO_INT,
	AU1000_AC97C_INT,
	AU1000_GPIO0_INT,
	AU1000_GPIO1_INT,
	AU1000_GPIO2_INT,
	AU1000_GPIO3_INT,
	AU1000_GPIO4_INT,
	AU1000_GPIO5_INT,
	AU1000_GPIO6_INT,
	AU1000_GPIO7_INT,
	AU1000_GPIO8_INT,
	AU1000_GPIO9_INT,
	AU1000_GPIO10_INT,
	AU1000_GPIO11_INT,
	AU1000_GPIO12_INT,
	AU1000_GPIO13_INT,
	AU1000_GPIO14_INT,
	AU1000_GPIO15_INT,
	AU1000_GPIO16_INT,
	AU1000_GPIO17_INT,
	AU1000_GPIO18_INT,
	AU1000_GPIO19_INT,
	AU1000_GPIO20_INT,
	AU1000_GPIO21_INT,
	AU1000_GPIO22_INT,
	AU1000_GPIO23_INT,
	AU1000_GPIO24_INT,
	AU1000_GPIO25_INT,
	AU1000_GPIO26_INT,
	AU1000_GPIO27_INT,
	AU1000_GPIO28_INT,
	AU1000_GPIO29_INT,
	AU1000_GPIO30_INT,
	AU1000_GPIO31_INT,
};

enum soc_au1100_ints {
	AU1100_FIRST_INT	= AU1000_INTC0_INT_BASE,
	AU1100_UART0_INT	= AU1100_FIRST_INT,
	AU1100_UART1_INT,
	AU1100_SD_INT,
	AU1100_UART3_INT,
	AU1100_SSI0_INT,
	AU1100_SSI1_INT,
	AU1100_DMA_INT_BASE,

	AU1100_TOY_INT		= AU1100_FIRST_INT + 14,
	AU1100_TOY_MATCH0_INT,
	AU1100_TOY_MATCH1_INT,
	AU1100_TOY_MATCH2_INT,
	AU1100_RTC_INT,
	AU1100_RTC_MATCH0_INT,
	AU1100_RTC_MATCH1_INT,
	AU1100_RTC_MATCH2_INT,
	AU1100_IRDA_TX_INT,
	AU1100_IRDA_RX_INT,
	AU1100_USB_DEV_REQ_INT,
	AU1100_USB_DEV_SUS_INT,
	AU1100_USB_HOST_INT,
	AU1100_ACSYNC_INT,
	AU1100_MAC0_DMA_INT,
	AU1100_GPIO208_215_INT,
	AU1100_LCD_INT,
	AU1100_AC97C_INT,
	AU1100_GPIO0_INT,
	AU1100_GPIO1_INT,
	AU1100_GPIO2_INT,
	AU1100_GPIO3_INT,
	AU1100_GPIO4_INT,
	AU1100_GPIO5_INT,
	AU1100_GPIO6_INT,
	AU1100_GPIO7_INT,
	AU1100_GPIO8_INT,
	AU1100_GPIO9_INT,
	AU1100_GPIO10_INT,
	AU1100_GPIO11_INT,
	AU1100_GPIO12_INT,
	AU1100_GPIO13_INT,
	AU1100_GPIO14_INT,
	AU1100_GPIO15_INT,
	AU1100_GPIO16_INT,
	AU1100_GPIO17_INT,
	AU1100_GPIO18_INT,
	AU1100_GPIO19_INT,
	AU1100_GPIO20_INT,
	AU1100_GPIO21_INT,
	AU1100_GPIO22_INT,
	AU1100_GPIO23_INT,
	AU1100_GPIO24_INT,
	AU1100_GPIO25_INT,
	AU1100_GPIO26_INT,
	AU1100_GPIO27_INT,
	AU1100_GPIO28_INT,
	AU1100_GPIO29_INT,
	AU1100_GPIO30_INT,
	AU1100_GPIO31_INT,
};

enum soc_au1500_ints {
	AU1500_FIRST_INT	= AU1000_INTC0_INT_BASE,
	AU1500_UART0_INT	= AU1500_FIRST_INT,
	AU1500_PCI_INTA,
	AU1500_PCI_INTB,
	AU1500_UART3_INT,
	AU1500_PCI_INTC,
	AU1500_PCI_INTD,
	AU1500_DMA_INT_BASE,

	AU1500_TOY_INT		= AU1500_FIRST_INT + 14,
	AU1500_TOY_MATCH0_INT,
	AU1500_TOY_MATCH1_INT,
	AU1500_TOY_MATCH2_INT,
	AU1500_RTC_INT,
	AU1500_RTC_MATCH0_INT,
	AU1500_RTC_MATCH1_INT,
	AU1500_RTC_MATCH2_INT,
	AU1500_PCI_ERR_INT,
	AU1500_RESERVED_INT,
	AU1500_USB_DEV_REQ_INT,
	AU1500_USB_DEV_SUS_INT,
	AU1500_USB_HOST_INT,
	AU1500_ACSYNC_INT,
	AU1500_MAC0_DMA_INT,
	AU1500_MAC1_DMA_INT,
	AU1500_AC97C_INT	= AU1500_FIRST_INT + 31,
	AU1500_GPIO0_INT,
	AU1500_GPIO1_INT,
	AU1500_GPIO2_INT,
	AU1500_GPIO3_INT,
	AU1500_GPIO4_INT,
	AU1500_GPIO5_INT,
	AU1500_GPIO6_INT,
	AU1500_GPIO7_INT,
	AU1500_GPIO8_INT,
	AU1500_GPIO9_INT,
	AU1500_GPIO10_INT,
	AU1500_GPIO11_INT,
	AU1500_GPIO12_INT,
	AU1500_GPIO13_INT,
	AU1500_GPIO14_INT,
	AU1500_GPIO15_INT,
	AU1500_GPIO200_INT,
	AU1500_GPIO201_INT,
	AU1500_GPIO202_INT,
	AU1500_GPIO203_INT,
	AU1500_GPIO20_INT,
	AU1500_GPIO204_INT,
	AU1500_GPIO205_INT,
	AU1500_GPIO23_INT,
	AU1500_GPIO24_INT,
	AU1500_GPIO25_INT,
	AU1500_GPIO26_INT,
	AU1500_GPIO27_INT,
	AU1500_GPIO28_INT,
	AU1500_GPIO206_INT,
	AU1500_GPIO207_INT,
	AU1500_GPIO208_215_INT,
};

enum soc_au1550_ints {
	AU1550_FIRST_INT	= AU1000_INTC0_INT_BASE,
	AU1550_UART0_INT	= AU1550_FIRST_INT,
	AU1550_PCI_INTA,
	AU1550_PCI_INTB,
	AU1550_DDMA_INT,
	AU1550_CRYPTO_INT,
	AU1550_PCI_INTC,
	AU1550_PCI_INTD,
	AU1550_PCI_RST_INT,
	AU1550_UART1_INT,
	AU1550_UART3_INT,
	AU1550_PSC0_INT,
	AU1550_PSC1_INT,
	AU1550_PSC2_INT,
	AU1550_PSC3_INT,
	AU1550_TOY_INT,
	AU1550_TOY_MATCH0_INT,
	AU1550_TOY_MATCH1_INT,
	AU1550_TOY_MATCH2_INT,
	AU1550_RTC_INT,
	AU1550_RTC_MATCH0_INT,
	AU1550_RTC_MATCH1_INT,
	AU1550_RTC_MATCH2_INT,

	AU1550_NAND_INT		= AU1550_FIRST_INT + 23,
	AU1550_USB_DEV_REQ_INT,
	AU1550_USB_DEV_SUS_INT,
	AU1550_USB_HOST_INT,
	AU1550_MAC0_DMA_INT,
	AU1550_MAC1_DMA_INT,
	AU1550_GPIO0_INT	= AU1550_FIRST_INT + 32,
	AU1550_GPIO1_INT,
	AU1550_GPIO2_INT,
	AU1550_GPIO3_INT,
	AU1550_GPIO4_INT,
	AU1550_GPIO5_INT,
	AU1550_GPIO6_INT,
	AU1550_GPIO7_INT,
	AU1550_GPIO8_INT,
	AU1550_GPIO9_INT,
	AU1550_GPIO10_INT,
	AU1550_GPIO11_INT,
	AU1550_GPIO12_INT,
	AU1550_GPIO13_INT,
	AU1550_GPIO14_INT,
	AU1550_GPIO15_INT,
	AU1550_GPIO200_INT,
	AU1550_GPIO201_205_INT,	/* Logical or of GPIO201:205 */
	AU1550_GPIO16_INT,
	AU1550_GPIO17_INT,
	AU1550_GPIO20_INT,
	AU1550_GPIO21_INT,
	AU1550_GPIO22_INT,
	AU1550_GPIO23_INT,
	AU1550_GPIO24_INT,
	AU1550_GPIO25_INT,
	AU1550_GPIO26_INT,
	AU1550_GPIO27_INT,
	AU1550_GPIO28_INT,
	AU1550_GPIO206_INT,
	AU1550_GPIO207_INT,
	AU1550_GPIO208_215_INT,	/* Logical or of GPIO208:215 */
};

enum soc_au1200_ints {
	AU1200_FIRST_INT	= AU1000_INTC0_INT_BASE,
	AU1200_UART0_INT	= AU1200_FIRST_INT,
	AU1200_SWT_INT,
	AU1200_SD_INT,
	AU1200_DDMA_INT,
	AU1200_MAE_BE_INT,
	AU1200_GPIO200_INT,
	AU1200_GPIO201_INT,
	AU1200_GPIO202_INT,
	AU1200_UART1_INT,
	AU1200_MAE_FE_INT,
	AU1200_PSC0_INT,
	AU1200_PSC1_INT,
	AU1200_AES_INT,
	AU1200_CAMERA_INT,
	AU1200_TOY_INT,
	AU1200_TOY_MATCH0_INT,
	AU1200_TOY_MATCH1_INT,
	AU1200_TOY_MATCH2_INT,
	AU1200_RTC_INT,
	AU1200_RTC_MATCH0_INT,
	AU1200_RTC_MATCH1_INT,
	AU1200_RTC_MATCH2_INT,
	AU1200_GPIO203_INT,
	AU1200_NAND_INT,
	AU1200_GPIO204_INT,
	AU1200_GPIO205_INT,
	AU1200_GPIO206_INT,
	AU1200_GPIO207_INT,
	AU1200_GPIO208_215_INT,	/* Logical OR of 208:215 */
	AU1200_USB_INT,
	AU1200_LCD_INT,
	AU1200_MAE_BOTH_INT,
	AU1200_GPIO0_INT,
	AU1200_GPIO1_INT,
	AU1200_GPIO2_INT,
	AU1200_GPIO3_INT,
	AU1200_GPIO4_INT,
	AU1200_GPIO5_INT,
	AU1200_GPIO6_INT,
	AU1200_GPIO7_INT,
	AU1200_GPIO8_INT,
	AU1200_GPIO9_INT,
	AU1200_GPIO10_INT,
	AU1200_GPIO11_INT,
	AU1200_GPIO12_INT,
	AU1200_GPIO13_INT,
	AU1200_GPIO14_INT,
	AU1200_GPIO15_INT,
	AU1200_GPIO16_INT,
	AU1200_GPIO17_INT,
	AU1200_GPIO18_INT,
	AU1200_GPIO19_INT,
	AU1200_GPIO20_INT,
	AU1200_GPIO21_INT,
	AU1200_GPIO22_INT,
	AU1200_GPIO23_INT,
	AU1200_GPIO24_INT,
	AU1200_GPIO25_INT,
	AU1200_GPIO26_INT,
	AU1200_GPIO27_INT,
	AU1200_GPIO28_INT,
	AU1200_GPIO29_INT,
	AU1200_GPIO30_INT,
	AU1200_GPIO31_INT,
};

#endif /* !defined (_LANGUAGE_ASSEMBLY) */

/*
 * SDRAM register offsets
 */
#if defined(CONFIG_SOC_AU1000) || defined(CONFIG_SOC_AU1500) || \
    defined(CONFIG_SOC_AU1100)
#define MEM_SDMODE0		0x0000
#define MEM_SDMODE1		0x0004
#define MEM_SDMODE2		0x0008
#define MEM_SDADDR0		0x000C
#define MEM_SDADDR1		0x0010
#define MEM_SDADDR2		0x0014
#define MEM_SDREFCFG		0x0018
#define MEM_SDPRECMD		0x001C
#define MEM_SDAUTOREF		0x0020
#define MEM_SDWRMD0		0x0024
#define MEM_SDWRMD1		0x0028
#define MEM_SDWRMD2		0x002C
#define MEM_SDSLEEP		0x0030
#define MEM_SDSMCKE		0x0034

/*
 * MEM_SDMODE register content definitions
 */
#define MEM_SDMODE_F		(1 << 22)
#define MEM_SDMODE_SR		(1 << 21)
#define MEM_SDMODE_BS		(1 << 20)
#define MEM_SDMODE_RS		(3 << 18)
#define MEM_SDMODE_CS		(7 << 15)
#define MEM_SDMODE_TRAS 	(15 << 11)
#define MEM_SDMODE_TMRD 	(3 << 9)
#define MEM_SDMODE_TWR		(3 << 7)
#define MEM_SDMODE_TRP		(3 << 5)
#define MEM_SDMODE_TRCD 	(3 << 3)
#define MEM_SDMODE_TCL		(7 << 0)

#define MEM_SDMODE_BS_2Bank	(0 << 20)
#define MEM_SDMODE_BS_4Bank	(1 << 20)
#define MEM_SDMODE_RS_11Row	(0 << 18)
#define MEM_SDMODE_RS_12Row	(1 << 18)
#define MEM_SDMODE_RS_13Row	(2 << 18)
#define MEM_SDMODE_RS_N(N)	((N) << 18)
#define MEM_SDMODE_CS_7Col	(0 << 15)
#define MEM_SDMODE_CS_8Col	(1 << 15)
#define MEM_SDMODE_CS_9Col	(2 << 15)
#define MEM_SDMODE_CS_10Col	(3 << 15)
#define MEM_SDMODE_CS_11Col	(4 << 15)
#define MEM_SDMODE_CS_N(N)	((N) << 15)
#define MEM_SDMODE_TRAS_N(N)	((N) << 11)
#define MEM_SDMODE_TMRD_N(N)	((N) << 9)
#define MEM_SDMODE_TWR_N(N)	((N) << 7)
#define MEM_SDMODE_TRP_N(N)	((N) << 5)
#define MEM_SDMODE_TRCD_N(N)	((N) << 3)
#define MEM_SDMODE_TCL_N(N)	((N) << 0)

/*
 * MEM_SDADDR register contents definitions
 */
#define MEM_SDADDR_E		(1 << 20)
#define MEM_SDADDR_CSBA 	(0x03FF << 10)
#define MEM_SDADDR_CSMASK	(0x03FF << 0)
#define MEM_SDADDR_CSBA_N(N)	((N) & (0x03FF << 22) >> 12)
#define MEM_SDADDR_CSMASK_N(N)	((N)&(0x03FF << 22) >> 22)

/*
 * MEM_SDREFCFG register content definitions
 */
#define MEM_SDREFCFG_TRC	(15 << 28)
#define MEM_SDREFCFG_TRPM	(3 << 26)
#define MEM_SDREFCFG_E		(1 << 25)
#define MEM_SDREFCFG_RE 	(0x1ffffff << 0)
#define MEM_SDREFCFG_TRC_N(N)	((N) << MEM_SDREFCFG_TRC)
#define MEM_SDREFCFG_TRPM_N(N)	((N) << MEM_SDREFCFG_TRPM)
#define MEM_SDREFCFG_REF_N(N)	(N)
#endif

/***********************************************************************/

/*
 * Au1550 SDRAM Register Offsets
 */

/***********************************************************************/

#if defined(CONFIG_SOC_AU1550) || defined(CONFIG_SOC_AU1200)
#define MEM_SDMODE0		0x0800
#define MEM_SDMODE1		0x0808
#define MEM_SDMODE2		0x0810
#define MEM_SDADDR0		0x0820
#define MEM_SDADDR1		0x0828
#define MEM_SDADDR2		0x0830
#define MEM_SDCONFIGA		0x0840
#define MEM_SDCONFIGB		0x0848
#define MEM_SDSTAT		0x0850
#define MEM_SDERRADDR		0x0858
#define MEM_SDSTRIDE0		0x0860
#define MEM_SDSTRIDE1		0x0868
#define MEM_SDSTRIDE2		0x0870
#define MEM_SDWRMD0		0x0880
#define MEM_SDWRMD1		0x0888
#define MEM_SDWRMD2		0x0890
#define MEM_SDPRECMD		0x08C0
#define MEM_SDAUTOREF		0x08C8
#define MEM_SDSREF		0x08D0
#define MEM_SDSLEEP		MEM_SDSREF

#endif

/*
 * Physical base addresses for integrated peripherals
 * 0..au1000 1..au1500 2..au1100 3..au1550 4..au1200
 */

#define AU1000_AC97_PHYS_ADDR		0x10000000 /* 012 */
#define AU1000_USB_OHCI_PHYS_ADDR	0x10100000 /* 012 */
#define AU1000_USB_UDC_PHYS_ADDR	0x10200000 /* 0123 */
#define AU1000_IC0_PHYS_ADDR		0x10400000 /* 01234 */
#define AU1000_MAC0_PHYS_ADDR		0x10500000 /* 023 */
#define AU1000_MAC1_PHYS_ADDR		0x10510000 /* 023 */
#define AU1000_MACEN_PHYS_ADDR		0x10520000 /* 023 */
#define AU1100_SD0_PHYS_ADDR		0x10600000 /* 24 */
#define AU1100_SD1_PHYS_ADDR		0x10680000 /* 24 */
#define AU1000_I2S_PHYS_ADDR		0x11000000 /* 02 */
#define AU1500_MAC0_PHYS_ADDR		0x11500000 /* 1 */
#define AU1500_MAC1_PHYS_ADDR		0x11510000 /* 1 */
#define AU1500_MACEN_PHYS_ADDR		0x11520000 /* 1 */
#define AU1000_UART0_PHYS_ADDR		0x11100000 /* 01234 */
#define AU1000_UART1_PHYS_ADDR		0x11200000 /* 0234 */
#define AU1000_UART2_PHYS_ADDR		0x11300000 /* 0 */
#define AU1000_UART3_PHYS_ADDR		0x11400000 /* 0123 */
#define AU1500_GPIO2_PHYS_ADDR		0x11700000 /* 1234 */
#define AU1000_IC1_PHYS_ADDR		0x11800000 /* 01234 */
#define AU1000_SYS_PHYS_ADDR		0x11900000 /* 01234 */
#define AU1000_DMA_PHYS_ADDR		0x14002000 /* 012 */
#define AU1550_DBDMA_PHYS_ADDR		0x14002000 /* 34 */
#define AU1550_DBDMA_CONF_PHYS_ADDR	0x14003000 /* 34 */
#define AU1000_MACDMA0_PHYS_ADDR	0x14004000 /* 0123 */
#define AU1000_MACDMA1_PHYS_ADDR	0x14004200 /* 0123 */
#define AU1550_USB_OHCI_PHYS_ADDR	0x14020000 /* 3 */
#define AU1200_USB_CTL_PHYS_ADDR	0x14020000 /* 4 */
#define AU1200_USB_OTG_PHYS_ADDR	0x14020020 /* 4 */
#define AU1200_USB_OHCI_PHYS_ADDR	0x14020100 /* 4 */
#define AU1200_USB_EHCI_PHYS_ADDR	0x14020200 /* 4 */
#define AU1200_USB_UDC_PHYS_ADDR	0x14022000 /* 4 */


#ifdef CONFIG_SOC_AU1000
#define	MEM_PHYS_ADDR		0x14000000
#define	STATIC_MEM_PHYS_ADDR	0x14001000
#define	IRDA_PHYS_ADDR		0x10300000
#define	SSI0_PHYS_ADDR		0x11600000
#define	SSI1_PHYS_ADDR		0x11680000
#define PCMCIA_IO_PHYS_ADDR	0xF00000000ULL
#define PCMCIA_ATTR_PHYS_ADDR	0xF40000000ULL
#define PCMCIA_MEM_PHYS_ADDR	0xF80000000ULL
#endif

/********************************************************************/

#ifdef CONFIG_SOC_AU1500
#define	MEM_PHYS_ADDR		0x14000000
#define	STATIC_MEM_PHYS_ADDR	0x14001000
#define PCI_PHYS_ADDR		0x14005000
#define PCI_MEM_PHYS_ADDR	0x400000000ULL
#define PCI_IO_PHYS_ADDR	0x500000000ULL
#define PCI_CONFIG0_PHYS_ADDR	0x600000000ULL
#define PCI_CONFIG1_PHYS_ADDR	0x680000000ULL
#define PCMCIA_IO_PHYS_ADDR	0xF00000000ULL
#define PCMCIA_ATTR_PHYS_ADDR	0xF40000000ULL
#define PCMCIA_MEM_PHYS_ADDR	0xF80000000ULL
#endif

/********************************************************************/

#ifdef CONFIG_SOC_AU1100
#define	MEM_PHYS_ADDR		0x14000000
#define	STATIC_MEM_PHYS_ADDR	0x14001000
#define	IRDA_PHYS_ADDR		0x10300000
#define	SSI0_PHYS_ADDR		0x11600000
#define	SSI1_PHYS_ADDR		0x11680000
#define LCD_PHYS_ADDR		0x15000000
#define PCMCIA_IO_PHYS_ADDR	0xF00000000ULL
#define PCMCIA_ATTR_PHYS_ADDR	0xF40000000ULL
#define PCMCIA_MEM_PHYS_ADDR	0xF80000000ULL
#endif

/***********************************************************************/

#ifdef CONFIG_SOC_AU1550
#define	MEM_PHYS_ADDR		0x14000000
#define	STATIC_MEM_PHYS_ADDR	0x14001000
#define PCI_PHYS_ADDR		0x14005000
#define PE_PHYS_ADDR		0x14008000
#define PSC0_PHYS_ADDR		0x11A00000
#define PSC1_PHYS_ADDR		0x11B00000
#define PSC2_PHYS_ADDR		0x10A00000
#define PSC3_PHYS_ADDR		0x10B00000
#define PCI_MEM_PHYS_ADDR	0x400000000ULL
#define PCI_IO_PHYS_ADDR	0x500000000ULL
#define PCI_CONFIG0_PHYS_ADDR	0x600000000ULL
#define PCI_CONFIG1_PHYS_ADDR	0x680000000ULL
#define PCMCIA_IO_PHYS_ADDR	0xF00000000ULL
#define PCMCIA_ATTR_PHYS_ADDR	0xF40000000ULL
#define PCMCIA_MEM_PHYS_ADDR	0xF80000000ULL
#endif

/***********************************************************************/

#ifdef CONFIG_SOC_AU1200
#define	MEM_PHYS_ADDR		0x14000000
#define	STATIC_MEM_PHYS_ADDR	0x14001000
#define AES_PHYS_ADDR		0x10300000
#define CIM_PHYS_ADDR		0x14004000
#define PSC0_PHYS_ADDR	 	0x11A00000
#define PSC1_PHYS_ADDR	 	0x11B00000
#define LCD_PHYS_ADDR		0x15000000
#define SWCNT_PHYS_ADDR		0x1110010C
#define MAEFE_PHYS_ADDR		0x14012000
#define MAEBE_PHYS_ADDR		0x14010000
#define PCMCIA_IO_PHYS_ADDR	0xF00000000ULL
#define PCMCIA_ATTR_PHYS_ADDR	0xF40000000ULL
#define PCMCIA_MEM_PHYS_ADDR	0xF80000000ULL
#endif

/* Static Bus Controller */
#define MEM_STCFG0		0xB4001000
#define MEM_STTIME0		0xB4001004
#define MEM_STADDR0		0xB4001008

#define MEM_STCFG1		0xB4001010
#define MEM_STTIME1		0xB4001014
#define MEM_STADDR1		0xB4001018

#define MEM_STCFG2		0xB4001020
#define MEM_STTIME2		0xB4001024
#define MEM_STADDR2		0xB4001028

#define MEM_STCFG3		0xB4001030
#define MEM_STTIME3		0xB4001034
#define MEM_STADDR3		0xB4001038

#if defined(CONFIG_SOC_AU1550) || defined(CONFIG_SOC_AU1200)
#define MEM_STNDCTL		0xB4001100
#define MEM_STSTAT		0xB4001104

#define MEM_STNAND_CMD		0x0
#define MEM_STNAND_ADDR 	0x4
#define MEM_STNAND_DATA 	0x20
#endif


/* Programmable Counters 0 and 1 */
#define SYS_BASE		0xB1900000
#define SYS_COUNTER_CNTRL	(SYS_BASE + 0x14)
#  define SYS_CNTRL_E1S 	(1 << 23)
#  define SYS_CNTRL_T1S 	(1 << 20)
#  define SYS_CNTRL_M21 	(1 << 19)
#  define SYS_CNTRL_M11 	(1 << 18)
#  define SYS_CNTRL_M01 	(1 << 17)
#  define SYS_CNTRL_C1S 	(1 << 16)
#  define SYS_CNTRL_BP		(1 << 14)
#  define SYS_CNTRL_EN1 	(1 << 13)
#  define SYS_CNTRL_BT1 	(1 << 12)
#  define SYS_CNTRL_EN0 	(1 << 11)
#  define SYS_CNTRL_BT0 	(1 << 10)
#  define SYS_CNTRL_E0		(1 << 8)
#  define SYS_CNTRL_E0S 	(1 << 7)
#  define SYS_CNTRL_32S 	(1 << 5)
#  define SYS_CNTRL_T0S 	(1 << 4)
#  define SYS_CNTRL_M20 	(1 << 3)
#  define SYS_CNTRL_M10 	(1 << 2)
#  define SYS_CNTRL_M00 	(1 << 1)
#  define SYS_CNTRL_C0S 	(1 << 0)

/* Programmable Counter 0 Registers */
#define SYS_TOYTRIM		(SYS_BASE + 0)
#define SYS_TOYWRITE		(SYS_BASE + 4)
#define SYS_TOYMATCH0		(SYS_BASE + 8)
#define SYS_TOYMATCH1		(SYS_BASE + 0xC)
#define SYS_TOYMATCH2		(SYS_BASE + 0x10)
#define SYS_TOYREAD		(SYS_BASE + 0x40)

/* Programmable Counter 1 Registers */
#define SYS_RTCTRIM		(SYS_BASE + 0x44)
#define SYS_RTCWRITE		(SYS_BASE + 0x48)
#define SYS_RTCMATCH0		(SYS_BASE + 0x4C)
#define SYS_RTCMATCH1		(SYS_BASE + 0x50)
#define SYS_RTCMATCH2		(SYS_BASE + 0x54)
#define SYS_RTCREAD		(SYS_BASE + 0x58)

/* I2S Controller */
#define I2S_DATA		0xB1000000
#  define I2S_DATA_MASK 	0xffffff
#define I2S_CONFIG		0xB1000004
#  define I2S_CONFIG_XU 	(1 << 25)
#  define I2S_CONFIG_XO 	(1 << 24)
#  define I2S_CONFIG_RU 	(1 << 23)
#  define I2S_CONFIG_RO 	(1 << 22)
#  define I2S_CONFIG_TR 	(1 << 21)
#  define I2S_CONFIG_TE 	(1 << 20)
#  define I2S_CONFIG_TF 	(1 << 19)
#  define I2S_CONFIG_RR 	(1 << 18)
#  define I2S_CONFIG_RE 	(1 << 17)
#  define I2S_CONFIG_RF 	(1 << 16)
#  define I2S_CONFIG_PD 	(1 << 11)
#  define I2S_CONFIG_LB 	(1 << 10)
#  define I2S_CONFIG_IC 	(1 << 9)
#  define I2S_CONFIG_FM_BIT	7
#  define I2S_CONFIG_FM_MASK	(0x3 << I2S_CONFIG_FM_BIT)
#    define I2S_CONFIG_FM_I2S	(0x0 << I2S_CONFIG_FM_BIT)
#    define I2S_CONFIG_FM_LJ	(0x1 << I2S_CONFIG_FM_BIT)
#    define I2S_CONFIG_FM_RJ	(0x2 << I2S_CONFIG_FM_BIT)
#  define I2S_CONFIG_TN 	(1 << 6)
#  define I2S_CONFIG_RN 	(1 << 5)
#  define I2S_CONFIG_SZ_BIT	0
#  define I2S_CONFIG_SZ_MASK	(0x1F << I2S_CONFIG_SZ_BIT)

#define I2S_CONTROL		0xB1000008
#  define I2S_CONTROL_D 	(1 << 1)
#  define I2S_CONTROL_CE	(1 << 0)


/* Ethernet Controllers  */

/* 4 byte offsets from AU1000_ETH_BASE */
#define MAC_CONTROL		0x0
#  define MAC_RX_ENABLE 	(1 << 2)
#  define MAC_TX_ENABLE 	(1 << 3)
#  define MAC_DEF_CHECK 	(1 << 5)
#  define MAC_SET_BL(X) 	(((X) & 0x3) << 6)
#  define MAC_AUTO_PAD		(1 << 8)
#  define MAC_DISABLE_RETRY	(1 << 10)
#  define MAC_DISABLE_BCAST	(1 << 11)
#  define MAC_LATE_COL		(1 << 12)
#  define MAC_HASH_MODE 	(1 << 13)
#  define MAC_HASH_ONLY 	(1 << 15)
#  define MAC_PASS_ALL		(1 << 16)
#  define MAC_INVERSE_FILTER	(1 << 17)
#  define MAC_PROMISCUOUS	(1 << 18)
#  define MAC_PASS_ALL_MULTI	(1 << 19)
#  define MAC_FULL_DUPLEX	(1 << 20)
#  define MAC_NORMAL_MODE	0
#  define MAC_INT_LOOPBACK	(1 << 21)
#  define MAC_EXT_LOOPBACK	(1 << 22)
#  define MAC_DISABLE_RX_OWN	(1 << 23)
#  define MAC_BIG_ENDIAN	(1 << 30)
#  define MAC_RX_ALL		(1 << 31)
#define MAC_ADDRESS_HIGH	0x4
#define MAC_ADDRESS_LOW		0x8
#define MAC_MCAST_HIGH		0xC
#define MAC_MCAST_LOW		0x10
#define MAC_MII_CNTRL		0x14
#  define MAC_MII_BUSY		(1 << 0)
#  define MAC_MII_READ		0
#  define MAC_MII_WRITE		(1 << 1)
#  define MAC_SET_MII_SELECT_REG(X) (((X) & 0x1f) << 6)
#  define MAC_SET_MII_SELECT_PHY(X) (((X) & 0x1f) << 11)
#define MAC_MII_DATA		0x18
#define MAC_FLOW_CNTRL		0x1C
#  define MAC_FLOW_CNTRL_BUSY	(1 << 0)
#  define MAC_FLOW_CNTRL_ENABLE (1 << 1)
#  define MAC_PASS_CONTROL	(1 << 2)
#  define MAC_SET_PAUSE(X)	(((X) & 0xffff) << 16)
#define MAC_VLAN1_TAG		0x20
#define MAC_VLAN2_TAG		0x24

/* Ethernet Controller Enable */

#  define MAC_EN_CLOCK_ENABLE	(1 << 0)
#  define MAC_EN_RESET0		(1 << 1)
#  define MAC_EN_TOSS		(0 << 2)
#  define MAC_EN_CACHEABLE	(1 << 3)
#  define MAC_EN_RESET1 	(1 << 4)
#  define MAC_EN_RESET2 	(1 << 5)
#  define MAC_DMA_RESET 	(1 << 6)

/* Ethernet Controller DMA Channels */

#define MAC0_TX_DMA_ADDR	0xB4004000
#define MAC1_TX_DMA_ADDR	0xB4004200
/* offsets from MAC_TX_RING_ADDR address */
#define MAC_TX_BUFF0_STATUS	0x0
#  define TX_FRAME_ABORTED	(1 << 0)
#  define TX_JAB_TIMEOUT	(1 << 1)
#  define TX_NO_CARRIER 	(1 << 2)
#  define TX_LOSS_CARRIER	(1 << 3)
#  define TX_EXC_DEF		(1 << 4)
#  define TX_LATE_COLL_ABORT	(1 << 5)
#  define TX_EXC_COLL		(1 << 6)
#  define TX_UNDERRUN		(1 << 7)
#  define TX_DEFERRED		(1 << 8)
#  define TX_LATE_COLL		(1 << 9)
#  define TX_COLL_CNT_MASK	(0xF << 10)
#  define TX_PKT_RETRY		(1 << 31)
#define MAC_TX_BUFF0_ADDR	0x4
#  define TX_DMA_ENABLE 	(1 << 0)
#  define TX_T_DONE		(1 << 1)
#  define TX_GET_DMA_BUFFER(X)	(((X) >> 2) & 0x3)
#define MAC_TX_BUFF0_LEN	0x8
#define MAC_TX_BUFF1_STATUS	0x10
#define MAC_TX_BUFF1_ADDR	0x14
#define MAC_TX_BUFF1_LEN	0x18
#define MAC_TX_BUFF2_STATUS	0x20
#define MAC_TX_BUFF2_ADDR	0x24
#define MAC_TX_BUFF2_LEN	0x28
#define MAC_TX_BUFF3_STATUS	0x30
#define MAC_TX_BUFF3_ADDR	0x34
#define MAC_TX_BUFF3_LEN	0x38

#define MAC0_RX_DMA_ADDR	0xB4004100
#define MAC1_RX_DMA_ADDR	0xB4004300
/* offsets from MAC_RX_RING_ADDR */
#define MAC_RX_BUFF0_STATUS	0x0
#  define RX_FRAME_LEN_MASK	0x3fff
#  define RX_WDOG_TIMER 	(1 << 14)
#  define RX_RUNT		(1 << 15)
#  define RX_OVERLEN		(1 << 16)
#  define RX_COLL		(1 << 17)
#  define RX_ETHER		(1 << 18)
#  define RX_MII_ERROR		(1 << 19)
#  define RX_DRIBBLING		(1 << 20)
#  define RX_CRC_ERROR		(1 << 21)
#  define RX_VLAN1		(1 << 22)
#  define RX_VLAN2		(1 << 23)
#  define RX_LEN_ERROR		(1 << 24)
#  define RX_CNTRL_FRAME	(1 << 25)
#  define RX_U_CNTRL_FRAME	(1 << 26)
#  define RX_MCAST_FRAME	(1 << 27)
#  define RX_BCAST_FRAME	(1 << 28)
#  define RX_FILTER_FAIL	(1 << 29)
#  define RX_PACKET_FILTER	(1 << 30)
#  define RX_MISSED_FRAME	(1 << 31)

#  define RX_ERROR (RX_WDOG_TIMER | RX_RUNT | RX_OVERLEN |  \
		    RX_COLL | RX_MII_ERROR | RX_CRC_ERROR | \
		    RX_LEN_ERROR | RX_U_CNTRL_FRAME | RX_MISSED_FRAME)
#define MAC_RX_BUFF0_ADDR	0x4
#  define RX_DMA_ENABLE 	(1 << 0)
#  define RX_T_DONE		(1 << 1)
#  define RX_GET_DMA_BUFFER(X)	(((X) >> 2) & 0x3)
#  define RX_SET_BUFF_ADDR(X)	((X) & 0xffffffc0)
#define MAC_RX_BUFF1_STATUS	0x10
#define MAC_RX_BUFF1_ADDR	0x14
#define MAC_RX_BUFF2_STATUS	0x20
#define MAC_RX_BUFF2_ADDR	0x24
#define MAC_RX_BUFF3_STATUS	0x30
#define MAC_RX_BUFF3_ADDR	0x34

#define UART_RX		0	/* Receive buffer */
#define UART_TX		4	/* Transmit buffer */
#define UART_IER	8	/* Interrupt Enable Register */
#define UART_IIR	0xC	/* Interrupt ID Register */
#define UART_FCR	0x10	/* FIFO Control Register */
#define UART_LCR	0x14	/* Line Control Register */
#define UART_MCR	0x18	/* Modem Control Register */
#define UART_LSR	0x1C	/* Line Status Register */
#define UART_MSR	0x20	/* Modem Status Register */
#define UART_CLK	0x28	/* Baud Rate Clock Divider */
#define UART_MOD_CNTRL	0x100	/* Module Control */

/* SSIO */
#define SSI0_STATUS		0xB1600000
#  define SSI_STATUS_BF 	(1 << 4)
#  define SSI_STATUS_OF 	(1 << 3)
#  define SSI_STATUS_UF 	(1 << 2)
#  define SSI_STATUS_D		(1 << 1)
#  define SSI_STATUS_B		(1 << 0)
#define SSI0_INT		0xB1600004
#  define SSI_INT_OI		(1 << 3)
#  define SSI_INT_UI		(1 << 2)
#  define SSI_INT_DI		(1 << 1)
#define SSI0_INT_ENABLE 	0xB1600008
#  define SSI_INTE_OIE		(1 << 3)
#  define SSI_INTE_UIE		(1 << 2)
#  define SSI_INTE_DIE		(1 << 1)
#define SSI0_CONFIG		0xB1600020
#  define SSI_CONFIG_AO 	(1 << 24)
#  define SSI_CONFIG_DO 	(1 << 23)
#  define SSI_CONFIG_ALEN_BIT	20
#  define SSI_CONFIG_ALEN_MASK	(0x7 << 20)
#  define SSI_CONFIG_DLEN_BIT	16
#  define SSI_CONFIG_DLEN_MASK	(0x7 << 16)
#  define SSI_CONFIG_DD 	(1 << 11)
#  define SSI_CONFIG_AD 	(1 << 10)
#  define SSI_CONFIG_BM_BIT	8
#  define SSI_CONFIG_BM_MASK	(0x3 << 8)
#  define SSI_CONFIG_CE 	(1 << 7)
#  define SSI_CONFIG_DP 	(1 << 6)
#  define SSI_CONFIG_DL 	(1 << 5)
#  define SSI_CONFIG_EP 	(1 << 4)
#define SSI0_ADATA		0xB1600024
#  define SSI_AD_D		(1 << 24)
#  define SSI_AD_ADDR_BIT	16
#  define SSI_AD_ADDR_MASK	(0xff << 16)
#  define SSI_AD_DATA_BIT	0
#  define SSI_AD_DATA_MASK	(0xfff << 0)
#define SSI0_CLKDIV		0xB1600028
#define SSI0_CONTROL		0xB1600100
#  define SSI_CONTROL_CD	(1 << 1)
#  define SSI_CONTROL_E 	(1 << 0)

/* SSI1 */
#define SSI1_STATUS		0xB1680000
#define SSI1_INT		0xB1680004
#define SSI1_INT_ENABLE 	0xB1680008
#define SSI1_CONFIG		0xB1680020
#define SSI1_ADATA		0xB1680024
#define SSI1_CLKDIV		0xB1680028
#define SSI1_ENABLE		0xB1680100

/*
 * Register content definitions
 */
#define SSI_STATUS_BF		(1 << 4)
#define SSI_STATUS_OF		(1 << 3)
#define SSI_STATUS_UF		(1 << 2)
#define SSI_STATUS_D		(1 << 1)
#define SSI_STATUS_B		(1 << 0)

/* SSI_INT */
#define SSI_INT_OI		(1 << 3)
#define SSI_INT_UI		(1 << 2)
#define SSI_INT_DI		(1 << 1)

/* SSI_INTEN */
#define SSI_INTEN_OIE		(1 << 3)
#define SSI_INTEN_UIE		(1 << 2)
#define SSI_INTEN_DIE		(1 << 1)

#define SSI_CONFIG_AO		(1 << 24)
#define SSI_CONFIG_DO		(1 << 23)
#define SSI_CONFIG_ALEN 	(7 << 20)
#define SSI_CONFIG_DLEN 	(15 << 16)
#define SSI_CONFIG_DD		(1 << 11)
#define SSI_CONFIG_AD		(1 << 10)
#define SSI_CONFIG_BM		(3 << 8)
#define SSI_CONFIG_CE		(1 << 7)
#define SSI_CONFIG_DP		(1 << 6)
#define SSI_CONFIG_DL		(1 << 5)
#define SSI_CONFIG_EP		(1 << 4)
#define SSI_CONFIG_ALEN_N(N)	((N-1) << 20)
#define SSI_CONFIG_DLEN_N(N)	((N-1) << 16)
#define SSI_CONFIG_BM_HI	(0 << 8)
#define SSI_CONFIG_BM_LO	(1 << 8)
#define SSI_CONFIG_BM_CY	(2 << 8)

#define SSI_ADATA_D		(1 << 24)
#define SSI_ADATA_ADDR		(0xFF << 16)
#define SSI_ADATA_DATA		0x0FFF
#define SSI_ADATA_ADDR_N(N)	(N << 16)

#define SSI_ENABLE_CD		(1 << 1)
#define SSI_ENABLE_E		(1 << 0)

/* IrDA Controller */
#define IRDA_BASE		0xB0300000
#define IR_RING_PTR_STATUS	(IRDA_BASE + 0x00)
#define IR_RING_BASE_ADDR_H	(IRDA_BASE + 0x04)
#define IR_RING_BASE_ADDR_L	(IRDA_BASE + 0x08)
#define IR_RING_SIZE		(IRDA_BASE + 0x0C)
#define IR_RING_PROMPT		(IRDA_BASE + 0x10)
#define IR_RING_ADDR_CMPR	(IRDA_BASE + 0x14)
#define IR_INT_CLEAR		(IRDA_BASE + 0x18)
#define IR_CONFIG_1		(IRDA_BASE + 0x20)
#  define IR_RX_INVERT_LED	(1 << 0)
#  define IR_TX_INVERT_LED	(1 << 1)
#  define IR_ST 		(1 << 2)
#  define IR_SF 		(1 << 3)
#  define IR_SIR		(1 << 4)
#  define IR_MIR		(1 << 5)
#  define IR_FIR		(1 << 6)
#  define IR_16CRC		(1 << 7)
#  define IR_TD 		(1 << 8)
#  define IR_RX_ALL		(1 << 9)
#  define IR_DMA_ENABLE 	(1 << 10)
#  define IR_RX_ENABLE		(1 << 11)
#  define IR_TX_ENABLE		(1 << 12)
#  define IR_LOOPBACK		(1 << 14)
#  define IR_SIR_MODE		(IR_SIR | IR_DMA_ENABLE | \
				 IR_RX_ALL | IR_RX_ENABLE | IR_SF | IR_16CRC)
#define IR_SIR_FLAGS		(IRDA_BASE + 0x24)
#define IR_ENABLE		(IRDA_BASE + 0x28)
#  define IR_RX_STATUS		(1 << 9)
#  define IR_TX_STATUS		(1 << 10)
#define IR_READ_PHY_CONFIG	(IRDA_BASE + 0x2C)
#define IR_WRITE_PHY_CONFIG	(IRDA_BASE + 0x30)
#define IR_MAX_PKT_LEN		(IRDA_BASE + 0x34)
#define IR_RX_BYTE_CNT		(IRDA_BASE + 0x38)
#define IR_CONFIG_2		(IRDA_BASE + 0x3C)
#  define IR_MODE_INV		(1 << 0)
#  define IR_ONE_PIN		(1 << 1)
#define IR_INTERFACE_CONFIG	(IRDA_BASE + 0x40)

/* GPIO */
#define SYS_PINFUNC		0xB190002C
#  define SYS_PF_USB		(1 << 15)	/* 2nd USB device/host */
#  define SYS_PF_U3		(1 << 14)	/* GPIO23/U3TXD */
#  define SYS_PF_U2		(1 << 13)	/* GPIO22/U2TXD */
#  define SYS_PF_U1		(1 << 12)	/* GPIO21/U1TXD */
#  define SYS_PF_SRC		(1 << 11)	/* GPIO6/SROMCKE */
#  define SYS_PF_CK5		(1 << 10)	/* GPIO3/CLK5 */
#  define SYS_PF_CK4		(1 << 9)	/* GPIO2/CLK4 */
#  define SYS_PF_IRF		(1 << 8)	/* GPIO15/IRFIRSEL */
#  define SYS_PF_UR3		(1 << 7)	/* GPIO[14:9]/UART3 */
#  define SYS_PF_I2D		(1 << 6)	/* GPIO8/I2SDI */
#  define SYS_PF_I2S		(1 << 5)	/* I2S/GPIO[29:31] */
#  define SYS_PF_NI2		(1 << 4)	/* NI2/GPIO[24:28] */
#  define SYS_PF_U0		(1 << 3)	/* U0TXD/GPIO20 */
#  define SYS_PF_RD		(1 << 2)	/* IRTXD/GPIO19 */
#  define SYS_PF_A97		(1 << 1)	/* AC97/SSL1 */
#  define SYS_PF_S0		(1 << 0)	/* SSI_0/GPIO[16:18] */

/* Au1100 only */
#  define SYS_PF_PC		(1 << 18)	/* PCMCIA/GPIO[207:204] */
#  define SYS_PF_LCD		(1 << 17)	/* extern lcd/GPIO[203:200] */
#  define SYS_PF_CS		(1 << 16)	/* EXTCLK0/32KHz to gpio2 */
#  define SYS_PF_EX0		(1 << 9)	/* GPIO2/clock */

/* Au1550 only.  Redefines lots of pins */
#  define SYS_PF_PSC2_MASK	(7 << 17)
#  define SYS_PF_PSC2_AC97	0
#  define SYS_PF_PSC2_SPI	0
#  define SYS_PF_PSC2_I2S	(1 << 17)
#  define SYS_PF_PSC2_SMBUS	(3 << 17)
#  define SYS_PF_PSC2_GPIO	(7 << 17)
#  define SYS_PF_PSC3_MASK	(7 << 20)
#  define SYS_PF_PSC3_AC97	0
#  define SYS_PF_PSC3_SPI	0
#  define SYS_PF_PSC3_I2S	(1 << 20)
#  define SYS_PF_PSC3_SMBUS	(3 << 20)
#  define SYS_PF_PSC3_GPIO	(7 << 20)
#  define SYS_PF_PSC1_S1	(1 << 1)
#  define SYS_PF_MUST_BE_SET	((1 << 5) | (1 << 2))

/* Au1200 only */
#ifdef CONFIG_SOC_AU1200
#define SYS_PINFUNC_DMA 	(1 << 31)
#define SYS_PINFUNC_S0A 	(1 << 30)
#define SYS_PINFUNC_S1A 	(1 << 29)
#define SYS_PINFUNC_LP0 	(1 << 28)
#define SYS_PINFUNC_LP1 	(1 << 27)
#define SYS_PINFUNC_LD16 	(1 << 26)
#define SYS_PINFUNC_LD8 	(1 << 25)
#define SYS_PINFUNC_LD1 	(1 << 24)
#define SYS_PINFUNC_LD0 	(1 << 23)
#define SYS_PINFUNC_P1A 	(3 << 21)
#define SYS_PINFUNC_P1B 	(1 << 20)
#define SYS_PINFUNC_FS3 	(1 << 19)
#define SYS_PINFUNC_P0A 	(3 << 17)
#define SYS_PINFUNC_CS		(1 << 16)
#define SYS_PINFUNC_CIM 	(1 << 15)
#define SYS_PINFUNC_P1C 	(1 << 14)
#define SYS_PINFUNC_U1T 	(1 << 12)
#define SYS_PINFUNC_U1R 	(1 << 11)
#define SYS_PINFUNC_EX1 	(1 << 10)
#define SYS_PINFUNC_EX0 	(1 << 9)
#define SYS_PINFUNC_U0R 	(1 << 8)
#define SYS_PINFUNC_MC		(1 << 7)
#define SYS_PINFUNC_S0B 	(1 << 6)
#define SYS_PINFUNC_S0C 	(1 << 5)
#define SYS_PINFUNC_P0B 	(1 << 4)
#define SYS_PINFUNC_U0T 	(1 << 3)
#define SYS_PINFUNC_S1B 	(1 << 2)
#endif

/* Power Management */
#define SYS_SCRATCH0		0xB1900018
#define SYS_SCRATCH1		0xB190001C
#define SYS_WAKEMSK		0xB1900034
#define SYS_ENDIAN		0xB1900038
#define SYS_POWERCTRL		0xB190003C
#define SYS_WAKESRC		0xB190005C
#define SYS_SLPPWR		0xB1900078
#define SYS_SLEEP		0xB190007C

#define SYS_WAKEMSK_D2		(1 << 9)
#define SYS_WAKEMSK_M2		(1 << 8)
#define SYS_WAKEMSK_GPIO(x)	(1 << (x))

/* Clock Controller */
#define SYS_FREQCTRL0		0xB1900020
#  define SYS_FC_FRDIV2_BIT	22
#  define SYS_FC_FRDIV2_MASK	(0xff << SYS_FC_FRDIV2_BIT)
#  define SYS_FC_FE2		(1 << 21)
#  define SYS_FC_FS2		(1 << 20)
#  define SYS_FC_FRDIV1_BIT	12
#  define SYS_FC_FRDIV1_MASK	(0xff << SYS_FC_FRDIV1_BIT)
#  define SYS_FC_FE1		(1 << 11)
#  define SYS_FC_FS1		(1 << 10)
#  define SYS_FC_FRDIV0_BIT	2
#  define SYS_FC_FRDIV0_MASK	(0xff << SYS_FC_FRDIV0_BIT)
#  define SYS_FC_FE0		(1 << 1)
#  define SYS_FC_FS0		(1 << 0)
#define SYS_FREQCTRL1		0xB1900024
#  define SYS_FC_FRDIV5_BIT	22
#  define SYS_FC_FRDIV5_MASK	(0xff << SYS_FC_FRDIV5_BIT)
#  define SYS_FC_FE5		(1 << 21)
#  define SYS_FC_FS5		(1 << 20)
#  define SYS_FC_FRDIV4_BIT	12
#  define SYS_FC_FRDIV4_MASK	(0xff << SYS_FC_FRDIV4_BIT)
#  define SYS_FC_FE4		(1 << 11)
#  define SYS_FC_FS4		(1 << 10)
#  define SYS_FC_FRDIV3_BIT	2
#  define SYS_FC_FRDIV3_MASK	(0xff << SYS_FC_FRDIV3_BIT)
#  define SYS_FC_FE3		(1 << 1)
#  define SYS_FC_FS3		(1 << 0)
#define SYS_CLKSRC		0xB1900028
#  define SYS_CS_ME1_BIT	27
#  define SYS_CS_ME1_MASK	(0x7 << SYS_CS_ME1_BIT)
#  define SYS_CS_DE1		(1 << 26)
#  define SYS_CS_CE1		(1 << 25)
#  define SYS_CS_ME0_BIT	22
#  define SYS_CS_ME0_MASK	(0x7 << SYS_CS_ME0_BIT)
#  define SYS_CS_DE0		(1 << 21)
#  define SYS_CS_CE0		(1 << 20)
#  define SYS_CS_MI2_BIT	17
#  define SYS_CS_MI2_MASK	(0x7 << SYS_CS_MI2_BIT)
#  define SYS_CS_DI2		(1 << 16)
#  define SYS_CS_CI2		(1 << 15)
#ifdef CONFIG_SOC_AU1100
#  define SYS_CS_ML_BIT 	7
#  define SYS_CS_ML_MASK	(0x7 << SYS_CS_ML_BIT)
#  define SYS_CS_DL		(1 << 6)
#  define SYS_CS_CL		(1 << 5)
#else
#  define SYS_CS_MUH_BIT	12
#  define SYS_CS_MUH_MASK	(0x7 << SYS_CS_MUH_BIT)
#  define SYS_CS_DUH		(1 << 11)
#  define SYS_CS_CUH		(1 << 10)
#  define SYS_CS_MUD_BIT	7
#  define SYS_CS_MUD_MASK	(0x7 << SYS_CS_MUD_BIT)
#  define SYS_CS_DUD		(1 << 6)
#  define SYS_CS_CUD		(1 << 5)
#endif
#  define SYS_CS_MIR_BIT	2
#  define SYS_CS_MIR_MASK	(0x7 << SYS_CS_MIR_BIT)
#  define SYS_CS_DIR		(1 << 1)
#  define SYS_CS_CIR		(1 << 0)

#  define SYS_CS_MUX_AUX	0x1
#  define SYS_CS_MUX_FQ0	0x2
#  define SYS_CS_MUX_FQ1	0x3
#  define SYS_CS_MUX_FQ2	0x4
#  define SYS_CS_MUX_FQ3	0x5
#  define SYS_CS_MUX_FQ4	0x6
#  define SYS_CS_MUX_FQ5	0x7
#define SYS_CPUPLL		0xB1900060
#define SYS_AUXPLL		0xB1900064

/* AC97 Controller */
#define AC97C_CONFIG		0xB0000000
#  define AC97C_RECV_SLOTS_BIT	13
#  define AC97C_RECV_SLOTS_MASK (0x3ff << AC97C_RECV_SLOTS_BIT)
#  define AC97C_XMIT_SLOTS_BIT	3
#  define AC97C_XMIT_SLOTS_MASK (0x3ff << AC97C_XMIT_SLOTS_BIT)
#  define AC97C_SG		(1 << 2)
#  define AC97C_SYNC		(1 << 1)
#  define AC97C_RESET		(1 << 0)
#define AC97C_STATUS		0xB0000004
#  define AC97C_XU		(1 << 11)
#  define AC97C_XO		(1 << 10)
#  define AC97C_RU		(1 << 9)
#  define AC97C_RO		(1 << 8)
#  define AC97C_READY		(1 << 7)
#  define AC97C_CP		(1 << 6)
#  define AC97C_TR		(1 << 5)
#  define AC97C_TE		(1 << 4)
#  define AC97C_TF		(1 << 3)
#  define AC97C_RR		(1 << 2)
#  define AC97C_RE		(1 << 1)
#  define AC97C_RF		(1 << 0)
#define AC97C_DATA		0xB0000008
#define AC97C_CMD		0xB000000C
#  define AC97C_WD_BIT		16
#  define AC97C_READ		(1 << 7)
#  define AC97C_INDEX_MASK	0x7f
#define AC97C_CNTRL		0xB0000010
#  define AC97C_RS		(1 << 1)
#  define AC97C_CE		(1 << 0)

#if defined(CONFIG_SOC_AU1500) || defined(CONFIG_SOC_AU1550)
/* Au1500 PCI Controller */
#define Au1500_CFG_BASE 	0xB4005000	/* virtual, KSEG1 addr */
#define Au1500_PCI_CMEM 	(Au1500_CFG_BASE + 0)
#define Au1500_PCI_CFG		(Au1500_CFG_BASE + 4)
#  define PCI_ERROR		((1 << 22) | (1 << 23) | (1 << 24) | \
				 (1 << 25) | (1 << 26) | (1 << 27))
#define Au1500_PCI_B2BMASK_CCH	(Au1500_CFG_BASE + 8)
#define Au1500_PCI_B2B0_VID	(Au1500_CFG_BASE + 0xC)
#define Au1500_PCI_B2B1_ID	(Au1500_CFG_BASE + 0x10)
#define Au1500_PCI_MWMASK_DEV	(Au1500_CFG_BASE + 0x14)
#define Au1500_PCI_MWBASE_REV_CCL (Au1500_CFG_BASE + 0x18)
#define Au1500_PCI_ERR_ADDR	(Au1500_CFG_BASE + 0x1C)
#define Au1500_PCI_SPEC_INTACK	(Au1500_CFG_BASE + 0x20)
#define Au1500_PCI_ID		(Au1500_CFG_BASE + 0x100)
#define Au1500_PCI_STATCMD	(Au1500_CFG_BASE + 0x104)
#define Au1500_PCI_CLASSREV	(Au1500_CFG_BASE + 0x108)
#define Au1500_PCI_HDRTYPE	(Au1500_CFG_BASE + 0x10C)
#define Au1500_PCI_MBAR 	(Au1500_CFG_BASE + 0x110)

#define Au1500_PCI_HDR		0xB4005100	/* virtual, KSEG1 addr */

/*
 * All of our structures, like PCI resource, have 32-bit members.
 * Drivers are expected to do an ioremap on the PCI MEM resource, but it's
 * hard to store 0x4 0000 0000 in a 32-bit type.  We require a small patch
 * to __ioremap to check for addresses between (u32)Au1500_PCI_MEM_START and
 * (u32)Au1500_PCI_MEM_END and change those to the full 36-bit PCI MEM
 * addresses.  For PCI I/O, it's simpler because we get to do the ioremap
 * ourselves and then adjust the device's resources.
 */
#define Au1500_EXT_CFG		0x600000000ULL
#define Au1500_EXT_CFG_TYPE1	0x680000000ULL
#define Au1500_PCI_IO_START	0x500000000ULL
#define Au1500_PCI_IO_END	0x5000FFFFFULL
#define Au1500_PCI_MEM_START	0x440000000ULL
#define Au1500_PCI_MEM_END	0x44FFFFFFFULL

#define PCI_IO_START	0x00001000
#define PCI_IO_END	0x000FFFFF
#define PCI_MEM_START	0x40000000
#define PCI_MEM_END	0x4FFFFFFF

#define PCI_FIRST_DEVFN (0 << 3)
#define PCI_LAST_DEVFN	(19 << 3)

#define IOPORT_RESOURCE_START	0x00001000	/* skip legacy probing */
#define IOPORT_RESOURCE_END	0xffffffff
#define IOMEM_RESOURCE_START	0x10000000
#define IOMEM_RESOURCE_END	0xfffffffffULL

#else /* Au1000 and Au1100 and Au1200 */

/* Don't allow any legacy ports probing */
#define IOPORT_RESOURCE_START	0x10000000
#define IOPORT_RESOURCE_END	0xffffffff
#define IOMEM_RESOURCE_START	0x10000000
#define IOMEM_RESOURCE_END	0xfffffffffULL

#define PCI_IO_START	0
#define PCI_IO_END	0
#define PCI_MEM_START	0
#define PCI_MEM_END	0
#define PCI_FIRST_DEVFN 0
#define PCI_LAST_DEVFN	0

#endif

#endif
