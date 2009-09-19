/*
 * V4L2 Driver for SuperH Mobile CEU interface
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on V4L2 Driver for PXA camera host - "pxa_camera.c",
 *
 * Copyright (C) 2006, Sascha Hauer, Pengutronix
 * Copyright (C) 2008, Guennadi Liakhovetski <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/soc_camera.h>
#include <media/sh_mobile_ceu.h>
#include <media/videobuf-dma-contig.h>

/* register offsets for sh7722 / sh7723 */

#define CAPSR  0x00 /* Capture start register */
#define CAPCR  0x04 /* Capture control register */
#define CAMCR  0x08 /* Capture interface control register */
#define CMCYR  0x0c /* Capture interface cycle  register */
#define CAMOR  0x10 /* Capture interface offset register */
#define CAPWR  0x14 /* Capture interface width register */
#define CAIFR  0x18 /* Capture interface input format register */
#define CSTCR  0x20 /* Camera strobe control register (<= sh7722) */
#define CSECR  0x24 /* Camera strobe emission count register (<= sh7722) */
#define CRCNTR 0x28 /* CEU register control register */
#define CRCMPR 0x2c /* CEU register forcible control register */
#define CFLCR  0x30 /* Capture filter control register */
#define CFSZR  0x34 /* Capture filter size clip register */
#define CDWDR  0x38 /* Capture destination width register */
#define CDAYR  0x3c /* Capture data address Y register */
#define CDACR  0x40 /* Capture data address C register */
#define CDBYR  0x44 /* Capture data bottom-field address Y register */
#define CDBCR  0x48 /* Capture data bottom-field address C register */
#define CBDSR  0x4c /* Capture bundle destination size register */
#define CFWCR  0x5c /* Firewall operation control register */
#define CLFCR  0x60 /* Capture low-pass filter control register */
#define CDOCR  0x64 /* Capture data output control register */
#define CDDCR  0x68 /* Capture data complexity level register */
#define CDDAR  0x6c /* Capture data complexity level address register */
#define CEIER  0x70 /* Capture event interrupt enable register */
#define CETCR  0x74 /* Capture event flag clear register */
#define CSTSR  0x7c /* Capture status register */
#define CSRTR  0x80 /* Capture software reset register */
#define CDSSR  0x84 /* Capture data size register */
#define CDAYR2 0x90 /* Capture data address Y register 2 */
#define CDACR2 0x94 /* Capture data address C register 2 */
#define CDBYR2 0x98 /* Capture data bottom-field address Y register 2 */
#define CDBCR2 0x9c /* Capture data bottom-field address C register 2 */

/* per video frame buffer */
struct sh_mobile_ceu_buffer {
	struct videobuf_buffer vb; /* v4l buffer must be first */
	const struct soc_camera_data_format *fmt;
};

struct sh_mobile_ceu_dev {
	struct soc_camera_host ici;
	struct soc_camera_device *icd;

	unsigned int irq;
	void __iomem *base;
	unsigned long video_limit;

	/* lock used to protect videobuf */
	spinlock_t lock;
	struct list_head capture;
	struct videobuf_buffer *active;

	struct sh_mobile_ceu_info *pdata;

	u32 cflcr;

	unsigned int is_interlaced:1;
	unsigned int image_mode:1;
	unsigned int is_16bit:1;
};

struct sh_mobile_ceu_cam {
	struct v4l2_rect camera_rect;
	struct v4l2_rect camera_max;
	const struct soc_camera_data_format *extra_fmt;
	const struct soc_camera_data_format *camera_fmt;
};

static unsigned long make_bus_param(struct sh_mobile_ceu_dev *pcdev)
{
	unsigned long flags;

	flags = SOCAM_MASTER |
		SOCAM_PCLK_SAMPLE_RISING |
		SOCAM_HSYNC_ACTIVE_HIGH |
		SOCAM_HSYNC_ACTIVE_LOW |
		SOCAM_VSYNC_ACTIVE_HIGH |
		SOCAM_VSYNC_ACTIVE_LOW |
		SOCAM_DATA_ACTIVE_HIGH;

	if (pcdev->pdata->flags & SH_CEU_FLAG_USE_8BIT_BUS)
		flags |= SOCAM_DATAWIDTH_8;

	if (pcdev->pdata->flags & SH_CEU_FLAG_USE_16BIT_BUS)
		flags |= SOCAM_DATAWIDTH_16;

	if (flags & SOCAM_DATAWIDTH_MASK)
		return flags;

	return 0;
}

static void ceu_write(struct sh_mobile_ceu_dev *priv,
		      unsigned long reg_offs, u32 data)
{
	iowrite32(data, priv->base + reg_offs);
}

static u32 ceu_read(struct sh_mobile_ceu_dev *priv, unsigned long reg_offs)
{
	return ioread32(priv->base + reg_offs);
}

/*
 *  Videobuf operations
 */
static int sh_mobile_ceu_videobuf_setup(struct videobuf_queue *vq,
					unsigned int *count,
					unsigned int *size)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	int bytes_per_pixel = (icd->current_fmt->depth + 7) >> 3;

	*size = PAGE_ALIGN(icd->rect_current.width * icd->rect_current.height *
			   bytes_per_pixel);

	if (0 == *count)
		*count = 2;

	if (pcdev->video_limit) {
		while (*size * *count > pcdev->video_limit)
			(*count)--;
	}

	dev_dbg(&icd->dev, "count=%d, size=%d\n", *count, *size);

	return 0;
}

static void free_buffer(struct videobuf_queue *vq,
			struct sh_mobile_ceu_buffer *buf)
{
	struct soc_camera_device *icd = vq->priv_data;

	dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		&buf->vb, buf->vb.baddr, buf->vb.bsize);

	if (in_interrupt())
		BUG();

	videobuf_waiton(&buf->vb, 0, 0);
	videobuf_dma_contig_free(vq, &buf->vb);
	dev_dbg(&icd->dev, "%s freed\n", __func__);
	buf->vb.state = VIDEOBUF_NEEDS_INIT;
}

