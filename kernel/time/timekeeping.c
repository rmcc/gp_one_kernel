/*
 *  linux/kernel/time/timekeeping.c
 *
 *  Kernel timekeeping code and accessor functions
 *
 *  This code was moved from linux/kernel/timer.c.
 *  Please see that file for copyright and history logs.
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sysdev.h>
#include <linux/clocksource.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/tick.h>

/* Structure holding internal timekeeping values. */
struct timekeeper {
	/* Current clocksource used for timekeeping. */
	struct clocksource *clock;

	/* Number of clock cycles in one NTP interval. */
	cycle_t cycle_interval;
	/* Number of clock shifted nano seconds in one NTP interval. */
	u64	xtime_interval;
	/* Raw nano seconds accumulated per NTP interval. */
	u32	raw_interval;

	/* Clock shifted nano seconds remainder not stored in xtime.tv_nsec. */
	u64	xtime_nsec;
	/* Difference between accumulated time and NTP time in ntp
	 * shifted nano seconds. */
	s64	ntp_error;
};

struct timekeeper timekeeper;

/**
 * timekeeper_setup_internals - Set up internals to use clocksource clock.
 *
 * @clock:		Pointer to clocksource.
 *
 * Calculates a fixed cycle/nsec interval for a given clocksource/adjustment
 * pair and interval request.
 *
 * Unless you're the timekeeping code, you should not be using this!
 */
static void timekeeper_setup_internals(struct clocksource *clock)
{
	cycle_t interval;
	u64 tmp;

	timekeeper.clock = clock;
	clock->cycle_last = clock->read(clock);

	/* Do the ns -> cycle conversion first, using original mult */
	tmp = NTP_INTERVAL_LENGTH;
	tmp <<= clock->shift;
	tmp += clock->mult_orig/2;
	do_div(tmp, clock->mult_orig);
	if (tmp == 0)
		tmp = 1;

	interval = (cycle_t) tmp;
	timekeeper.cycle_interval = interval;

	/* Go back from cycles -> shifted ns */
	timekeeper.xtime_interval = (u64) interval * clock->mult;
	timekeeper.raw_interval =
		((u64) interval * clock->mult_orig) >> clock->shift;

	timekeeper.xtime_nsec = 0;

	timekeeper.ntp_error = 0;
}

/*
 * This read-write spinlock protects us from races in SMP while
 * playing with xtime.
 */
__cacheline_aligned_in_smp DEFINE_SEQLOCK(xtime_lock);


/*
 * The current time
 * wall_to_monotonic is what we need to add to xtime (or xtime corrected
 * for sub jiffie times) to get to monotonic time.  Monotonic is pegged
 * at zero at system boot time, so wall_to_monotonic will be negative,
 * however, we will ALWAYS keep the tv_nsec part positive so we can use
 * the usual normalization.
 *
 * wall_to_monotonic is moved after resume from suspend for the monotonic
 * time not to jump. We need to add total_sleep_time to wall_to_monotonic
 * to get the real boot based time offset.
 *
 * - wall_to_monotonic is no longer the boot time, getboottime must be
 * used instead.
 */
struct timespec xtime __attribute__ ((aligned (16)));
struct timespec wall_to_monotonic __attribute__ ((aligned (16)));
static unsigned long total_sleep_time;		/* seconds */

/*
 * The raw monotonic time for the CLOCK_MONOTONIC_RAW posix clock.
 */
struct timespec raw_time;

/* flag for if timekeeping is suspended */
int __read_mostly timekeeping_suspended;

static struct timespec xtime_cache __attribute__ ((aligned (16)));
void update_xtime_cache(u64 nsec)
{
	xtime_cache = xtime;
	timespec_add_ns(&xtime_cache, nsec);
}

/* must hold xtime_lock */
void timekeeping_leap_insert(int leapsecond)
{
	xtime.tv_sec += leapsecond;
	wall_to_monotonic.tv_sec -= leapsecond;
	update_vsyscall(&xtime, timekeeper.clock);
}

#ifdef CONFIG_GENERIC_TIME
/**
 * timekeeping_forward_now - update clock to the current time
 *
 * Forward the current clock to update its state since the last call to
 * update_wall_time(). This is useful before significant clock changes,
 * as it avoids having to deal with this time offset explicitly.
 */
