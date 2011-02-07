/*
 * linux/kernel/irq/chip.c
 *
 * Copyright (C) 1992, 1998-2006 Linus Torvalds, Ingo Molnar
 * Copyright (C) 2005-2006, Thomas Gleixner, Russell King
 *
 * This file contains the core interrupt handling code, for irq-chip
 * based architectures.
 *
 * Detailed information is available in Documentation/DocBook/genericirq
 */

#include <linux/irq.h>
#include <linux/msi.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include "internals.h"

/**
 *	irq_set_chip - set the irq chip for an irq
 *	@irq:	irq number
 *	@chip:	pointer to irq chip description structure
 */
int irq_set_chip(unsigned int irq, struct irq_chip *chip)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		WARN(1, KERN_ERR "Trying to install chip for IRQ%d\n", irq);
		return -EINVAL;
	}

	if (!chip)
		chip = &no_irq_chip;

	raw_spin_lock_irqsave(&desc->lock, flags);
	irq_chip_set_defaults(chip);
	desc->irq_data.chip = chip;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(irq_set_chip);

/**
 *	irq_set_type - set the irq trigger type for an irq
 *	@irq:	irq number
 *	@type:	IRQ_TYPE_{LEVEL,EDGE}_* value - see include/linux/irq.h
 */
int irq_set_irq_type(unsigned int irq, unsigned int type)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;
	int ret = -ENXIO;

	if (!desc) {
		printk(KERN_ERR "Trying to set irq type for IRQ%d\n", irq);
		return -ENODEV;
	}

	type &= IRQ_TYPE_SENSE_MASK;
	if (type == IRQ_TYPE_NONE)
		return 0;

	chip_bus_lock(desc);
	raw_spin_lock_irqsave(&desc->lock, flags);
	ret = __irq_set_trigger(desc, irq, type);
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	chip_bus_sync_unlock(desc);
	return ret;
}
EXPORT_SYMBOL(irq_set_irq_type);

/**
 *	irq_set_handler_data - set irq handler data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to interrupt specific data
 *
 *	Set the hardware irq controller data for an irq
 */
int irq_set_handler_data(unsigned int irq, void *data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install controller data for IRQ%d\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->irq_data.handler_data = data;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}
EXPORT_SYMBOL(irq_set_handler_data);

/**
 *	irq_set_msi_desc - set MSI descriptor data for an irq
 *	@irq:	Interrupt number
 *	@entry:	Pointer to MSI descriptor data
 *
 *	Set the MSI descriptor entry for an irq
 */
int irq_set_msi_desc(unsigned int irq, struct msi_desc *entry)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install msi data for IRQ%d\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->irq_data.msi_desc = entry;
	if (entry)
		entry->irq = irq;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	return 0;
}

/**
 *	irq_set_chip_data - set irq chip data for an irq
 *	@irq:	Interrupt number
 *	@data:	Pointer to chip specific data
 *
 *	Set the hardware irq chip data for an irq
 */
int irq_set_chip_data(unsigned int irq, void *data)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install chip data for IRQ%d\n", irq);
		return -EINVAL;
	}

	if (!desc->irq_data.chip) {
		printk(KERN_ERR "BUG: bad set_irq_chip_data(IRQ#%d)\n", irq);
		return -EINVAL;
	}

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->irq_data.chip_data = data;
	raw_spin_unlock_irqrestore(&desc->lock, flags);

	return 0;
}
EXPORT_SYMBOL(irq_set_chip_data);

struct irq_data *irq_get_irq_data(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return desc ? &desc->irq_data : NULL;
}
EXPORT_SYMBOL_GPL(irq_get_irq_data);

int irq_startup(struct irq_desc *desc)
{
	desc->status &= ~IRQ_DISABLED;
	desc->depth = 0;

	if (desc->irq_data.chip->irq_startup) {
		int ret = desc->irq_data.chip->irq_startup(&desc->irq_data);
		desc->status &= ~IRQ_MASKED;
		return ret;
	}

	irq_enable(desc);
	return 0;
}

void irq_shutdown(struct irq_desc *desc)
{
	desc->status |= IRQ_DISABLED;
	desc->depth = 1;
	if (desc->irq_data.chip->irq_shutdown)
		desc->irq_data.chip->irq_shutdown(&desc->irq_data);
	if (desc->irq_data.chip->irq_disable)
		desc->irq_data.chip->irq_disable(&desc->irq_data);
	else
		desc->irq_data.chip->irq_mask(&desc->irq_data);
	desc->status |= IRQ_MASKED;
}

