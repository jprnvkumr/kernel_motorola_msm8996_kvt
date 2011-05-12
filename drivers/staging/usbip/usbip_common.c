/*
 * Copyright (C) 2003-2008 Takahiro Hirofuchi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <asm/byteorder.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <net/sock.h>

#include "usbip_common.h"

#define DRIVER_AUTHOR "Takahiro Hirofuchi <hirofuchi@users.sourceforge.net>"
#define DRIVER_DESC "USB/IP Common Driver"

#ifdef CONFIG_USB_IP_DEBUG
unsigned long usbip_debug_flag = 0xffffffff;
#else
unsigned long usbip_debug_flag;
#endif
EXPORT_SYMBOL_GPL(usbip_debug_flag);

/* FIXME */
struct device_attribute dev_attr_usbip_debug;
EXPORT_SYMBOL_GPL(dev_attr_usbip_debug);

static ssize_t show_flag(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%lx\n", usbip_debug_flag);
}

static ssize_t store_flag(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	sscanf(buf, "%lx", &usbip_debug_flag);
	return count;
}
DEVICE_ATTR(usbip_debug, (S_IRUGO | S_IWUSR), show_flag, store_flag);

static void usbip_dump_buffer(char *buff, int bufflen)
{
	print_hex_dump(KERN_DEBUG, "usb-ip", DUMP_PREFIX_OFFSET, 16, 4,
		       buff, bufflen, false);
}

static void usbip_dump_pipe(unsigned int p)
{
	unsigned char type = usb_pipetype(p);
	unsigned char ep = usb_pipeendpoint(p);
	unsigned char dev = usb_pipedevice(p);
	unsigned char dir = usb_pipein(p);

	printk(KERN_DEBUG "dev(%d) ", dev);
	printk(KERN_DEBUG "ep(%d) ",  ep);
	printk(KERN_DEBUG "%s ", dir ? "IN" : "OUT");

	switch (type) {
	case PIPE_ISOCHRONOUS:
		printk(KERN_DEBUG "%s ", "ISO");
		break;
	case PIPE_INTERRUPT:
		printk(KERN_DEBUG "%s ", "INT");
		break;
	case PIPE_CONTROL:
		printk(KERN_DEBUG "%s ", "CTL");
		break;
	case PIPE_BULK:
		printk(KERN_DEBUG "%s ", "BLK");
		break;
	default:
		printk(KERN_DEBUG "ERR");
		break;
	}

	printk(KERN_DEBUG "\n");

}

static void usbip_dump_usb_device(struct usb_device *udev)
{
	struct device *dev = &udev->dev;
	int i;

	dev_dbg(dev, "       devnum(%d) devpath(%s)",
		udev->devnum, udev->devpath);

	switch (udev->speed) {
	case USB_SPEED_HIGH:
		printk(KERN_DEBUG " SPD_HIGH");
		break;
	case USB_SPEED_FULL:
		printk(KERN_DEBUG " SPD_FULL");
		break;
	case USB_SPEED_LOW:
		printk(KERN_DEBUG " SPD_LOW");
		break;
	case USB_SPEED_UNKNOWN:
		printk(KERN_DEBUG " SPD_UNKNOWN");
		break;
	default:
		printk(KERN_DEBUG " SPD_ERROR");
		break;
	}

	printk(KERN_DEBUG " tt %p, ttport %d", udev->tt, udev->ttport);
	printk(KERN_DEBUG "\n");

	dev_dbg(dev, "                    ");
	for (i = 0; i < 16; i++)
		printk(KERN_DEBUG " %2u", i);
	printk(KERN_DEBUG "\n");

	dev_dbg(dev, "       toggle0(IN) :");
	for (i = 0; i < 16; i++)
		printk(KERN_DEBUG " %2u", (udev->toggle[0] & (1 << i)) ? 1 : 0);
	printk(KERN_DEBUG "\n");

	dev_dbg(dev, "       toggle1(OUT):");
	for (i = 0; i < 16; i++)
		printk(KERN_DEBUG " %2u", (udev->toggle[1] & (1 << i)) ? 1 : 0);
	printk(KERN_DEBUG "\n");


	dev_dbg(dev, "       epmaxp_in   :");
	for (i = 0; i < 16; i++) {
		if (udev->ep_in[i])
			printk(KERN_DEBUG " %2u",
			     le16_to_cpu(udev->ep_in[i]->desc.wMaxPacketSize));
	}
	printk(KERN_DEBUG "\n");

	dev_dbg(dev, "       epmaxp_out  :");
	for (i = 0; i < 16; i++) {
		if (udev->ep_out[i])
			printk(KERN_DEBUG " %2u",
			     le16_to_cpu(udev->ep_out[i]->desc.wMaxPacketSize));
	}
	printk(KERN_DEBUG "\n");

	dev_dbg(dev, "parent %p, bus %p\n", udev->parent, udev->bus);

	dev_dbg(dev, "descriptor %p, config %p, actconfig %p, "
		"rawdescriptors %p\n", &udev->descriptor, udev->config,
		udev->actconfig, udev->rawdescriptors);

	dev_dbg(dev, "have_langid %d, string_langid %d\n",
		udev->have_langid, udev->string_langid);

	dev_dbg(dev, "maxchild %d, children %p\n",
		udev->maxchild, udev->children);
}

