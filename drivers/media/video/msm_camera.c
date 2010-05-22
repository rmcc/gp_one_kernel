/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <mach/board.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/android_pmem.h>
#include <linux/poll.h>
#include <media/msm_camera.h>
#include <mach/camera.h>
#include <mach/msm_camio.h>
#include <media/msm_camera_sensor.h>

#define DEV_NAME "msm_camera"
static dev_t msm_devno;

struct msm_fs_pmem_t {
	struct hlist_head pmem_regions;
	struct mutex pmem_mutex;
};

struct msm_sync_t {

	spinlock_t msg_event_queue_lock;
	struct list_head msg_event_queue;
	wait_queue_head_t msg_event_wait;

	spinlock_t prev_frame_q_lock;
	struct list_head prev_frame_q;
	wait_queue_head_t prev_frame_wait;

	spinlock_t pict_frame_q_lock;
	struct list_head pict_frame_q;
	wait_queue_head_t pict_frame_wait;

	spinlock_t ctrl_status_lock;
	struct list_head ctrl_status_queue;
	wait_queue_head_t ctrl_status_wait;

	//FIH_ADQ,JOE HSU,Update patch
	spinlock_t af_status_lock;
	struct msm_ctrl_cmd_t af_status;
	int af_flag;
	wait_queue_head_t af_status_wait;

	struct msm_fs_pmem_t frame;
	struct msm_fs_pmem_t stats;
};
static struct msm_sync_t msm_sync;

struct msm_device_t {
	struct msm_sync_t *camsync;
	struct msm_camvfe_fn_t vfefn;

	struct device *device;
	struct cdev camera_cdev;

	struct platform_device *pdev;

	uint8_t opencnt;
	const char *apps_id;

	void *cropinfo;
	int  croplen;

	unsigned pict_pp;
};

static DEFINE_MUTEX(msm_lock);
static DEFINE_MUTEX(pict_pp_lock);
DECLARE_MUTEX(msm_sem);
//	mutex_init(&msm_lock);
//	mutex_init(&pict_pp_lock);
//	mutex_init(&msm_sem);

static int msm_pmem_table_add(struct msm_fs_pmem_t *ptype,
	struct msm_pmem_info_t *info, struct file *file, unsigned long paddr,
	unsigned long len, int fd)
{
	struct msm_pmem_region *region =
		kmalloc(sizeof(*region), GFP_KERNEL);

	if (!region)
		return -ENOMEM;

	INIT_HLIST_NODE(&region->list);

	region->type = info->type;
	region->vaddr = info->vaddr;
	region->paddr = paddr;
	region->len = len;
	region->file = file;
	region->y_off = info->y_off;
	region->cbcr_off = info->cbcr_off;
	region->fd = fd;
	region->active = info->active;

	hlist_add_head(&(region->list), &(ptype->pmem_regions));

	return 0;
}

static uint8_t msm_pmem_region_lookup(struct msm_fs_pmem_t *ptype,
	enum msm_pmem_t type, struct msm_pmem_region *reg, uint8_t maxcount)
{
	struct msm_pmem_region *region;
	struct msm_pmem_region *regptr;
	struct hlist_node *node;

	uint8_t rc = 0;

	regptr = reg;

	hlist_for_each_entry(region, node, &(ptype->pmem_regions), list) {

		if ((region->type == type) &&
				(region->active)) {
			*regptr = *region;

			rc += 1;
			if (rc >= maxcount)
				break;

			regptr++;
		}
	}

	return rc;
}

static unsigned long msm_pmem_frame_ptov_lookup(unsigned long pyaddr,
	unsigned long pcbcraddr, uint32_t *yoff, uint32_t *cbcroff, int *fd)
{
	struct msm_pmem_region *region;
	struct hlist_node *node;
	unsigned long rc = 0;

	hlist_for_each_entry(region, node,
		&(msm_sync.frame.pmem_regions), list) {

		if (pyaddr == (region->paddr + region->y_off) &&
		pcbcraddr == (region->paddr + region->cbcr_off) &&
		region->active) {

			/* offset since we could pass vaddr inside
			 * a registerd pmem buffer */
			rc = (unsigned long)(region->vaddr);
			*yoff = region->y_off;
			*cbcroff = region->cbcr_off;
			*fd = region->fd;
			region->active = 0;

			return rc;
		}
	}

	return 0;
}

static unsigned long msm_pmem_stats_ptov_lookup(unsigned long addr, int *fd)
{
	struct msm_pmem_region *region;
	struct hlist_node *node;
	unsigned long rc = 0;

	hlist_for_each_entry(region, node,
		&(msm_sync.stats.pmem_regions), list) {

		if (addr == region->paddr &&
				region->active) {
			/* offset since we could pass vaddr inside a
			*  registered pmem buffer */
			rc = (unsigned long)(region->vaddr);
			*fd = region->fd;
			region->active = 0;
			return rc;
		}
	}

	return 0;
}

static void msm_pmem_frame_vtop_lookup(unsigned long buffer,
	uint32_t yoff, uint32_t cbcroff, int fd, unsigned long *phyaddr)
{
	struct msm_pmem_region *region;
	struct hlist_node *node;

	hlist_for_each_entry(region,
		node, &(msm_sync.frame.pmem_regions), list) {

		if (((unsigned long)(region->vaddr) == buffer) &&
				(region->y_off == yoff) &&
				(region->cbcr_off == cbcroff) &&
				(region->fd == fd) &&
				(region->active == 0)) {

			*phyaddr = region->paddr;
			region->active = 1;

			return;
		}
	}

	*phyaddr = 0;
}

static void msm_pmem_stats_vtop_lookup(unsigned long buffer,
	int fd, unsigned long *phyaddr)
{
	struct msm_pmem_region *region;
	struct hlist_node *node;

	hlist_for_each_entry(region, node,
		&(msm_sync.stats.pmem_regions), list) {

		if (((unsigned long)(region->vaddr) == buffer) &&
				(region->fd == fd) &&
				region->active == 0) {

			*phyaddr = region->paddr;
			region->active = 1;

			return;
		}
	}

	*phyaddr = 0;
}

static long msm_pmem_table_del_proc(struct msm_pmem_info_t *pinfo)
{
	long rc = 0;
	struct msm_pmem_region *region;
	struct hlist_node *node;
	struct hlist_node *n;

	mutex_lock(&msm_sem);//FIH_ADQ,JOE HSU
	switch (pinfo->type) {
	case MSM_PMEM_OUTPUT1:
	case MSM_PMEM_OUTPUT2:
	case MSM_PMEM_THUMBAIL:
	case MSM_PMEM_MAINIMG:
	case MSM_PMEM_RAW_MAINIMG:
		hlist_for_each_entry_safe(region, node, n,
			&(msm_sync.frame.pmem_regions), list) {

			if (pinfo->type == region->type &&
					pinfo->vaddr == region->vaddr &&
					pinfo->fd == region->fd) {
				hlist_del(node);
				put_pmem_file(region->file);
				kfree(region);
			}
		}
		break;

	case MSM_PMEM_AEC_AWB:
	case MSM_PMEM_AF:
		hlist_for_each_entry_safe(region, node, n,
			&(msm_sync.stats.pmem_regions), list) {

			if (pinfo->type == region->type &&
					pinfo->vaddr == region->vaddr &&
					pinfo->fd == region->fd) {
				hlist_del(node);
				put_pmem_file(region->file);
				kfree(region);
			}
		}
		break;

	default:
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&msm_sem);//FIH_ADQ,JOE HSU
	return rc;
}

static long msm_pmem_table_del(void __user *arg)
{
	struct msm_pmem_info_t info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	return msm_pmem_table_del_proc(&info);
}

static long msm_get_frame_proc(struct msm_frame_t *frame)
{
	unsigned long flags;
	long rc = 0;

	struct msm_queue_cmd_t *qcmd = NULL;
	struct msm_vfe_phy_info *pphy;

	spin_lock_irqsave(&(msm_sync.prev_frame_q_lock), flags);

	if (!list_empty(&(msm_sync.prev_frame_q))) {
		qcmd = list_first_entry(&(msm_sync.prev_frame_q),
			struct msm_queue_cmd_t, list);
		list_del(&qcmd->list);
	}

	spin_unlock_irqrestore(&(msm_sync.prev_frame_q_lock), flags);

	if (!qcmd)
		return -EAGAIN;

	pphy = (struct msm_vfe_phy_info *)(qcmd->command);

	frame->buffer =
		msm_pmem_frame_ptov_lookup(pphy->y_phy,
			pphy->cbcr_phy, &(frame->y_off),
			&(frame->cbcr_off), &(frame->fd));

	CDBG("get_fr_proc: y= 0x%x, cbcr= 0x%x, qcmd= 0x%x, virt_addr= 0x%x\n",
		pphy->y_phy, pphy->cbcr_phy, (int) qcmd, (int) frame->buffer);

	kfree(qcmd->command);
	kfree(qcmd);
	return rc;
}

static struct msm_device_t *msm_camera;
static struct class *msm_class;

