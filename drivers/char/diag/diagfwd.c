/* drivers/char/diag/diagfwd.c */

/* Copyright (c) 2008 QUALCOMM USA, INC. 
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <mach/usbdiag.h>
#include <mach/msm_smd.h>
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagchar_hdlc.h"

// FIH +++
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/rtc.h>
// FIH ---

MODULE_DESCRIPTION("Diag Char Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

/* Size of the USB buffers used for read and write*/
#define USB_MAX_BUF 4096

/* Size of the buffer used for deframing a packet
  reveived from the PC tool*/
#define HDLC_MAX 4096

/* Number of maximum USB requests that the USB layer should handle at
   one time. */
#define MAX_DIAG_USB_REQUESTS 12

// FIH +++
#define MAX_W_BUF_SIZE 65535
#define SAFE_W_BUF_SIZE 50000

static DEFINE_SPINLOCK(smd_lock);
static DECLARE_WAIT_QUEUE_HEAD(diag_wait_queue);

static unsigned char *pBuf = NULL;
static unsigned char *pBuf_Curr = NULL;

static int gBuf_Size = 0;

static struct file *gLog_filp = NULL;
static struct file *gFilter_filp = NULL;

static struct task_struct *kLogTsk;
static struct task_struct *kLoadFilterTsk;

static char gPath[] = "/sdcard/log/";
static char gLogFilePath[64];
static char gFilterFileName[] = "filter.bin";

static int bCaptureFilter = 0;
static int bLogStart = 0;
static int gTotalWrite = 0;
static int bWriteSD = 0;
static int bGetRespNow = 0;

static void diag_start_log(void);
static void diag_stop_log(void);
static int open_qxdm_filter_file(void);
static int close_qxdm_filter_file(void);

static void diag_log_smd_read(void)
{
	int sz;
	void *buf;
	unsigned long flags;
	
	for (;;) {
		sz = smd_cur_packet_size(driver->ch);
		if (sz == 0)
			break;
		if (sz > smd_read_avail(driver->ch)) {
			printk(KERN_INFO "[diagfwd.c][WARNING] sz > smd_read_avail(), sz = %d\n", sz);
			break;
		}
		if (sz > USB_MAX_BUF) {
			printk(KERN_INFO "[diagfwd.c][ERROR] sz > USB_MAX_BUF, sz = %d\n", sz);
			smd_read(driver->ch, 0, sz);
			continue;
		}
		
		buf = driver->usb_buf_in;
		
		if (!buf) {
			printk(KERN_INFO "Out of diagmem for a9\n");
			break;
		}

		if (smd_read(driver->ch, buf, sz) != sz) {
			printk(KERN_INFO "[diagfwd.c][WARNING] not enough data?!\n");
			continue;
		}
		
		spin_lock_irqsave(&smd_lock, flags);
		memcpy(pBuf_Curr, buf, sz);
		pBuf_Curr += sz;
		gBuf_Size += sz;
		spin_unlock_irqrestore(&smd_lock, flags);
		
		if ((gBuf_Size >= SAFE_W_BUF_SIZE) || bGetRespNow) {
			wake_up_interruptible(&diag_wait_queue);
		}
		
		gTotalWrite += sz;
	}
}
// FIH ---

static void diag_smd_send_req(void)
{
	void *buf;

	if (driver->ch && (!driver->in_busy)) {
		int r = smd_read_avail(driver->ch);

		if (r > USB_MAX_BUF) {
			printk(KERN_INFO "diag dropped num bytes = %d\n", r);
			return;
		}
		if (r > 0) {

			buf = driver->usb_buf_in;
			if (!buf) {
				printk(KERN_INFO "Out of diagmem for a9\n");
			} else {
				smd_read_from_cb(driver->ch, buf, r);
				driver->in_busy = 1;
				diag_write(buf, r);
			}
		}
	}
}

