/*
 * Copyright (c) 2014 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/ssb/ssb_regs.h>
#include <linux/bcma/bcma.h>
#include <linux/bcma/bcma_regs.h>

#include <defs.h>
#include <soc.h>
#include <brcm_hw_ids.h>
#include <brcmu_utils.h>
#include <chipcommon.h>
#include "dhd_dbg.h"
#include "chip.h"

/* SOC Interconnect types (aka chip types) */
#define SOCI_SB		0
#define SOCI_AI		1

/* PL-368 DMP definitions */
#define DMP_DESC_TYPE_MSK	0x0000000F
#define  DMP_DESC_EMPTY		0x00000000
#define  DMP_DESC_VALID		0x00000001
#define  DMP_DESC_COMPONENT	0x00000001
#define  DMP_DESC_MASTER_PORT	0x00000003
#define  DMP_DESC_ADDRESS	0x00000005
#define  DMP_DESC_ADDRSIZE_GT32	0x00000008
#define  DMP_DESC_EOT		0x0000000F

#define DMP_COMP_DESIGNER	0xFFF00000
#define DMP_COMP_DESIGNER_S	20
#define DMP_COMP_PARTNUM	0x000FFF00
#define DMP_COMP_PARTNUM_S	8
#define DMP_COMP_CLASS		0x000000F0
#define DMP_COMP_CLASS_S	4
#define DMP_COMP_REVISION	0xFF000000
#define DMP_COMP_REVISION_S	24
#define DMP_COMP_NUM_SWRAP	0x00F80000
#define DMP_COMP_NUM_SWRAP_S	19
#define DMP_COMP_NUM_MWRAP	0x0007C000
#define DMP_COMP_NUM_MWRAP_S	14
#define DMP_COMP_NUM_SPORT	0x00003E00
#define DMP_COMP_NUM_SPORT_S	9
#define DMP_COMP_NUM_MPORT	0x000001F0
#define DMP_COMP_NUM_MPORT_S	4

#define DMP_MASTER_PORT_UID	0x0000FF00
#define DMP_MASTER_PORT_UID_S	8
#define DMP_MASTER_PORT_NUM	0x000000F0
#define DMP_MASTER_PORT_NUM_S	4

#define DMP_SLAVE_ADDR_BASE	0xFFFFF000
#define DMP_SLAVE_ADDR_BASE_S	12
#define DMP_SLAVE_PORT_NUM	0x00000F00
#define DMP_SLAVE_PORT_NUM_S	8
#define DMP_SLAVE_TYPE		0x000000C0
#define DMP_SLAVE_TYPE_S	6
#define  DMP_SLAVE_TYPE_SLAVE	0
#define  DMP_SLAVE_TYPE_BRIDGE	1
#define  DMP_SLAVE_TYPE_SWRAP	2
#define  DMP_SLAVE_TYPE_MWRAP	3
#define DMP_SLAVE_SIZE_TYPE	0x00000030
#define DMP_SLAVE_SIZE_TYPE_S	4
#define  DMP_SLAVE_SIZE_4K	0
#define  DMP_SLAVE_SIZE_8K	1
#define  DMP_SLAVE_SIZE_16K	2
#define  DMP_SLAVE_SIZE_DESC	3

/* EROM CompIdentB */
#define CIB_REV_MASK		0xff000000
#define CIB_REV_SHIFT		24

/* ARM CR4 core specific control flag bits */
#define ARMCR4_BCMA_IOCTL_CPUHALT	0x0020

/* D11 core specific control flag bits */
#define D11_BCMA_IOCTL_PHYCLOCKEN	0x0004
#define D11_BCMA_IOCTL_PHYRESET		0x0008

/* chip core base & ramsize */
/* bcm4329 */
/* SDIO device core, ID 0x829 */
#define BCM4329_CORE_BUS_BASE		0x18011000
/* internal memory core, ID 0x80e */
#define BCM4329_CORE_SOCRAM_BASE	0x18003000
/* ARM Cortex M3 core, ID 0x82a */
#define BCM4329_CORE_ARM_BASE		0x18002000
#define BCM4329_RAMSIZE			0x48000

/* bcm43143 */
/* SDIO device core */
#define BCM43143_CORE_BUS_BASE		0x18002000
/* internal memory core */
#define BCM43143_CORE_SOCRAM_BASE	0x18004000
/* ARM Cortex M3 core, ID 0x82a */
#define BCM43143_CORE_ARM_BASE		0x18003000
#define BCM43143_RAMSIZE		0x70000

#define CORE_SB(base, field) \
		(base + SBCONFIGOFF + offsetof(struct sbconfig, field))
#define	SBCOREREV(sbidh) \
	((((sbidh) & SSB_IDHIGH_RCHI) >> SSB_IDHIGH_RCHI_SHIFT) | \
	  ((sbidh) & SSB_IDHIGH_RCLO))

