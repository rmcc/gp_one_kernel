/*
 * arch/score/kernel/syscall.c
 *
 * Score Processor version.
 *
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Chen Liqin <liqin.chen@sunplusct.com>
 *  Lennox Wu <lennox.wu@sunplusct.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/unistd.h>

unsigned long shm_align_mask = PAGE_SIZE - 1;
EXPORT_SYMBOL(shm_align_mask);

asmlinkage unsigned long
sys_mmap2(unsigned long addr, unsigned long len, unsigned long prot,
	  unsigned long flags, unsigned long fd, unsigned long pgoff)
{
	int error = -EBADF;
	struct file *file = NULL;

	if (pgoff & (~PAGE_MASK >> 12))
		return -EINVAL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			return error;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags,
			pgoff >> (PAGE_SHIFT - 12));
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);

	return error;
}

/*
 * Clone a task - this clones the calling program thread.
 * This is called indirectly via a small wrapper
 */
int score_clone(struct pt_regs *regs)
{
	unsigned long clone_flags;
	unsigned long newsp;
	int __user *parent_tidptr, *child_tidptr;

	clone_flags = regs->regs[4];
	newsp = regs->regs[5];
	if (!newsp)
		newsp = regs->regs[0];
	parent_tidptr = (int __user *)regs->regs[6];

	child_tidptr = NULL;
	if (clone_flags & (CLONE_CHILD_SETTID | CLONE_CHILD_CLEARTID)) {
		int __user *__user *usp = (int __user *__user *)regs->regs[0];

		if (get_user(child_tidptr, &usp[4]))
			return -EFAULT;
	}

	return do_fork(clone_flags, newsp, regs, 0,
			parent_tidptr, child_tidptr);
}

/*
 * sys_execve() executes a new program.
 * This is called indirectly via a small wrapper
 */
int score_execve(struct pt_regs *regs)
{
	int error;
	char *filename;

	filename = getname((char *) (long) regs->regs[4]);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		return error;

	error = do_execve(filename, (char **) (long) regs->regs[5],
			  (char **) (long) regs->regs[6], regs);

	putname(filename);
	return error;
}

/*
 * If we ever come here the user sp is bad.  Zap the process right away.
 * Due to the bad stack signaling wouldn't work.
 */
void bad_stack(void)
{
	do_exit(SIGSEGV);
}

/*
 * Do a system call from kernel instead of calling sys_execve so we
 * end up with proper pt_regs.
 */
int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
	register unsigned long __r4 asm("r4") = (unsigned long) filename;
	register unsigned long __r5 asm("r5") = (unsigned long) argv;
	register unsigned long __r6 asm("r6") = (unsigned long) envp;
	register unsigned long __r7 asm("r7");

	__asm__ __volatile__ ("	\n"
		"ldi	r27, %5		\n"
		"syscall		\n"
		"mv	%0, r4		\n"
		"mv	%1, r7		\n"
		: "=&r" (__r4), "=r" (__r7)
		: "r" (__r4), "r" (__r5), "r" (__r6), "i" (__NR_execve)
		: "r8", "r9", "r10", "r11", "r22", "r23", "r24", "r25",
		  "r26", "r27", "memory");

	if (__r7 == 0)
		return __r4;

	return -__r4;
}
