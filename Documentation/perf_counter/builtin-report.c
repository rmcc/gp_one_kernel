#include "util/util.h"

#include <libelf.h>
#include <gelf.h>
#include <elf.h>

#include "util/list.h"
#include "util/rbtree.h"

#include "perf.h"

#include "util/parse-options.h"
#include "util/parse-events.h"

#define SHOW_KERNEL	1
#define SHOW_USER	2
#define SHOW_HV		4

static char		const *input_name = "output.perf";
static int		input;
static int		show_mask = SHOW_KERNEL | SHOW_USER | SHOW_HV;

static int		dump_trace = 0;

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
	struct rb_node rb_node;
	uint64_t       start;
	uint64_t       end;
	char	       name[0];
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
	return fprintf(fp, " %lx-%lx %s\n",
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
	if (self == NULL)
		return NULL;

	struct rb_node *n = self->syms.rb_node;

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
	       sym->st_shndx != SHN_UNDEF;
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

static int dso__load(struct dso *self)
{
	int fd = open(self->name, O_RDONLY), err = -1;

	if (fd == -1)
		return -1;

	Elf *elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
	if (elf == NULL) {
		fprintf(stderr, "%s: cannot read %s ELF file.\n",
			__func__, self->name);
		goto out_close;
	}

	GElf_Ehdr ehdr;
	if (gelf_getehdr(elf, &ehdr) == NULL) {
		fprintf(stderr, "%s: cannot get elf header.\n", __func__);
		goto out_elf_end;
	}

	GElf_Shdr shdr;
	Elf_Scn *sec = elf_section_by_name(elf, &ehdr, &shdr, ".symtab", NULL);
	if (sec == NULL)
		sec = elf_section_by_name(elf, &ehdr, &shdr, ".dynsym", NULL);

	if (sec == NULL)
		goto out_elf_end;

	Elf_Data *syms = elf_getdata(sec, NULL);
	if (syms == NULL)
		goto out_elf_end;

	sec = elf_getscn(elf, shdr.sh_link);
	if (sec == NULL)
		goto out_elf_end;

	Elf_Data *symstrs = elf_getdata(sec, NULL);
	if (symstrs == NULL)
		goto out_elf_end;

	const uint32_t nr_syms = shdr.sh_size / shdr.sh_entsize;

	GElf_Sym sym;
	uint32_t index;
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
	}

	err = 0;
out_elf_end:
	elf_end(elf);
out_close:
	close(fd);
	return err;
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

	if (dso == NULL) {
		dso = dso__new(name);
		if (dso != NULL && dso__load(dso) < 0)
			goto out_delete_dso;

		dsos__add(dso);
	}

	return dso;

out_delete_dso:
	dso__delete(dso);
	return NULL;
}

void dsos__fprintf(FILE *fp)
{
	struct dso *pos;

	list_for_each_entry(pos, &dsos, node)
		dso__fprintf(pos, fp);
}

static int load_kallsyms(void)
{
	kernel_dso = dso__new("[kernel]");
	if (kernel_dso == NULL)
		return -1;

	FILE *file = fopen("/proc/kallsyms", "r");

	if (file == NULL)
		goto out_delete_dso;

	char *line = NULL;
	size_t n;

	while (!feof(file)) {
		unsigned long long start;
		char c, symbf[4096];

		if (getline(&line, &n, file) < 0)
			break;

		if (!line)
			goto out_delete_dso;

		if (sscanf(line, "%llx %c %s", &start, &c, symbf) == 3) {
			/*
			 * Well fix up the end later, when we have all sorted.
			 */
			struct symbol *sym = symbol__new(start, 0xdead, symbf);

			if (sym == NULL)
				goto out_delete_dso;

			dso__insert_symbol(kernel_dso, sym);
		}
	}

	/*
	 * Now that we have all sorted out, just set the ->end of all
	 * symbols
	 */
	struct rb_node *nd, *prevnd = rb_first(&kernel_dso->syms);

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

static size_t map__fprintf(struct map *self, FILE *fp)
{
	return fprintf(fp, " %lx-%lx %lx %s\n",
		       self->start, self->end, self->pgoff, self->dso->name);
}

struct thread;

static const char *thread__name(struct thread *self, char *bf, size_t size);

struct symhist {
	struct rb_node	 rb_node;
	struct dso	 *dso;
	struct symbol	 *sym;
	struct thread	 *thread;
	uint64_t	 ip;
	uint32_t	 count;
	char		 level;
};

static struct symhist *symhist__new(struct symbol *sym, uint64_t ip,
				    struct thread *thread, struct dso *dso,
				    char level)
{
	struct symhist *self = malloc(sizeof(*self));

	if (self != NULL) {
		self->sym    = sym;
		self->thread = thread;
		self->ip     = ip;
		self->dso    = dso;
		self->level  = level;
		self->count  = 1;
	}

	return self;
}

void symhist__delete(struct symhist *self)
{
	free(self);
}

static void symhist__inc(struct symhist *self)
{
	++self->count;
}

static size_t
symhist__fprintf(struct symhist *self, uint64_t total_samples, FILE *fp)
{
	char bf[32];
	size_t ret;

	if (total_samples)
		ret = fprintf(fp, "%5.2f", (self->count * 100.0) / total_samples);
	else
		ret = fprintf(fp, "%12d", self->count);

	ret += fprintf(fp, "%14s [%c] %#018llx ",
		       thread__name(self->thread, bf, sizeof(bf)),
		       self->level, (unsigned long long)self->ip);

	if (self->level != '.')
		ret += fprintf(fp, "%s\n",
			       self->sym ? self->sym->name : "<unknown>");
	else
		ret += fprintf(fp, "%s: %s\n",
			       self->dso ? self->dso->name : "<unknown>",
			       self->sym ? self->sym->name : "<unknown>");
	return ret;
}

struct thread {
	struct rb_node	 rb_node;
	struct list_head maps;
	struct rb_root	 symhists;
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
		self->symhists = RB_ROOT;
	}

	return self;
}