static void usbip_dump_request_type(__u8 rt)
{
	switch (rt & USB_RECIP_MASK) {
	case USB_RECIP_DEVICE:
		printk(KERN_DEBUG "DEVICE");
		break;
	case USB_RECIP_INTERFACE:
		printk(KERN_DEBUG "INTERF");
		break;
	case USB_RECIP_ENDPOINT:
		printk(KERN_DEBUG "ENDPOI");
		break;
	case USB_RECIP_OTHER:
		printk(KERN_DEBUG "OTHER ");
		break;
	default:
		printk(KERN_DEBUG "------");
		break;
	}
}

static void usbip_dump_usb_ctrlrequest(struct usb_ctrlrequest *cmd)
{
	if (!cmd) {
		printk(KERN_DEBUG "      %s : null pointer\n", __func__);
		return;
	}

	printk(KERN_DEBUG "       ");
	printk(KERN_DEBUG "bRequestType(%02X) ", cmd->bRequestType);
	printk(KERN_DEBUG "bRequest(%02X) " , cmd->bRequest);
	printk(KERN_DEBUG "wValue(%04X) ", cmd->wValue);
	printk(KERN_DEBUG "wIndex(%04X) ", cmd->wIndex);
	printk(KERN_DEBUG "wLength(%04X) ", cmd->wLength);

	printk(KERN_DEBUG "\n       ");

	if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_STANDARD) {
		printk(KERN_DEBUG "STANDARD ");
		switch (cmd->bRequest) {
		case USB_REQ_GET_STATUS:
			printk(KERN_DEBUG "GET_STATUS");
			break;
		case USB_REQ_CLEAR_FEATURE:
			printk(KERN_DEBUG "CLEAR_FEAT");
			break;
		case USB_REQ_SET_FEATURE:
			printk(KERN_DEBUG "SET_FEAT  ");
			break;
		case USB_REQ_SET_ADDRESS:
			printk(KERN_DEBUG "SET_ADDRRS");
			break;
		case USB_REQ_GET_DESCRIPTOR:
			printk(KERN_DEBUG "GET_DESCRI");
			break;
		case USB_REQ_SET_DESCRIPTOR:
			printk(KERN_DEBUG "SET_DESCRI");
			break;
		case USB_REQ_GET_CONFIGURATION:
			printk(KERN_DEBUG "GET_CONFIG");
			break;
		case USB_REQ_SET_CONFIGURATION:
			printk(KERN_DEBUG "SET_CONFIG");
			break;
		case USB_REQ_GET_INTERFACE:
			printk(KERN_DEBUG "GET_INTERF");
			break;
		case USB_REQ_SET_INTERFACE:
			printk(KERN_DEBUG "SET_INTERF");
			break;
		case USB_REQ_SYNCH_FRAME:
			printk(KERN_DEBUG "SYNC_FRAME");
			break;
		default:
			printk(KERN_DEBUG "REQ(%02X) ", cmd->bRequest);
			break;
		}

		printk(KERN_DEBUG " ");
		usbip_dump_request_type(cmd->bRequestType);

	} else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS)
		printk(KERN_DEBUG "CLASS   ");

	else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR)
		printk(KERN_DEBUG "VENDOR  ");

	else if ((cmd->bRequestType & USB_TYPE_MASK) == USB_TYPE_RESERVED)
		printk(KERN_DEBUG "RESERVED");

	printk(KERN_DEBUG "\n");
}

