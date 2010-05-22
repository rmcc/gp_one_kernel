/* linux/arch/arm/mach-msm/timer.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009 QUALCOMM USA, INC.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/mach/time.h>
#include <mach/msm_iomap.h>

#include "smd_private.h"
#include "timer.h"

enum {
	MSM_TIMER_DEBUG_SYNC = 1U << 0,
};
#define DEBUG_REBOOT    1
#if DEBUG_REBOOT
//static int paul_printlog = 0;
//static int react_count = 0;
#endif
static int msm_timer_debug_mask;
module_param_named(debug_mask, msm_timer_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

#define WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME     1   //paul
//#define WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG  1
#define MSM_DGT_BASE (MSM_GPT_BASE + 0x10)
#define MSM_DGT_SHIFT (5)

#define TIMER_MATCH_VAL         0x0000
#define TIMER_COUNT_VAL         0x0004
#define TIMER_ENABLE            0x0008
#define TIMER_ENABLE_CLR_ON_MATCH_EN    2
#define TIMER_ENABLE_EN                 1
#define TIMER_CLEAR             0x000C

#define CSR_PROTECTION          0x0020
#define CSR_PROTECTION_EN               1

#define GPT_HZ 32768
#if defined(CONFIG_ARCH_QSD)
#define DGT_HZ 4800000 /* DGT is run with divider of 4 */
#else
#define DGT_HZ 19200000 /* 19.2 MHz or 600 KHz after shift */
#endif
#define SCLK_HZ 32768

static int msm_timer_ready;

enum {
	MSM_CLOCK_FLAGS_UNSTABLE_COUNT = 1U << 0,
	MSM_CLOCK_FLAGS_ODD_MATCH_WRITE = 1U << 1,
	MSM_CLOCK_FLAGS_DELAYED_WRITE_POST = 1U << 2,
};

struct msm_clock {
	struct clock_event_device   clockevent;
	struct clocksource          clocksource;
	struct irqaction            irq;
	void __iomem                *regbase;
	uint32_t                    freq;
	uint32_t                    shift;
	uint32_t                    flags;
	uint32_t                    write_delay;
	uint32_t                    last_set;
#ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
    uint64_t                    offset;
	uint64_t                    alarm_vtime;
	uint64_t                    smem_offset;
#else
	uint32_t                    offset;
	uint32_t                    alarm_vtime;
	uint32_t                    smem_offset;
#endif
	uint32_t                    smem_in_sync;
};
enum {
	MSM_CLOCK_GPT,
	MSM_CLOCK_DGT,
};
static struct msm_clock msm_clocks[];
static struct msm_clock *msm_active_clock;

struct msm_timer_sync_data_t {
	struct msm_clock *clock;
	int64_t          timeout;
	int              exit_sleep;
};

static irqreturn_t msm_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static uint32_t msm_read_timer_count(struct msm_clock *clock)
{
	uint32_t t1, t2;
	int loop_count = 0;

	t1 = readl(clock->regbase + TIMER_COUNT_VAL);
	if (!(clock->flags & MSM_CLOCK_FLAGS_UNSTABLE_COUNT))
		return t1;
	while (1) {
		t2 = readl(clock->regbase + TIMER_COUNT_VAL);
		if (t1 == t2)
			return t1;
		if (loop_count++ > 10) {
			printk(KERN_ERR "msm_read_timer_count timer %s did not"
			       "stabilize %u != %u\n", clock->clockevent.name,
			       t2, t1);
			return t2;
		}
		t1 = t2;
	}
}

static cycle_t msm_gpt_read(void)
{
	struct msm_clock *clock = &msm_clocks[MSM_CLOCK_GPT];
		return msm_read_timer_count(clock) + clock->offset;
}
#ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
    #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG
static cycle_t old_timer[2] = {0,0};
static uint64_t old_timer_count[2] = {0,0};
static uint64_t old_offset[2] = {0,0};
    #endif
#endif