struct sbconfig {
	u32 PAD[2];
	u32 sbipsflag;	/* initiator port ocp slave flag */
	u32 PAD[3];
	u32 sbtpsflag;	/* target port ocp slave flag */
	u32 PAD[11];
	u32 sbtmerrloga;	/* (sonics >= 2.3) */
	u32 PAD;
	u32 sbtmerrlog;	/* (sonics >= 2.3) */
	u32 PAD[3];
	u32 sbadmatch3;	/* address match3 */
	u32 PAD;
	u32 sbadmatch2;	/* address match2 */
	u32 PAD;
	u32 sbadmatch1;	/* address match1 */
	u32 PAD[7];
	u32 sbimstate;	/* initiator agent state */
	u32 sbintvec;	/* interrupt mask */
	u32 sbtmstatelow;	/* target state */
	u32 sbtmstatehigh;	/* target state */
	u32 sbbwa0;		/* bandwidth allocation table0 */
	u32 PAD;
	u32 sbimconfiglow;	/* initiator configuration */
	u32 sbimconfighigh;	/* initiator configuration */
	u32 sbadmatch0;	/* address match0 */
	u32 PAD;
	u32 sbtmconfiglow;	/* target configuration */
	u32 sbtmconfighigh;	/* target configuration */
	u32 sbbconfig;	/* broadcast configuration */
	u32 PAD;
	u32 sbbstate;	/* broadcast state */
	u32 PAD[3];
	u32 sbactcnfg;	/* activate configuration */
	u32 PAD[3];
	u32 sbflagst;	/* current sbflags */
	u32 PAD[3];
	u32 sbidlow;		/* identification */
	u32 sbidhigh;	/* identification */
};

struct brcmf_core_priv {
	struct brcmf_core pub;
	u32 wrapbase;
	struct list_head list;
	struct brcmf_chip_priv *chip;
};

/* ARM CR4 core specific control flag bits */
#define ARMCR4_BCMA_IOCTL_CPUHALT	0x0020

/* D11 core specific control flag bits */
#define D11_BCMA_IOCTL_PHYCLOCKEN	0x0004
#define D11_BCMA_IOCTL_PHYRESET		0x0008

struct brcmf_chip_priv {
	struct brcmf_chip pub;
	const struct brcmf_buscore_ops *ops;
	void *ctx;
	/* assured first core is chipcommon, second core is buscore */
	struct list_head cores;
	u16 num_cores;

	bool (*iscoreup)(struct brcmf_core_priv *core);
	void (*coredisable)(struct brcmf_core_priv *core, u32 prereset,
			    u32 reset);
	void (*resetcore)(struct brcmf_core_priv *core, u32 prereset, u32 reset,
			  u32 postreset);
};

static void brcmf_chip_sb_corerev(struct brcmf_chip_priv *ci,
				  struct brcmf_core *core)
{
	u32 regdata;

	regdata = ci->ops->read32(ci->ctx, CORE_SB(core->base, sbidhigh));
	core->rev = SBCOREREV(regdata);
}

static bool brcmf_chip_sb_iscoreup(struct brcmf_core_priv *core)
{
	struct brcmf_chip_priv *ci;
	u32 regdata;
	u32 address;

	ci = core->chip;
	address = CORE_SB(core->pub.base, sbtmstatelow);
	regdata = ci->ops->read32(ci->ctx, address);
	regdata &= (SSB_TMSLOW_RESET | SSB_TMSLOW_REJECT |
		    SSB_IMSTATE_REJECT | SSB_TMSLOW_CLOCK);
	return SSB_TMSLOW_CLOCK == regdata;
}

static bool brcmf_chip_ai_iscoreup(struct brcmf_core_priv *core)
{
	struct brcmf_chip_priv *ci;
	u32 regdata;
	bool ret;

	ci = core->chip;
	regdata = ci->ops->read32(ci->ctx, core->wrapbase + BCMA_IOCTL);
	ret = (regdata & (BCMA_IOCTL_FGC | BCMA_IOCTL_CLK)) == BCMA_IOCTL_CLK;

	regdata = ci->ops->read32(ci->ctx, core->wrapbase + BCMA_RESET_CTL);
	ret = ret && ((regdata & BCMA_RESET_CTL_RESET) == 0);

	return ret;
}

