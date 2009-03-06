/*
 * linux/mm/percpu.c - percpu memory allocator
 *
 * Copyright (C) 2009		SUSE Linux Products GmbH
 * Copyright (C) 2009		Tejun Heo <tj@kernel.org>
 *
 * This file is released under the GPLv2.
 *
 * This is percpu allocator which can handle both static and dynamic
 * areas.  Percpu areas are allocated in chunks in vmalloc area.  Each
 * chunk is consisted of num_possible_cpus() units and the first chunk
 * is used for static percpu variables in the kernel image (special
 * boot time alloc/init handling necessary as these areas need to be
 * brought up before allocation services are running).  Unit grows as
 * necessary and all units grow or shrink in unison.  When a chunk is
 * filled up, another chunk is allocated.  ie. in vmalloc area
 *
 *  c0                           c1                         c2
 *  -------------------          -------------------        ------------
 * | u0 | u1 | u2 | u3 |        | u0 | u1 | u2 | u3 |      | u0 | u1 | u
 *  -------------------  ......  -------------------  ....  ------------
 *
 * Allocation is done in offset-size areas of single unit space.  Ie,
 * an area of 512 bytes at 6k in c1 occupies 512 bytes at 6k of c1:u0,
 * c1:u1, c1:u2 and c1:u3.  Percpu access can be done by configuring
 * percpu base registers UNIT_SIZE apart.
 *
 * There are usually many small percpu allocations many of them as
 * small as 4 bytes.  The allocator organizes chunks into lists
 * according to free size and tries to allocate from the fullest one.
 * Each chunk keeps the maximum contiguous area size hint which is
 * guaranteed to be eqaul to or larger than the maximum contiguous
 * area in the chunk.  This helps the allocator not to iterate the
 * chunk maps unnecessarily.
 *
 * Allocation state in each chunk is kept using an array of integers
 * on chunk->map.  A positive value in the map represents a free
 * region and negative allocated.  Allocation inside a chunk is done
 * by scanning this map sequentially and serving the first matching
 * entry.  This is mostly copied from the percpu_modalloc() allocator.
 * Chunks are also linked into a rb tree to ease address to chunk
 * mapping during free.
 *
 * To use this allocator, arch code should do the followings.
 *
 * - define CONFIG_HAVE_DYNAMIC_PER_CPU_AREA
 *
 * - define __addr_to_pcpu_ptr() and __pcpu_ptr_to_addr() to translate
 *   regular address to percpu pointer and back
 *
 * - use pcpu_setup_first_chunk() during percpu area initialization to
 *   setup the first chunk containing the kernel static percpu area
 */

#include <linux/bitmap.h>
#include <linux/bootmem.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/pfn.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#define PCPU_SLOT_BASE_SHIFT		5	/* 1-31 shares the same slot */
#define PCPU_DFL_MAP_ALLOC		16	/* start a map with 16 ents */

struct pcpu_chunk {
	struct list_head	list;		/* linked to pcpu_slot lists */
	struct rb_node		rb_node;	/* key is chunk->vm->addr */
	int			free_size;	/* free bytes in the chunk */
	int			contig_hint;	/* max contiguous size hint */
	struct vm_struct	*vm;		/* mapped vmalloc region */
	int			map_used;	/* # of map entries used */
	int			map_alloc;	/* # of map entries allocated */
	int			*map;		/* allocation map */
	bool			immutable;	/* no [de]population allowed */
	struct page		**page;		/* points to page array */
	struct page		*page_ar[];	/* #cpus * UNIT_PAGES */
};

static int pcpu_unit_pages __read_mostly;
static int pcpu_unit_size __read_mostly;
static int pcpu_chunk_size __read_mostly;
static int pcpu_nr_slots __read_mostly;
static size_t pcpu_chunk_struct_size __read_mostly;

/* the address of the first chunk which starts with the kernel static area */
void *pcpu_base_addr __read_mostly;
EXPORT_SYMBOL_GPL(pcpu_base_addr);

/* optional reserved chunk, only accessible for reserved allocations */
static struct pcpu_chunk *pcpu_reserved_chunk;
/* offset limit of the reserved chunk */
static int pcpu_reserved_chunk_limit;

/*
 * One mutex to rule them all.
 *
 * The following mutex is grabbed in the outermost public alloc/free
 * interface functions and released only when the operation is
 * complete.  As such, every function in this file other than the
 * outermost functions are called under pcpu_mutex.
 *
 * It can easily be switched to use spinlock such that only the area
 * allocation and page population commit are protected with it doing
 * actual [de]allocation without holding any lock.  However, given
 * what this allocator does, I think it's better to let them run
 * sequentially.
 */
static DEFINE_MUTEX(pcpu_mutex);