void usbip_dump_urb(struct urb *urb)
{
	struct device *dev;

	if (!urb) {
		printk(KERN_DEBUG KBUILD_MODNAME
		       ":%s: urb: null pointer!!\n", __func__);
		return;
	}

	if (!urb->dev) {
		printk(KERN_DEBUG KBUILD_MODNAME
		       ":%s: urb->dev: null pointer!!\n", __func__);
		return;
	}
	dev = &urb->dev->dev;

	dev_dbg(dev, "   urb                   :%p\n", urb);
	dev_dbg(dev, "   dev                   :%p\n", urb->dev);

	usbip_dump_usb_device(urb->dev);

	dev_dbg(dev, "   pipe                  :%08x ", urb->pipe);

	usbip_dump_pipe(urb->pipe);

	dev_dbg(dev, "   status                :%d\n", urb->status);
	dev_dbg(dev, "   transfer_flags        :%08X\n", urb->transfer_flags);
	dev_dbg(dev, "   transfer_buffer       :%p\n", urb->transfer_buffer);
	dev_dbg(dev, "   transfer_buffer_length:%d\n",
						urb->transfer_buffer_length);
	dev_dbg(dev, "   actual_length         :%d\n", urb->actual_length);
	dev_dbg(dev, "   setup_packet          :%p\n", urb->setup_packet);

	if (urb->setup_packet && usb_pipetype(urb->pipe) == PIPE_CONTROL)
		usbip_dump_usb_ctrlrequest(
			(struct usb_ctrlrequest *)urb->setup_packet);

	dev_dbg(dev, "   start_frame           :%d\n", urb->start_frame);
	dev_dbg(dev, "   number_of_packets     :%d\n", urb->number_of_packets);
	dev_dbg(dev, "   interval              :%d\n", urb->interval);
	dev_dbg(dev, "   error_count           :%d\n", urb->error_count);
	dev_dbg(dev, "   context               :%p\n", urb->context);
	dev_dbg(dev, "   complete              :%p\n", urb->complete);
}
EXPORT_SYMBOL_GPL(usbip_dump_urb);

void usbip_dump_header(struct usbip_header *pdu)
{
	usbip_udbg("BASE: cmd %u seq %u devid %u dir %u ep %u\n",
		   pdu->base.command,
		   pdu->base.seqnum,
		   pdu->base.devid,
		   pdu->base.direction,
		   pdu->base.ep);

	switch (pdu->base.command) {
	case USBIP_CMD_SUBMIT:
		usbip_udbg("CMD_SUBMIT: "
			   "x_flags %u x_len %u sf %u #p %u iv %u\n",
			   pdu->u.cmd_submit.transfer_flags,
			   pdu->u.cmd_submit.transfer_buffer_length,
			   pdu->u.cmd_submit.start_frame,
			   pdu->u.cmd_submit.number_of_packets,
			   pdu->u.cmd_submit.interval);
		break;
	case USBIP_CMD_UNLINK:
		usbip_udbg("CMD_UNLINK: seq %u\n", pdu->u.cmd_unlink.seqnum);
		break;
	case USBIP_RET_SUBMIT:
		usbip_udbg("RET_SUBMIT: st %d al %u sf %d #p %d ec %d\n",
			   pdu->u.ret_submit.status,
			   pdu->u.ret_submit.actual_length,
			   pdu->u.ret_submit.start_frame,
			   pdu->u.ret_submit.number_of_packets,
			   pdu->u.ret_submit.error_count);
	case USBIP_RET_UNLINK:
		usbip_udbg("RET_UNLINK: status %d\n", pdu->u.ret_unlink.status);
		break;
	default:
		/* NOT REACHED */
		usbip_udbg("unknown command\n");
		break;
	}
}
EXPORT_SYMBOL_GPL(usbip_dump_header);