static int thread__symbol_incnew(struct thread *self, struct symbol *sym,
				 uint64_t ip, struct dso *dso, char level)
{
	struct rb_node **p = &self->symhists.rb_node;
	struct rb_node *parent = NULL;
	struct symhist *sh;

	while (*p != NULL) {
		parent = *p;
		sh = rb_entry(parent, struct symhist, rb_node);

		if (sh->sym == sym || ip == sh->ip) {
			symhist__inc(sh);
			return 0;
		}

		/* Handle unresolved symbols too */
		const uint64_t start = !sh->sym ? sh->ip : sh->sym->start;

		if (ip < start)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	sh = symhist__new(sym, ip, self, dso, level);
	if (sh == NULL)
		return -ENOMEM;
	rb_link_node(&sh->rb_node, parent, p);
	rb_insert_color(&sh->rb_node, &self->symhists);
	return 0;
}

static int thread__set_comm(struct thread *self, const char *comm)
{
	self->comm = strdup(comm);
	return self->comm ? 0 : -ENOMEM;
}

size_t thread__maps_fprintf(struct thread *self, FILE *fp)
{
	struct map *pos;
	size_t ret = 0;

	list_for_each_entry(pos, &self->maps, node)
		ret += map__fprintf(pos, fp);

	return ret;
}

static size_t thread__fprintf(struct thread *self, FILE *fp)
{
	int ret = fprintf(fp, "thread: %d %s\n", self->pid, self->comm);
	struct rb_node *nd;

	for (nd = rb_first(&self->symhists); nd; nd = rb_next(nd)) {
		struct symhist *pos = rb_entry(nd, struct symhist, rb_node);
		ret += symhist__fprintf(pos, 0, fp);
	}

	return ret;
}

static struct rb_root threads = RB_ROOT;

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
	if (self == NULL)
		return NULL;

	struct map *pos;

	list_for_each_entry(pos, &self->maps, node)
		if (ip >= pos->start && ip <= pos->end)
			return pos;

	return NULL;
}

void threads__fprintf(FILE *fp)
{
	struct rb_node *nd;
	for (nd = rb_first(&threads); nd; nd = rb_next(nd)) {
		struct thread *pos = rb_entry(nd, struct thread, rb_node);
		thread__fprintf(pos, fp);
	}
}

static struct rb_root global_symhists = RB_ROOT;

static void threads__insert_symhist(struct symhist *sh)
{
	struct rb_node **p = &global_symhists.rb_node;
	struct rb_node *parent = NULL;
	struct symhist *iter;

	while (*p != NULL) {
		parent = *p;
		iter = rb_entry(parent, struct symhist, rb_node);

		/* Reverse order */
		if (sh->count > iter->count)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&sh->rb_node, parent, p);
	rb_insert_color(&sh->rb_node, &global_symhists);
}

static void threads__sort_symhists(void)
{
	struct rb_node *nd;

	for (nd = rb_first(&threads); nd; nd = rb_next(nd)) {
		struct thread *thread = rb_entry(nd, struct thread, rb_node);
		struct rb_node *next = rb_first(&thread->symhists);

		while (next) {
			struct symhist *n = rb_entry(next, struct symhist,
						     rb_node);
			next = rb_next(&n->rb_node);
			rb_erase(&n->rb_node, &thread->symhists);
			threads__insert_symhist(n);
		}

	}
}

static size_t threads__symhists_fprintf(uint64_t total_samples, FILE *fp)
{
	struct rb_node *nd;
	size_t ret = 0;

	for (nd = rb_first(&global_symhists); nd; nd = rb_next(nd)) {
		struct symhist *pos = rb_entry(nd, struct symhist, rb_node);
		ret += symhist__fprintf(pos, total_samples, fp);
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

	if (load_kallsyms() < 0) {
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

		if (dump_trace) {
			fprintf(stderr, "%p [%p]: PERF_EVENT (IP, %d): %d: %p\n",
				(void *)(offset + head),
				(void *)(long)(event->header.size),
				event->header.misc,
				event->ip.pid,
				(void *)event->ip.ip);
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
			struct map *map = thread__find_map(thread, ip);
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

			if (thread__symbol_incnew(thread, sym, ip, dso, level)) {
				fprintf(stderr, "problem incrementing symbol count, bailing out\n");
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
				(void *)event->mmap.start,
				(void *)event->mmap.len,
				(void *)event->mmap.pgoff,
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

	threads__sort_symhists();
	threads__symhists_fprintf(total, stdout);

	return rc;
}

static const char * const report_usage[] = {
	"perf report [<options>] <command>",
	NULL
};

static const struct option options[] = {
	OPT_STRING('i', "input", &input_name, "file",
		    "input file name"),
	OPT_BOOLEAN('D', "dump-raw-trace", &dump_trace,
		    "dump raw trace in ASCII"),
	OPT_END()
};

int cmd_report(int argc, const char **argv, const char *prefix)
{
	elf_version(EV_CURRENT);

	page_size = getpagesize();

	parse_options(argc, argv, options, report_usage, 0);

	return __cmd_report();
}
