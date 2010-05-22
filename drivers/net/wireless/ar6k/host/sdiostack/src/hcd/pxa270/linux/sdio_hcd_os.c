// Copyright (c) 2005, 2006 Atheros Communications Inc.
// 
//
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
//
//

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_hcd_os.c

@abstract: Linux PXA270 Local Bus SDIO Host Controller Driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#define DBG_DECLARE 7;
#include "../../../include/ctsystem.h"

#include "../sdio_pxa270hcd.h"
#include <linux/fs.h>
#include <linux/ioport.h> 
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <asm/arch/pxa-regs.h> 
#include <asm/arch/dma.h>
#include <linux/dma-mapping.h>

#ifdef CONFIG_MACH_SANDGATE2P
#include <asm/arch/sandgate2p.h>
#elif defined(CONFIG_MACH_SANDGATE2G)
#include <asm/arch/sandgate2g.h>
#elif defined(CONFIG_MACH_SANDGATE2)
#include <asm/arch/sandgate2.h>
#endif

#define DESCRIPTION "SDIO PXA270 Local Bus HCD"
#define AUTHOR "Atheros Communications, Inc."

#if defined(CONFIG_MACH_SANDGATE2G)
unsigned long sandgate2_bcr = 0x000003c0;
#endif

static int Probe(struct pnp_dev *pBusDevice, const struct pnp_device_id *pId);
static void Remove(struct pnp_dev *pBusDevice);
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription);
static void UnmapAddress(PSDHCD_MEMORY pMap);

static irqreturn_t hcd_mmc_irq(int irq, void *context, struct pt_regs * r);

static void hcd_iocomplete_wqueue_handler(void *context);
static void hcd_sdioirq_wqueue_handler(void *context);
static void DmaCompletionCallBack(int dma, void *devid, struct pt_regs *regs);

#define BASE_HCD_ATTRIBUTES (SDHCD_ATTRIB_BUS_1BIT  |      \
                             SDHCD_ATTRIB_BUS_4BIT)

#define BASE_HCD_SPI_ATTRIBUTES SDHCD_ATTRIB_BUS_SPI

#ifdef USE_CARD_DETECT_HW
static void hcd_carddetect_wqueue_handler(void *context);
static irqreturn_t hcd_card_detect_insert_irq(int irq, void *context, struct pt_regs * r);
static irqreturn_t hcd_card_detect_remove_irq(int irq, void *context, struct pt_regs * r);
static DECLARE_WORK(carddetect_work, hcd_carddetect_wqueue_handler, &HcdContext);
#define DEFAULT_ATTRIBUTES      BASE_HCD_ATTRIBUTES
#define DEFAULT_SPI_ATTRIBUTES  BASE_HCD_SPI_ATTRIBUTES
#else
#define DEFAULT_ATTRIBUTES      (BASE_HCD_ATTRIBUTES | SDHCD_ATTRIB_SLOT_POLLING)
#define DEFAULT_SPI_ATTRIBUTES  (BASE_HCD_SPI_ATTRIBUTES | SDHCD_ATTRIB_SLOT_POLLING)
#endif

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

                            
UINT32 UseSpi = 0;
module_param(UseSpi, int, 0644);
MODULE_PARM_DESC(UseSpi, "PXA270 HCD use SPI Mode");

UINT32 noDMA = 0;
module_param(noDMA, int, 0644);
MODULE_PARM_DESC(noDMA, "PXA270 HCD disable DMA");

UINT32 max_blocks = SDIO_PXA_MAX_BLOCKS;
module_param(max_blocks, int, 0644);
MODULE_PARM_DESC(max_blocks, "PXA270 HCD max blocks per transfer");

UINT32 max_bytes_per_block = SDIO_PXA_MAX_BYTES_PER_BLOCK;
module_param(max_bytes_per_block, int, 0644);
MODULE_PARM_DESC(max_bytes_per_block, "PXA270 HCD disable DMA");

UINT32 builtin_card = 0;
module_param(builtin_card, int, 0644);
MODULE_PARM_DESC(builtin_card, "SDIO card is built-in");

UINT32 multiblock_irq = 0;
module_param(multiblock_irq, int, 0644);
MODULE_PARM_DESC(multiblock_irq, "Enable Multi-block IRQ detect");

/* the driver context data */
BOOL IsCardInserted(PSDHCD_DRIVER_CONTEXT pHct);

#define MAX_DMA_DESCRIPTORS 64

#define PXA_DESCRIPTOR_BUFFER_SIZE (MAX_DMA_DESCRIPTORS*4)


#define PXA_MAX_BYTES_PER_DESCRIPTOR  8160 /* (8K - FIFO SIZE) */
#define PXA_DMA_COMMON_BUFFER_SIZE   PXA_MAX_BYTES_PER_DESCRIPTOR


