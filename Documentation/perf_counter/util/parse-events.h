
/*
 * Parse symbolic events/counts passed in as options:
 */

extern int			nr_counters;

extern struct perf_counter_attr attrs[MAX_COUNTERS];

extern char *event_name(int ctr);

extern int parse_events(const struct option *opt, const char *str, int unset);

#define EVENTS_HELP_MAX (128*1024)

extern void create_events_help(char *help_msg);