#define CEU_CETCR_MAGIC 0x0317f313 /* acknowledge magical interrupt sources */
#define CEU_CETCR_IGRW (1 << 4) /* prohibited register access interrupt bit */
#define CEU_CEIER_CPEIE (1 << 0) /* one-frame capture end interrupt */
#define CEU_CAPCR_CTNCP (1 << 16) /* continuous capture mode (if set) */


static void sh_mobile_ceu_capture(struct sh_mobile_ceu_dev *pcdev)
{
	struct soc_camera_device *icd = pcdev->icd;
	dma_addr_t phys_addr_top, phys_addr_bottom;

	/* The hardware is _very_ picky about this sequence. Especially
	 * the CEU_CETCR_MAGIC value. It seems like we need to acknowledge
	 * several not-so-well documented interrupt sources in CETCR.
	 */
	ceu_write(pcdev, CEIER, ceu_read(pcdev, CEIER) & ~CEU_CEIER_CPEIE);
	ceu_write(pcdev, CETCR, ~ceu_read(pcdev, CETCR) & CEU_CETCR_MAGIC);
	ceu_write(pcdev, CEIER, ceu_read(pcdev, CEIER) | CEU_CEIER_CPEIE);
	ceu_write(pcdev, CAPCR, ceu_read(pcdev, CAPCR) & ~CEU_CAPCR_CTNCP);
	ceu_write(pcdev, CETCR, CEU_CETCR_MAGIC ^ CEU_CETCR_IGRW);

	if (!pcdev->active)
		return;

	phys_addr_top = videobuf_to_dma_contig(pcdev->active);
	ceu_write(pcdev, CDAYR, phys_addr_top);
	if (pcdev->is_interlaced) {
		phys_addr_bottom = phys_addr_top + icd->rect_current.width;
		ceu_write(pcdev, CDBYR, phys_addr_bottom);
	}

	switch (icd->current_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		phys_addr_top += icd->rect_current.width *
			icd->rect_current.height;
		ceu_write(pcdev, CDACR, phys_addr_top);
		if (pcdev->is_interlaced) {
			phys_addr_bottom = phys_addr_top +
				icd->rect_current.width;
			ceu_write(pcdev, CDBCR, phys_addr_bottom);
		}
	}

	pcdev->active->state = VIDEOBUF_ACTIVE;
	ceu_write(pcdev, CAPSR, 0x1); /* start capture */
}

static int sh_mobile_ceu_videobuf_prepare(struct videobuf_queue *vq,
					  struct videobuf_buffer *vb,
					  enum v4l2_field field)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct sh_mobile_ceu_buffer *buf;
	int ret;

	buf = container_of(vb, struct sh_mobile_ceu_buffer, vb);

	dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		vb, vb->baddr, vb->bsize);

	/* Added list head initialization on alloc */
	WARN_ON(!list_empty(&vb->queue));

#ifdef DEBUG
	/* This can be useful if you want to see if we actually fill
	 * the buffer with something */
	memset((void *)vb->baddr, 0xaa, vb->bsize);
#endif

	BUG_ON(NULL == icd->current_fmt);

	if (buf->fmt	!= icd->current_fmt ||
	    vb->width	!= icd->rect_current.width ||
	    vb->height	!= icd->rect_current.height ||
	    vb->field	!= field) {
		buf->fmt	= icd->current_fmt;
		vb->width	= icd->rect_current.width;
		vb->height	= icd->rect_current.height;
		vb->field	= field;
		vb->state	= VIDEOBUF_NEEDS_INIT;
	}

	vb->size = vb->width * vb->height * ((buf->fmt->depth + 7) >> 3);
	if (0 != vb->baddr && vb->bsize < vb->size) {
		ret = -EINVAL;
		goto out;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		ret = videobuf_iolock(vq, vb, NULL);
		if (ret)
			goto fail;
		vb->state = VIDEOBUF_PREPARED;
	}

	return 0;
fail:
	free_buffer(vq, buf);
out:
	return ret;
}

/* Called under spinlock_irqsave(&pcdev->lock, ...) */
static void sh_mobile_ceu_videobuf_queue(struct videobuf_queue *vq,
					 struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;

	dev_dbg(&icd->dev, "%s (vb=0x%p) 0x%08lx %zd\n", __func__,
		vb, vb->baddr, vb->bsize);

	vb->state = VIDEOBUF_QUEUED;
	list_add_tail(&vb->queue, &pcdev->capture);

	if (!pcdev->active) {
		pcdev->active = vb;
		sh_mobile_ceu_capture(pcdev);
	}
}

static void sh_mobile_ceu_videobuf_release(struct videobuf_queue *vq,
					   struct videobuf_buffer *vb)
{
	struct soc_camera_device *icd = vq->priv_data;
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	unsigned long flags;

	spin_lock_irqsave(&pcdev->lock, flags);

	if (pcdev->active == vb) {
		/* disable capture (release DMA buffer), reset */
		ceu_write(pcdev, CAPSR, 1 << 16);
		pcdev->active = NULL;
	}

	if ((vb->state == VIDEOBUF_ACTIVE || vb->state == VIDEOBUF_QUEUED) &&
	    !list_empty(&vb->queue)) {
		vb->state = VIDEOBUF_ERROR;
		list_del_init(&vb->queue);
	}

	spin_unlock_irqrestore(&pcdev->lock, flags);

	free_buffer(vq, container_of(vb, struct sh_mobile_ceu_buffer, vb));
}

static struct videobuf_queue_ops sh_mobile_ceu_videobuf_ops = {
	.buf_setup      = sh_mobile_ceu_videobuf_setup,
	.buf_prepare    = sh_mobile_ceu_videobuf_prepare,
	.buf_queue      = sh_mobile_ceu_videobuf_queue,
	.buf_release    = sh_mobile_ceu_videobuf_release,
};

static irqreturn_t sh_mobile_ceu_irq(int irq, void *data)
{
	struct sh_mobile_ceu_dev *pcdev = data;
	struct videobuf_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&pcdev->lock, flags);

	vb = pcdev->active;
	if (!vb)
		/* Stale interrupt from a released buffer */
		goto out;

	list_del_init(&vb->queue);

	if (!list_empty(&pcdev->capture))
		pcdev->active = list_entry(pcdev->capture.next,
					   struct videobuf_buffer, queue);
	else
		pcdev->active = NULL;

	sh_mobile_ceu_capture(pcdev);

	vb->state = VIDEOBUF_DONE;
	do_gettimeofday(&vb->ts);
	vb->field_count++;
	wake_up(&vb->done);

