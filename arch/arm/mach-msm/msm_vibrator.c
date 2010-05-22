/* include/asm/mach-msm/msm_vibrator.c
 *
 * Copyright (C) 2009 FIH, Inc.
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/timed_output.h>
/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
///+FIH_ADQ
#include <linux/wakelock.h>
///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848, Header for using completion signal mechanism. */
#include <linux/completion.h>
#include <linux/sched.h>
/* } FIH, AudiPCHuang, 2009/07/07 */

#include <mach/msm_rpcrouter.h>

#define PM_LIBPROG      0x30000061
#define PM_LIBVERS      0x10001

#define PROCEDURE_SET_VIB_ON_OFF	21
#define PMIC_VIBRATOR_LEVEL	(3000)

static struct work_struct work_vibrator_on;
static struct work_struct work_vibrator_off;
static struct hrtimer vibe_timer;
/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
///+FIH_ADQ
static struct wake_lock vibrator_suspend_wake_lock;
///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848,  Declare a completion for notifying the thread to run vibrator off procedure*/
DEFINE_MUTEX(vibrator_lock);
DECLARE_COMPLETION(vibrator_comp);
static pid_t thread_id;
/* } FIH, AudiPCHuang, 2009/07/07 */

static void set_pmic_vibrator(int on)
{
	static struct msm_rpc_endpoint *vib_endpoint;
	struct set_vib_on_off_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	if (!vib_endpoint) {
		vib_endpoint = msm_rpc_connect(PM_LIBPROG, PM_LIBVERS, 0);
		if (IS_ERR(vib_endpoint)) {
			printk(KERN_ERR "init vib rpc failed!\n");
			vib_endpoint = 0;
			return;
		}
	}

	if (on) {
		req.data = cpu_to_be32(PMIC_VIBRATOR_LEVEL);
	}
	else {
		req.data = cpu_to_be32(0);
	}

	msm_rpc_call(vib_endpoint, PROCEDURE_SET_VIB_ON_OFF, &req,
		sizeof(req), 5 * HZ);
}

static void pmic_vibrator_on(struct work_struct *work)
{
	set_pmic_vibrator(1);
}

static void pmic_vibrator_off(struct work_struct *work)
{
	set_pmic_vibrator(0);
}

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848,  A kernel thread for stopping the vibration. */
static int pmic_vibrator_off_thread(void *arg)
{
	printk(KERN_INFO "%s: vib_off_thread running\n", __func__);

	daemonize("vib_off_thread");

	while (1) {
		wait_for_completion(&vibrator_comp);
		//printk(KERN_INFO "%s: Got complete signal\n", __func__);

/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
		///+FIH_ADQ
		wake_lock(&vibrator_suspend_wake_lock);
		///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */

		mutex_lock(&vibrator_lock);
		set_pmic_vibrator(0);
		mutex_unlock(&vibrator_lock);

/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
		///+FIH_ADQ
		wake_unlock(&vibrator_suspend_wake_lock);
		///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */
	}
	
	return 0;
}
/* } FIH, AudiPCHuang, 2009/07/07 */

static void timed_vibrator_on(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_on);
}

static void timed_vibrator_off(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_off);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
	///+FIH_ADQ
	wake_lock(&vibrator_suspend_wake_lock);
	///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */
	hrtimer_cancel(&vibe_timer);
	//printk(KERN_INFO "%s: TIMER canceled\n", __func__);

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848,  Turn on the vibrator */
	mutex_lock(&vibrator_lock);
	//printk(KERN_INFO "%s: Vibration period %d ms\n", __func__, value);
		value = (value > 15000 ? 15000 : value);
	value = (value > 10 ? (value - 10) : value);

	if (value != 0) {
		//printk(KERN_INFO "%s: execute pmic_vibrator_on\n", __func__);
		set_pmic_vibrator(1);
	}

		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	mutex_unlock(&vibrator_lock);

	/*if (hrtimer_active(&vibe_timer))
		printk(KERN_INFO "%s: TIMER running\n", __func__);*/
/* } FIH, AudiPCHuang, 2009/07/07 */
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		return r.tv.sec * 1000 + r.tv.nsec / 1000000;
	} else
		return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
	///+FIH_ADQ
	wake_unlock(&vibrator_suspend_wake_lock);
	///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848,  Signal thread to turn off the vibrator. */
	complete(&vibrator_comp);
/* } FIH, AudiPCHuang, 2009/07/07 */

	return HRTIMER_NORESTART;
}

static struct timed_output_dev pmic_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

void __init msm_init_pmic_vibrator(void)
{
	INIT_WORK(&work_vibrator_on, pmic_vibrator_on);
	INIT_WORK(&work_vibrator_off, pmic_vibrator_off);

	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;

	timed_output_dev_register(&pmic_vibrator);
/* FIH, AudiPCHuang, 2009/06/24, { */
/* ADQ.FC-489, Improve ringer switch detection when suspending */
	///+FIH_ADQ
	wake_lock_init(&vibrator_suspend_wake_lock, WAKE_LOCK_SUSPEND, "vibrator_suspend_work");
	///-FIH_ADQ
/* } FIH, AudiPCHuang, 2009/06/24 */

/* FIH, AudiPCHuang, 2009/07/07, { */
/* ADQ.B-2848,  A kernel thread for stopping the vibration. */
	thread_id = kernel_thread(pmic_vibrator_off_thread, NULL, CLONE_FS | CLONE_FILES);
/* } FIH, AudiPCHuang, 2009/07/07 */
}

MODULE_DESCRIPTION("timed output pmic vibrator device");
MODULE_LICENSE("GPL");

