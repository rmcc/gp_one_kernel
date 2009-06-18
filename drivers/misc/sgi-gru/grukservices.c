/*
 * SN Platform GRU Driver
 *
 *              KERNEL SERVICES THAT USE THE GRU
 *
 *  Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include "gru.h"
#include "grulib.h"
#include "grutables.h"
#include "grukservices.h"
#include "gru_instructions.h"
#include <asm/uv/uv_hub.h>

/*
 * Kernel GRU Usage
 *
 * The following is an interim algorithm for management of kernel GRU
 * resources. This will likely be replaced when we better understand the
 * kernel/user requirements.
 *
 * Blade percpu resources reserved for kernel use. These resources are
 * reserved whenever the the kernel context for the blade is loaded. Note
 * that the kernel context is not guaranteed to be always available. It is
 * loaded on demand & can be stolen by a user if the user demand exceeds the
 * kernel demand. The kernel can always reload the kernel context but
 * a SLEEP may be required!!!.
 */
#define GRU_NUM_KERNEL_CBR	1
#define GRU_NUM_KERNEL_DSR_BYTES 256
#define GRU_NUM_KERNEL_DSR_CL	(GRU_NUM_KERNEL_DSR_BYTES /		\
					GRU_CACHE_LINE_BYTES)

/* GRU instruction attributes for all instructions */
#define IMA			IMA_CB_DELAY

/* GRU cacheline size is always 64 bytes - even on arches with 128 byte lines */
#define __gru_cacheline_aligned__                               \
	__attribute__((__aligned__(GRU_CACHE_LINE_BYTES)))

#define MAGIC	0x1234567887654321UL

/* Default retry count for GRU errors on kernel instructions */
#define EXCEPTION_RETRY_LIMIT	3

/* Status of message queue sections */
#define MQS_EMPTY		0
#define MQS_FULL		1
#define MQS_NOOP		2

/*----------------- RESOURCE MANAGEMENT -------------------------------------*/
/* optimized for x86_64 */
struct message_queue {
	union gru_mesqhead	head __gru_cacheline_aligned__;	/* CL 0 */
	int			qlines;				/* DW 1 */
	long 			hstatus[2];
	void 			*next __gru_cacheline_aligned__;/* CL 1 */
	void 			*limit;
	void 			*start;
	void 			*start2;
	char			data ____cacheline_aligned;	/* CL 2 */
};

/* First word in every message - used by mesq interface */
struct message_header {
	char	present;
	char	present2;
	char 	lines;
	char	fill;
};

#define HSTATUS(mq, h)	((mq) + offsetof(struct message_queue, hstatus[h]))

/*
 * Allocate a kernel context (GTS) for the specified blade.
 * 	- protected by writelock on bs_kgts_sema.
 */
static void gru_alloc_kernel_context(struct gru_blade_state *bs, int blade_id)
{
	int cbr_au_count, dsr_au_count, ncpus;

	ncpus = uv_blade_nr_possible_cpus(blade_id);
	cbr_au_count = GRU_CB_COUNT_TO_AU(GRU_NUM_KERNEL_CBR * ncpus);
	dsr_au_count = GRU_DS_BYTES_TO_AU(GRU_NUM_KERNEL_DSR_BYTES * ncpus);
	bs->bs_kgts = gru_alloc_gts(NULL, cbr_au_count, dsr_au_count, 0, 0);
}

/*
 * Reload the blade's kernel context into a GRU chiplet. Called holding
 * the bs_kgts_sema for READ. Will steal user contexts if necessary.
 */
static void gru_load_kernel_context(struct gru_blade_state *bs, int blade_id)
{
	struct gru_state *gru;
	struct gru_thread_state *kgts;
	void *vaddr;
	int ctxnum;

	up_read(&bs->bs_kgts_sema);
	down_write(&bs->bs_kgts_sema);

	if (!bs->bs_kgts)
		gru_alloc_kernel_context(bs, blade_id);
	kgts = bs->bs_kgts;

	if (!kgts->ts_gru) {
		STAT(load_kernel_context);
		while (!gru_assign_gru_context(kgts, blade_id)) {
			msleep(1);
			gru_steal_context(kgts, blade_id);
		}
		gru_load_context(kgts);
		gru = bs->bs_kgts->ts_gru;
		vaddr = gru->gs_gru_base_vaddr;
		ctxnum = kgts->ts_ctxnum;
		bs->kernel_cb = get_gseg_base_address_cb(vaddr, ctxnum, 0);
		bs->kernel_dsr = get_gseg_base_address_ds(vaddr, ctxnum, 0);
	}
	downgrade_write(&bs->bs_kgts_sema);
}