static long msm_get_frame(void __user *arg)
{
	long rc = 0;
	struct msm_frame_t frame;

	if (copy_from_user(&frame,
				arg,
				sizeof(struct msm_frame_t)))
		return -EFAULT;

	rc = msm_get_frame_proc(&frame);
	if (rc < 0)
		return rc;
		
	if (msm_camera->croplen) {
		if (frame.croplen > msm_camera->croplen)
			return -EINVAL;

		if (copy_to_user((void *)frame.cropinfo,
				&msm_camera->cropinfo,
				msm_camera->croplen))
		return -EFAULT;
	}


	if (copy_to_user((void *)arg,
				&frame, sizeof(struct msm_frame_t)))
		rc = -EFAULT;

	CDBG("Got frame!!!\n");

	return rc;
}

static long msm_enable_vfe(void __user *arg)
{
	long rc = 0;
	struct camera_enable_cmd_t *cfg;
	struct camera_enable_cmd_t cfg_t;

	if (copy_from_user(
				&cfg_t,
				arg,
				sizeof(struct camera_enable_cmd_t)))
		return -EFAULT;

	cfg = kmalloc(sizeof(struct camera_enable_cmd_t),
					GFP_KERNEL);
	if (!cfg)
		return -ENOMEM;

	cfg->name = kmalloc(cfg_t.length, GFP_KERNEL);

	if (!(cfg->name)) {
		kfree(cfg);
		return -ENOMEM;
	}

	if (copy_from_user(cfg->name, (void *)cfg_t.name, cfg_t.length))
		return -EFAULT;

	if (msm_camera->vfefn.vfe_enable)
		rc = msm_camera->vfefn.vfe_enable(cfg);

	CDBG("msm_enable_vfe:returned rc = %ld\n", rc);

	kfree(cfg);
	return rc;
}

static int msm_ctrl_stats_pending(void)
{
	unsigned long flags;
	int yes = 0;

	spin_lock_irqsave(&(msm_sync.ctrl_status_lock),
		flags);

	yes = !list_empty(&(msm_sync.ctrl_status_queue));

	spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock),
		flags);

	CDBG("msm_ctrl_stats_pending, yes = %d\n", yes);
	return yes;
}

static long msm_control(void __user *arg)
{
	unsigned long flags;
	int timeout;
	long rc = 0;

	struct msm_ctrl_cmd_t ctrlcmd_t;
	struct msm_ctrl_cmd_t *ctrlcmd;
	struct msm_queue_cmd_t *qcmd = NULL;

	if (copy_from_user(&ctrlcmd_t,
				arg,
				sizeof(struct msm_ctrl_cmd_t))) {
		rc = -EFAULT;
		goto end;
	}

	ctrlcmd = kmalloc(sizeof(struct msm_ctrl_cmd_t), GFP_ATOMIC);
	if (!ctrlcmd) {
		CDBG("msm_control: cannot allocate buffer ctrlcmd\n");
		rc = -ENOMEM;
		goto end;
	}

	ctrlcmd->value = kmalloc(ctrlcmd_t.length, GFP_ATOMIC);
	if (!ctrlcmd->value) {
		CDBG("msm_control: cannot allocate buffer ctrlcmd->value\n");
		rc = -ENOMEM;
		goto no_mem;
	}

	if (copy_from_user(ctrlcmd->value,
				ctrlcmd_t.value,
				ctrlcmd_t.length)) {
		rc = -EFAULT;
		goto fail;
	}

	ctrlcmd->type = ctrlcmd_t.type;
	ctrlcmd->length = ctrlcmd_t.length;

	qcmd = kmalloc(sizeof(struct msm_queue_cmd_t), GFP_ATOMIC);
	if (!qcmd) {
		rc = -ENOMEM;
		goto fail;
	}

	spin_lock_irqsave(&(msm_sync.msg_event_queue_lock), flags);
	qcmd->type = MSM_CAM_Q_CTRL;
	qcmd->command = ctrlcmd;
	list_add_tail(&qcmd->list, &(msm_sync.msg_event_queue));
	/* wake up config thread */
	wake_up(&(msm_sync.msg_event_wait));
	spin_unlock_irqrestore(&(msm_sync.msg_event_queue_lock), flags);

	/* wait for config status */
	timeout = (int)ctrlcmd_t.timeout_ms;
	CDBG("msm_control, timeout = %d\n", timeout);
	if (timeout > 0) {
		rc = wait_event_timeout(msm_sync.ctrl_status_wait,
					msm_ctrl_stats_pending(),
					msecs_to_jiffies(timeout));

		CDBG("msm_control: rc = %ld\n", rc);

		if (rc == 0) {
			CDBG("msm_control: timed out\n");
			rc = -ETIMEDOUT;
			goto fail;
		}
	} else
		rc = wait_event_interruptible(msm_sync.ctrl_status_wait,
			msm_ctrl_stats_pending());

	if (rc < 0) {
		rc = -EAGAIN;
		goto fail;
	}

	/* control command status is ready */
	spin_lock_irqsave(&(msm_sync.ctrl_status_lock), flags);
	if (!list_empty(&(msm_sync.ctrl_status_queue))) {
		qcmd = list_first_entry(&(msm_sync.ctrl_status_queue),
			struct msm_queue_cmd_t, list);

		if (!qcmd) {
			spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock),
				flags);
			rc = -EAGAIN;
			goto fail;
		}

		list_del(&qcmd->list);
	}
	spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock), flags);

	if (!qcmd->command) {
		ctrlcmd_t.type = 0xFFFF;
		ctrlcmd_t.length = 0xFFFF;
		ctrlcmd_t.status = 0xFFFF;
	} else {

		CDBG("msm_control: length = %d\n",
			((struct msm_ctrl_cmd_t *)(qcmd->command))->length);
		ctrlcmd_t.type =
			((struct msm_ctrl_cmd_t *)(qcmd->command))->type;

		ctrlcmd_t.length =
			((struct msm_ctrl_cmd_t *)(qcmd->command))->length;

		ctrlcmd_t.status =
			((struct msm_ctrl_cmd_t *)(qcmd->command))->status;

		if (ctrlcmd_t.length > 0) {
			if (copy_to_user(ctrlcmd_t.value,
			((struct msm_ctrl_cmd_t *)(qcmd->command))->value,
			((struct msm_ctrl_cmd_t *)(qcmd->command))->length)) {

				CDBG("copy_to_user value failed!\n");
				rc = -EFAULT;
				goto end;
			}

			kfree(((struct msm_ctrl_cmd_t *)
				(qcmd->command))->value);
		}

		if (copy_to_user((void *)arg,
				&ctrlcmd_t,
				sizeof(struct msm_ctrl_cmd_t))) {
			CDBG("copy_to_user ctrlcmd failed!\n");
			rc = -EFAULT;
			goto end;
		}
	}

	goto end;

fail:
	kfree(ctrlcmd->value);

no_mem:
	kfree(ctrlcmd);

end:
	if (qcmd) {
		kfree(qcmd->command);
		kfree(qcmd);
	}

	CDBG("msm_control: end rc = %ld\n", rc);
	return rc;
}

static int msm_stats_pending(void)
{
	unsigned long flags;
	int yes = 0;

	struct msm_queue_cmd_t *qcmd = NULL;

	spin_lock_irqsave(&(msm_sync.msg_event_queue_lock),
		flags);

	if (!list_empty(&(msm_sync.msg_event_queue))) {

		qcmd = list_first_entry(&(msm_sync.msg_event_queue),
			struct msm_queue_cmd_t, list);

		if (qcmd) {

			if ((qcmd->type  == MSM_CAM_Q_CTRL)    ||
					(qcmd->type  == MSM_CAM_Q_VFE_EVT) ||
					(qcmd->type  == MSM_CAM_Q_VFE_MSG) ||
					(qcmd->type  == MSM_CAM_Q_V4L2_REQ)) {
				yes = 1;
			}
		}
	}
	spin_unlock_irqrestore(&(msm_sync.msg_event_queue_lock), flags);

	CDBG("msm_stats_pending, tyes = %d\n", yes);
	return yes;
}