static void brcmf_chip_sb_coredisable(struct brcmf_core_priv *core,
				      u32 prereset, u32 reset)
{
	struct brcmf_chip_priv *ci;
	u32 val, base;

	ci = core->chip;
	base = core->pub.base;
	val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
	if (val & SSB_TMSLOW_RESET)
		return;

	val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
	if ((val & SSB_TMSLOW_CLOCK) != 0) {
		/*
		 * set target reject and spin until busy is clear
		 * (preserve core-specific bits)
		 */
		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
		ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow),
					 val | SSB_TMSLOW_REJECT);

		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
		udelay(1);
		SPINWAIT((ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatehigh))
			  & SSB_TMSHIGH_BUSY), 100000);

		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatehigh));
		if (val & SSB_TMSHIGH_BUSY)
			brcmf_err("core state still busy\n");

		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbidlow));
		if (val & SSB_IDLOW_INITIATOR) {
			val = ci->ops->read32(ci->ctx,
					      CORE_SB(base, sbimstate));
			val |= SSB_IMSTATE_REJECT;
			ci->ops->write32(ci->ctx,
					 CORE_SB(base, sbimstate), val);
			val = ci->ops->read32(ci->ctx,
					      CORE_SB(base, sbimstate));
			udelay(1);
			SPINWAIT((ci->ops->read32(ci->ctx,
						  CORE_SB(base, sbimstate)) &
				  SSB_IMSTATE_BUSY), 100000);
		}

		/* set reset and reject while enabling the clocks */
		val = SSB_TMSLOW_FGC | SSB_TMSLOW_CLOCK |
		      SSB_TMSLOW_REJECT | SSB_TMSLOW_RESET;
		ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow), val);
		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
		udelay(10);

		/* clear the initiator reject bit */
		val = ci->ops->read32(ci->ctx, CORE_SB(base, sbidlow));
		if (val & SSB_IDLOW_INITIATOR) {
			val = ci->ops->read32(ci->ctx,
					      CORE_SB(base, sbimstate));
			val &= ~SSB_IMSTATE_REJECT;
			ci->ops->write32(ci->ctx,
					 CORE_SB(base, sbimstate), val);
		}
	}

	/* leave reset and reject asserted */
	ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow),
			 (SSB_TMSLOW_REJECT | SSB_TMSLOW_RESET));
	udelay(1);
}

static void brcmf_chip_ai_coredisable(struct brcmf_core_priv *core,
				      u32 prereset, u32 reset)
{
	struct brcmf_chip_priv *ci;
	u32 regdata;

	ci = core->chip;

	/* if core is already in reset, just return */
	regdata = ci->ops->read32(ci->ctx, core->wrapbase + BCMA_RESET_CTL);
	if ((regdata & BCMA_RESET_CTL_RESET) != 0)
		return;

	/* configure reset */
	ci->ops->write32(ci->ctx, core->wrapbase + BCMA_IOCTL,
			 prereset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK);
	ci->ops->read32(ci->ctx, core->wrapbase + BCMA_IOCTL);

	/* put in reset */
	ci->ops->write32(ci->ctx, core->wrapbase + BCMA_RESET_CTL,
			 BCMA_RESET_CTL_RESET);
	usleep_range(10, 20);

	/* wait till reset is 1 */
	SPINWAIT(ci->ops->read32(ci->ctx, core->wrapbase + BCMA_RESET_CTL) !=
		 BCMA_RESET_CTL_RESET, 300);

	/* in-reset configure */
	ci->ops->write32(ci->ctx, core->wrapbase + BCMA_IOCTL,
			 reset | BCMA_IOCTL_FGC | BCMA_IOCTL_CLK);
	ci->ops->read32(ci->ctx, core->wrapbase + BCMA_IOCTL);
}

static void brcmf_chip_sb_resetcore(struct brcmf_core_priv *core, u32 prereset,
				    u32 reset, u32 postreset)
{
	struct brcmf_chip_priv *ci;
	u32 regdata;
	u32 base;

	ci = core->chip;
	base = core->pub.base;
	/*
	 * Must do the disable sequence first to work for
	 * arbitrary current core state.
	 */
	brcmf_chip_sb_coredisable(core, 0, 0);

	/*
	 * Now do the initialization sequence.
	 * set reset while enabling the clock and
	 * forcing them on throughout the core
	 */
	ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow),
			 SSB_TMSLOW_FGC | SSB_TMSLOW_CLOCK |
			 SSB_TMSLOW_RESET);
	regdata = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
	udelay(1);

	/* clear any serror */
	regdata = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatehigh));
	if (regdata & SSB_TMSHIGH_SERR)
		ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatehigh), 0);

	regdata = ci->ops->read32(ci->ctx, CORE_SB(base, sbimstate));
	if (regdata & (SSB_IMSTATE_IBE | SSB_IMSTATE_TO)) {
		regdata &= ~(SSB_IMSTATE_IBE | SSB_IMSTATE_TO);
		ci->ops->write32(ci->ctx, CORE_SB(base, sbimstate), regdata);
	}

	/* clear reset and allow it to propagate throughout the core */
	ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow),
			 SSB_TMSLOW_FGC | SSB_TMSLOW_CLOCK);
	regdata = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
	udelay(1);

	/* leave clock enabled */
	ci->ops->write32(ci->ctx, CORE_SB(base, sbtmstatelow),
			 SSB_TMSLOW_CLOCK);
	regdata = ci->ops->read32(ci->ctx, CORE_SB(base, sbtmstatelow));
	udelay(1);
}

