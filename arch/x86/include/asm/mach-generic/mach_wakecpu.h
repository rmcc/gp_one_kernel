#ifndef _ASM_X86_MACH_GENERIC_MACH_WAKECPU_H
#define _ASM_X86_MACH_GENERIC_MACH_WAKECPU_H

#define smp_callin_clear_local_apic (apic->smp_callin_clear_local_apic)
#define store_NMI_vector (apic->store_NMI_vector)
#define restore_NMI_vector (apic->restore_NMI_vector)
#define inquire_remote_apic (apic->inquire_remote_apic)

#endif /* _ASM_X86_MACH_GENERIC_MACH_APIC_H */
