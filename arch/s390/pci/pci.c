/*
 * Copyright IBM Corp. 2012
 *
 * Author(s):
 *   Jan Glauber <jang@linux.vnet.ibm.com>
 *
 * The System z PCI code is a rewrite from a prototype by
 * the following people (Kudoz!):
 *   Alexander Schmidt <alexschm@de.ibm.com>
 *   Christoph Raisch <raisch@de.ibm.com>
 *   Hannes Hering <hering2@de.ibm.com>
 *   Hoang-Nam Nguyen <hnguyen@de.ibm.com>
 *   Jan-Bernd Themann <themann@de.ibm.com>
 *   Stefan Roscher <stefan.roscher@de.ibm.com>
 *   Thomas Klein <tklein@de.ibm.com>
 */

#define COMPONENT "zPCI"
#define pr_fmt(fmt) COMPONENT ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/seq_file.h>
#include <linux/pci.h>
#include <linux/msi.h>

#include <asm/isc.h>
#include <asm/airq.h>
#include <asm/facility.h>
#include <asm/pci_insn.h>
#include <asm/pci_clp.h>

#define DEBUG				/* enable pr_debug */

#define	SIC_IRQ_MODE_ALL		0
#define	SIC_IRQ_MODE_SINGLE		1

#define ZPCI_NR_DMA_SPACES		1
#define ZPCI_MSI_VEC_BITS		6
#define ZPCI_NR_DEVICES			CONFIG_PCI_NR_FUNCTIONS

/* list of all detected zpci devices */
LIST_HEAD(zpci_list);
DEFINE_MUTEX(zpci_list_lock);

static DECLARE_BITMAP(zpci_domain, ZPCI_NR_DEVICES);
static DEFINE_SPINLOCK(zpci_domain_lock);

struct callback {
	irq_handler_t	handler;
	void		*data;
};

struct zdev_irq_map {
	unsigned long	aibv;		/* AI bit vector */
	int		msi_vecs;	/* consecutive MSI-vectors used */
	int		__unused;
	struct callback	cb[ZPCI_NR_MSI_VECS]; /* callback handler array */
	spinlock_t	lock;		/* protect callbacks against de-reg */
};

struct intr_bucket {
	/* amap of adapters, one bit per dev, corresponds to one irq nr */
	unsigned long	*alloc;
	/* AI summary bit, global page for all devices */
	unsigned long	*aisb;
	/* pointer to aibv and callback data in zdev */
	struct zdev_irq_map *imap[ZPCI_NR_DEVICES];
	/* protects the whole bucket struct */
	spinlock_t	lock;
};

static struct intr_bucket *bucket;

/* Adapter local summary indicator */
static u8 *zpci_irq_si;

static atomic_t irq_retries = ATOMIC_INIT(0);

/* I/O Map */
static DEFINE_SPINLOCK(zpci_iomap_lock);
static DECLARE_BITMAP(zpci_iomap, ZPCI_IOMAP_MAX_ENTRIES);
struct zpci_iomap_entry *zpci_iomap_start;
EXPORT_SYMBOL_GPL(zpci_iomap_start);

/* highest irq summary bit */
static int __read_mostly aisb_max;

static struct kmem_cache *zdev_irq_cache;

static inline int irq_to_msi_nr(unsigned int irq)
{
	return irq & ZPCI_MSI_MASK;
}

static inline int irq_to_dev_nr(unsigned int irq)
{
	return irq >> ZPCI_MSI_VEC_BITS;
}

static inline struct zdev_irq_map *get_imap(unsigned int irq)
{
	return bucket->imap[irq_to_dev_nr(irq)];
}

struct zpci_dev *get_zdev(struct pci_dev *pdev)
{
	return (struct zpci_dev *) pdev->sysdata;
}

struct zpci_dev *get_zdev_by_fid(u32 fid)
{
	struct zpci_dev *tmp, *zdev = NULL;

	mutex_lock(&zpci_list_lock);
	list_for_each_entry(tmp, &zpci_list, entry) {
		if (tmp->fid == fid) {
			zdev = tmp;
			break;
		}
	}
	mutex_unlock(&zpci_list_lock);
	return zdev;
}

