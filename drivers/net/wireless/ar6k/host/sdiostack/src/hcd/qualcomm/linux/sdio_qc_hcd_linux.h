/******************************************************************************
 *
 * @File:       sdio_qualcomm_hcd_linux.c
 *
 * @Abstract:   Qualcomm SDIO Host Controller Driver Header File
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

#ifndef __SDIO_QUALCOMM_HCD_LINUX_H__
#define __SDIO_QUALCOMM_HCD_LINUX_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <asm/irq.h>
///+++FIH+++
//#include <asm/arch/dma.h>
#include <linux/dma-mapping.h>

/* module characteristic */
#define DESCRIPTION                              "SDIO Qualcomm HCD"
#define AUTHOR                                   "Atheros Communications, Inc."

/* clk-reg-related */
#define SDC_MD_REG                               0xA0
#define SDC_NS_REG                               0xA4
#define SDC_H_CLK_ENA                            7

/* flags */
#define SDIO_HCD_MAPPED                          0x01
#define qualcomm_SDIO_IRQ_SET                    0x01

/* SD clock rate */
///+++FIH+++
//#define WLAN_CLK_RATE                            144000
//#define WLAN_CLK_RATE                              16000000
#define WLAN_CLK_RATE                            20000000 
//#define WLAN_CLK_RATE                            24576000
//#define WLAN_CLK_RATE                            25000000
///---FIH---

/* customer environment-related */
#if defined(QSD8K_BSP2045)
  #define WLAN_3_0V_ID                           "gp5"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QST1105_BSP2510)
  #define WLAN_3_0V_ID                           "mmc"
#elif defined(QST1105_BSP2120)
  #define WLAN_3_0V_ID                           PM_VREG_WLAN_ID
#elif defined(ND1)
  #define WLAN_2_6V_ID                           PM_VREG_WLAN_ID
  #define WLAN_1_8V_ID                           23
  #define WLAN_1_2V_ID                           1
  #define WLAN_CHIP_PWD_L_ID                     38
#else
  #error Wrong environment
#endif

/* which slot?*/
#ifdef SLOT_1
  #define MSM_SDC_BASE_ADDR                      MSM_SDC1_BASE_ADDR
  #define MSM_SDC_ADDR_LEN                       MSM_SDC1_ADDR_LEN
  #define MSM_SDC_INT                            MSM_SDC1_INT
  #define MSM_SDC_PCLK                           MSM_SDC1_PCLK
  #define MSM_SDC_MCLK                           MSM_SDC1_MCLK
  #define SDIO_WLAN_SLOT_ID                      1
  #define SDIO_WLAN_SLOT_DMA_CHAN                8
  #define SDIO_WLAN_SLOT_DMA_CRCI                6
#elif defined(SLOT_2)
  #define MSM_SDC_BASE_ADDR                      MSM_SDC2_BASE_ADDR
  #define MSM_SDC_ADDR_LEN                       MSM_SDC2_ADDR_LEN
  #define MSM_SDC_INT                            MSM_SDC2_INT
  #define MSM_SDC_PCLK                           MSM_SDC2_PCLK
  #define MSM_SDC_MCLK                           MSM_SDC2_MCLK
  #define SDIO_WLAN_SLOT_ID                      2
  #define SDIO_WLAN_SLOT_DMA_CHAN                8
  #define SDIO_WLAN_SLOT_DMA_CRCI                7
#elif defined(SLOT_3)
  #define MSM_SDC_BASE_ADDR                      MSM_SDC3_BASE_ADDR
  #define MSM_SDC_ADDR_LEN                       MSM_SDC3_ADDR_LEN
  #define MSM_SDC_INT                            MSM_SDC3_INT
  #define MSM_SDC_PCLK                           MSM_SDC3_PCLK
  #define MSM_SDC_MCLK                           MSM_SDC3_MCLK
  #define SDIO_WLAN_SLOT_ID                      3
  #define SDIO_WLAN_SLOT_DMA_CHAN                8
  #define SDIO_WLAN_SLOT_DMA_CRCI                12
#elif defined(SLOT_4)
  #define MSM_SDC_BASE_ADDR                      MSM_SDC4_BASE_ADDR
  #define MSM_SDC_ADDR_LEN                       MSM_SDC4_ADDR_LEN
  #define MSM_SDC_INT                            MSM_SDC4_INT
  #define MSM_SDC_PCLK                           MSM_SDC4_PCLK
  #define MSM_SDC_MCLK                           MSM_SDC4_MCLK
  #define SDIO_WLAN_SLOT_ID                      4
  #define SDIO_WLAN_SLOT_DMA_CHAN                11
  #define SDIO_WLAN_SLOT_DMA_CRCI                13
