#ifndef _LINUX_FTRACE_H
#define _LINUX_FTRACE_H

#ifdef CONFIG_FTRACE

#include <linux/linkage.h>

extern int ftrace_enabled;
extern int
ftrace_enable_sysctl(struct ctl_table *table, int write,
		     struct file *filp, void __user *buffer, size_t *lenp,
		     loff_t *ppos);

typedef void (*ftrace_func_t)(unsigned long ip, unsigned long parent_ip);

struct ftrace_ops {
	ftrace_func_t	  func;
	struct ftrace_ops *next;
};

/*
 * The ftrace_ops must be a static and should also
 * be read_mostly.  These functions do modify read_mostly variables
 * so use them sparely. Never free an ftrace_op or modify the
 * next pointer after it has been registered. Even after unregistering
 * it, the next pointer may still be used internally.
 */
int register_ftrace_function(struct ftrace_ops *ops);
int unregister_ftrace_function(struct ftrace_ops *ops);
void clear_ftrace_function(void);

extern void ftrace_stub(unsigned long a0, unsigned long a1);
extern void mcount(void);

#else /* !CONFIG_FTRACE */
# define register_ftrace_function(ops) do { } while (0)
# define unregister_ftrace_function(ops) do { } while (0)
# define clear_ftrace_function(ops) do { } while (0)
#endif /* CONFIG_FTRACE */

#ifdef CONFIG_DYNAMIC_FTRACE
# define FTRACE_HASHBITS	10
# define FTRACE_HASHSIZE	(1<<FTRACE_HASHBITS)

struct dyn_ftrace {
	struct hlist_node node;
	unsigned long	  ip;
};

/* defined in arch */
extern struct dyn_ftrace *
ftrace_alloc_shutdown_node(unsigned long ip);
extern int ftrace_shutdown_arch_init(void);
extern void ftrace_code_disable(struct dyn_ftrace *rec);
extern void ftrace_startup_code(void);
extern void ftrace_shutdown_code(void);
extern void ftrace_shutdown_replenish(void);
#endif

#ifdef CONFIG_FRAME_POINTER
/* TODO: need to fix this for ARM */
# define CALLER_ADDR0 ((unsigned long)__builtin_return_address(0))
# define CALLER_ADDR1 ((unsigned long)__builtin_return_address(1))
# define CALLER_ADDR2 ((unsigned long)__builtin_return_address(2))
# define CALLER_ADDR3 ((unsigned long)__builtin_return_address(3))
# define CALLER_ADDR4 ((unsigned long)__builtin_return_address(4))
# define CALLER_ADDR5 ((unsigned long)__builtin_return_address(5))
#else
# define CALLER_ADDR0 ((unsigned long)__builtin_return_address(0))
# define CALLER_ADDR1 0UL
# define CALLER_ADDR2 0UL
# define CALLER_ADDR3 0UL
# define CALLER_ADDR4 0UL
# define CALLER_ADDR5 0UL
#endif

#ifdef CONFIG_IRQSOFF_TRACER
  extern void notrace time_hardirqs_on(unsigned long a0, unsigned long a1);
  extern void notrace time_hardirqs_off(unsigned long a0, unsigned long a1);
#else
# define time_hardirqs_on(a0, a1)		do { } while (0)
# define time_hardirqs_off(a0, a1)		do { } while (0)
#endif

#ifdef CONFIG_PREEMPT_TRACER
  extern void notrace trace_preempt_on(unsigned long a0, unsigned long a1);
  extern void notrace trace_preempt_off(unsigned long a0, unsigned long a1);
#else
# define trace_preempt_on(a0, a1)		do { } while (0)
# define trace_preempt_off(a0, a1)		do { } while (0)
#endif

#endif /* _LINUX_FTRACE_H */