static void diag_smd_qdsp_send_req(void)
{
	void *buf;

	if (driver->chqdsp && (!driver->in_busy_qdsp)) {
		int r = smd_read_avail(driver->chqdsp);

		if (r > USB_MAX_BUF) {
			printk(KERN_INFO "diag dropped num bytes = %d\n", r);
			return;
		}
		if (r > 0) {

			buf = driver->usb_buf_in_qdsp;
			if (!buf) {
				printk(KERN_INFO "Out of diagmem for q6\n");
			} else {
				smd_read_from_cb(driver->chqdsp, buf, r);
				driver->in_busy_qdsp = 1;
				diag_write(buf, r);
			}
		}

	}
}

static void diag_print_mask_table(void)
{
/* Enable this to print mask table when updated */
#ifdef MASK_DEBUG
	int first;
	int last;
	uint8_t *ptr = driver->msg_masks;
	int i = 0;

	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		printk(KERN_INFO "SSID %d - %d\n", first, last);
		for (i = 0 ; i <= last - first ; i++)
			printk(KERN_INFO "MASK:%x\n", *((uint32_t *)ptr + i));
		ptr += ((last - first) + 1)*4;

	}
#endif
}

static void diag_update_msg_mask(int start, int end , uint8_t *buf)
{
	int found = 0;
	int first;
	int last;
	uint8_t *ptr = driver->msg_masks;
	mutex_lock(&driver->diagchar_mutex);
	/* First SSID can be zero : So check that last is non-zero */

	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		if (start >= first && start <= last) {
			ptr += (start - first)*4;
			if (end <= last)
					memcpy(ptr, buf ,
						   (((end - start)+1)*4));
			else
				printk(KERN_INFO "Unable to copy mask "
						 "change \n");

			found = 1;
			break;
		} else {
			ptr += ((last - first) + 1)*4;
		}
	}
	/* Entry was not found - add new table */
	if (!found) {
			memcpy(ptr, &(start) , 4);
			ptr += 4;
			memcpy(ptr, &(end), 4);
			ptr += 4;
			memcpy(ptr, buf , ((end - start) + 1)*4);

	}
	mutex_unlock(&driver->diagchar_mutex);
	diag_print_mask_table();

}

static void diag_update_event_mask(uint8_t *buf, int toggle, int num_bits)
{
	uint8_t *ptr = driver->event_masks;
	uint8_t *temp = buf + 2;

	mutex_lock(&driver->diagchar_mutex);
	if (!toggle)
		memset(ptr, 0 , EVENT_MASK_SIZE);
	else
			memcpy(ptr, temp , num_bits/8 + 1);
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_update_log_mask(uint8_t *buf, int num_items)
{
	uint8_t *ptr = driver->log_masks;
	uint8_t *temp = buf;

	mutex_lock(&driver->diagchar_mutex);
		memcpy(ptr, temp , (num_items+7)/8);
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_update_pkt_buffer(unsigned char *buf)
{
	unsigned char *ptr = driver->pkt_buf;
	unsigned char *temp = buf;

	mutex_lock(&driver->diagchar_mutex);
		memcpy(ptr, temp , driver->pkt_length);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_userspace_clients(unsigned int type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i] != 0)
			driver->data_ready[i] |= type;
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_sleeping_process(int process_id)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i] == process_id) {
			driver->data_ready[i] |= PKT_TYPE;
			break;
		}
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

