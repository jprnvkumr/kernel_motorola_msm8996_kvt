#ifndef ___ASM_SPARC_SPINLOCK_H
#define ___ASM_SPARC_SPINLOCK_H
#if defined(__sparc__) && defined(__arch64__)
#include <asm/spinlock_64.h>
#else
#include <asm/spinlock_32.h>
#endif
#endif
