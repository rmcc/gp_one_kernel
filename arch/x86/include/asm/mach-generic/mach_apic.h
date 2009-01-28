#ifndef _ASM_X86_MACH_GENERIC_MACH_APIC_H
#define _ASM_X86_MACH_GENERIC_MACH_APIC_H

#include <asm/genapic.h>

#define init_apic_ldr (apic->init_apic_ldr)
#define ioapic_phys_id_map (apic->ioapic_phys_id_map)
#define setup_apic_routing (apic->setup_apic_routing)
#define multi_timer_check (apic->multi_timer_check)
#define apicid_to_node (apic->apicid_to_node)
#define cpu_to_logical_apicid (apic->cpu_to_logical_apicid) 
#define cpu_present_to_apicid (apic->cpu_present_to_apicid)
#define apicid_to_cpu_present (apic->apicid_to_cpu_present)
#define setup_portio_remap (apic->setup_portio_remap)
#define check_phys_apicid_present (apic->check_phys_apicid_present)
#define cpu_mask_to_apicid (apic->cpu_mask_to_apicid)
#define cpu_mask_to_apicid_and (apic->cpu_mask_to_apicid_and)
#define enable_apic_mode (apic->enable_apic_mode)
#define phys_pkg_id (apic->phys_pkg_id)
#define wakeup_secondary_cpu (apic->wakeup_cpu)

extern void generic_bigsmp_probe(void);

#endif /* _ASM_X86_MACH_GENERIC_MACH_APIC_H */