static SDHCD_DRIVER_CONTEXT HcdContext = {
   .pDescription  = DESCRIPTION,
   .Hcd.pName = "sdio_pxa2270hcd",
   .Hcd.Version = CT_SDIO_STACK_VERSION_CODE,
   .Hcd.SlotNumber = 0,
   .Hcd.Attributes = 0,
   .Hcd.MaxBytesPerBlock = 0, /* see below */
   .Hcd.MaxBlocksPerTrans = 0,
   .Hcd.MaxSlotCurrent = 500, /* 1/2 amp */
   .Hcd.SlotVoltageCaps = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.SlotVoltagePreferred = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.MaxClockRate = 19500000, /* 19.5 Mhz */
   .Hcd.pContext = &HcdContext,
   .Hcd.pRequest = HcdRequest,
   .Hcd.pConfigure = HcdConfig,
   .Device.HcdDevice.name = "sdio_pxa270_hcd",
   .Device.HcdDriver.name = "sdio_pxa270_hcd",
   .Device.HcdDriver.probe  = Probe,
   .Device.HcdDriver.remove = Remove,    
   .Device.Dma.Mask = 0xFFFFFFFF,    /* any address */
   .Device.Dma.Flags = SDDMA_DESCRIPTION_FLAG_DMA | SDDMA_DESCRIPTION_FLAG_SGDMA,
   .Device.Dma.MaxBytesPerDescriptor = PXA_MAX_BYTES_PER_DESCRIPTOR,
   .Device.Dma.AddressAlignment = 0x0,  /* no illegal bits, buffers address can be on any boundary*/
   .Device.Dma.LengthAlignment = 0x0,   /* no illegal bits, buffer lengths can be any byte count */
   .Device.Dma.MaxDescriptors = MAX_DMA_DESCRIPTORS,      /* */
};

    
/* work queues */
static DECLARE_WORK(iocomplete_work, hcd_iocomplete_wqueue_handler, &HcdContext);
static DECLARE_WORK(sdioirq_work, hcd_sdioirq_wqueue_handler, &HcdContext);


/*
 * Probe - probe to setup our device, if present
*/
static int Probe(struct pnp_dev *pBusDevice, const struct pnp_device_id *pId)
{
    SYSTEM_STATUS err = 0;
    SDIO_STATUS   status;
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;

    DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Local HCD: Probe - probing for new device\n"));
 
    pHcdContext->Device.pBusDevice = pBusDevice;
    pHcdContext->Hcd.pDevice = &pBusDevice->dev;
    pHcdContext->Hcd.pModule = THIS_MODULE;
    pHcdContext->Device.MMCInterrupt = SDIO_PXA_CONTROLLER_IRQ;
    pHcdContext->Device.CardInsertInterrupt = SDIO_PXA_CARD_INSERT_IRQ;  
    pHcdContext->Device.ControlRegs.Raw = PXA_MMC_CONTROLLER_BASE_ADDRESS;
    pHcdContext->Device.ControlRegs.Length = PXA_MMC_CONTROLLER_ADDRESS_LENGTH;  
    pHcdContext->Device.GpioRegs.Raw = PXA_GPIO_PIN_LVL_REGS_BASE;
    pHcdContext->Device.GpioRegs.Length = PXA_GPIO_PIN_LVL_REGS_LENGTH;

    spin_lock_init(&pHcdContext->Device.Lock);
    
    if (0 == UseSpi) {
        pHcdContext->Hcd.Attributes = DEFAULT_ATTRIBUTES;
        if (multiblock_irq) {
            pHcdContext->Hcd.Attributes |= SDHCD_ATTRIB_MULTI_BLK_IRQ; 
        }
    } else {
        /* pHcdContext->Hcd.Attributes = DEFAULT_SPI_ATTRIBUTES; */   
        /* not tested */
        DBG_ASSERT(FALSE);
    }
    
    if (builtin_card) {
            /* remove slot polling */
        pHcdContext->Hcd.Attributes &= ~SDHCD_ATTRIB_SLOT_POLLING;   
    }
    
    if (pHcdContext->Hcd.Attributes & SDHCD_ATTRIB_BUS_SPI) {
            /* in SPI mode, the controller only supports 1 block */
        pHcdContext->Hcd.MaxBytesPerBlock = SPI_PXA_MAX_BYTES_PER_BLOCK;
        pHcdContext->Hcd.MaxBlocksPerTrans = SPI_PXA_MAX_BLOCKS;  
    } else {
        pHcdContext->Hcd.MaxBytesPerBlock = min(max_bytes_per_block,(UINT32)SDIO_PXA_MAX_BYTES_PER_BLOCK);
        pHcdContext->Hcd.MaxBlocksPerTrans = min(max_blocks,(UINT32)SDIO_PXA_MAX_BLOCKS); 
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 - Max Blocks:%d, Max Bytes Per Block:%d \n",
          pHcdContext->Hcd.MaxBlocksPerTrans, pHcdContext->Hcd.MaxBytesPerBlock));
                                
    do {
        
        pHcdContext->Device.DmaChannel = -1;
        
        if (!noDMA) { 
            
            pHcdContext->Device.pDmaDescriptorBuffer = (PUINT32)dma_alloc_coherent(
                                                               &pHcdContext->Device.pBusDevice->dev, 
                                                               PXA_DESCRIPTOR_BUFFER_SIZE, 
                                                               &pHcdContext->Device.DmaDescriptorPhys, 
                                                               GFP_DMA);
                                                               
            if (NULL == pHcdContext->Device.pDmaDescriptorBuffer) {
                err = -ENOMEM;
                break;    
            }
            
            if (pHcdContext->Device.DmaDescriptorPhys & 0xF) { 
                err = -ENOMEM;
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO PXA270 Local HCD: Descriptor buffer is not 16 byte aligned! 0x%X\n",
                        pHcdContext->Device.DmaDescriptorPhys));
                break;    
            }            
             
            pHcdContext->Device.pDmaCommonBuffer = (PUINT8)dma_alloc_coherent(
                                                               &pHcdContext->Device.pBusDevice->dev, 
                                                               PXA_DMA_COMMON_BUFFER_SIZE, 
                                                               &pHcdContext->Device.DmaCommonBufferPhys, 
                                                               GFP_DMA);
                                                               
            if (NULL == pHcdContext->Device.pDmaCommonBuffer) {
                err = -ENOMEM;
                break;    
            }
            
            pHcdContext->Hcd.pDmaDescription = &HcdContext.Device.Dma;  
            pHcdContext->DmaCapable = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 - Common Buffer:0x%X Size:%d \n",
                (UINT32)pHcdContext->Device.DmaCommonBufferPhys, PXA_DMA_COMMON_BUFFER_SIZE));
        }
        
        /* map the devices memory addresses */
        err = MapAddress(&pHcdContext->Device.ControlRegs, "SDIOPXA270");
        if (err < 0) {
            /* couldn't map the address */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Local HCD: Probe - unable to map memory\n"));
            break; 
        }

        err = MapAddress(&pHcdContext->Device.GpioRegs, "SDIOPXA270");
        if (err < 0) {
            /* couldn't map the address */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Local HCD: Probe - unable to map memory\n"));
            break;
        }

        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Switching GPIO lines \n"));
        pxa_gpio_mode(GPIO92_MMCDAT0_MD);
        pxa_gpio_mode(GPIO109_MMCDAT1_MD);
        pxa_gpio_mode(GPIO110_MMCDAT2_MD);
        if (pHcdContext->Hcd.Attributes & SDHCD_ATTRIB_BUS_SPI) {
            pxa_gpio_mode(GPIO110_MMCCS1_MD); /* GPIO110_MMCCS1_MD is a typo in pxa-regs.h */
        } else {             
            pxa_gpio_mode(GPIO111_MMCDAT3_MD); 
        }
        pxa_gpio_mode(GPIO112_MMCCMD_MD);
        pxa_gpio_mode(GPIO32_MMCCLK_MD);

            /* route MMC clock */
        pxa_set_cken(CKEN12_MMC, 1);
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 - MMC IRQ:%d \n",
                                pHcdContext->Device.MMCInterrupt));
            
            /* map the controller interrupt */
        err = request_irq (pHcdContext->Device.MMCInterrupt, hcd_mmc_irq, 0,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
              DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - unable to map MMC interrupt \n"));
              err = -ENODEV;
              break;
        }
        
        pHcdContext->Device.InitStateMask |= MMC_INTERRUPT_INIT;
        
