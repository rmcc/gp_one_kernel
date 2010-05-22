/*
 * Copyright (C) 2008, Mathieu Desnoyers
 *
 * SPARC64 definitions for tracing system
 */

#ifndef _ASM_SPARC_LTT_H
#define _ASM_SPARC_LTT_H

#include <linux/timex.h>

static inline u32 ltt_get_timestamp32(void)
{
	return get_cycles();
}

static inline u64 ltt_get_timestamp64(void)
{
	return get_cycles();
}

static inline void ltt_add_timestamp(unsigned long ticks)
{ }

static inline unsigned int ltt_frequency(void)
{
	return CLOCK_TICK_RATE;
}

static inline u32 ltt_freq_scale(void)
{
	return 1;
}

#endif /* _ASM_SPARC_LTT_H */