static long msm_get_stats(void __user *arg)
{
	unsigned long flags;
	int           timeout;
	long          rc = 0;

	struct msm_stats_event_ctrl se;

	struct msm_queue_cmd_t *qcmd = NULL;
	struct msm_ctrl_cmd_t  *ctrl = NULL;
	struct msm_vfe_resp_t  *data = NULL;
	struct msm_stats_buf_t stats;

	if (copy_from_user(&se, arg,
				sizeof(struct msm_stats_event_ctrl)))
		return -EFAULT;

	timeout = (int)se.timeout_ms;

	if (timeout > 0) {
		rc =
			wait_event_timeout(
				msm_sync.msg_event_wait,
				msm_stats_pending(),
				msecs_to_jiffies(timeout));

		if (rc == 0) {
			CDBG("msm_get_stats, timeout\n");
			return -ETIMEDOUT;
		}
	} else {
		rc = wait_event_interruptible(msm_sync.msg_event_wait,
			msm_stats_pending());
	}

	if (rc < 0) {
		CDBG("msm_get_stats, rc = %ld\n", rc);
		//return -ERESTARTSYS;
		rc = 0;
		return -ETIMEDOUT;//FIH_ADQ,JOE HSU,Fix restart system ,workaround.
	}

	spin_lock_irqsave(&(msm_sync.msg_event_queue_lock), flags);
	if (!list_empty(&(msm_sync.msg_event_queue))) {
		qcmd = list_first_entry(&(msm_sync.msg_event_queue),
				struct msm_queue_cmd_t, list);

		if (!qcmd) {
			spin_unlock_irqrestore(
				&((msm_sync.msg_event_queue_lock)), flags);
			return -EAGAIN;
		}

		list_del(&qcmd->list);
	}
	spin_unlock_irqrestore(&((msm_sync.msg_event_queue_lock)), flags);

	CDBG("=== received from DSP === %d\n", qcmd->type);

	switch (qcmd->type) {
	case MSM_CAM_Q_VFE_EVT:
	case MSM_CAM_Q_VFE_MSG:
		data = (struct msm_vfe_resp_t *)(qcmd->command);

		/* adsp event and message */
		se.resptype = MSM_CAM_RESP_STAT_EVT_MSG;

		/* 0 - msg from aDSP, 1 - event from mARM */
		se.stats_event.type   = data->evt_msg.type;
		se.stats_event.msg_id = data->evt_msg.msg_id;
		se.stats_event.len    = data->evt_msg.len;

		CDBG("msm_get_stats, qcmd->type = %d\n", qcmd->type);
		CDBG("length = %d\n", se.stats_event.len);
		CDBG("msg_id = %d\n", se.stats_event.msg_id);

		if ((data->type == VFE_MSG_STATS_AF) ||
				(data->type == VFE_MSG_STATS_WE)) {

			stats.buffer =
			msm_pmem_stats_ptov_lookup(data->phy.sbuf_phy,
				&(stats.fd));

			if (copy_to_user((void *)(se.stats_event.data),
				&stats, sizeof(struct msm_stats_buf_t))) {

				rc = -EFAULT;
				goto failure;
			}
		} else if ((data->evt_msg.len > 0) &&
				(data->type == VFE_MSG_GENERAL)) {

			if (copy_to_user((void *)(se.stats_event.data),
					data->evt_msg.data,
					data->evt_msg.len))
				rc = -EFAULT;

		} else if (data->type == VFE_MSG_OUTPUT1 ||
			data->type == VFE_MSG_OUTPUT2) {
//FIH_ADQ,JOE HSU,Update patch
#if 0
			uint32_t pp_en;
			struct msm_postproc_t buf;
			struct msm_pmem_region region;
			down(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
			pp_en = msm_camera->pict_pp;
			up(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
			if (pp_en & PP_PREV) {
				CDBG("Started Preview post processing. pp_en = %d \n", pp_en);
				buf.fmain.buffer =
					msm_pmem_frame_ptov_lookup(data->phy.y_phy,
					data->phy.cbcr_phy, &buf.fmain.y_off,
					&buf.fmain.cbcr_off, &buf.fmain.fd);
				if (buf.fmain.buffer) {
          CDBG("%s: Copy_to_user: buf=0x%x fd=%d y_o=%d c_o=%d\n", __func__,
            buf.fmain.buffer, buf.fmain.fd, buf.fmain.y_off, buf.fmain.cbcr_off);
 			if (copy_to_user((void *)(se.stats_event.data),
						&(buf.fmain),
						sizeof(struct msm_frame_t)))
							rc = -EFAULT;
					} else
							rc = -EFAULT;
			} else {
#endif				
			if (copy_to_user((void *)(se.stats_event.data),
					data->extdata,
					data->extlen))
				rc = -EFAULT;
//			}
		} else if (data->type == VFE_MSG_SNAPSHOT) {

			uint32_t pp_en;//FIH_ADQ,JOE HSU,Update patch
			struct msm_postproc_t buf;
			struct msm_pmem_region region;

			mutex_lock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
			pp_en = msm_camera->pict_pp;
			mutex_unlock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning

			if (pp_en & PP_SNAP) { //FIH_ADQ,JOE HSU,Update patch
				buf.fmnum =
					msm_pmem_region_lookup(
						&(msm_sync.frame),
						MSM_PMEM_MAINIMG,
						&region, 1);

				if (buf.fmnum == 1) {
					buf.fmain.buffer = (unsigned long)region.vaddr;//FIH_ADQ,JOE HSU
					buf.fmain.y_off  = region.y_off;
					buf.fmain.cbcr_off = region.cbcr_off;
					buf.fmain.fd = region.fd;
				} else {
					buf.fmnum =
					msm_pmem_region_lookup(
						&(msm_sync.frame),
						MSM_PMEM_RAW_MAINIMG,
						&region, 1);

					if (buf.fmnum == 1) {
						buf.fmain.path =
							MSM_FRAME_PREV_2;
						buf.fmain.buffer = (unsigned long)region.vaddr;//FIH_ADQ,JOE HSU
						buf.fmain.fd = region.fd;
					}
				}

				if (copy_to_user((void *)(se.stats_event.data),
					&buf, sizeof(struct msm_postproc_t))) {

					rc = -EFAULT;
					goto failure;
				}
			}
			CDBG("SNAPSHOT copy_to_user!\n");
		}
		break;

	case MSM_CAM_Q_CTRL:{
		/* control command from control thread */
		ctrl = (struct msm_ctrl_cmd_t *)(qcmd->command);

		CDBG("msm_get_stats, qcmd->type = %d\n", qcmd->type);
		CDBG("length = %d\n", ctrl->length);

		if (ctrl->length > 0) {
			if (copy_to_user((void *)(se.ctrl_cmd.value),
						ctrl->value,
						ctrl->length)) {

				rc = -EFAULT;
				goto failure;
			}
		}

		se.resptype = MSM_CAM_RESP_CTRL;

		/* what to control */
		se.ctrl_cmd.type = ctrl->type;
		se.ctrl_cmd.length = ctrl->length;
	} /* MSM_CAM_Q_CTRL */
		break;

	case MSM_CAM_Q_V4L2_REQ: {
		/* control command from v4l2 client */
		ctrl = (struct msm_ctrl_cmd_t *)(qcmd->command);

		CDBG("msm_get_stats, qcmd->type = %d\n", qcmd->type);
		CDBG("length = %d\n", ctrl->length);

		if (ctrl->length > 0) {
			if (copy_to_user((void *)(se.ctrl_cmd.value),
					ctrl->value, ctrl->length)) {

				rc = -EFAULT;
				goto failure;
			}
		}

		/* 2 tells config thread this is v4l2 request */
		se.resptype = MSM_CAM_RESP_V4L2;

		/* what to control */
		se.ctrl_cmd.type   = ctrl->type;
		se.ctrl_cmd.length = ctrl->length;
	} /* MSM_CAM_Q_V4L2_REQ */
		break;

	default:
		rc = -EFAULT;
		goto failure;
	} /* switch qcmd->type */

	if (copy_to_user((void *)arg, &se, sizeof(se)))
		rc = -EFAULT;

failure:
	if (qcmd) {

		if (qcmd->type == MSM_CAM_Q_VFE_MSG)
			kfree(((struct msm_vfe_resp_t *)
				(qcmd->command))->evt_msg.data);

		kfree(qcmd->command);
		kfree(qcmd);
	}

	CDBG("msm_get_stats: end rc = %ld\n", rc);
	return rc;
}

static long msm_ctrl_cmd_done(void __user *arg)
{
	unsigned long flags;
	long rc = 0;

	struct msm_ctrl_cmd_t ctrlcmd_t;
	struct msm_ctrl_cmd_t *ctrlcmd;
	struct msm_queue_cmd_t *qcmd = NULL;

	if (copy_from_user(&ctrlcmd_t,
				arg,
				sizeof(struct msm_ctrl_cmd_t)))
		return -EFAULT;

	ctrlcmd = kzalloc(sizeof(struct msm_ctrl_cmd_t), GFP_ATOMIC);
	if (!ctrlcmd) {
		rc = -ENOMEM;
		goto end;
	}

	if (ctrlcmd_t.length > 0) {
		ctrlcmd->value = kmalloc(ctrlcmd_t.length, GFP_ATOMIC);
		if (!ctrlcmd->value) {
			rc = -ENOMEM;
			goto no_mem;
		}

		if (copy_from_user(ctrlcmd->value,
					(void *)ctrlcmd_t.value,
					ctrlcmd_t.length)) {

			rc = -EFAULT;
			goto fail;
		}
	} else
		ctrlcmd->value = NULL;

	ctrlcmd->type = ctrlcmd_t.type;
	ctrlcmd->length = ctrlcmd_t.length;
	ctrlcmd->status = ctrlcmd_t.status;

	qcmd = kmalloc(sizeof(*qcmd), GFP_ATOMIC);
	if (!qcmd) {
		rc = -ENOMEM;
		goto fail;
	}

	qcmd->command = (void *)ctrlcmd;

	goto end;

fail:
	kfree(ctrlcmd->value);
no_mem:
	kfree(ctrlcmd);
end:
	CDBG("msm_ctrl_cmd_done: end rc = %ld\n", rc);
	if (rc == 0) {
		/* wake up control thread */
		spin_lock_irqsave(&(msm_sync.ctrl_status_lock), flags);
		list_add_tail(&qcmd->list, &(msm_sync.ctrl_status_queue));
		spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock), flags);
		wake_up(&(msm_sync.ctrl_status_wait));
	}
	return rc;
}

static long msm_config_vfe(void __user *arg)
{
	struct msm_vfe_cfg_cmd_t cfgcmd_t;
	struct msm_pmem_region region[8];
	struct axidata_t axi_data;
	long rc = 0;

	memset(&axi_data, 0, sizeof(axi_data));

	if (copy_from_user(&cfgcmd_t, arg, sizeof(cfgcmd_t)))
		return -EFAULT;

	if (cfgcmd_t.cmd_type == CMD_STATS_ENABLE) {
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&(msm_sync.stats),
		MSM_PMEM_AEC_AWB, &region[0],
		NUM_WB_EXP_STAT_OUTPUT_BUFFERS);
		axi_data.region = &region[0];

	} else if (cfgcmd_t.cmd_type == CMD_STATS_AF_ENABLE) {
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&(msm_sync.stats),
			MSM_PMEM_AF, &region[0],
			NUM_AF_STAT_OUTPUT_BUFFERS);
		axi_data.region = &region[0];
	}

	if (msm_camera->vfefn.vfe_config)
		rc =  msm_camera->vfefn.vfe_config(&cfgcmd_t, &(axi_data));

	return rc;
}