#if defined(CONFIG_MACH_SANDGATE2P) || defined(CONFIG_MACH_SANDGATE2G) || defined(CONFIG_MACH_SANDGATE2)
        /* turn it on for now...the BCR register access is not protected, 
        * so we only want to touch this once */       
        SlotPowerOnOff(pHcdContext, TRUE);
#endif 
         
        if (!SDIO_SUCCESS((status = HcdInitialize(pHcdContext)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Probe - failed to init HW, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
               
        pHcdContext->Device.InitStateMask |= SDHC_HW_INIT;
        
        pHcdContext->Device.DmaChannel = pxa_request_dma(pHcdContext->pDescription, 
                                                         DMA_PRIO_LOW, 
                                                         DmaCompletionCallBack,
                                                         pHcdContext);
        if (pHcdContext->Device.DmaChannel < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Probe - failed to allocate DMA channel\n"));
            err = -ENODEV;
            break;  
        }
        
    	   /* register with the SDIO bus driver */
    	if (!SDIO_SUCCESS((status = SDIO_RegisterHostController(&pHcdContext->Hcd)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Probe - failed to register with host, status =%d\n",
                                    status));
            err = SDIOErrorToOSError(status);
            break;
    	} 
             
        pHcdContext->Device.InitStateMask |= SDHC_REGISTERED;
       
#ifdef CONFIG_MACH_SANDGATE2P
#ifdef USE_CARD_DETECT_HW
        pHcdContext->Device.CardInsertInterrupt = SANDGATE2P_MMC_IN_IRQ;
        pHcdContext->Device.CardRemoveInterrupt = SANDGATE2P_MMC_OUT_IRQ;

        DBG_PRINT(SDDBG_TRACE,("SDIO PXA270 - card insert IRQ :%d, card remove IRQ:%d \n",
                                pHcdContext->Device.CardInsertInterrupt,  
                                pHcdContext->Device.CardRemoveInterrupt)); 
                                
            /* map the card insert interrupt */
        err = request_irq (pHcdContext->Device.CardInsertInterrupt, hcd_card_detect_insert_irq, 0,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - unable to map CardInsert interrupt \n"));
            err = -ENODEV;
            break; 
        } 
        
        pHcdContext->Device.InitStateMask |= CARD_DETECT_INSERT_INTERRUPT_INIT;
        
            /* map the card remove interrupt */
        err = request_irq (pHcdContext->Device.CardRemoveInterrupt, hcd_card_detect_remove_irq, 0,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - unable to map CardRemove interrupt \n"));
            err = -ENODEV;
            break; 
        } 
            /* disable remove */
        disable_irq(pHcdContext->Device.CardRemoveInterrupt);
               
        pHcdContext->Device.InitStateMask |= CARD_DETECT_REMOVE_INTERRUPT_INIT;
#endif
#endif

        if (builtin_card) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Forcing ATTACH on built-in card \n"));
            SDIO_HandleHcdEvent(&pHcdContext->Hcd, EVENT_HCD_ATTACH);    
        }
        
    } while (FALSE);
    
    if (err < 0) {
        Remove(pBusDevice); /* TODO: the cleanup should not really be done in the Remove function */
    } else {
#ifdef USE_CARD_DETECT_HW
        pHcdContext->Device.StartUpCheck = TRUE;
            /* queue the work item test the slot */
        if (!SDIO_SUCCESS(QueueEventResponse(pHcdContext, WORK_ITEM_CARD_DETECT))) {
                /* failed */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Probe - queue event failed\n"));
        }
#endif
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Probe - HCD ready! \n"));
    }
    return err;  
}