out:
	spin_unlock_irqrestore(&pcdev->lock, flags);

	return IRQ_HANDLED;
}

/* Called with .video_lock held */
static int sh_mobile_ceu_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;

	if (pcdev->icd)
		return -EBUSY;

	dev_info(&icd->dev,
		 "SuperH Mobile CEU driver attached to camera %d\n",
		 icd->devnum);

	clk_enable(pcdev->clk);

	ceu_write(pcdev, CAPSR, 1 << 16); /* reset */
	while (ceu_read(pcdev, CSTSR) & 1)
		msleep(1);

	pcdev->icd = icd;

	return 0;
}

/* Called with .video_lock held */
static void sh_mobile_ceu_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	unsigned long flags;

	BUG_ON(icd != pcdev->icd);

	/* disable capture, disable interrupts */
	ceu_write(pcdev, CEIER, 0);
	ceu_write(pcdev, CAPSR, 1 << 16); /* reset */

	/* make sure active buffer is canceled */
	spin_lock_irqsave(&pcdev->lock, flags);
	if (pcdev->active) {
		list_del(&pcdev->active->queue);
		pcdev->active->state = VIDEOBUF_ERROR;
		wake_up_all(&pcdev->active->done);
		pcdev->active = NULL;
	}
	spin_unlock_irqrestore(&pcdev->lock, flags);

	clk_disable(pcdev->clk);

	dev_info(&icd->dev,
		 "SuperH Mobile CEU driver detached from camera %d\n",
		 icd->devnum);

	pcdev->icd = NULL;
}

/*
 * See chapter 29.4.12 "Capture Filter Control Register (CFLCR)"
 * in SH7722 Hardware Manual
 */
static unsigned int size_dst(unsigned int src, unsigned int scale)
{
	unsigned int mant_pre = scale >> 12;
	if (!src || !scale)
		return src;
	return ((mant_pre + 2 * (src - 1)) / (2 * mant_pre) - 1) *
		mant_pre * 4096 / scale + 1;
}

static unsigned int size_src(unsigned int dst, unsigned int scale)
{
	unsigned int mant_pre = scale >> 12, tmp;
	if (!dst || !scale)
		return dst;
	for (tmp = ((dst - 1) * scale + 2048 * mant_pre) / 4096 + 1;
	     size_dst(tmp, scale) < dst;
	     tmp++)
		;
	return tmp;
}

static u16 calc_scale(unsigned int src, unsigned int *dst)
{
	u16 scale;

	if (src == *dst)
		return 0;

	scale = (src * 4096 / *dst) & ~7;

	while (scale > 4096 && size_dst(src, scale) < *dst)
		scale -= 8;

	*dst = size_dst(src, scale);

	return scale;
}

/* rect is guaranteed to not exceed the scaled camera rectangle */
static void sh_mobile_ceu_set_rect(struct soc_camera_device *icd,
				   struct v4l2_rect *rect)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_cam *cam = icd->host_priv;
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	int width, height, cfszr_width, cdwdr_width, in_width, in_height;
	unsigned int left_offset, top_offset, left, top;
	unsigned int hscale = pcdev->cflcr & 0xffff;
	unsigned int vscale = (pcdev->cflcr >> 16) & 0xffff;
	u32 camor;

	/* Switch to the camera scale */
	left = size_src(rect->left, hscale);
	top = size_src(rect->top, vscale);

	dev_dbg(&icd->dev, "Left %u * 0x%x = %u, top %u * 0x%x = %u\n",
		rect->left, hscale, left, rect->top, vscale, top);

	if (left > cam->camera_rect.left) {
		left_offset = left - cam->camera_rect.left;
	} else {
		left_offset = 0;
		left = cam->camera_rect.left;
	}

	if (top > cam->camera_rect.top) {
		top_offset = top - cam->camera_rect.top;
	} else {
		top_offset = 0;
		top = cam->camera_rect.top;
	}

	dev_dbg(&icd->dev, "New left %u, top %u, offsets %u:%u\n",
		rect->left, rect->top, left_offset, top_offset);

	if (pcdev->image_mode) {
		width = rect->width;
		in_width = cam->camera_rect.width;
		if (!pcdev->is_16bit) {
			width *= 2;
			in_width *= 2;
			left_offset *= 2;
		}
		cfszr_width = cdwdr_width = rect->width;
	} else {
		unsigned int w_factor = (icd->current_fmt->depth + 7) >> 3;
		if (!pcdev->is_16bit)
			w_factor *= 2;

		width = rect->width * w_factor / 2;
		in_width = cam->camera_rect.width * w_factor / 2;
		left_offset = left_offset * w_factor / 2;

		cfszr_width = pcdev->is_16bit ? width : width / 2;
		cdwdr_width = pcdev->is_16bit ? width * 2 : width;
	}

	height = rect->height;
	in_height = cam->camera_rect.height;
	if (pcdev->is_interlaced) {
		height /= 2;
		in_height /= 2;
		top_offset /= 2;
		cdwdr_width *= 2;
	}

	camor = left_offset | (top_offset << 16);
	ceu_write(pcdev, CAMOR, camor);
	ceu_write(pcdev, CAPWR, (in_height << 16) | in_width);
	ceu_write(pcdev, CFSZR, (height << 16) | cfszr_width);
	ceu_write(pcdev, CDWDR, cdwdr_width);
}

static u32 capture_save_reset(struct sh_mobile_ceu_dev *pcdev)
{
	u32 capsr = ceu_read(pcdev, CAPSR);
	ceu_write(pcdev, CAPSR, 1 << 16); /* reset, stop capture */
	return capsr;
}

