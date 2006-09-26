#ifndef __ARCH_DESC_H
#define __ARCH_DESC_H

#include <asm/ldt.h>
#include <asm/segment.h>

#define CPU_16BIT_STACK_SIZE 1024

#ifndef __ASSEMBLY__

#include <linux/preempt.h>
#include <linux/smp.h>
#include <linux/percpu.h>

#include <asm/mmu.h>

extern struct desc_struct cpu_gdt_table[GDT_ENTRIES];

DECLARE_PER_CPU(unsigned char, cpu_16bit_stack[CPU_16BIT_STACK_SIZE]);

struct Xgt_desc_struct {
	unsigned short size;
	unsigned long address __attribute__((packed));
	unsigned short pad;
} __attribute__ ((packed));

extern struct Xgt_desc_struct idt_descr;
DECLARE_PER_CPU(struct Xgt_desc_struct, cpu_gdt_descr);


static inline struct desc_struct *get_cpu_gdt_table(unsigned int cpu)
{
	return (struct desc_struct *)per_cpu(cpu_gdt_descr, cpu).address;
}

/*
 * This is the ldt that every process will get unless we need
 * something other than this.
 */
extern struct desc_struct default_ldt[];
extern struct desc_struct idt_table[];
extern void set_intr_gate(unsigned int irq, void * addr);

static inline void pack_descriptor(__u32 *a, __u32 *b,
	unsigned long base, unsigned long limit, unsigned char type, unsigned char flags)
{
	*a = ((base & 0xffff) << 16) | (limit & 0xffff);
	*b = (base & 0xff000000) | ((base & 0xff0000) >> 16) |
		(limit & 0x000f0000) | ((type & 0xff) << 8) | ((flags & 0xf) << 20);
}

static inline void pack_gate(__u32 *a, __u32 *b,
	unsigned long base, unsigned short seg, unsigned char type, unsigned char flags)
{
	*a = (seg << 16) | (base & 0xffff);
	*b = (base & 0xffff0000) | ((type & 0xff) << 8) | (flags & 0xff);
}

#define DESCTYPE_LDT 	0x82	/* present, system, DPL-0, LDT */
#define DESCTYPE_TSS 	0x89	/* present, system, DPL-0, 32-bit TSS */
#define DESCTYPE_TASK	0x85	/* present, system, DPL-0, task gate */
#define DESCTYPE_INT	0x8e	/* present, system, DPL-0, interrupt gate */
#define DESCTYPE_TRAP	0x8f	/* present, system, DPL-0, trap gate */
#define DESCTYPE_DPL3	0x60	/* DPL-3 */
#define DESCTYPE_S	0x10	/* !system */

#define load_TR_desc() __asm__ __volatile__("ltr %w0"::"q" (GDT_ENTRY_TSS*8))
#define load_LDT_desc() __asm__ __volatile__("lldt %w0"::"q" (GDT_ENTRY_LDT*8))

#define load_gdt(dtr) __asm__ __volatile("lgdt %0"::"m" (*dtr))
#define load_idt(dtr) __asm__ __volatile("lidt %0"::"m" (*dtr))
#define load_tr(tr) __asm__ __volatile("ltr %0"::"m" (tr))
#define load_ldt(ldt) __asm__ __volatile("lldt %0"::"m" (ldt))

#define store_gdt(dtr) __asm__ ("sgdt %0":"=m" (*dtr))
#define store_idt(dtr) __asm__ ("sidt %0":"=m" (*dtr))
#define store_tr(tr) __asm__ ("str %0":"=m" (tr))
#define store_ldt(ldt) __asm__ ("sldt %0":"=m" (ldt))

#if TLS_SIZE != 24
# error update this code.
#endif

static inline void load_TLS(struct thread_struct *t, unsigned int cpu)
{
#define C(i) get_cpu_gdt_table(cpu)[GDT_ENTRY_TLS_MIN + i] = t->tls_array[i]
	C(0); C(1); C(2);
#undef C
}