bool zpci_fid_present(u32 fid)
{
	return (get_zdev_by_fid(fid) != NULL) ? true : false;
}

static struct zpci_dev *get_zdev_by_bus(struct pci_bus *bus)
{
	return (bus && bus->sysdata) ? (struct zpci_dev *) bus->sysdata : NULL;
}

int pci_domain_nr(struct pci_bus *bus)
{
	return ((struct zpci_dev *) bus->sysdata)->domain;
}
EXPORT_SYMBOL_GPL(pci_domain_nr);

int pci_proc_domain(struct pci_bus *bus)
{
	return pci_domain_nr(bus);
}
EXPORT_SYMBOL_GPL(pci_proc_domain);

/* Store PCI function information block */
static int zpci_store_fib(struct zpci_dev *zdev, u8 *fc)
{
	struct zpci_fib *fib;
	u8 status, cc;

	fib = (void *) get_zeroed_page(GFP_KERNEL);
	if (!fib)
		return -ENOMEM;

	do {
		cc = __stpcifc(zdev->fh, 0, fib, &status);
		if (cc == 2) {
			msleep(ZPCI_INSN_BUSY_DELAY);
			memset(fib, 0, PAGE_SIZE);
		}
	} while (cc == 2);

	if (cc)
		pr_err_once("%s: cc: %u  status: %u\n",
			    __func__, cc, status);

	/* Return PCI function controls */
	*fc = fib->fc;

	free_page((unsigned long) fib);
	return (cc) ? -EIO : 0;
}

/* Modify PCI: Register adapter interruptions */
static int zpci_register_airq(struct zpci_dev *zdev, unsigned int aisb,
			      u64 aibv)
{
	u64 req = ZPCI_CREATE_REQ(zdev->fh, 0, ZPCI_MOD_FC_REG_INT);
	struct zpci_fib *fib;
	int rc;

	fib = (void *) get_zeroed_page(GFP_KERNEL);
	if (!fib)
		return -ENOMEM;

	fib->isc = PCI_ISC;
	fib->noi = zdev->irq_map->msi_vecs;
	fib->sum = 1;		/* enable summary notifications */
	fib->aibv = aibv;
	fib->aibvo = 0;		/* every function has its own page */
	fib->aisb = (u64) bucket->aisb + aisb / 8;
	fib->aisbo = aisb & ZPCI_MSI_MASK;

	rc = mpcifc_instr(req, fib);
	pr_debug("%s mpcifc returned noi: %d\n", __func__, fib->noi);

	free_page((unsigned long) fib);
	return rc;
}

struct mod_pci_args {
	u64 base;
	u64 limit;
	u64 iota;
};

static int mod_pci(struct zpci_dev *zdev, int fn, u8 dmaas, struct mod_pci_args *args)
{
	u64 req = ZPCI_CREATE_REQ(zdev->fh, dmaas, fn);
	struct zpci_fib *fib;
	int rc;

	/* The FIB must be available even if it's not used */
	fib = (void *) get_zeroed_page(GFP_KERNEL);
	if (!fib)
		return -ENOMEM;

	fib->pba = args->base;
	fib->pal = args->limit;
	fib->iota = args->iota;

	rc = mpcifc_instr(req, fib);
	free_page((unsigned long) fib);
	return rc;
}

/* Modify PCI: Unregister adapter interruptions */
static int zpci_unregister_airq(struct zpci_dev *zdev)
{
	struct mod_pci_args args = { 0, 0, 0 };

	return mod_pci(zdev, ZPCI_MOD_FC_DEREG_INT, 0, &args);
}

#define ZPCI_PCIAS_CFGSPC	15

static int zpci_cfg_load(struct zpci_dev *zdev, int offset, u32 *val, u8 len)
{
	u64 req = ZPCI_CREATE_REQ(zdev->fh, ZPCI_PCIAS_CFGSPC, len);
	u64 data;
	int rc;

	rc = pcilg_instr(&data, req, offset);
	data = data << ((8 - len) * 8);
	data = le64_to_cpu(data);
	if (!rc)
		*val = (u32) data;
	else
		*val = 0xffffffff;
	return rc;
}