/*
 * Lock & load the kernel context for the specified blade.
 */
static struct gru_blade_state *gru_lock_kernel_context(int blade_id)
{
	struct gru_blade_state *bs;

	STAT(lock_kernel_context);
	bs = gru_base[blade_id];

	down_read(&bs->bs_kgts_sema);
	if (!bs->bs_kgts || !bs->bs_kgts->ts_gru)
		gru_load_kernel_context(bs, blade_id);
	return bs;

}

/*
 * Unlock the kernel context for the specified blade. Context is not
 * unloaded but may be stolen before next use.
 */
static void gru_unlock_kernel_context(int blade_id)
{
	struct gru_blade_state *bs;

	bs = gru_base[blade_id];
	up_read(&bs->bs_kgts_sema);
	STAT(unlock_kernel_context);
}

/*
 * Reserve & get pointers to the DSR/CBRs reserved for the current cpu.
 * 	- returns with preemption disabled
 */
static int gru_get_cpu_resources(int dsr_bytes, void **cb, void **dsr)
{
	struct gru_blade_state *bs;
	int lcpu;

	BUG_ON(dsr_bytes > GRU_NUM_KERNEL_DSR_BYTES);
	preempt_disable();
	bs = gru_lock_kernel_context(uv_numa_blade_id());
	lcpu = uv_blade_processor_id();
	*cb = bs->kernel_cb + lcpu * GRU_HANDLE_STRIDE;
	*dsr = bs->kernel_dsr + lcpu * GRU_NUM_KERNEL_DSR_BYTES;
	return 0;
}

/*
 * Free the current cpus reserved DSR/CBR resources.
 */
static void gru_free_cpu_resources(void *cb, void *dsr)
{
	gru_unlock_kernel_context(uv_numa_blade_id());
	preempt_enable();
}

/*----------------------------------------------------------------------*/
int gru_get_cb_exception_detail(void *cb,
		struct control_block_extended_exc_detail *excdet)
{
	struct gru_control_block_extended *cbe;

	cbe = get_cbe(GRUBASE(cb), get_cb_number(cb));
	prefetchw(cbe);	/* Harmless on hardware, required for emulator */
	excdet->opc = cbe->opccpy;
	excdet->exopc = cbe->exopccpy;
	excdet->ecause = cbe->ecause;
	excdet->exceptdet0 = cbe->idef1upd;
	excdet->exceptdet1 = cbe->idef3upd;
	return 0;
}

char *gru_get_cb_exception_detail_str(int ret, void *cb,
				      char *buf, int size)
{
	struct gru_control_block_status *gen = (void *)cb;
	struct control_block_extended_exc_detail excdet;

	if (ret > 0 && gen->istatus == CBS_EXCEPTION) {
		gru_get_cb_exception_detail(cb, &excdet);
		snprintf(buf, size,
			"GRU exception: cb %p, opc %d, exopc %d, ecause 0x%x,"
			"excdet0 0x%lx, excdet1 0x%x",
			gen, excdet.opc, excdet.exopc, excdet.ecause,
			excdet.exceptdet0, excdet.exceptdet1);
	} else {
		snprintf(buf, size, "No exception");
	}
	return buf;
}

static int gru_wait_idle_or_exception(struct gru_control_block_status *gen)
{
	while (gen->istatus >= CBS_ACTIVE) {
		cpu_relax();
		barrier();
	}
	return gen->istatus;
}

static int gru_retry_exception(void *cb)
{
	struct gru_control_block_status *gen = (void *)cb;
	struct control_block_extended_exc_detail excdet;
	int retry = EXCEPTION_RETRY_LIMIT;

	while (1)  {
		if (gru_get_cb_message_queue_substatus(cb))
			break;
		if (gru_wait_idle_or_exception(gen) == CBS_IDLE)
			return CBS_IDLE;

		gru_get_cb_exception_detail(cb, &excdet);
		if (excdet.ecause & ~EXCEPTION_RETRY_BITS)
			break;
		if (retry-- == 0)
			break;
		gen->icmd = 1;
		gru_flush_cache(gen);
	}
	return CBS_EXCEPTION;
}