/* Remove - remove  device
 * perform the undo of the Probe
*/
static void Remove(struct pnp_dev *pBusDevice) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO PXA270 Local HCD: Remove - removing device\n"));
    
    pHcdContext->Device.ShutDown = TRUE;
    
    OSSleep(1000);
    
    if (pHcdContext->Device.InitStateMask & MMC_INTERRUPT_INIT) {
        WRITE_MMC_REG(pHcdContext, MMC_I_MASK_REG, MMC_MASK_ALL_INTS);
    }
 
    if (pHcdContext->Device.InitStateMask & SDHC_REGISTERED) {
            /* unregister from the bus driver */
        SDIO_UnregisterHostController(&pHcdContext->Hcd);
    }
    
    if (pHcdContext->Device.DmaChannel >= 0) {
        pxa_free_dma(pHcdContext->Device.DmaChannel); 
        pHcdContext->Device.DmaChannel = -1;
    }
    
        /* free irqs */
    if (pHcdContext->Device.InitStateMask & MMC_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.MMCInterrupt, pHcdContext);
    }
    if (pHcdContext->Device.InitStateMask & CARD_DETECT_INSERT_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.CardInsertInterrupt, pHcdContext);
    }
    if (pHcdContext->Device.InitStateMask & CARD_DETECT_REMOVE_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.CardRemoveInterrupt, pHcdContext);
    }
    if (pHcdContext->Device.InitStateMask & SDHC_HW_INIT) {
        HcdDeinitialize(pHcdContext);
    }
    
    pHcdContext->Device.InitStateMask = 0;
    
        /* free mapped registers */
    if (pHcdContext->Device.ControlRegs.pMapped != NULL) {
        UnmapAddress(&pHcdContext->Device.ControlRegs);
        pHcdContext->Device.ControlRegs.pMapped = NULL;
    }
    
    if (pHcdContext->Device.GpioRegs.pMapped != NULL) {
        UnmapAddress(&pHcdContext->Device.GpioRegs);
        pHcdContext->Device.GpioRegs.pMapped = NULL;
    }


    if (pHcdContext->Device.pDmaDescriptorBuffer != NULL) {
        dma_free_coherent(&pHcdContext->Device.pBusDevice->dev, 
                          PXA_DESCRIPTOR_BUFFER_SIZE, 
                          pHcdContext->Device.pDmaDescriptorBuffer, 
                          pHcdContext->Device.DmaDescriptorPhys);
        pHcdContext->Device.pDmaDescriptorBuffer = NULL;
    }
    
    if (pHcdContext->Device.pDmaCommonBuffer != NULL) {
        dma_free_coherent(&pHcdContext->Device.pBusDevice->dev, 
                          PXA_DMA_COMMON_BUFFER_SIZE, 
                          pHcdContext->Device.pDmaCommonBuffer, 
                          pHcdContext->Device.DmaCommonBufferPhys);
        pHcdContext->Device.pDmaCommonBuffer = NULL;
    }
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO PXA270 Local HCD: Remove\n"));
}

/*
 * MapAddress - maps I/O address
*/
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription) {
     
    if (request_mem_region(pMap->Raw, pMap->Length, pDescription) == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Local HCD: MapAddress - memory in use\n"));
        return -EBUSY;
    }
    pMap->pMapped = ioremap_nocache(pMap->Raw, pMap->Length);
    if (pMap->pMapped == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Local HCD: MapAddress - unable to map memory\n"));
        /* cleanup region */
        release_mem_region(pMap->Raw, pMap->Length);
        return -EFAULT;
    }
    return 0;
}

/*
 * UnmapAddress - unmaps the address 
*/
static void UnmapAddress(PSDHCD_MEMORY pMap) {
    iounmap(pMap->pMapped);
    release_mem_region(pMap->Raw, pMap->Length);
    pMap->pMapped = NULL;
}