static void timekeeping_forward_now(void)
{
	cycle_t cycle_now, cycle_delta;
	struct clocksource *clock;
	s64 nsec;

	clock = timekeeper.clock;
	cycle_now = clock->read(clock);
	cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;
	clock->cycle_last = cycle_now;

	nsec = clocksource_cyc2ns(cycle_delta, clock->mult, clock->shift);

	/* If arch requires, add in gettimeoffset() */
	nsec += arch_gettimeoffset();

	timespec_add_ns(&xtime, nsec);

	nsec = clocksource_cyc2ns(cycle_delta, clock->mult_orig, clock->shift);
	timespec_add_ns(&raw_time, nsec);
}

/**
 * getnstimeofday - Returns the time of day in a timespec
 * @ts:		pointer to the timespec to be set
 *
 * Returns the time of day in a timespec.
 */
void getnstimeofday(struct timespec *ts)
{
	cycle_t cycle_now, cycle_delta;
	struct clocksource *clock;
	unsigned long seq;
	s64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqbegin(&xtime_lock);

		*ts = xtime;

		/* read clocksource: */
		clock = timekeeper.clock;
		cycle_now = clock->read(clock);

		/* calculate the delta since the last update_wall_time: */
		cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

		/* convert to nanoseconds: */
		nsecs = clocksource_cyc2ns(cycle_delta, clock->mult,
					   clock->shift);

		/* If arch requires, add in gettimeoffset() */
		nsecs += arch_gettimeoffset();

	} while (read_seqretry(&xtime_lock, seq));

	timespec_add_ns(ts, nsecs);
}

EXPORT_SYMBOL(getnstimeofday);

ktime_t ktime_get(void)
{
	cycle_t cycle_now, cycle_delta;
	struct clocksource *clock;
	unsigned int seq;
	s64 secs, nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqbegin(&xtime_lock);
		secs = xtime.tv_sec + wall_to_monotonic.tv_sec;
		nsecs = xtime.tv_nsec + wall_to_monotonic.tv_nsec;

		/* read clocksource: */
		clock = timekeeper.clock;
		cycle_now = clock->read(clock);

		/* calculate the delta since the last update_wall_time: */
		cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

		/* convert to nanoseconds: */
		nsecs += clocksource_cyc2ns(cycle_delta, clock->mult,
					    clock->shift);

	} while (read_seqretry(&xtime_lock, seq));
	/*
	 * Use ktime_set/ktime_add_ns to create a proper ktime on
	 * 32-bit architectures without CONFIG_KTIME_SCALAR.
	 */
	return ktime_add_ns(ktime_set(secs, 0), nsecs);
}
EXPORT_SYMBOL_GPL(ktime_get);

/**
 * ktime_get_ts - get the monotonic clock in timespec format
 * @ts:		pointer to timespec variable
 *
 * The function calculates the monotonic clock from the realtime
 * clock and the wall_to_monotonic offset and stores the result
 * in normalized timespec format in the variable pointed to by @ts.
 */
void ktime_get_ts(struct timespec *ts)
{
	cycle_t cycle_now, cycle_delta;
	struct clocksource *clock;
	struct timespec tomono;
	unsigned int seq;
	s64 nsecs;

	WARN_ON(timekeeping_suspended);

	do {
		seq = read_seqbegin(&xtime_lock);
		*ts = xtime;
		tomono = wall_to_monotonic;

		/* read clocksource: */
		clock = timekeeper.clock;
		cycle_now = clock->read(clock);

		/* calculate the delta since the last update_wall_time: */
		cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

		/* convert to nanoseconds: */
		nsecs = clocksource_cyc2ns(cycle_delta, clock->mult,
					   clock->shift);

	} while (read_seqretry(&xtime_lock, seq));

	set_normalized_timespec(ts, ts->tv_sec + tomono.tv_sec,
				ts->tv_nsec + tomono.tv_nsec + nsecs);
}
EXPORT_SYMBOL_GPL(ktime_get_ts);

/**
 * do_gettimeofday - Returns the time of day in a timeval
 * @tv:		pointer to the timeval to be set
 *
 * NOTE: Users should be converted to using getnstimeofday()
 */
