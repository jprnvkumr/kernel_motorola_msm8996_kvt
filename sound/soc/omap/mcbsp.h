/*
 * sound/soc/omap/mcbsp.h
 *
 * OMAP Multi-Channel Buffered Serial Port
 *
 * Contact: Jarkko Nikula <jarkko.nikula@bitmer.com>
 *          Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#ifndef __ASOC_MCBSP_H
#define __ASOC_MCBSP_H

#include "omap-pcm.h"

/* McBSP register numbers. Register address offset = num * reg_step */
enum {
	/* Common registers */
	OMAP_MCBSP_REG_SPCR2 = 4,
	OMAP_MCBSP_REG_SPCR1,
	OMAP_MCBSP_REG_RCR2,
	OMAP_MCBSP_REG_RCR1,
	OMAP_MCBSP_REG_XCR2,
	OMAP_MCBSP_REG_XCR1,
	OMAP_MCBSP_REG_SRGR2,
	OMAP_MCBSP_REG_SRGR1,
	OMAP_MCBSP_REG_MCR2,
	OMAP_MCBSP_REG_MCR1,
	OMAP_MCBSP_REG_RCERA,
	OMAP_MCBSP_REG_RCERB,
	OMAP_MCBSP_REG_XCERA,
	OMAP_MCBSP_REG_XCERB,
	OMAP_MCBSP_REG_PCR0,
	OMAP_MCBSP_REG_RCERC,
	OMAP_MCBSP_REG_RCERD,
	OMAP_MCBSP_REG_XCERC,
	OMAP_MCBSP_REG_XCERD,
	OMAP_MCBSP_REG_RCERE,
	OMAP_MCBSP_REG_RCERF,
	OMAP_MCBSP_REG_XCERE,
	OMAP_MCBSP_REG_XCERF,
	OMAP_MCBSP_REG_RCERG,
	OMAP_MCBSP_REG_RCERH,
	OMAP_MCBSP_REG_XCERG,
	OMAP_MCBSP_REG_XCERH,

	/* OMAP1-OMAP2420 registers */
	OMAP_MCBSP_REG_DRR2 = 0,
	OMAP_MCBSP_REG_DRR1,
	OMAP_MCBSP_REG_DXR2,
	OMAP_MCBSP_REG_DXR1,

	/* OMAP2430 and onwards */
	OMAP_MCBSP_REG_DRR = 0,
	OMAP_MCBSP_REG_DXR = 2,
	OMAP_MCBSP_REG_SYSCON =	35,
	OMAP_MCBSP_REG_THRSH2,
	OMAP_MCBSP_REG_THRSH1,
	OMAP_MCBSP_REG_IRQST = 40,
	OMAP_MCBSP_REG_IRQEN,
	OMAP_MCBSP_REG_WAKEUPEN,
	OMAP_MCBSP_REG_XCCR,
	OMAP_MCBSP_REG_RCCR,
	OMAP_MCBSP_REG_XBUFFSTAT,
	OMAP_MCBSP_REG_RBUFFSTAT,
	OMAP_MCBSP_REG_SSELCR,
};

/* OMAP3 sidetone control registers */
#define OMAP_ST_REG_REV		0x00
#define OMAP_ST_REG_SYSCONFIG	0x10
#define OMAP_ST_REG_IRQSTATUS	0x18
#define OMAP_ST_REG_IRQENABLE	0x1C
#define OMAP_ST_REG_SGAINCR	0x24
#define OMAP_ST_REG_SFIRCR	0x28
#define OMAP_ST_REG_SSELCR	0x2C

/************************** McBSP SPCR1 bit definitions ***********************/
#define RRST			0x0001
#define RRDY			0x0002
#define RFULL			0x0004
#define RSYNC_ERR		0x0008
#define RINTM(value)		((value)<<4)	/* bits 4:5 */
#define ABIS			0x0040
#define DXENA			0x0080
#define CLKSTP(value)		((value)<<11)	/* bits 11:12 */
#define RJUST(value)		((value)<<13)	/* bits 13:14 */
#define ALB			0x8000
#define DLB			0x8000

/************************** McBSP SPCR2 bit definitions ***********************/
#define XRST		0x0001
#define XRDY		0x0002
#define XEMPTY		0x0004
#define XSYNC_ERR	0x0008
#define XINTM(value)	((value)<<4)		/* bits 4:5 */
#define GRST		0x0040
#define FRST		0x0080
#define SOFT		0x0100
#define FREE		0x0200

/************************** McBSP PCR bit definitions *************************/
#define CLKRP		0x0001
#define CLKXP		0x0002
#define FSRP		0x0004
#define FSXP		0x0008
#define DR_STAT		0x0010
#define DX_STAT		0x0020
#define CLKS_STAT	0x0040
#define SCLKME		0x0080
#define CLKRM		0x0100
#define CLKXM		0x0200
#define FSRM		0x0400
#define FSXM		0x0800
#define RIOEN		0x1000
#define XIOEN		0x2000
#define IDLE_EN		0x4000