#else
  #error Wrong Slot Number
#endif

/* SDC1 */
#if defined(QST1105_BSP2120) || defined(ND1)
  #define MSM_SDC1_BASE_ADDR                     MSM_SDCC2_1_PHYS
  #define MSM_SDC1_ADDR_LEN                      MSM_SDCC2_1_SIZE
  #define MSM_SDC1_PCLK                          "sdc1_p_clk"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
  #define MSM_SDC1_BASE_ADDR                     MSM_SDC1_PHYS
  #define MSM_SDC1_ADDR_LEN                      MSM_SDC1_SIZE
  #define MSM_SDC1_PCLK                          "sdc1_pclk"
#else
  #error Wrong SDC1 BSP type
#endif
#define MSM_SDC1_MCLK                            "sdc1_clk"
#define MSM_SDC1_INT                             INT_SDC1_0

/* SDC2 */
#if defined(QST1105_BSP2120) || defined(ND1)
  #define MSM_SDC2_BASE_ADDR                     (MSM_SDCC2_1_PHYS + 0x100000)
  #define MSM_SDC2_ADDR_LEN                      MSM_SDCC2_1_SIZE
  #define MSM_SDC2_PCLK                          "sdc2_p_clk"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
  ///+++FIH+++
  ///#define MSM_SDC2_BASE_ADDR                     MSM_SDC2_PHYS
  ///#define MSM_SDC2_ADDR_LEN                      MSM_SDC2_SIZE
  /// #define MSM_SDC2_PCLK                          "sdc2_pclk"
	#define MSM_SDC2_BASE_ADDR                     0xA0500000       
	#define MSM_SDC2_ADDR_LEN                      SZ_4K   
	#define MSM_SDC2_PCLK                          "sdc_pclk"
  ///---FIH---
  
#else
  #error Wrong SDC2 BSP type
#endif
///+++FIH+++
//#define MSM_SDC2_MCLK                            "sdc2_clk"
#define MSM_SDC2_MCLK                            "sdc_clk"
///---FIH---
#define MSM_SDC2_INT                             INT_SDC2_0

/* SDC3 */
#if defined(QST1105_BSP2120) || defined(ND1)
  #define MSM_SDC3_BASE_ADDR                     (MSM_SDCC2_1_PHYS + 0x200000)
  #define MSM_SDC3_ADDR_LEN                      MSM_SDCC2_1_SIZE
  #define MSM_SDC3_PCLK                          "sdc3_p_clk"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
  #define MSM_SDC3_BASE_ADDR                     MSM_SDC3_PHYS
  #define MSM_SDC3_ADDR_LEN                      MSM_SDC3_SIZE
  #define MSM_SDC3_PCLK                          "sdc3_pclk"
#else
  #error Wrong SDC3 BSP type
#endif
#define MSM_SDC3_MCLK                            "sdc3_clk"
#define MSM_SDC3_INT                             INT_SDC3_0

/* SDC4 */
#if defined(QST1105_BSP2120) || defined(ND1)
  #define MSM_SDC4_BASE_ADDR                     (MSM_SDCC2_1_PHYS + 0x300000)
  #define MSM_SDC4_ADDR_LEN                      MSM_SDCC2_1_SIZE
  #define MSM_SDC4_PCLK                          "sdc4_p_clk"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
  #define MSM_SDC4_BASE_ADDR                     MSM_SDC4_PHYS
  #define MSM_SDC4_ADDR_LEN                      MSM_SDC4_SIZE
  #define MSM_SDC4_PCLK                          "sdc4_pclk"
#else
  #error Wrong SDC4 BSP type
#endif
#define MSM_SDC4_MCLK                            "sdc4_clk"
#define MSM_SDC4_INT                             INT_SDC4_0

/* macro for platform driver */
#if defined(QST1105_BSP2120) || defined(ND1)
  #define SDIO_HCD_PD_NAME                       "mmci-sdcc2"
#elif defined(MSM7201A_BSP6310) || defined(MSM7201A_BSP6320) || defined(QSD8K_BSP2045) || defined(QST1105_BSP2510)
  #define SDIO_HCD_PD_NAME                       "msm_sdcc_wlan"