void do_gettimeofday(struct timeval *tv)
{
	struct timespec now;

	getnstimeofday(&now);
	tv->tv_sec = now.tv_sec;
	tv->tv_usec = now.tv_nsec/1000;
}

EXPORT_SYMBOL(do_gettimeofday);
/**
 * do_settimeofday - Sets the time of day
 * @tv:		pointer to the timespec variable containing the new time
 *
 * Sets the time of day to the new time and update NTP and notify hrtimers
 */
int do_settimeofday(struct timespec *tv)
{
	struct timespec ts_delta;
	unsigned long flags;

	if ((unsigned long)tv->tv_nsec >= NSEC_PER_SEC)
		return -EINVAL;

	write_seqlock_irqsave(&xtime_lock, flags);

	timekeeping_forward_now();

	ts_delta.tv_sec = tv->tv_sec - xtime.tv_sec;
	ts_delta.tv_nsec = tv->tv_nsec - xtime.tv_nsec;
	wall_to_monotonic = timespec_sub(wall_to_monotonic, ts_delta);

	xtime = *tv;

	update_xtime_cache(0);

	timekeeper.ntp_error = 0;
	ntp_clear();

	update_vsyscall(&xtime, timekeeper.clock);

	write_sequnlock_irqrestore(&xtime_lock, flags);

	/* signal hrtimers about time change */
	clock_was_set();

	return 0;
}

EXPORT_SYMBOL(do_settimeofday);

/**
 * change_clocksource - Swaps clocksources if a new one is available
 *
 * Accumulates current time interval and initializes new clocksource
 */
static void change_clocksource(void)
{
	struct clocksource *new, *old;

	new = clocksource_get_next();

	if (!new || timekeeper.clock == new)
		return;

	timekeeping_forward_now();

	if (new->enable && !new->enable(new))
		return;
	/*
	 * The frequency may have changed while the clocksource
	 * was disabled. If so the code in ->enable() must update
	 * the mult value to reflect the new frequency. Make sure
	 * mult_orig follows this change.
	 */
	new->mult_orig = new->mult;

	old = timekeeper.clock;
	timekeeper_setup_internals(new);

	/*
	 * Save mult_orig in mult so that the value can be restored
	 * regardless if ->enable() updates the value of mult or not.
	 */
	old->mult = old->mult_orig;
	if (old->disable)
		old->disable(old);

	tick_clock_notify();
}
#else /* GENERIC_TIME */
static inline void timekeeping_forward_now(void) { }
static inline void change_clocksource(void) { }

/**
 * ktime_get - get the monotonic time in ktime_t format
 *
 * returns the time in ktime_t format
 */
ktime_t ktime_get(void)
{
	struct timespec now;

	ktime_get_ts(&now);

	return timespec_to_ktime(now);
}
EXPORT_SYMBOL_GPL(ktime_get);

/**
 * ktime_get_ts - get the monotonic clock in timespec format
 * @ts:		pointer to timespec variable
 *
 * The function calculates the monotonic clock from the realtime
 * clock and the wall_to_monotonic offset and stores the result
 * in normalized timespec format in the variable pointed to by @ts.
 */
void ktime_get_ts(struct timespec *ts)
{
	struct timespec tomono;
	unsigned long seq;

	do {
		seq = read_seqbegin(&xtime_lock);
		getnstimeofday(ts);
		tomono = wall_to_monotonic;

	} while (read_seqretry(&xtime_lock, seq));

	set_normalized_timespec(ts, ts->tv_sec + tomono.tv_sec,
				ts->tv_nsec + tomono.tv_nsec);
}
EXPORT_SYMBOL_GPL(ktime_get_ts);
#endif /* !GENERIC_TIME */

/**
 * ktime_get_real - get the real (wall-) time in ktime_t format
 *
 * returns the time in ktime_t format
 */
ktime_t ktime_get_real(void)
{
	struct timespec now;

	getnstimeofday(&now);

	return timespec_to_ktime(now);
}
EXPORT_SYMBOL_GPL(ktime_get_real);

/**
 * getrawmonotonic - Returns the raw monotonic time in a timespec
 * @ts:		pointer to the timespec to be set
 *
 * Returns the raw monotonic time (completely un-modified by ntp)
 */
