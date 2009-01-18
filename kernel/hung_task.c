/*
 * Detect Hung Task
 *
 * kernel/hung_task.c - kernel thread for detecting tasks stuck in D state
 *
 */

#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/sysctl.h>

/*
 * Have a reasonable limit on the number of tasks checked:
 */
unsigned long __read_mostly sysctl_hung_task_check_count = 1024;

/*
 * Zero means infinite timeout - no checking done:
 */
unsigned long __read_mostly sysctl_hung_task_timeout_secs = 120;
static unsigned long __read_mostly hung_task_poll_jiffies;

unsigned long __read_mostly sysctl_hung_task_warnings = 10;

static int __read_mostly did_panic;

static struct task_struct *watchdog_task;

/*
 * Should we panic (and reboot, if panic_timeout= is set) when a
 * hung task is detected:
 */
unsigned int __read_mostly sysctl_hung_task_panic =
				CONFIG_BOOTPARAM_HUNG_TASK_PANIC_VALUE;

static int __init hung_task_panic_setup(char *str)
{
	sysctl_hung_task_panic = simple_strtoul(str, NULL, 0);

	return 1;
}
__setup("hung_task_panic=", hung_task_panic_setup);

static int
hung_task_panic(struct notifier_block *this, unsigned long event, void *ptr)
{
	did_panic = 1;

	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = hung_task_panic,
};

/*
 * Returns seconds, approximately.  We don't need nanosecond
 * resolution, and we don't need to waste time with a big divide when
 * 2^30ns == 1.074s.
 */
static unsigned long get_timestamp(void)
{
	int this_cpu = raw_smp_processor_id();

	return cpu_clock(this_cpu) >> 30LL;  /* 2^30 ~= 10^9 */
}

static void check_hung_task(struct task_struct *t, unsigned long now,
			    unsigned long timeout)
{
	unsigned long switch_count = t->nvcsw + t->nivcsw;

	if (t->flags & PF_FROZEN)
		return;

	if (switch_count != t->last_switch_count || !t->last_switch_timestamp) {
		t->last_switch_count = switch_count;
		t->last_switch_timestamp = now;
		return;
	}
	if ((long)(now - t->last_switch_timestamp) < timeout)
		return;
	if (!sysctl_hung_task_warnings)
		return;
	sysctl_hung_task_warnings--;

	/*
	 * Ok, the task did not get scheduled for more than 2 minutes,
	 * complain:
	 */
	printk(KERN_ERR "INFO: task %s:%d blocked for more than "
			"%ld seconds.\n", t->comm, t->pid, timeout);
	printk(KERN_ERR "\"echo 0 > /proc/sys/kernel/hung_task_timeout_secs\""
			" disables this message.\n");
	sched_show_task(t);
	__debug_show_held_locks(t);

	t->last_switch_timestamp = now;
	touch_nmi_watchdog();

	if (sysctl_hung_task_panic)
		panic("hung_task: blocked tasks");
}

/*
 * Check whether a TASK_UNINTERRUPTIBLE does not get woken up for
 * a really long time (120 seconds). If that happens, print out
 * a warning.
 */
static void check_hung_uninterruptible_tasks(unsigned long timeout)
{
	int max_count = sysctl_hung_task_check_count;
	unsigned long now = get_timestamp();
	struct task_struct *g, *t;

	/*
	 * If the system crashed already then all bets are off,
	 * do not report extra hung tasks:
	 */
	if (test_taint(TAINT_DIE) || did_panic)
		return;

	read_lock(&tasklist_lock);
	do_each_thread(g, t) {
		if (!--max_count)
			goto unlock;
		/* use "==" to skip the TASK_KILLABLE tasks waiting on NFS */
		if (t->state == TASK_UNINTERRUPTIBLE)
			check_hung_task(t, now, timeout);
	} while_each_thread(g, t);
 unlock:
	read_unlock(&tasklist_lock);
}

static void update_poll_jiffies(void)
{
	/* timeout of 0 will disable the watchdog */
	if (sysctl_hung_task_timeout_secs == 0)
		hung_task_poll_jiffies = MAX_SCHEDULE_TIMEOUT;
	else
		hung_task_poll_jiffies = sysctl_hung_task_timeout_secs * HZ / 2;
}

/*
 * Process updating of timeout sysctl
 */
int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
				  struct file *filp, void __user *buffer,
				  size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_doulongvec_minmax(table, write, filp, buffer, lenp, ppos);

	if (ret || !write)
		goto out;

	update_poll_jiffies();

	wake_up_process(watchdog_task);

 out:
	return ret;
}

/*
 * kthread which checks for tasks stuck in D state
 */
static int watchdog(void *dummy)
{
	set_user_nice(current, 0);
	update_poll_jiffies();

	for ( ; ; ) {
		unsigned long timeout;

		while (schedule_timeout_interruptible(hung_task_poll_jiffies));

		/*
		 * Need to cache timeout here to avoid timeout being set
		 * to 0 via sysctl while inside check_hung_*_tasks().
		 */
		timeout = sysctl_hung_task_timeout_secs;
		if (timeout)
			check_hung_uninterruptible_tasks(timeout);
	}

	return 0;
}

static int __init hung_task_init(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	watchdog_task = kthread_run(watchdog, NULL, "khungtaskd");

	return 0;
}

module_init(hung_task_init);
