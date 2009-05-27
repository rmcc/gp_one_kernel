#include "util/util.h"
#include "builtin.h"

#include <libelf.h>
#include <gelf.h>
#include <elf.h>

#include "util/list.h"
#include "util/cache.h"
#include "util/rbtree.h"

#include "perf.h"

#include "util/parse-options.h"
#include "util/parse-events.h"

#define SHOW_KERNEL	1
#define SHOW_USER	2
#define SHOW_HV		4

static char		const *input_name = "perf.data";
static char		*vmlinux = NULL;
static int		input;
static int		show_mask = SHOW_KERNEL | SHOW_USER | SHOW_HV;

static int		dump_trace = 0;
static int		verbose;

static unsigned long	page_size;
static unsigned long	mmap_window = 32;

const char *perf_event_names[] = {
	[PERF_EVENT_MMAP]   = " PERF_EVENT_MMAP",
	[PERF_EVENT_MUNMAP] = " PERF_EVENT_MUNMAP",
	[PERF_EVENT_COMM]   = " PERF_EVENT_COMM",
};

struct ip_event {
	struct perf_event_header header;
	__u64 ip;
	__u32 pid, tid;
};
struct mmap_event {
	struct perf_event_header header;
	__u32 pid, tid;
	__u64 start;
	__u64 len;
	__u64 pgoff;
	char filename[PATH_MAX];
};
struct comm_event {
	struct perf_event_header header;
	__u32 pid,tid;
	char comm[16];
};

typedef union event_union {
	struct perf_event_header header;
	struct ip_event ip;
	struct mmap_event mmap;
	struct comm_event comm;
} event_t;

struct symbol {
	struct rb_node		rb_node;
	__u64			start;
	__u64			end;
	char			name[0];
};

static struct symbol *symbol__new(uint64_t start, uint64_t len, const char *name)
{
	struct symbol *self = malloc(sizeof(*self) + strlen(name) + 1);

	if (self != NULL) {
		self->start = start;
		self->end   = start + len;
		strcpy(self->name, name);
	}

	return self;
}

static void symbol__delete(struct symbol *self)
{
	free(self);
}

static size_t symbol__fprintf(struct symbol *self, FILE *fp)
{
	return fprintf(fp, " %llx-%llx %s\n",
		       self->start, self->end, self->name);
}

struct dso {
	struct list_head node;
	struct rb_root	 syms;
	char		 name[0];
};

static struct dso *dso__new(const char *name)
{
	struct dso *self = malloc(sizeof(*self) + strlen(name) + 1);

	if (self != NULL) {
		strcpy(self->name, name);
		self->syms = RB_ROOT;
	}

	return self;
}

static void dso__delete_symbols(struct dso *self)
{
	struct symbol *pos;
	struct rb_node *next = rb_first(&self->syms);

	while (next) {
		pos = rb_entry(next, struct symbol, rb_node);
		next = rb_next(&pos->rb_node);
		symbol__delete(pos);
	}
}

static void dso__delete(struct dso *self)
{
	dso__delete_symbols(self);
	free(self);
}

static void dso__insert_symbol(struct dso *self, struct symbol *sym)
{
	struct rb_node **p = &self->syms.rb_node;
	struct rb_node *parent = NULL;
	const uint64_t ip = sym->start;
	struct symbol *s;

	while (*p != NULL) {
		parent = *p;
		s = rb_entry(parent, struct symbol, rb_node);
		if (ip < s->start)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}
	rb_link_node(&sym->rb_node, parent, p);
	rb_insert_color(&sym->rb_node, &self->syms);
}

static struct symbol *dso__find_symbol(struct dso *self, uint64_t ip)
{
	struct rb_node *n;

	if (self == NULL)
		return NULL;

	n = self->syms.rb_node;

	while (n) {
		struct symbol *s = rb_entry(n, struct symbol, rb_node);

		if (ip < s->start)
			n = n->rb_left;
		else if (ip > s->end)
			n = n->rb_right;
		else
			return s;
	}