/* Send/receive messages over TCP/IP. I refer drivers/block/nbd.c */
int usbip_xmit(int send, struct socket *sock, char *buf,
	       int size, int msg_flags)
{
	int result;
	struct msghdr msg;
	struct kvec iov;
	int total = 0;

	/* for blocks of if (usbip_dbg_flag_xmit) */
	char *bp = buf;
	int osize = size;

	usbip_dbg_xmit("enter\n");

	if (!sock || !buf || !size) {
		printk(KERN_ERR "%s: invalid arg, sock %p buff %p size %d\n",
		       __func__, sock, buf, size);
		return -EINVAL;
	}

	if (usbip_dbg_flag_xmit) {
		if (send) {
			if (!in_interrupt())
				printk(KERN_DEBUG "%-10s:", current->comm);
			else
				printk(KERN_DEBUG "interrupt  :");

			printk(KERN_DEBUG "%s: sending... , sock %p, buf %p, "
			       "size %d, msg_flags %d\n", __func__,
			       sock, buf, size, msg_flags);
			usbip_dump_buffer(buf, size);
		}
	}

	do {
		sock->sk->sk_allocation = GFP_NOIO;
		iov.iov_base    = buf;
		iov.iov_len     = size;
		msg.msg_name    = NULL;
		msg.msg_namelen = 0;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_namelen    = 0;
		msg.msg_flags      = msg_flags | MSG_NOSIGNAL;

		if (send)
			result = kernel_sendmsg(sock, &msg, &iov, 1, size);
		else
			result = kernel_recvmsg(sock, &msg, &iov, 1, size,
						MSG_WAITALL);

		if (result <= 0) {
			usbip_udbg("usbip_xmit: %s sock %p buf %p size %u ret "
				   "%d total %d\n", send ? "send" : "receive",
				   sock, buf, size, result, total);
			goto err;
		}

		size -= result;
		buf += result;
		total += result;

	} while (size > 0);

	if (usbip_dbg_flag_xmit) {
		if (!send) {
			if (!in_interrupt())
				printk(KERN_DEBUG "%-10s:", current->comm);
			else
				printk(KERN_DEBUG "interrupt  :");

			printk(KERN_DEBUG "usbip_xmit: receiving....\n");
			usbip_dump_buffer(bp, osize);
			printk(KERN_DEBUG "usbip_xmit: received, osize %d ret "
			       "%d size %d total %d\n", osize, result, size,
			       total);
		}

		if (send)
			printk(KERN_DEBUG "usbip_xmit: send, total %d\n",
			       total);
	}

	return total;

err:
	return result;
}
EXPORT_SYMBOL_GPL(usbip_xmit);

struct socket *sockfd_to_socket(unsigned int sockfd)
{
	struct socket *socket;
	struct file *file;
	struct inode *inode;

	file = fget(sockfd);
	if (!file) {
		printk(KERN_ERR "%s: invalid sockfd\n", __func__);
		return NULL;
	}

	inode = file->f_dentry->d_inode;

	if (!inode || !S_ISSOCK(inode->i_mode))
		return NULL;

	socket = SOCKET_I(inode);

	return socket;
}
EXPORT_SYMBOL_GPL(sockfd_to_socket);

/* there may be more cases to tweak the flags. */
static unsigned int tweak_transfer_flags(unsigned int flags)
{
	flags &= ~URB_NO_TRANSFER_DMA_MAP;
	return flags;
}

