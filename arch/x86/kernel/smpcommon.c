/*
 * SMP stuff which is common to all sub-architectures.
 */
#include <linux/module.h>
#include <asm/smp.h>
#include <asm/sections.h>

#ifdef CONFIG_X86_32
/*
 * Initialize the CPU's GDT.  This is either the boot CPU doing itself
 * (still using the master per-cpu area), or a CPU doing it for a
 * secondary which will soon come up.
 */
__cpuinit void init_gdt(int cpu)
{
	struct desc_struct gdt;

	pack_descriptor(&gdt, __per_cpu_offset[cpu], 0xFFFFF,
			0x2 | DESCTYPE_S, 0x8);
	gdt.s = 1;

	write_gdt_entry(get_cpu_gdt_table(cpu),
			GDT_ENTRY_PERCPU, &gdt, DESCTYPE_S);
}
#endif
