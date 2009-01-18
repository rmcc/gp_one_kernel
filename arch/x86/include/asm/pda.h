#ifndef _ASM_X86_PDA_H
#define _ASM_X86_PDA_H

#ifndef __ASSEMBLY__
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/cache.h>
#include <linux/threads.h>
#include <asm/page.h>
#include <asm/percpu.h>

/* Per processor datastructure. %gs points to it while the kernel runs */
struct x8664_pda {
	struct task_struct *pcurrent;	/* 0  Current process */
	unsigned long dummy;
	unsigned long kernelstack;	/* 16 top of kernel stack for current */
	unsigned long oldrsp;		/* 24 user rsp for system call */
	int irqcount;			/* 32 Irq nesting counter. Starts -1 */
	unsigned int cpunumber;		/* 36 Logical CPU number */
#ifdef CONFIG_CC_STACKPROTECTOR
	unsigned long stack_canary;	/* 40 stack canary value */
					/* gcc-ABI: this canary MUST be at
					   offset 40!!! */
#endif
	char *irqstackptr;
	short nodenumber;		/* number of current node (32k max) */
	short in_bootmem;		/* pda lives in bootmem */
	short isidle;
} ____cacheline_aligned_in_smp;

DECLARE_PER_CPU(struct x8664_pda, __pda);
extern void pda_init(int);

#define cpu_pda(cpu)		(&per_cpu(__pda, cpu))

#define read_pda(field)		percpu_read(__pda.field)
#define write_pda(field, val)	percpu_write(__pda.field, val)
#define add_pda(field, val)	percpu_add(__pda.field, val)
#define sub_pda(field, val)	percpu_sub(__pda.field, val)
#define or_pda(field, val)	percpu_or(__pda.field, val)

/* This is not atomic against other CPUs -- CPU preemption needs to be off */
#define test_and_clear_bit_pda(bit, field)				\
	x86_test_and_clear_bit_percpu(bit, __pda.field)

#endif

#define PDA_STACKOFFSET (5*8)

#endif /* _ASM_X86_PDA_H */