static cycle_t msm_dgt_read(void)
{
	struct msm_clock *clock = &msm_clocks[MSM_CLOCK_DGT];
    #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
    uint64_t timer_count;
    cycle_t timer;
    cycle_t tmp_timer;
    timer_count = msm_read_timer_count(clock);
    timer = (timer_count + clock->offset) >> MSM_DGT_SHIFT;

    if (timer < clock->clocksource.cycle_last)
    {
        #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG
        printk("ntimer=0x%llx t_cnt=0x%llx os=0x%llx cyclast=0x%llx\n", timer, timer_count, clock->offset, clock->clocksource.cycle_last);
        printk("otimer=0x%llx t_cnt=0x%llx os=0x%llx\n", old_timer[0], old_timer_count[0], old_offset[0]);
        //printk("otimer1=0x%llx t_cnt=0x%llx os=0x%llx\n", old_timer[1], old_timer_count[1], old_offset[1]);
        #endif
        if (clock->offset != 0 )
        {
            tmp_timer = (clock->clocksource.cycle_last - timer);
            clock->offset += ((tmp_timer)<< MSM_DGT_SHIFT);
            if (clock->clocksource.cycle_last - timer >= (clock->clocksource.mask - clock->clocksource.cycle_interval))
            {
            }
            else
            {
                timer = clock->clocksource.cycle_last;
            }
            #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG
            printk("clock->offset=0x%llx\n", clock->offset);
            #endif
        }
    }
    #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG
    old_timer[1] = old_timer[0];
    old_timer_count[1] = old_timer_count[0];
    old_offset[1] = old_offset[0];
    old_timer[0] = timer;
    old_timer_count[0] = timer_count;
    old_offset[0] = clock->offset;
    #endif
    return timer;
    #else
	return (msm_read_timer_count(clock) + clock->offset) >> MSM_DGT_SHIFT;
    #endif
}

static int msm_timer_set_next_event(unsigned long cycles,
				    struct clock_event_device *evt)
{
	int i;
	struct msm_clock *clock;
	uint32_t now;
	uint32_t alarm;
	int late;

	clock = container_of(evt, struct msm_clock, clockevent);
	now = msm_read_timer_count(clock);
	alarm = now + (cycles << clock->shift);
	if (clock->flags & MSM_CLOCK_FLAGS_ODD_MATCH_WRITE)
		while (now == clock->last_set)
			now = msm_read_timer_count(clock);
	writel(alarm, clock->regbase + TIMER_MATCH_VAL);
	if (clock->flags & MSM_CLOCK_FLAGS_DELAYED_WRITE_POST) {
		/* read the counter four extra times to make sure write posts
		   before reading the time */
		for (i = 0; i < 4; i++)
			readl(clock->regbase + TIMER_COUNT_VAL);
	}
	now = msm_read_timer_count(clock);
	clock->last_set = now;
	clock->alarm_vtime = (int64_t)(alarm + clock->offset);
	late = now - alarm;
    if (late >= (int)(-clock->write_delay << clock->shift) && late < DGT_HZ*5) 
        {
        
		static int print_limit = 10;
		if (print_limit > 0) {
			print_limit--;
			printk(KERN_NOTICE "msm_timer_set_next_event(%lu) "
			       "clock %s, alarm already expired, now %x, "
			       "alarm %x, late %d%s\n",
			       cycles, clock->clockevent.name, now, alarm, late,
			       print_limit ? "" : " stop printing");
		}
		return -ETIME;
	}
	return 0;
}

static void msm_timer_set_mode(enum clock_event_mode mode,
			      struct clock_event_device *evt)
{
	struct msm_clock *clock;
	clock = container_of(evt, struct msm_clock, clockevent);
	switch (mode) {
	case CLOCK_EVT_MODE_RESUME:
	case CLOCK_EVT_MODE_PERIODIC:
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		msm_active_clock = clock;
		writel(TIMER_ENABLE_EN, clock->regbase + TIMER_ENABLE);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		msm_active_clock = NULL;
		clock->smem_in_sync = 0;
		writel(0, clock->regbase + TIMER_ENABLE);
		break;
	}
}

