#ifndef _TRACE_BLOCK_H
#define _TRACE_BLOCK_H

#include <linux/blkdev.h>
#include <linux/tracepoint.h>

DEFINE_TRACE(block_rq,
	TPPROTO(struct request_queue *q, struct request *rq, u32 what),
	TPARGS(q, rq, what));
DEFINE_TRACE(block_bio,
	TPPROTO(struct request_queue *q, struct bio *bio, u32 what),
	TPARGS(q, bio, what));
DEFINE_TRACE(block_generic,
	TPPROTO(struct request_queue *q, struct bio *bio, int rw, u32 what),
	TPARGS(q, bio, rw, what));
DEFINE_TRACE(block_pdu_int,
	TPPROTO(struct request_queue *q, u32 what, struct bio *bio,
		unsigned int pdu),
	TPARGS(q, what, bio, pdu));
DEFINE_TRACE(block_remap,
	TPPROTO(struct request_queue *q, struct bio *bio, dev_t dev,
		sector_t from, sector_t to),
	TPARGS(q, bio, dev, from, to));

#endif
