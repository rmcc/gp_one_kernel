#ifndef _ASM_SCORE_PGTABLE_H
#define _ASM_SCORE_PGTABLE_H

#include <linux/const.h>
#include <asm-generic/pgtable-nopmd.h>

#include <asm/fixmap.h>
#include <asm/setup.h>
#include <asm/pgtable-bits.h>

extern void load_pgd(unsigned long pg_dir);
extern pte_t invalid_pte_table[PAGE_SIZE/sizeof(pte_t)];

/* PGDIR_SHIFT determines what a third-level page table entry can map */
#define PGDIR_SHIFT	22
#define PGDIR_SIZE	(_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK	(~(PGDIR_SIZE - 1))

/*
 * Entries per page directory level: we use two-level, so
 * we don't really have any PUD/PMD directory physically.
 */
#define PGD_ORDER	0
#define PTE_ORDER	0

#define PTRS_PER_PGD	1024
#define PTRS_PER_PTE	1024

#define USER_PTRS_PER_PGD	(0x80000000UL/PGDIR_SIZE)
#define FIRST_USER_ADDRESS	0

#define VMALLOC_START		(0xc0000000UL)

#define PKMAP_BASE		(0xfd000000UL)

#define VMALLOC_END		(FIXADDR_START - 2*PAGE_SIZE)

#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte %08lx.\n", \
		__FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", \
		__FILE__, __LINE__, pgd_val(e))

/*
 * Empty pgd/pmd entries point to the invalid_pte_table.
 */
static inline int pmd_none(pmd_t pmd)
{
	return pmd_val(pmd) == (unsigned long) invalid_pte_table;
}

#define pmd_bad(pmd)		(pmd_val(pmd) & ~PAGE_MASK)

static inline int pmd_present(pmd_t pmd)
{
	return pmd_val(pmd) != (unsigned long) invalid_pte_table;
}

static inline void pmd_clear(pmd_t *pmdp)
{
	pmd_val(*pmdp) = ((unsigned long) invalid_pte_table);
}

#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pte_pfn(x)		((unsigned long)((x).pte >> PAGE_SHIFT))
#define pfn_pte(pfn, prot)	\
	__pte(((unsigned long long)(pfn) << PAGE_SHIFT) | pgprot_val(prot))

#define __pgd_offset(address)	pgd_index(address)
#define __pud_offset(address)	(((address) >> PUD_SHIFT) & (PTRS_PER_PUD-1))
#define __pmd_offset(address)	(((address) >> PMD_SHIFT) & (PTRS_PER_PMD-1))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address)	pgd_offset(&init_mm, address)
#define pgd_index(address)	(((address) >> PGDIR_SHIFT) & (PTRS_PER_PGD-1))

/* to find an entry in a page-table-directory */
#define pgd_offset(mm, addr)	((mm)->pgd + pgd_index(addr))

/* Find an entry in the third-level page table.. */
#define __pte_offset(address)		\
	(((address) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, address)	\
	((pte_t *) pmd_page_vaddr(*(dir)) + __pte_offset(address))
#define pte_offset_kernel(dir, address)	\
	((pte_t *) pmd_page_vaddr(*(dir)) + __pte_offset(address))

#define pte_offset_map(dir, address)	\
	((pte_t *)page_address(pmd_page(*(dir))) + __pte_offset(address))
#define pte_offset_map_nested(dir, address)	\
	((pte_t *)page_address(pmd_page(*(dir))) + __pte_offset(address))
#define pte_unmap(pte) ((void)(pte))
#define pte_unmap_nested(pte) ((void)(pte))

/*
 * Bits 9(_PAGE_PRESENT) and 10(_PAGE_FILE)are taken,
 * split up 30 bits of offset into this range:
 */
#define PTE_FILE_MAX_BITS	30
#define pte_to_pgoff(_pte)		\
	(((_pte).pte & 0x1ff) | (((_pte).pte >> 11) << 9))
#define pgoff_to_pte(off)		\
	((pte_t) {((off) & 0x1ff) | (((off) >> 9) << 11) | _PAGE_FILE})
#define __pte_to_swp_entry(pte)		\
	((swp_entry_t) { pte_val(pte)})
#define __swp_entry_to_pte(x)	((pte_t) {(x).val})

#define __P000 __pgprot(0)
#define __P001 __pgprot(0)
#define __P010 __pgprot(0)
#define __P011 __pgprot(0)
#define __P100 __pgprot(0)
#define __P101 __pgprot(0)
#define __P110 __pgprot(0)
#define __P111 __pgprot(0)

#define __S000 __pgprot(0)
#define __S001 __pgprot(0)
#define __S010 __pgprot(0)
#define __S011 __pgprot(0)
#define __S100 __pgprot(0)
#define __S101 __pgprot(0)
#define __S110 __pgprot(0)
#define __S111 __pgprot(0)

#define pmd_page(pmd) virt_to_page(__va(pmd_val(pmd)))
#define mk_pte(page, prot)	pfn_pte(page_to_pfn(page), prot)
static inline pte_t pte_mkspecial(pte_t pte) { return pte; }

#define set_pte(pteptr, pteval) (*(pteptr) = pteval)
#define set_pte_at(mm, addr, ptep, pteval) set_pte(ptep, pteval)
#define pte_clear(mm, addr, xp)		\
	do { set_pte_at(mm, addr, xp, __pte(0)); } while (0)

#define io_remap_pfn_range(vma, vaddr, pfn, size, prot)		\
		remap_pfn_range(vma, vaddr, pfn, size, prot)

#define pgd_present(pgd)	(1) /* pages are always present on non MMU */
#define pgd_none(pgd)		(0)
#define pgd_bad(pgd)		(0)
#define pgd_clear(pgdp)

#define kern_addr_valid(addr)	(1)
#define	pmd_offset(a, b)	((void *) 0)
#define pmd_page_vaddr(pmd)	pmd_val(pmd)

#define pte_none(pte)		(!(pte_val(pte) & ~_PAGE_GLOBAL))
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)