static inline void write_dt_entry(void *dt, int entry, __u32 entry_a, __u32 entry_b)
{
	__u32 *lp = (__u32 *)((char *)dt + entry*8);
	*lp = entry_a;
	*(lp+1) = entry_b;
}

#define write_ldt_entry(dt, entry, a, b) write_dt_entry(dt, entry, a, b)
#define write_gdt_entry(dt, entry, a, b) write_dt_entry(dt, entry, a, b)
#define write_idt_entry(dt, entry, a, b) write_dt_entry(dt, entry, a, b)

static inline void _set_gate(int gate, unsigned int type, void *addr, unsigned short seg)
{
	__u32 a, b;
	pack_gate(&a, &b, (unsigned long)addr, seg, type, 0);
	write_idt_entry(idt_table, gate, a, b);
}

static inline void __set_tss_desc(unsigned int cpu, unsigned int entry, const void *addr)
{
	__u32 a, b;
	pack_descriptor(&a, &b, (unsigned long)addr,
			offsetof(struct tss_struct, __cacheline_filler) - 1,
			DESCTYPE_TSS, 0);
	write_gdt_entry(get_cpu_gdt_table(cpu), entry, a, b);
}

static inline void set_ldt_desc(unsigned int cpu, void *addr, unsigned int entries)
{
	__u32 a, b;
	pack_descriptor(&a, &b, (unsigned long)addr,
			entries * sizeof(struct desc_struct) - 1,
			DESCTYPE_LDT, 0);
	write_gdt_entry(get_cpu_gdt_table(cpu), GDT_ENTRY_LDT, a, b);
}

#define set_tss_desc(cpu,addr) __set_tss_desc(cpu, GDT_ENTRY_TSS, addr)

#define LDT_entry_a(info) \
	((((info)->base_addr & 0x0000ffff) << 16) | ((info)->limit & 0x0ffff))

#define LDT_entry_b(info) \
	(((info)->base_addr & 0xff000000) | \
	(((info)->base_addr & 0x00ff0000) >> 16) | \
	((info)->limit & 0xf0000) | \
	(((info)->read_exec_only ^ 1) << 9) | \
	((info)->contents << 10) | \
	(((info)->seg_not_present ^ 1) << 15) | \
	((info)->seg_32bit << 22) | \
	((info)->limit_in_pages << 23) | \
	((info)->useable << 20) | \
	0x7000)

#define LDT_empty(info) (\
	(info)->base_addr	== 0	&& \
	(info)->limit		== 0	&& \
	(info)->contents	== 0	&& \
	(info)->read_exec_only	== 1	&& \
	(info)->seg_32bit	== 0	&& \
	(info)->limit_in_pages	== 0	&& \
	(info)->seg_not_present	== 1	&& \
	(info)->useable		== 0	)

static inline void clear_LDT(void)
{
	int cpu = get_cpu();

	set_ldt_desc(cpu, &default_ldt[0], 5);
	load_LDT_desc();
	put_cpu();
}

/*
 * load one particular LDT into the current CPU
 */
static inline void load_LDT_nolock(mm_context_t *pc, int cpu)
{
	void *segments = pc->ldt;
	int count = pc->size;

	if (likely(!count)) {
		segments = &default_ldt[0];
		count = 5;
	}
		
	set_ldt_desc(cpu, segments, count);
	load_LDT_desc();
}

static inline void load_LDT(mm_context_t *pc)
{
	int cpu = get_cpu();
	load_LDT_nolock(pc, cpu);
	put_cpu();
}

static inline unsigned long get_desc_base(unsigned long *desc)
{
	unsigned long base;
	base = ((desc[0] >> 16)  & 0x0000ffff) |
		((desc[1] << 16) & 0x00ff0000) |
		(desc[1] & 0xff000000);
	return base;
}

#endif /* !__ASSEMBLY__ */

#endif