static long msm_frame_axi_cfg(struct msm_vfe_cfg_cmd_t *cfgcmd_t)
{
	long rc = 0;
	struct axidata_t axi_data;
	struct msm_pmem_region region[8];
	enum msm_pmem_t mtype;

	memset(&axi_data, 0, sizeof(axi_data));

	switch (cfgcmd_t->cmd_type) {
	case CMD_AXI_CFG_OUT1:
		mtype = MSM_PMEM_OUTPUT1;
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&(msm_sync.frame), mtype,
				&region[0], 8);
		break;

	case CMD_AXI_CFG_OUT2:
		mtype = MSM_PMEM_OUTPUT2;
		axi_data.bufnum2 =
			msm_pmem_region_lookup(&(msm_sync.frame), mtype,
				&region[0], 8);
		break;

	case CMD_AXI_CFG_SNAP_O1_AND_O2:
		mtype = MSM_PMEM_THUMBAIL;
		axi_data.bufnum1 =
			msm_pmem_region_lookup(&(msm_sync.frame), mtype,
				&region[0], 8);

		mtype = MSM_PMEM_MAINIMG;
		axi_data.bufnum2 =
			msm_pmem_region_lookup(&(msm_sync.frame), mtype,
				&region[axi_data.bufnum1], 8);
		break;

	case CMD_RAW_PICT_AXI_CFG:
		mtype = MSM_PMEM_RAW_MAINIMG;
		axi_data.bufnum2 =
			msm_pmem_region_lookup(&(msm_sync.frame), mtype,
				&region[0], 8);
		break;

	default:
		break;
	}

	axi_data.region = &region[0];

	/* send the AXI configuration command to driver */
	if (msm_camera->vfefn.vfe_config)
		rc =  msm_camera->vfefn.vfe_config(cfgcmd_t, &axi_data);

	return rc;
}

static long msm_get_sensor_info(void __user *arg)
{
	long rc = 0;
	int  cnt;
	struct msm_camsensor_info_t info;
	struct msm_camera_device_platform_data *pdata;

	if (copy_from_user(&info,
			arg,
			sizeof(struct msm_camsensor_info_t)))
		return -EFAULT;

	pdata = msm_camera->pdev->dev.platform_data;

	info.num = 0;

	for (cnt = 0; cnt < pdata->snum; cnt++) {

		CDBG("sensor_name %s\n", pdata->sinfo[cnt].sensor_name);

		memcpy(&(info.sensor[cnt].name[0]),
				pdata->sinfo[cnt].sensor_name,
		MAX_SENSOR_NAME);

		info.num += 1;
	}

	/* copy back to user space */
	if (copy_to_user((void *)arg,
			&info,
			sizeof(struct msm_camsensor_info_t))) {

		CDBG("get senso copy to user failed!\n");
		rc = -EFAULT;
	}

	return rc;
}

static long msm_put_frame_buf_proc(struct msm_frame_t *pb)
{
	unsigned long pphy;
	struct msm_vfe_cfg_cmd_t cfgcmd_t;

	long rc = 0;

	msm_pmem_frame_vtop_lookup(pb->buffer,
		pb->y_off, pb->cbcr_off,
		pb->fd, &pphy);

	CDBG("rel: vaddr = 0x%lx, paddr = 0x%lx\n",
		pb->buffer, pphy);

	if (pphy != 0) {

		cfgcmd_t.cmd_type = CMD_FRAME_BUF_RELEASE;
		cfgcmd_t.value    = (void *)pb;

		if (msm_camera->vfefn.vfe_config)
			rc =
				msm_camera->vfefn.vfe_config(&cfgcmd_t, &pphy);
	} else
		rc = -EFAULT;

	return rc;
}

static long msm_put_frame_buffer(void __user *arg)
{
	struct msm_frame_t buf_t;

	if (copy_from_user(&buf_t,
				arg,
				sizeof(struct msm_frame_t)))
		return -EFAULT;

	return msm_put_frame_buf_proc(&buf_t);
}

static long msm_register_pmem_proc(struct msm_pmem_info_t *pinfo)
{
	unsigned long paddr, len, rc = 0;
	struct file   *file;
	unsigned long vstart;

	mutex_lock(&msm_sem);

	CDBG("Here1 ==> reg: type = %d, paddr = 0x%lx, vaddr = 0x%lx\n",
		pinfo->type, paddr, (unsigned long)pinfo->vaddr);

	get_pmem_file(pinfo->fd, &paddr, &vstart, &len, &file);

	switch (pinfo->type) {
	case MSM_PMEM_OUTPUT1:
	case MSM_PMEM_OUTPUT2:
	case MSM_PMEM_THUMBAIL:
	case MSM_PMEM_MAINIMG:
	case MSM_PMEM_RAW_MAINIMG:
		rc = msm_pmem_table_add(&(msm_sync.frame),
			pinfo, file, paddr, len, pinfo->fd);
		break;

	case MSM_PMEM_AEC_AWB:
	case MSM_PMEM_AF:
		rc = msm_pmem_table_add(&(msm_sync.stats),
			pinfo, file, paddr, len, pinfo->fd);
		break;

	default:
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&msm_sem);
	return rc;
}

static long msm_register_pmem(void __user *arg)
{
	struct msm_pmem_info_t info;

	if (copy_from_user(&info, arg, sizeof(info)))
		return -EFAULT;

	return msm_register_pmem_proc(&info);
}

static long msm_stats_axi_cfg(struct msm_vfe_cfg_cmd_t *cfgcmd_t)
{
	long rc = 0;
	struct axidata_t axi_data;

	struct msm_pmem_region region[3];
	enum msm_pmem_t mtype = MSM_PMEM_MAX;

	memset(&axi_data, 0, sizeof(axi_data));

	if (cfgcmd_t->cmd_type == CMD_STATS_AXI_CFG)
		mtype = MSM_PMEM_AEC_AWB;
	else if (cfgcmd_t->cmd_type == CMD_STATS_AF_AXI_CFG)
		mtype = MSM_PMEM_AF;

	axi_data.bufnum1 =
		msm_pmem_region_lookup(&(msm_sync.stats), mtype,
			&region[0], NUM_WB_EXP_STAT_OUTPUT_BUFFERS);

	axi_data.region = &region[0];

	/* send the AEC/AWB STATS configuration command to driver */
	if (msm_camera->vfefn.vfe_config)
		rc =  msm_camera->vfefn.vfe_config(cfgcmd_t, &axi_data);

	return rc;
}

static long msm_put_stats_buffer(void __user *arg)
{
	long rc = 0;

	struct msm_stats_buf_t buf;
	unsigned long pphy;
	struct msm_vfe_cfg_cmd_t cfgcmd_t;

	if (copy_from_user(&buf, arg,
				sizeof(struct msm_stats_buf_t)))
		return -EFAULT;

  CDBG("msm_put_stats_buffer\n");
	msm_pmem_stats_vtop_lookup(buf.buffer,
		buf.fd, &pphy);

	if (pphy != 0) {

		if (buf.type == STAT_AEAW)
			cfgcmd_t.cmd_type = CMD_STATS_BUF_RELEASE;
		else if (buf.type == STAT_AF)
			cfgcmd_t.cmd_type = CMD_STATS_AF_BUF_RELEASE;
		else {
			rc = -EINVAL;
			goto put_done;
		}

		cfgcmd_t.value    = (void *)&buf;

		if (msm_camera->vfefn.vfe_config)
			rc =
			msm_camera->vfefn.vfe_config(&cfgcmd_t, &pphy);
	} else
		rc = -EFAULT;

put_done:
	return rc;
}