static int diag_process_apps_pkt(unsigned char *buf, int len)
{
	uint16_t start;
	uint16_t end, subsys_cmd_code;
	int i, cmd_code, subsys_id;
	int packet_type = 1;
	unsigned char *temp = buf;

	/* event mask */
	if ((*buf == 0x60) && (*(++buf) == 0x0)) {
		diag_update_event_mask(buf, 0, 0);
		diag_update_userspace_clients(EVENT_MASKS_TYPE);
	}
	/* check for set event mask */
	else if (*buf == 0x82) {
		buf += 4;
		diag_update_event_mask(buf, 1, *(uint16_t *)buf);
		diag_update_userspace_clients(
		EVENT_MASKS_TYPE);
	}
	/* log mask */
	else if (*buf == 0x73) {
		buf += 4;
		if (*(int *)buf == 3) {
			buf += 8;
			diag_update_log_mask(buf+4, *(int *)buf);
			diag_update_userspace_clients(LOG_MASKS_TYPE);
		}
	}
	/* Check for set message mask  */
	else if ((*buf == 0x7d) && (*(++buf) == 0x4)) {
		buf++;
		start = *(uint16_t *)buf;
		buf += 2;
		end = *(uint16_t *)buf;
		buf += 4;
		diag_update_msg_mask((uint32_t)start, (uint32_t)end , buf);
		diag_update_userspace_clients(MSG_MASKS_TYPE);
	}
	/* Set all run-time masks
	if ((*buf == 0x7d) && (*(++buf) == 0x5)) {
		TO DO
	} */

	/* Check for registered clients and forward packet to user-space */
	else{
		cmd_code = (int)(*(char *)buf);
		temp++;
		subsys_id = (int)(*(char *)temp);
		temp++;
		subsys_cmd_code = *(uint16_t *)temp;
		temp += 2;

		for (i = 0; i < REG_TABLE_SIZE; i++) {
			if (driver->table[i].process_id != 0) {
				if (driver->table[i].cmd_code ==
				     cmd_code && driver->table[i].subsys_id ==
				     subsys_id &&
				    driver->table[i].cmd_code_lo <=
				     subsys_cmd_code &&
					  driver->table[i].cmd_code_hi >=
				     subsys_cmd_code){
					driver->pkt_length = len;
					diag_update_pkt_buffer(buf);
					diag_update_sleeping_process(
						driver->table[i].process_id);
						return 0;
				    } /* end of if */
				else if (driver->table[i].cmd_code == 255
					  && cmd_code == 75) {
					if (driver->table[i].subsys_id ==
					    subsys_id &&
					   driver->table[i].cmd_code_lo <=
					    subsys_cmd_code &&
					     driver->table[i].cmd_code_hi >=
					    subsys_cmd_code){
						driver->pkt_length = len;
						diag_update_pkt_buffer(buf);
						diag_update_sleeping_process(
							driver->table[i].
							process_id);
						return 0;
					}
				} /* end of else-if */
				else if (driver->table[i].cmd_code == 255 &&
					  driver->table[i].subsys_id == 255) {
					if (driver->table[i].cmd_code_lo <=
							 cmd_code &&
						     driver->table[i].
						    cmd_code_hi >= cmd_code){
						driver->pkt_length = len;
						diag_update_pkt_buffer(buf);
						diag_update_sleeping_process
							(driver->table[i].
							 process_id);
						return 0;
					}
				} /* end of else-if */
			} /* if(driver->table[i].process_id != 0) */
		}  /* for (i = 0; i < REG_TABLE_SIZE; i++) */
	} /* else */
		return packet_type;
}

static void diag_process_hdlc(void *data, unsigned len)
{
	struct diag_hdlc_decode_type hdlc;
	int ret, type;

	hdlc.dest_ptr = driver->hdlc_buf;
	hdlc.dest_size = USB_MAX_BUF;
	hdlc.src_ptr = data;
	hdlc.src_size = len;
	hdlc.src_idx = 0;
	hdlc.dest_idx = 0;
	hdlc.escaping = 0;

	ret = diag_hdlc_decode(&hdlc);

	if (ret)
		type = diag_process_apps_pkt(driver->hdlc_buf,
					      hdlc.dest_idx - 3);

	/* ignore 2 bytes for CRC, one for 7E and send */
	if ((driver->ch) && (ret) && (type) && (hdlc.dest_idx > 3))
		smd_write(driver->ch, driver->hdlc_buf, hdlc.dest_idx - 3);
}