int gru_check_status_proc(void *cb)
{
	struct gru_control_block_status *gen = (void *)cb;
	int ret;

	ret = gen->istatus;
	if (ret != CBS_EXCEPTION)
		return ret;
	return gru_retry_exception(cb);

}

int gru_wait_proc(void *cb)
{
	struct gru_control_block_status *gen = (void *)cb;
	int ret;

	ret = gru_wait_idle_or_exception(gen);
	if (ret == CBS_EXCEPTION)
		ret = gru_retry_exception(cb);

	return ret;
}

void gru_abort(int ret, void *cb, char *str)
{
	char buf[GRU_EXC_STR_SIZE];

	panic("GRU FATAL ERROR: %s - %s\n", str,
	      gru_get_cb_exception_detail_str(ret, cb, buf, sizeof(buf)));
}

void gru_wait_abort_proc(void *cb)
{
	int ret;

	ret = gru_wait_proc(cb);
	if (ret)
		gru_abort(ret, cb, "gru_wait_abort");
}


/*------------------------------ MESSAGE QUEUES -----------------------------*/

/* Internal status . These are NOT returned to the user. */
#define MQIE_AGAIN		-1	/* try again */


/*
 * Save/restore the "present" flag that is in the second line of 2-line
 * messages
 */
static inline int get_present2(void *p)
{
	struct message_header *mhdr = p + GRU_CACHE_LINE_BYTES;
	return mhdr->present;
}

static inline void restore_present2(void *p, int val)
{
	struct message_header *mhdr = p + GRU_CACHE_LINE_BYTES;
	mhdr->present = val;
}

/*
 * Create a message queue.
 * 	qlines - message queue size in cache lines. Includes 2-line header.
 */
int gru_create_message_queue(struct gru_message_queue_desc *mqd,
		void *p, unsigned int bytes, int nasid, int vector, int apicid)
{
	struct message_queue *mq = p;
	unsigned int qlines;

	qlines = bytes / GRU_CACHE_LINE_BYTES - 2;
	memset(mq, 0, bytes);
	mq->start = &mq->data;
	mq->start2 = &mq->data + (qlines / 2 - 1) * GRU_CACHE_LINE_BYTES;
	mq->next = &mq->data;
	mq->limit = &mq->data + (qlines - 2) * GRU_CACHE_LINE_BYTES;
	mq->qlines = qlines;
	mq->hstatus[0] = 0;
	mq->hstatus[1] = 1;
	mq->head = gru_mesq_head(2, qlines / 2 + 1);
	mqd->mq = mq;
	mqd->mq_gpa = uv_gpa(mq);
	mqd->qlines = qlines;
	mqd->interrupt_pnode = UV_NASID_TO_PNODE(nasid);
	mqd->interrupt_vector = vector;
	mqd->interrupt_apicid = apicid;
	return 0;
}
EXPORT_SYMBOL_GPL(gru_create_message_queue);

/*
 * Send a NOOP message to a message queue
 * 	Returns:
 * 		 0 - if queue is full after the send. This is the normal case
 * 		     but various races can change this.
 *		-1 - if mesq sent successfully but queue not full
 *		>0 - unexpected error. MQE_xxx returned
 */