static void usbip_pack_cmd_submit(struct usbip_header *pdu, struct urb *urb,
				  int pack)
{
	struct usbip_header_cmd_submit *spdu = &pdu->u.cmd_submit;

	/*
	 * Some members are not still implemented in usbip. I hope this issue
	 * will be discussed when usbip is ported to other operating systems.
	 */
	if (pack) {
		/* vhci_tx.c */
		spdu->transfer_flags =
			tweak_transfer_flags(urb->transfer_flags);
		spdu->transfer_buffer_length	= urb->transfer_buffer_length;
		spdu->start_frame		= urb->start_frame;
		spdu->number_of_packets		= urb->number_of_packets;
		spdu->interval			= urb->interval;
	} else  {
		/* stub_rx.c */
		urb->transfer_flags         = spdu->transfer_flags;

		urb->transfer_buffer_length = spdu->transfer_buffer_length;
		urb->start_frame            = spdu->start_frame;
		urb->number_of_packets      = spdu->number_of_packets;
		urb->interval               = spdu->interval;
	}
}

static void usbip_pack_ret_submit(struct usbip_header *pdu, struct urb *urb,
				  int pack)
{
	struct usbip_header_ret_submit *rpdu = &pdu->u.ret_submit;

	if (pack) {
		/* stub_tx.c */

		rpdu->status		= urb->status;
		rpdu->actual_length	= urb->actual_length;
		rpdu->start_frame	= urb->start_frame;
		rpdu->number_of_packets = urb->number_of_packets;
		rpdu->error_count	= urb->error_count;
	} else {
		/* vhci_rx.c */

		urb->status		= rpdu->status;
		urb->actual_length	= rpdu->actual_length;
		urb->start_frame	= rpdu->start_frame;
		urb->number_of_packets = rpdu->number_of_packets;
		urb->error_count	= rpdu->error_count;
	}
}

void usbip_pack_pdu(struct usbip_header *pdu, struct urb *urb, int cmd,
		    int pack)
{
	switch (cmd) {
	case USBIP_CMD_SUBMIT:
		usbip_pack_cmd_submit(pdu, urb, pack);
		break;
	case USBIP_RET_SUBMIT:
		usbip_pack_ret_submit(pdu, urb, pack);
		break;
	default:
		/* NOT REACHED */
		err("unknown command");
		/* BUG(); */
		break;
	}
}
EXPORT_SYMBOL_GPL(usbip_pack_pdu);

static void correct_endian_basic(struct usbip_header_basic *base, int send)
{
	if (send) {
		base->command	= cpu_to_be32(base->command);
		base->seqnum	= cpu_to_be32(base->seqnum);
		base->devid	= cpu_to_be32(base->devid);
		base->direction	= cpu_to_be32(base->direction);
		base->ep	= cpu_to_be32(base->ep);
	} else {
		base->command	= be32_to_cpu(base->command);
		base->seqnum	= be32_to_cpu(base->seqnum);
		base->devid	= be32_to_cpu(base->devid);
		base->direction	= be32_to_cpu(base->direction);
		base->ep	= be32_to_cpu(base->ep);
	}
}

static void correct_endian_cmd_submit(struct usbip_header_cmd_submit *pdu,
				      int send)
{
	if (send) {
		pdu->transfer_flags = cpu_to_be32(pdu->transfer_flags);

		cpu_to_be32s(&pdu->transfer_buffer_length);
		cpu_to_be32s(&pdu->start_frame);
		cpu_to_be32s(&pdu->number_of_packets);
		cpu_to_be32s(&pdu->interval);
	} else {
		pdu->transfer_flags = be32_to_cpu(pdu->transfer_flags);

		be32_to_cpus(&pdu->transfer_buffer_length);
		be32_to_cpus(&pdu->start_frame);
		be32_to_cpus(&pdu->number_of_packets);
		be32_to_cpus(&pdu->interval);
	}
}

static void correct_endian_ret_submit(struct usbip_header_ret_submit *pdu,
				      int send)
{
	if (send) {
		cpu_to_be32s(&pdu->status);
		cpu_to_be32s(&pdu->actual_length);
		cpu_to_be32s(&pdu->start_frame);
		cpu_to_be32s(&pdu->number_of_packets);
		cpu_to_be32s(&pdu->error_count);
	} else {
		be32_to_cpus(&pdu->status);
		be32_to_cpus(&pdu->actual_length);
		be32_to_cpus(&pdu->start_frame);
		cpu_to_be32s(&pdu->number_of_packets);
		be32_to_cpus(&pdu->error_count);
	}
}