static void DmaCompletionCallBack(int dma, void *devid, struct pt_regs *regs)
{
    PSDHCD_DRIVER_CONTEXT   pHct = (PSDHCD_DRIVER_CONTEXT)devid;
    SDIO_STATUS             status = SDIO_STATUS_SUCCESS;
    PSDREQUEST              pReq;
    UINT32                  dcsr;
    
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
        
    dcsr = DCSR(pHct->Device.DmaChannel);
    
    DBG_PRINT(PXA_TRACE_DATA, ("+SDIO PXA270 DMA Complete, DCSR :0x%X\n",dcsr));
    
    do {
        if (dcsr & DCSR_BUSERR) {
            DBG_PRINT(SDDBG_ERROR, ("  DCSR Bus Error\n"));
            status = SDIO_STATUS_DEVICE_ERROR;
        } else if (dcsr & DCSR_ENDINTR) {
            DBG_PRINT(PXA_TRACE_DATA, ("   DMA complete\n"));        
        } else if (dcsr & DCSR_STOPSTATE) {
            DBG_PRINT(PXA_TRACE_DATA, ("   DMA stopped\n"));
        } else {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270  Unhandled DMA status : 0x%X \n",dcsr)); 
            status = SDIO_STATUS_DEVICE_ERROR; 
        }
        
            /* stop DMA and clear interrupt sources */    
        DCSR(pHct->Device.DmaChannel) = DCSR_ENDINTR | DCSR_BUSERR;    
        
        if (NULL == pReq) {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_PENDING;
            break;    
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }       
        
        if (pHct->DmaType != PXA_DMA_COMMON_BUFFER) { 
            break;
        }
            /* handle common buffer case */
        if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* read DMA */
            pReq->DataRemaining -= pHct->Device.LastRxCopy;
                /* copy from common buffer */
            memcpy(pReq->pHcdContext, pHct->Device.pDmaCommonBuffer,pHct->Device.LastRxCopy);
                /* advance buffer position */
            pReq->pHcdContext = (PUINT8)pReq->pHcdContext + pHct->Device.LastRxCopy;
            DBG_PRINT(PXA_TRACE_DATA, 
                  ("SDIO PXA270 DMA data remaining RX: %d \n",pReq->DataRemaining));
        }
        
        if (pReq->DataRemaining) {
                /* setup next common buffer */
            status = SetUpPXADMA(pHct, 
                                 pReq, 
                                 pHct->Device.pDmaCompletion,
                                 pHct->Device.pContext);
            if (SDIO_SUCCESS(status) && IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                 DBG_PRINT(PXA_TRACE_DATA, 
                    ("SDIO PXA270 DMA data remaining TX: %d \n", pReq->DataRemaining));    
            }
        }
        
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) {
            /* call completion callback */
        pHct->Device.pDmaCompletion(pHct->Device.pContext, status, TRUE);        
    }
    
    DBG_PRINT(PXA_TRACE_DATA, ("-SDIO PXA270 DMA Complete\n"));   
}

static void DumpDescriptors(struct pxa_dma_desc *pDesc,
                            INT                 DescCnt)
{
    INT i;
    DBG_PRINT(SDDBG_WARN, ("SDIO PXA270 Dumping DMA Descriptors at 0x%X :\n",(UINT32)pDesc));
     
    for (i = 0; i < DescCnt; i++,pDesc++) {
        DBG_PRINT(SDDBG_WARN, (" Entry %d \n",i));
        DBG_PRINT(SDDBG_WARN, ("     DSADR: 0x%X \n", pDesc->dsadr));
        DBG_PRINT(SDDBG_WARN, ("     DTADR: 0x%X \n", pDesc->dtadr));
        DBG_PRINT(SDDBG_WARN, ("     DCMD : 0x%X \n", pDesc->dcmd));
        DBG_PRINT(SDDBG_WARN, ("     DDADR: 0x%X \n",pDesc->ddadr));
    } 
}   
        