static long msm_axi_config(void __user *arg)
{
	long rc = 0;
	struct msm_vfe_cfg_cmd_t cfgcmd_t;

	if (copy_from_user(&cfgcmd_t, arg, sizeof(cfgcmd_t)))
		return -EFAULT;

	switch (cfgcmd_t.cmd_type) {
	case CMD_AXI_CFG_OUT1:
	case CMD_AXI_CFG_OUT2:
	case CMD_AXI_CFG_SNAP_O1_AND_O2:
	case CMD_RAW_PICT_AXI_CFG:
		return msm_frame_axi_cfg(&cfgcmd_t);

	case CMD_STATS_AXI_CFG:
	case CMD_STATS_AF_AXI_CFG:
		return msm_stats_axi_cfg(&cfgcmd_t);

	default:
		rc = -EFAULT;
	}

	return rc;
}

static int msm_camera_pict_pending(void)
{
	unsigned long flags;
	int yes = 0;

	struct msm_queue_cmd_t *qcmd = NULL;

	spin_lock_irqsave(&(msm_sync.pict_frame_q_lock),
		flags);

	if (!list_empty(&(msm_sync.pict_frame_q))) {

		qcmd =
			list_first_entry(&(msm_sync.pict_frame_q),
				struct msm_queue_cmd_t, list);

		if (qcmd) {
			if (qcmd->type  == MSM_CAM_Q_VFE_MSG)
				yes = 1;
		}
	}
	spin_unlock_irqrestore(&(msm_sync.pict_frame_q_lock), flags);

	CDBG("msm_camera_pict_pending, yes = %d\n", yes);

	//FIH_ADQ,JOE HSU,Fix restart system ,workaround.	
	if (yes <= 0)
	   yes =0;
	   
	return yes;
}

static long msm_get_pict_proc(struct msm_ctrl_cmd_t *ctrl)
{
	unsigned long flags;
	long rc = 0;
	int tm;

	struct msm_queue_cmd_t *qcmd = NULL;

	tm = (int)ctrl->timeout_ms;

	if (tm > 0) {
		rc =
			wait_event_timeout(
				msm_sync.pict_frame_wait,
				msm_camera_pict_pending(),
				msecs_to_jiffies(tm));

		if (rc == 0) {
			CDBG("msm_camera_get_picture, tm\n");
			return -ETIMEDOUT;
		}
	} else
		rc = wait_event_interruptible(
					msm_sync.pict_frame_wait,
					msm_camera_pict_pending());

	if (rc < 0) {
		CDBG("msm_camera_get_picture, rc = %ld\n", rc);
		//return -ERESTARTSYS;
		rc = 0;//FIH_ADQ,JOE HSU,Fix restart system ,workaround.
		return -ETIMEDOUT;
	}

	spin_lock_irqsave(&(msm_sync.pict_frame_q_lock), flags);
	if (!list_empty(&(msm_sync.pict_frame_q))) {
		qcmd = list_first_entry(&(msm_sync.pict_frame_q),
			struct msm_queue_cmd_t, list);
		list_del(&qcmd->list);
	}
	spin_unlock_irqrestore(&(msm_sync.pict_frame_q_lock), flags);

	if (qcmd->command != NULL) {
		ctrl->type =
		((struct msm_ctrl_cmd_t *)(qcmd->command))->type;

		ctrl->status =
		((struct msm_ctrl_cmd_t *)(qcmd->command))->status;

		kfree(qcmd->command);
	} else {
		ctrl->type = 0xFFFF;
		ctrl->status = 0xFFFF;
	}

	kfree(qcmd);

	return rc;
}

static long msm_get_pic(void __user *arg)
{
	struct msm_ctrl_cmd_t ctrlcmd_t;

	if (copy_from_user(&ctrlcmd_t,
				arg,
				sizeof(struct msm_ctrl_cmd_t)))
		return -EFAULT;

	if (msm_get_pict_proc(&ctrlcmd_t) < 0)
		return -EFAULT;
		
	if (msm_camera->croplen) {
		if (ctrlcmd_t.length < msm_camera->croplen)
			return -EINVAL;

		if (copy_to_user(ctrlcmd_t.value,
				&msm_camera->cropinfo,
				msm_camera->croplen))
			return -EINVAL;
	}


	if (copy_to_user((void *)arg,
		&ctrlcmd_t,
		sizeof(struct msm_ctrl_cmd_t)))
		return -EFAULT;
	return 0;
}

static long msm_set_crop(void __user *arg)
{
	int timeout;
	struct crop_info_t crop;

	if (copy_from_user(&crop,
				arg,
				sizeof(struct crop_info_t)))
		return -EFAULT;

	if (!msm_camera->croplen) {
		msm_camera->cropinfo = kmalloc(crop.len, GFP_KERNEL);

		if (!msm_camera->cropinfo)
			return -ENOMEM;
	} else if (msm_camera->croplen < crop.len)
		return -EINVAL;

	if (copy_from_user(&msm_camera->cropinfo,
				crop.info,
				crop.len)) {
		kfree(msm_camera->cropinfo);
		return -EFAULT;
	}

	msm_camera->croplen = crop.len;

	return 0;
}

static long msm_pict_pp_done(void __user *arg)
{
	struct msm_ctrl_cmd_t ctrlcmd_t;
	struct msm_ctrl_cmd_t *ctrlcmd = NULL;
	struct msm_queue_cmd_t *qcmd = NULL;
	unsigned long flags;
	long rc = 0;

	uint32_t pp_en;//FIH_ADQ,JOE HSU,Update patch

	mutex_lock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
	pp_en = msm_camera->pict_pp;
	mutex_unlock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
        CDBG("%s: %d is done\n", __func__, pp_en);

	if (!pp_en)
		return -EINVAL;

	if (copy_from_user(&ctrlcmd_t,
				arg,
				sizeof(struct msm_ctrl_cmd_t))) {
		rc = -EFAULT;
		goto pp_done;
	}

	qcmd =
		kmalloc(sizeof(struct msm_queue_cmd_t),
						GFP_ATOMIC);
	if (!qcmd) {
		rc = -ENOMEM;
		goto pp_fail;
	}

	ctrlcmd = kzalloc(sizeof(struct msm_ctrl_cmd_t), GFP_ATOMIC);
	if (!ctrlcmd) {
		rc = -ENOMEM;
		goto pp_done;
	}

	ctrlcmd->type = ctrlcmd_t.type;
	ctrlcmd->status = ctrlcmd_t.status;

pp_done:
	qcmd->type = MSM_CAM_Q_VFE_MSG;
	qcmd->command = ctrlcmd;

	spin_lock_irqsave(&(msm_sync.pict_frame_q_lock), flags);
	list_add_tail(&qcmd->list, &(msm_sync.pict_frame_q));
	spin_unlock_irqrestore(&(msm_sync.pict_frame_q_lock), flags);
	wake_up(&(msm_sync.pict_frame_wait));

pp_fail:
	return rc;
}

//FIH_ADQ,JOE HSU,Update patch
static int msm_af_status_pending(struct msm_device_t *msm)
{
	int rc;
	unsigned long flags;
	spin_lock_irqsave(&msm_sync.af_status_lock, flags);
	rc = msm_sync.af_flag;
	spin_unlock_irqrestore(&msm_sync.af_status_lock, flags);
	return rc;
}
static long msm_af_control(void __user *arg,
	struct msm_device_t *msm)
{
	unsigned long flags;
	int timeout;
	long rc = 0;
	if (copy_from_user(&msm_sync.af_status,
			arg, sizeof(struct msm_ctrl_cmd_t))) {
		rc = -EFAULT;
		goto end;
	}
	timeout = (int)msm_sync.af_status.timeout_ms;
	CDBG("msm_af_control, timeout = %d\n", timeout);
	if (timeout > 0) {
		rc = wait_event_timeout(msm_sync.af_status_wait,
			msm_af_status_pending(msm),
			msecs_to_jiffies(timeout));
		CDBG("msm_af_control: rc = %ld\n", rc);
		if (rc == 0) {
			CDBG("msm_af_control: timed out\n");
			rc = -ETIMEDOUT;
			goto end;
		}
	} else
		rc = wait_event_interruptible(msm_sync.af_status_wait,
			msm_af_status_pending(msm));
	if (rc < 0) {
		rc = -EAGAIN;
		goto end;
	}
	spin_lock_irqsave(&msm_sync.af_status_lock, flags);
	if (msm_sync.af_flag < 0) {
		msm_sync.af_status.type = 0xFFFF;
		msm_sync.af_status.status = 0xFFFF;
	}
	msm_sync.af_flag = 0;
	spin_unlock_irqrestore(&msm_sync.af_status_lock, flags);
	if (copy_to_user((void *)arg,
			&msm_sync.af_status,
			sizeof(struct msm_ctrl_cmd_t))) {
		CDBG("copy_to_user ctrlcmd failed!\n");
		rc = -EFAULT;
	}

end:
	CDBG("msm_control: end rc = %ld\n", rc);
	return rc;
}
static long msm_af_control_done(void __user *arg,
	struct msm_device_t *msm)
{
	unsigned long flags;
	long rc = 0;
	rc = copy_from_user(&msm_sync.af_status,
		arg, sizeof(struct msm_ctrl_cmd_t));
	spin_lock_irqsave(&msm_sync.af_status_lock, flags);
	msm_sync.af_flag = (rc == 0 ? 1 : -1);
	spin_unlock_irqrestore(&msm_sync.af_status_lock, flags);
	wake_up(&msm_sync.af_status_wait);
	return rc;
}