int diagfwd_connect(void)
{
	diag_open(driver->poolsize + 3); /* 2 for A9 ; 1 for q6*/

	driver->usb_connected = 1;
	driver->in_busy = 0;
	driver->in_busy_qdsp = 0;

	diag_read(driver->usb_buf_out, USB_MAX_BUF);
	return 0;
}

int diagfwd_disconnect(void)
{
	driver->usb_connected = 0;
	driver->in_busy = 1;
	driver->in_busy_qdsp = 1;

	diag_close();
	/* TBD - notify and flow control SMD */
	return 0;
}

int diagfwd_write_complete(unsigned char *buf, int len, int status)
{
	/*Determine if the write complete is for data from arm9/apps/q6 */
	/* Need a context variable here instead */
	if (buf == (void *)driver->usb_buf_in) {
		driver->in_busy = 0;
		diag_smd_send_req();
	} else if (buf == (void *)driver->usb_buf_in_qdsp) {
		driver->in_busy_qdsp = 0;
		diag_smd_qdsp_send_req();
	} else
		diagmem_free(driver, buf);

	return 0;
}

int diagfwd_read_complete(unsigned char *buf, int len, int status)
{
	driver->read_len = len;
	schedule_work(&(driver->diag_read_work));
	return 0;
}

static struct diag_operations diagfwdops = {
	.diag_connect = diagfwd_connect,
	.diag_disconnect = diagfwd_disconnect,
	.diag_char_write_complete = diagfwd_write_complete,
	.diag_char_read_complete = diagfwd_read_complete
};

static void diag_smd_notify(void *ctxt, unsigned event)
{
	if (bLogStart) {
		if (event != SMD_EVENT_DATA)
			return;
		diag_log_smd_read();
	}else {
	diag_smd_send_req();
	}
}

static void diag_smd_qdsp_notify(void *ctxt, unsigned event)
{
	diag_smd_qdsp_send_req();
}

// FIH +++
static ssize_t qxdm2sd_run(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned LogRun;
	
	sscanf(buf, "%1d\n", &LogRun);
	
	dev_dbg(dev, "%s: %d %d\n", __func__, count, LogRun);

	if (LogRun == 1)
	{
		// Open log file and start capture
		diag_start_log();
	}
	else if (LogRun == 0)
	{
		// Close log file and stop capture
		diag_stop_log();
	}
	else if (LogRun == 2)
	{
		open_qxdm_filter_file();
	}
	else if (LogRun == 3)
	{
		close_qxdm_filter_file();
	}
	
	return count;
}

DEVICE_ATTR(qxdm2sd, 0644, NULL, qxdm2sd_run);
// FIH ---

static int diag_smd_probe(struct platform_device *pdev)
{
	int r = 0;

	if (pdev->id == 0) {
		if (driver->usb_buf_in == NULL &&
			(driver->usb_buf_in =
			kzalloc(USB_MAX_BUF, GFP_KERNEL)) == NULL)

			goto err;
		else

		r = smd_open("DIAG", &driver->ch, driver, diag_smd_notify);
	}
#if defined(CONFIG_MSM_N_WAY_SMD)
	if (pdev->id == 1) {
		if (driver->usb_buf_in_qdsp == NULL &&
			(driver->usb_buf_in_qdsp =
			kzalloc(USB_MAX_BUF, GFP_KERNEL)) == NULL)

			goto err;
		else

		r = smd_named_open_on_edge("DIAG", SMD_APPS_QDSP,
			&driver->chqdsp, driver, diag_smd_qdsp_notify);

	}
#endif
	printk(KERN_INFO "diag opened SMD port ; r = %d\n", r);
// FIH +++
	r = device_create_file(&pdev->dev, &dev_attr_qxdm2sd);

	if (r < 0)
	{
		dev_err(&pdev->dev, "%s: Create qxdm2sd attribute \"qxdm2sd\" failed!! <%d>", __func__, r);
	}
// FIH ---
err:
	return 0;
}

