/*
 * include/asm-sh/cpu-sh3/cacheflush.h
 *
 * Copyright (C) 1999 Niibe Yutaka
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_CPU_SH3_CACHEFLUSH_H
#define __ASM_CPU_SH3_CACHEFLUSH_H

#if defined(CONFIG_SH7705_CACHE_32KB)
/* SH7705 is an SH3 processor with 32KB cache. This has alias issues like the
 * SH4. Unlike the SH4 this is a unified cache so we need to do some work
 * in mmap when 'exec'ing a new binary
 */
void flush_cache_all(void);
void flush_cache_mm(struct mm_struct *mm);
#define flush_cache_dup_mm(mm) flush_cache_mm(mm)
void flush_cache_range(struct vm_area_struct *vma, unsigned long start,
                              unsigned long end);
void flush_cache_page(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn);
void flush_dcache_page(struct page *pg);
void flush_icache_range(unsigned long start, unsigned long end);
void flush_icache_page(struct vm_area_struct *vma, struct page *page);

/* SH3 has unified cache so no special action needed here */
#define flush_cache_sigtramp(vaddr)		do { } while (0)
#define flush_icache_user_range(vma,pg,adr,len)	do { } while (0)

#else
#include <cpu-common/cpu/cacheflush.h>
#endif

#endif /* __ASM_CPU_SH3_CACHEFLUSH_H */
