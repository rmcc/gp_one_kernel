#include <linux/kernel.h>
#include <linux/ftrace.h>
#include <asm/syscall.h>

#include "trace_output.h"
#include "trace.h"

static atomic_t refcount;

/* Our two options */
enum {
	TRACE_SYSCALLS_OPT_TYPES = 0x1,
};

static struct tracer_opt syscalls_opts[] = {
	{ TRACER_OPT(syscall_arg_type, TRACE_SYSCALLS_OPT_TYPES) },
	{ }
};

static struct tracer_flags syscalls_flags = {
	.val = 0, /* By default: no args types */
	.opts = syscalls_opts
};

enum print_line_t
print_syscall_enter(struct trace_iterator *iter, int flags)
{
	struct trace_seq *s = &iter->seq;
	struct trace_entry *ent = iter->ent;
	struct syscall_trace_enter *trace;
	struct syscall_metadata *entry;
	int i, ret, syscall;

	trace_assign_type(trace, ent);

	syscall = trace->nr;

	entry = syscall_nr_to_meta(syscall);
	if (!entry)
		goto end;

	ret = trace_seq_printf(s, "%s(", entry->name);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	for (i = 0; i < entry->nb_args; i++) {
		/* parameter types */
		if (syscalls_flags.val & TRACE_SYSCALLS_OPT_TYPES) {
			ret = trace_seq_printf(s, "%s ", entry->types[i]);
			if (!ret)
				return TRACE_TYPE_PARTIAL_LINE;
		}
		/* parameter values */
		ret = trace_seq_printf(s, "%s: %lx%s ", entry->args[i],
				       trace->args[i],
				       i == entry->nb_args - 1 ? ")" : ",");
		if (!ret)
			return TRACE_TYPE_PARTIAL_LINE;
	}

end:
	trace_seq_printf(s, "\n");
	return TRACE_TYPE_HANDLED;
}

enum print_line_t
print_syscall_exit(struct trace_iterator *iter, int flags)
{
	struct trace_seq *s = &iter->seq;
	struct trace_entry *ent = iter->ent;
	struct syscall_trace_exit *trace;
	int syscall;
	struct syscall_metadata *entry;
	int ret;

	trace_assign_type(trace, ent);

	syscall = trace->nr;

	entry = syscall_nr_to_meta(syscall);
	if (!entry) {
		trace_seq_printf(s, "\n");
		return TRACE_TYPE_HANDLED;
	}

	ret = trace_seq_printf(s, "%s -> 0x%lx\n", entry->name,
				trace->ret);
	if (!ret)
		return TRACE_TYPE_PARTIAL_LINE;

	return TRACE_TYPE_HANDLED;
}

void start_ftrace_syscalls(void)
{
	unsigned long flags;
	struct task_struct *g, *t;

	if (atomic_inc_return(&refcount) != 1)
		goto out;

	arch_init_ftrace_syscalls();
	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, t) {
		set_tsk_thread_flag(t, TIF_SYSCALL_FTRACE);
	} while_each_thread(g, t);

	read_unlock_irqrestore(&tasklist_lock, flags);
out:
	atomic_dec(&refcount);
}

void stop_ftrace_syscalls(void)
{
	unsigned long flags;
	struct task_struct *g, *t;

	if (atomic_dec_return(&refcount))
		goto out;

	read_lock_irqsave(&tasklist_lock, flags);

	do_each_thread(g, t) {
		clear_tsk_thread_flag(t, TIF_SYSCALL_FTRACE);
	} while_each_thread(g, t);

	read_unlock_irqrestore(&tasklist_lock, flags);
out:
	atomic_inc(&refcount);
}

void ftrace_syscall_enter(struct pt_regs *regs)
{
	struct syscall_trace_enter *entry;
	struct syscall_metadata *sys_data;
	struct ring_buffer_event *event;
	int size;
	int syscall_nr;
	int cpu;

	syscall_nr = syscall_get_nr(current, regs);

	cpu = raw_smp_processor_id();

	sys_data = syscall_nr_to_meta(syscall_nr);
	if (!sys_data)
		return;

	size = sizeof(*entry) + sizeof(unsigned long) * sys_data->nb_args;

	event = trace_current_buffer_lock_reserve(TRACE_SYSCALL_ENTER, size,
							0, 0);
	if (!event)
		return;

	entry = ring_buffer_event_data(event);
	entry->nr = syscall_nr;
	syscall_get_arguments(current, regs, 0, sys_data->nb_args, entry->args);

	trace_current_buffer_unlock_commit(event, 0, 0);
	trace_wake_up();
}

void ftrace_syscall_exit(struct pt_regs *regs)
{
	struct syscall_trace_exit *entry;
	struct syscall_metadata *sys_data;
	struct ring_buffer_event *event;
	int syscall_nr;
	int cpu;

	syscall_nr = syscall_get_nr(current, regs);

	cpu = raw_smp_processor_id();

	sys_data = syscall_nr_to_meta(syscall_nr);
	if (!sys_data)
		return;

	event = trace_current_buffer_lock_reserve(TRACE_SYSCALL_EXIT,
				sizeof(*entry), 0, 0);
	if (!event)
		return;

	entry = ring_buffer_event_data(event);
	entry->nr = syscall_nr;
	entry->ret = syscall_get_return_value(current, regs);

	trace_current_buffer_unlock_commit(event, 0, 0);
	trace_wake_up();
}

static int init_syscall_tracer(struct trace_array *tr)
{
	start_ftrace_syscalls();

	return 0;
}

static void reset_syscall_tracer(struct trace_array *tr)
{
	stop_ftrace_syscalls();
}

static struct trace_event syscall_enter_event = {
	.type	 	= TRACE_SYSCALL_ENTER,
	.trace		= print_syscall_enter,
};

static struct trace_event syscall_exit_event = {
	.type	 	= TRACE_SYSCALL_EXIT,
	.trace		= print_syscall_exit,
};

static struct tracer syscall_tracer __read_mostly = {
	.name	     	= "syscall",
	.init		= init_syscall_tracer,
	.reset		= reset_syscall_tracer,
	.flags		= &syscalls_flags,
};

__init int register_ftrace_syscalls(void)
{
	int ret;

	ret = register_ftrace_event(&syscall_enter_event);
	if (!ret) {
		printk(KERN_WARNING "event %d failed to register\n",
		       syscall_enter_event.type);
		WARN_ON_ONCE(1);
	}

	ret = register_ftrace_event(&syscall_exit_event);
	if (!ret) {
		printk(KERN_WARNING "event %d failed to register\n",
		       syscall_exit_event.type);
		WARN_ON_ONCE(1);
	}

	return register_tracer(&syscall_tracer);
}
device_initcall(register_ftrace_syscalls);