/************************** McBSP RCR1 bit definitions ************************/
#define RWDLEN1(value)		((value)<<5)	/* Bits 5:7 */
#define RFRLEN1(value)		((value)<<8)	/* Bits 8:14 */

/************************** McBSP XCR1 bit definitions ************************/
#define XWDLEN1(value)		((value)<<5)	/* Bits 5:7 */
#define XFRLEN1(value)		((value)<<8)	/* Bits 8:14 */

/*************************** McBSP RCR2 bit definitions ***********************/
#define RDATDLY(value)		(value)		/* Bits 0:1 */
#define RFIG			0x0004
#define RCOMPAND(value)		((value)<<3)	/* Bits 3:4 */
#define RWDLEN2(value)		((value)<<5)	/* Bits 5:7 */
#define RFRLEN2(value)		((value)<<8)	/* Bits 8:14 */
#define RPHASE			0x8000

/*************************** McBSP XCR2 bit definitions ***********************/
#define XDATDLY(value)		(value)		/* Bits 0:1 */
#define XFIG			0x0004
#define XCOMPAND(value)		((value)<<3)	/* Bits 3:4 */
#define XWDLEN2(value)		((value)<<5)	/* Bits 5:7 */
#define XFRLEN2(value)		((value)<<8)	/* Bits 8:14 */
#define XPHASE			0x8000

/************************* McBSP SRGR1 bit definitions ************************/
#define CLKGDV(value)		(value)		/* Bits 0:7 */
#define FWID(value)		((value)<<8)	/* Bits 8:15 */

/************************* McBSP SRGR2 bit definitions ************************/
#define FPER(value)		(value)		/* Bits 0:11 */
#define FSGM			0x1000
#define CLKSM			0x2000
#define CLKSP			0x4000
#define GSYNC			0x8000

/************************* McBSP MCR1 bit definitions *************************/
#define RMCM			0x0001
#define RCBLK(value)		((value)<<2)	/* Bits 2:4 */
#define RPABLK(value)		((value)<<5)	/* Bits 5:6 */
#define RPBBLK(value)		((value)<<7)	/* Bits 7:8 */

/************************* McBSP MCR2 bit definitions *************************/
#define XMCM(value)		(value)		/* Bits 0:1 */
#define XCBLK(value)		((value)<<2)	/* Bits 2:4 */
#define XPABLK(value)		((value)<<5)	/* Bits 5:6 */
#define XPBBLK(value)		((value)<<7)	/* Bits 7:8 */

/*********************** McBSP XCCR bit definitions *************************/
#define EXTCLKGATE		0x8000
#define PPCONNECT		0x4000
#define DXENDLY(value)		((value)<<12)	/* Bits 12:13 */
#define XFULL_CYCLE		0x0800
#define DILB			0x0020
#define XDMAEN			0x0008
#define XDISABLE		0x0001

/********************** McBSP RCCR bit definitions *************************/
#define RFULL_CYCLE		0x0800
#define RDMAEN			0x0008
#define RDISABLE		0x0001

/********************** McBSP SYSCONFIG bit definitions ********************/
#define CLOCKACTIVITY(value)	((value)<<8)
#define SIDLEMODE(value)	((value)<<3)
#define ENAWAKEUP		0x0004
#define SOFTRST			0x0002

/********************** McBSP SSELCR bit definitions ***********************/
#define SIDETONEEN		0x0400

/********************** McBSP Sidetone SYSCONFIG bit definitions ***********/
#define ST_AUTOIDLE		0x0001

/********************** McBSP Sidetone SGAINCR bit definitions *************/
#define ST_CH1GAIN(value)	((value<<16))	/* Bits 16:31 */
#define ST_CH0GAIN(value)	(value)		/* Bits 0:15 */

/********************** McBSP Sidetone SFIRCR bit definitions **************/
#define ST_FIRCOEFF(value)	(value)		/* Bits 0:15 */

/********************** McBSP Sidetone SSELCR bit definitions **************/
#define ST_COEFFWRDONE		0x0004
#define ST_COEFFWREN		0x0002
#define ST_SIDETONEEN		0x0001

/********************** McBSP DMA operating modes **************************/
#define MCBSP_DMA_MODE_ELEMENT		0
#define MCBSP_DMA_MODE_THRESHOLD	1
#define MCBSP_DMA_MODE_FRAME		2

/********************** McBSP WAKEUPEN bit definitions *********************/
#define XEMPTYEOFEN		0x4000
#define XRDYEN			0x0400
#define XEOFEN			0x0200
#define XFSXEN			0x0100
#define XSYNCERREN		0x0080
#define RRDYEN			0x0008
#define REOFEN			0x0004
#define RFSREN			0x0002
#define RSYNCERREN		0x0001

