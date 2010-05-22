/*
 * linux/include/linux/ltt-relay.h
 *
 * Copyright (C) 2002, 2003 - Tom Zanussi (zanussi@us.ibm.com), IBM Corp
 * Copyright (C) 1999, 2000, 2001, 2002 - Karim Yaghmour (karim@opersys.com)
 * Copyright (C) 2008 - Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * CONFIG_RELAY definitions and declarations
 */

#ifndef _LINUX_LTT_RELAY_H
#define _LINUX_LTT_RELAY_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/kref.h>
#include <linux/mm.h>

/* Needs a _much_ better name... */
#define FIX_SIZE(x) ((((x) - 1) & PAGE_MASK) + PAGE_SIZE)

/*
 * Tracks changes to rchan/rchan_buf structs
 */
#define LTT_RELAY_CHANNEL_VERSION		8

struct rchan_buf;

struct buf_page {
	struct page *page;
	struct rchan_buf *buf;	/* buffer the page belongs to */
	size_t offset;		/* page offset in the buffer */
	struct list_head list;	/* buffer linked list */
};

/*
 * Per-cpu relay channel buffer
 */
struct rchan_buf {
	struct rchan *chan;		/* associated channel */
	wait_queue_head_t read_wait;	/* reader wait queue */
	struct timer_list timer; 	/* reader wake-up timer */
	struct dentry *dentry;		/* channel file dentry */
	struct kref kref;		/* channel buffer refcount */
	struct list_head pages;		/* list of buffer pages */
	struct buf_page *wpage;		/* current write page (cache) */
	struct buf_page *hpage[2];	/* current subbuf header page (cache) */
	struct buf_page *rpage;		/* current subbuf read page (cache) */
	unsigned int page_count;	/* number of current buffer pages */
	unsigned int finalized;		/* buffer has been finalized */
	unsigned int cpu;		/* this buf's cpu */
} ____cacheline_aligned;

/*
 * Relay channel data structure
 */
struct rchan {
	u32 version;			/* the version of this struct */
	size_t subbuf_size;		/* sub-buffer size */
	size_t n_subbufs;		/* number of sub-buffers per buffer */
	size_t alloc_size;		/* total buffer size allocated */
	struct rchan_callbacks *cb;	/* client callbacks */
	struct kref kref;		/* channel refcount */
	void *private_data;		/* for user-defined data */
	struct rchan_buf *buf[NR_CPUS]; /* per-cpu channel buffers */
	struct list_head list;		/* for channel list */
	struct dentry *parent;		/* parent dentry passed to open */
	int subbuf_size_order;		/* order of sub-buffer size */
	char base_filename[NAME_MAX];	/* saved base filename */
};

/*
 * Relay channel client callbacks
 */
struct rchan_callbacks {
	/*
	 * subbuf_start - called on buffer-switch to a new sub-buffer
	 * @buf: the channel buffer containing the new sub-buffer
	 * @subbuf: the start of the new sub-buffer
	 * @prev_subbuf: the start of the previous sub-buffer
	 * @prev_padding: unused space at the end of previous sub-buffer
	 *
	 * The client should return 1 to continue logging, 0 to stop
	 * logging.
	 *
	 * NOTE: subbuf_start will also be invoked when the buffer is
	 *       created, so that the first sub-buffer can be initialized
	 *       if necessary.  In this case, prev_subbuf will be NULL.
	 *
	 * NOTE: the client can reserve bytes at the beginning of the new
	 *       sub-buffer by calling subbuf_start_reserve() in this callback.
	 */
	int (*subbuf_start) (struct rchan_buf *buf,
			     void *subbuf,
			     void *prev_subbuf,
			     size_t prev_padding);

	/*
	 * create_buf_file - create file to represent a relay channel buffer
	 * @filename: the name of the file to create
	 * @parent: the parent of the file to create
	 * @mode: the mode of the file to create
	 * @buf: the channel buffer
	 *
	 * Called during relay_open(), once for each per-cpu buffer,
	 * to allow the client to create a file to be used to
	 * represent the corresponding channel buffer.  If the file is
	 * created outside of relay, the parent must also exist in
	 * that filesystem.
	 *
	 * The callback should return the dentry of the file created
	 * to represent the relay buffer.
	 *
	 * Setting the is_global outparam to a non-zero value will
	 * cause relay_open() to create a single global buffer rather
	 * than the default set of per-cpu buffers.
	 *
	 * See Documentation/filesystems/relayfs.txt for more info.
	 */
	struct dentry *(*create_buf_file)(const char *filename,
					  struct dentry *parent,
					  int mode,
					  struct rchan_buf *buf);