static int send_noop_message(void *cb, struct gru_message_queue_desc *mqd,
				void *mesg)
{
	const struct message_header noop_header = {
					.present = MQS_NOOP, .lines = 1};
	unsigned long m;
	int substatus, ret;
	struct message_header save_mhdr, *mhdr = mesg;

	STAT(mesq_noop);
	save_mhdr = *mhdr;
	*mhdr = noop_header;
	gru_mesq(cb, mqd->mq_gpa, gru_get_tri(mhdr), 1, IMA);
	ret = gru_wait(cb);

	if (ret) {
		substatus = gru_get_cb_message_queue_substatus(cb);
		switch (substatus) {
		case CBSS_NO_ERROR:
			STAT(mesq_noop_unexpected_error);
			ret = MQE_UNEXPECTED_CB_ERR;
			break;
		case CBSS_LB_OVERFLOWED:
			STAT(mesq_noop_lb_overflow);
			ret = MQE_CONGESTION;
			break;
		case CBSS_QLIMIT_REACHED:
			STAT(mesq_noop_qlimit_reached);
			ret = 0;
			break;
		case CBSS_AMO_NACKED:
			STAT(mesq_noop_amo_nacked);
			ret = MQE_CONGESTION;
			break;
		case CBSS_PUT_NACKED:
			STAT(mesq_noop_put_nacked);
			m = mqd->mq_gpa + (gru_get_amo_value_head(cb) << 6);
			gru_vstore(cb, m, gru_get_tri(mesg), XTYPE_CL, 1, 1,
						IMA);
			if (gru_wait(cb) == CBS_IDLE)
				ret = MQIE_AGAIN;
			else
				ret = MQE_UNEXPECTED_CB_ERR;
			break;
		case CBSS_PAGE_OVERFLOW:
		default:
			BUG();
		}
	}
	*mhdr = save_mhdr;
	return ret;
}

/*
 * Handle a gru_mesq full.
 */
static int send_message_queue_full(void *cb, struct gru_message_queue_desc *mqd,
				void *mesg, int lines)
{
	union gru_mesqhead mqh;
	unsigned int limit, head;
	unsigned long avalue;
	int half, qlines;

	/* Determine if switching to first/second half of q */
	avalue = gru_get_amo_value(cb);
	head = gru_get_amo_value_head(cb);
	limit = gru_get_amo_value_limit(cb);

	qlines = mqd->qlines;
	half = (limit != qlines);

	if (half)
		mqh = gru_mesq_head(qlines / 2 + 1, qlines);
	else
		mqh = gru_mesq_head(2, qlines / 2 + 1);

	/* Try to get lock for switching head pointer */
	gru_gamir(cb, EOP_IR_CLR, HSTATUS(mqd->mq_gpa, half), XTYPE_DW, IMA);
	if (gru_wait(cb) != CBS_IDLE)
		goto cberr;
	if (!gru_get_amo_value(cb)) {
		STAT(mesq_qf_locked);
		return MQE_QUEUE_FULL;
	}

	/* Got the lock. Send optional NOP if queue not full, */
	if (head != limit) {
		if (send_noop_message(cb, mqd, mesg)) {
			gru_gamir(cb, EOP_IR_INC, HSTATUS(mqd->mq_gpa, half),
					XTYPE_DW, IMA);
			if (gru_wait(cb) != CBS_IDLE)
				goto cberr;
			STAT(mesq_qf_noop_not_full);
			return MQIE_AGAIN;
		}
		avalue++;
	}

	/* Then flip queuehead to other half of queue. */
	gru_gamer(cb, EOP_ERR_CSWAP, mqd->mq_gpa, XTYPE_DW, mqh.val, avalue,
							IMA);
	if (gru_wait(cb) != CBS_IDLE)
		goto cberr;

	/* If not successfully in swapping queue head, clear the hstatus lock */
	if (gru_get_amo_value(cb) != avalue) {
		STAT(mesq_qf_switch_head_failed);
		gru_gamir(cb, EOP_IR_INC, HSTATUS(mqd->mq_gpa, half), XTYPE_DW,
							IMA);
		if (gru_wait(cb) != CBS_IDLE)
			goto cberr;
	}
	return MQIE_AGAIN;
cberr:
	STAT(mesq_qf_unexpected_error);
	return MQE_UNEXPECTED_CB_ERR;
}

/*
 * Send a cross-partition interrupt to the SSI that contains the target
 * message queue. Normally, the interrupt is automatically delivered by hardware
 * but some error conditions require explicit delivery.
 */
static void send_message_queue_interrupt(struct gru_message_queue_desc *mqd)
{
	if (mqd->interrupt_vector)
		uv_hub_send_ipi(mqd->interrupt_pnode, mqd->interrupt_apicid,
				mqd->interrupt_vector);
}

/*
 * Handle a PUT failure. Note: if message was a 2-line message, one of the
 * lines might have successfully have been written. Before sending the
 * message, "present" must be cleared in BOTH lines to prevent the receiver
 * from prematurely seeing the full message.
 */