/*
 * Retrieve the cycle count from the slow clock through SMEM and optionally
 * synchronize local clock(s) with the slow clock.  The function implements
 * the inter-processor time-sync protocol.
 *
 * time_start and time_expired are callbacks that must be specified.  The
 * protocol uses them to detect timeout.  The update callback is optional.
 * If not NULL, update will be called so that it can update local clock(s).
 *
 * The function does not use the argument data directly; it passes data to
 * the callbacks.
 *
 * Return value:
 *      0: the operation failed
 *      >0: the slow clock value after time-sync
 */
#if defined(CONFIG_MSM_N_WAY_SMSM)
static uint32_t msm_timer_sync_sclk(
	void (*time_start)(struct msm_timer_sync_data_t *data),
	bool (*time_expired)(struct msm_timer_sync_data_t *data),
	void (*update)(struct msm_timer_sync_data_t *data, uint32_t clk_val),
	struct msm_timer_sync_data_t *data)
{
	/* Time Master State Bits */
	#define MASTER_BITS_PER_CPU        1
	#define MASTER_TIME_PENDING \
		(0x01UL << (MASTER_BITS_PER_CPU * SMSM_APPS_STATE))

	/* Time Slave State Bits */
	#define SLAVE_TIME_REQUEST         0x0400
	#define SLAVE_TIME_POLL            0x0800
	#define SLAVE_TIME_INIT            0x1000

	uint32_t *smem_clock;
	uint32_t smem_clock_val;
	uint32_t state;

	smem_clock = smem_alloc(SMEM_SMEM_SLOW_CLOCK_VALUE, sizeof(uint32_t));
	if (smem_clock == NULL) {
		printk(KERN_ERR "no smem clock\n");
		return 0;
	}

	state = smsm_get_state(SMSM_MODEM_STATE);
	if ((state & SMSM_INIT) == 0) {
		printk(KERN_ERR "smsm not initialized\n");
		return 0;
	}

	time_start(data);
	while ((state = smsm_get_state(SMSM_TIME_MASTER_DEM)) &
		MASTER_TIME_PENDING) {
		if (time_expired(data)) {
			printk(KERN_INFO "get_smem_clock: timeout 1 still "
				"invalid state %x\n", state);
			return 0;
		}
	}

	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_POLL | SLAVE_TIME_INIT,
		SLAVE_TIME_REQUEST);

	time_start(data);
	while (!((state = smsm_get_state(SMSM_TIME_MASTER_DEM)) &
		MASTER_TIME_PENDING)) {
		if (time_expired(data)) {
			printk(KERN_INFO "get_smem_clock: timeout 2 still "
				"invalid state %x\n", state);
			smem_clock_val = 0;
			goto sync_sclk_exit;
		}
	}

	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_REQUEST, SLAVE_TIME_POLL);

	time_start(data);
	do {
		smem_clock_val = *smem_clock;
	} while (smem_clock_val == 0 && !time_expired(data));

	state = smsm_get_state(SMSM_TIME_MASTER_DEM);

	if (smem_clock_val) {
		if (update != NULL)
			update(data, smem_clock_val);

		if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
			printk(KERN_INFO
				"get_smem_clock: state %x clock %u\n",
				state, smem_clock_val);
	} else {
		printk(KERN_INFO "get_smem_clock: timeout state %x clock %u\n",
			state, smem_clock_val);
	}

sync_sclk_exit:
	smsm_change_state(SMSM_APPS_DEM, SLAVE_TIME_REQUEST | SLAVE_TIME_POLL,
		SLAVE_TIME_INIT);
	return smem_clock_val;
}
#else /* CONFIG_MSM_N_WAY_SMSM */
static uint32_t msm_timer_sync_sclk(
	void (*time_start)(struct msm_timer_sync_data_t *data),
	bool (*time_expired)(struct msm_timer_sync_data_t *data),
	void (*update)(struct msm_timer_sync_data_t *data, uint32_t clk_val),
	struct msm_timer_sync_data_t *data)
{
	uint32_t *smem_clock;
	uint32_t smem_clock_val;
	uint32_t last_state;
	uint32_t state;