#define pud_offset(pgd, address) ((pud_t *) pgd)

#define PAGE_NONE		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_SHARED		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_COPY		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_READONLY		__pgprot(0) /* these mean nothing to non MMU */
#define PAGE_KERNEL		__pgprot(0) /* these mean nothing to non MMU */

#define pgprot_noncached(x)	(x)

#define __swp_type(x)		(0)
#define __swp_offset(x)		(0)
#define __swp_entry(typ, off)	((swp_entry_t) { ((typ) | ((off) << 7)) })

#define ZERO_PAGE(vaddr)	({ BUG(); NULL; })

#define swapper_pg_dir ((pgd_t *) NULL)

#define pgtable_cache_init()	do {} while (0)

#define arch_enter_lazy_cpu_mode()	do {} while (0)

static inline int pte_write(pte_t pte)
{
	return pte_val(pte) & _PAGE_WRITE;
}

static inline int pte_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_MODIFIED;
}

static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}

static inline int pte_file(pte_t pte)
{
	return pte_val(pte) & _PAGE_FILE;
}

#define pte_special(pte)	(0)

static inline pte_t pte_wrprotect(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_WRITE | _PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_mkclean(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_MODIFIED|_PAGE_SILENT_WRITE);
	return pte;
}

static inline pte_t pte_mkold(pte_t pte)
{
	pte_val(pte) &= ~(_PAGE_ACCESSED|_PAGE_SILENT_READ);
	return pte;
}

static inline pte_t pte_mkwrite(pte_t pte)
{
	pte_val(pte) |= _PAGE_WRITE;
	if (pte_val(pte) & _PAGE_MODIFIED)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkdirty(pte_t pte)
{
	pte_val(pte) |= _PAGE_MODIFIED;
	if (pte_val(pte) & _PAGE_WRITE)
		pte_val(pte) |= _PAGE_SILENT_WRITE;
	return pte;
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	pte_val(pte) |= _PAGE_ACCESSED;
	if (pte_val(pte) & _PAGE_READ)
		pte_val(pte) |= _PAGE_SILENT_READ;
	return pte;
}

#define set_pmd(pmdptr, pmdval)		\
	 do { *(pmdptr) = (pmdval); } while (0)
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)

extern unsigned long pgd_current;
extern void paging_init(void);

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot));
}

extern void __update_tlb(struct vm_area_struct *vma,
	unsigned long address,	pte_t pte);
extern void __update_cache(struct vm_area_struct *vma,
	unsigned long address,	pte_t pte);

static inline void update_mmu_cache(struct vm_area_struct *vma,
	unsigned long address, pte_t pte)
{
	__update_tlb(vma, address, pte);
	__update_cache(vma, address, pte);
}

#ifndef __ASSEMBLY__
#include <asm-generic/pgtable.h>

void setup_memory(void);
#endif /* __ASSEMBLY__ */

#endif /* _ASM_SCORE_PGTABLE_H */