static void capture_restore(struct sh_mobile_ceu_dev *pcdev, u32 capsr)
{
	unsigned long timeout = jiffies + 10 * HZ;

	/*
	 * Wait until the end of the current frame. It can take a long time,
	 * but if it has been aborted by a CAPSR reset, it shoule exit sooner.
	 */
	while ((ceu_read(pcdev, CSTSR) & 1) && time_before(jiffies, timeout))
		msleep(1);

	if (time_after(jiffies, timeout)) {
		dev_err(pcdev->ici.v4l2_dev.dev,
			"Timeout waiting for frame end! Interface problem?\n");
		return;
	}

	/* Wait until reset clears, this shall not hang... */
	while (ceu_read(pcdev, CAPSR) & (1 << 16))
		udelay(10);

	/* Anything to restore? */
	if (capsr & ~(1 << 16))
		ceu_write(pcdev, CAPSR, capsr);
}

static int sh_mobile_ceu_set_bus_param(struct soc_camera_device *icd,
				       __u32 pixfmt)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	int ret;
	unsigned long camera_flags, common_flags, value;
	int yuv_lineskip;
	struct sh_mobile_ceu_cam *cam = icd->host_priv;
	u32 capsr = capture_save_reset(pcdev);

	camera_flags = icd->ops->query_bus_param(icd);
	common_flags = soc_camera_bus_param_compatible(camera_flags,
						       make_bus_param(pcdev));
	if (!common_flags)
		return -EINVAL;

	ret = icd->ops->set_bus_param(icd, common_flags);
	if (ret < 0)
		return ret;

	switch (common_flags & SOCAM_DATAWIDTH_MASK) {
	case SOCAM_DATAWIDTH_8:
		pcdev->is_16bit = 0;
		break;
	case SOCAM_DATAWIDTH_16:
		pcdev->is_16bit = 1;
		break;
	default:
		return -EINVAL;
	}

	ceu_write(pcdev, CRCNTR, 0);
	ceu_write(pcdev, CRCMPR, 0);

	value = 0x00000010; /* data fetch by default */
	yuv_lineskip = 0;

	switch (icd->current_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		yuv_lineskip = 1; /* skip for NV12/21, no skip for NV16/61 */
		/* fall-through */
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		switch (cam->camera_fmt->fourcc) {
		case V4L2_PIX_FMT_UYVY:
			value = 0x00000000; /* Cb0, Y0, Cr0, Y1 */
			break;
		case V4L2_PIX_FMT_VYUY:
			value = 0x00000100; /* Cr0, Y0, Cb0, Y1 */
			break;
		case V4L2_PIX_FMT_YUYV:
			value = 0x00000200; /* Y0, Cb0, Y1, Cr0 */
			break;
		case V4L2_PIX_FMT_YVYU:
			value = 0x00000300; /* Y0, Cr0, Y1, Cb0 */
			break;
		default:
			BUG();
		}
	}

	if (icd->current_fmt->fourcc == V4L2_PIX_FMT_NV21 ||
	    icd->current_fmt->fourcc == V4L2_PIX_FMT_NV61)
		value ^= 0x00000100; /* swap U, V to change from NV1x->NVx1 */

	value |= common_flags & SOCAM_VSYNC_ACTIVE_LOW ? 1 << 1 : 0;
	value |= common_flags & SOCAM_HSYNC_ACTIVE_LOW ? 1 << 0 : 0;
	value |= pcdev->is_16bit ? 1 << 12 : 0;
	ceu_write(pcdev, CAMCR, value);

	ceu_write(pcdev, CAPCR, 0x00300000);
	ceu_write(pcdev, CAIFR, pcdev->is_interlaced ? 0x101 : 0);

	mdelay(1);
	sh_mobile_ceu_set_rect(icd, &icd->rect_current);

	ceu_write(pcdev, CFLCR, pcdev->cflcr);

	/* A few words about byte order (observed in Big Endian mode)
	 *
	 * In data fetch mode bytes are received in chunks of 8 bytes.
	 * D0, D1, D2, D3, D4, D5, D6, D7 (D0 received first)
	 *
	 * The data is however by default written to memory in reverse order:
	 * D7, D6, D5, D4, D3, D2, D1, D0 (D7 written to lowest byte)
	 *
	 * The lowest three bits of CDOCR allows us to do swapping,
	 * using 7 we swap the data bytes to match the incoming order:
	 * D0, D1, D2, D3, D4, D5, D6, D7
	 */
	value = 0x00000017;
	if (yuv_lineskip)
		value &= ~0x00000010; /* convert 4:2:2 -> 4:2:0 */

	ceu_write(pcdev, CDOCR, value);
	ceu_write(pcdev, CFWCR, 0); /* keep "datafetch firewall" disabled */

	dev_dbg(&icd->dev, "S_FMT successful for %c%c%c%c %ux%u@%u:%u\n",
		pixfmt & 0xff, (pixfmt >> 8) & 0xff,
		(pixfmt >> 16) & 0xff, (pixfmt >> 24) & 0xff,
		icd->rect_current.width, icd->rect_current.height,
		icd->rect_current.left, icd->rect_current.top);

	capture_restore(pcdev, capsr);

	/* not in bundle mode: skip CBDSR, CDAYR2, CDACR2, CDBYR2, CDBCR2 */
	return 0;
}

static int sh_mobile_ceu_try_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	unsigned long camera_flags, common_flags;

	camera_flags = icd->ops->query_bus_param(icd);
	common_flags = soc_camera_bus_param_compatible(camera_flags,
						       make_bus_param(pcdev));
	if (!common_flags)
		return -EINVAL;

	return 0;
}