/* DMA setup */
SDIO_STATUS SetUpPXADMA(PSDHCD_DRIVER_CONTEXT    pHct, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext)
{
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    UINT32      dmaCommand;        
    struct pxa_dma_desc *pDesc;
    INT         i;
    INT         descCnt;
    PSDDMA_DESCRIPTOR   pSgItem;
    UINT32      dmaTransferLength;
    UINT32      thisDescrLength;
    
    pHct->Device.pDmaCompletion = pCompletion;
    pHct->Device.pContext = pContext;
    
    DBG_ASSERT(pReq->DataRemaining != 0);
    
        /* disable any existing channel mappings */
    DRCMRRXMMC = 0;
    DRCMRTXMMC = 0;
    
        /* make sure DMA is disabled */
    DCSR(pHct->Device.DmaChannel) = 0;
        /* get ptr to descriptor memory area */
    pDesc = (struct pxa_dma_desc *)pHct->Device.pDmaDescriptorBuffer; 
    descCnt = 0;
        /* clear descriptors */
    memset(pDesc, 0, (sizeof(struct pxa_dma_desc))*MAX_DMA_DESCRIPTORS);
    
    if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            /* write DMA, target is SDIO TX FIFO, source is memory*/
        dmaCommand = DCMD_FLOWTRG | DCMD_INCSRCADDR;
            /* map DMA channel to TX DMA request line */
        DRCMRTXMMC = pHct->Device.DmaChannel | DRCMR_MAPVLD;        
    } else {
            /* read DMA, target is memory, source is SDIO RX FIFO*/
        dmaCommand = DCMD_FLOWSRC| DCMD_INCTRGADDR;
            /* map DMA channel to RX DMA request line */
        DRCMRRXMMC = pHct->Device.DmaChannel | DRCMR_MAPVLD;
    }
        /* try bursting and set width to 1 byte, enable IRQ at end of DMA */
    dmaCommand |= DCMD_WIDTH1 | DCMD_BURST32;
 
    do {
        if (pHct->DmaType == PXA_DMA_COMMON_BUFFER) {
            UINT32 dataCopy;
            
            dataCopy = min(pReq->DataRemaining,(UINT32)PXA_DMA_COMMON_BUFFER_SIZE);
            descCnt = 1;            
                /* just one scatter gather list for the common buffer */
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* write DMA */
                pReq->DataRemaining -= dataCopy;
                    /* copy to common buffer */
                memcpy(pHct->Device.pDmaCommonBuffer, pReq->pHcdContext,dataCopy);
                pReq->pHcdContext = (PUINT8)pReq->pHcdContext + dataCopy;
                pDesc[0].dsadr = pHct->Device.DmaCommonBufferPhys;
                pDesc[0].dtadr = PXA_MMC_CONTROLLER_BASE_ADDRESS + MMC_TXFIFO_REG;    
            } else {
                    /* read DMA , just save the amount we need to copy */
                pHct->Device.LastRxCopy = dataCopy;
                pDesc[0].dsadr = PXA_MMC_CONTROLLER_BASE_ADDRESS + MMC_RXFIFO_REG;
                pDesc[0].dtadr = pHct->Device.DmaCommonBufferPhys;
            }
                /* only 1 descriptor, so generate END interrupt */
            dmaCommand |= DCMD_ENDIRQEN;
                /* set command and length */
            pDesc[0].dcmd = dmaCommand | dataCopy;
            pDesc[0].ddadr = DDADR_STOP;  
                /* we're done */
            break;      
        }
        
            /* scatter gather version */
        DBG_ASSERT(pHct->DmaType == PXA_DMA_SCATTER_GATHER);
            /* get the start of the list */
        pSgItem = (PSDDMA_DESCRIPTOR)pReq->pDataBuffer;
        dmaTransferLength = pReq->DataRemaining; 
        descCnt = pReq->DescriptorCount;
        
        dma_map_sg(pHct->Hcd.pDevice, 
                   pSgItem, 
                   descCnt, 
                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);           
        pHct->Device.DmaSgMapped = TRUE;
                
        for (i = 0; i < descCnt; i++,pDesc++,pSgItem++) {
            DBG_ASSERT(sg_dma_len(pSgItem) <= PXA_MAX_BYTES_PER_DESCRIPTOR);
            
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* write DMA */
                pDesc->dsadr = sg_dma_address(pSgItem);
                pDesc->dtadr = PXA_MMC_CONTROLLER_BASE_ADDRESS + MMC_TXFIFO_REG;    
            } else {
                    /* read DMA */
                pDesc->dsadr = PXA_MMC_CONTROLLER_BASE_ADDRESS + MMC_RXFIFO_REG;
                pDesc->dtadr = sg_dma_address(pSgItem);
            }
               
            if (i == (descCnt - 1)) {
                    /* last one */
                pDesc->ddadr = DDADR_STOP; 
                    /* trigger DMA interrupt when the last one is finished */
                dmaCommand |= DCMD_ENDIRQEN;
                    /* handle the last one in a special way */
                thisDescrLength = min(dmaTransferLength,sg_dma_len(pSgItem));
            } else {
                    /* chain to the next one */
                pDesc->ddadr =  pHct->Device.DmaDescriptorPhys + 
                                ((i + 1) * (sizeof(struct pxa_dma_desc))); 
                thisDescrLength = sg_dma_len(pSgItem); 
            }
            
                /* set command and length */
            pDesc->dcmd = dmaCommand | thisDescrLength;   
            dmaTransferLength -= thisDescrLength;  
        }   
        
    } while (FALSE);
    
    if (SDIO_SUCCESS(status)) {
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_DATA) {
            DumpDescriptors((struct pxa_dma_desc *)pHct->Device.pDmaDescriptorBuffer,
                            descCnt); 
        } 
        DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Starting DMA (Descriptors at Phys: 0x%X)\n",pHct->Device.DmaDescriptorPhys));
            /* set descriptor start address */
        DDADR(pHct->Device.DmaChannel) = pHct->Device.DmaDescriptorPhys; 
        DCSR(pHct->Device.DmaChannel) = DCSR_RUN;
    }
        
         
    return status;  
}

void SDCancelDMATransfer(PSDHCD_DRIVER_CONTEXT pHct)
{
    DCSR(pHct->Device.DmaChannel) = 0;
        /* wait for DMA channel to stop */
    while (!(DCSR(pHct->Device.DmaChannel) & DCSR_STOPSTATE)) {};
}
    