static int zpci_cfg_store(struct zpci_dev *zdev, int offset, u32 val, u8 len)
{
	u64 req = ZPCI_CREATE_REQ(zdev->fh, ZPCI_PCIAS_CFGSPC, len);
	u64 data = val;
	int rc;

	data = cpu_to_le64(data);
	data = data >> ((8 - len) * 8);
	rc = pcistg_instr(data, req, offset);
	return rc;
}

void synchronize_irq(unsigned int irq)
{
	/*
	 * Not needed, the handler is protected by a lock and IRQs that occur
	 * after the handler is deleted are just NOPs.
	 */
}
EXPORT_SYMBOL_GPL(synchronize_irq);

void enable_irq(unsigned int irq)
{
	struct msi_desc *msi = irq_get_msi_desc(irq);

	zpci_msi_set_mask_bits(msi, 1, 0);
}
EXPORT_SYMBOL_GPL(enable_irq);

void disable_irq(unsigned int irq)
{
	struct msi_desc *msi = irq_get_msi_desc(irq);

	zpci_msi_set_mask_bits(msi, 1, 1);
}
EXPORT_SYMBOL_GPL(disable_irq);

void disable_irq_nosync(unsigned int irq)
{
	disable_irq(irq);
}
EXPORT_SYMBOL_GPL(disable_irq_nosync);