static struct list_head *pcpu_slot __read_mostly; /* chunk list slots */
static struct rb_root pcpu_addr_root = RB_ROOT;	/* chunks by address */

static int __pcpu_size_to_slot(int size)
{
	int highbit = fls(size);	/* size is in bytes */
	return max(highbit - PCPU_SLOT_BASE_SHIFT + 2, 1);
}

static int pcpu_size_to_slot(int size)
{
	if (size == pcpu_unit_size)
		return pcpu_nr_slots - 1;
	return __pcpu_size_to_slot(size);
}

static int pcpu_chunk_slot(const struct pcpu_chunk *chunk)
{
	if (chunk->free_size < sizeof(int) || chunk->contig_hint < sizeof(int))
		return 0;

	return pcpu_size_to_slot(chunk->free_size);
}

static int pcpu_page_idx(unsigned int cpu, int page_idx)
{
	return cpu * pcpu_unit_pages + page_idx;
}

static struct page **pcpu_chunk_pagep(struct pcpu_chunk *chunk,
				      unsigned int cpu, int page_idx)
{
	return &chunk->page[pcpu_page_idx(cpu, page_idx)];
}

static unsigned long pcpu_chunk_addr(struct pcpu_chunk *chunk,
				     unsigned int cpu, int page_idx)
{
	return (unsigned long)chunk->vm->addr +
		(pcpu_page_idx(cpu, page_idx) << PAGE_SHIFT);
}

static bool pcpu_chunk_page_occupied(struct pcpu_chunk *chunk,
				     int page_idx)
{
	return *pcpu_chunk_pagep(chunk, 0, page_idx) != NULL;
}

/**
 * pcpu_realloc - versatile realloc
 * @p: the current pointer (can be NULL for new allocations)
 * @size: the current size in bytes (can be 0 for new allocations)
 * @new_size: the wanted new size in bytes (can be 0 for free)
 *
 * More robust realloc which can be used to allocate, resize or free a
 * memory area of arbitrary size.  If the needed size goes over
 * PAGE_SIZE, kernel VM is used.
 *
 * RETURNS:
 * The new pointer on success, NULL on failure.
 */
static void *pcpu_realloc(void *p, size_t size, size_t new_size)
{
	void *new;

	if (new_size <= PAGE_SIZE)
		new = kmalloc(new_size, GFP_KERNEL);
	else
		new = vmalloc(new_size);
	if (new_size && !new)
		return NULL;

	memcpy(new, p, min(size, new_size));
	if (new_size > size)
		memset(new + size, 0, new_size - size);

	if (size <= PAGE_SIZE)
		kfree(p);
	else
		vfree(p);

	return new;
}

/**
 * pcpu_chunk_relocate - put chunk in the appropriate chunk slot
 * @chunk: chunk of interest
 * @oslot: the previous slot it was on
 *
 * This function is called after an allocation or free changed @chunk.
 * New slot according to the changed state is determined and @chunk is
 * moved to the slot.  Note that the reserved chunk is never put on
 * chunk slots.
 */
static void pcpu_chunk_relocate(struct pcpu_chunk *chunk, int oslot)
{
	int nslot = pcpu_chunk_slot(chunk);

	if (chunk != pcpu_reserved_chunk && oslot != nslot) {
		if (oslot < nslot)
			list_move(&chunk->list, &pcpu_slot[nslot]);
		else
			list_move_tail(&chunk->list, &pcpu_slot[nslot]);
	}
}

static struct rb_node **pcpu_chunk_rb_search(void *addr,
					     struct rb_node **parentp)
{
	struct rb_node **p = &pcpu_addr_root.rb_node;
	struct rb_node *parent = NULL;
	struct pcpu_chunk *chunk;

	while (*p) {
		parent = *p;
		chunk = rb_entry(parent, struct pcpu_chunk, rb_node);

		if (addr < chunk->vm->addr)
			p = &(*p)->rb_left;
		else if (addr > chunk->vm->addr)
			p = &(*p)->rb_right;
		else
			break;
	}

	if (parentp)
		*parentp = parent;
	return p;
}

/**
 * pcpu_chunk_addr_search - search for chunk containing specified address
 * @addr: address to search for
 *
 * Look for chunk which might contain @addr.  More specifically, it
 * searchs for the chunk with the highest start address which isn't
 * beyond @addr.
 *
 * RETURNS:
 * The address of the found chunk.
 */
static struct pcpu_chunk *pcpu_chunk_addr_search(void *addr)
{
	struct rb_node *n, *parent;
	struct pcpu_chunk *chunk;

	/* is it in the reserved chunk? */
	if (pcpu_reserved_chunk) {
		void *start = pcpu_reserved_chunk->vm->addr;

		if (addr >= start && addr < start + pcpu_reserved_chunk_limit)
			return pcpu_reserved_chunk;
	}