static const struct soc_camera_data_format sh_mobile_ceu_formats[] = {
	{
		.name		= "NV12",
		.depth		= 12,
		.fourcc		= V4L2_PIX_FMT_NV12,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	{
		.name		= "NV21",
		.depth		= 12,
		.fourcc		= V4L2_PIX_FMT_NV21,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	{
		.name		= "NV16",
		.depth		= 16,
		.fourcc		= V4L2_PIX_FMT_NV16,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
	{
		.name		= "NV61",
		.depth		= 16,
		.fourcc		= V4L2_PIX_FMT_NV61,
		.colorspace	= V4L2_COLORSPACE_JPEG,
	},
};

static int sh_mobile_ceu_get_formats(struct soc_camera_device *icd, int idx,
				     struct soc_camera_format_xlate *xlate)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	int ret, k, n;
	int formats = 0;
	struct sh_mobile_ceu_cam *cam;

	ret = sh_mobile_ceu_try_bus_param(icd);
	if (ret < 0)
		return 0;

	if (!icd->host_priv) {
		cam = kzalloc(sizeof(*cam), GFP_KERNEL);
		if (!cam)
			return -ENOMEM;

		icd->host_priv = cam;
		cam->camera_max = icd->rect_max;
	} else {
		cam = icd->host_priv;
	}

	/* Beginning of a pass */
	if (!idx)
		cam->extra_fmt = NULL;

	switch (icd->formats[idx].fourcc) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
		if (cam->extra_fmt)
			goto add_single_format;

		/*
		 * Our case is simple so far: for any of the above four camera
		 * formats we add all our four synthesized NV* formats, so,
		 * just marking the device with a single flag suffices. If
		 * the format generation rules are more complex, you would have
		 * to actually hang your already added / counted formats onto
		 * the host_priv pointer and check whether the format you're
		 * going to add now is already there.
		 */
		cam->extra_fmt = (void *)sh_mobile_ceu_formats;

		n = ARRAY_SIZE(sh_mobile_ceu_formats);
		formats += n;
		for (k = 0; xlate && k < n; k++) {
			xlate->host_fmt = &sh_mobile_ceu_formats[k];
			xlate->cam_fmt = icd->formats + idx;
			xlate->buswidth = icd->formats[idx].depth;
			xlate++;
			dev_dbg(ici->v4l2_dev.dev, "Providing format %s using %s\n",
				sh_mobile_ceu_formats[k].name,
				icd->formats[idx].name);
		}
	default:
add_single_format:
		/* Generic pass-through */
		formats++;
		if (xlate) {
			xlate->host_fmt = icd->formats + idx;
			xlate->cam_fmt = icd->formats + idx;
			xlate->buswidth = icd->formats[idx].depth;
			xlate++;
			dev_dbg(ici->v4l2_dev.dev,
				"Providing format %s in pass-through mode\n",
				icd->formats[idx].name);
		}
	}

	return formats;
}

static void sh_mobile_ceu_put_formats(struct soc_camera_device *icd)
{
	kfree(icd->host_priv);
	icd->host_priv = NULL;
}

/* Check if any dimension of r1 is smaller than respective one of r2 */
static bool is_smaller(struct v4l2_rect *r1, struct v4l2_rect *r2)
{
	return r1->width < r2->width || r1->height < r2->height;
}

/* Check if r1 fails to cover r2 */
static bool is_inside(struct v4l2_rect *r1, struct v4l2_rect *r2)
{
	return r1->left > r2->left || r1->top > r2->top ||
		r1->left + r1->width < r2->left + r2->width ||
		r1->top + r1->height < r2->top + r2->height;
}

/*
 * CEU can scale and crop, but we don't want to waste bandwidth and kill the
 * framerate by always requesting the maximum image from the client. For
 * cropping we also have to take care of the current scale. The common for both
 * scaling and cropping approach is:
 * 1. try if the client can produce exactly what requested by the user
 * 2. if (1) failed, try to double the client image until we get one big enough
 * 3. if (2) failed, try to request the maximum image
 */
static int sh_mobile_ceu_set_crop(struct soc_camera_device *icd,
				  struct v4l2_rect *rect)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	struct v4l2_rect cam_rect, target, cam_max;
	struct sh_mobile_ceu_cam *cam = icd->host_priv;
	unsigned int hscale = pcdev->cflcr & 0xffff;
	unsigned int vscale = (pcdev->cflcr >> 16) & 0xffff;
	unsigned short width, height;
	u32 capsr;
	int ret;

	/* Scale back up into client units */
	cam_rect.left	= size_src(rect->left, hscale);
	cam_rect.width	= size_src(rect->width, hscale);
	cam_rect.top	= size_src(rect->top, vscale);
	cam_rect.height	= size_src(rect->height, vscale);

	target = cam_rect;

	capsr = capture_save_reset(pcdev);
	dev_dbg(&icd->dev, "CAPSR 0x%x, CFLCR 0x%x\n", capsr, pcdev->cflcr);

	/* First attempt - see if the client can deliver a perfect result */
	ret = icd->ops->set_crop(icd, &cam_rect);
	if (!ret && !memcmp(&target, &cam_rect, sizeof(target))) {
		dev_dbg(&icd->dev, "Camera S_CROP successful for %ux%u@%u:%u\n",
			cam_rect.width, cam_rect.height,
			cam_rect.left, cam_rect.top);
		goto ceu_set_rect;
	}

	/* Try to fix cropping, that camera hasn't managed to do */
	dev_dbg(&icd->dev, "Fix camera S_CROP %d for %ux%u@%u:%u"
		" to %ux%u@%u:%u\n",
		ret, cam_rect.width, cam_rect.height,
		cam_rect.left, cam_rect.top,
		target.width, target.height, target.left, target.top);

	/*
	 * Popular special case - some cameras can only handle fixed sizes like
	 * QVGA, VGA,... Take care to avoid infinite loop.
	 */
	width = max(cam_rect.width, 1);
	height = max(cam_rect.height, 1);
	cam_max.width = size_src(icd->rect_max.width, hscale);
	cam_max.left = size_src(icd->rect_max.left, hscale);
	cam_max.height = size_src(icd->rect_max.height, vscale);
	cam_max.top = size_src(icd->rect_max.top, vscale);
	while (!ret && (is_smaller(&cam_rect, &target) ||
			is_inside(&cam_rect, &target)) &&
	       cam_max.width >= width && cam_max.height >= height) {

		width *= 2;
		height *= 2;
		cam_rect.width = width;
		cam_rect.height = height;

		/* We do not know what the camera is capable of, play safe */
		if (cam_rect.left > target.left)
			cam_rect.left = cam_max.left;

		if (cam_rect.left + cam_rect.width < target.left + target.width)
			cam_rect.width = target.left + target.width -
				cam_rect.left;

		if (cam_rect.top > target.top)
			cam_rect.top = cam_max.top;

		if (cam_rect.top + cam_rect.height < target.top + target.height)
			cam_rect.height = target.top + target.height -
				cam_rect.top;

		if (cam_rect.width + cam_rect.left >
		    cam_max.width + cam_max.left)
			cam_rect.left = max(cam_max.width + cam_max.left -
					    cam_rect.width, cam_max.left);

		if (cam_rect.height + cam_rect.top >
		    cam_max.height + cam_max.top)
			cam_rect.top = max(cam_max.height + cam_max.top -
					   cam_rect.height, cam_max.top);

		ret = icd->ops->set_crop(icd, &cam_rect);
		dev_dbg(&icd->dev, "Camera S_CROP %d for %ux%u@%u:%u\n",
			ret, cam_rect.width, cam_rect.height,
			cam_rect.left, cam_rect.top);
	}

	/*
	 * If the camera failed to configure cropping, it should not modify the
	 * rectangle
	 */
	if ((ret < 0 && (is_smaller(&icd->rect_current, rect) ||
			 is_inside(&icd->rect_current, rect))) ||
	    is_smaller(&cam_rect, &target) || is_inside(&cam_rect, &target)) {
		/*
		 * The camera failed to configure a suitable cropping,
		 * we cannot use the current rectangle, set to max
		 */
		cam_rect = cam_max;
		ret = icd->ops->set_crop(icd, &cam_rect);
		dev_dbg(&icd->dev, "Camera S_CROP %d for max %ux%u@%u:%u\n",
			ret, cam_rect.width, cam_rect.height,
			cam_rect.left, cam_rect.top);
		if (ret < 0)
			/* All failed, hopefully resume current capture */
			goto resume_capture;

		/* Finally, adjust the target rectangle */
		if (target.width > cam_rect.width)
			target.width = cam_rect.width;
		if (target.height > cam_rect.height)
			target.height = cam_rect.height;
		if (target.left + target.width > cam_rect.left + cam_rect.width)
			target.left = cam_rect.left + cam_rect.width -
				target.width;
		if (target.top + target.height > cam_rect.top + cam_rect.height)
			target.top = cam_rect.top + cam_rect.height -
				target.height;
	}

	/* We now have a rectangle, larger than requested, let's crop */

	/*
	 * We have to preserve camera rectangle between close() / open(),
	 * because soc-camera core calls .set_fmt() on each first open() with
	 * last before last close() _user_ rectangle, which can be different
	 * from camera rectangle.
	 */
	dev_dbg(&icd->dev,
		"SH S_CROP from %ux%u@%u:%u to %ux%u@%u:%u, scale to %ux%u@%u:%u\n",
		cam_rect.width, cam_rect.height, cam_rect.left, cam_rect.top,
		target.width, target.height, target.left, target.top,
		rect->width, rect->height, rect->left, rect->top);

	ret = 0;

ceu_set_rect:
	cam->camera_rect = cam_rect;

	rect->width	= size_dst(target.width, hscale);
	rect->left	= size_dst(target.left, hscale);
	rect->height	= size_dst(target.height, vscale);
	rect->top	= size_dst(target.top, vscale);

	sh_mobile_ceu_set_rect(icd, rect);

resume_capture:
	/* Set CAMOR, CAPWR, CFSZR, take care of CDWDR */
	if (pcdev->active)
		capsr |= 1;
	capture_restore(pcdev, capsr);

	/* Even if only camera cropping succeeded */
	return ret;
}

