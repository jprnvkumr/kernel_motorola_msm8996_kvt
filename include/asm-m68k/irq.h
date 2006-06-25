#ifndef _M68K_IRQ_H_
#define _M68K_IRQ_H_

#include <linux/hardirq.h>
#include <linux/spinlock_types.h>

/*
 * # of m68k auto vector interrupts
 */

#define SYS_IRQS 8

/*
 * This should be the same as the max(NUM_X_SOURCES) for all the
 * different m68k hosts compiled into the kernel.
 * Currently the Atari has 72 and the Amiga 24, but if both are
 * supported in the kernel it is better to make room for 72.
 */
#if defined(CONFIG_ATARI) || defined(CONFIG_MAC)
#define NR_IRQS (72+SYS_IRQS)
#else
#define NR_IRQS (24+SYS_IRQS)
#endif

/*
 * The hardirq mask has to be large enough to have
 * space for potentially all IRQ sources in the system
 * nesting on a single CPU:
 */
#if (1 << HARDIRQ_BITS) < NR_IRQS
# error HARDIRQ_BITS is too low!
#endif

/*
 * Interrupt source definitions
 * General interrupt sources are the level 1-7.
 * Adding an interrupt service routine for one of these sources
 * results in the addition of that routine to a chain of routines.
 * Each one is called in succession.  Each individual interrupt
 * service routine should determine if the device associated with
 * that routine requires service.
 */

#define IRQ_SPURIOUS	0

#define IRQ_AUTO_1	1	/* level 1 interrupt */
#define IRQ_AUTO_2	2	/* level 2 interrupt */
#define IRQ_AUTO_3	3	/* level 3 interrupt */
#define IRQ_AUTO_4	4	/* level 4 interrupt */
#define IRQ_AUTO_5	5	/* level 5 interrupt */
#define IRQ_AUTO_6	6	/* level 6 interrupt */
#define IRQ_AUTO_7	7	/* level 7 interrupt (non-maskable) */

#define IRQ_USER	8

static __inline__ int irq_canonicalize(int irq)
{
	return irq;
}

/*
 * Machine specific interrupt sources.
 *
 * Adding an interrupt service routine for a source with this bit
 * set indicates a special machine specific interrupt source.
 * The machine specific files define these sources.
 *
 * The IRQ_MACHSPEC bit is now gone - the only thing it did was to
 * introduce unnecessary overhead.
 *
 * All interrupt handling is actually machine specific so it is better
 * to use function pointers, as used by the Sparc port, and select the
 * interrupt handling functions when initializing the kernel. This way
 * we save some unnecessary overhead at run-time.
 *                                                      01/11/97 - Jes
 */

extern void (*enable_irq)(unsigned int);
extern void (*disable_irq)(unsigned int);
#define disable_irq_nosync	disable_irq

struct pt_regs;

extern int cpu_request_irq(unsigned int,
			   int (*)(int, void *, struct pt_regs *),
			   unsigned long, const char *, void *);
extern void cpu_free_irq(unsigned int, void *);

/*
 * various flags for request_irq() - the Amiga now uses the standard
 * mechanism like all other architectures - SA_INTERRUPT and SA_SHIRQ
 * are your friends.
 */
#ifndef MACH_AMIGA_ONLY
#define IRQ_FLG_LOCK	(0x0001)	/* handler is not replaceable	*/
#define IRQ_FLG_REPLACE	(0x0002)	/* replace existing handler	*/
#define IRQ_FLG_FAST	(0x0004)
#define IRQ_FLG_SLOW	(0x0008)
#define IRQ_FLG_STD	(0x8000)	/* internally used		*/
#endif

/*
 * This structure is used to chain together the ISRs for a particular
 * interrupt source (if it supports chaining).
 */
typedef struct irq_node {
	int		(*handler)(int, void *, struct pt_regs *);
	void		*dev_id;
	struct irq_node *next;
	unsigned long	flags;
	const char	*devname;
} irq_node_t;

/*
 * This structure has only 4 elements for speed reasons
 */
typedef struct irq_handler {
	int		(*handler)(int, void *, struct pt_regs *);
	unsigned long	flags;
	void		*dev_id;
	const char	*devname;
} irq_handler_t;

struct irq_controller {
	const char *name;
	spinlock_t lock;
	int (*startup)(unsigned int irq);
	void (*shutdown)(unsigned int irq);
	void (*enable)(unsigned int irq);
	void (*disable)(unsigned int irq);
};

extern int m68k_irq_startup(unsigned int);
extern void m68k_irq_shutdown(unsigned int);

/* count of spurious interrupts */
extern volatile unsigned int num_spurious;

/*
 * This function returns a new irq_node_t
 */
extern irq_node_t *new_irq_node(void);

#endif /* _M68K_IRQ_H_ */