	return NULL;
}

/**
 * elf_symtab__for_each_symbol - iterate thru all the symbols
 *
 * @self: struct elf_symtab instance to iterate
 * @index: uint32_t index
 * @sym: GElf_Sym iterator
 */
#define elf_symtab__for_each_symbol(syms, nr_syms, index, sym) \
	for (index = 0, gelf_getsym(syms, index, &sym);\
	     index < nr_syms; \
	     index++, gelf_getsym(syms, index, &sym))

static inline uint8_t elf_sym__type(const GElf_Sym *sym)
{
	return GELF_ST_TYPE(sym->st_info);
}

static inline int elf_sym__is_function(const GElf_Sym *sym)
{
	return elf_sym__type(sym) == STT_FUNC &&
	       sym->st_name != 0 &&
	       sym->st_shndx != SHN_UNDEF &&
	       sym->st_size != 0;
}

static inline const char *elf_sym__name(const GElf_Sym *sym,
					const Elf_Data *symstrs)
{
	return symstrs->d_buf + sym->st_name;
}

static Elf_Scn *elf_section_by_name(Elf *elf, GElf_Ehdr *ep,
				    GElf_Shdr *shp, const char *name,
				    size_t *index)
{
	Elf_Scn *sec = NULL;
	size_t cnt = 1;

	while ((sec = elf_nextscn(elf, sec)) != NULL) {
		char *str;

		gelf_getshdr(sec, shp);
		str = elf_strptr(elf, ep->e_shstrndx, shp->sh_name);
		if (!strcmp(name, str)) {
			if (index)
				*index = cnt;
			break;
		}
		++cnt;
	}

	return sec;
}

static int dso__load_sym(struct dso *self, int fd, char *name)
{
	Elf_Data *symstrs;
	uint32_t nr_syms;
	int err = -1;
	uint32_t index;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr;
	Elf_Data *syms;
	GElf_Sym sym;
	Elf_Scn *sec;
	Elf *elf;
	int nr = 0;

	elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
	if (elf == NULL) {
		fprintf(stderr, "%s: cannot read %s ELF file.\n",
			__func__, name);
		goto out_close;
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "%s: cannot get elf header.\n", __func__);
		goto out_elf_end;
	}

	sec = elf_section_by_name(elf, &ehdr, &shdr, ".symtab", NULL);
	if (sec == NULL)
		sec = elf_section_by_name(elf, &ehdr, &shdr, ".dynsym", NULL);

	if (sec == NULL)
		goto out_elf_end;

	syms = elf_getdata(sec, NULL);
	if (syms == NULL)
		goto out_elf_end;

	sec = elf_getscn(elf, shdr.sh_link);
	if (sec == NULL)
		goto out_elf_end;

	symstrs = elf_getdata(sec, NULL);
	if (symstrs == NULL)
		goto out_elf_end;

	nr_syms = shdr.sh_size / shdr.sh_entsize;

	elf_symtab__for_each_symbol(syms, nr_syms, index, sym) {
		struct symbol *f;

		if (!elf_sym__is_function(&sym))
			continue;

		sec = elf_getscn(elf, sym.st_shndx);
		if (!sec)
			goto out_elf_end;

		gelf_getshdr(sec, &shdr);
		sym.st_value -= shdr.sh_addr - shdr.sh_offset;

		f = symbol__new(sym.st_value, sym.st_size,
				elf_sym__name(&sym, symstrs));
		if (!f)
			goto out_elf_end;

		dso__insert_symbol(self, f);

		nr++;
	}

	err = nr;
out_elf_end:
	elf_end(elf);
out_close:
	return err;
}