void CompleteRequestSyncDMA(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq, BOOL FromIsr)
{
    ULONG  flags;
   
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
         /* make sure DMA has stopped and interrupt sources are cleared */
    DCSR(pHct->Device.DmaChannel) = DCSR_ENDINTR | DCSR_BUSERR;  

        /* clean up SG mapping */
    if (pHct->Device.DmaSgMapped) {    
        pHct->Device.DmaSgMapped = FALSE; 
        dma_unmap_sg(pHct->Hcd.pDevice, 
                     (PSDDMA_DESCRIPTOR)pReq->pDataBuffer, 
                     pReq->DescriptorCount,
                     IS_SDREQ_WRITE_DATA(pReq->Flags) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    }  
    
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
    
    QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE); 
}

/* MMC controller interrupt routine */
static irqreturn_t hcd_mmc_irq(int irq, void *context, struct pt_regs * r)
{
        /* call OS independent ISR */
    if (HcdMMCInterrupt((PSDHCD_DRIVER_CONTEXT)context)) {
        return IRQ_HANDLED;
    } else {
        return IRQ_NONE;
    }    
}

#ifdef USE_CARD_DETECT_HW
/* card detect insert interrupt request */
static irqreturn_t hcd_card_detect_insert_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("SDIO PXA270 Card Detect Insert Interrupt, queueing work item \n"));
        /* just queue the work item to debounce the pin */
    QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_CARD_DETECT);
    return IRQ_HANDLED;
}

/* card detect remove interrupt request */
static irqreturn_t hcd_card_detect_remove_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("SDIO PXA270 Card Detect Remove Interrupt, queueing work item \n"));
        /* just queue the work item to debounce the pin */
    QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_CARD_DETECT);
    return IRQ_HANDLED;
}
#endif
/*
 * QueueEventResponse - queues an event in a process context back to the bus driver
 * 
*/
SDIO_STATUS QueueEventResponse(PSDHCD_DRIVER_CONTEXT pHcdContext, INT WorkItemID)
{
    struct work_struct *work;
    
    switch (WorkItemID) {
        case WORK_ITEM_IO_COMPLETE:
            work = &iocomplete_work;
            break;
#ifdef USE_CARD_DETECT_HW
        case WORK_ITEM_CARD_DETECT:
            work = &carddetect_work;
            break;
#endif
        case WORK_ITEM_SDIO_IRQ:
            work = &sdioirq_work;
            break;
        default:
            DBG_ASSERT(FALSE);
            return SDIO_STATUS_ERROR;
            break;  
    }
    
    if (schedule_work(work) > 0) {
        return SDIO_STATUS_SUCCESS;
    } else {
        return SDIO_STATUS_PENDING;
    }
}

/*
 * hcd_iocomplete_wqueue_handler - the work queue for io completion
*/
static void hcd_iocomplete_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    
    SDIO_HandleHcdEvent(&pHcdContext->Hcd, EVENT_HCD_TRANSFER_DONE);
}

#ifdef USE_CARD_DETECT_HW
/*
 * hcd_carddetect_handler - the work queue for card detect debouncing
*/
static void hcd_carddetect_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    HCD_EVENT event;
    
    event = EVENT_HCD_NOP;
    //PXA_TRACE_CARD_INSERT
    
    DBG_PRINT(SDDBG_TRACE, ("+ SDIO PXA270 Card Detect Work Item \n"));
    
    if (!pHcdContext->CardInserted) { 
        DBG_PRINT(SDDBG_TRACE, ("Delaying to debounce card... \n"));
            /* sleep for slot debounce if there is no card */
        msleep(PXA_SLOT_DEBOUNCE_MS);
    }
                
        /* check board status pin */
    if (IsCardInserted(pHcdContext)) {
        if (!pHcdContext->CardInserted) {  
            pHcdContext->CardInserted = TRUE;
            event = EVENT_HCD_ATTACH;
            DBG_PRINT(SDDBG_TRACE, (" Card Inserted! \n"));
                /* disable insert */
            disable_irq(pHcdContext->Device.CardInsertInterrupt); 
                /* enable remove */
            enable_irq(pHcdContext->Device.CardRemoveInterrupt);
        } else {
            DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already inserted card! \n"));
        }
    } else { 
        if (pHcdContext->CardInserted) {  
            event = EVENT_HCD_DETACH;
            pHcdContext->CardInserted = FALSE; 
            DBG_PRINT(SDDBG_TRACE, (" Card Removed! \n"));
                /* disable remove */
            disable_irq(pHcdContext->Device.CardRemoveInterrupt);
                /* enable insert */
            enable_irq(pHcdContext->Device.CardInsertInterrupt);
        } else {
            if (pHcdContext->Device.StartUpCheck) {
                pHcdContext->Device.StartUpCheck = FALSE;    
                DBG_PRINT(SDDBG_TRACE, ("No card at power up. \n"));
            } else {
                DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already removed card! \n"));
            }
        }
    }
    
    if (event != EVENT_HCD_NOP) {
        SDIO_HandleHcdEvent(&pHcdContext->Hcd, event);
    }
    
    DBG_PRINT(SDDBG_TRACE, ("- SDIO PXA270 Card Detect Work Item \n"));
}