static void brcmf_chip_ai_resetcore(struct brcmf_core_priv *core, u32 prereset,
				    u32 reset, u32 postreset)
{
	struct brcmf_chip_priv *ci;
	int count;

	ci = core->chip;

	/* must disable first to work for arbitrary current core state */
	brcmf_chip_ai_coredisable(core, prereset, reset);

	count = 0;
	while (ci->ops->read32(ci->ctx, core->wrapbase + BCMA_RESET_CTL) &
	       BCMA_RESET_CTL_RESET) {
		ci->ops->write32(ci->ctx, core->wrapbase + BCMA_RESET_CTL, 0);
		count++;
		if (count > 50)
			break;
		usleep_range(40, 60);
	}

	ci->ops->write32(ci->ctx, core->wrapbase + BCMA_IOCTL,
			 postreset | BCMA_IOCTL_CLK);
	ci->ops->read32(ci->ctx, core->wrapbase + BCMA_IOCTL);
}

static char *brcmf_chip_name(uint chipid, char *buf, uint len)
{
	const char *fmt;

	fmt = ((chipid > 0xa000) || (chipid < 0x4000)) ? "%d" : "%x";
	snprintf(buf, len, fmt, chipid);
	return buf;
}

static struct brcmf_core *brcmf_chip_add_core(struct brcmf_chip_priv *ci,
					      u16 coreid, u32 base,
					      u32 wrapbase)
{
	struct brcmf_core_priv *core;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core)
		return ERR_PTR(-ENOMEM);

	core->pub.id = coreid;
	core->pub.base = base;
	core->chip = ci;
	core->wrapbase = wrapbase;

	list_add_tail(&core->list, &ci->cores);
	return &core->pub;
}

#ifdef DEBUG
/* safety check for chipinfo */
static int brcmf_chip_cores_check(struct brcmf_chip_priv *ci)
{
	struct brcmf_core_priv *core;
	bool need_socram = false;
	bool has_socram = false;
	int idx = 1;

	list_for_each_entry(core, &ci->cores, list) {
		brcmf_dbg(INFO, " [%-2d] core 0x%x:%-2d base 0x%08x wrap 0x%08x\n",
			  idx++, core->pub.id, core->pub.rev, core->pub.base,
			  core->wrapbase);

		switch (core->pub.id) {
		case BCMA_CORE_ARM_CM3:
			need_socram = true;
			break;
		case BCMA_CORE_INTERNAL_MEM:
			has_socram = true;
			break;
		case BCMA_CORE_ARM_CR4:
			if (ci->pub.rambase == 0) {
				brcmf_err("RAM base not provided with ARM CR4 core\n");
				return -ENOMEM;
			}
			break;
		default:
			break;
		}
	}

	/* check RAM core presence for ARM CM3 core */
	if (need_socram && !has_socram) {
		brcmf_err("RAM core not provided with ARM CM3 core\n");
		return -ENODEV;
	}
	return 0;
}
#else	/* DEBUG */
static inline int brcmf_chip_cores_check(struct brcmf_chip_priv *ci)
{
	return 0;
}
#endif

static void brcmf_chip_get_raminfo(struct brcmf_chip_priv *ci)
{
	switch (ci->pub.chip) {
	case BCM4329_CHIP_ID:
		ci->pub.ramsize = BCM4329_RAMSIZE;
		break;
	case BCM43143_CHIP_ID:
		ci->pub.ramsize = BCM43143_RAMSIZE;
		break;
	case BCM43241_CHIP_ID:
		ci->pub.ramsize = 0x90000;
		break;
	case BCM4330_CHIP_ID:
		ci->pub.ramsize = 0x48000;
		break;
	case BCM4334_CHIP_ID:
		ci->pub.ramsize = 0x80000;
		break;
	case BCM4335_CHIP_ID:
		ci->pub.ramsize = 0xc0000;
		ci->pub.rambase = 0x180000;
		break;
	case BCM43362_CHIP_ID:
		ci->pub.ramsize = 0x3c000;
		break;
	case BCM4339_CHIP_ID:
	case BCM4354_CHIP_ID:
		ci->pub.ramsize = 0xc0000;
		ci->pub.rambase = 0x180000;
		break;
	default:
		brcmf_err("unknown chip: %s\n", ci->pub.name);
		break;
	}
}