	smem_clock = smem_alloc(SMEM_SMEM_SLOW_CLOCK_VALUE,
				sizeof(uint32_t));

	if (smem_clock == NULL) {
		printk(KERN_ERR "no smem clock\n");
		return 0;
	}

	last_state = state = smsm_get_state(SMSM_MODEM_STATE);
	smem_clock_val = *smem_clock;
	if (smem_clock_val) {
		printk(KERN_INFO "get_smem_clock: invalid start state %x "
			"clock %u\n", state, smem_clock_val);
		smsm_change_state(SMSM_APPS_STATE,
				  SMSM_TIMEWAIT, SMSM_TIMEINIT);

		time_start(data);
		while (*smem_clock != 0 && !time_expired(data))
			;

		smem_clock_val = *smem_clock;
		if (smem_clock_val) {
			printk(KERN_INFO "get_smem_clock: timeout still "
				"invalid state %x clock %u\n",
				state, smem_clock_val);
			return 0;
		}
	}

	time_start(data);
	smsm_change_state(SMSM_APPS_STATE, SMSM_TIMEINIT, SMSM_TIMEWAIT);
	do {
		smem_clock_val = *smem_clock;
		state = smsm_get_state(SMSM_MODEM_STATE);
		if (state != last_state) {
			last_state = state;
			if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
				printk(KERN_INFO
					"get_smem_clock: state %x clock %u\n",
					state, smem_clock_val);
		}
	} while (smem_clock_val == 0 && !time_expired(data));

	if (smem_clock_val) {
		if (update != NULL)
			update(data, smem_clock_val);
	} else {
		printk(KERN_INFO "get_smem_clock: timeout state %x clock %u\n",
			state, smem_clock_val);
	}

	smsm_change_state(SMSM_APPS_STATE, SMSM_TIMEWAIT, SMSM_TIMEINIT);
	time_start(data);
	while (*smem_clock != 0 && !time_expired(data))
		;

	if (*smem_clock)
		printk(KERN_INFO "get_smem_clock: exit timeout state %x "
			"clock %u\n", state, *smem_clock);
	return smem_clock_val;
}
#endif /* CONFIG_MSM_N_WAY_SMSM */

/*
 * Callback function that initializes the timeout value.
 */
static void msm_timer_sync_smem_clock_time_start(
	struct msm_timer_sync_data_t *data)
{
	data->timeout = ktime_to_ns(ktime_get()) + NSEC_PER_MSEC * 10;
}

/*
 * Callback function that checks the timeout.
 */
static bool msm_timer_sync_smem_clock_time_expired(
	struct msm_timer_sync_data_t *data)
{
	return ktime_to_ns(ktime_get()) >= data->timeout;
}

/*
 * Callback function that updates clock(s) with the specified slow clock
 * value.  The GPT clock is always updated.
 */
static void msm_timer_sync_smem_clock_update(
	struct msm_timer_sync_data_t *data, uint32_t clock_value)
{
	struct msm_clock *clocks[2];
	uint32_t timer_counts[2];
    #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
	uint64_t new_offset;
    #else
	uint32_t new_offset;
    #endif
	int i;

	clocks[0] = data->clock;

	if (data->clock != &msm_clocks[MSM_CLOCK_GPT])
		clocks[1] = &msm_clocks[MSM_CLOCK_GPT];
	else
		clocks[1] = NULL;

	for (i = 0; i < ARRAY_SIZE(clocks); i++) {
		if (clocks[i] == NULL)
			continue;

		timer_counts[i] = msm_read_timer_count(clocks[i]);
		writel(TIMER_ENABLE_EN, clocks[i]->regbase + TIMER_ENABLE);
	}