/* Similar to set_crop multistage iterative algorithm */
static int sh_mobile_ceu_set_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	struct sh_mobile_ceu_cam *cam = icd->host_priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	__u32 pixfmt = pix->pixelformat;
	const struct soc_camera_format_xlate *xlate;
	unsigned int width = pix->width, height = pix->height, tmp_w, tmp_h;
	u16 vscale, hscale;
	int ret, is_interlaced;

	switch (pix->field) {
	case V4L2_FIELD_INTERLACED:
		is_interlaced = 1;
		break;
	case V4L2_FIELD_ANY:
	default:
		pix->field = V4L2_FIELD_NONE;
		/* fall-through */
	case V4L2_FIELD_NONE:
		is_interlaced = 0;
		break;
	}

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(ici->v4l2_dev.dev, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	pix->pixelformat = xlate->cam_fmt->fourcc;
	ret = v4l2_device_call_until_err(&ici->v4l2_dev, (__u32)icd, video, s_fmt, f);
	pix->pixelformat = pixfmt;
	dev_dbg(&icd->dev, "Camera %d fmt %ux%u, requested %ux%u, max %ux%u\n",
		ret, pix->width, pix->height, width, height,
		icd->rect_max.width, icd->rect_max.height);
	if (ret < 0)
		return ret;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		pcdev->image_mode = 1;
		break;
	default:
		pcdev->image_mode = 0;
	}

	if ((abs(width - pix->width) < 4 && abs(height - pix->height) < 4) ||
	    !pcdev->image_mode || is_interlaced) {
		hscale = 0;
		vscale = 0;
		goto out;
	}

	/* Camera set a format, but geometry is not precise, try to improve */
	/*
	 * FIXME: when soc-camera is converted to implement traditional S_FMT
	 * and S_CROP semantics, replace CEU limits with camera maxima
	 */
	tmp_w = pix->width;
	tmp_h = pix->height;
	while ((width > tmp_w || height > tmp_h) &&
	       tmp_w < 2560 && tmp_h < 1920) {
		tmp_w = min(2 * tmp_w, (__u32)2560);
		tmp_h = min(2 * tmp_h, (__u32)1920);
		pix->width = tmp_w;
		pix->height = tmp_h;
		pix->pixelformat = xlate->cam_fmt->fourcc;
		ret = v4l2_device_call_until_err(&ici->v4l2_dev, (__u32)icd,
						 video, s_fmt, f);
		pix->pixelformat = pixfmt;
		dev_dbg(&icd->dev, "Camera scaled to %ux%u\n",
			pix->width, pix->height);
		if (ret < 0) {
			/* This shouldn't happen */
			dev_err(&icd->dev, "Client failed to set format: %d\n",
				ret);
			return ret;
		}
	}

	/* We cannot scale up */
	if (width > pix->width)
		width = pix->width;

	if (height > pix->height)
		height = pix->height;

	/* Let's rock: scale pix->{width x height} down to width x height */
	hscale = calc_scale(pix->width, &width);
	vscale = calc_scale(pix->height, &height);

	dev_dbg(&icd->dev, "W: %u : 0x%x = %u, H: %u : 0x%x = %u\n",
		pix->width, hscale, width, pix->height, vscale, height);