static u32 brcmf_chip_dmp_get_desc(struct brcmf_chip_priv *ci, u32 *eromaddr,
				   u8 *type)
{
	u32 val;

	/* read next descriptor */
	val = ci->ops->read32(ci->ctx, *eromaddr);
	*eromaddr += 4;

	if (!type)
		return val;

	/* determine descriptor type */
	*type = (val & DMP_DESC_TYPE_MSK);
	if ((*type & ~DMP_DESC_ADDRSIZE_GT32) == DMP_DESC_ADDRESS)
		*type = DMP_DESC_ADDRESS;

	return val;
}

static int brcmf_chip_dmp_get_regaddr(struct brcmf_chip_priv *ci, u32 *eromaddr,
				      u32 *regbase, u32 *wrapbase)
{
	u8 desc;
	u32 val;
	u8 mpnum = 0;
	u8 stype, sztype, wraptype;

	*regbase = 0;
	*wrapbase = 0;

	val = brcmf_chip_dmp_get_desc(ci, eromaddr, &desc);
	if (desc == DMP_DESC_MASTER_PORT) {
		mpnum = (val & DMP_MASTER_PORT_NUM) >> DMP_MASTER_PORT_NUM_S;
		wraptype = DMP_SLAVE_TYPE_MWRAP;
	} else if (desc == DMP_DESC_ADDRESS) {
		/* revert erom address */
		*eromaddr -= 4;
		wraptype = DMP_SLAVE_TYPE_SWRAP;
	} else {
		*eromaddr -= 4;
		return -EILSEQ;
	}

	do {
		/* locate address descriptor */
		do {
			val = brcmf_chip_dmp_get_desc(ci, eromaddr, &desc);
			/* unexpected table end */
			if (desc == DMP_DESC_EOT) {
				*eromaddr -= 4;
				return -EFAULT;
			}
		} while (desc != DMP_DESC_ADDRESS);

		/* skip upper 32-bit address descriptor */
		if (val & DMP_DESC_ADDRSIZE_GT32)
			brcmf_chip_dmp_get_desc(ci, eromaddr, NULL);

		sztype = (val & DMP_SLAVE_SIZE_TYPE) >> DMP_SLAVE_SIZE_TYPE_S;

		/* next size descriptor can be skipped */
		if (sztype == DMP_SLAVE_SIZE_DESC) {
			val = brcmf_chip_dmp_get_desc(ci, eromaddr, NULL);
			/* skip upper size descriptor if present */
			if (val & DMP_DESC_ADDRSIZE_GT32)
				brcmf_chip_dmp_get_desc(ci, eromaddr, NULL);
		}

		/* only look for 4K register regions */
		if (sztype != DMP_SLAVE_SIZE_4K)
			continue;

		stype = (val & DMP_SLAVE_TYPE) >> DMP_SLAVE_TYPE_S;

		/* only regular slave and wrapper */
		if (*regbase == 0 && stype == DMP_SLAVE_TYPE_SLAVE)
			*regbase = val & DMP_SLAVE_ADDR_BASE;
		if (*wrapbase == 0 && stype == wraptype)
			*wrapbase = val & DMP_SLAVE_ADDR_BASE;
	} while (*regbase == 0 || *wrapbase == 0);

	return 0;
}

static
int brcmf_chip_dmp_erom_scan(struct brcmf_chip_priv *ci)
{
	struct brcmf_core *core;
	u32 eromaddr;
	u8 desc_type = 0;
	u32 val;
	u16 id;
	u8 nmp, nsp, nmw, nsw, rev;
	u32 base, wrap;
	int err;

	eromaddr = ci->ops->read32(ci->ctx, CORE_CC_REG(SI_ENUM_BASE, eromptr));

	while (desc_type != DMP_DESC_EOT) {
		val = brcmf_chip_dmp_get_desc(ci, &eromaddr, &desc_type);
		if (!(val & DMP_DESC_VALID))
			continue;

		if (desc_type == DMP_DESC_EMPTY)
			continue;

		/* need a component descriptor */
		if (desc_type != DMP_DESC_COMPONENT)
			continue;

		id = (val & DMP_COMP_PARTNUM) >> DMP_COMP_PARTNUM_S;

		/* next descriptor must be component as well */
		val = brcmf_chip_dmp_get_desc(ci, &eromaddr, &desc_type);
		if (WARN_ON((val & DMP_DESC_TYPE_MSK) != DMP_DESC_COMPONENT))
			return -EFAULT;

		/* only look at cores with master port(s) */
		nmp = (val & DMP_COMP_NUM_MPORT) >> DMP_COMP_NUM_MPORT_S;
		nsp = (val & DMP_COMP_NUM_SPORT) >> DMP_COMP_NUM_SPORT_S;
		nmw = (val & DMP_COMP_NUM_MWRAP) >> DMP_COMP_NUM_MWRAP_S;
		nsw = (val & DMP_COMP_NUM_SWRAP) >> DMP_COMP_NUM_SWRAP_S;
		rev = (val & DMP_COMP_REVISION) >> DMP_COMP_REVISION_S;

		/* need core with ports */
		if (nmw + nsw == 0)
			continue;

		/* try to obtain register address info */
		err = brcmf_chip_dmp_get_regaddr(ci, &eromaddr, &base, &wrap);
		if (err)
			continue;

		/* finally a core to be added */
		core = brcmf_chip_add_core(ci, id, base, wrap);
		if (IS_ERR(core))
			return PTR_ERR(core);

		core->rev = rev;
	}

	return 0;
}