	for (i = 0; i < ARRAY_SIZE(clocks); i++) {
		if (clocks[i] == NULL)
			continue;

		new_offset = (uint64_t)clock_value *
			((clocks[i]->freq << clocks[i]->shift) / SCLK_HZ) -
			timer_counts[i];

		if (clocks[i]->offset + clocks[i]->smem_offset != new_offset) {
			if (data->exit_sleep)
				clocks[i]->offset
					= new_offset - clocks[i]->smem_offset;
			else
				clocks[i]->smem_offset
					= new_offset - clocks[i]->offset;

			clocks[i]->smem_in_sync = 1;
        #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME_DEBUG
            if (i == 0)
                printk("no:0x%llx tnct:0x%x cv:0x%x of:0x%llx so: 0x%llx\n",
                new_offset, 
                timer_counts[i],
					clock_value, 
					clocks[i]->offset,
					clocks[i]->smem_offset);
        #endif
        #ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
			if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
				printk(KERN_INFO "get_smem_clock: "
					"clock 0x%x new offset 0x%llx+0x%llx\n",
					clock_value, clocks[i]->offset,
					clocks[i]->smem_offset);
        #else
			if (msm_timer_debug_mask & MSM_TIMER_DEBUG_SYNC)
				printk(KERN_INFO "get_smem_clock: "
					"clock %u new offset %u+%u\n",
					clock_value, clocks[i]->offset,
					clocks[i]->smem_offset);
        #endif
		}
	}
}

/*
 * Synchronize clock(s) with the slow clock through SMEM.
 *
 * Return value:
 *      0: the operation failed
 *      >0: the slow clock value after time-sync
 */
static uint32_t msm_timer_sync_smem_clock(struct msm_clock *clock,
	int exit_sleep)
{
	struct msm_timer_sync_data_t data;

	if (!exit_sleep && clock->smem_in_sync &&
		msm_clocks[MSM_CLOCK_GPT].smem_in_sync)
		return 0;

	data.clock = clock;
	data.timeout = 0;
	data.exit_sleep = exit_sleep;

	return msm_timer_sync_sclk(
		msm_timer_sync_smem_clock_time_start,
		msm_timer_sync_smem_clock_time_expired,
		msm_timer_sync_smem_clock_update,
		&data);
}

static void msm_timer_reactivate_alarm(struct msm_clock *clock)
{
#ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
    #if DEBUG_REBOOT
#define VIC_REG(off) (MSM_VIC_BASE + (off))
#define VIC_INT_EN0         VIC_REG(0x0010)
#define VIC_INT_EN1         VIC_REG(0x0014)
    int64_t alarm_delta;
    uint32_t count = msm_read_timer_count(clock);
    alarm_delta = (int64_t)clock->alarm_vtime - clock->offset - count;
    #else
	int64_t alarm_delta = (int64_t)clock->alarm_vtime - clock->offset -
		msm_read_timer_count(clock);
    #endif
#else
	long alarm_delta = clock->alarm_vtime - clock->offset -
		msm_read_timer_count(clock);
#endif
	alarm_delta >>= clock->shift;
#ifdef WORK_AROUND_FOR_ABNORMAL_SYSTEM_TIME
	if (alarm_delta < (int64_t)clock->write_delay + 4)
		alarm_delta = (int64_t)clock->write_delay + 4;
    #if DEBUG_REBOOT
    if (alarm_delta > (clock->freq))
    {
        printk("irq=%x delta=0x%llx vtime=0x%llx ofs=0x%llx cnt=0x%x reg: 0x%x 0x%x 0x%x %x %x\n",
        irqs_disabled(),
        alarm_delta, clock->alarm_vtime, clock->offset, count, 
        readl(clock->regbase + TIMER_MATCH_VAL),
        readl(clock->regbase + TIMER_COUNT_VAL),
        readl(clock->regbase + TIMER_ENABLE),
        readl(VIC_INT_EN0),
        readl(VIC_INT_EN1));
    }
    #endif
#else
	if (alarm_delta < (long)clock->write_delay + 4)
		alarm_delta = clock->write_delay + 4;
#endif
	while (msm_timer_set_next_event(alarm_delta, &clock->clockevent))
		;
}