static int dso__load(struct dso *self)
{
	int size = strlen(self->name) + sizeof("/usr/lib/debug%s.debug");
	char *name = malloc(size);
	int variant = 0;
	int ret = -1;
	int fd;

	if (!name)
		return -1;

more:
	do {
		switch (variant) {
		case 0: /* Fedora */
			snprintf(name, size, "/usr/lib/debug%s.debug", self->name);
			break;
		case 1: /* Ubuntu */
			snprintf(name, size, "/usr/lib/debug%s", self->name);
			break;
		case 2: /* Sane people */
			snprintf(name, size, "%s", self->name);
			break;

		default:
			goto out;
		}
		variant++;

		fd = open(name, O_RDONLY);
	} while (fd < 0);

	ret = dso__load_sym(self, fd, name);
	close(fd);

	/*
	 * Some people seem to have debuginfo files _WITHOUT_ debug info!?!?
	 */
	if (!ret)
		goto more;

out:
	free(name);
	return ret;
}

static size_t dso__fprintf(struct dso *self, FILE *fp)
{
	size_t ret = fprintf(fp, "dso: %s\n", self->name);

	struct rb_node *nd;
	for (nd = rb_first(&self->syms); nd; nd = rb_next(nd)) {
		struct symbol *pos = rb_entry(nd, struct symbol, rb_node);
		ret += symbol__fprintf(pos, fp);
	}

	return ret;
}

static LIST_HEAD(dsos);
static struct dso *kernel_dso;

static void dsos__add(struct dso *dso)
{
	list_add_tail(&dso->node, &dsos);
}

static struct dso *dsos__find(const char *name)
{
	struct dso *pos;

	list_for_each_entry(pos, &dsos, node)
		if (strcmp(pos->name, name) == 0)
			return pos;
	return NULL;
}

static struct dso *dsos__findnew(const char *name)
{
	struct dso *dso = dsos__find(name);
	int nr;

	if (dso == NULL) {
		dso = dso__new(name);
		if (!dso)
			goto out_delete_dso;

		nr = dso__load(dso);
		if (nr < 0) {
			fprintf(stderr, "Failed to open: %s\n", name);
			goto out_delete_dso;
		}
		if (!nr) {
			fprintf(stderr,
		"Failed to find debug symbols for: %s, maybe install a debug package?\n",
					name);
		}

		dsos__add(dso);
	}

	return dso;

out_delete_dso:
	dso__delete(dso);
	return NULL;
}

static void dsos__fprintf(FILE *fp)
{
	struct dso *pos;

	list_for_each_entry(pos, &dsos, node)
		dso__fprintf(pos, fp);
}

static int hex(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	if ((ch >= 'A') && (ch <= 'F'))
		return ch - 'A' + 10;
	return -1;
}

/*
 * While we find nice hex chars, build a long_val.
 * Return number of chars processed.
 */
static int hex2long(char *ptr, unsigned long *long_val)
{
	const char *p = ptr;
	*long_val = 0;

	while (*p) {
		const int hex_val = hex(*p);

		if (hex_val < 0)
			break;

		*long_val = (*long_val << 4) | hex_val;
		p++;
	}

	return p - ptr;
}

static int load_kallsyms(void)
{
	struct rb_node *nd, *prevnd;
	char *line = NULL;
	FILE *file;
	size_t n;

	kernel_dso = dso__new("[kernel]");
	if (kernel_dso == NULL)
		return -1;

	file = fopen("/proc/kallsyms", "r");
	if (file == NULL)
		goto out_delete_dso;

	while (!feof(file)) {
		unsigned long start;
		struct symbol *sym;
		int line_len, len;
		char symbol_type;

		line_len = getline(&line, &n, file);
		if (line_len < 0)
			break;

		if (!line)
			goto out_delete_dso;

		line[--line_len] = '\0'; /* \n */

		len = hex2long(line, &start);

		len++;
		if (len + 2 >= line_len)
			continue;

		symbol_type = toupper(line[len]);
		/*
		 * We're interested only in code ('T'ext)
		 */
		if (symbol_type != 'T' && symbol_type != 'W')
			continue;
		/*
		 * Well fix up the end later, when we have all sorted.
		 */
		sym = symbol__new(start, 0xdead, line + len + 2);

		if (sym == NULL)
			goto out_delete_dso;

		dso__insert_symbol(kernel_dso, sym);
	}

	/*
	 * Now that we have all sorted out, just set the ->end of all
	 * symbols
	 */
	prevnd = rb_first(&kernel_dso->syms);

	if (prevnd == NULL)
		goto out_delete_line;

	for (nd = rb_next(prevnd); nd; nd = rb_next(nd)) {
		struct symbol *prev = rb_entry(prevnd, struct symbol, rb_node),
			      *curr = rb_entry(nd, struct symbol, rb_node);

		prev->end = curr->start - 1;
		prevnd = nd;
	}

	dsos__add(kernel_dso);
	free(line);
	fclose(file);

	return 0;

out_delete_line:
	free(line);
out_delete_dso:
	dso__delete(kernel_dso);
	return -1;
}