static int brcmf_chip_recognition(struct brcmf_chip_priv *ci)
{
	struct brcmf_core *core;
	u32 regdata;
	u32 socitype;

	/* Get CC core rev
	 * Chipid is assume to be at offset 0 from SI_ENUM_BASE
	 * For different chiptypes or old sdio hosts w/o chipcommon,
	 * other ways of recognition should be added here.
	 */
	regdata = ci->ops->read32(ci->ctx, CORE_CC_REG(SI_ENUM_BASE, chipid));
	ci->pub.chip = regdata & CID_ID_MASK;
	ci->pub.chiprev = (regdata & CID_REV_MASK) >> CID_REV_SHIFT;
	socitype = (regdata & CID_TYPE_MASK) >> CID_TYPE_SHIFT;

	brcmf_chip_name(ci->pub.chip, ci->pub.name, sizeof(ci->pub.name));
	brcmf_dbg(INFO, "found %s chip: BCM%s, rev=%d\n",
		  socitype == SOCI_SB ? "SB" : "AXI", ci->pub.name,
		  ci->pub.chiprev);

	if (socitype == SOCI_SB) {
		if (ci->pub.chip != BCM4329_CHIP_ID) {
			brcmf_err("SB chip is not supported\n");
			return -ENODEV;
		}
		ci->iscoreup = brcmf_chip_sb_iscoreup;
		ci->coredisable = brcmf_chip_sb_coredisable;
		ci->resetcore = brcmf_chip_sb_resetcore;

		core = brcmf_chip_add_core(ci, BCMA_CORE_CHIPCOMMON,
					   SI_ENUM_BASE, 0);
		brcmf_chip_sb_corerev(ci, core);
		core = brcmf_chip_add_core(ci, BCMA_CORE_SDIO_DEV,
					   BCM4329_CORE_BUS_BASE, 0);
		brcmf_chip_sb_corerev(ci, core);
		core = brcmf_chip_add_core(ci, BCMA_CORE_INTERNAL_MEM,
					   BCM4329_CORE_SOCRAM_BASE, 0);
		brcmf_chip_sb_corerev(ci, core);
		core = brcmf_chip_add_core(ci, BCMA_CORE_ARM_CM3,
					   BCM4329_CORE_ARM_BASE, 0);
		brcmf_chip_sb_corerev(ci, core);

		core = brcmf_chip_add_core(ci, BCMA_CORE_80211, 0x18001000, 0);
		brcmf_chip_sb_corerev(ci, core);
	} else if (socitype == SOCI_AI) {
		ci->iscoreup = brcmf_chip_ai_iscoreup;
		ci->coredisable = brcmf_chip_ai_coredisable;
		ci->resetcore = brcmf_chip_ai_resetcore;

		brcmf_chip_dmp_erom_scan(ci);
	} else {
		brcmf_err("chip backplane type %u is not supported\n",
			  socitype);
		return -ENODEV;
	}

	brcmf_chip_get_raminfo(ci);

	return brcmf_chip_cores_check(ci);
}

static void brcmf_chip_disable_arm(struct brcmf_chip_priv *chip, u16 id)
{
	struct brcmf_core *core;
	struct brcmf_core_priv *cr4;
	u32 val;


	core = brcmf_chip_get_core(&chip->pub, id);
	if (!core)
		return;

	switch (id) {
	case BCMA_CORE_ARM_CM3:
		brcmf_chip_coredisable(core, 0, 0);
		break;
	case BCMA_CORE_ARM_CR4:
		cr4 = container_of(core, struct brcmf_core_priv, pub);

		/* clear all IOCTL bits except HALT bit */
		val = chip->ops->read32(chip->ctx, cr4->wrapbase + BCMA_IOCTL);
		val &= ARMCR4_BCMA_IOCTL_CPUHALT;
		brcmf_chip_resetcore(core, val, ARMCR4_BCMA_IOCTL_CPUHALT,
				     ARMCR4_BCMA_IOCTL_CPUHALT);
		break;
	default:
		brcmf_err("unknown id: %u\n", id);
		break;
	}
}