int64_t msm_timer_enter_idle(void)
{
	struct msm_clock *clock = msm_active_clock;
	uint32_t alarm;
	uint32_t count;
	int32_t delta;

	BUG_ON(clock != &msm_clocks[MSM_CLOCK_GPT] &&
		clock != &msm_clocks[MSM_CLOCK_DGT]);

	msm_timer_sync_smem_clock(clock, 0);

	count = msm_read_timer_count(clock);
	alarm = readl(clock->regbase + TIMER_MATCH_VAL);
	delta = alarm - count;

	if (delta <= -(int32_t)((clock->freq << clock->shift) >> 10)) {
		/* timer should have triggered 1ms ago */
    	#if DEBUG_REBOOT
	    printk(KERN_ERR "msm_timer_enter_idle: timer late %d, alarm=0x%x, count=0x%x"
			"reprogram it\n", delta, alarm, count);
        #else
		printk(KERN_ERR "msm_timer_enter_idle: timer late %d, "
			"reprogram it\n", delta);
        #endif
		msm_timer_reactivate_alarm(clock);
	}
	if (delta <= 0)
		return 0;
	return cyc2ns(&clock->clocksource, (alarm - count) >> clock->shift);
}

void msm_timer_exit_idle(int low_power)
{
	struct msm_clock *clock = msm_active_clock;
	uint32_t smem_clock;

	if (!low_power)
		return;

	BUG_ON(clock != &msm_clocks[MSM_CLOCK_GPT] &&
		clock != &msm_clocks[MSM_CLOCK_DGT]);

#if defined(CONFIG_ARCH_QSD)
			smem_clock = msm_timer_sync_smem_clock(clock, 1);
#else
	if (!(readl(clock->regbase + TIMER_ENABLE) & TIMER_ENABLE_EN))
		smem_clock = msm_timer_sync_smem_clock(clock, 1);
#endif
		msm_timer_reactivate_alarm(clock);
}

/*
 * Callback function that initializes the timeout value.
 */
static void msm_timer_get_smem_clock_time_start(
	struct msm_timer_sync_data_t *data)
{
	data->timeout = 10000;
}

/*
 * Callback function that checks the timeout.
 */
static bool msm_timer_get_smem_clock_time_expired(
	struct msm_timer_sync_data_t *data)
{
	return --data->timeout <= 0;
}

/*
 * Retrieve the cycle count from the slow clock through SMEM and
 * convert it into nanoseconds.
 *
 * On exit, if period is not NULL, it contains the period of the
 * slow clock in nanoseconds, i.e. how long the cycle count wraps
 * around.
 *
 * Return value:
 *      0: the operation failed; period is not set either
 *      >0: time in nanoseconds
 */
int64_t msm_timer_get_smem_clock_time(int64_t *period)
{
	struct msm_timer_sync_data_t data;
	uint32_t clock_value;
	int64_t tmp;

	memset(&data, 0, sizeof(data));
	clock_value = msm_timer_sync_sclk(
		msm_timer_get_smem_clock_time_start,
		msm_timer_get_smem_clock_time_expired,
		NULL,
		&data);

	if (!clock_value)
		return 0;

	if (period) {
		tmp = (int64_t)UINT_MAX;
		tmp = tmp * NSEC_PER_SEC / SCLK_HZ;
		*period = tmp;
	}

	tmp = (int64_t)clock_value;
	tmp = tmp * NSEC_PER_SEC / SCLK_HZ;
	return tmp;
}

unsigned long long sched_clock(void)
{
	if (msm_timer_ready)
		return ktime_to_ns(ktime_get());
	else
		return 0;
}

#ifdef CONFIG_MSM7X00A_USE_GP_TIMER
	#define DG_TIMER_RATING 100
#else
	#define DG_TIMER_RATING 300
#endif