	/* nah... search the regular ones */
	n = *pcpu_chunk_rb_search(addr, &parent);
	if (!n) {
		/* no exactly matching chunk, the parent is the closest */
		n = parent;
		BUG_ON(!n);
	}
	chunk = rb_entry(n, struct pcpu_chunk, rb_node);

	if (addr < chunk->vm->addr) {
		/* the parent was the next one, look for the previous one */
		n = rb_prev(n);
		BUG_ON(!n);
		chunk = rb_entry(n, struct pcpu_chunk, rb_node);
	}

	return chunk;
}

/**
 * pcpu_chunk_addr_insert - insert chunk into address rb tree
 * @new: chunk to insert
 *
 * Insert @new into address rb tree.
 */
static void pcpu_chunk_addr_insert(struct pcpu_chunk *new)
{
	struct rb_node **p, *parent;

	p = pcpu_chunk_rb_search(new->vm->addr, &parent);
	BUG_ON(*p);
	rb_link_node(&new->rb_node, parent, p);
	rb_insert_color(&new->rb_node, &pcpu_addr_root);
}

/**
 * pcpu_split_block - split a map block
 * @chunk: chunk of interest
 * @i: index of map block to split
 * @head: head size in bytes (can be 0)
 * @tail: tail size in bytes (can be 0)
 *
 * Split the @i'th map block into two or three blocks.  If @head is
 * non-zero, @head bytes block is inserted before block @i moving it
 * to @i+1 and reducing its size by @head bytes.
 *
 * If @tail is non-zero, the target block, which can be @i or @i+1
 * depending on @head, is reduced by @tail bytes and @tail byte block
 * is inserted after the target block.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
static int pcpu_split_block(struct pcpu_chunk *chunk, int i, int head, int tail)
{
	int nr_extra = !!head + !!tail;
	int target = chunk->map_used + nr_extra;

	/* reallocation required? */
	if (chunk->map_alloc < target) {
		int new_alloc;
		int *new;

		new_alloc = PCPU_DFL_MAP_ALLOC;
		while (new_alloc < target)
			new_alloc *= 2;

		if (chunk->map_alloc < PCPU_DFL_MAP_ALLOC) {
			/*
			 * map_alloc smaller than the default size
			 * indicates that the chunk is one of the
			 * first chunks and still using static map.
			 * Allocate a dynamic one and copy.
			 */
			new = pcpu_realloc(NULL, 0, new_alloc * sizeof(new[0]));
			if (new)
				memcpy(new, chunk->map,
				       chunk->map_alloc * sizeof(new[0]));
		} else
			new = pcpu_realloc(chunk->map,
					   chunk->map_alloc * sizeof(new[0]),
					   new_alloc * sizeof(new[0]));
		if (!new)
			return -ENOMEM;

		chunk->map_alloc = new_alloc;
		chunk->map = new;
	}

	/* insert a new subblock */
	memmove(&chunk->map[i + nr_extra], &chunk->map[i],
		sizeof(chunk->map[0]) * (chunk->map_used - i));
	chunk->map_used += nr_extra;

	if (head) {
		chunk->map[i + 1] = chunk->map[i] - head;
		chunk->map[i++] = head;
	}
	if (tail) {
		chunk->map[i++] -= tail;
		chunk->map[i] = tail;
	}
	return 0;
}

/**
 * pcpu_alloc_area - allocate area from a pcpu_chunk
 * @chunk: chunk of interest
 * @size: wanted size in bytes
 * @align: wanted align
 *
 * Try to allocate @size bytes area aligned at @align from @chunk.
 * Note that this function only allocates the offset.  It doesn't
 * populate or map the area.
 *
 * RETURNS:
 * Allocated offset in @chunk on success, -errno on failure.
 */
