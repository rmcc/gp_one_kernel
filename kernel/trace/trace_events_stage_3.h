/*
 * Stage 3 of the trace events.
 *
 * Override the macros in <trace/trace_event_types.h> to include the following:
 *
 * static void ftrace_event_<call>(proto)
 * {
 * 	event_trace_printk(_RET_IP_, "(<call>) " <fmt>);
 * }
 *
 * static int ftrace_reg_event_<call>(void)
 * {
 * 	int ret;
 *
 * 	ret = register_trace_<call>(ftrace_event_<call>);
 * 	if (!ret)
 * 		pr_info("event trace: Could not activate trace point "
 * 			"probe to  <call>");
 * 	return ret;
 * }
 *
 * static void ftrace_unreg_event_<call>(void)
 * {
 * 	unregister_trace_<call>(ftrace_event_<call>);
 * }
 *
 * For those macros defined with TRACE_FORMAT:
 *
 * static struct ftrace_event_call __used
 * __attribute__((__aligned__(4)))
 * __attribute__((section("_ftrace_events"))) event_<call> = {
 * 	.name 			= "<call>",
 * 	.regfunc		= ftrace_reg_event_<call>,
 * 	.unregfunc		= ftrace_unreg_event_<call>,
 * }
 *
 *
 * For those macros defined with TRACE_EVENT_FORMAT:
 *
 * static struct ftrace_event_call event_<call>;
 *
 * static void ftrace_raw_event_<call>(proto)
 * {
 * 	struct ring_buffer_event *event;
 * 	struct ftrace_raw_<call> *entry; <-- defined in stage 1
 * 	unsigned long irq_flags;
 * 	int pc;
 *
 * 	local_save_flags(irq_flags);
 * 	pc = preempt_count();
 *
 * 	event = trace_current_buffer_lock_reserve(event_<call>.id,
 * 				  sizeof(struct ftrace_raw_<call>),
 * 				  irq_flags, pc);
 * 	if (!event)
 * 		return;
 * 	entry	= ring_buffer_event_data(event);
 *
 * 	<tstruct>;  <-- Here we assign the entries by the TRACE_FIELD.
 *
 * 	trace_current_buffer_unlock_commit(event, irq_flags, pc);
 * }
 *
 * static int ftrace_raw_reg_event_<call>(void)
 * {
 * 	int ret;
 *
 * 	ret = register_trace_<call>(ftrace_raw_event_<call>);
 * 	if (!ret)
 * 		pr_info("event trace: Could not activate trace point "
 * 			"probe to <call>");
 * 	return ret;
 * }
 *
 * static void ftrace_unreg_event_<call>(void)
 * {
 * 	unregister_trace_<call>(ftrace_raw_event_<call>);
 * }
 *
 * static struct trace_event ftrace_event_type_<call> = {
 * 	.trace			= ftrace_raw_output_<call>, <-- stage 2
 * };
 *
 * static int ftrace_raw_init_event_<call>(void)
 * {
 * 	int id;
 *
 * 	id = register_ftrace_event(&ftrace_event_type_<call>);
 * 	if (!id)
 * 		return -ENODEV;
 * 	event_<call>.id = id;
 * 	return 0;
 * }
 *
 * static struct ftrace_event_call __used
 * __attribute__((__aligned__(4)))
 * __attribute__((section("_ftrace_events"))) event_<call> = {
 * 	.name 			= "<call>",
 * 	.regfunc		= ftrace_reg_event_<call>,
 * 	.unregfunc		= ftrace_unreg_event_<call>,
 * 	.raw_init		= ftrace_raw_init_event_<call>,
 * 	.raw_reg		= ftrace_raw_reg_event_<call>,
 * 	.raw_unreg		= ftrace_raw_unreg_event_<call>,
 * }
 *
 */

#undef TPFMT
#define TPFMT(fmt, args...)	fmt "\n", ##args