static int brcmf_chip_setup(struct brcmf_chip_priv *chip)
{
	struct brcmf_chip *pub;
	struct brcmf_core_priv *cc;
	u32 base;
	u32 val;
	int ret = 0;

	pub = &chip->pub;
	cc = list_first_entry(&chip->cores, struct brcmf_core_priv, list);
	base = cc->pub.base;

	/* get chipcommon capabilites */
	pub->cc_caps = chip->ops->read32(chip->ctx,
					 CORE_CC_REG(base, capabilities));

	/* get pmu caps & rev */
	if (pub->cc_caps & CC_CAP_PMU) {
		val = chip->ops->read32(chip->ctx,
					CORE_CC_REG(base, pmucapabilities));
		pub->pmurev = val & PCAP_REV_MASK;
		pub->pmucaps = val;
	}

	brcmf_dbg(INFO, "ccrev=%d, pmurev=%d, pmucaps=0x%x\n",
		  cc->pub.rev, pub->pmurev, pub->pmucaps);

	/* execute bus core specific setup */
	if (chip->ops->setup)
		ret = chip->ops->setup(chip->ctx, pub);

	/*
	 * Make sure any on-chip ARM is off (in case strapping is wrong),
	 * or downloaded code was already running.
	 */
	brcmf_chip_disable_arm(chip, BCMA_CORE_ARM_CM3);
	brcmf_chip_disable_arm(chip, BCMA_CORE_ARM_CR4);
	return ret;
}

struct brcmf_chip *brcmf_chip_attach(void *ctx,
				     const struct brcmf_buscore_ops *ops)
{
	struct brcmf_chip_priv *chip;
	int err = 0;

	if (WARN_ON(!ops->read32))
		err = -EINVAL;
	if (WARN_ON(!ops->write32))
		err = -EINVAL;
	if (WARN_ON(!ops->prepare))
		err = -EINVAL;
	if (WARN_ON(!ops->exit_dl))
		err = -EINVAL;
	if (err < 0)
		return ERR_PTR(-EINVAL);

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&chip->cores);
	chip->num_cores = 0;
	chip->ops = ops;
	chip->ctx = ctx;

	err = ops->prepare(ctx);
	if (err < 0)
		goto fail;

	err = brcmf_chip_recognition(chip);
	if (err < 0)
		goto fail;

	err = brcmf_chip_setup(chip);
	if (err < 0)
		goto fail;

	return &chip->pub;

fail:
	brcmf_chip_detach(&chip->pub);
	return ERR_PTR(err);
}

void brcmf_chip_detach(struct brcmf_chip *pub)
{
	struct brcmf_chip_priv *chip;
	struct brcmf_core_priv *core;
	struct brcmf_core_priv *tmp;

	chip = container_of(pub, struct brcmf_chip_priv, pub);
	list_for_each_entry_safe(core, tmp, &chip->cores, list) {
		list_del(&core->list);
		kfree(core);
	}
	kfree(chip);
}

struct brcmf_core *brcmf_chip_get_core(struct brcmf_chip *pub, u16 coreid)
{
	struct brcmf_chip_priv *chip;
	struct brcmf_core_priv *core;

	chip = container_of(pub, struct brcmf_chip_priv, pub);
	list_for_each_entry(core, &chip->cores, list)
		if (core->pub.id == coreid)
			return &core->pub;

	return NULL;
}

struct brcmf_core *brcmf_chip_get_chipcommon(struct brcmf_chip *pub)
{
	struct brcmf_chip_priv *chip;
	struct brcmf_core_priv *cc;

	chip = container_of(pub, struct brcmf_chip_priv, pub);
	cc = list_first_entry(&chip->cores, struct brcmf_core_priv, list);
	if (WARN_ON(!cc || cc->pub.id != BCMA_CORE_CHIPCOMMON))
		return brcmf_chip_get_core(pub, BCMA_CORE_CHIPCOMMON);
	return &cc->pub;
}

bool brcmf_chip_iscoreup(struct brcmf_core *pub)
{
	struct brcmf_core_priv *core;

	core = container_of(pub, struct brcmf_core_priv, pub);
	return core->chip->iscoreup(core);
}

void brcmf_chip_coredisable(struct brcmf_core *pub, u32 prereset, u32 reset)
{
	struct brcmf_core_priv *core;

	core = container_of(pub, struct brcmf_core_priv, pub);
	core->chip->coredisable(core, prereset, reset);
}

void brcmf_chip_resetcore(struct brcmf_core *pub, u32 prereset, u32 reset,
			  u32 postreset)
{
	struct brcmf_core_priv *core;

	core = container_of(pub, struct brcmf_core_priv, pub);
	core->chip->resetcore(core, prereset, reset, postreset);
}