static int pcpu_alloc_area(struct pcpu_chunk *chunk, int size, int align)
{
	int oslot = pcpu_chunk_slot(chunk);
	int max_contig = 0;
	int i, off;

	for (i = 0, off = 0; i < chunk->map_used; off += abs(chunk->map[i++])) {
		bool is_last = i + 1 == chunk->map_used;
		int head, tail;

		/* extra for alignment requirement */
		head = ALIGN(off, align) - off;
		BUG_ON(i == 0 && head != 0);

		if (chunk->map[i] < 0)
			continue;
		if (chunk->map[i] < head + size) {
			max_contig = max(chunk->map[i], max_contig);
			continue;
		}

		/*
		 * If head is small or the previous block is free,
		 * merge'em.  Note that 'small' is defined as smaller
		 * than sizeof(int), which is very small but isn't too
		 * uncommon for percpu allocations.
		 */
		if (head && (head < sizeof(int) || chunk->map[i - 1] > 0)) {
			if (chunk->map[i - 1] > 0)
				chunk->map[i - 1] += head;
			else {
				chunk->map[i - 1] -= head;
				chunk->free_size -= head;
			}
			chunk->map[i] -= head;
			off += head;
			head = 0;
		}

		/* if tail is small, just keep it around */
		tail = chunk->map[i] - head - size;
		if (tail < sizeof(int))
			tail = 0;

		/* split if warranted */
		if (head || tail) {
			if (pcpu_split_block(chunk, i, head, tail))
				return -ENOMEM;
			if (head) {
				i++;
				off += head;
				max_contig = max(chunk->map[i - 1], max_contig);
			}
			if (tail)
				max_contig = max(chunk->map[i + 1], max_contig);
		}

		/* update hint and mark allocated */
		if (is_last)
			chunk->contig_hint = max_contig; /* fully scanned */
		else
			chunk->contig_hint = max(chunk->contig_hint,
						 max_contig);

		chunk->free_size -= chunk->map[i];
		chunk->map[i] = -chunk->map[i];

		pcpu_chunk_relocate(chunk, oslot);
		return off;
	}

	chunk->contig_hint = max_contig;	/* fully scanned */
	pcpu_chunk_relocate(chunk, oslot);

	/*
	 * Tell the upper layer that this chunk has no area left.
	 * Note that this is not an error condition but a notification
	 * to upper layer that it needs to look at other chunks.
	 * -ENOSPC is chosen as it isn't used in memory subsystem and
	 * matches the meaning in a way.
	 */
	return -ENOSPC;
}

/**
 * pcpu_free_area - free area to a pcpu_chunk
 * @chunk: chunk of interest
 * @freeme: offset of area to free
 *
 * Free area starting from @freeme to @chunk.  Note that this function
 * only modifies the allocation map.  It doesn't depopulate or unmap
 * the area.
 */
static void pcpu_free_area(struct pcpu_chunk *chunk, int freeme)
{
	int oslot = pcpu_chunk_slot(chunk);
	int i, off;

	for (i = 0, off = 0; i < chunk->map_used; off += abs(chunk->map[i++]))
		if (off == freeme)
			break;
	BUG_ON(off != freeme);
	BUG_ON(chunk->map[i] > 0);

	chunk->map[i] = -chunk->map[i];
	chunk->free_size += chunk->map[i];

	/* merge with previous? */
	if (i > 0 && chunk->map[i - 1] >= 0) {
		chunk->map[i - 1] += chunk->map[i];
		chunk->map_used--;
		memmove(&chunk->map[i], &chunk->map[i + 1],
			(chunk->map_used - i) * sizeof(chunk->map[0]));
		i--;
	}
	/* merge with next? */
	if (i + 1 < chunk->map_used && chunk->map[i + 1] >= 0) {
		chunk->map[i] += chunk->map[i + 1];
		chunk->map_used--;
		memmove(&chunk->map[i + 1], &chunk->map[i + 2],
			(chunk->map_used - (i + 1)) * sizeof(chunk->map[0]));
	}

	chunk->contig_hint = max(chunk->map[i], chunk->contig_hint);
	pcpu_chunk_relocate(chunk, oslot);
}

/**
 * pcpu_unmap - unmap pages out of a pcpu_chunk
 * @chunk: chunk of interest
 * @page_start: page index of the first page to unmap
 * @page_end: page index of the last page to unmap + 1
 * @flush: whether to flush cache and tlb or not
 *
 * For each cpu, unmap pages [@page_start,@page_end) out of @chunk.
 * If @flush is true, vcache is flushed before unmapping and tlb
 * after.
 */
static void pcpu_unmap(struct pcpu_chunk *chunk, int page_start, int page_end,
		       bool flush)
{
	unsigned int last = num_possible_cpus() - 1;
	unsigned int cpu;

	/* unmap must not be done on immutable chunk */
	WARN_ON(chunk->immutable);

	/*
	 * Each flushing trial can be very expensive, issue flush on
	 * the whole region at once rather than doing it for each cpu.
	 * This could be an overkill but is more scalable.
	 */
	if (flush)
		flush_cache_vunmap(pcpu_chunk_addr(chunk, 0, page_start),
				   pcpu_chunk_addr(chunk, last, page_end));

	for_each_possible_cpu(cpu)
		unmap_kernel_range_noflush(
				pcpu_chunk_addr(chunk, cpu, page_start),
				(page_end - page_start) << PAGE_SHIFT);

	/* ditto as flush_cache_vunmap() */
	if (flush)
		flush_tlb_kernel_range(pcpu_chunk_addr(chunk, 0, page_start),
				       pcpu_chunk_addr(chunk, last, page_end));
}

