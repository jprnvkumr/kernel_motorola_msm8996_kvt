/*
    comedi/drivers/ke_counter.c
    Comedi driver for Kolter-Electronic PCI Counter 1 Card

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2000 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
/*
Driver: ke_counter
Description: Driver for Kolter Electronic Counter Card
Devices: [Kolter Electronic] PCI Counter Card (ke_counter)
Author: Michael Hillmann
Updated: Mon, 14 Apr 2008 15:42:42 +0100
Status: tested

Configuration Options: not applicable, uses PCI auto config

This driver is a simple driver to read the counter values from
Kolter Electronic PCI Counter Card.
*/

#include <linux/module.h>
#include <linux/pci.h>

#include "../comedidev.h"

/*
 * PCI BAR 0 Register I/O map
 */
#define KE_RESET_REG(x)			(0x00 + ((x) * 0x20))
#define KE_LATCH_REG(x)			(0x00 + ((x) * 0x20))
#define KE_LSB_REG(x)			(0x04 + ((x) * 0x20))
#define KE_MID_REG(x)			(0x08 + ((x) * 0x20))
#define KE_MSB_REG(x)			(0x0c + ((x) * 0x20))
#define KE_SIGN_REG(x)			(0x10 + ((x) * 0x20))
#define KE_OSC_SEL_REG			0xf8
#define KE_OSC_SEL_EXT			(1 << 0)
#define KE_OSC_SEL_4MHZ			(2 << 0)
#define KE_OSC_SEL_20MHZ		(3 << 0)
#define KE_DO_REG			0xfc

/*-- counter write ----------------------------------------------------------*/

/* This should be used only for resetting the counters; maybe it is better
   to make a special command 'reset'. */
static int cnt_winsn(struct comedi_device *dev,
		     struct comedi_subdevice *s, struct comedi_insn *insn,
		     unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);

	outb((unsigned char)((data[0] >> 24) & 0xff),
	     dev->iobase + KE_SIGN_REG(chan));
	outb((unsigned char)((data[0] >> 16) & 0xff),
	     dev->iobase + KE_MSB_REG(chan));
	outb((unsigned char)((data[0] >> 8) & 0xff),
	     dev->iobase + KE_MID_REG(chan));
	outb((unsigned char)((data[0] >> 0) & 0xff),
	     dev->iobase + KE_LSB_REG(chan));

	/* return the number of samples written */
	return 1;
}

/*-- counter read -----------------------------------------------------------*/

static int cnt_rinsn(struct comedi_device *dev,
		     struct comedi_subdevice *s, struct comedi_insn *insn,
		     unsigned int *data)
{
	unsigned char a0, a1, a2, a3, a4;
	int chan = CR_CHAN(insn->chanspec);
	int result;

	a0 = inb(dev->iobase + KE_LATCH_REG(chan));
	a1 = inb(dev->iobase + KE_LSB_REG(chan));
	a2 = inb(dev->iobase + KE_MID_REG(chan));
	a3 = inb(dev->iobase + KE_MSB_REG(chan));
	a4 = inb(dev->iobase + KE_SIGN_REG(chan));

	result = (a1 + (a2 * 256) + (a3 * 65536));
	if (a4 > 0)
		result = result - s->maxdata;

	*data = (unsigned int)result;

	/* return the number of samples read */
	return 1;
}

static int cnt_auto_attach(struct comedi_device *dev,
				     unsigned long context_unused)
{
	struct pci_dev *pcidev = comedi_to_pci_dev(dev);
	struct comedi_subdevice *s;
	int ret;

	ret = comedi_pci_enable(dev);
	if (ret)
		return ret;
	dev->iobase = pci_resource_start(pcidev, 0);

	ret = comedi_alloc_subdevices(dev, 1);
	if (ret)
		return ret;

	s = &dev->subdevices[0];
	dev->read_subdev = s;

	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_READABLE /* | SDF_COMMON */ ;
	s->n_chan = 3;
	s->maxdata = 0x00ffffff;
	s->insn_read = cnt_rinsn;
	s->insn_write = cnt_winsn;

	outb(KE_OSC_SEL_20MHZ, dev->iobase + KE_OSC_SEL_REG);

	outb(0, dev->iobase + KE_RESET_REG(0));
	outb(0, dev->iobase + KE_RESET_REG(1));
	outb(0, dev->iobase + KE_RESET_REG(2));

	return 0;
}

static struct comedi_driver ke_counter_driver = {
	.driver_name	= "ke_counter",
	.module		= THIS_MODULE,
	.auto_attach	= cnt_auto_attach,
	.detach		= comedi_pci_disable,
};

static int ke_counter_pci_probe(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	return comedi_pci_auto_config(dev, &ke_counter_driver,
				      id->driver_data);
}

static const struct pci_device_id ke_counter_pci_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_KOLTER, 0x0014) },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, ke_counter_pci_table);

static struct pci_driver ke_counter_pci_driver = {
	.name		= "ke_counter",
	.id_table	= ke_counter_pci_table,
	.probe		= ke_counter_pci_probe,
	.remove		= comedi_pci_auto_unconfig,
};
module_comedi_pci_driver(ke_counter_driver, ke_counter_pci_driver);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
