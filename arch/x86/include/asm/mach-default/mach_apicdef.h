#ifndef _ASM_X86_MACH_DEFAULT_MACH_APICDEF_H
#define _ASM_X86_MACH_DEFAULT_MACH_APICDEF_H

#include <asm/apic.h>

#ifdef CONFIG_X86_64
#define	SET_APIC_ID(x)		(apic->set_apic_id(x))
#else
#define		DEFAULT_APIC_ID_MASK	(0x0F<<24)

static inline unsigned default_get_apic_id(unsigned long x) 
{
	unsigned int ver = GET_APIC_VERSION(apic_read(APIC_LVR));

	if (APIC_XAPIC(ver))
		return (x >> 24) & 0xFF;
	else
		return (x >> 24) & 0x0F;
} 

#endif

#endif /* _ASM_X86_MACH_DEFAULT_MACH_APICDEF_H */