static void
brcmf_chip_cm3_enterdl(struct brcmf_chip_priv *chip)
{
	struct brcmf_core *core;

	brcmf_chip_disable_arm(chip, BCMA_CORE_ARM_CM3);
	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_80211);
	brcmf_chip_resetcore(core, D11_BCMA_IOCTL_PHYRESET |
				   D11_BCMA_IOCTL_PHYCLOCKEN,
			     D11_BCMA_IOCTL_PHYCLOCKEN,
			     D11_BCMA_IOCTL_PHYCLOCKEN);
	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_INTERNAL_MEM);
	brcmf_chip_resetcore(core, 0, 0, 0);
}

static bool brcmf_chip_cm3_exitdl(struct brcmf_chip_priv *chip)
{
	struct brcmf_core *core;

	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_INTERNAL_MEM);
	if (!brcmf_chip_iscoreup(core)) {
		brcmf_err("SOCRAM core is down after reset?\n");
		return false;
	}

	chip->ops->exit_dl(chip->ctx, &chip->pub, 0);

	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_ARM_CM3);
	brcmf_chip_resetcore(core, 0, 0, 0);

	return true;
}

static inline void
brcmf_chip_cr4_enterdl(struct brcmf_chip_priv *chip)
{
	struct brcmf_core *core;

	brcmf_chip_disable_arm(chip, BCMA_CORE_ARM_CR4);

	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_80211);
	brcmf_chip_resetcore(core, D11_BCMA_IOCTL_PHYRESET |
				   D11_BCMA_IOCTL_PHYCLOCKEN,
			     D11_BCMA_IOCTL_PHYCLOCKEN,
			     D11_BCMA_IOCTL_PHYCLOCKEN);
}

static bool brcmf_chip_cr4_exitdl(struct brcmf_chip_priv *chip, u32 rstvec)
{
	struct brcmf_core *core;

	chip->ops->exit_dl(chip->ctx, &chip->pub, rstvec);

	/* restore ARM */
	core = brcmf_chip_get_core(&chip->pub, BCMA_CORE_ARM_CR4);
	brcmf_chip_resetcore(core, ARMCR4_BCMA_IOCTL_CPUHALT, 0, 0);

	return true;
}

void brcmf_chip_enter_download(struct brcmf_chip *pub)
{
	struct brcmf_chip_priv *chip;
	struct brcmf_core *arm;

	brcmf_dbg(TRACE, "Enter\n");

	chip = container_of(pub, struct brcmf_chip_priv, pub);
	arm = brcmf_chip_get_core(pub, BCMA_CORE_ARM_CR4);
	if (arm) {
		brcmf_chip_cr4_enterdl(chip);
		return;
	}

	brcmf_chip_cm3_enterdl(chip);
}

bool brcmf_chip_exit_download(struct brcmf_chip *pub, u32 rstvec)
{
	struct brcmf_chip_priv *chip;
	struct brcmf_core *arm;

	brcmf_dbg(TRACE, "Enter\n");

	chip = container_of(pub, struct brcmf_chip_priv, pub);
	arm = brcmf_chip_get_core(pub, BCMA_CORE_ARM_CR4);
	if (arm)
		return brcmf_chip_cr4_exitdl(chip, rstvec);

	return brcmf_chip_cm3_exitdl(chip);
}

bool brcmf_chip_sr_capable(struct brcmf_chip *pub)
{
	u32 base, addr, reg, pmu_cc3_mask = ~0;
	struct brcmf_chip_priv *chip;

	brcmf_dbg(TRACE, "Enter\n");

	/* old chips with PMU version less than 17 don't support save restore */
	if (pub->pmurev < 17)
		return false;

	base = brcmf_chip_get_chipcommon(pub)->base;
	chip = container_of(pub, struct brcmf_chip_priv, pub);

	switch (pub->chip) {
	case BCM4354_CHIP_ID:
		/* explicitly check SR engine enable bit */
		pmu_cc3_mask = BIT(2);
		/* fall-through */
	case BCM43241_CHIP_ID:
	case BCM4335_CHIP_ID:
	case BCM4339_CHIP_ID:
		/* read PMU chipcontrol register 3 */
		addr = CORE_CC_REG(base, chipcontrol_addr);
		chip->ops->write32(chip->ctx, addr, 3);
		addr = CORE_CC_REG(base, chipcontrol_data);
		reg = chip->ops->read32(chip->ctx, addr);
		return (reg & pmu_cc3_mask) != 0;
	default:
		addr = CORE_CC_REG(base, pmucapabilities_ext);
		reg = chip->ops->read32(chip->ctx, addr);
		if ((reg & PCAPEXT_SR_SUPPORTED_MASK) == 0)
			return false;

		addr = CORE_CC_REG(base, retention_ctl);
		reg = chip->ops->read32(chip->ctx, addr);
		return (reg & (PMU_RCTL_MACPHY_DISABLE_MASK |
			       PMU_RCTL_LOGIC_DISABLE_MASK)) == 0;
	}
}