#else
  #error Wrong platform driver name
#endif

/* max device name string length */
#define SDHCD_MAX_DEVICE_NAME                    64

/* Advance DMA parameters */
#define SDHCD_MAX_ADMA_DESCRIPTOR                32
#define SDHCD_ADMA_DESCRIPTOR_SIZE               \
                (SDHCD_MAX_ADMA_DESCRIPTOR * sizeof(SDHCD_SGDMA_DESCRIPTOR))
#define SDHCD_MAX_ADMA_LENGTH                    0x8000     /* up to 32KB per descriptor */    
#define SDHCD_ADMA_ADDRESS_MASK                  0xFFFFE000 /* 4KB boundaries */
#define SDHCD_ADMA_ALIGNMENT                     0xFFF      /* illegal alignment bits*/
#define SDHCD_ADMA_LENGTH_ALIGNMENT              0x0        /* any length up to the max */

/* simple DMA */
#define SDHCD_MAX_SDMA_DESCRIPTOR                1
#define SDHCD_MAX_SDMA_LENGTH                    0x80000    /* up to 512KB for a single descriptor*/    
#define SDHCD_SDMA_ADDRESS_MASK                  0xFFFFFFFF /* any 32 bit address */
#define SDHCD_SDMA_ALIGNMENT                     0x0        /* any 32 bit address */
#define SDHCD_SDMA_LENGTH_ALIGNMENT              0x0        /* any length up to the max */

/* mapped memory address */
typedef struct _SDHCD_MEMORY {
    ULONG Raw;                                   /* start of address range */
    ULONG Length;                                /* length of range */
    PVOID pMapped;                               /* the mapped address */
} SDHCD_MEMORY, *PSDHCD_MEMORY;

/* dma-related */
#define NR_SG                                    32

#ifdef DMA_SUPPORT
struct nc_dmadata {
	dmov_box	cmd[NR_SG];
	uint32_t	cmdptr;
};

struct dma_data {
	struct nc_dmadata		*nc;
	dma_addr_t			nc_busaddr;
	dma_addr_t			cmd_busaddr;
	dma_addr_t			cmdptr_busaddr;

	struct msm_dmov_cmd		hdr;
	enum dma_data_direction		dir;

	struct scatterlist		sg;
	int				num_ents;
	int				user_pages;

	int				channel;
	void				*pHost;
};
#endif

typedef struct _SDHCD_OS_SPECIFIC {
    SDHCD_MEMORY Address;                        /* memory address of this device */ 
    spinlock_t   RegAccessLock;                  /* use to protect registers when needed */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    struct work_struct iocomplete_work;          /* work item definitions */
    struct work_struct carddetect_work;          /* work item definintions */
    struct work_struct sdioirq_work;             /* work item definintions */
#else
    struct delayed_work iocomplete_work;         /* work item definitions */ 
    struct delayed_work carddetect_work;         /* work item definintions */
    struct delayed_work sdioirq_work;            /* work item definintions */
#endif
#ifdef DMA_SUPPORT
    struct dma_data     dma;                     /* dma-related info */
#endif
    spinlock_t   Lock;                           /* general purpose lock against the ISR */
    DMA_ADDRESS  hDmaBuffer;                     /* handle for data buffer */
    PUINT8       pDmaBuffer;                     /* virtual address of DMA command buffer */
    PSDDMA_DESCRIPTOR pDmaList;                  /* in use scatter-gather list */
    UINT         SGcount;                        /* count of in-use scatter gather list */
    /* the STD-host defined slot number assigned to this instance */
    UINT         SlotNumber;
    /* everything below this line is used by the implementation that uses this STD core */
    UINT16        InitMask;                      /* implementation specific portion init mask */
    UINT32        ImpSpecific0;                  /* implementation specific storage */           
    UINT32        ImpSpecific1;                  /* implementation specific storage */ 
} SDHCD_OS_SPECIFIC, *PSDHCD_OS_SPECIFIC;

/* prototype */
static int qualcomm_probe(struct platform_device *pdev);
static int qualcomm_remove(struct platform_device *pdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static irqreturn_t sdio_hcd_irq(int irq, void *context, struct pt_regs *r);
#else
static irqreturn_t sdio_hcd_irq(int irq, void *context);
#endif /* LINUX_VERSION_CODE */

#endif /* __SDIO_QUALCOMM_HCD_LINUX_H__ */