static int load_kernel(void)
{
	int fd, nr;

	if (!vmlinux)
		goto kallsyms;

	fd = open(vmlinux, O_RDONLY);
	if (fd < 0)
		goto kallsyms;

	kernel_dso = dso__new("[kernel]");
	if (!kernel_dso)
		goto fail_open;

	nr = dso__load_sym(kernel_dso, fd, vmlinux);

	if (nr <= 0)
		goto fail_load;

	dsos__add(kernel_dso);
	close(fd);

	return 0;

fail_load:
	dso__delete(kernel_dso);
fail_open:
	close(fd);
kallsyms:
	return load_kallsyms();
}

struct map {
	struct list_head node;
	uint64_t	 start;
	uint64_t	 end;
	uint64_t	 pgoff;
	struct dso	 *dso;
};

static struct map *map__new(struct mmap_event *event)
{
	struct map *self = malloc(sizeof(*self));

	if (self != NULL) {
		self->start = event->start;
		self->end   = event->start + event->len;
		self->pgoff = event->pgoff;

		self->dso = dsos__findnew(event->filename);
		if (self->dso == NULL)
			goto out_delete;
	}
	return self;
out_delete:
	free(self);
	return NULL;
}

struct thread;

static const char *thread__name(struct thread *self, char *bf, size_t size);

struct thread {
	struct rb_node	 rb_node;
	struct list_head maps;
	pid_t		 pid;
	char		 *comm;
};

static const char *thread__name(struct thread *self, char *bf, size_t size)
{
	if (self->comm)
		return self->comm;

	snprintf(bf, sizeof(bf), ":%u", self->pid);
	return bf;
}

static struct thread *thread__new(pid_t pid)
{
	struct thread *self = malloc(sizeof(*self));

	if (self != NULL) {
		self->pid = pid;
		self->comm = NULL;
		INIT_LIST_HEAD(&self->maps);
	}

	return self;
}

static int thread__set_comm(struct thread *self, const char *comm)
{
	self->comm = strdup(comm);
	return self->comm ? 0 : -ENOMEM;
}

static struct rb_root threads;

static struct thread *threads__findnew(pid_t pid)
{
	struct rb_node **p = &threads.rb_node;
	struct rb_node *parent = NULL;
	struct thread *th;

	while (*p != NULL) {
		parent = *p;
		th = rb_entry(parent, struct thread, rb_node);

		if (th->pid == pid)
			return th;

		if (pid < th->pid)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	th = thread__new(pid);
	if (th != NULL) {
		rb_link_node(&th->rb_node, parent, p);
		rb_insert_color(&th->rb_node, &threads);
	}
	return th;
}

static void thread__insert_map(struct thread *self, struct map *map)
{
	list_add_tail(&map->node, &self->maps);
}

static struct map *thread__find_map(struct thread *self, uint64_t ip)
{
	struct map *pos;

	if (self == NULL)
		return NULL;

	list_for_each_entry(pos, &self->maps, node)
		if (ip >= pos->start && ip <= pos->end)
			return pos;

	return NULL;
}

/*
 * histogram, sorted on item, collects counts
 */

static struct rb_root hist;

struct hist_entry {
	struct rb_node	 rb_node;

