/* Glue code to lib/swiotlb.c */

#include <linux/pci.h>
#include <linux/cache.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>

#include <asm/swiotlb.h>
#include <asm/dma.h>
#include <asm/iommu.h>
#include <asm/machvec.h>

int swiotlb __read_mostly;
EXPORT_SYMBOL(swiotlb);

/* Set this to 1 if there is a HW IOMMU in the system */
int iommu_detected __read_mostly;

struct dma_mapping_ops swiotlb_dma_ops = {
	.alloc_coherent = swiotlb_alloc_coherent,
	.free_coherent = swiotlb_free_coherent,
	.map_single = swiotlb_map_single,
	.unmap_single = swiotlb_unmap_single,
	.map_single_attrs = swiotlb_map_single_attrs,
	.unmap_single_attrs = swiotlb_unmap_single_attrs,
	.map_sg_attrs = swiotlb_map_sg_attrs,
	.unmap_sg_attrs	= swiotlb_unmap_sg_attrs,
	.sync_single_for_cpu = swiotlb_sync_single_for_cpu,
	.sync_single_for_device = swiotlb_sync_single_for_device,
	.sync_single_range_for_cpu = swiotlb_sync_single_range_for_cpu,
	.sync_single_range_for_device = swiotlb_sync_single_range_for_device,
	.sync_sg_for_cpu = swiotlb_sync_sg_for_cpu,
	.sync_sg_for_device = swiotlb_sync_sg_for_device,
	.map_sg = swiotlb_map_sg,
	.unmap_sg = swiotlb_unmap_sg,
	.dma_supported_op = swiotlb_dma_supported,
	.mapping_error = swiotlb_dma_mapping_error,
};

void __init pci_swiotlb_init(void)
{
	if (!iommu_detected) {
#ifdef CONFIG_IA64_GENERIC
		swiotlb = 1;
		printk(KERN_INFO "PCI-DMA: Re-initialize machine vector.\n");
		machvec_init("dig");
		swiotlb_init();
		dma_ops = &swiotlb_dma_ops;
#else
		panic("Unable to find Intel IOMMU");
#endif
	}
}