out:
	pcdev->cflcr = hscale | (vscale << 16);

	icd->buswidth = xlate->buswidth;
	icd->current_fmt = xlate->host_fmt;
	cam->camera_fmt = xlate->cam_fmt;
	cam->camera_rect.width = pix->width;
	cam->camera_rect.height = pix->height;

	icd->rect_max.left = size_dst(cam->camera_max.left, hscale);
	icd->rect_max.width = size_dst(cam->camera_max.width, hscale);
	icd->rect_max.top = size_dst(cam->camera_max.top, vscale);
	icd->rect_max.height = size_dst(cam->camera_max.height, vscale);

	icd->rect_current.left = icd->rect_max.left;
	icd->rect_current.top = icd->rect_max.top;

	pcdev->is_interlaced = is_interlaced;

	pix->width = width;
	pix->height = height;

	return 0;
}

static int sh_mobile_ceu_try_fmt(struct soc_camera_device *icd,
				 struct v4l2_format *f)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	__u32 pixfmt = pix->pixelformat;
	int width, height;
	int ret;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_warn(ici->v4l2_dev.dev, "Format %x not found\n", pixfmt);
		return -EINVAL;
	}

	/* FIXME: calculate using depth and bus width */

	v4l_bound_align_image(&pix->width, 2, 2560, 1,
			      &pix->height, 4, 1920, 2, 0);

	width = pix->width;
	height = pix->height;

	pix->bytesperline = pix->width *
		DIV_ROUND_UP(xlate->host_fmt->depth, 8);
	pix->sizeimage = pix->height * pix->bytesperline;

	pix->pixelformat = xlate->cam_fmt->fourcc;

	/* limit to sensor capabilities */
	ret = v4l2_device_call_until_err(&ici->v4l2_dev, (__u32)icd, video,
					 try_fmt, f);
	pix->pixelformat = pixfmt;
	if (ret < 0)
		return ret;

	switch (pixfmt) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		/* FIXME: check against rect_max after converting soc-camera */
		/* We can scale precisely, need a bigger image from camera */
		if (pix->width < width || pix->height < height) {
			int tmp_w = pix->width, tmp_h = pix->height;
			pix->width = 2560;
			pix->height = 1920;
			ret = v4l2_device_call_until_err(&ici->v4l2_dev,
							 (__u32)icd, video,
							 try_fmt, f);
			if (ret < 0) {
				/* Shouldn't actually happen... */
				dev_err(&icd->dev,
					"FIXME: try_fmt() returned %d\n", ret);
				pix->width = tmp_w;
				pix->height = tmp_h;
			}
		}
		if (pix->width > width)
			pix->width = width;
		if (pix->height > height)
			pix->height = height;
	}

	return ret;
}

static int sh_mobile_ceu_reqbufs(struct soc_camera_file *icf,
				 struct v4l2_requestbuffers *p)
{
	int i;

	/* This is for locking debugging only. I removed spinlocks and now I
	 * check whether .prepare is ever called on a linked buffer, or whether
	 * a dma IRQ can occur for an in-work or unlinked buffer. Until now
	 * it hadn't triggered */
	for (i = 0; i < p->count; i++) {
		struct sh_mobile_ceu_buffer *buf;

		buf = container_of(icf->vb_vidq.bufs[i],
				   struct sh_mobile_ceu_buffer, vb);
		INIT_LIST_HEAD(&buf->vb.queue);
	}

	return 0;
}

static unsigned int sh_mobile_ceu_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_file *icf = file->private_data;
	struct sh_mobile_ceu_buffer *buf;

	buf = list_entry(icf->vb_vidq.stream.next,
			 struct sh_mobile_ceu_buffer, vb.stream);

	poll_wait(file, &buf->vb.done, pt);

	if (buf->vb.state == VIDEOBUF_DONE ||
	    buf->vb.state == VIDEOBUF_ERROR)
		return POLLIN|POLLRDNORM;

	return 0;
}

static int sh_mobile_ceu_querycap(struct soc_camera_host *ici,
				  struct v4l2_capability *cap)
{
	strlcpy(cap->card, "SuperH_Mobile_CEU", sizeof(cap->card));
	cap->version = KERNEL_VERSION(0, 0, 5);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	return 0;
}

static void sh_mobile_ceu_init_videobuf(struct videobuf_queue *q,
					struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;

	videobuf_queue_dma_contig_init(q,
				       &sh_mobile_ceu_videobuf_ops,
				       ici->v4l2_dev.dev, &pcdev->lock,
				       V4L2_BUF_TYPE_VIDEO_CAPTURE,
				       pcdev->is_interlaced ?
				       V4L2_FIELD_INTERLACED : V4L2_FIELD_NONE,
				       sizeof(struct sh_mobile_ceu_buffer),
				       icd);
}

static int sh_mobile_ceu_get_ctrl(struct soc_camera_device *icd,
				  struct v4l2_control *ctrl)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;
	u32 val;

	switch (ctrl->id) {
	case V4L2_CID_SHARPNESS:
		val = ceu_read(pcdev, CLFCR);
		ctrl->value = val ^ 1;
		return 0;
	}
	return -ENOIOCTLCMD;
}

static int sh_mobile_ceu_set_ctrl(struct soc_camera_device *icd,
				  struct v4l2_control *ctrl)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->dev.parent);
	struct sh_mobile_ceu_dev *pcdev = ici->priv;

	switch (ctrl->id) {
	case V4L2_CID_SHARPNESS:
		switch (icd->current_fmt->fourcc) {
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
			ceu_write(pcdev, CLFCR, !ctrl->value);
			return 0;
		}
		return -EINVAL;
	}
	return -ENOIOCTLCMD;
}