void getrawmonotonic(struct timespec *ts)
{
	unsigned long seq;
	s64 nsecs;
	cycle_t cycle_now, cycle_delta;
	struct clocksource *clock;

	do {
		seq = read_seqbegin(&xtime_lock);

		/* read clocksource: */
		clock = timekeeper.clock;
		cycle_now = clock->read(clock);

		/* calculate the delta since the last update_wall_time: */
		cycle_delta = (cycle_now - clock->cycle_last) & clock->mask;

		/* convert to nanoseconds: */
		nsecs = clocksource_cyc2ns(cycle_delta, clock->mult_orig,
					   clock->shift);

		*ts = raw_time;

	} while (read_seqretry(&xtime_lock, seq));

	timespec_add_ns(ts, nsecs);
}
EXPORT_SYMBOL(getrawmonotonic);


/**
 * timekeeping_valid_for_hres - Check if timekeeping is suitable for hres
 */
int timekeeping_valid_for_hres(void)
{
	unsigned long seq;
	int ret;

	do {
		seq = read_seqbegin(&xtime_lock);

		ret = timekeeper.clock->flags & CLOCK_SOURCE_VALID_FOR_HRES;

	} while (read_seqretry(&xtime_lock, seq));

	return ret;
}

/**
 * read_persistent_clock -  Return time in seconds from the persistent clock.
 *
 * Weak dummy function for arches that do not yet support it.
 * Returns seconds from epoch using the battery backed persistent clock.
 * Returns zero if unsupported.
 *
 *  XXX - Do be sure to remove it once all arches implement it.
 */
unsigned long __attribute__((weak)) read_persistent_clock(void)
{
	return 0;
}

/*
 * timekeeping_init - Initializes the clocksource and common timekeeping values
 */
void __init timekeeping_init(void)
{
	struct clocksource *clock;
	unsigned long flags;
	unsigned long sec = read_persistent_clock();

	write_seqlock_irqsave(&xtime_lock, flags);

	ntp_init();

	clock = clocksource_default_clock();
	if (clock->enable)
		clock->enable(clock);
	/* set mult_orig on enable */
	clock->mult_orig = clock->mult;

	timekeeper_setup_internals(clock);

	xtime.tv_sec = sec;
	xtime.tv_nsec = 0;
	raw_time.tv_sec = 0;
	raw_time.tv_nsec = 0;
	set_normalized_timespec(&wall_to_monotonic,
		-xtime.tv_sec, -xtime.tv_nsec);
	update_xtime_cache(0);
	total_sleep_time = 0;
	write_sequnlock_irqrestore(&xtime_lock, flags);
}

/* time in seconds when suspend began */
static unsigned long timekeeping_suspend_time;

/**
 * timekeeping_resume - Resumes the generic timekeeping subsystem.
 * @dev:	unused
 *
 * This is for the generic clocksource timekeeping.
 * xtime/wall_to_monotonic/jiffies/etc are
 * still managed by arch specific suspend/resume code.
 */
static int timekeeping_resume(struct sys_device *dev)
{
	unsigned long flags;
	unsigned long now = read_persistent_clock();

	clocksource_resume();

	write_seqlock_irqsave(&xtime_lock, flags);

	if (now && (now > timekeeping_suspend_time)) {
		unsigned long sleep_length = now - timekeeping_suspend_time;

		xtime.tv_sec += sleep_length;
		wall_to_monotonic.tv_sec -= sleep_length;
		total_sleep_time += sleep_length;
	}
	update_xtime_cache(0);
	/* re-base the last cycle value */
	timekeeper.clock->cycle_last = timekeeper.clock->read(timekeeper.clock);
	timekeeper.ntp_error = 0;
	timekeeping_suspended = 0;
	write_sequnlock_irqrestore(&xtime_lock, flags);

	touch_softlockup_watchdog();

	clockevents_notify(CLOCK_EVT_NOTIFY_RESUME, NULL);

	/* Resume hrtimers */
	hres_timers_resume();

	return 0;
}

static int timekeeping_suspend(struct sys_device *dev, pm_message_t state)
{
	unsigned long flags;

	timekeeping_suspend_time = read_persistent_clock();

	write_seqlock_irqsave(&xtime_lock, flags);
	timekeeping_forward_now();
	timekeeping_suspended = 1;
	write_sequnlock_irqrestore(&xtime_lock, flags);

	clockevents_notify(CLOCK_EVT_NOTIFY_SUSPEND, NULL);

	return 0;
}