	/*
	 * remove_buf_file - remove file representing a relay channel buffer
	 * @dentry: the dentry of the file to remove
	 *
	 * Called during relay_close(), once for each per-cpu buffer,
	 * to allow the client to remove a file used to represent a
	 * channel buffer.
	 *
	 * The callback should return 0 if successful, negative if not.
	 */
	int (*remove_buf_file)(struct dentry *dentry);
};

/*
 * Start iteration at the previous element. Skip the real list head.
 */
static inline struct buf_page *ltt_relay_find_prev_page(struct rchan_buf *buf,
	struct buf_page *page, size_t offset, ssize_t diff_offset)
{
	struct buf_page *iter;
	size_t orig_iter_off;
	unsigned int i = 0;

	orig_iter_off = page->offset;
	list_for_each_entry_reverse(iter, &page->list, list) {
		/*
		 * Skip the real list head.
		 */
		if (&iter->list == &buf->pages)
			continue;
		i++;
		if (offset >= iter->offset
			&& offset < iter->offset + PAGE_SIZE) {
#ifdef CONFIG_LTT_RELAY_CHECK_RANDOM_ACCESS
			if (i > 1) {
				printk(KERN_WARNING
					"Backward random access detected in "
					"ltt_relay. Iterations %u, "
					"offset %zu, orig iter->off %zu, "
					"iter->off %zu diff_offset %zd.\n", i,
					offset, orig_iter_off, iter->offset,
					diff_offset);
				WARN_ON(1);
			}
#endif
			return iter;
		}
	}
	return NULL;
}

/*
 * Start iteration at the next element. Skip the real list head.
 */
static inline struct buf_page *ltt_relay_find_next_page(struct rchan_buf *buf,
	struct buf_page *page, size_t offset, ssize_t diff_offset)
{
	struct buf_page *iter;
	unsigned int i = 0;
	size_t orig_iter_off;

	orig_iter_off = page->offset;
	list_for_each_entry(iter, &page->list, list) {
		/*
		 * Skip the real list head.
		 */
		if (&iter->list == &buf->pages)
			continue;
		i++;
		if (offset >= iter->offset
			&& offset < iter->offset + PAGE_SIZE) {
#ifdef CONFIG_LTT_RELAY_CHECK_RANDOM_ACCESS
			if (i > 1) {
				printk(KERN_WARNING
					"Forward random access detected in "
					"ltt_relay. Iterations %u, "
					"offset %zu, orig iter->off %zu, "
					"iter->off %zu diff_offset %zd.\n", i,
					offset, orig_iter_off, iter->offset,
					diff_offset);
				WARN_ON(1);
			}
#endif
			return iter;
		}
	}
	return NULL;
}

/*
 * Find the page containing "offset". Cache it if it is after the currently
 * cached page.
 */
static inline struct buf_page *ltt_relay_cache_page(struct rchan_buf *buf,
		struct buf_page **page_cache,
		struct buf_page *page, size_t offset)
{
	ssize_t diff_offset;
	ssize_t half_buf_size = buf->chan->alloc_size >> 1;

	/*
	 * Make sure this is the page we want to write into. The current
	 * page is changed concurrently by other writers. [wrh]page are
	 * used as a cache remembering the last page written
	 * to/read/looked up for header address. No synchronization;
	 * could have to find the previous page is a nested write
	 * occured. Finding the right page is done by comparing the
	 * dest_offset with the buf_page offsets.
	 * When at the exact opposite of the buffer, bias towards forward search
	 * because it will be cached.
	 */

	diff_offset = (ssize_t)offset - (ssize_t)page->offset;
	if (diff_offset <= -(ssize_t)half_buf_size)
		diff_offset += buf->chan->alloc_size;
	else if (diff_offset > half_buf_size)
		diff_offset -= buf->chan->alloc_size;