/**
 * pcpu_depopulate_chunk - depopulate and unmap an area of a pcpu_chunk
 * @chunk: chunk to depopulate
 * @off: offset to the area to depopulate
 * @size: size of the area to depopulate in bytes
 * @flush: whether to flush cache and tlb or not
 *
 * For each cpu, depopulate and unmap pages [@page_start,@page_end)
 * from @chunk.  If @flush is true, vcache is flushed before unmapping
 * and tlb after.
 */
static void pcpu_depopulate_chunk(struct pcpu_chunk *chunk, int off, int size,
				  bool flush)
{
	int page_start = PFN_DOWN(off);
	int page_end = PFN_UP(off + size);
	int unmap_start = -1;
	int uninitialized_var(unmap_end);
	unsigned int cpu;
	int i;

	for (i = page_start; i < page_end; i++) {
		for_each_possible_cpu(cpu) {
			struct page **pagep = pcpu_chunk_pagep(chunk, cpu, i);

			if (!*pagep)
				continue;

			__free_page(*pagep);

			/*
			 * If it's partial depopulation, it might get
			 * populated or depopulated again.  Mark the
			 * page gone.
			 */
			*pagep = NULL;

			unmap_start = unmap_start < 0 ? i : unmap_start;
			unmap_end = i + 1;
		}
	}

	if (unmap_start >= 0)
		pcpu_unmap(chunk, unmap_start, unmap_end, flush);
}

/**
 * pcpu_map - map pages into a pcpu_chunk
 * @chunk: chunk of interest
 * @page_start: page index of the first page to map
 * @page_end: page index of the last page to map + 1
 *
 * For each cpu, map pages [@page_start,@page_end) into @chunk.
 * vcache is flushed afterwards.
 */
static int pcpu_map(struct pcpu_chunk *chunk, int page_start, int page_end)
{
	unsigned int last = num_possible_cpus() - 1;
	unsigned int cpu;
	int err;

	/* map must not be done on immutable chunk */
	WARN_ON(chunk->immutable);

	for_each_possible_cpu(cpu) {
		err = map_kernel_range_noflush(
				pcpu_chunk_addr(chunk, cpu, page_start),
				(page_end - page_start) << PAGE_SHIFT,
				PAGE_KERNEL,
				pcpu_chunk_pagep(chunk, cpu, page_start));
		if (err < 0)
			return err;
	}

	/* flush at once, please read comments in pcpu_unmap() */
	flush_cache_vmap(pcpu_chunk_addr(chunk, 0, page_start),
			 pcpu_chunk_addr(chunk, last, page_end));
	return 0;
}

/**
 * pcpu_populate_chunk - populate and map an area of a pcpu_chunk
 * @chunk: chunk of interest
 * @off: offset to the area to populate
 * @size: size of the area to populate in bytes
 *
 * For each cpu, populate and map pages [@page_start,@page_end) into
 * @chunk.  The area is cleared on return.
 */
static int pcpu_populate_chunk(struct pcpu_chunk *chunk, int off, int size)
{
	const gfp_t alloc_mask = GFP_KERNEL | __GFP_HIGHMEM | __GFP_COLD;
	int page_start = PFN_DOWN(off);
	int page_end = PFN_UP(off + size);
	int map_start = -1;
	int uninitialized_var(map_end);
	unsigned int cpu;
	int i;

	for (i = page_start; i < page_end; i++) {
		if (pcpu_chunk_page_occupied(chunk, i)) {
			if (map_start >= 0) {
				if (pcpu_map(chunk, map_start, map_end))
					goto err;
				map_start = -1;
			}
			continue;
		}

		map_start = map_start < 0 ? i : map_start;
		map_end = i + 1;

		for_each_possible_cpu(cpu) {
			struct page **pagep = pcpu_chunk_pagep(chunk, cpu, i);

			*pagep = alloc_pages_node(cpu_to_node(cpu),
						  alloc_mask, 0);
			if (!*pagep)
				goto err;
		}
	}

	if (map_start >= 0 && pcpu_map(chunk, map_start, map_end))
		goto err;

	for_each_possible_cpu(cpu)
		memset(chunk->vm->addr + cpu * pcpu_unit_size + off, 0,
		       size);

	return 0;
err:
	/* likely under heavy memory pressure, give memory back */
	pcpu_depopulate_chunk(chunk, off, size, true);
	return -ENOMEM;
}

static void free_pcpu_chunk(struct pcpu_chunk *chunk)
{
	if (!chunk)
		return;
	if (chunk->vm)
		free_vm_area(chunk->vm);
	pcpu_realloc(chunk->map, chunk->map_alloc * sizeof(chunk->map[0]), 0);
	kfree(chunk);
}

static struct pcpu_chunk *alloc_pcpu_chunk(void)
{
	struct pcpu_chunk *chunk;

	chunk = kzalloc(pcpu_chunk_struct_size, GFP_KERNEL);
	if (!chunk)
		return NULL;