static void correct_endian_cmd_unlink(struct usbip_header_cmd_unlink *pdu,
				      int send)
{
	if (send)
		pdu->seqnum = cpu_to_be32(pdu->seqnum);
	else
		pdu->seqnum = be32_to_cpu(pdu->seqnum);
}

static void correct_endian_ret_unlink(struct usbip_header_ret_unlink *pdu,
				      int send)
{
	if (send)
		cpu_to_be32s(&pdu->status);
	else
		be32_to_cpus(&pdu->status);
}

void usbip_header_correct_endian(struct usbip_header *pdu, int send)
{
	__u32 cmd = 0;

	if (send)
		cmd = pdu->base.command;

	correct_endian_basic(&pdu->base, send);

	if (!send)
		cmd = pdu->base.command;

	switch (cmd) {
	case USBIP_CMD_SUBMIT:
		correct_endian_cmd_submit(&pdu->u.cmd_submit, send);
		break;
	case USBIP_RET_SUBMIT:
		correct_endian_ret_submit(&pdu->u.ret_submit, send);
		break;
	case USBIP_CMD_UNLINK:
		correct_endian_cmd_unlink(&pdu->u.cmd_unlink, send);
		break;
	case USBIP_RET_UNLINK:
		correct_endian_ret_unlink(&pdu->u.ret_unlink, send);
		break;
	default:
		/* NOT REACHED */
		err("unknown command in pdu header: %d", cmd);
		/* BUG(); */
		break;
	}
}
EXPORT_SYMBOL_GPL(usbip_header_correct_endian);

static void usbip_iso_pakcet_correct_endian(
	struct usbip_iso_packet_descriptor *iso,
	int send)
{
	/* does not need all members. but copy all simply. */
	if (send) {
		iso->offset	= cpu_to_be32(iso->offset);
		iso->length	= cpu_to_be32(iso->length);
		iso->status	= cpu_to_be32(iso->status);
		iso->actual_length = cpu_to_be32(iso->actual_length);
	} else {
		iso->offset	= be32_to_cpu(iso->offset);
		iso->length	= be32_to_cpu(iso->length);
		iso->status	= be32_to_cpu(iso->status);
		iso->actual_length = be32_to_cpu(iso->actual_length);
	}
}

static void usbip_pack_iso(struct usbip_iso_packet_descriptor *iso,
			   struct usb_iso_packet_descriptor *uiso, int pack)
{
	if (pack) {
		iso->offset		= uiso->offset;
		iso->length		= uiso->length;
		iso->status		= uiso->status;
		iso->actual_length	= uiso->actual_length;
	} else {
		uiso->offset		= iso->offset;
		uiso->length		= iso->length;
		uiso->status		= iso->status;
		uiso->actual_length	= iso->actual_length;
	}
}

/* must free buffer */
void *usbip_alloc_iso_desc_pdu(struct urb *urb, ssize_t *bufflen)
{
	void *buff;
	struct usbip_iso_packet_descriptor *iso;
	int np = urb->number_of_packets;
	ssize_t size = np * sizeof(*iso);
	int i;

	buff = kzalloc(size, GFP_KERNEL);
	if (!buff)
		return NULL;

	for (i = 0; i < np; i++) {
		iso = buff + (i * sizeof(*iso));

		usbip_pack_iso(iso, &urb->iso_frame_desc[i], 1);
		usbip_iso_pakcet_correct_endian(iso, 1);
	}

	*bufflen = size;

	return buff;
}
EXPORT_SYMBOL_GPL(usbip_alloc_iso_desc_pdu);