static struct platform_driver msm_smd_ch1_driver = {

	.probe = diag_smd_probe,
	.driver = {
		   .name = "DIAG",
		   .owner = THIS_MODULE,
		   },
};

void diag_read_work_fn(struct work_struct *work)
{
// FIH +++
	unsigned char *head = NULL;
// FIH ---
	diag_process_hdlc(driver->usb_buf_out, driver->read_len);
	diag_read(driver->usb_buf_out, USB_MAX_BUF);
// FIH +++
	if (bCaptureFilter)
	{
		if (gFilter_filp != NULL)
		{
			head = driver->usb_buf_out;

			if (head[0] != 0x4b && head[1] != 0x04)
			{
				gFilter_filp->f_op->write(gFilter_filp, (unsigned char __user *)driver->usb_buf_out, driver->read_len, &gFilter_filp->f_pos);

				printk(KERN_INFO "Filter string capture (%d).\n", driver->read_len);
			}
			
		}
	}
// FIH ---
}

// FIH +++
void diag_close_log_file_work_fn(struct work_struct *work)
{
	if (gLog_filp != NULL)
	{
		do_fsync(gLog_filp, 1);
		filp_close(gLog_filp, NULL);
		gLog_filp = NULL;
		
		kfree(pBuf);

		printk(KERN_INFO "[diagfwd.c][DEBUG] Log file %s (%d) is closed.\n", gLogFilePath, gTotalWrite);
	}
	
	if (gFilter_filp != NULL)
	{
		// Close filter file
		do_fsync(gFilter_filp, 1);
		filp_close(gFilter_filp, NULL);
		gFilter_filp = NULL;
		
		printk(KERN_INFO "Filter file %s closed.\n", gFilterFileName);
	}
}

static int diag_check_response(unsigned char *str1, unsigned char *str2, int len1, int len2)
{
	int i = 0;
	int bPass = 0;
	unsigned char *buf1 = str1;
	unsigned char *buf2 = str2;
	unsigned char *buf3 = NULL;

	unsigned char *pos = NULL;

	if (*buf1 == 0x7d)
	{
		buf3 = kzalloc(7, GFP_KERNEL);
		memcpy(buf3, buf1, 7);
		
		for (i=0;i<len2;i++)
		{
			if (*buf2 == 0x7d)
			{
				if (memcmp(buf2, buf3, 7) == 0)
				{
					if (*(buf2+7) == 0x01)
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter SUCCESS!\n");
						bPass = 1;
						break;
					}
					else
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter FAIL!\n");
						bPass = 0;
						// +++ Debug +++
						pos = buf1;
						printk(KERN_INFO "==============================> Send\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						
						pos = buf2;
						printk(KERN_INFO "==============================> Return\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						// --- Debug ---
						break;
					}
				}
			}
			
			buf2++;
		}
		
		kfree(buf3);
	}
	
	if (*buf1 == 0x73)
	{
		buf3 = kzalloc(5, GFP_KERNEL);
		memcpy(buf3, buf1, 5);

		for (i=0;i<len2;i++)
		{
			if (*buf2 == 0x73)
			{
				if (memcmp(buf2, buf3, 5) == 0)
				{
					if (*(buf2+5) == 0 && *(buf2+6) == 0 && *(buf2+7) == 0 && *(buf2+8) == 0)
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter SUCCESS!\n");
						bPass = 1;
						break;
					}
					else
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter FAIL!\n");
						bPass = 0;
						// +++ Debug +++
						pos = buf1;
						printk(KERN_INFO "==============================> Send\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						
						pos = buf2;
						printk(KERN_INFO "==============================> Return\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						// --- Debug ---
						break;
					}
				}
			}
			
			buf2++;
		}
		
		kfree(buf3);
	}
	
	if (*buf1 == 0x82)
	{
		buf3 = kzalloc(4, GFP_KERNEL);
		memcpy(buf3, (buf1+2), 4);

		for (i=0;i<len2;i++)
		{
			if (*buf2 == 0x82)
			{
				if (memcmp((buf2+2), buf3, 4) == 0)
				{
					if (*(buf2+1) == 0)
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter SUCCESS!\n");
						bPass = 1;
						break;
					}
					else
					{
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter FAIL!\n");
						bPass = 0;
						// +++ Debug +++
						pos = buf1;
						printk(KERN_INFO "==============================> Send\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						
						pos = buf2;
						printk(KERN_INFO "==============================> Return\n");
						while(1) {
							printk(KERN_INFO "%.2x ", *pos);
							if (*pos == 0x7e) break;
							pos++;
						}
						printk(KERN_INFO "\n");
						// --- Debug ---
						break;
					}
				}
			}
			
			buf2++;
		}
		
		kfree(buf3);
	}
	
	return bPass;
}

