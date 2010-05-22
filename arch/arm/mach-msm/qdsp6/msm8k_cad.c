/*
 *
 * Copyright (c) 2009 QUALCOMM USA, INC.
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
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <mach/qdsp6/msm8k_cad_module.h>
#include <mach/qdsp6/msm8k_cad_q6dec_drv.h>
#include <mach/qdsp6/msm8k_cad_itypes.h>
#include <mach/qdsp6/msm8k_cad_volume.h>

#define MODULE_NAME "CAD"

#if 0
#define D(fmt, args...) printk(KERN_INFO "msm8k_cad: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

#define CAD_SESSION_INUSE 1
#define CAD_SESSION_FREE  0


struct cad_session_info_struct_type {
	s8				status;
	struct mutex			sync;
	struct cad_func_tbl_type	*hw_accel;
};

struct cad_aux_pcm_gpios {
	int	dout;
	int	din;
	int	syncout;
	int	clkin_a;
};

struct cad_state_struct_type {
	struct mutex			sync;
	struct cad_func_tbl_type	*resource_alloc;
	struct cad_func_tbl_type	*volume;
	struct cad_func_tbl_type	*dtmf;
	struct cad_func_tbl_type	*audiodec;
	struct cad_func_tbl_type	*audioenc;
	struct cad_func_tbl_type	*voicedec;
	struct cad_func_tbl_type	*voiceenc;
	struct cad_func_tbl_type	*device_filter;
	struct cad_func_tbl_type	*ard;

	struct cad_session_info_struct_type session_info[CAD_MAX_SESSION];
	struct cad_aux_pcm_gpios	aux_pcm;
};

struct cad_singleton_info_struct_type {
	u8	cad_ref_ct;
};


static DEFINE_SPINLOCK(slock);

static struct cad_state_struct_type cad;
static struct cad_singleton_info_struct_type cad_singleton;

u8 *g_audio_mem;
u32 g_audio_base;
u32 g_audio_size;

static s32 get_gpios(struct platform_device *pdev);
static u8 add_ref_count(void);
static u8 release_ref_count(void);

static int __init cad_probe(struct platform_device *pdev)
{
	u8			ref_count;
	s32			rc;

	rc = CAD_RES_SUCCESS;

	ref_count = add_ref_count();

	if (ref_count != 1) {
		pr_err("CAD already Initialized %d\n",
			cad_singleton.cad_ref_ct);
		rc = CAD_RES_FAILURE;
		goto done;
	}

	D("%s: %s called\n", MODULE_NAME, __func__);

	mutex_init(&cad.sync);

	g_audio_base = pdev->resource[0].start;
	g_audio_size = pdev->resource[0].end - pdev->resource[0].start + 1;

	g_audio_mem = ioremap(g_audio_base, g_audio_size);
	if (g_audio_mem == NULL)
		return -ENOMEM;

	cad.ard			= NULL;
	cad.audiodec		= NULL;
	cad.audioenc		= NULL;
	cad.device_filter	= NULL;
	cad.voicedec		= NULL;
	cad.voiceenc		= NULL;
	cad.resource_alloc	= NULL;

	rc = get_gpios(pdev);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("GPIO configuration failed\n");
		return rc;
	}

	rc = cad_audio_dec_init(&cad.audiodec);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_audio_dec_init failed\n");
		return rc;
	}

	rc = cad_audio_enc_init(&cad.audioenc);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_audio_enc_init failed\n");
		return rc;
	}

	rc = cad_ard_init(&cad.ard);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_ard_init failed\n");
		return rc;
	}

	rc = cad_volume_init(&cad.volume);
	if (rc != CAD_RES_SUCCESS) {
		pr_err("cad_volume_init failed\n");
		return rc;
	}

done:
	return rc;
}

s32 cad_switch_bt_sco(int on)
{
	s32 rc;
	int val;
	rc = CAD_RES_SUCCESS;

	D("%s: %s called\n", MODULE_NAME, __func__);

	if (on)
		val = 1;
	else
		val = 0;

	gpio_set_value(cad.aux_pcm.dout, val);
	gpio_set_value(cad.aux_pcm.din, val);
	gpio_set_value(cad.aux_pcm.syncout, val);
	gpio_set_value(cad.aux_pcm.clkin_a, val);

	return rc;
}
EXPORT_SYMBOL(cad_switch_bt_sco);



s32 cad_open(struct cad_open_struct_type *open_param)
{
	s32 handle;
	s32 handle_temp;
	s8 resource_alloc, ard, hw_accel;
	s32 rc;

	D("%s: %s called\n", MODULE_NAME, __func__);

	rc = CAD_RES_FAILURE;
	handle = handle_temp = resource_alloc = ard = hw_accel = 0;

	mutex_lock(&cad.sync);

	if (open_param != NULL) {
		for (handle_temp = 1; handle_temp < CAD_MAX_SESSION;
				handle_temp++) {
			if (CAD_SESSION_FREE ==
					cad.session_info[handle_temp].status) {
				if (CAD_OPEN_OP_READ == open_param->op_code)
					cad.session_info[handle_temp].hw_accel
							= cad.audioenc;
				else if (CAD_OPEN_OP_WRITE ==
							open_param->op_code)
					cad.session_info[handle_temp].hw_accel
							= cad.audiodec;
				rc = CAD_RES_SUCCESS;
				break;
			}
		}
	} else {
		rc = CAD_RES_FAILURE;
		goto done;
	}

	if (rc == CAD_RES_FAILURE)
		goto done;

	if (cad.resource_alloc != NULL) {
		if (cad.resource_alloc->open != NULL) {

			rc = cad.resource_alloc->open(handle_temp,
							open_param);

			if (rc == CAD_RES_FAILURE)
				goto done;
			resource_alloc = 1;
		}

	}

	if ((cad.ard != NULL) && (cad.ard->open != NULL)) {

		rc = cad.ard->open(handle_temp, open_param);

		if (rc == CAD_RES_FAILURE)
			goto done;

		ard = 1;
	}

	if ((cad.session_info[handle_temp].hw_accel != NULL) &&
		(cad.session_info[handle_temp].hw_accel->open != NULL)) {

		rc = cad.session_info[handle_temp].hw_accel->open(
						handle_temp, open_param);

		if (rc == CAD_RES_FAILURE)
			goto done;

		hw_accel = 1;
	}

	mutex_init(&cad.session_info[handle_temp].sync);

	if (rc == CAD_RES_SUCCESS) {
		handle = handle_temp;
		cad.session_info[handle].status = CAD_SESSION_INUSE;
	}

done:
	/* This could be implemented with multiple goto labels, but that would
	   be ugly.
	*/
	if (rc == CAD_RES_FAILURE) {

		if (ard && cad.ard->close)
			cad.ard->close(handle_temp);

		if (hw_accel &&
			cad.session_info[handle_temp].hw_accel->close) {

			cad.session_info[handle_temp].hw_accel->close(
								handle_temp);
		}

		if (resource_alloc && cad.resource_alloc->close)
			cad.resource_alloc->close(handle_temp);
	}
	mutex_unlock(&cad.sync);

	return handle;
}
EXPORT_SYMBOL(cad_open);




