#ifndef _ASM_ALPHA_TOPOLOGY_H
#define _ASM_ALPHA_TOPOLOGY_H

#include <linux/smp.h>
#include <linux/threads.h>
#include <asm/machvec.h>

#ifdef CONFIG_NUMA
static inline int cpu_to_node(int cpu)
{
	int node;
	
	if (!alpha_mv.cpuid_to_nid)
		return 0;

	node = alpha_mv.cpuid_to_nid(cpu);

#ifdef DEBUG_NUMA
	BUG_ON(node < 0);
#endif

	return node;
}

static inline cpumask_t node_to_cpumask(int node)
{
	cpumask_t node_cpu_mask = CPU_MASK_NONE;
	int cpu;

	for_each_online_cpu(cpu) {
		if (cpu_to_node(cpu) == node)
			cpu_set(cpu, node_cpu_mask);
	}

#ifdef DEBUG_NUMA
	printk("node %d: cpu_mask: %016lx\n", node, node_cpu_mask);
#endif

	return node_cpu_mask;
}

#define pcibus_to_cpumask(bus)	(cpu_online_map)

struct pci_bus;

static inline int parent_node(int node)
{
	return node;
}

static inline int pcibus_to_node(struct pci_bus *bus)
{
	return -1;
}

static inline cpumask_t pcibus_to_cpumask(struct pci_bus *bus)
{
	return pcibus_to_node(bus) == -1 ?
		CPU_MASK_ALL :
		node_to_cpumask(pcibus_to_node(bus));
}

/* returns pointer to cpumask for specified node */
#define	node_to_cpumask_ptr(v, node) 					\
		cpumask_t _##v = node_to_cpumask(node);			\
		const cpumask_t *v = &_##v

#define node_to_cpumask_ptr_next(v, node)				\
			  _##v = node_to_cpumask(node)

static inline int node_to_first_cpu(int node)
{
	node_to_cpumask_ptr(mask, node);
	return first_cpu(*mask);
}

#else

#include <asm-generic/topology.h>

#endif /* !CONFIG_NUMA */

#endif /* _ASM_ALPHA_TOPOLOGY_H */