static int diag_write_filter_thread(void *__unused)
{
	unsigned long flags;
	struct file *Filter_filp = NULL;
	char FilterFile[64];
	unsigned char w_buf[512];
	unsigned char *p = NULL;
	unsigned char *r_buf = NULL;
	int nRead = 0;
	int w_size = 0;
	int r_size = 0;
	int bEOF = 0;
	int bSuccess = 0;
	int rt = 0;

	// Open filter.bin
	sprintf(FilterFile, "%s%s", gPath, gFilterFileName);
		
	Filter_filp = filp_open(FilterFile, O_RDONLY|O_LARGEFILE, 0);
	
	if (IS_ERR(Filter_filp))
	{
		Filter_filp = NULL;
		printk(KERN_ERR "[diagfwd.c][ERROR] Open filter file %s error!\n", FilterFile);
		goto err;
	}

	bGetRespNow = 1;

	// Read filter.bin
	r_buf = kzalloc(MAX_W_BUF_SIZE, GFP_KERNEL);

	p = kmalloc(1, GFP_KERNEL);

	for (;;) {
		w_size = 0;
		memset(w_buf, 0 , 512);
		
		do{
			nRead = Filter_filp->f_op->read(Filter_filp, (unsigned char __user *)p, 1, &Filter_filp->f_pos);

			if (nRead != 1) {
				bEOF = 1;
				break;
			}
			
			w_buf[w_size++] = *p;
		}while (*p != 0x7e);
		
		if (bEOF) break;
		
		// Ignore filter except MSG, LOG, EVENT
		if (w_buf[0] != 0x7d  && w_buf[0] != 0x73 && w_buf[0] != 0x82) {
			continue;
		}
		
		bSuccess = 0;
		rt = 5;
		
		switch (w_buf[0])
		{
			case 0x7d:
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set MSG filter string (%d).\n", w_size);
						break;
			case 0x73:
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set LOG filter string (%d).\n", w_size);
						break;
			case 0x82:
						printk(KERN_INFO "[diagfwd.c][DEBUG] Set EVENT filter string (%d).\n", w_size);
						break;
		}

		while (!bSuccess && rt--)
		{
			diag_process_hdlc(w_buf, w_size);

			wait_event_interruptible(diag_wait_queue, gBuf_Size && !bWriteSD);

			spin_lock_irqsave(&smd_lock, flags);

			r_size = gBuf_Size;
			memcpy(r_buf, pBuf, MAX_W_BUF_SIZE);
			gBuf_Size = 0;
			pBuf_Curr = pBuf;
			spin_unlock_irqrestore(&smd_lock, flags);

			bSuccess = diag_check_response(w_buf, r_buf, w_size, r_size);
			
			if (!bSuccess) {
				printk(KERN_INFO "[diagfwd.c][DEBUG] Try again.\n");
				msleep(100);
			}
		}

		if (rt <= 0) {
			printk(KERN_INFO "[diagfwd.c][DEBUG] Set filter FAIL!\n");
		}
	}

	// Close filter.bin
	kfree(r_buf);
	kfree(p);
	filp_close(Filter_filp, NULL);
	Filter_filp = NULL;

	bGetRespNow = 0;
	bWriteSD = 1;
	
	printk(KERN_INFO "[diagfwd.c][DEBUG] Load filter.bin is finished.\n");

err:
	return 0;
}