#define _TRACE_FORMAT(call, proto, args, fmt)				\
static void ftrace_event_##call(proto)					\
{									\
	event_trace_printk(_RET_IP_, "(" #call ") " fmt);		\
}									\
									\
static int ftrace_reg_event_##call(void)				\
{									\
	int ret;							\
									\
	ret = register_trace_##call(ftrace_event_##call);		\
	if (!ret)							\
		pr_info("event trace: Could not activate trace point "	\
			"probe to " #call);				\
	return ret;							\
}									\
									\
static void ftrace_unreg_event_##call(void)				\
{									\
	unregister_trace_##call(ftrace_event_##call);			\
}									\


#undef TRACE_FORMAT
#define TRACE_FORMAT(call, proto, args, fmt)				\
_TRACE_FORMAT(call, PARAMS(proto), PARAMS(args), PARAMS(fmt))		\
static struct ftrace_event_call __used					\
__attribute__((__aligned__(4)))						\
__attribute__((section("_ftrace_events"))) event_##call = {		\
	.name 			= #call,				\
	.system			= STR(TRACE_SYSTEM),			\
	.regfunc		= ftrace_reg_event_##call,		\
	.unregfunc		= ftrace_unreg_event_##call,		\
}

#undef TRACE_FIELD
#define TRACE_FIELD(type, item, assign)\
	entry->item = assign;

#undef TRACE_FIELD
#define TRACE_FIELD(type, item, assign)\
	entry->item = assign;

#undef TPCMD
#define TPCMD(cmd...)	cmd

#undef TRACE_ENTRY
#define TRACE_ENTRY	entry

#undef TRACE_FIELD_SPECIAL
#define TRACE_FIELD_SPECIAL(type_item, item, cmd) \
	cmd;

#undef TRACE_EVENT_FORMAT
#define TRACE_EVENT_FORMAT(call, proto, args, fmt, tstruct, tpfmt)	\
_TRACE_FORMAT(call, PARAMS(proto), PARAMS(args), PARAMS(fmt))		\
									\
static struct ftrace_event_call event_##call;				\
									\
static void ftrace_raw_event_##call(proto)				\
{									\
	struct ring_buffer_event *event;				\
	struct ftrace_raw_##call *entry;				\
	unsigned long irq_flags;					\
	int pc;								\
									\
	local_save_flags(irq_flags);					\
	pc = preempt_count();						\
									\
	event = trace_current_buffer_lock_reserve(event_##call.id,	\
				  sizeof(struct ftrace_raw_##call), 	\
				  irq_flags, pc);			\
	if (!event)							\
		return;							\
	entry	= ring_buffer_event_data(event);			\
									\
	tstruct;							\
									\
	trace_current_buffer_unlock_commit(event, irq_flags, pc);	\
}									\
									\
static int ftrace_raw_reg_event_##call(void)				\
{									\
	int ret;							\
									\
	ret = register_trace_##call(ftrace_raw_event_##call);		\
	if (!ret)							\
		pr_info("event trace: Could not activate trace point "	\
			"probe to " #call);				\
	return ret;							\
}									\
									\
static void ftrace_raw_unreg_event_##call(void)				\
{									\
	unregister_trace_##call(ftrace_raw_event_##call);		\
}									\
									\
static struct trace_event ftrace_event_type_##call = {			\
	.trace			= ftrace_raw_output_##call,		\
};									\
									\
static int ftrace_raw_init_event_##call(void)				\
{									\
	int id;								\
									\
	id = register_ftrace_event(&ftrace_event_type_##call);		\
	if (!id)							\
		return -ENODEV;						\
	event_##call.id = id;						\
	return 0;							\
}									\
									\
static struct ftrace_event_call __used					\
__attribute__((__aligned__(4)))						\
__attribute__((section("_ftrace_events"))) event_##call = {		\
	.name 			= #call,				\
	.system			= STR(TRACE_SYSTEM),			\
	.regfunc		= ftrace_reg_event_##call,		\
	.unregfunc		= ftrace_unreg_event_##call,		\
	.raw_init		= ftrace_raw_init_event_##call,		\
	.raw_reg		= ftrace_raw_reg_event_##call,		\
	.raw_unreg		= ftrace_raw_unreg_event_##call,	\
}