static struct msm_clock msm_clocks[] = {
	[MSM_CLOCK_GPT] = {
		.clockevent = {
			.name           = "gp_timer",
			.features       = CLOCK_EVT_FEAT_ONESHOT,
			.shift          = 32,
			.rating         = 200,
			.set_next_event = msm_timer_set_next_event,
			.set_mode       = msm_timer_set_mode,
		},
		.clocksource = {
			.name           = "gp_timer",
			.rating         = 200,
			.read           = msm_gpt_read,
			.mask           = CLOCKSOURCE_MASK(32),
			.shift          = 17,
			.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
		},
		.irq = {
			.name    = "gp_timer",
			.flags   = IRQF_DISABLED | IRQF_TIMER |
				   IRQF_TRIGGER_RISING,
			.handler = msm_timer_interrupt,
			.dev_id  = &msm_clocks[0].clockevent,
			.irq     = INT_GP_TIMER_EXP
		},
		.regbase = MSM_GPT_BASE,
		.freq = GPT_HZ,
		.flags   =
			MSM_CLOCK_FLAGS_UNSTABLE_COUNT |
			MSM_CLOCK_FLAGS_ODD_MATCH_WRITE |
			MSM_CLOCK_FLAGS_DELAYED_WRITE_POST,
		.write_delay = 9,
	},
	[MSM_CLOCK_DGT] = {
		.clockevent = {
			.name           = "dg_timer",
			.features       = CLOCK_EVT_FEAT_ONESHOT,
			.shift          = 32 + MSM_DGT_SHIFT,
			.rating         = DG_TIMER_RATING,
			.set_next_event = msm_timer_set_next_event,
			.set_mode       = msm_timer_set_mode,
		},
		.clocksource = {
			.name           = "dg_timer",
			.rating         = DG_TIMER_RATING,
			.read           = msm_dgt_read,
			.mask           = CLOCKSOURCE_MASK((32-MSM_DGT_SHIFT)),
			.shift          = 24 - MSM_DGT_SHIFT,
			.flags          = CLOCK_SOURCE_IS_CONTINUOUS,
		},
		.irq = {
			.name    = "dg_timer",
			.flags   = IRQF_DISABLED | IRQF_TIMER |
				   IRQF_TRIGGER_RISING,
			.handler = msm_timer_interrupt,
			.dev_id  = &msm_clocks[1].clockevent,
			.irq     = INT_DEBUG_TIMER_EXP
		},
		.regbase = MSM_DGT_BASE,
		.freq = DGT_HZ >> MSM_DGT_SHIFT,
		.shift = MSM_DGT_SHIFT,
		.write_delay = 2,
	}
};

static void __init msm_timer_init(void)
{
	int i;
	int res;

	for (i = 0; i < ARRAY_SIZE(msm_clocks); i++) {
		struct msm_clock *clock = &msm_clocks[i];
		struct clock_event_device *ce = &clock->clockevent;
		struct clocksource *cs = &clock->clocksource;
		writel(0, clock->regbase + TIMER_ENABLE);
		writel(~0, clock->regbase + TIMER_MATCH_VAL);

		ce->mult = div_sc(clock->freq, NSEC_PER_SEC, ce->shift);
		/* allow at least 10 seconds to notice that the timer wrapped */
		ce->max_delta_ns =
			clockevent_delta2ns(0xf0000000 >> clock->shift, ce);
		/* ticks gets rounded down by one */
		ce->min_delta_ns =
			clockevent_delta2ns(clock->write_delay + 4, ce);
		ce->cpumask = cpumask_of_cpu(0);

		cs->mult = clocksource_hz2mult(clock->freq, cs->shift);
		res = clocksource_register(cs);
		if (res)
			printk(KERN_ERR "msm_timer_init: clocksource_register "
			       "failed for %s\n", cs->name);

		res = setup_irq(clock->irq.irq, &clock->irq);
		if (res)
			printk(KERN_ERR "msm_timer_init: setup_irq "
			       "failed for %s\n", cs->name);

		clockevents_register_device(ce);
		msm_timer_ready = 1;
	}
}

struct sys_timer msm_timer = {
	.init = msm_timer_init
};