	chunk->map = pcpu_realloc(NULL, 0,
				  PCPU_DFL_MAP_ALLOC * sizeof(chunk->map[0]));
	chunk->map_alloc = PCPU_DFL_MAP_ALLOC;
	chunk->map[chunk->map_used++] = pcpu_unit_size;
	chunk->page = chunk->page_ar;

	chunk->vm = get_vm_area(pcpu_chunk_size, GFP_KERNEL);
	if (!chunk->vm) {
		free_pcpu_chunk(chunk);
		return NULL;
	}

	INIT_LIST_HEAD(&chunk->list);
	chunk->free_size = pcpu_unit_size;
	chunk->contig_hint = pcpu_unit_size;

	return chunk;
}

/**
 * pcpu_alloc - the percpu allocator
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 * @reserved: allocate from the reserved chunk if available
 *
 * Allocate percpu area of @size bytes aligned at @align.  Might
 * sleep.  Might trigger writeouts.
 *
 * RETURNS:
 * Percpu pointer to the allocated area on success, NULL on failure.
 */
static void *pcpu_alloc(size_t size, size_t align, bool reserved)
{
	void *ptr = NULL;
	struct pcpu_chunk *chunk;
	int slot, off;

	if (unlikely(!size || size > PCPU_MIN_UNIT_SIZE || align > PAGE_SIZE)) {
		WARN(true, "illegal size (%zu) or align (%zu) for "
		     "percpu allocation\n", size, align);
		return NULL;
	}

	mutex_lock(&pcpu_mutex);

	/* serve reserved allocations from the reserved chunk if available */
	if (reserved && pcpu_reserved_chunk) {
		chunk = pcpu_reserved_chunk;
		if (size > chunk->contig_hint)
			goto out_unlock;
		off = pcpu_alloc_area(chunk, size, align);
		if (off >= 0)
			goto area_found;
		goto out_unlock;
	}

	/* search through normal chunks */
	for (slot = pcpu_size_to_slot(size); slot < pcpu_nr_slots; slot++) {
		list_for_each_entry(chunk, &pcpu_slot[slot], list) {
			if (size > chunk->contig_hint)
				continue;
			off = pcpu_alloc_area(chunk, size, align);
			if (off >= 0)
				goto area_found;
			if (off != -ENOSPC)
				goto out_unlock;
		}
	}

	/* hmmm... no space left, create a new chunk */
	chunk = alloc_pcpu_chunk();
	if (!chunk)
		goto out_unlock;
	pcpu_chunk_relocate(chunk, -1);
	pcpu_chunk_addr_insert(chunk);

	off = pcpu_alloc_area(chunk, size, align);
	if (off < 0)
		goto out_unlock;

area_found:
	/* populate, map and clear the area */
	if (pcpu_populate_chunk(chunk, off, size)) {
		pcpu_free_area(chunk, off);
		goto out_unlock;
	}

	ptr = __addr_to_pcpu_ptr(chunk->vm->addr + off);
out_unlock:
	mutex_unlock(&pcpu_mutex);
	return ptr;
}

/**
 * __alloc_percpu - allocate dynamic percpu area
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 *
 * Allocate percpu area of @size bytes aligned at @align.  Might
 * sleep.  Might trigger writeouts.
 *
 * RETURNS:
 * Percpu pointer to the allocated area on success, NULL on failure.
 */
void *__alloc_percpu(size_t size, size_t align)
{
	return pcpu_alloc(size, align, false);
}
EXPORT_SYMBOL_GPL(__alloc_percpu);

/**
 * __alloc_reserved_percpu - allocate reserved percpu area
 * @size: size of area to allocate in bytes
 * @align: alignment of area (max PAGE_SIZE)
 *
 * Allocate percpu area of @size bytes aligned at @align from reserved
 * percpu area if arch has set it up; otherwise, allocation is served
 * from the same dynamic area.  Might sleep.  Might trigger writeouts.
 *
 * RETURNS:
 * Percpu pointer to the allocated area on success, NULL on failure.
 */
void *__alloc_reserved_percpu(size_t size, size_t align)
{
	return pcpu_alloc(size, align, true);
}

static void pcpu_kill_chunk(struct pcpu_chunk *chunk)
{
	WARN_ON(chunk->immutable);
	pcpu_depopulate_chunk(chunk, 0, pcpu_unit_size, false);
	list_del(&chunk->list);
	rb_erase(&chunk->rb_node, &pcpu_addr_root);
	free_pcpu_chunk(chunk);
}

/**
 * free_percpu - free percpu area
 * @ptr: pointer to area to free
 *
 * Free percpu area @ptr.  Might sleep.
 */