	struct thread	 *thread;
	struct map	 *map;
	struct dso	 *dso;
	struct symbol	 *sym;
	uint64_t	 ip;
	char		 level;

	uint32_t	 count;
};

/*
 * configurable sorting bits
 */

struct sort_entry {
	struct list_head list;

	int64_t (*cmp)(struct hist_entry *, struct hist_entry *);
	size_t	(*print)(FILE *fp, struct hist_entry *);
};

static int64_t
sort__thread_cmp(struct hist_entry *left, struct hist_entry *right)
{
	return right->thread->pid - left->thread->pid;
}

static size_t
sort__thread_print(FILE *fp, struct hist_entry *self)
{
	char bf[32];

	return fprintf(fp, "%14s ",
			thread__name(self->thread, bf, sizeof(bf)));
}

static struct sort_entry sort_thread = {
	.cmp	= sort__thread_cmp,
	.print	= sort__thread_print,
};

static int64_t
sort__sym_cmp(struct hist_entry *left, struct hist_entry *right)
{
	uint64_t ip_l, ip_r;

	if (left->sym == right->sym)
		return 0;

	ip_l = left->sym ? left->sym->start : left->ip;
	ip_r = right->sym ? right->sym->start : right->ip;

	return (int64_t)(ip_r - ip_l);
}

static size_t
sort__sym_print(FILE *fp, struct hist_entry *self)
{
	size_t ret = 0;

	ret += fprintf(fp, "[%c] ", self->level);

	if (verbose)
		ret += fprintf(fp, "%#018llx ", (unsigned long long)self->ip);

	if (self->level != '.')
		ret += fprintf(fp, "%s ",
			       self->sym ? self->sym->name : "<unknown>");
	else
		ret += fprintf(fp, "%s: %s ",
			       self->dso ? self->dso->name : "<unknown>",
			       self->sym ? self->sym->name : "<unknown>");

	return ret;
}

static struct sort_entry sort_sym = {
	.cmp	= sort__sym_cmp,
	.print	= sort__sym_print,
};

static LIST_HEAD(hist_entry__sort_list);

static void setup_sorting(void)
{
	list_add_tail(&sort_thread.list, &hist_entry__sort_list);
	list_add_tail(&sort_sym.list, &hist_entry__sort_list);
}

static int64_t
hist_entry__cmp(struct hist_entry *left, struct hist_entry *right)
{
	struct sort_entry *se;
	int64_t cmp = 0;

	list_for_each_entry(se, &hist_entry__sort_list, list) {
		cmp = se->cmp(left, right);
		if (cmp)
			break;
	}

	return cmp;
}

static size_t
hist_entry__fprintf(FILE *fp, struct hist_entry *self, uint64_t total_samples)
{
	struct sort_entry *se;
	size_t ret;

	if (total_samples) {
		ret = fprintf(fp, "%5.2f%% ",
				(self->count * 100.0) / total_samples);
	} else
		ret = fprintf(fp, "%12d ", self->count);

	list_for_each_entry(se, &hist_entry__sort_list, list)
		ret += se->print(fp, self);

	ret += fprintf(fp, "\n");

	return ret;
}

/*
 * collect histogram counts
 */

static int
hist_entry__add(struct thread *thread, struct map *map, struct dso *dso,
		struct symbol *sym, uint64_t ip, char level)
{
	struct rb_node **p = &hist.rb_node;
	struct rb_node *parent = NULL;
	struct hist_entry *he;
	struct hist_entry entry = {
		.thread	= thread,
		.map	= map,
		.dso	= dso,
		.sym	= sym,
		.ip	= ip,
		.level	= level,
		.count	= 1,
	};
	int cmp;