/* sysfs resume/suspend bits for timekeeping */
static struct sysdev_class timekeeping_sysclass = {
	.name		= "timekeeping",
	.resume		= timekeeping_resume,
	.suspend	= timekeeping_suspend,
};

static struct sys_device device_timer = {
	.id		= 0,
	.cls		= &timekeeping_sysclass,
};

static int __init timekeeping_init_device(void)
{
	int error = sysdev_class_register(&timekeeping_sysclass);
	if (!error)
		error = sysdev_register(&device_timer);
	return error;
}

device_initcall(timekeeping_init_device);

/*
 * If the error is already larger, we look ahead even further
 * to compensate for late or lost adjustments.
 */
static __always_inline int timekeeping_bigadjust(s64 error, s64 *interval,
						 s64 *offset)
{
	s64 tick_error, i;
	u32 look_ahead, adj;
	s32 error2, mult;

	/*
	 * Use the current error value to determine how much to look ahead.
	 * The larger the error the slower we adjust for it to avoid problems
	 * with losing too many ticks, otherwise we would overadjust and
	 * produce an even larger error.  The smaller the adjustment the
	 * faster we try to adjust for it, as lost ticks can do less harm
	 * here.  This is tuned so that an error of about 1 msec is adjusted
	 * within about 1 sec (or 2^20 nsec in 2^SHIFT_HZ ticks).
	 */
	error2 = timekeeper.ntp_error >> (NTP_SCALE_SHIFT + 22 - 2 * SHIFT_HZ);
	error2 = abs(error2);
	for (look_ahead = 0; error2 > 0; look_ahead++)
		error2 >>= 2;

	/*
	 * Now calculate the error in (1 << look_ahead) ticks, but first
	 * remove the single look ahead already included in the error.
	 */
	tick_error = tick_length >>
			(NTP_SCALE_SHIFT - timekeeper.clock->shift + 1);
	tick_error -= timekeeper.xtime_interval >> 1;
	error = ((error - tick_error) >> look_ahead) + tick_error;

	/* Finally calculate the adjustment shift value.  */
	i = *interval;
	mult = 1;
	if (error < 0) {
		error = -error;
		*interval = -*interval;
		*offset = -*offset;
		mult = -1;
	}
	for (adj = 0; error > i; adj++)
		error >>= 1;

	*interval <<= adj;
	*offset <<= adj;
	return mult << adj;
}

/*
 * Adjust the multiplier to reduce the error value,
 * this is optimized for the most common adjustments of -1,0,1,
 * for other values we can do a bit more work.
 */
static void timekeeping_adjust(s64 offset)
{
	s64 error, interval = timekeeper.cycle_interval;
	int adj;

	error = timekeeper.ntp_error >>
		(NTP_SCALE_SHIFT - timekeeper.clock->shift - 1);
	if (error > interval) {
		error >>= 2;
		if (likely(error <= interval))
			adj = 1;
		else
			adj = timekeeping_bigadjust(error, &interval, &offset);
	} else if (error < -interval) {
		error >>= 2;
		if (likely(error >= -interval)) {
			adj = -1;
			interval = -interval;
			offset = -offset;
		} else
			adj = timekeeping_bigadjust(error, &interval, &offset);
	} else
		return;

	timekeeper.clock->mult += adj;
	timekeeper.xtime_interval += interval;
	timekeeper.xtime_nsec -= offset;
	timekeeper.ntp_error -= (interval - offset) <<
			(NTP_SCALE_SHIFT - timekeeper.clock->shift);
}

/**
 * update_wall_time - Uses the current clocksource to increment the wall time
 *
 * Called from the timer interrupt, must hold a write on xtime_lock.
 */