unsigned long probe_irq_on(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(probe_irq_on);

int probe_irq_off(unsigned long val)
{
	return 0;
}
EXPORT_SYMBOL_GPL(probe_irq_off);

unsigned int probe_irq_mask(unsigned long val)
{
	return val;
}
EXPORT_SYMBOL_GPL(probe_irq_mask);

void __devinit pcibios_fixup_bus(struct pci_bus *bus)
{
}

resource_size_t pcibios_align_resource(void *data, const struct resource *res,
				       resource_size_t size,
				       resource_size_t align)
{
	return 0;
}

/* Create a virtual mapping cookie for a PCI BAR */
void __iomem *pci_iomap(struct pci_dev *pdev, int bar, unsigned long max)
{
	struct zpci_dev *zdev =	get_zdev(pdev);
	u64 addr;
	int idx;

	if ((bar & 7) != bar)
		return NULL;

	idx = zdev->bars[bar].map_idx;
	spin_lock(&zpci_iomap_lock);
	zpci_iomap_start[idx].fh = zdev->fh;
	zpci_iomap_start[idx].bar = bar;
	spin_unlock(&zpci_iomap_lock);

	addr = ZPCI_IOMAP_ADDR_BASE | ((u64) idx << 48);
	return (void __iomem *) addr;
}
EXPORT_SYMBOL_GPL(pci_iomap);

void pci_iounmap(struct pci_dev *pdev, void __iomem *addr)
{
	unsigned int idx;

	idx = (((__force u64) addr) & ~ZPCI_IOMAP_ADDR_BASE) >> 48;
	spin_lock(&zpci_iomap_lock);
	zpci_iomap_start[idx].fh = 0;
	zpci_iomap_start[idx].bar = 0;
	spin_unlock(&zpci_iomap_lock);
}
EXPORT_SYMBOL_GPL(pci_iounmap);

static int pci_read(struct pci_bus *bus, unsigned int devfn, int where,
		    int size, u32 *val)
{
	struct zpci_dev *zdev = get_zdev_by_bus(bus);

	if (!zdev || devfn != ZPCI_DEVFN)
		return 0;
	return zpci_cfg_load(zdev, where, val, size);
}

static int pci_write(struct pci_bus *bus, unsigned int devfn, int where,
		     int size, u32 val)
{
	struct zpci_dev *zdev = get_zdev_by_bus(bus);

	if (!zdev || devfn != ZPCI_DEVFN)
		return 0;
	return zpci_cfg_store(zdev, where, val, size);
}

static struct pci_ops pci_root_ops = {
	.read = pci_read,
	.write = pci_write,
};

/* store the last handled bit to implement fair scheduling of devices */
static DEFINE_PER_CPU(unsigned long, next_sbit);

static void zpci_irq_handler(void *dont, void *need)
{
	unsigned long sbit, mbit, last = 0, start = __get_cpu_var(next_sbit);
	int rescan = 0, max = aisb_max;
	struct zdev_irq_map *imap;

	kstat_cpu(smp_processor_id()).irqs[IOINT_PCI]++;
	sbit = start;

scan:
	/* find summary_bit */
	for_each_set_bit_left_cont(sbit, bucket->aisb, max) {
		clear_bit(63 - (sbit & 63), bucket->aisb + (sbit >> 6));
		last = sbit;

		/* find vector bit */
		imap = bucket->imap[sbit];
		for_each_set_bit_left(mbit, &imap->aibv, imap->msi_vecs) {
			kstat_cpu(smp_processor_id()).irqs[IOINT_MSI]++;
			clear_bit(63 - mbit, &imap->aibv);

			spin_lock(&imap->lock);
			if (imap->cb[mbit].handler)
				imap->cb[mbit].handler(mbit,
					imap->cb[mbit].data);
			spin_unlock(&imap->lock);
		}
	}

	if (rescan)
		goto out;

	/* scan the skipped bits */
	if (start > 0) {
		sbit = 0;
		max = start;
		start = 0;
		goto scan;
	}

	/* enable interrupts again */
	sic_instr(SIC_IRQ_MODE_SINGLE, NULL, PCI_ISC);

	/* check again to not lose initiative */
	rmb();
	max = aisb_max;
	sbit = find_first_bit_left(bucket->aisb, max);
	if (sbit != max) {
		atomic_inc(&irq_retries);
		rescan++;
		goto scan;
	}
out:
	/* store next device bit to scan */
	__get_cpu_var(next_sbit) = (++last >= aisb_max) ? 0 : last;
}

/* msi_vecs - number of requested interrupts, 0 place function to error state */
static int zpci_setup_msi(struct pci_dev *pdev, int msi_vecs)
{
	struct zpci_dev *zdev = get_zdev(pdev);
	unsigned int aisb, msi_nr;
	struct msi_desc *msi;
	int rc;

	/* store the number of used MSI vectors */
	zdev->irq_map->msi_vecs = min(msi_vecs, ZPCI_NR_MSI_VECS);

	spin_lock(&bucket->lock);
	aisb = find_first_zero_bit(bucket->alloc, PAGE_SIZE);
	/* alloc map exhausted? */
	if (aisb == PAGE_SIZE) {
		spin_unlock(&bucket->lock);
		return -EIO;
	}
	set_bit(aisb, bucket->alloc);
	spin_unlock(&bucket->lock);

	zdev->aisb = aisb;
	if (aisb + 1 > aisb_max)
		aisb_max = aisb + 1;

	/* wire up IRQ shortcut pointer */
	bucket->imap[zdev->aisb] = zdev->irq_map;
	pr_debug("%s: imap[%u] linked to %p\n", __func__, zdev->aisb, zdev->irq_map);

	/* TODO: irq number 0 wont be found if we return less than requested MSIs.
	 * ignore it for now and fix in common code.
	 */
	msi_nr = aisb << ZPCI_MSI_VEC_BITS;

	list_for_each_entry(msi, &pdev->msi_list, list) {
		rc = zpci_setup_msi_irq(zdev, msi, msi_nr,
					  aisb << ZPCI_MSI_VEC_BITS);
		if (rc)
			return rc;
		msi_nr++;
	}

	rc = zpci_register_airq(zdev, aisb, (u64) &zdev->irq_map->aibv);
	if (rc) {
		clear_bit(aisb, bucket->alloc);
		dev_err(&pdev->dev, "register MSI failed with: %d\n", rc);
		return rc;
	}
	return (zdev->irq_map->msi_vecs == msi_vecs) ?
		0 : zdev->irq_map->msi_vecs;
}

static void zpci_teardown_msi(struct pci_dev *pdev)
{
	struct zpci_dev *zdev = get_zdev(pdev);
	struct msi_desc *msi;
	int aisb, rc;

	rc = zpci_unregister_airq(zdev);
	if (rc) {
		dev_err(&pdev->dev, "deregister MSI failed with: %d\n", rc);
		return;
	}

	msi = list_first_entry(&pdev->msi_list, struct msi_desc, list);
	aisb = irq_to_dev_nr(msi->irq);

	list_for_each_entry(msi, &pdev->msi_list, list)
		zpci_teardown_msi_irq(zdev, msi);

	clear_bit(aisb, bucket->alloc);
	if (aisb + 1 == aisb_max)
		aisb_max--;
}

int arch_setup_msi_irqs(struct pci_dev *pdev, int nvec, int type)
{
	pr_debug("%s: requesting %d MSI-X interrupts...", __func__, nvec);
	if (type != PCI_CAP_ID_MSIX && type != PCI_CAP_ID_MSI)
		return -EINVAL;
	return zpci_setup_msi(pdev, nvec);
}

void arch_teardown_msi_irqs(struct pci_dev *pdev)
{
	pr_info("%s: on pdev: %p\n", __func__, pdev);
	zpci_teardown_msi(pdev);
}

static void zpci_map_resources(struct zpci_dev *zdev)
{
	struct pci_dev *pdev = zdev->pdev;
	resource_size_t len;
	int i;

	for (i = 0; i < PCI_BAR_COUNT; i++) {
		len = pci_resource_len(pdev, i);
		if (!len)
			continue;
		pdev->resource[i].start = (resource_size_t) pci_iomap(pdev, i, 0);
		pdev->resource[i].end = pdev->resource[i].start + len - 1;
		pr_debug("BAR%i: -> start: %Lx  end: %Lx\n",
			i, pdev->resource[i].start, pdev->resource[i].end);
	}
};

static void zpci_unmap_resources(struct pci_dev *pdev)
{
	resource_size_t len;
	int i;

	for (i = 0; i < PCI_BAR_COUNT; i++) {
		len = pci_resource_len(pdev, i);
		if (!len)
			continue;
		pci_iounmap(pdev, (void *) pdev->resource[i].start);
	}
};

struct zpci_dev *zpci_alloc_device(void)
{
	struct zpci_dev *zdev;

	/* Alloc memory for our private pci device data */
	zdev = kzalloc(sizeof(*zdev), GFP_KERNEL);
	if (!zdev)
		return ERR_PTR(-ENOMEM);

	/* Alloc aibv & callback space */
	zdev->irq_map = kmem_cache_alloc(zdev_irq_cache, GFP_KERNEL);
	if (!zdev->irq_map)
		goto error;
	memset(zdev->irq_map, 0, sizeof(*zdev->irq_map));
	WARN_ON((u64) zdev->irq_map & 0xff);
	return zdev;

error:
	kfree(zdev);
	return ERR_PTR(-ENOMEM);
}

void zpci_free_device(struct zpci_dev *zdev)
{
	kmem_cache_free(zdev_irq_cache, zdev->irq_map);
	kfree(zdev);
}

/* Called on removal of pci_dev, leaves zpci and bus device */
static void zpci_remove_device(struct pci_dev *pdev)
{
	struct zpci_dev *zdev = get_zdev(pdev);

	dev_info(&pdev->dev, "Removing device %u\n", zdev->domain);
	zdev->state = ZPCI_FN_STATE_CONFIGURED;
	zpci_unmap_resources(pdev);
	list_del(&zdev->entry);		/* can be called from init */
	zdev->pdev = NULL;
}

static void zpci_scan_devices(void)
{
	struct zpci_dev *zdev;

	mutex_lock(&zpci_list_lock);
	list_for_each_entry(zdev, &zpci_list, entry)
		if (zdev->state == ZPCI_FN_STATE_CONFIGURED)
			zpci_scan_device(zdev);
	mutex_unlock(&zpci_list_lock);
}

/*
 * Too late for any s390 specific setup, since interrupts must be set up
 * already which requires DMA setup too and the pci scan will access the
 * config space, which only works if the function handle is enabled.
 */
int pcibios_enable_device(struct pci_dev *pdev, int mask)
{
	struct resource *res;
	u16 cmd;
	int i;

	pci_read_config_word(pdev, PCI_COMMAND, &cmd);

	for (i = 0; i < PCI_BAR_COUNT; i++) {
		res = &pdev->resource[i];

		if (res->flags & IORESOURCE_IO)
			return -EINVAL;

		if (res->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	pci_write_config_word(pdev, PCI_COMMAND, cmd);
	return 0;
}

void pcibios_disable_device(struct pci_dev *pdev)
{
	zpci_remove_device(pdev);
	pdev->sysdata = NULL;
}

int zpci_request_irq(unsigned int irq, irq_handler_t handler, void *data)
{
	int msi_nr = irq_to_msi_nr(irq);
	struct zdev_irq_map *imap;
	struct msi_desc *msi;

	msi = irq_get_msi_desc(irq);
	if (!msi)
		return -EIO;

	imap = get_imap(irq);
	spin_lock_init(&imap->lock);

	pr_debug("%s: register handler for IRQ:MSI %d:%d\n", __func__, irq >> 6, msi_nr);
	imap->cb[msi_nr].handler = handler;
	imap->cb[msi_nr].data = data;

	/*
	 * The generic MSI code returns with the interrupt disabled on the
	 * card, using the MSI mask bits. Firmware doesn't appear to unmask
	 * at that level, so we do it here by hand.
	 */
	zpci_msi_set_mask_bits(msi, 1, 0);
	return 0;
}

void zpci_free_irq(unsigned int irq)
{
	struct zdev_irq_map *imap = get_imap(irq);
	int msi_nr = irq_to_msi_nr(irq);
	unsigned long flags;

	pr_debug("%s: for irq: %d\n", __func__, irq);

	spin_lock_irqsave(&imap->lock, flags);
	imap->cb[msi_nr].handler = NULL;
	imap->cb[msi_nr].data = NULL;
	spin_unlock_irqrestore(&imap->lock, flags);
}

int request_irq(unsigned int irq, irq_handler_t handler,
		unsigned long irqflags, const char *devname, void *dev_id)
{
	pr_debug("%s: irq: %d  handler: %p  flags: %lx  dev: %s\n",
		__func__, irq, handler, irqflags, devname);

	return zpci_request_irq(irq, handler, dev_id);
}
EXPORT_SYMBOL_GPL(request_irq);

void free_irq(unsigned int irq, void *dev_id)
{
	zpci_free_irq(irq);
}
EXPORT_SYMBOL_GPL(free_irq);

static int __init zpci_irq_init(void)
{
	int cpu, rc;

	bucket = kzalloc(sizeof(*bucket), GFP_KERNEL);
	if (!bucket)
		return -ENOMEM;

	bucket->aisb = (unsigned long *) get_zeroed_page(GFP_KERNEL);
	if (!bucket->aisb) {
		rc = -ENOMEM;
		goto out_aisb;
	}

	bucket->alloc = (unsigned long *) get_zeroed_page(GFP_KERNEL);
	if (!bucket->alloc) {
		rc = -ENOMEM;
		goto out_alloc;
	}

	isc_register(PCI_ISC);
	zpci_irq_si = s390_register_adapter_interrupt(&zpci_irq_handler, NULL, PCI_ISC);
	if (IS_ERR(zpci_irq_si)) {
		rc = PTR_ERR(zpci_irq_si);
		zpci_irq_si = NULL;
		goto out_ai;
	}

	for_each_online_cpu(cpu)
		per_cpu(next_sbit, cpu) = 0;

	spin_lock_init(&bucket->lock);
	/* set summary to 1 to be called every time for the ISC */
	*zpci_irq_si = 1;
	sic_instr(SIC_IRQ_MODE_SINGLE, NULL, PCI_ISC);
	return 0;

out_ai:
	isc_unregister(PCI_ISC);
	free_page((unsigned long) bucket->alloc);
out_alloc:
	free_page((unsigned long) bucket->aisb);
out_aisb:
	kfree(bucket);
	return rc;
}

static void zpci_irq_exit(void)
{
	free_page((unsigned long) bucket->alloc);
	free_page((unsigned long) bucket->aisb);
	s390_unregister_adapter_interrupt(zpci_irq_si, PCI_ISC);
	isc_unregister(PCI_ISC);
	kfree(bucket);
}

static struct resource *zpci_alloc_bus_resource(unsigned long start, unsigned long size,
						unsigned long flags, int domain)
{
	struct resource *r;
	char *name;
	int rc;

	r = kzalloc(sizeof(*r), GFP_KERNEL);
	if (!r)
		return ERR_PTR(-ENOMEM);
	r->start = start;
	r->end = r->start + size - 1;
	r->flags = flags;
	r->parent = &iomem_resource;
	name = kmalloc(18, GFP_KERNEL);
	if (!name) {
		kfree(r);
		return ERR_PTR(-ENOMEM);
	}
	sprintf(name, "PCI Bus: %04x:%02x", domain, ZPCI_BUS_NR);
	r->name = name;

	rc = request_resource(&iomem_resource, r);
	if (rc)
		pr_debug("request resource %pR failed\n", r);
	return r;
}

static int zpci_alloc_iomap(struct zpci_dev *zdev)
{
	int entry;

	spin_lock(&zpci_iomap_lock);
	entry = find_first_zero_bit(zpci_iomap, ZPCI_IOMAP_MAX_ENTRIES);
	if (entry == ZPCI_IOMAP_MAX_ENTRIES) {
		spin_unlock(&zpci_iomap_lock);
		return -ENOSPC;
	}
	set_bit(entry, zpci_iomap);
	spin_unlock(&zpci_iomap_lock);
	return entry;
}

static void zpci_free_iomap(struct zpci_dev *zdev, int entry)
{
	spin_lock(&zpci_iomap_lock);
	memset(&zpci_iomap_start[entry], 0, sizeof(struct zpci_iomap_entry));
	clear_bit(entry, zpci_iomap);
	spin_unlock(&zpci_iomap_lock);
}

static int zpci_create_device_bus(struct zpci_dev *zdev)
{
	struct resource *res;
	LIST_HEAD(resources);
	int i;

	/* allocate mapping entry for each used bar */
	for (i = 0; i < PCI_BAR_COUNT; i++) {
		unsigned long addr, size, flags;
		int entry;

		if (!zdev->bars[i].size)
			continue;
		entry = zpci_alloc_iomap(zdev);
		if (entry < 0)
			return entry;
		zdev->bars[i].map_idx = entry;

		/* only MMIO is supported */
		flags = IORESOURCE_MEM;
		if (zdev->bars[i].val & 8)
			flags |= IORESOURCE_PREFETCH;
		if (zdev->bars[i].val & 4)
			flags |= IORESOURCE_MEM_64;

		addr = ZPCI_IOMAP_ADDR_BASE + ((u64) entry << 48);

		size = 1UL << zdev->bars[i].size;

		res = zpci_alloc_bus_resource(addr, size, flags, zdev->domain);
		if (IS_ERR(res)) {
			zpci_free_iomap(zdev, entry);
			return PTR_ERR(res);
		}
		pci_add_resource(&resources, res);
	}

	zdev->bus = pci_create_root_bus(NULL, ZPCI_BUS_NR, &pci_root_ops,
					zdev, &resources);
	if (!zdev->bus)
		return -EIO;

	zdev->bus->max_bus_speed = zdev->max_bus_speed;
	return 0;
}

static int zpci_alloc_domain(struct zpci_dev *zdev)
{
	spin_lock(&zpci_domain_lock);
	zdev->domain = find_first_zero_bit(zpci_domain, ZPCI_NR_DEVICES);
	if (zdev->domain == ZPCI_NR_DEVICES) {
		spin_unlock(&zpci_domain_lock);
		return -ENOSPC;
	}
	set_bit(zdev->domain, zpci_domain);
	spin_unlock(&zpci_domain_lock);
	return 0;
}

static void zpci_free_domain(struct zpci_dev *zdev)
{
	spin_lock(&zpci_domain_lock);
	clear_bit(zdev->domain, zpci_domain);
	spin_unlock(&zpci_domain_lock);
}

int zpci_enable_device(struct zpci_dev *zdev)
{
	int rc;

	rc = clp_enable_fh(zdev, ZPCI_NR_DMA_SPACES);
	if (rc)
		goto out;
	pr_info("Enabled fh: 0x%x fid: 0x%x\n", zdev->fh, zdev->fid);
	return 0;
out:
	return rc;
}
EXPORT_SYMBOL_GPL(zpci_enable_device);

int zpci_create_device(struct zpci_dev *zdev)
{
	int rc;

	rc = zpci_alloc_domain(zdev);
	if (rc)
		goto out;

	rc = zpci_create_device_bus(zdev);
	if (rc)
		goto out_bus;

	mutex_lock(&zpci_list_lock);
	list_add_tail(&zdev->entry, &zpci_list);
	mutex_unlock(&zpci_list_lock);

	if (zdev->state == ZPCI_FN_STATE_STANDBY)
		return 0;

	rc = zpci_enable_device(zdev);
	if (rc)
		goto out_start;
	return 0;

out_start:
	mutex_lock(&zpci_list_lock);
	list_del(&zdev->entry);
	mutex_unlock(&zpci_list_lock);
out_bus:
	zpci_free_domain(zdev);
out:
	return rc;
}

void zpci_stop_device(struct zpci_dev *zdev)
{
	/*
	 * Note: SCLP disables fh via set-pci-fn so don't
	 * do that here.
	 */
}
EXPORT_SYMBOL_GPL(zpci_stop_device);

int zpci_scan_device(struct zpci_dev *zdev)
{
	zdev->pdev = pci_scan_single_device(zdev->bus, ZPCI_DEVFN);
	if (!zdev->pdev) {
		pr_err("pci_scan_single_device failed for fid: 0x%x\n",
			zdev->fid);
		goto out;
	}

	zpci_map_resources(zdev);
	pci_bus_add_devices(zdev->bus);

	/* now that pdev was added to the bus mark it as used */
	zdev->state = ZPCI_FN_STATE_ONLINE;
	return 0;

out:
	clp_disable_fh(zdev);
	return -EIO;
}
EXPORT_SYMBOL_GPL(zpci_scan_device);

static inline int barsize(u8 size)
{
	return (size) ? (1 << size) >> 10 : 0;
}

static int zpci_mem_init(void)
{
	zdev_irq_cache = kmem_cache_create("PCI_IRQ_cache", sizeof(struct zdev_irq_map),
				L1_CACHE_BYTES, SLAB_HWCACHE_ALIGN, NULL);
	if (!zdev_irq_cache)
		goto error_zdev;

	/* TODO: use realloc */
	zpci_iomap_start = kzalloc(ZPCI_IOMAP_MAX_ENTRIES * sizeof(*zpci_iomap_start),
				   GFP_KERNEL);
	if (!zpci_iomap_start)
		goto error_iomap;
	return 0;

error_iomap:
	kmem_cache_destroy(zdev_irq_cache);
error_zdev:
	return -ENOMEM;
}

static void zpci_mem_exit(void)
{
	kfree(zpci_iomap_start);
	kmem_cache_destroy(zdev_irq_cache);
}

unsigned int pci_probe = 1;
EXPORT_SYMBOL_GPL(pci_probe);

char * __init pcibios_setup(char *str)
{
	if (!strcmp(str, "off")) {
		pci_probe = 0;
		return NULL;
	}
	return str;
}

static int __init pci_base_init(void)
{
	int rc;

	if (!pci_probe)
		return 0;

	if (!test_facility(2) || !test_facility(69)
	    || !test_facility(71) || !test_facility(72))
		return 0;

	pr_info("Probing PCI hardware: PCI:%d  SID:%d  AEN:%d\n",
		test_facility(69), test_facility(70),
		test_facility(71));

	rc = zpci_mem_init();
	if (rc)
		goto out_mem;

	rc = zpci_msihash_init();
	if (rc)
		goto out_hash;

	rc = zpci_irq_init();
	if (rc)
		goto out_irq;

	rc = clp_find_pci_devices();
	if (rc)
		goto out_find;

	zpci_scan_devices();
	return 0;

out_find:
	zpci_irq_exit();
out_irq:
	zpci_msihash_exit();
out_hash:
	zpci_mem_exit();
out_mem:
	return rc;
}
subsys_initcall(pci_base_init);