void irq_enable(struct irq_desc *desc)
{
	desc->status &= ~IRQ_DISABLED;
	if (desc->irq_data.chip->irq_enable)
		desc->irq_data.chip->irq_enable(&desc->irq_data);
	else
		desc->irq_data.chip->irq_unmask(&desc->irq_data);
	desc->status &= ~IRQ_MASKED;
}

void irq_disable(struct irq_desc *desc)
{
	desc->status |= IRQ_DISABLED;
	if (desc->irq_data.chip->irq_disable) {
		desc->irq_data.chip->irq_disable(&desc->irq_data);
		desc->status |= IRQ_MASKED;
	}
}

#ifndef CONFIG_GENERIC_HARDIRQS_NO_DEPRECATED
/* Temporary migration helpers */
static void compat_irq_mask(struct irq_data *data)
{
	data->chip->mask(data->irq);
}

static void compat_irq_unmask(struct irq_data *data)
{
	data->chip->unmask(data->irq);
}

static void compat_irq_ack(struct irq_data *data)
{
	data->chip->ack(data->irq);
}

static void compat_irq_mask_ack(struct irq_data *data)
{
	data->chip->mask_ack(data->irq);
}

static void compat_irq_eoi(struct irq_data *data)
{
	data->chip->eoi(data->irq);
}

static void compat_irq_enable(struct irq_data *data)
{
	data->chip->enable(data->irq);
}

static void compat_irq_disable(struct irq_data *data)
{
	data->chip->disable(data->irq);
}

static void compat_irq_shutdown(struct irq_data *data)
{
	data->chip->shutdown(data->irq);
}

static unsigned int compat_irq_startup(struct irq_data *data)
{
	return data->chip->startup(data->irq);
}

static int compat_irq_set_affinity(struct irq_data *data,
				   const struct cpumask *dest, bool force)
{
	return data->chip->set_affinity(data->irq, dest);
}

static int compat_irq_set_type(struct irq_data *data, unsigned int type)
{
	return data->chip->set_type(data->irq, type);
}

static int compat_irq_set_wake(struct irq_data *data, unsigned int on)
{
	return data->chip->set_wake(data->irq, on);
}

static int compat_irq_retrigger(struct irq_data *data)
{
	return data->chip->retrigger(data->irq);
}

static void compat_bus_lock(struct irq_data *data)
{
	data->chip->bus_lock(data->irq);
}

static void compat_bus_sync_unlock(struct irq_data *data)
{
	data->chip->bus_sync_unlock(data->irq);
}
#endif

/*
 * Fixup enable/disable function pointers
 */
void irq_chip_set_defaults(struct irq_chip *chip)
{
#ifndef CONFIG_GENERIC_HARDIRQS_NO_DEPRECATED
	if (chip->enable)
		chip->irq_enable = compat_irq_enable;
	if (chip->disable)
		chip->irq_disable = compat_irq_disable;
	if (chip->shutdown)
		chip->irq_shutdown = compat_irq_shutdown;
	if (chip->startup)
		chip->irq_startup = compat_irq_startup;
	if (!chip->end)
		chip->end = dummy_irq_chip.end;
	if (chip->bus_lock)
		chip->irq_bus_lock = compat_bus_lock;
	if (chip->bus_sync_unlock)
		chip->irq_bus_sync_unlock = compat_bus_sync_unlock;
	if (chip->mask)
		chip->irq_mask = compat_irq_mask;
	if (chip->unmask)
		chip->irq_unmask = compat_irq_unmask;
	if (chip->ack)
		chip->irq_ack = compat_irq_ack;
	if (chip->mask_ack)
		chip->irq_mask_ack = compat_irq_mask_ack;
	if (chip->eoi)
		chip->irq_eoi = compat_irq_eoi;
	if (chip->set_affinity)
		chip->irq_set_affinity = compat_irq_set_affinity;
	if (chip->set_type)
		chip->irq_set_type = compat_irq_set_type;
	if (chip->set_wake)
		chip->irq_set_wake = compat_irq_set_wake;
	if (chip->retrigger)
		chip->irq_retrigger = compat_irq_retrigger;
#endif
}

static inline void mask_ack_irq(struct irq_desc *desc)
{
	if (desc->irq_data.chip->irq_mask_ack)
		desc->irq_data.chip->irq_mask_ack(&desc->irq_data);
	else {
		desc->irq_data.chip->irq_mask(&desc->irq_data);
		if (desc->irq_data.chip->irq_ack)
			desc->irq_data.chip->irq_ack(&desc->irq_data);
	}
	desc->status |= IRQ_MASKED;
}

static inline void mask_irq(struct irq_desc *desc)
{
	if (desc->irq_data.chip->irq_mask) {
		desc->irq_data.chip->irq_mask(&desc->irq_data);
		desc->status |= IRQ_MASKED;
	}
}

