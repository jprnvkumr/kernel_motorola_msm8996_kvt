#ifndef _ASM_X86_EFI_H
#define _ASM_X86_EFI_H

#ifdef CONFIG_X86_32
#else /* !CONFIG_X86_32 */

#define MAX_EFI_IO_PAGES	100

extern u64 efi_call0(void *fp);
extern u64 efi_call1(void *fp, u64 arg1);
extern u64 efi_call2(void *fp, u64 arg1, u64 arg2);
extern u64 efi_call3(void *fp, u64 arg1, u64 arg2, u64 arg3);
extern u64 efi_call4(void *fp, u64 arg1, u64 arg2, u64 arg3, u64 arg4);
extern u64 efi_call5(void *fp, u64 arg1, u64 arg2, u64 arg3,
		     u64 arg4, u64 arg5);
extern u64 efi_call6(void *fp, u64 arg1, u64 arg2, u64 arg3,
		     u64 arg4, u64 arg5, u64 arg6);

#define efi_call_phys0(f)			\
	efi_call0((void *)(f))
#define efi_call_phys1(f, a1)			\
	efi_call1((void *)(f), (u64)(a1))
#define efi_call_phys2(f, a1, a2)			\
	efi_call2((void *)(f), (u64)(a1), (u64)(a2))
#define efi_call_phys3(f, a1, a2, a3)				\
	efi_call3((void *)(f), (u64)(a1), (u64)(a2), (u64)(a3))
#define efi_call_phys4(f, a1, a2, a3, a4)				\
	efi_call4((void *)(f), (u64)(a1), (u64)(a2), (u64)(a3),		\
		  (u64)(a4))
#define efi_call_phys5(f, a1, a2, a3, a4, a5)				\
	efi_call5((void *)(f), (u64)(a1), (u64)(a2), (u64)(a3),		\
		  (u64)(a4), (u64)(a5))
#define efi_call_phys6(f, a1, a2, a3, a4, a5, a6)			\
	efi_call6((void *)(f), (u64)(a1), (u64)(a2), (u64)(a3),		\
		  (u64)(a4), (u64)(a5), (u64)(a6))

#define efi_call_virt0(f)				\
	efi_call0((void *)(efi.systab->runtime->f))
#define efi_call_virt1(f, a1)					\
	efi_call1((void *)(efi.systab->runtime->f), (u64)(a1))
#define efi_call_virt2(f, a1, a2)					\
	efi_call2((void *)(efi.systab->runtime->f), (u64)(a1), (u64)(a2))
#define efi_call_virt3(f, a1, a2, a3)					\
	efi_call3((void *)(efi.systab->runtime->f), (u64)(a1), (u64)(a2), \
		  (u64)(a3))
#define efi_call_virt4(f, a1, a2, a3, a4)				\
	efi_call4((void *)(efi.systab->runtime->f), (u64)(a1), (u64)(a2), \
		  (u64)(a3), (u64)(a4))
#define efi_call_virt5(f, a1, a2, a3, a4, a5)				\
	efi_call5((void *)(efi.systab->runtime->f), (u64)(a1), (u64)(a2), \
		  (u64)(a3), (u64)(a4), (u64)(a5))
#define efi_call_virt6(f, a1, a2, a3, a4, a5, a6)			\
	efi_call6((void *)(efi.systab->runtime->f), (u64)(a1), (u64)(a2), \
		  (u64)(a3), (u64)(a4), (u64)(a5), (u64)(a6))

#define efi_early_ioremap(addr, size)		early_ioremap(addr, size)
#define efi_early_iounmap(vaddr, size)		early_iounmap(vaddr, size)

extern void *efi_ioremap(unsigned long offset, unsigned long size);

extern int efi_time;

#endif /* CONFIG_X86_32 */

extern void efi_reserve_bootmem(void);
extern void efi_call_phys_prelog(void);
extern void efi_call_phys_epilog(void);
extern void runtime_code_page_mkexec(void);

#endif
