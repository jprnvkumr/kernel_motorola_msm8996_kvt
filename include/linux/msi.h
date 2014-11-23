#ifndef LINUX_MSI_H
#define LINUX_MSI_H

#include <linux/kobject.h>
#include <linux/list.h>

struct msi_msg {
	u32	address_lo;	/* low 32 bits of msi message address */
	u32	address_hi;	/* high 32 bits of msi message address */
	u32	data;		/* 16 bits of msi message data */
};

extern int pci_msi_ignore_mask;
/* Helper functions */
struct irq_data;
struct msi_desc;
void __get_cached_msi_msg(struct msi_desc *entry, struct msi_msg *msg);
void get_cached_msi_msg(unsigned int irq, struct msi_msg *msg);

struct msi_desc {
	struct {
		__u8	is_msix	: 1;
		__u8	multiple: 3;	/* log2 num of messages allocated */
		__u8	multi_cap : 3;	/* log2 num of messages supported */
		__u8	maskbit	: 1;	/* mask-pending bit supported ? */
		__u8	is_64	: 1;	/* Address size: 0=32bit 1=64bit */
		__u16	entry_nr;	/* specific enabled entry */
		unsigned default_irq;	/* default pre-assigned irq */
	} msi_attrib;

	u32 masked;			/* mask bits */
	unsigned int irq;
	unsigned int nvec_used;		/* number of messages */
	struct list_head list;

	union {
		void __iomem *mask_base;
		u8 mask_pos;
	};
	struct pci_dev *dev;

	/* Last set MSI message */
	struct msi_msg msg;
};

void __pci_read_msi_msg(struct msi_desc *entry, struct msi_msg *msg);
void __pci_write_msi_msg(struct msi_desc *entry, struct msi_msg *msg);
void pci_write_msi_msg(unsigned int irq, struct msi_msg *msg);

u32 __pci_msix_desc_mask_irq(struct msi_desc *desc, u32 flag);
u32 __pci_msi_desc_mask_irq(struct msi_desc *desc, u32 mask, u32 flag);
void pci_msi_mask_irq(struct irq_data *data);
void pci_msi_unmask_irq(struct irq_data *data);

/* Conversion helpers. Should be removed after merging */
static inline void __write_msi_msg(struct msi_desc *entry, struct msi_msg *msg)
{
	__pci_write_msi_msg(entry, msg);
}
static inline void write_msi_msg(int irq, struct msi_msg *msg)
{
	pci_write_msi_msg(irq, msg);
}
static inline void mask_msi_irq(struct irq_data *data)
{
	pci_msi_mask_irq(data);
}
static inline void unmask_msi_irq(struct irq_data *data)
{
	pci_msi_unmask_irq(data);
}

/*
 * The arch hooks to setup up msi irqs. Those functions are
 * implemented as weak symbols so that they /can/ be overriden by
 * architecture specific code if needed.
 */
int arch_setup_msi_irq(struct pci_dev *dev, struct msi_desc *desc);
void arch_teardown_msi_irq(unsigned int irq);
int arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type);
void arch_teardown_msi_irqs(struct pci_dev *dev);
void arch_restore_msi_irqs(struct pci_dev *dev);

void default_teardown_msi_irqs(struct pci_dev *dev);
void default_restore_msi_irqs(struct pci_dev *dev);

struct msi_controller {
	struct module *owner;
	struct device *dev;
	struct device_node *of_node;
	struct list_head list;

	int (*setup_irq)(struct msi_controller *chip, struct pci_dev *dev,
			 struct msi_desc *desc);
	void (*teardown_irq)(struct msi_controller *chip, unsigned int irq);
};

#endif /* LINUX_MSI_H */