void free_percpu(void *ptr)
{
	void *addr = __pcpu_ptr_to_addr(ptr);
	struct pcpu_chunk *chunk;
	int off;

	if (!ptr)
		return;

	mutex_lock(&pcpu_mutex);

	chunk = pcpu_chunk_addr_search(addr);
	off = addr - chunk->vm->addr;

	pcpu_free_area(chunk, off);

	/* the chunk became fully free, kill one if there are other free ones */
	if (chunk->free_size == pcpu_unit_size) {
		struct pcpu_chunk *pos;

		list_for_each_entry(pos,
				    &pcpu_slot[pcpu_chunk_slot(chunk)], list)
			if (pos != chunk) {
				pcpu_kill_chunk(pos);
				break;
			}
	}

	mutex_unlock(&pcpu_mutex);
}
EXPORT_SYMBOL_GPL(free_percpu);

/**
 * pcpu_setup_first_chunk - initialize the first percpu chunk
 * @get_page_fn: callback to fetch page pointer
 * @static_size: the size of static percpu area in bytes
 * @reserved_size: the size of reserved percpu area in bytes
 * @unit_size: unit size in bytes, must be multiple of PAGE_SIZE, -1 for auto
 * @dyn_size: free size for dynamic allocation in bytes, -1 for auto
 * @base_addr: mapped address, NULL for auto
 * @populate_pte_fn: callback to allocate pagetable, NULL if unnecessary
 *
 * Initialize the first percpu chunk which contains the kernel static
 * perpcu area.  This function is to be called from arch percpu area
 * setup path.  The first two parameters are mandatory.  The rest are
 * optional.
 *
 * @get_page_fn() should return pointer to percpu page given cpu
 * number and page number.  It should at least return enough pages to
 * cover the static area.  The returned pages for static area should
 * have been initialized with valid data.  If @unit_size is specified,
 * it can also return pages after the static area.  NULL return
 * indicates end of pages for the cpu.  Note that @get_page_fn() must
 * return the same number of pages for all cpus.
 *
 * @reserved_size, if non-zero, specifies the amount of bytes to
 * reserve after the static area in the first chunk.  This reserves
 * the first chunk such that it's available only through reserved
 * percpu allocation.  This is primarily used to serve module percpu
 * static areas on architectures where the addressing model has
 * limited offset range for symbol relocations to guarantee module
 * percpu symbols fall inside the relocatable range.
 *
 * @unit_size, if non-negative, specifies unit size and must be
 * aligned to PAGE_SIZE and equal to or larger than @static_size +
 * @reserved_size + @dyn_size.
 *
 * @dyn_size, if non-negative, limits the number of bytes available
 * for dynamic allocation in the first chunk.  Specifying non-negative
 * value make percpu leave alone the area beyond @static_size +
 * @reserved_size + @dyn_size.
 *
 * Non-null @base_addr means that the caller already allocated virtual
 * region for the first chunk and mapped it.  percpu must not mess
 * with the chunk.  Note that @base_addr with 0 @unit_size or non-NULL
 * @populate_pte_fn doesn't make any sense.
 *
 * @populate_pte_fn is used to populate the pagetable.  NULL means the
 * caller already populated the pagetable.
 *
 * If the first chunk ends up with both reserved and dynamic areas, it
 * is served by two chunks - one to serve the core static and reserved
 * areas and the other for the dynamic area.  They share the same vm
 * and page map but uses different area allocation map to stay away
 * from each other.  The latter chunk is circulated in the chunk slots
 * and available for dynamic allocation like any other chunks.
 *
 * RETURNS:
 * The determined pcpu_unit_size which can be used to initialize
 * percpu access.
 */
