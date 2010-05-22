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

#ifndef __ASM__ARCH_CAMERA_H
#define __ASM__ARCH_CAMERA_H

#include <linux/list.h>
#include <linux/poll.h>
#include <linux/platform_device.h>

#include <media/msm_camera.h>

#define MSM_CAMERA_MSG 0
#define MSM_CAMERA_EVT 1
#define NUM_WB_EXP_NEUTRAL_REGION_LINES 4
#define NUM_WB_EXP_STAT_OUTPUT_BUFFERS  3
#define NUM_AUTOFOCUS_MULTI_WINDOW_GRIDS 16
#define NUM_AF_STAT_OUTPUT_BUFFERS      3

enum msm_queut_t {
	MSM_CAM_Q_IVALID,
	MSM_CAM_Q_CTRL,
	MSM_CAM_Q_VFE_EVT,
	MSM_CAM_Q_VFE_MSG,
	MSM_CAM_Q_V4L2_REQ,

	MSM_CAM_Q_MAX
};

/* this structure is used in kernel */
struct msm_queue_cmd_t {
	struct list_head list;

	/* 1 - control command or control command status;
	 * 2 - adsp event;
	 * 3 - adsp message;
	 * 4 - v4l2 request;
	 */
	enum msm_queut_t type;
	void             *command;
};

struct register_address_value_pair_t {
  uint16_t register_address;
  uint16_t register_value;
};

struct msm_pmem_region {
	struct hlist_node list;
	enum msm_pmem_t type;
	void *vaddr;
	unsigned long paddr;
	unsigned long len;
	struct file *file;
	uint32_t y_off;
	uint32_t cbcr_off;
	int fd;
	uint8_t  active;
};

struct axidata_t {
	uint32_t bufnum1;
	uint32_t bufnum2;
	struct msm_pmem_region *region;
};

enum vfe_resp_msg_t {
	VFE_EVENT,
	VFE_MSG_GENERAL,
	VFE_MSG_SNAPSHOT,
	VFE_MSG_OUTPUT1,
	VFE_MSG_OUTPUT2,
	VFE_MSG_STATS_AF,
	VFE_MSG_STATS_WE,

	VFE_MSG_INVALID
};

struct msm_vfe_phy_info {
	uint32_t sbuf_phy;
	uint32_t y_phy;
	uint32_t cbcr_phy;
};

/* this is for 7k */
struct msm_vfe_resp_t {
	enum vfe_resp_msg_t type;
	struct msm_vfe_evt_msg_t evt_msg;
	struct msm_vfe_phy_info  phy;
	void    *extdata;
	int32_t extlen;
};

int32_t mt9d112_init(void *pdata);
void mt9d112_exit(void);
int32_t mt9t013_init(void *pdata);
void mt9t013_exit(void);
int32_t mt9p012_init(void *pdata);
void mt9p012_exit(void);

/* Below functions are added for V4L2 kernel APIs */
//FIH_ADQ,JOE HSU ,update to 6370
struct msm_driver {
	struct msm_device_t *vmsm;
	long (*init)(struct msm_device_t *);
	long (*ctrl)(struct msm_ctrl_cmd_t *,
		struct msm_device_t *);

	long (*reg_pmem)(struct msm_pmem_info_t *,
		struct msm_device_t *);

	long (*get_frame) (struct msm_frame_t *,
		struct msm_device_t *);

	long (*put_frame) (struct msm_frame_t *,
		struct msm_device_t *msm);

	long (*get_pict) (struct msm_ctrl_cmd_t *,
		struct msm_device_t *msm);

	unsigned int (*drv_poll) (struct file *, struct poll_table_struct *,
		struct msm_device_t *msm);
};

unsigned int msm_poll(struct file *, struct poll_table_struct *);

long msm_register(struct msm_driver *,
	const char *);
long msm_unregister(const char *);


struct msm_vfe_resp {
	void (*vfe_resp)(struct msm_vfe_resp_t *,
		enum msm_queut_t, void *syncdata); //FIH_ADQ,JOE HSU
};

struct msm_camvfe_fn_t {
	int (*vfe_init)      (struct msm_vfe_resp *, struct platform_device *);
	int (*vfe_enable)    (struct camera_enable_cmd_t *);
	int (*vfe_config)    (struct msm_vfe_cfg_cmd_t *, void *);
	int (*vfe_disable)   (struct camera_enable_cmd_t *,
		struct platform_device *dev);
	void (*vfe_release)  (struct platform_device *);
};

//FIH_ADQ,JOE HSU
void msm_camvfe_init(void);
int msm_camvfe_check(void *);
void msm_camvfe_fn_init(struct msm_camvfe_fn_t *);
#endif
