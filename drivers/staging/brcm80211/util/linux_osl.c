/*
 * Copyright (c) 2010 Broadcom Corporation
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

#include <linux/delay.h>
#include <linux/fs.h>
#ifdef mips
#include <asm/paccess.h>
#endif				/* mips */
#include <bcmendian.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/sched.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <pcicfg.h>


#define OS_HANDLE_MAGIC		0x1234abcd	/* Magic # to recognise osh */
#define BCM_MEM_FILENAME_LEN 	24	/* Mem. filename length */

struct osl_info {
	osl_pubinfo_t pub;
	uint magic;
	void *pdev;
	uint bustype;
};

/* Global ASSERT type flag */
u32 g_assert_type;

osl_t *osl_attach(void *pdev, uint bustype)
{
	osl_t *osh;

	osh = kmalloc(sizeof(osl_t), GFP_ATOMIC);
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));

	osh->magic = OS_HANDLE_MAGIC;
	osh->pdev = pdev;
	osh->bustype = bustype;

	switch (bustype) {
	case PCI_BUS:
	case SI_BUS:
	case PCMCIA_BUS:
		osh->pub.mmbus = true;
		break;
	case JTAG_BUS:
	case SDIO_BUS:
	case USB_BUS:
	case SPI_BUS:
	case RPC_BUS:
		osh->pub.mmbus = false;
		break;
	default:
		ASSERT(false);
		break;
	}

	return osh;
}

void osl_detach(osl_t *osh)
{
	if (osh == NULL)
		return;

	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	kfree(osh);
}

void *BCMFASTPATH osl_pktget(osl_t *osh, uint len)
{
	struct sk_buff *skb;

	skb = dev_alloc_skb(len);
	if (skb) {
		skb_put(skb, len);
		skb->priority = 0;

		osh->pub.pktalloced++;
	}

	return (void *)skb;
}

/* Free the driver packet. Free the tag if present */
void BCMFASTPATH osl_pktfree(osl_t *osh, void *p, bool send)
{
	struct sk_buff *skb, *nskb;
	int nest = 0;

	skb = (struct sk_buff *)p;
	ASSERT(skb);

	if (send && osh->pub.tx_fn)
		osh->pub.tx_fn(osh->pub.tx_ctx, p, 0);

	/* perversion: we use skb->next to chain multi-skb packets */
	while (skb) {
		nskb = skb->next;
		skb->next = NULL;

		if (skb->destructor)
			/* cannot kfree_skb() on hard IRQ (net/core/skbuff.c) if
			 * destructor exists
			 */
			dev_kfree_skb_any(skb);
		else
			/* can free immediately (even in_irq()) if destructor
			 * does not exist
			 */
			dev_kfree_skb(skb);

		osh->pub.pktalloced--;
		nest++;
		skb = nskb;
	}
}

u32 osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	uint val;
	pci_read_config_dword(osh->pdev, offset, &val);
	return val;
}

void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	pci_write_config_dword(osh->pdev, offset, val);
	if (offset == PCI_BAR0_WIN)
		ASSERT(osl_pci_read_config(osh, offset, size) == val);
}

/* return bus # for the pci device pointed by osh->pdev */
uint osl_pci_bus(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return ((struct pci_dev *)osh->pdev)->bus->number;
}

/* return slot # for the pci device pointed by osh->pdev */
uint osl_pci_slot(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn);
}

void *osl_dma_alloc_consistent(osl_t *osh, uint size, u16 align_bits,
			       uint *alloced, unsigned long *pap)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	if (align_bits) {
		u16 align = (1 << align_bits);
		if (!IS_ALIGNED(PAGE_SIZE, align))
			size += align;
		*alloced = size;
	}
	return pci_alloc_consistent(osh->pdev, size, (dma_addr_t *) pap);
}

void osl_dma_free_consistent(osl_t *osh, void *va, uint size, unsigned long pa)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	pci_free_consistent(osh->pdev, size, va, (dma_addr_t) pa);
}

uint BCMFASTPATH osl_dma_map(osl_t *osh, void *va, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX) ? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE;
	return pci_map_single(osh->pdev, va, size, dir);
}

void BCMFASTPATH osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX) ? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE;
	pci_unmap_single(osh->pdev, (u32) pa, size, dir);
}

#if defined(BCMDBG_ASSERT)
void osl_assert(char *exp, char *file, int line)
{
	char tempbuf[256];
	char *basename;

	basename = strrchr(file, '/');
	/* skip the '/' */
	if (basename)
		basename++;

	if (!basename)
		basename = file;

#ifdef BCMDBG_ASSERT
	snprintf(tempbuf, 256,
		 "assertion \"%s\" failed: file \"%s\", line %d\n", exp,
		 basename, line);

	/* Print assert message and give it time to be written to /var/log/messages */
	if (!in_interrupt()) {
		const int delay = 3;
		printk(KERN_ERR "%s", tempbuf);
		printk(KERN_ERR "panic in %d seconds\n", delay);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay * HZ);
	}

	switch (g_assert_type) {
	case 0:
		panic(KERN_ERR "%s", tempbuf);
		break;
	case 1:
		printk(KERN_ERR "%s", tempbuf);
		BUG();
		break;
	case 2:
		printk(KERN_ERR "%s", tempbuf);
		break;
	default:
		break;
	}
#endif				/* BCMDBG_ASSERT */

}
#endif				/* defined(BCMDBG_ASSERT) */

#if defined(BCMSDIO) && !defined(BRCM_FULLMAC)
u8 osl_readb(osl_t *osh, volatile u8 *r)
{
	osl_rreg_fn_t rreg = ((osl_pubinfo_t *) osh)->rreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	return (u8) ((rreg) (ctx, (void *)r, sizeof(u8)));
}

u16 osl_readw(osl_t *osh, volatile u16 *r)
{
	osl_rreg_fn_t rreg = ((osl_pubinfo_t *) osh)->rreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	return (u16) ((rreg) (ctx, (void *)r, sizeof(u16)));
}

u32 osl_readl(osl_t *osh, volatile u32 *r)
{
	osl_rreg_fn_t rreg = ((osl_pubinfo_t *) osh)->rreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	return (u32) ((rreg) (ctx, (void *)r, sizeof(u32)));
}

void osl_writeb(osl_t *osh, volatile u8 *r, u8 v)
{
	osl_wreg_fn_t wreg = ((osl_pubinfo_t *) osh)->wreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	((wreg) (ctx, (void *)r, v, sizeof(u8)));
}

void osl_writew(osl_t *osh, volatile u16 *r, u16 v)
{
	osl_wreg_fn_t wreg = ((osl_pubinfo_t *) osh)->wreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	((wreg) (ctx, (void *)r, v, sizeof(u16)));
}

void osl_writel(osl_t *osh, volatile u32 *r, u32 v)
{
	osl_wreg_fn_t wreg = ((osl_pubinfo_t *) osh)->wreg_fn;
	void *ctx = ((osl_pubinfo_t *) osh)->reg_ctx;

	((wreg) (ctx, (void *)r, v, sizeof(u32)));
}
#endif	/* BCMSDIO */
