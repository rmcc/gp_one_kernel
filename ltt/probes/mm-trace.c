/*
 * ltt/probes/mm-trace.c
 *
 * MM tracepoint probes.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/swap.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/swapops.h>
#include <trace/page_alloc.h>
#include <trace/filemap.h>
#include <trace/swap.h>
#include <trace/memory.h>
#include <trace/hugetlb.h>

void probe_wait_on_page_start(struct page *page, int bit_nr)
{
	trace_mark_tp(mm_wait_on_page_start, wait_on_page_start,
		probe_wait_on_page_start, "pfn %lu bit_nr %d",
		page_to_pfn(page), bit_nr);
}

void probe_wait_on_page_end(struct page *page, int bit_nr)
{
	trace_mark_tp(mm_wait_on_page_end, wait_on_page_end,
		probe_wait_on_page_end, "pfn %lu bit_nr %d",
		page_to_pfn(page), bit_nr);
}

void probe_hugetlb_page_free(struct page *page)
{
	trace_mark_tp(mm_huge_page_free, hugetlb_page_free,
		probe_hugetlb_page_free, "pfn %lu", page_to_pfn(page));
}

void probe_hugetlb_page_alloc(struct page *page)
{
	if (page)
		trace_mark_tp(mm_huge_page_alloc, hugetlb_page_alloc,
			probe_hugetlb_page_alloc, "pfn %lu", page_to_pfn(page));
}

void probe_memory_handle_fault_entry(struct mm_struct *mm,
	struct vm_area_struct *vma, unsigned long address, int write_access)
{
	trace_mark_tp(mm_handle_fault_entry, memory_handle_fault_entry,
		probe_memory_handle_fault_entry,
		"address %lu ip #p%ld write_access %d",
		address, KSTK_EIP(current), write_access);
}

void probe_memory_handle_fault_exit(int res)
{
	trace_mark_tp(mm_handle_fault_exit, memory_handle_fault_exit,
		probe_memory_handle_fault_exit, "res %d", res);
}

void probe_page_free(struct page *page, unsigned int order)
{
	trace_mark_tp(mm_page_free, page_free,
		probe_page_free, "order %u pfn %lu", order, page_to_pfn(page));
}

void probe_page_alloc(struct page *page, unsigned int order)
{
	if (page)
		trace_mark_tp(mm_page_alloc, page_alloc,
			probe_page_alloc, "order %u pfn %lu", order,
			page_to_pfn(page));
}

#ifdef CONFIG_SWAP
void probe_swap_in(struct page *page, swp_entry_t entry)
{
	trace_mark_tp(mm_swap_in, swap_in, probe_swap_in,
		"pfn %lu filp %p offset %lu",
		page_to_pfn(page),
		get_swap_info_struct(swp_type(entry))->swap_file,
		swp_offset(entry));
}

void probe_swap_out(struct page *page)
{
	trace_mark_tp(mm_swap_out, swap_out, probe_swap_out,
		"pfn %lu filp %p offset %lu",
		page_to_pfn(page),
		get_swap_info_struct(swp_type(
			page_swp_entry(page)))->swap_file,
		swp_offset(page_swp_entry(page)));
}

void probe_swap_file_close(struct file *file)
{
	trace_mark_tp(mm_swap_file_close, swap_file_close,
		probe_swap_file_close, "filp %p", file);
}

void probe_swap_file_open(struct file *file, char *filename)
{
	trace_mark_tp(mm_swap_file_open, swap_file_open,
		probe_swap_file_open, "filp %p filename %s",
		file, filename);
}
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathieu Desnoyers");
MODULE_DESCRIPTION("MM Tracepoint Probes");
