#ifndef _ASM_X86_GENAPIC_H
#define _ASM_X86_GENAPIC_H

#include <linux/cpumask.h>

#include <asm/mpspec.h>
#include <asm/atomic.h>

/*
 * Copyright 2004 James Cleverdon, IBM.
 * Subject to the GNU Public License, v.2
 *
 * Generic APIC sub-arch data struct.
 *
 * Hacked for x86-64 by James Cleverdon from i386 architecture code by
 * Martin Bligh, Andi Kleen, James Bottomley, John Stultz, and
 * James Cleverdon.
 */
struct genapic {
	char *name;

	int (*probe)(void);
	int (*acpi_madt_oem_check)(char *oem_id, char *oem_table_id);
	int (*apic_id_registered)(void);

	u32 irq_delivery_mode;
	u32 irq_dest_mode;

	const struct cpumask *(*target_cpus)(void);

	int disable_esr;

	int apic_destination_logical;
	unsigned long (*check_apicid_used)(physid_mask_t bitmap, int apicid);
	unsigned long (*check_apicid_present)(int apicid);

	int no_balance_irq;
	int no_ioapic_check;

	void (*vector_allocation_domain)(int cpu, struct cpumask *retmask);
	void (*init_apic_ldr)(void);

	physid_mask_t (*ioapic_phys_id_map)(physid_mask_t map);

	void (*setup_apic_routing)(void);
	int (*multi_timer_check)(int apic, int irq);
	int (*apicid_to_node)(int logical_apicid);
	int (*cpu_to_logical_apicid)(int cpu);
	int (*cpu_present_to_apicid)(int mps_cpu);
	physid_mask_t (*apicid_to_cpu_present)(int phys_apicid);
	void (*setup_portio_remap)(void);
	int (*check_phys_apicid_present)(int boot_cpu_physical_apicid);
	void (*enable_apic_mode)(void);
#ifdef CONFIG_X86_32
	u32 (*phys_pkg_id)(u32 cpuid_apic, int index_msb);
#else
	unsigned int (*phys_pkg_id)(int index_msb);
#endif

	/*
	 * When one of the next two hooks returns 1 the genapic
	 * is switched to this. Essentially they are additional
	 * probe functions:
	 */
	int (*mps_oem_check)(struct mpc_table *mpc, char *oem,
			     char *productid);

	unsigned int (*get_apic_id)(unsigned long x);
	unsigned long (*set_apic_id)(unsigned int id);
	unsigned long apic_id_mask;

	unsigned int (*cpu_mask_to_apicid)(const struct cpumask *cpumask);
	unsigned int (*cpu_mask_to_apicid_and)(const struct cpumask *cpumask,
					       const struct cpumask *andmask);

	/* ipi */
	void (*send_IPI_mask)(const struct cpumask *mask, int vector);
	void (*send_IPI_mask_allbutself)(const struct cpumask *mask,
					 int vector);
	void (*send_IPI_allbutself)(int vector);
	void (*send_IPI_all)(int vector);
	void (*send_IPI_self)(int vector);

	/* wakeup_secondary_cpu */
	int (*wakeup_cpu)(int apicid, unsigned long start_eip);

	int trampoline_phys_low;
	int trampoline_phys_high;

	void (*wait_for_init_deassert)(atomic_t *deassert);
	void (*smp_callin_clear_local_apic)(void);
	void (*store_NMI_vector)(unsigned short *high, unsigned short *low);
	void (*restore_NMI_vector)(unsigned short *high, unsigned short *low);
	void (*inquire_remote_apic)(int apicid);
};

extern struct genapic *apic;

#ifdef CONFIG_X86_32
extern void es7000_update_genapic_to_cluster(void);
#else
extern struct genapic apic_flat;
extern struct genapic apic_physflat;
extern struct genapic apic_x2apic_cluster;
extern struct genapic apic_x2apic_phys;
extern int default_acpi_madt_oem_check(char *, char *);

extern void apic_send_IPI_self(int vector);

extern struct genapic apic_x2apic_uv_x;
DECLARE_PER_CPU(int, x2apic_extra_bits);

extern void setup_apic_routing(void);
#endif

#endif /* _ASM_X86_GENAPIC_64_H */
