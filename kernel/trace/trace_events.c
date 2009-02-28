/*
 * event tracer
 *
 * Copyright (C) 2008 Red Hat Inc, Steven Rostedt <srostedt@redhat.com>
 *
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/ctype.h>

#include "trace_events.h"

#define events_for_each(event)						\
	for (event = __start_ftrace_events;				\
	     (unsigned long)event < (unsigned long)__stop_ftrace_events; \
	     event++)

void event_trace_printk(unsigned long ip, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	tracing_record_cmdline(current);
	trace_vprintk(ip, task_curr_ret_stack(current), fmt, ap);
	va_end(ap);
}

static void ftrace_clear_events(void)
{
	struct ftrace_event_call *call = (void *)__start_ftrace_events;


	while ((unsigned long)call < (unsigned long)__stop_ftrace_events) {

		if (call->enabled) {
			call->enabled = 0;
			call->unregfunc();
		}
		call++;
	}
}

static int ftrace_set_clr_event(char *buf, int set)
{
	struct ftrace_event_call *call = __start_ftrace_events;


	events_for_each(call) {

		if (!call->name)
			continue;

		if (strcmp(buf, call->name) != 0)
			continue;

		if (set) {
			/* Already set? */
			if (call->enabled)
				return 0;
			call->enabled = 1;
			call->regfunc();
		} else {
			/* Already cleared? */
			if (!call->enabled)
				return 0;
			call->enabled = 0;
			call->unregfunc();
		}
		return 0;
	}
	return -EINVAL;
}

/* 128 should be much more than enough */
#define EVENT_BUF_SIZE		127

static ssize_t
ftrace_event_write(struct file *file, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	size_t read = 0;
	int i, set = 1;
	ssize_t ret;
	char *buf;
	char ch;

	if (!cnt || cnt < 0)
		return 0;

	ret = get_user(ch, ubuf++);
	if (ret)
		return ret;
	read++;
	cnt--;

	/* skip white space */
	while (cnt && isspace(ch)) {
		ret = get_user(ch, ubuf++);
		if (ret)
			return ret;
		read++;
		cnt--;
	}

	/* Only white space found? */
	if (isspace(ch)) {
		file->f_pos += read;
		ret = read;
		return ret;
	}

	buf = kmalloc(EVENT_BUF_SIZE+1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (cnt > EVENT_BUF_SIZE)
		cnt = EVENT_BUF_SIZE;

	i = 0;
	while (cnt && !isspace(ch)) {
		if (!i && ch == '!')
			set = 0;
		else
			buf[i++] = ch;

		ret = get_user(ch, ubuf++);
		if (ret)
			goto out_free;
		read++;
		cnt--;
	}
	buf[i] = 0;

	file->f_pos += read;

	ret = ftrace_set_clr_event(buf, set);
	if (ret)
		goto out_free;

	ret = read;

 out_free:
	kfree(buf);

	return ret;
}

static void *
t_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ftrace_event_call *call = m->private;
	struct ftrace_event_call *next = call;

	(*pos)++;

	if ((unsigned long)call >= (unsigned long)__stop_ftrace_events)
		return NULL;

	m->private = ++next;

	return call;
}

static void *t_start(struct seq_file *m, loff_t *pos)
{
	return t_next(m, NULL, pos);
}

static void *
s_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ftrace_event_call *call = m->private;
	struct ftrace_event_call *next;

	(*pos)++;

 retry:
	if ((unsigned long)call >= (unsigned long)__stop_ftrace_events)
		return NULL;

	if (!call->enabled) {
		call++;
		goto retry;
	}

	next = call;
	m->private = ++next;

	return call;
}

static void *s_start(struct seq_file *m, loff_t *pos)
{
	return s_next(m, NULL, pos);
}

static int t_show(struct seq_file *m, void *v)
{
	struct ftrace_event_call *call = v;

	seq_printf(m, "%s\n", call->name);

	return 0;
}

static void t_stop(struct seq_file *m, void *p)
{
}

static int
ftrace_event_seq_open(struct inode *inode, struct file *file)
{
	int ret;
	const struct seq_operations *seq_ops;

	if ((file->f_mode & FMODE_WRITE) &&
	    !(file->f_flags & O_APPEND))
		ftrace_clear_events();

	seq_ops = inode->i_private;
	ret = seq_open(file, seq_ops);
	if (!ret) {
		struct seq_file *m = file->private_data;

		m->private = __start_ftrace_events;
	}
	return ret;
}