static const struct v4l2_queryctrl sh_mobile_ceu_controls[] = {
	{
		.id		= V4L2_CID_SHARPNESS,
		.type		= V4L2_CTRL_TYPE_BOOLEAN,
		.name		= "Low-pass filter",
		.minimum	= 0,
		.maximum	= 1,
		.step		= 1,
		.default_value	= 0,
	},
};

static struct soc_camera_host_ops sh_mobile_ceu_host_ops = {
	.owner		= THIS_MODULE,
	.add		= sh_mobile_ceu_add_device,
	.remove		= sh_mobile_ceu_remove_device,
	.get_formats	= sh_mobile_ceu_get_formats,
	.put_formats	= sh_mobile_ceu_put_formats,
	.set_crop	= sh_mobile_ceu_set_crop,
	.set_fmt	= sh_mobile_ceu_set_fmt,
	.try_fmt	= sh_mobile_ceu_try_fmt,
	.set_ctrl	= sh_mobile_ceu_set_ctrl,
	.get_ctrl	= sh_mobile_ceu_get_ctrl,
	.reqbufs	= sh_mobile_ceu_reqbufs,
	.poll		= sh_mobile_ceu_poll,
	.querycap	= sh_mobile_ceu_querycap,
	.set_bus_param	= sh_mobile_ceu_set_bus_param,
	.init_videobuf	= sh_mobile_ceu_init_videobuf,
	.controls	= sh_mobile_ceu_controls,
	.num_controls	= ARRAY_SIZE(sh_mobile_ceu_controls),
};

static int __devinit sh_mobile_ceu_probe(struct platform_device *pdev)
{
	struct sh_mobile_ceu_dev *pcdev;
	struct resource *res;
	void __iomem *base;
	unsigned int irq;
	int err = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res || !irq) {
		dev_err(&pdev->dev, "Not enough CEU platform resources.\n");
		err = -ENODEV;
		goto exit;
	}

	pcdev = kzalloc(sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "Could not allocate pcdev\n");
		err = -ENOMEM;
		goto exit;
	}

	INIT_LIST_HEAD(&pcdev->capture);
	spin_lock_init(&pcdev->lock);

	pcdev->pdata = pdev->dev.platform_data;
	if (!pcdev->pdata) {
		err = -EINVAL;
		dev_err(&pdev->dev, "CEU platform data not set.\n");
		goto exit_kfree;
	}

	base = ioremap_nocache(res->start, resource_size(res));
	if (!base) {
		err = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap CEU registers.\n");
		goto exit_kfree;
	}

	pcdev->irq = irq;
	pcdev->base = base;
	pcdev->video_limit = 0; /* only enabled if second resource exists */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		err = dma_declare_coherent_memory(&pdev->dev, res->start,
						  res->start,
						  resource_size(res),
						  DMA_MEMORY_MAP |
						  DMA_MEMORY_EXCLUSIVE);
		if (!err) {
			dev_err(&pdev->dev, "Unable to declare CEU memory.\n");
			err = -ENXIO;
			goto exit_iounmap;
		}

		pcdev->video_limit = resource_size(res);
	}

	/* request irq */
	err = request_irq(pcdev->irq, sh_mobile_ceu_irq, IRQF_DISABLED,
			  dev_name(&pdev->dev), pcdev);
	if (err) {
		dev_err(&pdev->dev, "Unable to register CEU interrupt.\n");
		goto exit_release_mem;
	}

	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);

	pcdev->ici.priv = pcdev;
	pcdev->ici.v4l2_dev.dev = &pdev->dev;
	pcdev->ici.nr = pdev->id;
	pcdev->ici.drv_name = dev_name(&pdev->dev);
	pcdev->ici.ops = &sh_mobile_ceu_host_ops;

	err = soc_camera_host_register(&pcdev->ici);
	if (err)
		goto exit_free_irq;

	return 0;

exit_free_irq:
	free_irq(pcdev->irq, pcdev);
exit_release_mem:
	if (platform_get_resource(pdev, IORESOURCE_MEM, 1))
		dma_release_declared_memory(&pdev->dev);
exit_iounmap:
	iounmap(base);
exit_kfree:
	kfree(pcdev);
exit:
	return err;
}

static int __devexit sh_mobile_ceu_remove(struct platform_device *pdev)
{
	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct sh_mobile_ceu_dev *pcdev = container_of(soc_host,
					struct sh_mobile_ceu_dev, ici);

	soc_camera_host_unregister(soc_host);
	free_irq(pcdev->irq, pcdev);
	if (platform_get_resource(pdev, IORESOURCE_MEM, 1))
		dma_release_declared_memory(&pdev->dev);
	iounmap(pcdev->base);
	kfree(pcdev);
	return 0;
}

static int sh_mobile_ceu_runtime_nop(struct device *dev)
{
	/* Runtime PM callback shared between ->runtime_suspend()
	 * and ->runtime_resume(). Simply returns success.
	 *
	 * This driver re-initializes all registers after
	 * pm_runtime_get_sync() anyway so there is no need
	 * to save and restore registers here.
	 */
	return 0;
}

static struct dev_pm_ops sh_mobile_ceu_dev_pm_ops = {
	.runtime_suspend = sh_mobile_ceu_runtime_nop,
	.runtime_resume = sh_mobile_ceu_runtime_nop,
};

static struct platform_driver sh_mobile_ceu_driver = {
	.driver 	= {
		.name	= "sh_mobile_ceu",
		.pm	= &sh_mobile_ceu_dev_pm_ops,
	},
	.probe		= sh_mobile_ceu_probe,
	.remove		= __exit_p(sh_mobile_ceu_remove),
};

static int __init sh_mobile_ceu_init(void)
{
	return platform_driver_register(&sh_mobile_ceu_driver);
}

static void __exit sh_mobile_ceu_exit(void)
{
	platform_driver_unregister(&sh_mobile_ceu_driver);
}

module_init(sh_mobile_ceu_init);
module_exit(sh_mobile_ceu_exit);

MODULE_DESCRIPTION("SuperH Mobile CEU driver");
MODULE_AUTHOR("Magnus Damm");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sh_mobile_ceu");
