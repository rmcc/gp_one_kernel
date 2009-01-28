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

	.irq_delivery_mode		= IRQ_DELIVERY_MODE,
	.irq_dest_mode			= IRQ_DEST_MODE,

	.target_cpus			= target_cpus,
	.ESR_DISABLE			= esr_disable,
	.apic_destination_logical	= APIC_DEST_LOGICAL,
	.check_apicid_used		= check_apicid_used,
	.check_apicid_present		= check_apicid_present,

	.no_balance_irq			= NO_BALANCE_IRQ,
	.no_ioapic_check		= 0,

	.vector_allocation_domain	= vector_allocation_domain,
	.init_apic_ldr			= init_apic_ldr,

	.ioapic_phys_id_map		= ioapic_phys_id_map,
	.setup_apic_routing		= setup_apic_routing,
	.multi_timer_check		= multi_timer_check,
	.apicid_to_node			= apicid_to_node,
	.cpu_to_logical_apicid		= cpu_to_logical_apicid,
	.cpu_present_to_apicid		= cpu_present_to_apicid,
	.apicid_to_cpu_present		= apicid_to_cpu_present,
	.setup_portio_remap		= setup_portio_remap,
	.check_phys_apicid_present	= check_phys_apicid_present,
	.enable_apic_mode		= enable_apic_mode,
	.phys_pkg_id			= phys_pkg_id,
	.mps_oem_check			= mps_oem_check,

	.get_apic_id			= get_apic_id,
	.set_apic_id			= NULL,
	.apic_id_mask			= APIC_ID_MASK,

	.cpu_mask_to_apicid		= cpu_mask_to_apicid,
	.cpu_mask_to_apicid_and		= cpu_mask_to_apicid_and,

	.send_IPI_mask			= send_IPI_mask,
	.send_IPI_mask_allbutself	= NULL,
	.send_IPI_allbutself		= send_IPI_allbutself,
	.send_IPI_all			= send_IPI_all,
	.send_IPI_self			= NULL,

	.wakeup_cpu			= NULL,
	.trampoline_phys_low		= TRAMPOLINE_PHYS_LOW,
	.trampoline_phys_high		= TRAMPOLINE_PHYS_HIGH,
	.wait_for_init_deassert		= wait_for_init_deassert,
	.smp_callin_clear_local_apic	= smp_callin_clear_local_apic,
	.store_NMI_vector		= store_NMI_vector,
	.restore_NMI_vector		= restore_NMI_vector,
	.inquire_remote_apic		= inquire_remote_apic,
};