static long msm_ioctl(struct file *filep, unsigned int cmd,
	unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct msm_device_t *camdev;

	CDBG("!!! msm_ioctl !!!, cmd = %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case MSM_CAM_IOCTL_GET_SENSOR_INFO:
		return msm_get_sensor_info(argp);

	case MSM_CAM_IOCTL_REGISTER_PMEM:
		return msm_register_pmem(argp);

	case MSM_CAM_IOCTL_UNREGISTER_PMEM:
		return msm_pmem_table_del(argp);

	case MSM_CAM_IOCTL_CTRL_COMMAND:
		/* Coming from control thread, may need to wait for
		* command status */
		return msm_control(argp);

	case MSM_CAM_IOCTL_CONFIG_VFE:
		/* Coming from config thread for update */
		return msm_config_vfe(argp);

	case MSM_CAM_IOCTL_GET_STATS:
		/* Coming from config thread wait
		* for vfe statistics and control requests */
		return msm_get_stats(argp);

	case MSM_CAM_IOCTL_GETFRAME:
		/* Coming from frame thread to get frame
		* after SELECT is done */
		return msm_get_frame(argp);

	case MSM_CAM_IOCTL_ENABLE_VFE:
		/* This request comes from control thread:
		* enable either QCAMTASK or VFETASK */
		return msm_enable_vfe(argp);

	case MSM_CAM_IOCTL_CTRL_CMD_DONE:
		/* Config thread notifies the result of contrl command */
		return msm_ctrl_cmd_done(argp);

	case MSM_CAM_IOCTL_VFE_APPS_RESET:
		msm_camio_vfe_blk_reset();
		return 0;

	case MSM_CAM_IOCTL_RELEASE_FRAMEE_BUFFER:
		return msm_put_frame_buffer(argp);

	case MSM_CAM_IOCTL_RELEASE_STATS_BUFFER:
		return msm_put_stats_buffer(argp);

	case MSM_CAM_IOCTL_AXI_CONFIG:
		return msm_axi_config(argp);

	case MSM_CAM_IOCTL_GET_PICTURE:
		return msm_get_pic(argp);

	case MSM_CAM_IOCTL_SET_CROP:
		return msm_set_crop(argp);

	case MSM_CAM_IOCTL_PICT_PP: {
//FIH_ADQ,JOE HSU,Update patch
		uint32_t pp;
		if (copy_from_user(&pp, argp, sizeof(pp)))
 			return -EFAULT;
 		mutex_lock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
		msm_camera->pict_pp = pp;
 		mutex_unlock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
		CDBG("%s: case: MSM_CAM_IOCTL_PICT_PP: pp = %d \n", __func__, pp);
		return 0;
	}

	case MSM_CAM_IOCTL_PICT_PP_DONE:
		return msm_pict_pp_done(argp);
//FIH_ADQ,JOE HSU,Update patch
	case MSM_CAM_IOCTL_AF_CTRL:
		return msm_af_control(argp, camdev);

	case MSM_CAM_IOCTL_AF_CTRL_DONE:
		return msm_af_control_done(argp, camdev);


	default:
		break;
	}

	return -EINVAL;
}

static int msm_frame_pending(void)
{
	unsigned long flags;
	int yes = 0;

	struct msm_queue_cmd_t *qcmd = NULL;

	spin_lock_irqsave(&(msm_sync.prev_frame_q_lock), flags);

	if (!list_empty(&(msm_sync.prev_frame_q))) {

		qcmd = list_first_entry(&(msm_sync.prev_frame_q),
			struct msm_queue_cmd_t, list);

		if (!qcmd)
			yes = 0;
		else {
			yes = 1;
			CDBG("msm_frame_pending: yes = %d\n",
				yes);
		}
	}

	spin_unlock_irqrestore(&(msm_sync.prev_frame_q_lock), flags);

	CDBG("msm_frame_pending, yes = %d\n", yes);
	return yes;
}

static int msm_release(struct inode *node, struct file *filep)
{
	struct msm_pmem_region *region;
	struct hlist_node *hnode;
	struct hlist_node *n;
	struct msm_queue_cmd_t *qcmd = NULL;
	unsigned long flags;

	mutex_lock(&msm_lock);//FIH_ADQ,JOE HSU,Mutex warning
	msm_camera->opencnt -= 1;
	mutex_unlock(&msm_lock);//FIH_ADQ,JOE HSU,Mutex warning

	if (!msm_camera->opencnt) {
		/* need to clean up
		 * system resource */
		if (msm_camera->vfefn.vfe_release)
			msm_camera->vfefn.vfe_release(msm_camera->pdev);
#if 1
		if (msm_camera->croplen) {
			kfree(msm_camera->cropinfo);
			msm_camera->croplen = 0;
		}

		hlist_for_each_entry_safe(region, hnode, n,
			&(msm_sync.frame.pmem_regions), list) {

			hlist_del(hnode);
			put_pmem_file(region->file);
			kfree(region);
		}

		hlist_for_each_entry_safe(region, hnode, n,
			&(msm_sync.stats.pmem_regions), list) {

			hlist_del(hnode);
			put_pmem_file(region->file);
			kfree(region);
		}

		while (msm_ctrl_stats_pending()) {
			spin_lock_irqsave(&(msm_sync.ctrl_status_lock),
				flags);
			qcmd = list_first_entry(&(msm_sync.ctrl_status_queue),
				struct msm_queue_cmd_t, list);
			spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock),
				flags);
//FIH_ADQ,JOE HSU
			if (qcmd) {
				list_del(&qcmd->list);
				if (qcmd->type == MSM_CAM_Q_VFE_MSG)
					kfree(((struct msm_vfe_resp_t *)
						(qcmd->command))->evt_msg.data);
				kfree(qcmd->command);
				kfree(qcmd);
			}
		};

		while (msm_stats_pending()) {
			spin_lock_irqsave(&(msm_sync.msg_event_queue_lock),
				flags);
			qcmd = list_first_entry(&(msm_sync.msg_event_queue),
				struct msm_queue_cmd_t, list);
			spin_unlock_irqrestore(&(msm_sync.msg_event_queue_lock),
				flags);

			if (qcmd) {
				list_del(&qcmd->list);
				kfree(qcmd->command);
				kfree(qcmd);
			}
		};

		while (msm_camera_pict_pending()) {
			spin_lock_irqsave(&(msm_sync.pict_frame_q_lock),
				flags);
			qcmd = list_first_entry(&(msm_sync.pict_frame_q),
				struct msm_queue_cmd_t, list);
			spin_unlock_irqrestore(&(msm_sync.pict_frame_q_lock),
				flags);

			if (qcmd) {
				list_del(&qcmd->list);
				kfree(qcmd->command);
				kfree(qcmd);
			}
		};

		while (msm_frame_pending()) {
			spin_lock_irqsave(&(msm_sync.prev_frame_q_lock),
				flags);
			qcmd = list_first_entry(&(msm_sync.prev_frame_q),
				struct msm_queue_cmd_t, list);
			spin_unlock_irqrestore(&(msm_sync.prev_frame_q_lock),
				flags);

			if (qcmd) {
				list_del(&qcmd->list);
				kfree(qcmd->command);
				kfree(qcmd);
			}
		};
#endif
		CDBG("msm_release completed!\n");
	}

	return 0;
}

static ssize_t msm_read(struct file *filep, char __user *arg,
	size_t size, loff_t *loff)
{
	return 0;
}

static ssize_t msm_write(struct file *filep, const char __user *arg,
	size_t size, loff_t *loff)
{
	return 0;
}

unsigned int msm_poll(struct file *filep,
	struct poll_table_struct *pll_table)
{
	struct msm_queue_cmd_t *qcmd = NULL;
	unsigned long flags;
//FIH_ADQ ,JOE HSU
	while (msm_camera_pict_pending()) {
		spin_lock_irqsave(&(msm_sync.pict_frame_q_lock),
			flags);
		qcmd = list_first_entry(&(msm_sync.pict_frame_q),
			struct msm_queue_cmd_t, list);
		spin_unlock_irqrestore(&(msm_sync.pict_frame_q_lock),
			flags);

		if (qcmd) {
			list_del(&qcmd->list);
			kfree(qcmd->command);
			kfree(qcmd);
		}
	};

	poll_wait(filep, &(msm_sync.prev_frame_wait), pll_table);
	if (msm_frame_pending())
		/* frame ready */
		return POLLIN | POLLRDNORM;