	while (*p != NULL) {
		parent = *p;
		he = rb_entry(parent, struct hist_entry, rb_node);

		cmp = hist_entry__cmp(&entry, he);

		if (!cmp) {
			he->count++;
			return 0;
		}

		if (cmp < 0)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	he = malloc(sizeof(*he));
	if (!he)
		return -ENOMEM;
	*he = entry;
	rb_link_node(&he->rb_node, parent, p);
	rb_insert_color(&he->rb_node, &hist);

	return 0;
}

/*
 * reverse the map, sort on count.
 */

static struct rb_root output_hists;

static void output__insert_entry(struct hist_entry *he)
{
	struct rb_node **p = &output_hists.rb_node;
	struct rb_node *parent = NULL;
	struct hist_entry *iter;

	while (*p != NULL) {
		parent = *p;
		iter = rb_entry(parent, struct hist_entry, rb_node);

		if (he->count > iter->count)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&he->rb_node, parent, p);
	rb_insert_color(&he->rb_node, &output_hists);
}

static void output__resort(void)
{
	struct rb_node *next = rb_first(&hist);
	struct hist_entry *n;

	while (next) {
		n = rb_entry(next, struct hist_entry, rb_node);
		next = rb_next(&n->rb_node);

		rb_erase(&n->rb_node, &hist);
		output__insert_entry(n);
	}
}

static size_t output__fprintf(FILE *fp, uint64_t total_samples)
{
	struct hist_entry *pos;
	struct rb_node *nd;
	size_t ret = 0;

	for (nd = rb_first(&output_hists); nd; nd = rb_next(nd)) {
		pos = rb_entry(nd, struct hist_entry, rb_node);
		ret += hist_entry__fprintf(fp, pos, total_samples);
	}

	return ret;
}


static int __cmd_report(void)
{
	unsigned long offset = 0;
	unsigned long head = 0;
	struct stat stat;
	char *buf;
	event_t *event;
	int ret, rc = EXIT_FAILURE;
	uint32_t size;
	unsigned long total = 0, total_mmap = 0, total_comm = 0, total_unknown = 0;

	input = open(input_name, O_RDONLY);
	if (input < 0) {
		perror("failed to open file");
		exit(-1);
	}

	ret = fstat(input, &stat);
	if (ret < 0) {
		perror("failed to stat file");
		exit(-1);
	}

	if (!stat.st_size) {
		fprintf(stderr, "zero-sized file, nothing to do!\n");
		exit(0);
	}

	if (load_kernel() < 0) {
		perror("failed to open kallsyms");
		return EXIT_FAILURE;
	}

remap:
	buf = (char *)mmap(NULL, page_size * mmap_window, PROT_READ,
			   MAP_SHARED, input, offset);
	if (buf == MAP_FAILED) {
		perror("failed to mmap file");
		exit(-1);
	}

more:
	event = (event_t *)(buf + head);

	size = event->header.size;
	if (!size)
		size = 8;

	if (head + event->header.size >= page_size * mmap_window) {
		unsigned long shift = page_size * (head / page_size);
		int ret;

		ret = munmap(buf, page_size * mmap_window);
		assert(ret == 0);

		offset += shift;
		head -= shift;
		goto remap;
	}

	size = event->header.size;
	if (!size)
		goto broken_event;

	if (event->header.misc & PERF_EVENT_MISC_OVERFLOW) {
		char level;
		int show = 0;
		struct dso *dso = NULL;
		struct thread *thread = threads__findnew(event->ip.pid);
		uint64_t ip = event->ip.ip;
		struct map *map = NULL;

		if (dump_trace) {
			fprintf(stderr, "%p [%p]: PERF_EVENT (IP, %d): %d: %p\n",
				(void *)(offset + head),
				(void *)(long)(event->header.size),
				event->header.misc,
				event->ip.pid,
				(void *)(long)ip);
		}

		if (thread == NULL) {
			fprintf(stderr, "problem processing %d event, bailing out\n",
				event->header.type);
			goto done;
		}

		if (event->header.misc & PERF_EVENT_MISC_KERNEL) {
			show = SHOW_KERNEL;
			level = 'k';

			dso = kernel_dso;

		} else if (event->header.misc & PERF_EVENT_MISC_USER) {

			show = SHOW_USER;
			level = '.';

			map = thread__find_map(thread, ip);
			if (map != NULL) {
				dso = map->dso;
				ip -= map->start + map->pgoff;
			}

		} else {
			show = SHOW_HV;
			level = 'H';
		}

		if (show & show_mask) {
			struct symbol *sym = dso__find_symbol(dso, ip);

			if (hist_entry__add(thread, map, dso, sym, ip, level)) {
				fprintf(stderr,
		"problem incrementing symbol count, bailing out\n");
				goto done;
			}
		}
		total++;
	} else switch (event->header.type) {
	case PERF_EVENT_MMAP: {
		struct thread *thread = threads__findnew(event->mmap.pid);
		struct map *map = map__new(&event->mmap);

		if (dump_trace) {
			fprintf(stderr, "%p [%p]: PERF_EVENT_MMAP: [%p(%p) @ %p]: %s\n",
				(void *)(offset + head),
				(void *)(long)(event->header.size),
				(void *)(long)event->mmap.start,
				(void *)(long)event->mmap.len,
				(void *)(long)event->mmap.pgoff,
				event->mmap.filename);
		}
		if (thread == NULL || map == NULL) {
			fprintf(stderr, "problem processing PERF_EVENT_MMAP, bailing out\n");
			goto done;
		}
		thread__insert_map(thread, map);
		total_mmap++;
		break;
	}
	case PERF_EVENT_COMM: {
		struct thread *thread = threads__findnew(event->comm.pid);

		if (dump_trace) {
			fprintf(stderr, "%p [%p]: PERF_EVENT_COMM: %s:%d\n",
				(void *)(offset + head),
				(void *)(long)(event->header.size),
				event->comm.comm, event->comm.pid);
		}
		if (thread == NULL ||
		    thread__set_comm(thread, event->comm.comm)) {
			fprintf(stderr, "problem processing PERF_EVENT_COMM, bailing out\n");
			goto done;
		}
		total_comm++;
		break;
	}
	default: {
broken_event:
		if (dump_trace)
			fprintf(stderr, "%p [%p]: skipping unknown header type: %d\n",
					(void *)(offset + head),
					(void *)(long)(event->header.size),
					event->header.type);

		total_unknown++;

		/*
		 * assume we lost track of the stream, check alignment, and
		 * increment a single u64 in the hope to catch on again 'soon'.
		 */

		if (unlikely(head & 7))
			head &= ~7ULL;

		size = 8;
	}
	}

	head += size;

	if (offset + head < stat.st_size)
		goto more;

	rc = EXIT_SUCCESS;
done:
	close(input);

	if (dump_trace) {
		fprintf(stderr, "      IP events: %10ld\n", total);
		fprintf(stderr, "    mmap events: %10ld\n", total_mmap);
		fprintf(stderr, "    comm events: %10ld\n", total_comm);
		fprintf(stderr, " unknown events: %10ld\n", total_unknown);

		return 0;
	}

	if (verbose >= 2)
		dsos__fprintf(stdout);

	output__resort();
	output__fprintf(stdout, total);

	return rc;
}

static const char * const report_usage[] = {
	"perf report [<options>] <command>",
	NULL
};

static const struct option options[] = {
	OPT_STRING('i', "input", &input_name, "file",
		    "input file name"),
	OPT_BOOLEAN('v', "verbose", &verbose,
		    "be more verbose (show symbol address, etc)"),
	OPT_BOOLEAN('D', "dump-raw-trace", &dump_trace,
		    "dump raw trace in ASCII"),
	OPT_STRING('k', "vmlinux", &vmlinux, "file", "vmlinux pathname"),
	OPT_END()
};

int cmd_report(int argc, const char **argv, const char *prefix)
{
	elf_version(EV_CURRENT);

	page_size = getpagesize();

	parse_options(argc, argv, options, report_usage, 0);

	setup_sorting();

	setup_pager();

	return __cmd_report();
}
