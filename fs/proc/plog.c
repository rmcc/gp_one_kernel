/*
 *  linux/fs/proc/plog.c
 *
 *  Copyright (C) 2009  by FIHTDC Chih-Chia
 *
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/io.h>


#include "internal.h"

#define PLOG_BUFFER (1 << CONFIG_LOG_BUF_SHIFT)
static int offset = 0;


static int plog_open(struct inode * inode, struct file * file)
{
	offset = 0;
	printk(KERN_INFO "[cch] %s:%s LINE:%d offset:%d\n", __FILE__, __func__, __LINE__, offset);
	return 0;
}

static int plog_release(struct inode * inode, struct file * file)
{
	printk(KERN_INFO "[cch] %s:%s LINE:%d \n", __FILE__, __func__, __LINE__);
	return 0;
}


extern int plog_buf_copy(char *dest, int idx, int len);
extern int log_buf_copy(char *dest, int idx, int len);

static ssize_t plog_read(struct file *file, char __user *buf,
			 size_t count, loff_t *ppos)
{
	int read_len = 0;
	int plog_len = 0;
	char tmp[4097];

	plog_len = plog_buf_copy(tmp, offset, count);

	if(plog_len <= 0)
	{
		offset = 0;
		return read_len;
	}	

	offset += plog_len;

	if(plog_len > count)
	{
		copy_to_user(buf, tmp, count);
		read_len = count;
	}
	else
	{
		copy_to_user(buf, tmp, plog_len);
		read_len = plog_len;
	}
	return read_len;
}

static unsigned int plog_poll(struct file *file, poll_table *wait)
{
	printk(KERN_INFO "[cch] %s:%s LINE:%d \n", __FILE__, __func__, __LINE__);
	return 0;
}


const struct file_operations proc_plog_operations = {
	.read		= plog_read,
	.poll		= plog_poll,
	.open		= plog_open,
	.release	= plog_release,
};