static int send_message_put_nacked(void *cb, struct gru_message_queue_desc *mqd,
			void *mesg, int lines)
{
	unsigned long m;

	m = mqd->mq_gpa + (gru_get_amo_value_head(cb) << 6);
	if (lines == 2) {
		gru_vset(cb, m, 0, XTYPE_CL, lines, 1, IMA);
		if (gru_wait(cb) != CBS_IDLE)
			return MQE_UNEXPECTED_CB_ERR;
	}
	gru_vstore(cb, m, gru_get_tri(mesg), XTYPE_CL, lines, 1, IMA);
	if (gru_wait(cb) != CBS_IDLE)
		return MQE_UNEXPECTED_CB_ERR;
	send_message_queue_interrupt(mqd);
	return MQE_OK;
}

/*
 * Handle a gru_mesq failure. Some of these failures are software recoverable
 * or retryable.
 */
static int send_message_failure(void *cb, struct gru_message_queue_desc *mqd,
				void *mesg, int lines)
{
	int substatus, ret = 0;

	substatus = gru_get_cb_message_queue_substatus(cb);
	switch (substatus) {
	case CBSS_NO_ERROR:
		STAT(mesq_send_unexpected_error);
		ret = MQE_UNEXPECTED_CB_ERR;
		break;
	case CBSS_LB_OVERFLOWED:
		STAT(mesq_send_lb_overflow);
		ret = MQE_CONGESTION;
		break;
	case CBSS_QLIMIT_REACHED:
		STAT(mesq_send_qlimit_reached);
		ret = send_message_queue_full(cb, mqd, mesg, lines);
		break;
	case CBSS_AMO_NACKED:
		STAT(mesq_send_amo_nacked);
		ret = MQE_CONGESTION;
		break;
	case CBSS_PUT_NACKED:
		STAT(mesq_send_put_nacked);
		ret = send_message_put_nacked(cb, mqd, mesg, lines);
		break;
	default:
		BUG();
	}
	return ret;
}

/*
 * Send a message to a message queue
 * 	mqd	message queue descriptor
 * 	mesg	message. ust be vaddr within a GSEG
 * 	bytes	message size (<= 2 CL)
 */
int gru_send_message_gpa(struct gru_message_queue_desc *mqd, void *mesg,
				unsigned int bytes)
{
	struct message_header *mhdr;
	void *cb;
	void *dsr;
	int istatus, clines, ret;

	STAT(mesq_send);
	BUG_ON(bytes < sizeof(int) || bytes > 2 * GRU_CACHE_LINE_BYTES);

	clines = DIV_ROUND_UP(bytes, GRU_CACHE_LINE_BYTES);
	if (gru_get_cpu_resources(bytes, &cb, &dsr))
		return MQE_BUG_NO_RESOURCES;
	memcpy(dsr, mesg, bytes);
	mhdr = dsr;
	mhdr->present = MQS_FULL;
	mhdr->lines = clines;
	if (clines == 2) {
		mhdr->present2 = get_present2(mhdr);
		restore_present2(mhdr, MQS_FULL);
	}

	do {
		ret = MQE_OK;
		gru_mesq(cb, mqd->mq_gpa, gru_get_tri(mhdr), clines, IMA);
		istatus = gru_wait(cb);
		if (istatus != CBS_IDLE)
			ret = send_message_failure(cb, mqd, dsr, clines);
	} while (ret == MQIE_AGAIN);
	gru_free_cpu_resources(cb, dsr);

	if (ret)
		STAT(mesq_send_failed);
	return ret;
}
EXPORT_SYMBOL_GPL(gru_send_message_gpa);

/*
 * Advance the receive pointer for the queue to the next message.
 */
void gru_free_message(struct gru_message_queue_desc *mqd, void *mesg)
{
	struct message_queue *mq = mqd->mq;
	struct message_header *mhdr = mq->next;
	void *next, *pnext;
	int half = -1;
	int lines = mhdr->lines;

	if (lines == 2)
		restore_present2(mhdr, MQS_EMPTY);
	mhdr->present = MQS_EMPTY;

	pnext = mq->next;
	next = pnext + GRU_CACHE_LINE_BYTES * lines;
	if (next == mq->limit) {
		next = mq->start;
		half = 1;
	} else if (pnext < mq->start2 && next >= mq->start2) {
		half = 0;
	}

	if (half >= 0)
		mq->hstatus[half] = 1;
	mq->next = next;
}
EXPORT_SYMBOL_GPL(gru_free_message);