	if (unlikely(diff_offset >= (ssize_t)PAGE_SIZE)) {
		page = ltt_relay_find_next_page(buf, page, offset, diff_offset);
		WARN_ON(!page);
		*page_cache = page;
	} else if (unlikely(diff_offset < 0)) {
		page = ltt_relay_find_prev_page(buf, page, offset, diff_offset);
		WARN_ON(!page);
	}
	return page;
}

static inline int ltt_relay_write(struct rchan_buf *buf, size_t offset,
	const void *src, size_t len)
{
	struct buf_page *page;
	ssize_t pagecpy, orig_len;

	orig_len = len;
	offset &= buf->chan->alloc_size - 1;
	page = buf->wpage;
	if (unlikely(!len))
		return 0;
	for (;;) {
		page = ltt_relay_cache_page(buf, &buf->wpage, page, offset);
		pagecpy = min_t(size_t, len, PAGE_SIZE - (offset & ~PAGE_MASK));
		memcpy(page_address(page->page)
			+ (offset & ~PAGE_MASK), src, pagecpy);
		len -= pagecpy;
		if (likely(!len))
			break;
		src += pagecpy;
		offset += pagecpy;
		/*
		 * Underlying layer should never ask for writes across
		 * subbuffers.
		 */
		WARN_ON(offset >= buf->chan->alloc_size);
	}
	return orig_len;
}

static inline int ltt_relay_read(struct rchan_buf *buf, size_t offset,
	void *dest, size_t len)
{
	struct buf_page *page;
	ssize_t pagecpy, orig_len;

	orig_len = len;
	offset &= buf->chan->alloc_size - 1;
	page = buf->rpage;
	if (unlikely(!len))
		return 0;
	for (;;) {
		page = ltt_relay_cache_page(buf, &buf->rpage, page, offset);
		pagecpy = min_t(size_t, len, PAGE_SIZE - (offset & ~PAGE_MASK));
		memcpy(dest, page_address(page->page) + (offset & ~PAGE_MASK),
			pagecpy);
		len -= pagecpy;
		if (likely(!len))
			break;
		dest += pagecpy;
		offset += pagecpy;
		/*
		 * Underlying layer should never ask for reads across
		 * subbuffers.
		 */
		WARN_ON(offset >= buf->chan->alloc_size);
	}
	return orig_len;
}

static inline struct buf_page *ltt_relay_read_get_page(struct rchan_buf *buf,
	size_t offset)
{
	struct buf_page *page;

	offset &= buf->chan->alloc_size - 1;
	page = buf->rpage;
	page = ltt_relay_cache_page(buf, &buf->rpage, page, offset);
	return page;
}

/*
 * Return the address where a given offset is located.
 * Should be used to get the current subbuffer header pointer. Given we know
 * it's never on a page boundary, it's safe to write directly to this address,
 * as long as the write is never bigger than a page size.
 */
static inline void *ltt_relay_offset_address(struct rchan_buf *buf,
	size_t offset)
{
	struct buf_page *page;
	unsigned int odd;

	offset &= buf->chan->alloc_size - 1;
	odd = !!(offset & buf->chan->subbuf_size);
	page = buf->hpage[odd];
	if (offset < page->offset || offset >= page->offset + PAGE_SIZE)
		buf->hpage[odd] = page = buf->wpage;
	page = ltt_relay_cache_page(buf, &buf->hpage[odd], page, offset);
	return page_address(page->page) + (offset & ~PAGE_MASK);
}

/*
 * CONFIG_LTT_RELAY kernel API, ltt/ltt-relay-alloc.c
 */

struct rchan *ltt_relay_open(const char *base_filename,
			 struct dentry *parent,
			 size_t subbuf_size,
			 size_t n_subbufs,
			 struct rchan_callbacks *cb,
			 void *private_data);
extern void ltt_relay_close(struct rchan *chan);

/*
 * exported ltt_relay file operations, ltt/ltt-relay-alloc.c
 */
extern const struct file_operations ltt_relay_file_operations;

#endif /* _LINUX_LTT_RELAY_H */