static inline void unmask_irq(struct irq_desc *desc)
{
	if (desc->irq_data.chip->irq_unmask) {
		desc->irq_data.chip->irq_unmask(&desc->irq_data);
		desc->status &= ~IRQ_MASKED;
	}
}

/*
 *	handle_nested_irq - Handle a nested irq from a irq thread
 *	@irq:	the interrupt number
 *
 *	Handle interrupts which are nested into a threaded interrupt
 *	handler. The handler function is called inside the calling
 *	threads context.
 */
void handle_nested_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	struct irqaction *action;
	irqreturn_t action_ret;

	might_sleep();

	raw_spin_lock_irq(&desc->lock);

	kstat_incr_irqs_this_cpu(irq, desc);

	action = desc->action;
	if (unlikely(!action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	irq_compat_set_progress(desc);
	desc->istate |= IRQS_INPROGRESS;
	raw_spin_unlock_irq(&desc->lock);

	action_ret = action->thread_fn(action->irq, action->dev_id);
	if (!noirqdebug)
		note_interrupt(irq, desc, action_ret);

	raw_spin_lock_irq(&desc->lock);
	desc->istate &= ~IRQS_INPROGRESS;
	irq_compat_clr_progress(desc);

out_unlock:
	raw_spin_unlock_irq(&desc->lock);
}
EXPORT_SYMBOL_GPL(handle_nested_irq);

static bool irq_check_poll(struct irq_desc *desc)
{
	if (!(desc->istate & IRQS_POLL_INPROGRESS))
		return false;
	return irq_wait_for_poll(desc);
}

/**
 *	handle_simple_irq - Simple and software-decoded IRQs.
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Simple interrupts are either sent from a demultiplexing interrupt
 *	handler or come from hardware, where no interrupt hardware control
 *	is necessary.
 *
 *	Note: The caller is expected to handle the ack, clear, mask and
 *	unmask issues if necessary.
 */
void
handle_simple_irq(unsigned int irq, struct irq_desc *desc)
{
	raw_spin_lock(&desc->lock);

	if (unlikely(desc->istate & IRQS_INPROGRESS))
		if (!irq_check_poll(desc))
			goto out_unlock;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	if (unlikely(!desc->action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	handle_irq_event(desc);

out_unlock:
	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_level_irq - Level type irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Level type interrupts are active as long as the hardware line has
 *	the active level. This may require to mask the interrupt and unmask
 *	it after the associated handler has acknowledged the device, so the
 *	interrupt line is back to inactive.
 */
void
handle_level_irq(unsigned int irq, struct irq_desc *desc)
{
	raw_spin_lock(&desc->lock);
	mask_ack_irq(desc);

	if (unlikely(desc->istate & IRQS_INPROGRESS))
		if (!irq_check_poll(desc))
			goto out_unlock;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	/*
	 * If its disabled or no action available
	 * keep it masked and get out of here
	 */
	if (unlikely(!desc->action || (desc->status & IRQ_DISABLED)))
		goto out_unlock;

	handle_irq_event(desc);

	if (!(desc->status & (IRQ_DISABLED | IRQ_ONESHOT)))
		unmask_irq(desc);
out_unlock:
	raw_spin_unlock(&desc->lock);
}
EXPORT_SYMBOL_GPL(handle_level_irq);

/**
 *	handle_fasteoi_irq - irq handler for transparent controllers
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Only a single callback will be issued to the chip: an ->eoi()
 *	call when the interrupt has been serviced. This enables support
 *	for modern forms of interrupt handlers, which handle the flow
 *	details in hardware, transparently.
 */
void
handle_fasteoi_irq(unsigned int irq, struct irq_desc *desc)
{
	raw_spin_lock(&desc->lock);

	if (unlikely(desc->istate & IRQS_INPROGRESS))
		if (!irq_check_poll(desc))
			goto out;

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);
	kstat_incr_irqs_this_cpu(irq, desc);

	/*
	 * If its disabled or no action available
	 * then mask it and get out of here:
	 */
	if (unlikely(!desc->action || (desc->status & IRQ_DISABLED))) {
		desc->status |= IRQ_PENDING;
		mask_irq(desc);
		goto out;
	}
	handle_irq_event(desc);
out:
	desc->irq_data.chip->irq_eoi(&desc->irq_data);
	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_edge_irq - edge type IRQ handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Interrupt occures on the falling and/or rising edge of a hardware
 *	signal. The occurence is latched into the irq controller hardware
 *	and must be acked in order to be reenabled. After the ack another
 *	interrupt can happen on the same source even before the first one
 *	is handled by the associated event handler. If this happens it
 *	might be necessary to disable (mask) the interrupt depending on the
 *	controller hardware. This requires to reenable the interrupt inside
 *	of the loop which handles the interrupts which have arrived while
 *	the handler was running. If all pending interrupts are handled, the
 *	loop is left.
 */
void
handle_edge_irq(unsigned int irq, struct irq_desc *desc)
{
	raw_spin_lock(&desc->lock);

	desc->status &= ~(IRQ_REPLAY | IRQ_WAITING);

	/*
	 * If we're currently running this IRQ, or its disabled,
	 * we shouldn't process the IRQ. Mark it pending, handle
	 * the necessary masking and go out
	 */
	if (unlikely((desc->istate & (IRQS_INPROGRESS) ||
		      (desc->status & IRQ_DISABLED) || !desc->action))) {
		if (!irq_check_poll(desc)) {
			desc->status |= IRQ_PENDING;
			mask_ack_irq(desc);
			goto out_unlock;
		}
	}
	kstat_incr_irqs_this_cpu(irq, desc);

	/* Start handling the irq */
	desc->irq_data.chip->irq_ack(&desc->irq_data);

	do {
		if (unlikely(!desc->action)) {
			mask_irq(desc);
			goto out_unlock;
		}

		/*
		 * When another irq arrived while we were handling
		 * one, we could have masked the irq.
		 * Renable it, if it was not disabled in meantime.
		 */
		if (unlikely((desc->status &
			       (IRQ_PENDING | IRQ_MASKED | IRQ_DISABLED)) ==
			      (IRQ_PENDING | IRQ_MASKED))) {
			unmask_irq(desc);
		}

		handle_irq_event(desc);

	} while ((desc->status & (IRQ_PENDING | IRQ_DISABLED)) == IRQ_PENDING);

out_unlock:
	raw_spin_unlock(&desc->lock);
}

/**
 *	handle_percpu_irq - Per CPU local irq handler
 *	@irq:	the interrupt number
 *	@desc:	the interrupt description structure for this irq
 *
 *	Per CPU interrupts on SMP machines without locking requirements
 */
void
handle_percpu_irq(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc_get_chip(desc);

	kstat_incr_irqs_this_cpu(irq, desc);

	if (chip->irq_ack)
		chip->irq_ack(&desc->irq_data);

	handle_irq_event_percpu(desc, desc->action);

	if (chip->irq_eoi)
		chip->irq_eoi(&desc->irq_data);
}

void
__set_irq_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install type control for IRQ%d\n", irq);
		return;
	}

	if (!handle)
		handle = handle_bad_irq;
	else if (desc->irq_data.chip == &no_irq_chip) {
		printk(KERN_WARNING "Trying to install %sinterrupt handler "
		       "for IRQ%d\n", is_chained ? "chained " : "", irq);
		/*
		 * Some ARM implementations install a handler for really dumb
		 * interrupt hardware without setting an irq_chip. This worked
		 * with the ARM no_irq_chip but the check in setup_irq would
		 * prevent us to setup the interrupt at all. Switch it to
		 * dummy_irq_chip for easy transition.
		 */
		desc->irq_data.chip = &dummy_irq_chip;
	}

	chip_bus_lock(desc);
	raw_spin_lock_irqsave(&desc->lock, flags);

	/* Uninstall? */
	if (handle == handle_bad_irq) {
		if (desc->irq_data.chip != &no_irq_chip)
			mask_ack_irq(desc);
		desc->status |= IRQ_DISABLED;
		desc->depth = 1;
	}
	desc->handle_irq = handle;
	desc->name = name;

	if (handle != handle_bad_irq && is_chained) {
		desc->status |= IRQ_NOREQUEST | IRQ_NOPROBE;
		irq_startup(desc);
	}
	raw_spin_unlock_irqrestore(&desc->lock, flags);
	chip_bus_sync_unlock(desc);
}
EXPORT_SYMBOL_GPL(__set_irq_handler);

void
set_irq_chip_and_handler(unsigned int irq, struct irq_chip *chip,
			 irq_flow_handler_t handle)
{
	irq_set_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, NULL);
}

void
set_irq_chip_and_handler_name(unsigned int irq, struct irq_chip *chip,
			      irq_flow_handler_t handle, const char *name)
{
	irq_set_chip(irq, chip);
	__set_irq_handler(irq, handle, 0, name);
}

void irq_modify_status(unsigned int irq, unsigned long clr, unsigned long set)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc)
		return;

	/* Sanitize flags */
	set &= IRQF_MODIFY_MASK;
	clr &= IRQF_MODIFY_MASK;

	raw_spin_lock_irqsave(&desc->lock, flags);
	desc->status &= ~clr;
	desc->status |= set;
	raw_spin_unlock_irqrestore(&desc->lock, flags);
}
