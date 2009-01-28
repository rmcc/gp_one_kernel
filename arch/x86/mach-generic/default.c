/*
 * Default generic APIC driver. This handles up to 8 CPUs.
 */
#define APIC_DEFINITION 1
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <asm/mpspec.h>
#include <asm/mach-default/mach_apicdef.h>
#include <asm/genapic.h>
#include <asm/fixmap.h>
#include <asm/apicdef.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <asm/mach-default/mach_apic.h>
#include <asm/mach-default/mach_ipi.h>
#include <asm/mach-default/mach_mpparse.h>
#include <asm/mach-default/mach_wakecpu.h>

static void default_vector_allocation_domain(int cpu, struct cpumask *retmask)
{
	/*
	 * Careful. Some cpus do not strictly honor the set of cpus
	 * specified in the interrupt destination when using lowest
	 * priority interrupt delivery mode.
	 *
	 * In particular there was a hyperthreading cpu observed to
	 * deliver interrupts to the wrong hyperthread when only one
	 * hyperthread was specified in the interrupt desitination.
	 */
	*retmask = (cpumask_t) { { [0] = APIC_ALL_CPUS } };
}

/* should be called last. */
static int probe_default(void)
{
	return 1;
}

struct genapic apic_default = {

	.name				= "default",
	.probe				= probe_default,
	.acpi_madt_oem_check		= NULL,
	.apic_id_registered		= default_apic_id_registered,

	.irq_delivery_mode		= dest_LowestPrio,
	/* logical delivery broadcast to all CPUs: */
	.irq_dest_mode			= 1,

	.target_cpus			= default_target_cpus,
	.disable_esr			= 0,
	.dest_logical			= APIC_DEST_LOGICAL,
	.check_apicid_used		= default_check_apicid_used,
	.check_apicid_present		= default_check_apicid_present,

	.vector_allocation_domain	= default_vector_allocation_domain,
	.init_apic_ldr			= default_init_apic_ldr,

	.ioapic_phys_id_map		= default_ioapic_phys_id_map,
	.setup_apic_routing		= default_setup_apic_routing,
	.multi_timer_check		= NULL,
	.apicid_to_node			= default_apicid_to_node,
	.cpu_to_logical_apicid		= default_cpu_to_logical_apicid,
	.cpu_present_to_apicid		= default_cpu_present_to_apicid,
	.apicid_to_cpu_present		= default_apicid_to_cpu_present,
	.setup_portio_remap		= NULL,
	.check_phys_apicid_present	= default_check_phys_apicid_present,
	.enable_apic_mode		= NULL,
	.phys_pkg_id			= default_phys_pkg_id,
	.mps_oem_check			= NULL,

	.get_apic_id			= default_get_apic_id,
	.set_apic_id			= NULL,
	.apic_id_mask			= 0x0F << 24,

	.cpu_mask_to_apicid		= default_cpu_mask_to_apicid,
	.cpu_mask_to_apicid_and		= default_cpu_mask_to_apicid_and,

	.send_IPI_mask			= default_send_IPI_mask,
	.send_IPI_mask_allbutself	= NULL,
	.send_IPI_allbutself		= default_send_IPI_allbutself,
	.send_IPI_all			= default_send_IPI_all,
	.send_IPI_self			= NULL,

	.wakeup_cpu			= NULL,
	.trampoline_phys_low		= DEFAULT_TRAMPOLINE_PHYS_LOW,
	.trampoline_phys_high		= DEFAULT_TRAMPOLINE_PHYS_HIGH,

	.wait_for_init_deassert		= default_wait_for_init_deassert,

	.smp_callin_clear_local_apic	= NULL,
	.store_NMI_vector		= NULL,
	.inquire_remote_apic		= default_inquire_remote_apic,
};