static int diag_log_write_thread(void *__unused)
{
	unsigned long flags;
	unsigned char *buf = NULL;
	int buf_size = 0;
	
	if ((buf = kzalloc(MAX_W_BUF_SIZE, GFP_KERNEL)) == NULL){
		printk(KERN_ERR "[diagfwd.c][ERROR] kzalloc fail!\n");
		return 0;
	}
	
	while (!kthread_should_stop())
	{
		wait_event_interruptible(diag_wait_queue, (gBuf_Size && bWriteSD) || kthread_should_stop());
		
		spin_lock_irqsave(&smd_lock, flags);

//		printk(KERN_INFO "[diagfwd.c][DEBUG] Buffer write to SD %d\n", gBuf_Size);

		buf_size = gBuf_Size;
		memcpy(buf, pBuf, MAX_W_BUF_SIZE);
		gBuf_Size = 0;
		pBuf_Curr = pBuf;
		spin_unlock_irqrestore(&smd_lock, flags);
		
		gLog_filp->f_op->write(gLog_filp, (unsigned char __user *)buf, buf_size, &gLog_filp->f_pos);
	}
	
	kfree(buf);
	return 0;
}

void diag_open_log_file_work_fn(struct work_struct *work)
{
	struct timespec ts;
	struct rtc_time tm;

	// Create log file
	if (gLog_filp == NULL)
	{
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);

		sprintf(gLogFilePath,
		"%sQXDM-%d-%02d-%02d-%02d-%02d-%02d.bin",
			gPath,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

		gLog_filp = filp_open(gLogFilePath, O_CREAT|O_WRONLY|O_LARGEFILE, 0);
		
		if (IS_ERR(gLog_filp))
		{
			gLog_filp = NULL;
			printk(KERN_ERR "[diagfwd.c][ERROR] Create log file %s error!\n", gLogFilePath);
			goto err;
		}
	}
	
	printk(KERN_INFO "[diagfwd.c][DEBUG] Log file %s is created.\n", gLogFilePath);
	
err:
	return;
}