static ssize_t
event_enable_read(struct file *filp, char __user *ubuf, size_t cnt,
		  loff_t *ppos)
{
	struct ftrace_event_call *call = filp->private_data;
	char *buf;

	if (call->enabled)
		buf = "1\n";
	else
		buf = "0\n";

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, 2);
}

static ssize_t
event_enable_write(struct file *filp, const char __user *ubuf, size_t cnt,
		   loff_t *ppos)
{
	struct ftrace_event_call *call = filp->private_data;
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	switch (val) {
	case 0:
		if (!call->enabled)
			break;

		call->enabled = 0;
		call->unregfunc();
		break;
	case 1:
		if (call->enabled)
			break;

		call->enabled = 1;
		call->regfunc();
		break;

	default:
		return -EINVAL;
	}

	*ppos += cnt;

	return cnt;
}

static const struct seq_operations show_event_seq_ops = {
	.start = t_start,
	.next = t_next,
	.show = t_show,
	.stop = t_stop,
};

static const struct seq_operations show_set_event_seq_ops = {
	.start = s_start,
	.next = s_next,
	.show = t_show,
	.stop = t_stop,
};

static const struct file_operations ftrace_avail_fops = {
	.open = ftrace_event_seq_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations ftrace_set_event_fops = {
	.open = ftrace_event_seq_open,
	.read = seq_read,
	.write = ftrace_event_write,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations ftrace_enable_fops = {
	.open = tracing_open_generic,
	.read = event_enable_read,
	.write = event_enable_write,
};

static struct dentry *event_trace_events_dir(void)
{
	static struct dentry *d_tracer;
	static struct dentry *d_events;

	if (d_events)
		return d_events;

	d_tracer = tracing_init_dentry();
	if (!d_tracer)
		return NULL;

	d_events = debugfs_create_dir("events", d_tracer);
	if (!d_events)
		pr_warning("Could not create debugfs "
			   "'events' directory\n");

	return d_events;
}

struct event_subsystem {
	struct list_head	list;
	const char		*name;
	struct dentry		*entry;
};

static LIST_HEAD(event_subsystems);

static struct dentry *
event_subsystem_dir(const char *name, struct dentry *d_events)
{
	struct event_subsystem *system;

	/* First see if we did not already create this dir */
	list_for_each_entry(system, &event_subsystems, list) {
		if (strcmp(system->name, name) == 0)
			return system->entry;
	}

	/* need to create new entry */
	system = kmalloc(sizeof(*system), GFP_KERNEL);
	if (!system) {
		pr_warning("No memory to create event subsystem %s\n",
			   name);
		return d_events;
	}

	system->entry = debugfs_create_dir(name, d_events);
	if (!system->entry) {
		pr_warning("Could not create event subsystem %s\n",
			   name);
		kfree(system);
		return d_events;
	}

	system->name = name;
	list_add(&system->list, &event_subsystems);

	return system->entry;
}

static int
event_create_dir(struct ftrace_event_call *call, struct dentry *d_events)
{
	struct dentry *entry;

	/*
	 * If the trace point header did not define TRACE_SYSTEM
	 * then the system would be called "TRACE_SYSTEM".
	 */
	if (strcmp(call->system, "TRACE_SYSTEM") != 0)
		d_events = event_subsystem_dir(call->system, d_events);

	call->dir = debugfs_create_dir(call->name, d_events);
	if (!call->dir) {
		pr_warning("Could not create debugfs "
			   "'%s' directory\n", call->name);
		return -1;
	}

	entry = debugfs_create_file("enable", 0644, call->dir, call,
				    &ftrace_enable_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'%s/enable' entry\n", call->name);

	return 0;
}

static __init int event_trace_init(void)
{
	struct ftrace_event_call *call = __start_ftrace_events;
	struct dentry *d_tracer;
	struct dentry *entry;
	struct dentry *d_events;

	d_tracer = tracing_init_dentry();
	if (!d_tracer)
		return 0;

	entry = debugfs_create_file("available_events", 0444, d_tracer,
				    (void *)&show_event_seq_ops,
				    &ftrace_avail_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'available_events' entry\n");

	entry = debugfs_create_file("set_event", 0644, d_tracer,
				    (void *)&show_set_event_seq_ops,
				    &ftrace_set_event_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'set_event' entry\n");

	d_events = event_trace_events_dir();
	if (!d_events)
		return 0;

	events_for_each(call) {
		/* The linker may leave blanks */
		if (!call->name)
			continue;
		event_create_dir(call, d_events);
	}

	return 0;
}
fs_initcall(event_trace_init);