s32 cad_close(s32 driver_handle)
{
	s32 rc;

	msleep(2000);

	D("%s: %s called\n", MODULE_NAME, __func__);

	rc = CAD_RES_SUCCESS;

	if (driver_handle > 0) {

		mutex_lock(&cad.session_info[driver_handle].sync);

		if (cad.session_info[driver_handle].hw_accel &&
			cad.session_info[driver_handle].hw_accel->close)
			cad.session_info[driver_handle].hw_accel->
				close(driver_handle);

		if (cad.ard && cad.ard->close)
			(void) cad.ard->close(driver_handle);

		if (cad.resource_alloc && cad.resource_alloc->close)
			(void) cad.resource_alloc->close(driver_handle);

		mutex_lock(&cad.sync);
		mutex_unlock(&cad.session_info[driver_handle].sync);

		cad.session_info[driver_handle].status = CAD_SESSION_FREE;
		cad.session_info[driver_handle].hw_accel = NULL;

		mutex_unlock(&cad.sync);
	}

	return rc;
}
EXPORT_SYMBOL(cad_close);



s32 cad_read(s32 driver_handle, struct cad_buf_struct_type *buf)
{
	s32 data_read;

	D("%s: %s called\n", MODULE_NAME, __func__);

	data_read = CAD_RES_FAILURE;

	if (cad.session_info[driver_handle].hw_accel &&
		cad.session_info[driver_handle].hw_accel->read)
		data_read = cad.session_info[driver_handle].hw_accel->
					read(driver_handle, buf);

	return data_read;
}
EXPORT_SYMBOL(cad_read);


s32 cad_write(s32 driver_handle, struct cad_buf_struct_type *buf)
{
	s32 data_written;

	D("%s: %s called\n", MODULE_NAME, __func__);

	data_written = CAD_RES_FAILURE;

	if (cad.session_info[driver_handle].hw_accel &&
		cad.session_info[driver_handle].hw_accel->write)
		data_written = cad.session_info[driver_handle].hw_accel->
			write(driver_handle, buf);

	else if (cad.ard && cad.ard->write)
		data_written = cad.ard->write(driver_handle, buf);

	return data_written;
}
EXPORT_SYMBOL(cad_write);


