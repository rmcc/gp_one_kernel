/*
 * Copyright (C) 2005, Mathieu Desnoyers
 *
 * MIPS definitions for tracing system
 */

#ifndef _ASM_MIPS_LTT_H
#define _ASM_MIPS_LTT_H

#include <linux/timex.h>
#include <asm/processor.h>

extern u64 ltt_read_synthetic_tsc(void);

/*
 * MIPS get_cycles only returns a 32 bits TSC (see timex.h). The assumption
 * there is that the reschedule is done every 8 seconds or so. Given that
 * tracing needs to detect delays longer than 8 seconds, we need a full 64-bits
 * TSC, whic is provided by LTTng syntheric TSC.
*/
static inline u32 ltt_get_timestamp32(void)
{
	return get_cycles();
}

static inline u64 ltt_get_timestamp64(void)
{
	return ltt_read_synthetic_tsc();
}

static inline void ltt_add_timestamp(unsigned long ticks)
{ }

static inline unsigned int ltt_frequency(void)
{
	return mips_hpt_frequency;
}

static inline u32 ltt_freq_scale(void)
{
	return 1;
}

#endif /* _ASM_MIPS_LTT_H */
