/* K8 NUMA support */
/* Copyright 2002,2003 by Andi Kleen, SuSE Labs */
/* 2.5 Version loosely based on the NUMAQ Code by Pat Gaughen. */
#ifndef _ASM_X86_MMZONE_64_H
#define _ASM_X86_MMZONE_64_H

#ifdef CONFIG_NUMA

#include <linux/mmdebug.h>
#include <asm/smp.h>

extern struct pglist_data *node_data[];

#define NODE_DATA(nid)		(node_data[nid])

#define node_start_pfn(nid)	(NODE_DATA(nid)->node_start_pfn)
#define node_end_pfn(nid)       (NODE_DATA(nid)->node_start_pfn +	\
				 NODE_DATA(nid)->node_spanned_pages)
#endif
#endif /* _ASM_X86_MMZONE_64_H */