	return 0;
}

static void msm_vfe_sync(struct msm_vfe_resp_t *vdata,
	 enum msm_queut_t qtype, void *syncdata)
{
	struct msm_queue_cmd_t *qcmd = NULL;
	struct msm_queue_cmd_t *qcmd_frame = NULL;
	struct msm_vfe_phy_info *fphy;

//        uint32_t pp;//FIH_ADQ,JOE HSU,Update patch
	unsigned long flags;
	
//FIH_ADQ,JOE HSU
	struct msm_device_t *msm =
		(struct msm_device_t *)syncdata;

	if (!msm)
		return;

	qcmd = kmalloc(sizeof(struct msm_queue_cmd_t),
					GFP_ATOMIC);
	if (!qcmd) {
		CDBG("evt_msg: cannot allocate buffer\n");
		goto mem_fail1;
	}

	if (qtype == MSM_CAM_Q_VFE_EVT) {
		qcmd->type    = MSM_CAM_Q_VFE_EVT;
	} else if (qtype == MSM_CAM_Q_VFE_MSG) {
//		qcmd->type = MSM_CAM_Q_VFE_MSG;      //FIH_ADQ,JOE HSU,Update patch
		if (vdata->type == VFE_MSG_OUTPUT1 ||
		    vdata->type == VFE_MSG_OUTPUT2) {
//FIH_ADQ,JOE HSU,Update patch
//			mutex_lock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
//			pp = msm_camera->pict_pp;
//			mutex_unlock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning

//			if (pp & PP_PREV)
//				goto sync_done;

			qcmd_frame =
				kmalloc(sizeof(struct msm_queue_cmd_t),
					GFP_ATOMIC);
			if (!qcmd_frame)
				goto mem_fail2;
			fphy = kmalloc(sizeof(struct msm_vfe_phy_info),
				GFP_ATOMIC);
			if (!fphy)
				goto mem_fail3;

			*fphy = vdata->phy;

			qcmd_frame->type    = MSM_CAM_Q_VFE_MSG;
			qcmd_frame->command = fphy;

			CDBG("qcmd_frame= 0x%x phy_y= 0x%x, phy_cbcr= 0x%x\n",
			(int) qcmd_frame, fphy->y_phy, fphy->cbcr_phy);//FIH_ADQ,JOE HSU,Update patch

			spin_lock_irqsave(&(msm_sync.prev_frame_q_lock),
				flags);

			list_add_tail(&qcmd_frame->list,
				&(msm_sync.prev_frame_q));

			spin_unlock_irqrestore(&(msm_sync.prev_frame_q_lock),
				flags);

			wake_up(&(msm_sync.prev_frame_wait));

			CDBG("waked up frame thread\n");

		} else if (vdata->type == VFE_MSG_SNAPSHOT) {
			unsigned pp;//FIH_ADQ,JOE HSU,Update patch
			mutex_lock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
			pp = msm_camera->pict_pp;
			mutex_unlock(&pict_pp_lock);//FIH_ADQ,JOE HSU,Mutex warning
//FIH_ADQ,JOE HSU,Update patch			
//			if ((pp & PP_SNAP) || (pp & PP_RAW_SNAP))
//				goto sync_done;
			CDBG("SNAPSHOT pp = %d\n", pp);
			if (!pp) {
//FIH_ADQ,JOE HSU,Update patch
				qcmd_frame =
					kmalloc(sizeof(struct msm_queue_cmd_t),
						GFP_ATOMIC);
				if (!qcmd_frame)
					goto mem_fail2;

				qcmd_frame->type    = MSM_CAM_Q_VFE_MSG;
				qcmd_frame->command = NULL;

			spin_lock_irqsave(&(msm_sync.pict_frame_q_lock),
					flags);

				list_add_tail(&qcmd_frame->list,
				&(msm_sync.pict_frame_q));

				spin_unlock_irqrestore(
				&(msm_sync.pict_frame_q_lock), flags);
			wake_up(&(msm_sync.pict_frame_wait));
			}
		}
//FIH_ADQ ,JOE HSU
		qcmd->type = MSM_CAM_Q_VFE_MSG;
	}

sync_done:
	qcmd->command = (void *)vdata;
	CDBG("vdata->type = %d\n", vdata->type);

	spin_lock_irqsave(&(msm_sync.msg_event_queue_lock),
		flags);
	list_add_tail(&qcmd->list, &(msm_sync.msg_event_queue));
	spin_unlock_irqrestore(&((msm_sync.msg_event_queue_lock)),
		flags);
	wake_up(&(msm_sync.msg_event_wait));
	CDBG("waked up config thread\n");

	return;

mem_fail3:
	kfree(qcmd_frame);

mem_fail2:
	kfree(qcmd);

mem_fail1:
	if (qtype == MSM_CAMERA_MSG &&
			vdata->evt_msg.len > 0)
		kfree(vdata->evt_msg.data);

	kfree(vdata);
	return;
}

static struct msm_vfe_resp msm_vfe_s = {
	.vfe_resp = msm_vfe_sync,
};

static long msm_open_proc(struct msm_device_t *msm)
{
	long rc = 0;
//FIH_ADQ,JOE HSU
	struct msm_camera_device_platform_data *pdata =
		msm->pdev->dev.platform_data;

	rc = msm_camvfe_check(msm);
	if (rc < 0)
		goto msm_open_proc_done;

	if (!pdata) {
		rc = -ENODEV;
		goto msm_open_proc_done;
	}

	mutex_lock(&msm_lock);//FIH_ADQ,JOE HSU,Mutex warning
	if (msm_camera->opencnt > 5) {
		mutex_unlock(&msm_lock);//FIH_ADQ,JOE HSU,Mutex warning
		return -EFAULT;
	}
	msm_camera->opencnt += 1;
	mutex_unlock(&msm_lock);//FIH_ADQ,JOE HSU,Mutex warning

	if (msm_camera->opencnt == 1) {

		if (msm_camera->vfefn.vfe_init) {

			rc = msm_camera->vfefn.vfe_init(&msm_vfe_s,
				msm_camera->pdev);
			if (rc < 0) {
				CDBG("vfe_init failed at %ld\n", rc);
				msm_camera->opencnt -= 1;
				goto msm_open_proc_done;
			}
		} else {
			rc = -ENODEV;
			msm_camera->opencnt -= 1;
			goto msm_open_proc_done;
		}
	mutex_lock(&msm_sem);
		if (rc >= 0) {
			INIT_HLIST_HEAD(&(msm_sync.frame.pmem_regions));
			INIT_HLIST_HEAD(&(msm_sync.stats.pmem_regions));
			//mutex_init(&msm_sync.frame.pmem_mutex);
			//mutex_init(&msm_sync.stats.pmem_mutex);
		}
	mutex_unlock(&msm_sem);		
	} else if (msm_camera->opencnt > 1)
		rc = 0;

	if (rc < 0)
		goto msm_open_proc_fail;
	else
		goto msm_open_proc_done;

msm_open_proc_fail:
msm_open_proc_done:
	return rc;
}

static int msm_open(struct inode *inode, struct file *filep)
{
	struct msm_device_t *camdev;
	int rc = 0;

	rc = nonseekable_open(inode, filep);
	if (rc < 0)
		goto cam_open_fail;

	camdev =
		container_of(inode->i_cdev,
			struct msm_device_t, camera_cdev);

	if (!camdev) {
		rc = -ENODEV;
		goto cam_open_fail;
	}
//FIH_ADQ,JOE HSU
	rc = msm_open_proc(camdev);

	if (rc < 0)
		goto cam_open_done;

	filep->private_data = camdev;

cam_open_fail:
cam_open_done:
	CDBG("msm_open() open: rc = %d\n", rc);
	return rc;
}

static struct file_operations msm_camera_fops = {
	.owner = THIS_MODULE,
	.open = msm_open,
	.unlocked_ioctl = msm_ioctl,
	.release = msm_release,
	.read = msm_read,
	.write = msm_write,
	.poll = msm_poll,
};

static long msm_camera_setup_cdevs(struct platform_device *pdev)
{
	int rc, cnt;
	struct device *class_dev;
	struct msm_camera_device_platform_data *pdata;
	struct msm_camera_sensor_info *sinfo;

	msm_camera = kzalloc(sizeof(struct msm_device_t), GFP_KERNEL);
	if (!msm_camera)
		return -ENOMEM;

	rc = alloc_chrdev_region(&msm_devno, 0, 1, DEV_NAME);
	if (rc < 0)
		goto setup_failure_return;

	msm_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(msm_class))
		goto setup_unregister;

	class_dev = device_create(msm_class,
		NULL, msm_devno, NULL, DEV_NAME);
	if (IS_ERR(class_dev))
		goto setup_destroy_class;

	cdev_init(&msm_camera->camera_cdev, &msm_camera_fops);

	/* local data initialization */
	msm_camera->opencnt = 0;
	msm_camera->camera_cdev.owner = THIS_MODULE;

	msm_camvfe_fn_init(&(msm_camera->vfefn));//FIH_ADQ,JOE HSU
	msm_camera->camsync = &msm_sync;

	spin_lock_init(&(msm_sync.msg_event_queue_lock));
	INIT_LIST_HEAD(&(msm_sync.msg_event_queue));
	init_waitqueue_head(&(msm_sync.msg_event_wait));

	spin_lock_init(&(msm_sync.prev_frame_q_lock));
	INIT_LIST_HEAD(&(msm_sync.prev_frame_q));
	init_waitqueue_head(&(msm_sync.prev_frame_wait));

	spin_lock_init(&(msm_sync.pict_frame_q_lock));
	INIT_LIST_HEAD(&(msm_sync.pict_frame_q));
	init_waitqueue_head(&(msm_sync.pict_frame_wait));

	spin_lock_init(&(msm_sync.ctrl_status_lock));
	INIT_LIST_HEAD(&(msm_sync.ctrl_status_queue));
	init_waitqueue_head(&(msm_sync.ctrl_status_wait));