BOOL IsCardInserted(PSDHCD_DRIVER_CONTEXT pHct)
{
#ifdef CONFIG_MACH_SANDGATE2P 
        /* check board status FPGA, active low */
    return !(SG2P_STATUS & SG2P_STATUS_nSD_DETECT);  
#elif defined(CONFIG_MACH_SANDGATE2G)
	return !(SG2G_STATUS & SG2G_STATUS_MMC_DETECT);
#elif defined(CONFIG_MACH_SANDGATE2)
	return !(SG2_STATUS & SG2_STATUS_MMC_DETECT);
#else 
#error "Must define card insertion for PXA270 HCD"
#endif 
}

#endif

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
*/
static void hcd_sdioirq_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO PXA270: hcd_sdioirq_wqueue_handler \n"));
    SDIO_HandleHcdEvent(&pHcdContext->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  UnmaskMMCIrq - Un mask an MMC interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes:  
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void UnmaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr)
{
    ULONG  flags;
    UINT32 ints;
   
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    ints = READ_MMC_REG(pHct, MMC_I_MASK_REG);
    ints &= ~Mask;
    WRITE_MMC_REG(pHct, MMC_I_MASK_REG, ints);
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MaskMMCIrq - Mask MMC interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void MaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr)
{
    UINT32 ints;
    ULONG  flags;
    
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    ints = READ_MMC_REG(pHct, MMC_I_MASK_REG);
    ints |= Mask;
    WRITE_MMC_REG(pHct, MMC_I_MASK_REG, ints);  
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    } 
}

void ModifyCSForSPIIntDetection(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable)
{
    if (Enable) {    
            /* set pin level low to keep CS0 line low all the time, this is required
             * for some cards to assert their SDIO interrupt line , set the pin register low
             * before switching functions so that it does not glitch*/
        WRITE_GPIO_REG(pHcdContext,GPIO_GPCR0,GPIO_bit(GPIO8_MMCCS0));  
            /* set GPIO8 for output mode, ALTFN 0, general purpose I/O */
        pxa_gpio_mode(GPIO8_MMCCS0 | GPIO_OUT);                 
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 SPI Mode - CS0 driven low for interrupt detect \n"));
    } else {       
            /* switch mode back to normal CS0 operation */
        pxa_gpio_mode(GPIO8_MMCCS0_MD); 
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 SPI Mode - normal CS0 operation \n"));
    }    
}


void SlotPowerOnOff(PSDHCD_DRIVER_CONTEXT pHct , BOOL On)
{ 
#ifdef CONFIG_MACH_SANDGATE2P 
    if (On) {
        SG2P_BCR |= SG2P_BCR_SD_PWR_ON;  
    } else {
        SG2P_BCR &= ~SG2P_BCR_SD_PWR_ON;     
    } 
#elif defined(CONFIG_MACH_SANDGATE2G)
	if (On) {
		sandgate2_bcr |= SG2G_BCR1_MMC_PWR_ON;
        SG2G_BCR1 = sandgate2_bcr;
    } else {
		sandgate2_bcr &= ~SG2G_BCR1_MMC_PWR_ON;
        SG2G_BCR1 = sandgate2_bcr;
    } 
#elif defined(CONFIG_MACH_SANDGATE2)
	if (On) {
		sandgate2_bcr |= SG2_BCR_MMC_PWR_ON;
        SG2_BCR = sandgate2_bcr;
    } else {
		sandgate2_bcr &= ~SG2_BCR_MMC_PWR_ON;
        SG2_BCR = sandgate2_bcr;
    } 

#else
#error "Must define slot power on/off for PXA270 HCD"
#endif 
}

BOOL IsSlotWPSet(PSDHCD_DRIVER_CONTEXT pHct)
{
#ifdef CONFIG_MACH_SANDGATE2P 
    return SG2P_STATUS & SG2P_STATUS_SD_WP;  
#elif defined(CONFIG_MACH_SANDGATE2G)
	return SG2G_STATUS & SG2G_STATUS_MMC_WP;
#elif defined(CONFIG_MACH_SANDGATE2)
	return SG2_STATUS & SG2_STATUS_MMC_WP;
#else
    return 0;
#endif
} 


/*
 * module init
*/
static int __init sdio_local_hcd_init(void) {
    SDIO_STATUS status;
    
    REL_PRINT(SDDBG_TRACE, ("SDIO PXA270 Local HCD: loaded\n"));
   
    status = SDIO_BusAddOSDevice(&HcdContext.Device.Dma, 
                                 &HcdContext.Device.HcdDriver, 
                                 &HcdContext.Device.HcdDevice);
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Local HCD: sdio_local_hcd_init exit\n"));
    return SDIOErrorToOSError(status);
}

/*
 * module cleanup
*/
static void __exit sdio_local_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SDIO PXA270 Local HCD: unloaded\n"));
    SDIO_BusRemoveOSDevice(&HcdContext.Device.HcdDriver, &HcdContext.Device.HcdDevice);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO PXA270 Local HCD: leave sdio_local_hcd_cleanup\n"));
}

// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_local_hcd_init);
module_exit(sdio_local_hcd_cleanup);

