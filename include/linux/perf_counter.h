/*
 *  Performance counters:
 *
 *   Copyright(C) 2008, Thomas Gleixner <tglx@linutronix.de>
 *   Copyright(C) 2008, Red Hat, Inc., Ingo Molnar
 *
 *  Data type definitions, declarations, prototypes.
 *
 *  Started by: Thomas Gleixner and Ingo Molnar
 *
 *  For licencing details see kernel-base/COPYING
 */
#ifndef _LINUX_PERF_COUNTER_H
#define _LINUX_PERF_COUNTER_H

#include <asm/atomic.h>

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/spinlock.h>

struct task_struct;

/*
 * User-space ABI bits:
 */

/*
 * Generalized performance counter event types, used by the hw_event.type
 * parameter of the sys_perf_counter_open() syscall:
 */
enum hw_event_types {
	/*
	 * Common hardware events, generalized by the kernel:
	 */
	PERF_COUNT_CYCLES		=  0,
	PERF_COUNT_INSTRUCTIONS		=  1,
	PERF_COUNT_CACHE_REFERENCES	=  2,
	PERF_COUNT_CACHE_MISSES		=  3,
	PERF_COUNT_BRANCH_INSTRUCTIONS	=  4,
	PERF_COUNT_BRANCH_MISSES	=  5,

	/*
	 * Special "software" counters provided by the kernel, even if
	 * the hardware does not support performance counters. These
	 * counters measure various physical and sw events of the
	 * kernel (and allow the profiling of them as well):
	 */
	PERF_COUNT_CPU_CLOCK		= -1,
	PERF_COUNT_TASK_CLOCK		= -2,
	PERF_COUNT_PAGE_FAULTS		= -3,
	PERF_COUNT_CONTEXT_SWITCHES	= -4,
};

/*
 * IRQ-notification data record type:
 */
enum perf_counter_record_type {
	PERF_RECORD_SIMPLE		=  0,
	PERF_RECORD_IRQ			=  1,
	PERF_RECORD_GROUP		=  2,
};

/*
 * Hardware event to monitor via a performance monitoring counter:
 */
struct perf_counter_hw_event {
	u64			type;

	u64			irq_period;
	u32			record_type;

	u32			disabled     :  1, /* off by default */
				nmi	     :  1, /* NMI sampling   */
				raw	     :  1, /* raw event type */
				__reserved_1 : 29;

	u64			__reserved_2;
};

/*
 * Kernel-internal data types:
 */

/**
 * struct hw_perf_counter - performance counter hardware details:
 */
struct hw_perf_counter {
	u64				config;
	unsigned long			config_base;
	unsigned long			counter_base;
	int				nmi;
	unsigned int			idx;
	u64				prev_count;
	u64				irq_period;
	s32				next_count;
};

/*
 * Hardcoded buffer length limit for now, for IRQ-fed events:
 */
#define PERF_DATA_BUFLEN		2048

/**
 * struct perf_data - performance counter IRQ data sampling ...
 */
struct perf_data {
	int				len;
	int				rd_idx;
	int				overrun;
	u8				data[PERF_DATA_BUFLEN];
};

/**
 * struct perf_counter - performance counter kernel representation:
 */
struct perf_counter {
	struct list_head		list;
	int				active;
#if BITS_PER_LONG == 64
	atomic64_t			count;
#else
	atomic_t			count32[2];
#endif
	struct perf_counter_hw_event	hw_event;
	struct hw_perf_counter		hw;

	struct perf_counter_context	*ctx;
	struct task_struct		*task;

	/*
	 * Protect attach/detach:
	 */
	struct mutex			mutex;

	int				oncpu;
	int				cpu;

	/* read() / irq related data */
	wait_queue_head_t		waitq;
	/* optional: for NMIs */
	int				wakeup_pending;
	struct perf_data		*irqdata;
	struct perf_data		*usrdata;
	struct perf_data		data[2];
};

/**
 * struct perf_counter_context - counter context structure
 *
 * Used as a container for task counters and CPU counters as well:
 */
struct perf_counter_context {
#ifdef CONFIG_PERF_COUNTERS
	/*
	 * Protect the list of counters:
	 */
	spinlock_t		lock;
	struct list_head	counters;
	int			nr_counters;
	int			nr_active;
	struct task_struct	*task;
#endif
};

/**
 * struct perf_counter_cpu_context - per cpu counter context structure
 */
struct perf_cpu_context {
	struct perf_counter_context	ctx;
	struct perf_counter_context	*task_ctx;
	int				active_oncpu;
	int				max_pertask;
};

/*
 * Set by architecture code:
 */
extern int perf_max_counters;

#ifdef CONFIG_PERF_COUNTERS
extern void perf_counter_task_sched_in(struct task_struct *task, int cpu);
extern void perf_counter_task_sched_out(struct task_struct *task, int cpu);
extern void perf_counter_task_tick(struct task_struct *task, int cpu);
extern void perf_counter_init_task(struct task_struct *task);
extern void perf_counter_notify(struct pt_regs *regs);
extern void perf_counter_print_debug(void);
extern void hw_perf_restore_ctrl(u64 ctrl);
extern u64 hw_perf_disable_all(void);
#else
static inline void
perf_counter_task_sched_in(struct task_struct *task, int cpu)		{ }
static inline void
perf_counter_task_sched_out(struct task_struct *task, int cpu)		{ }
static inline void
perf_counter_task_tick(struct task_struct *task, int cpu)		{ }
static inline void perf_counter_init_task(struct task_struct *task)	{ }
static inline void perf_counter_notify(struct pt_regs *regs)		{ }
static inline void perf_counter_print_debug(void)			{ }
static inline void hw_perf_restore_ctrl(u64 ctrl)			{ }
static inline u64 hw_perf_disable_all(void)		{ return 0; }
#endif

#endif /* _LINUX_PERF_COUNTER_H */