//FIH_ADQ,JOE HSU,Update patch
	spin_lock_init(&msm_sync.af_status_lock);
	msm_sync.af_flag = 0;
	init_waitqueue_head(&msm_sync.af_status_wait);
	
//	mutex_init(&msm_lock);
//	mutex_init(&pict_pp_lock);
//	mutex_init(&msm_sem);
	
	rc = cdev_add(&msm_camera->camera_cdev, msm_devno, 1);
	if (rc)
		goto setup_cleanup_all;

	
	if (pdev->dev.platform_data) {
		msm_camera->pdev = pdev;

		pdata =
			pdev->dev.platform_data;

		sinfo = pdata->sinfo;
		
                pdata->camera_gpio_on();//FIH_ADQ,JOE HSU
                
		for (cnt = 0;
			cnt < pdata->snum;
			cnt++) {
//FIH_ADQ,JOE HSU				
/*
			if (!strcmp(sinfo->sensor_name, "mt9d112"))
				rc = mt9d112_init(sinfo);

			if (!strcmp(sinfo->sensor_name, "mt9p012"))
				rc = mt9p012_init(sinfo);
*/
			if (!strcmp(sinfo->sensor_name, "mt9t013"))
				rc = mt9t013_init(sinfo);

			sinfo++;
		}
	} else
		rc = -ENODEV;

	if (rc < 0)
		goto setup_cleanup_all;

	pdata->camera_gpio_off();//FIH_ADQ,JOE HSU

	if (cnt > 0) {
		msm_camvfe_init();
	}
	
	CDBG("msm_camera setup finishes!\n");
	return 0;

setup_cleanup_all:
	cdev_del(&msm_camera->camera_cdev);
	device_destroy(msm_class, msm_devno);
setup_destroy_class:
	class_destroy(msm_class);
setup_unregister:
	unregister_chrdev_region(msm_devno, 1);
setup_failure_return:
	kfree(msm_camera);
	return rc;
}

static int __init msm_camera_probe(struct platform_device *pdev)
{
	return msm_camera_setup_cdevs(pdev);
}

static long msm_camera_del_cdevs(struct platform_device *pdev)
{
	struct msm_camera_device_platform_data *pdata;
	struct msm_camera_sensor_info *sinfo;
	int cnt;

	pdata =
		pdev->dev.platform_data;

	sinfo = pdata->sinfo;

	for (cnt = 0;
		cnt < pdata->snum;
		cnt++) {
////FIH_ADQ,JOE HSU			
/*
		if (!strcmp(sinfo->sensor_name, "mt9d112"))
			mt9d112_exit();

		if (!strcmp(sinfo->sensor_name, "mt9p012"))
			mt9p012_exit();
*/
		if (!strcmp(sinfo->sensor_name, "mt9t013"))
			mt9t013_exit();

		sinfo++;
	}

	cdev_del(&msm_camera->camera_cdev);
	device_destroy(msm_class, msm_devno);
	class_destroy(msm_class);
	unregister_chrdev_region(msm_devno, 1);
	kfree(msm_camera);

	return 0;
}

static int msm_camera_remove(struct platform_device *pdev)
{
	return msm_camera_del_cdevs(pdev);
}

static struct platform_driver msm_camera_driver = {
	.probe = msm_camera_probe,
	.remove	 = msm_camera_remove,
	.driver = {
		.name = "msm_camera",
		.owner = THIS_MODULE,
	},
};

static int __init msm_camera_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

static void __exit msm_camera_exit(void)
{
	platform_driver_unregister(&msm_camera_driver);
}

module_init(msm_camera_init);
module_exit(msm_camera_exit);

static long msm_control_proc(struct msm_ctrl_cmd_t *ctrlcmd)
{
	unsigned long flags;
	int timeout;
	long rc = 0;

	struct msm_queue_cmd_t *qcmd = NULL;
	struct msm_queue_cmd_t *rcmd = NULL;

	/* wake up config thread, 4 is for V4L2 application */
	qcmd = kmalloc(sizeof(struct msm_queue_cmd_t), GFP_ATOMIC);
	if (!qcmd) {
		CDBG("msm_control: cannot allocate buffer\n");
		rc = -ENOMEM;
		goto end;
	}

	spin_lock_irqsave(&(msm_sync.msg_event_queue_lock), flags);
	qcmd->type = MSM_CAM_Q_V4L2_REQ;
	qcmd->command = ctrlcmd;
	list_add_tail(&qcmd->list, &(msm_sync.msg_event_queue));
	wake_up(&(msm_sync.msg_event_wait));
	spin_unlock_irqrestore(&(msm_sync.msg_event_queue_lock), flags);

	/* wait for config status */
	timeout = ctrlcmd->timeout_ms;
	CDBG("msm_control, timeout = %d\n", timeout);
	if (timeout > 0) {
		rc =
			wait_event_timeout(
				msm_sync.ctrl_status_wait,
				msm_ctrl_stats_pending(),
				msecs_to_jiffies(timeout));

		CDBG("msm_control: rc = %ld\n", rc);

		if (rc == 0) {
			CDBG("msm_control: timed out\n");
			rc = -ETIMEDOUT;
			goto fail;
		}
	} else
		rc = wait_event_interruptible(
			msm_sync.ctrl_status_wait,
			msm_ctrl_stats_pending());

	if (rc < 0) {
		//return -ERESTARTSYS;
		rc = 0 ; //FIH_ADQ,JOE HSU,Fix restart system ,workaround.
		return -ETIMEDOUT;
		goto fail;
	}

	/* control command status is ready */
	spin_lock_irqsave(&(msm_sync.ctrl_status_lock), flags);
	if (!list_empty(&(msm_sync.ctrl_status_queue))) {
		rcmd = list_first_entry(
			&(msm_sync.ctrl_status_queue),
			struct msm_queue_cmd_t,
			list);

		if (!rcmd) {
			spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock),
				flags);
			rc = -EAGAIN;
			goto end;
		}

		list_del(&(rcmd->list));
	}
	spin_unlock_irqrestore(&(msm_sync.ctrl_status_lock), flags);

	memcpy(ctrlcmd->value,
		((struct msm_ctrl_cmd_t *)(rcmd->command))->value,
		((struct msm_ctrl_cmd_t *)(rcmd->command))->length);

	if (((struct msm_ctrl_cmd_t *)(rcmd->command))->length > 0)
		kfree(((struct msm_ctrl_cmd_t *)
					 (rcmd->command))->value);
	goto end;

fail:
	kfree(qcmd);
end:
	kfree(rcmd);
	CDBG("msm_control_proc: end rc = %ld\n", rc);
	return rc;
}

//FIH_ADQ ,JOE HSU
unsigned int msm_apps_poll(struct file *filep,
	struct poll_table_struct *pll_table)
{
printk(KERN_INFO "msm_camera msm_apps_poll...\n");
	poll_wait(filep, &(msm_sync.prev_frame_wait), pll_table);

	if (msm_frame_pending())
		/* frame ready */
		return POLLIN | POLLRDNORM;

	return 0;
}


long msm_register(struct msm_driver *drv, const char *id)
{
	long rc = -EFAULT;

	mutex_lock(&msm_sem);
	if (msm_camera->apps_id == NULL) {
		msm_camera->apps_id = id;

		drv->init = msm_open_proc;
		drv->ctrl = msm_control_proc;
		drv->reg_pmem = msm_register_pmem_proc;
		drv->get_frame = msm_get_frame_proc;
		drv->put_frame = msm_put_frame_buf_proc;
		drv->get_pict  = msm_get_pict_proc;
		drv->drv_poll  = msm_apps_poll; //msm_poll;//FIH_ADQ ,JOE HSU
		rc = 0;

	} else
		rc = -EFAULT;

	mutex_unlock(&msm_sem);
	return rc;
}

long msm_unregister(const char *id)
{
	long rc = -EFAULT;

	mutex_lock(&msm_sem);
	if (!strcmp(msm_camera->apps_id, id)) {
		msm_camera->apps_id = NULL;
		rc = 0;
	}
	mutex_unlock(&msm_sem);

	if (!rc)
	  msm_camera = NULL;


	return rc;
}