static void diag_start_log(void)
{
	if (bLogStart) goto err;
	
	// Open log file
	schedule_work(&(driver->diag_open_log_file_work));

	// Allocate write buffer
	if ((pBuf = kzalloc(MAX_W_BUF_SIZE, GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "[diagfwd.c][ERROR] kzalloc fail!\n");
		goto err;
	}
	
	memset(pBuf, 0, MAX_W_BUF_SIZE);
	
	pBuf_Curr = pBuf;
	gBuf_Size = 0;
	gTotalWrite = 0;
	bLogStart = 1;
	bWriteSD = 0;

	// Start thread for write log to SD
	kLogTsk = kthread_create(diag_log_write_thread, NULL, "Write QXDM Log To File");
	
	if (!IS_ERR(kLogTsk)) {
		wake_up_process(kLogTsk);
	}else {
		printk(KERN_ERR "[diagfwd.c][ERROR] Start write log thread error.\n");
		goto err;
	}
	
	// Load filter
	kLoadFilterTsk = kthread_create(diag_write_filter_thread, NULL, "Load filter");

	if (!IS_ERR(kLoadFilterTsk)) {
		wake_up_process(kLoadFilterTsk);
	}else {
		printk(KERN_ERR "[diagfwd.c][ERROR] Start load filter thread error.\n");
		goto err;
	}

err:
	return;
}

static void diag_stop_log(void)
{
	if (bLogStart)
	{
		// Stop thread
		kthread_stop(kLogTsk);

		// Clear flag
		bLogStart = 0;

		// Close file
		schedule_work(&(driver->diag_close_log_file_work));
	}
}

static int open_qxdm_filter_file(void)
{
	char FilterFilePath[64];
	
	if (gFilter_filp == NULL)
	{
		sprintf(FilterFilePath, "%s%s", gPath, gFilterFileName);
		
		gFilter_filp = filp_open(FilterFilePath, O_CREAT|O_WRONLY|O_LARGEFILE, 0);
		
		if (IS_ERR(gFilter_filp))
		{
			gFilter_filp = NULL;
			printk(KERN_ERR "Create filter file %s error.\n", FilterFilePath);
			goto err;
		}

		bCaptureFilter = 1;

		printk(KERN_INFO "Create filter file %s success.\n", FilterFilePath);
	}

err:
	return 0;
}

static int close_qxdm_filter_file(void)
{
	if (bCaptureFilter)
	{
		bCaptureFilter = 0;
		schedule_work(&(driver->diag_close_log_file_work));
	}

	return 0;
}
// FIH ---

void diagfwd_init(void)
{

	if (driver->usb_buf_out  == NULL &&
	     (driver->usb_buf_out = kzalloc(USB_MAX_BUF, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->hdlc_buf == NULL
	    && (driver->hdlc_buf = kzalloc(HDLC_MAX, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->msg_masks == NULL
	    && (driver->msg_masks = kzalloc(MSG_MASK_SIZE,
					     GFP_KERNEL)) == NULL)
		goto err;
	if (driver->log_masks == NULL &&
	    (driver->log_masks = kzalloc(LOG_MASK_SIZE, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->event_masks == NULL &&
	    (driver->event_masks = kzalloc(EVENT_MASK_SIZE,
					    GFP_KERNEL)) == NULL)
		goto err;
	if (driver->client_map == NULL &&
	    (driver->client_map = kzalloc
	     (driver->num_clients, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->data_ready == NULL &&
	     (driver->data_ready = kzalloc(driver->num_clients,
					    GFP_KERNEL)) == NULL)
		goto err;
	if (driver->table == NULL &&
	     (driver->table = kzalloc(REG_TABLE_SIZE*
				      sizeof(struct diag_master_table),
				       GFP_KERNEL)) == NULL)
		goto err;
	if (driver->pkt_buf == NULL &&
	     (driver->pkt_buf = kzalloc(PKT_SIZE,
					 GFP_KERNEL)) == NULL)
		goto err;

	driver->diag_wq = create_singlethread_workqueue("diag_wq");
	INIT_WORK(&(driver->diag_read_work), diag_read_work_fn);
// FIH +++
	INIT_WORK(&(driver->diag_close_log_file_work), diag_close_log_file_work_fn);
	INIT_WORK(&(driver->diag_open_log_file_work), diag_open_log_file_work_fn);
// FIH ---

	diag_usb_register(&diagfwdops);

	platform_driver_register(&msm_smd_ch1_driver);

	return;
err:
		printk(KERN_INFO "\n Could not initialize diag buffers \n");
		kfree(driver->usb_buf_out);
		kfree(driver->hdlc_buf);
		kfree(driver->msg_masks);
		kfree(driver->log_masks);
		kfree(driver->event_masks);
		kfree(driver->client_map);
		kfree(driver->data_ready);
		kfree(driver->table);
		kfree(driver->pkt_buf);
}

void diagfwd_exit(void)
{
	smd_close(driver->ch);
	smd_close(driver->chqdsp);
	driver->ch = 0;		/*SMD can make this NULL */
	driver->chqdsp = 0;

	if (driver->usb_connected)
		diag_close();

	platform_driver_unregister(&msm_smd_ch1_driver);

	diag_usb_unregister();

	kfree(driver->usb_buf_in);
	kfree(driver->usb_buf_in_qdsp);
	kfree(driver->usb_buf_out);
	kfree(driver->hdlc_buf);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
	kfree(driver->client_map);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
}