/* we don't do multichannel for now */
struct omap_mcbsp_reg_cfg {
	u16 spcr2;
	u16 spcr1;
	u16 rcr2;
	u16 rcr1;
	u16 xcr2;
	u16 xcr1;
	u16 srgr2;
	u16 srgr1;
	u16 mcr2;
	u16 mcr1;
	u16 pcr0;
	u16 rcerc;
	u16 rcerd;
	u16 xcerc;
	u16 xcerd;
	u16 rcere;
	u16 rcerf;
	u16 xcere;
	u16 xcerf;
	u16 rcerg;
	u16 rcerh;
	u16 xcerg;
	u16 xcerh;
	u16 xccr;
	u16 rccr;
};

struct omap_mcbsp_st_data {
	void __iomem *io_base_st;
	bool running;
	bool enabled;
	s16 taps[128];	/* Sidetone filter coefficients */
	int nr_taps;	/* Number of filter coefficients in use */
	s16 ch0gain;
	s16 ch1gain;
};

struct omap_mcbsp_data {
	struct omap_mcbsp_reg_cfg	regs;
	struct omap_pcm_dma_data	dma_data[2];
	unsigned int			fmt;
	/*
	 * Flags indicating is the bus already activated and configured by
	 * another substream
	 */
	int				active;
	int				configured;
	unsigned int			in_freq;
	int				clk_div;
	int				wlen;
};

struct omap_mcbsp {
	struct device *dev;
	unsigned long phys_base;
	unsigned long phys_dma_base;
	void __iomem *io_base;
	u8 id;
	u8 free;

	int rx_irq;
	int tx_irq;

	/* DMA stuff */
	u8 dma_rx_sync;
	u8 dma_tx_sync;

	/* Protect the field .free, while checking if the mcbsp is in use */
	spinlock_t lock;
	struct omap_mcbsp_platform_data *pdata;
	struct clk *fclk;
	struct omap_mcbsp_st_data *st_data;
	struct omap_mcbsp_data mcbsp_data;
	int dma_op_mode;
	u16 max_tx_thres;
	u16 max_rx_thres;
	void *reg_cache;
	int reg_cache_size;
};

void omap_mcbsp_config(struct omap_mcbsp *mcbsp,
		       const struct omap_mcbsp_reg_cfg *config);
void omap_mcbsp_set_tx_threshold(struct omap_mcbsp *mcbsp, u16 threshold);
void omap_mcbsp_set_rx_threshold(struct omap_mcbsp *mcbsp, u16 threshold);
u16 omap_mcbsp_get_max_tx_threshold(struct omap_mcbsp *mcbsp);
u16 omap_mcbsp_get_max_rx_threshold(struct omap_mcbsp *mcbsp);
u16 omap_mcbsp_get_fifo_size(struct omap_mcbsp *mcbsp);
u16 omap_mcbsp_get_tx_delay(struct omap_mcbsp *mcbsp);
u16 omap_mcbsp_get_rx_delay(struct omap_mcbsp *mcbsp);
int omap_mcbsp_get_dma_op_mode(struct omap_mcbsp *mcbsp);
int omap_mcbsp_request(struct omap_mcbsp *mcbsp);
void omap_mcbsp_free(struct omap_mcbsp *mcbsp);
void omap_mcbsp_start(struct omap_mcbsp *mcbsp, int tx, int rx);
void omap_mcbsp_stop(struct omap_mcbsp *mcbsp, int tx, int rx);

/* McBSP functional clock source changing function */
int omap2_mcbsp_set_clks_src(struct omap_mcbsp *mcbsp, u8 fck_src_id);

/* McBSP signal muxing API */
void omap2_mcbsp1_mux_clkr_src(struct omap_mcbsp *mcbsp, u8 mux);
void omap2_mcbsp1_mux_fsr_src(struct omap_mcbsp *mcbsp, u8 mux);

int omap_mcbsp_dma_ch_params(struct omap_mcbsp *mcbsp, unsigned int stream);
int omap_mcbsp_dma_reg_params(struct omap_mcbsp *mcbsp, unsigned int stream);

/* Sidetone specific API */
int omap_st_set_chgain(struct omap_mcbsp *mcbsp, int channel, s16 chgain);
int omap_st_get_chgain(struct omap_mcbsp *mcbsp, int channel, s16 *chgain);
int omap_st_enable(struct omap_mcbsp *mcbsp);
int omap_st_disable(struct omap_mcbsp *mcbsp);
int omap_st_is_enabled(struct omap_mcbsp *mcbsp);

int __devinit omap_mcbsp_probe(struct platform_device *pdev);
int __devexit omap_mcbsp_remove(struct platform_device *pdev);

#endif /* __ASOC_MCBSP_H */
