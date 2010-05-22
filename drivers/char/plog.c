/*
 * 	FIH 2009 Chihchia 
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/switch.h>

#include <asm/uaccess.h>

const char flush_plog[] = "flush_plog!";

static int plog_read (struct file *filp, char __user *buffer,
			size_t count, loff_t *ppos);
static int plog_write (struct file *filp, char __user *buffer,
			size_t count, loff_t *ppos);

static struct switch_dev plogflush_switch_dev = {
	.name = "plogflush_switch",
};


static const struct file_operations plog_fops = {
	.owner		= THIS_MODULE,
	.read		= plog_read,
	.write          = plog_write,
};


static struct miscdevice plog_misc_device = {
	MISC_DYNAMIC_MINOR,
	"plog",
	&plog_fops,
};


static struct work_struct plogflush_work_struct;

static void plogflush_work(struct work_struct *work)
{
	printk(KERN_INFO "[cch] %s:%s LINE:%d \n", __FILE__, __func__, __LINE__);
	plogflush_switch_dev.state = 0;
	switch_set_state(&plogflush_switch_dev, 1);
}


static int plog_read (struct file *filp, char __user *buffer,
			size_t count, loff_t *ppos)
{
	return 0;
}

void flush_plog_to_file()
{
	schedule_work(&plogflush_work_struct);
}
EXPORT_SYMBOL(flush_plog_to_file);

static int plog_write (struct file *filp, char __user *buffer,
			size_t count, loff_t *ppos)
{
        char buf[4096];
        int len = 0;
	memset(buf, 0, 4096);
        len = copy_from_user(buf, buffer, 4096);
	
	if(strncmp(flush_plog, buf, strlen(flush_plog)) == 0)
	{
		flush_plog_to_file();
	}
	return strlen(buf);
}

static int __init plog_init(void)
{
	printk(KERN_INFO "[cch] %s:%s LINE:%d \n", __FILE__, __func__, __LINE__);
	if (misc_register (&plog_misc_device)) {
		//printk (KERN_WARNING "nwbutton: Couldn't register device 10, "
		//		"%d.\n", BUTTON_MINOR);
		printk(KERN_INFO "[cch] misc_register fail \n");
		return -EBUSY;
	}

	if (switch_dev_register(&plogflush_switch_dev) < 0) {
		printk(KERN_INFO "[cch] switch_dev_register failed\n");
	}
	else
	{
		printk(KERN_INFO "[cch] switch_dev_register success\n");
		INIT_WORK(&plogflush_work_struct, plogflush_work);
	}
	return 0;
}

static void __exit plog_exit (void) 
{
	printk(KERN_INFO "[cch] %s:%s LINE:%d \n", __FILE__, __func__, __LINE__);
	misc_deregister (&plog_misc_device);

	switch_dev_unregister(&plogflush_switch_dev);
}


MODULE_AUTHOR("Chihchia Hung");
MODULE_LICENSE("GPL");

module_init(plog_init);
module_exit(plog_exit);