s32 cad_ioctl(s32 driver_handle, u32 cmd_code, void *cmd_buf, u32 cmd_buf_len)
{
	s32 ret_val;

	D("%s: %s called\n", MODULE_NAME, __func__);

	ret_val = CAD_RES_SUCCESS;

	mutex_lock(&cad.session_info[driver_handle].sync);

	if (cad.ard && cad.ard->ioctl)
		ret_val = cad.ard->ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	if (ret_val == CAD_RES_SUCCESS && cad.volume && cad.volume->ioctl)
		ret_val = cad.volume->ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	if ((ret_val != CAD_RES_FAILURE)
		&& cad.session_info[driver_handle].hw_accel
		&& cad.session_info[driver_handle].hw_accel->ioctl)
		ret_val = cad.session_info[driver_handle].hw_accel->
				ioctl(driver_handle, cmd_code, cmd_buf,
								cmd_buf_len);

	mutex_unlock(&cad.session_info[driver_handle].sync);

	return ret_val;
}
EXPORT_SYMBOL(cad_ioctl);

static struct platform_driver cad_driver = {
	.probe = cad_probe,
	.driver = {
		.name = "msm_audio",
		.owner = THIS_MODULE,
	},
};

static int __init cad_init(void)
{
	s32 rc;

	rc = platform_driver_register(&cad_driver);

	return rc;
}

static void __exit cad_exit(void)
{
	u8	ref_count;
	s32	i = 0;

	ref_count = release_ref_count();

	if (ref_count == 0) {
		D("%s: %s called\n", MODULE_NAME, __func__);

		mutex_unlock(&cad.sync);

		while (i < CAD_MAX_SESSION)
			mutex_unlock(&cad.session_info[i++].sync);

		(void)cad_audio_dec_dinit();
		(void)cad_audio_enc_dinit();
		(void)cad_ard_dinit();
		(void)cad_volume_dinit();

		iounmap(g_audio_mem);

		gpio_free(cad.aux_pcm.dout);
		gpio_free(cad.aux_pcm.din);
		gpio_free(cad.aux_pcm.syncout);
		gpio_free(cad.aux_pcm.clkin_a);
	} else {
		pr_err("CAD not De-Initialized as cad_ref_ct = %d\n",
			cad_singleton.cad_ref_ct);
	}
}


static s32 get_gpios(struct platform_device *pdev)
{
	s32			rc;
	struct resource		*res;

	rc = CAD_RES_SUCCESS;


	/* Claim all of the GPIOs. */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_dout");
	if  (!res) {
		pr_err("%s: failed to get gpio AUX PCM DOUT\n", __func__);
		return -ENODEV;
	}

	cad.aux_pcm.dout = res->start;
	rc = gpio_request(res->start, "AUX PCM DOUT");
	if (rc) {
		pr_err("GPIO request for AUX PCM DOUT failed\n");
		return rc;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_din");
	if  (!res) {
		pr_err("%s: failed to get gpio AUX PCM DIN\n", __func__);
		return -ENODEV;
	}

	cad.aux_pcm.din = res->start;
	rc = gpio_request(res->start, "AUX PCM DIN");
	if (rc) {
		pr_err("GPIO request for AUX PCM DIN failed\n");
		return rc;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_syncout");
	if  (!res) {
		pr_err("%s: failed to get gpio AUX PCM SYNC OUT\n", __func__);
		return -ENODEV;
	}

	cad.aux_pcm.syncout = res->start;

	rc = gpio_request(res->start, "AUX PCM SYNC OUT");
	if (rc)	{
		pr_err("GPIO request for AUX PCM SYNC OUT failed\n");
		return rc;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					"aux_pcm_clkin_a");
	if  (!res) {
		pr_err("%s: failed to get gpio AUX PCM CLKIN A\n", __func__);
		return -ENODEV;
	}

	cad.aux_pcm.clkin_a = res->start;
	rc = gpio_request(res->start, "AUX PCM CLKIN A");
	if (rc) {
		pr_err("GPIO request for AUX PCM CLKIN A failed\n");
		return rc;
	}

	return rc;
}



u8 add_ref_count(void)
{
	unsigned long	flags;

	spin_lock_irqsave(&slock, flags);
	cad_singleton.cad_ref_ct++;
	spin_unlock_irqrestore(&slock, flags);

	D("add_ref_count(cad_ref_ct = %d)\n", cad_singleton.cad_ref_ct);

	return cad_singleton.cad_ref_ct;
}

u8 release_ref_count(void)
{
	unsigned long	flags;

	spin_lock_irqsave(&slock, flags);
	cad_singleton.cad_ref_ct--;
	spin_unlock_irqrestore(&slock, flags);

	D("release_ref_count(cad_ref_ct = %d)\n", cad_singleton.cad_ref_ct);

	return cad_singleton.cad_ref_ct;
}

module_init(cad_init);
module_exit(cad_exit);

MODULE_DESCRIPTION("CAD driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");