/* some members of urb must be substituted before. */
int usbip_recv_iso(struct usbip_device *ud, struct urb *urb)
{
	void *buff;
	struct usbip_iso_packet_descriptor *iso;
	int np = urb->number_of_packets;
	int size = np * sizeof(*iso);
	int i;
	int ret;
	int total_length = 0;

	if (!usb_pipeisoc(urb->pipe))
		return 0;

	/* my Bluetooth dongle gets ISO URBs which are np = 0 */
	if (np == 0) {
		/* usbip_uinfo("iso np == 0\n"); */
		/* usbip_dump_urb(urb); */
		return 0;
	}

	buff = kzalloc(size, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	ret = usbip_xmit(0, ud->tcp_socket, buff, size, 0);
	if (ret != size) {
		dev_err(&urb->dev->dev, "recv iso_frame_descriptor, %d\n",
			ret);
		kfree(buff);

		if (ud->side == USBIP_STUB)
			usbip_event_add(ud, SDEV_EVENT_ERROR_TCP);
		else
			usbip_event_add(ud, VDEV_EVENT_ERROR_TCP);

		return -EPIPE;
	}

	for (i = 0; i < np; i++) {
		iso = buff + (i * sizeof(*iso));

		usbip_iso_pakcet_correct_endian(iso, 0);
		usbip_pack_iso(iso, &urb->iso_frame_desc[i], 0);
		total_length += urb->iso_frame_desc[i].actual_length;
	}

	kfree(buff);

	if (total_length != urb->actual_length) {
		dev_err(&urb->dev->dev,
			"total length of iso packets %d not equal to actual "
			"length of buffer %d\n",
			total_length, urb->actual_length);

		if (ud->side == USBIP_STUB)
			usbip_event_add(ud, SDEV_EVENT_ERROR_TCP);
		else
			usbip_event_add(ud, VDEV_EVENT_ERROR_TCP);

		return -EPIPE;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(usbip_recv_iso);

/*
 * This functions restores the padding which was removed for optimizing
 * the bandwidth during transfer over tcp/ip
 *
 * buffer and iso packets need to be stored and be in propeper endian in urb
 * before calling this function
 */
int usbip_pad_iso(struct usbip_device *ud, struct urb *urb)
{
	int np = urb->number_of_packets;
	int i;
	int ret;
	int actualoffset = urb->actual_length;

	if (!usb_pipeisoc(urb->pipe))
		return 0;

	/* if no packets or length of data is 0, then nothing to unpack */
	if (np == 0 || urb->actual_length == 0)
		return 0;

	/*
	 * if actual_length is transfer_buffer_length then no padding is
	 * present.
	*/
	if (urb->actual_length == urb->transfer_buffer_length)
		return 0;

	/*
	 * loop over all packets from last to first (to prevent overwritting
	 * memory when padding) and move them into the proper place
	 */
	for (i = np-1; i > 0; i--) {
		actualoffset -= urb->iso_frame_desc[i].actual_length;
		memmove(urb->transfer_buffer + urb->iso_frame_desc[i].offset,
			urb->transfer_buffer + actualoffset,
			urb->iso_frame_desc[i].actual_length);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(usbip_pad_iso);

/* some members of urb must be substituted before. */
int usbip_recv_xbuff(struct usbip_device *ud, struct urb *urb)
{
	int ret;
	int size;

	if (ud->side == USBIP_STUB) {
		/* stub_rx.c */
		/* the direction of urb must be OUT. */
		if (usb_pipein(urb->pipe))
			return 0;

		size = urb->transfer_buffer_length;
	} else {
		/* vhci_rx.c */
		/* the direction of urb must be IN. */
		if (usb_pipeout(urb->pipe))
			return 0;

		size = urb->actual_length;
	}

	/* no need to recv xbuff */
	if (!(size > 0))
		return 0;

	ret = usbip_xmit(0, ud->tcp_socket, (char *)urb->transfer_buffer,
			 size, 0);
	if (ret != size) {
		dev_err(&urb->dev->dev, "recv xbuf, %d\n", ret);
		if (ud->side == USBIP_STUB) {
			usbip_event_add(ud, SDEV_EVENT_ERROR_TCP);
		} else {
			usbip_event_add(ud, VDEV_EVENT_ERROR_TCP);
			return -EPIPE;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(usbip_recv_xbuff);

static int __init usbip_common_init(void)
{
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_DESC " " USBIP_VERSION
	       "\n");
	return 0;
}

static void __exit usbip_common_exit(void)
{
	return;
}

module_init(usbip_common_init);
module_exit(usbip_common_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(USBIP_VERSION);