/*
 * Get next message from message queue. Return NULL if no message
 * present. User must call next_message() to move to next message.
 * 	rmq	message queue
 */
void *gru_get_next_message(struct gru_message_queue_desc *mqd)
{
	struct message_queue *mq = mqd->mq;
	struct message_header *mhdr = mq->next;
	int present = mhdr->present;

	/* skip NOOP messages */
	STAT(mesq_receive);
	while (present == MQS_NOOP) {
		gru_free_message(mqd, mhdr);
		mhdr = mq->next;
		present = mhdr->present;
	}

	/* Wait for both halves of 2 line messages */
	if (present == MQS_FULL && mhdr->lines == 2 &&
				get_present2(mhdr) == MQS_EMPTY)
		present = MQS_EMPTY;

	if (!present) {
		STAT(mesq_receive_none);
		return NULL;
	}

	if (mhdr->lines == 2)
		restore_present2(mhdr, mhdr->present2);

	return mhdr;
}
EXPORT_SYMBOL_GPL(gru_get_next_message);

/* ---------------------- GRU DATA COPY FUNCTIONS ---------------------------*/

/*
 * Copy a block of data using the GRU resources
 */
int gru_copy_gpa(unsigned long dest_gpa, unsigned long src_gpa,
				unsigned int bytes)
{
	void *cb;
	void *dsr;
	int ret;

	STAT(copy_gpa);
	if (gru_get_cpu_resources(GRU_NUM_KERNEL_DSR_BYTES, &cb, &dsr))
		return MQE_BUG_NO_RESOURCES;
	gru_bcopy(cb, src_gpa, dest_gpa, gru_get_tri(dsr),
		  XTYPE_B, bytes, GRU_NUM_KERNEL_DSR_CL, IMA);
	ret = gru_wait(cb);
	gru_free_cpu_resources(cb, dsr);
	return ret;
}
EXPORT_SYMBOL_GPL(gru_copy_gpa);

/* ------------------- KERNEL QUICKTESTS RUN AT STARTUP ----------------*/
/* 	Temp - will delete after we gain confidence in the GRU		*/

int quicktest(void)
{
	unsigned long word0;
	unsigned long word1;
	void *cb;
	void *dsr;
	unsigned long *p;

	if (gru_get_cpu_resources(GRU_CACHE_LINE_BYTES, &cb, &dsr))
		return MQE_BUG_NO_RESOURCES;
	p = dsr;
	word0 = MAGIC;
	word1 = 0;

	gru_vload(cb, uv_gpa(&word0), gru_get_tri(dsr), XTYPE_DW, 1, 1, IMA);
	if (gru_wait(cb) != CBS_IDLE)
		BUG();

	if (*p != MAGIC)
		BUG();
	gru_vstore(cb, uv_gpa(&word1), gru_get_tri(dsr), XTYPE_DW, 1, 1, IMA);
	if (gru_wait(cb) != CBS_IDLE)
		BUG();
	gru_free_cpu_resources(cb, dsr);

	if (word0 != word1 || word1 != MAGIC) {
		printk
		    ("GRU quicktest err: found 0x%lx, expected 0x%lx\n",
		     word1, MAGIC);
		BUG();		/* ZZZ should not be fatal */
	}

	return 0;
}


int gru_kservices_init(struct gru_state *gru)
{
	struct gru_blade_state *bs;

	bs = gru->gs_blade;
	if (gru != &bs->bs_grus[0])
		return 0;

	init_rwsem(&bs->bs_kgts_sema);

	if (gru_options & GRU_QUICKLOOK)
		quicktest();
	return 0;
}

void gru_kservices_exit(struct gru_state *gru)
{
	struct gru_blade_state *bs;
	struct gru_thread_state *kgts;

	bs = gru->gs_blade;
	if (gru != &bs->bs_grus[0])
		return;

	kgts = bs->bs_kgts;
	if (kgts && kgts->ts_gru)
		gru_unload_context(kgts, 0);
	kfree(kgts);
}