void update_wall_time(void)
{
	struct clocksource *clock;
	cycle_t offset;
	s64 nsecs;

	/* Make sure we're fully resumed: */
	if (unlikely(timekeeping_suspended))
		return;

	clock = timekeeper.clock;
#ifdef CONFIG_GENERIC_TIME
	offset = (clock->read(clock) - clock->cycle_last) & clock->mask;
#else
	offset = timekeeper.cycle_interval;
#endif
	timekeeper.xtime_nsec = (s64)xtime.tv_nsec << clock->shift;

	/* normally this loop will run just once, however in the
	 * case of lost or late ticks, it will accumulate correctly.
	 */
	while (offset >= timekeeper.cycle_interval) {
		u64 nsecps = (u64)NSEC_PER_SEC << clock->shift;

		/* accumulate one interval */
		offset -= timekeeper.cycle_interval;
		clock->cycle_last += timekeeper.cycle_interval;

		timekeeper.xtime_nsec += timekeeper.xtime_interval;
		if (timekeeper.xtime_nsec >= nsecps) {
			timekeeper.xtime_nsec -= nsecps;
			xtime.tv_sec++;
			second_overflow();
		}

		raw_time.tv_nsec += timekeeper.raw_interval;
		if (raw_time.tv_nsec >= NSEC_PER_SEC) {
			raw_time.tv_nsec -= NSEC_PER_SEC;
			raw_time.tv_sec++;
		}

		/* accumulate error between NTP and clock interval */
		timekeeper.ntp_error += tick_length;
		timekeeper.ntp_error -= timekeeper.xtime_interval <<
					(NTP_SCALE_SHIFT - clock->shift);
	}

	/* correct the clock when NTP error is too big */
	timekeeping_adjust(offset);

	/*
	 * Since in the loop above, we accumulate any amount of time
	 * in xtime_nsec over a second into xtime.tv_sec, its possible for
	 * xtime_nsec to be fairly small after the loop. Further, if we're
	 * slightly speeding the clocksource up in timekeeping_adjust(),
	 * its possible the required corrective factor to xtime_nsec could
	 * cause it to underflow.
	 *
	 * Now, we cannot simply roll the accumulated second back, since
	 * the NTP subsystem has been notified via second_overflow. So
	 * instead we push xtime_nsec forward by the amount we underflowed,
	 * and add that amount into the error.
	 *
	 * We'll correct this error next time through this function, when
	 * xtime_nsec is not as small.
	 */
	if (unlikely((s64)timekeeper.xtime_nsec < 0)) {
		s64 neg = -(s64)timekeeper.xtime_nsec;
		timekeeper.xtime_nsec = 0;
		timekeeper.ntp_error += neg << (NTP_SCALE_SHIFT - clock->shift);
	}

	/* store full nanoseconds into xtime after rounding it up and
	 * add the remainder to the error difference.
	 */
	xtime.tv_nsec = ((s64)timekeeper.xtime_nsec >> clock->shift) + 1;
	timekeeper.xtime_nsec -= (s64)xtime.tv_nsec << clock->shift;
	timekeeper.ntp_error += timekeeper.xtime_nsec <<
				(NTP_SCALE_SHIFT - clock->shift);

	nsecs = clocksource_cyc2ns(offset, clock->mult, clock->shift);
	update_xtime_cache(nsecs);

	/* check to see if there is a new clocksource to use */
	change_clocksource();
	update_vsyscall(&xtime, timekeeper.clock);
}

/**
 * getboottime - Return the real time of system boot.
 * @ts:		pointer to the timespec to be set
 *
 * Returns the time of day in a timespec.
 *
 * This is based on the wall_to_monotonic offset and the total suspend
 * time. Calls to settimeofday will affect the value returned (which
 * basically means that however wrong your real time clock is at boot time,
 * you get the right time here).
 */
void getboottime(struct timespec *ts)
{
	set_normalized_timespec(ts,
		- (wall_to_monotonic.tv_sec + total_sleep_time),
		- wall_to_monotonic.tv_nsec);
}

/**
 * monotonic_to_bootbased - Convert the monotonic time to boot based.
 * @ts:		pointer to the timespec to be converted
 */
void monotonic_to_bootbased(struct timespec *ts)
{
	ts->tv_sec += total_sleep_time;
}

unsigned long get_seconds(void)
{
	return xtime_cache.tv_sec;
}
EXPORT_SYMBOL(get_seconds);


struct timespec current_kernel_time(void)
{
	struct timespec now;
	unsigned long seq;

	do {
		seq = read_seqbegin(&xtime_lock);

		now = xtime_cache;
	} while (read_seqretry(&xtime_lock, seq));

	return now;
}
EXPORT_SYMBOL(current_kernel_time);