size_t __init pcpu_setup_first_chunk(pcpu_get_page_fn_t get_page_fn,
				     size_t static_size, size_t reserved_size,
				     ssize_t unit_size, ssize_t dyn_size,
				     void *base_addr,
				     pcpu_populate_pte_fn_t populate_pte_fn)
{
	static struct vm_struct first_vm;
	static int smap[2], dmap[2];
	struct pcpu_chunk *schunk, *dchunk = NULL;
	unsigned int cpu;
	int nr_pages;
	int err, i;

	/* santiy checks */
	BUILD_BUG_ON(ARRAY_SIZE(smap) >= PCPU_DFL_MAP_ALLOC ||
		     ARRAY_SIZE(dmap) >= PCPU_DFL_MAP_ALLOC);
	BUG_ON(!static_size);
	if (unit_size >= 0) {
		BUG_ON(unit_size < static_size + reserved_size +
				   (dyn_size >= 0 ? dyn_size : 0));
		BUG_ON(unit_size & ~PAGE_MASK);
	} else {
		BUG_ON(dyn_size >= 0);
		BUG_ON(base_addr);
	}
	BUG_ON(base_addr && populate_pte_fn);

	if (unit_size >= 0)
		pcpu_unit_pages = unit_size >> PAGE_SHIFT;
	else
		pcpu_unit_pages = max_t(int, PCPU_MIN_UNIT_SIZE >> PAGE_SHIFT,
					PFN_UP(static_size + reserved_size));

	pcpu_unit_size = pcpu_unit_pages << PAGE_SHIFT;
	pcpu_chunk_size = num_possible_cpus() * pcpu_unit_size;
	pcpu_chunk_struct_size = sizeof(struct pcpu_chunk)
		+ num_possible_cpus() * pcpu_unit_pages * sizeof(struct page *);

	if (dyn_size < 0)
		dyn_size = pcpu_unit_size - static_size - reserved_size;

	/*
	 * Allocate chunk slots.  The additional last slot is for
	 * empty chunks.
	 */
	pcpu_nr_slots = __pcpu_size_to_slot(pcpu_unit_size) + 2;
	pcpu_slot = alloc_bootmem(pcpu_nr_slots * sizeof(pcpu_slot[0]));
	for (i = 0; i < pcpu_nr_slots; i++)
		INIT_LIST_HEAD(&pcpu_slot[i]);

	/*
	 * Initialize static chunk.  If reserved_size is zero, the
	 * static chunk covers static area + dynamic allocation area
	 * in the first chunk.  If reserved_size is not zero, it
	 * covers static area + reserved area (mostly used for module
	 * static percpu allocation).
	 */
	schunk = alloc_bootmem(pcpu_chunk_struct_size);
	INIT_LIST_HEAD(&schunk->list);
	schunk->vm = &first_vm;
	schunk->map = smap;
	schunk->map_alloc = ARRAY_SIZE(smap);
	schunk->page = schunk->page_ar;

	if (reserved_size) {
		schunk->free_size = reserved_size;
		pcpu_reserved_chunk = schunk;	/* not for dynamic alloc */
	} else {
		schunk->free_size = dyn_size;
		dyn_size = 0;			/* dynamic area covered */
	}
	schunk->contig_hint = schunk->free_size;

	schunk->map[schunk->map_used++] = -static_size;
	if (schunk->free_size)
		schunk->map[schunk->map_used++] = schunk->free_size;

	pcpu_reserved_chunk_limit = static_size + schunk->free_size;

	/* init dynamic chunk if necessary */
	if (dyn_size) {
		dchunk = alloc_bootmem(sizeof(struct pcpu_chunk));
		INIT_LIST_HEAD(&dchunk->list);
		dchunk->vm = &first_vm;
		dchunk->map = dmap;
		dchunk->map_alloc = ARRAY_SIZE(dmap);
		dchunk->page = schunk->page_ar;	/* share page map with schunk */

		dchunk->contig_hint = dchunk->free_size = dyn_size;
		dchunk->map[dchunk->map_used++] = -pcpu_reserved_chunk_limit;
		dchunk->map[dchunk->map_used++] = dchunk->free_size;
	}

	/* allocate vm address */
	first_vm.flags = VM_ALLOC;
	first_vm.size = pcpu_chunk_size;

	if (!base_addr)
		vm_area_register_early(&first_vm, PAGE_SIZE);
	else {
		/*
		 * Pages already mapped.  No need to remap into
		 * vmalloc area.  In this case the first chunks can't
		 * be mapped or unmapped by percpu and are marked
		 * immutable.
		 */
		first_vm.addr = base_addr;
		schunk->immutable = true;
		if (dchunk)
			dchunk->immutable = true;
	}

	/* assign pages */
	nr_pages = -1;
	for_each_possible_cpu(cpu) {
		for (i = 0; i < pcpu_unit_pages; i++) {
			struct page *page = get_page_fn(cpu, i);

			if (!page)
				break;
			*pcpu_chunk_pagep(schunk, cpu, i) = page;
		}

		BUG_ON(i < PFN_UP(static_size));

		if (nr_pages < 0)
			nr_pages = i;
		else
			BUG_ON(nr_pages != i);
	}

	/* map them */
	if (populate_pte_fn) {
		for_each_possible_cpu(cpu)
			for (i = 0; i < nr_pages; i++)
				populate_pte_fn(pcpu_chunk_addr(schunk,
								cpu, i));

		err = pcpu_map(schunk, 0, nr_pages);
		if (err)
			panic("failed to setup static percpu area, err=%d\n",
			      err);
	}

	/* link the first chunk in */
	if (!dchunk) {
		pcpu_chunk_relocate(schunk, -1);
		pcpu_chunk_addr_insert(schunk);
	} else {
		pcpu_chunk_relocate(dchunk, -1);
		pcpu_chunk_addr_insert(dchunk);
	}

	/* we're done */
	pcpu_base_addr = (void *)pcpu_chunk_addr(schunk, 0, 0);
	return pcpu_unit_size;
}
