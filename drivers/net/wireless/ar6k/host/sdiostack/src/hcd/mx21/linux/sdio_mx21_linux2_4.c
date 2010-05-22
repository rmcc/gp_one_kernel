// Copyright (c) 2006 Atheros Communications Inc.
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
@file: sdio_mx21_linux2_4.c

@abstract: Linux MX21 Local Bus SDIO Host Controller Driver

#notes: includes resource setup (DMA and I/O)
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#include <ctsystem.h>
#include "../sdio_mx21.h"
#include <linux/fs.h>
#include <linux/ioport.h> 
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/arch/mx2.h>
#define GPIO_PTE_GIUS    __REG32(0x10015420)  
#define GPIO_PTE_GPR     __REG32(0x10015438) 
#define GPIO_PTE_PUEN    __REG32(0x10015440)  
#include <asm/arch/clk.h>
#include <asm/arch/sctl.h>
#include <asm/arch/dma.h>
#include <asm/arch/dmac.h>

static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription);
static void UnmapAddress(PSDHCD_MEMORY pMap);
static void hcd_irq_handler(int irq, void *context, struct pt_regs * r);
static void DmaCompletionCallBack(PSDHCD_DRIVER_CONTEXT pHct, int DMAStatus);

extern int perclkfreq_get(int);
extern unsigned int hclkfreq_get(void);
 
void DumpDmaInfo(PSDHCD_DRIVER_CONTEXT pHct) 
{
    mx_dump_dma_register(pHct->Device.DmaChannel);    
}

void DisableHcdInterrupt(PSDHCD_DRIVER_CONTEXT pHct, BOOL FromIsr)
{
    ULONG  flags = 0;
     
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    
    if (!pHct->Device.LocalIrqDisabled) {
        pHct->Device.LocalIrqDisabled = TRUE;
        disable_irq(pHct->Device.ControllerInterrupt);
    }
    
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
    
}

void EnableHcdInterrupt(PSDHCD_DRIVER_CONTEXT pHct, BOOL FromIsr)
{
    ULONG  flags = 0;
     
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    
    if (pHct->Device.LocalIrqDisabled) {
        pHct->Device.LocalIrqDisabled = FALSE;
        enable_irq(pHct->Device.ControllerInterrupt);
    }
    
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
    
}

PSDHCD_DRIVER_CONTEXT g_Hcd1Context = NULL;
PSDHCD_DRIVER_CONTEXT g_Hcd2Context = NULL;

static void DmaCallBackHCD1(void *arg) 
{
   DBG_ASSERT(g_Hcd1Context != NULL);
   DmaCompletionCallBack(g_Hcd1Context,*((int *)arg));
}

static void DmaCallBackHCD2(void *arg) 
{
   DBG_ASSERT(g_Hcd2Context != NULL);
   DmaCompletionCallBack(g_Hcd2Context,*((int *)arg));
}

#define SD1_CLK_PIN (1 << 23)
#define SD1_CMD_PIN (1 << 22)
#define SD1_D3_PIN  (1 << 21)
#define SD1_D2_PIN  (1 << 20)
#define SD1_D1_PIN  (1 << 19)
#define SD1_D0_PIN  (1 << 18)

SDIO_STATUS InitMX21(PSDHCD_DRIVER_CONTEXT pHct)
{
    SDIO_STATUS status = SDIO_STATUS_NO_RESOURCES;
    INT err;
    UINT32 gpioPinMask;
    UINT32   hclk;
    
    do {   
        pHct->Device.DmaChannel = -1;
        pHct->Hcd.pDmaDescription = NULL;
        
        if (SDHC_CONTROLLER1_BASE_ADDRESS == pHct->Device.ControlRegs.Raw) {
            pHct->Device.ControllerInterrupt = INT_SDHC1;  
            g_Hcd1Context = pHct;
        } else if (SDHC_CONTROLLER2_BASE_ADDRESS == pHct->Device.ControlRegs.Raw) {
            pHct->Device.ControllerInterrupt = INT_SDHC2;  
            g_Hcd2Context = pHct; 
        } else {
            DBG_ASSERT(FALSE);
            break;    
        }
         
        hclk = hclkfreq_get();
        pHct->Device.DmaHclkErrata = hclk * 216; /* multiply by 0.216 */
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - HCLK:%d hz, Minimum SD Clock For Errata: %d Hz\n",
                                hclk*1000, pHct->Device.DmaHclkErrata));
                                
        
        
            /* setup all SD interface pins */
        gpioPinMask = SD1_CLK_PIN | SD1_CMD_PIN | SD1_D3_PIN | SD1_D2_PIN | 
                      SD1_D1_PIN | SD1_D0_PIN;
                      
            /* clear GPIO in-use bits for the SD interface */
        GPIO_PTE_GIUS &= ~gpioPinMask;
            /* clear primary function bits to use SD interface */
        GPIO_PTE_GPR &= ~gpioPinMask;
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - pin config mask:0x%X \n",
                                gpioPinMask));
            /* pull up these pins */
        gpioPinMask = SD1_CMD_PIN | SD1_D2_PIN | SD1_D1_PIN | SD1_D0_PIN | SD1_D3_PIN;                  
        GPIO_PTE_PUEN |= gpioPinMask;
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - pin pullup mask:0x%X \n",
                                gpioPinMask));
            /* disable these pullups to reduce power */
        gpioPinMask = SD1_CLK_PIN;                  
        GPIO_PTE_PUEN &= ~gpioPinMask;
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - pin pullup disables:0x%X \n",
                                gpioPinMask));
                                
            /* enable SDHC1 clock */
        _reg_CLK_PCCR0 |= CLK_PCCR0_SDHC1_EN;
        
         DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - DSCR1 :0x%X \n",_reg_SCTL_DSCR1));    
            /* according to AP NOTE AN2906, the SD pin drive strength is configured by DS_SLOW1 field
             * of DSCR1 */
        _reg_SCTL_DSCR1 &= ~SCTL_DSCR1_DS_SLOW1(7);
        _reg_SCTL_DSCR1 |= SCTL_DSCR1_DS_SLOW1(7); /* set for 12 mA drive strength */
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - New DSCR1 :0x%X \n",_reg_SCTL_DSCR1)); 
               
            /* get the peripheral clock rate for the controller and convert to Mhz */
        pHct->Device.PeripheralClockRate = perclkfreq_get(2) * 1000;
                      
            /* map the devices memory addresses */
        err = MapAddress(&pHct->Device.ControlRegs, "SDIOMX21");
        if (err < 0) {
                /* couldn't map the address */
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Local HCD: Probe - unable to map memory\n"));
            break; 
        }
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - HCD IRQ:%d \n",
                                pHct->Device.ControllerInterrupt));
            
            /* map the controller interrupt */
        err = request_irq (pHct->Device.ControllerInterrupt, hcd_irq_handler, 0,
                           pHct->pDescription, pHct);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - unable to map HC interrupt \n"));
            break;
        }
        
        pHct->Device.InitStateMask |= SDHC_INTERRUPT_INIT;
            
        pHct->Device.DmaChannel = mx_request_dma(-1, pHct->Hcd.pName);
        if (pHct->Device.DmaChannel < 0) {    
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - failed to allocate DMA channel, switching to PIO mode only \n"));             
        } else {    
            pHct->Device.InitStateMask |= SDHC_DMA_ALLOCATED;  
            pHct->Hcd.pDmaDescription = &pHct->Device.Dma;  
            DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - DMA Channel:%d \n",pHct->Device.DmaChannel));
        }   
        
        if (pHct->Device.DmaChannel < 0) {                       
            DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 - Not using DMA\n"));    
        } else {
            pHct->Device.DmaCommonBufferSize = SDHC_DMA_COMMON_BUFFER_SIZE;
               
            pHct->Device.pDmaCommonBuffer = (PUINT8)consistent_alloc(GFP_KERNEL | GFP_DMA | GFP_ATOMIC,
                                                                     pHct->Device.DmaCommonBufferSize, 
                                                                     &pHct->Device.DmaCommonBufferPhys); 
                                                                     
            if (NULL == pHct->Device.pDmaCommonBuffer) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - Failed to allocate common buffer for DMA \n"));  
                break;    
            }
            
                /* adjust max blocks per transfer for common buffer */
            pHct->Hcd.MaxBlocksPerTrans = 
                    pHct->Device.DmaCommonBufferSize/pHct->Hcd.MaxBytesPerBlock;
        } 
        
        SlotPowerOnOff(pHct, TRUE);   
           
        status = SDIO_STATUS_SUCCESS;
           
    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) {
        DeinitMX21(pHct);    
    }
    
    return status;
}

BOOL IsDMAAllowed(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
       
    if (pHct->Device.DmaChannel < 0) {
            /* no DMA at all */
        return FALSE;    
    }
    
    if (!pHct->SD4Bit) {
            /* errata does not affect 1 bit mode */
        return TRUE;
    }
    
    if (!pHct->Device.DmaHclkErrata) {
            /* no DMA HCLK errata in effect */
        return TRUE;    
    }
    
    if (SDHCD_GET_OPER_CLOCK(&pHct->Hcd) >= pHct->Device.DmaHclkErrata) {
            /* if the SD clock is operating above the minimum clock for this errata we are okay */
        return TRUE;  
    }
    
    if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            /* DMA HCLK errata only affects writes, allow all Read Operations */
        return TRUE;    
    }
    
    if (1 == pReq->BlockCount) {
            /* DMA HCLK only affects multi-block writes */
        return TRUE;    
    }
    
        /* otherwise we need to punt to PIO mode */
    DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 - current clock :%d, blocks:%d write operation must use PIO mode \n",
        SDHCD_GET_OPER_CLOCK(&pHct->Hcd),pReq->BlockCount));  
               
    return  FALSE;    
}

void DeinitMX21(PSDHCD_DRIVER_CONTEXT pHct)
{
        /* free DMA */
    if (pHct->Device.InitStateMask & SDHC_DMA_ALLOCATED) {
        mx_disable_dma(pHct->Device.DmaChannel);
        mx_free_dma(pHct->Device.DmaChannel);
        pHct->Device.InitStateMask &= ~SDHC_DMA_ALLOCATED;    
    }    
        /* free irqs */
    if (pHct->Device.InitStateMask & SDHC_INTERRUPT_INIT) {
        free_irq(pHct->Device.ControllerInterrupt, pHct);
        pHct->Device.InitStateMask &= ~SDHC_INTERRUPT_INIT;
    }
        
        /* free mapped registers */
    if (pHct->Device.ControlRegs.pMapped != NULL) {
        UnmapAddress(&pHct->Device.ControlRegs);
        pHct->Device.ControlRegs.pMapped = NULL;
    }
       
    if (pHct->Device.pDmaCommonBuffer != NULL) {
        consistent_free(pHct->Device.pDmaCommonBuffer, 
                        pHct->Device.DmaCommonBufferSize, 
                        pHct->Device.DmaCommonBufferPhys);
        pHct->Device.pDmaCommonBuffer = NULL;
    }    
    
    if (SDHC_CONTROLLER1_BASE_ADDRESS == pHct->Device.ControlRegs.Raw) {
        g_Hcd1Context = NULL;
    } else if (SDHC_CONTROLLER2_BASE_ADDRESS == pHct->Device.ControlRegs.Raw) {
        g_Hcd2Context = NULL; 
    } else {
        DBG_ASSERT(FALSE);
    }
    
}


/*
 * MapAddress - maps I/O address
*/
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription) {
     
    if (request_mem_region(pMap->Raw, pMap->Length, pDescription) == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Local HCD: MapAddress - memory in use\n"));
        return -EBUSY;
    }
    pMap->pMapped = ioremap_nocache(pMap->Raw, pMap->Length);
    if (pMap->pMapped == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Local HCD: MapAddress - unable to map memory\n"));
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


static void DmaCompletionCallBack(PSDHCD_DRIVER_CONTEXT pHct, int DMAStatus)
{
    
    SDIO_STATUS             status = SDIO_STATUS_SUCCESS;
    PSDREQUEST              pReq;
    int                     errors = 0;
    
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    
    DBG_ASSERT(pReq != NULL);
    
    DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("+SDIO MX21 DMA Complete, Status :0x%X\n",DMAStatus));
    
        /* check status */
    if (DMAStatus & DMA_BURST_TIMEOUT) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21  DMA Error:0x%X , DMA_BURST_TIMEOUT \n",DMAStatus)); 
        errors++;    
    }
    if (DMAStatus & DMA_TRANSFER_ERROR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21  DMA Error:0x%X , DMA_TRANSFER_ERROR \n",DMAStatus)); 
        errors++;    
    }
    if (DMAStatus & DMA_BUFFER_OVERFLOW) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21  DMA Error:0x%X , DMA_BUFFER_OVERFLOW \n",DMAStatus)); 
        errors++;    
    }
    if (DMAStatus & DMA_REQUEST_TIMEOUT) {        
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21  DMA Error:0x%X , DMA_REQUEST_TIMEOUT \n",DMAStatus)); 
        errors++;     
    }
    
    if (errors) {
        status = SDIO_STATUS_ERROR;    
        DumpDmaInfo(pHct);
    } else {    
        if (!(DMAStatus & DMA_DONE)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 unknown DMA Status :0x%X\n",DMAStatus));    
            status = SDIO_STATUS_PENDING;
        } else {
            DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("SDIO MX21 DMA Complete DONE OK\n"));    
        }
    }
    
    if (SDIO_SUCCESS(status)) {
            /* handle common buffer case */
        if (pHct->DmaType == SDHC_DMA_COMMON_BUFFER) { 
            if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* read DMA */
                pReq->DataRemaining -= pHct->Device.LastRxCopy;
                    /* copy from common buffer */
                memcpy(pReq->pHcdContext, pHct->Device.pDmaCommonBuffer,pHct->Device.LastRxCopy);
                    /* advance buffer position */
                pReq->pHcdContext = (PUINT8)pReq->pHcdContext + pHct->Device.LastRxCopy;
            }
            
            DBG_ASSERT(pReq->DataRemaining == 0);
                        
            #if 0
             /* it would have been nice to moving data but the 2.4.20 implementation of the MX21 DMA 
              * clears status and marks the channel inactive when we return from this callback! Thus
              * we cannot setup another transfer from within a callback */
            if (pReq->DataRemaining) {
                    /* setup next common buffer */
                status = SetUpHCDDMA(pHct, 
                                     pReq, 
                                     pHct->Device.pDmaCompletion,
                                     pHct->Device.pContext);
            }
            #endif
        }
    }
    
    if (status != SDIO_STATUS_PENDING) {
            /* call completion callback , this is called from an ISR context */
        pHct->Device.pDmaCompletion(pHct->Device.pContext, status, TRUE);        
    }
    
    DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("-SDIO MX21 DMA Complete\n"));  

}
        
BOOL mx21_map_sg(struct scatterlist *pSg, int Entries, BOOL ToDevice)
{
    int           i;
    
    for (i = 0; i < Entries; i++,pSg++) {
        if (pSg->address) {
            if (ToDevice) {
                    /* flush data to main memory for DMA to read from */
                consistent_sync(pSg->address,pSg->length,PCI_DMA_TODEVICE);
            } else {
                    /* invalidate pages in main memory that DMA is writing into */
                consistent_sync(pSg->address,pSg->length,PCI_DMA_FROMDEVICE);
            }
            /*** WARNING, virt_to_bus macro on MX21 kernel is very BROKEN 
             * pSg->dma_address = virt_to_bus(pSg->address); we can accomplish the
             * same thing by using page_to_phys and then adding the offset ***/
            
            pSg->dma_address = page_to_phys(pSg->page) + pSg->offset;
            pSg->dma_length =  pSg->length;
        } else {
            DBG_ASSERT(FALSE);  
            return FALSE;
        }
    }
    
    return TRUE;
}

void mx21_unmap_sg(struct scatterlist *pSg, int Entries, BOOL ToDevice)
{
    int           i;
        
    for (i = 0; i < Entries; i++,pSg++) {
        pSg->dma_address = 0;
        pSg->dma_length =  0;       
    }  
     
}

/* DMA setup */
SDIO_STATUS SetUpHCDDMA(PSDHCD_DRIVER_CONTEXT    pHct, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext)
{
    SDIO_STATUS         status = SDIO_STATUS_ERROR;    
    PSDDMA_DESCRIPTOR   pSgItem;
    int                 err;
    dma_request_t       dmareq;
    
    pHct->Device.pDmaCompletion = pCompletion;
    pHct->Device.pContext = pContext;
     
    DBG_ASSERT(pReq->DataRemaining != 0);
    
        /* make sure DMA is disabled */
    mx_disable_dma(pHct->Device.DmaChannel);
    ZERO_POBJECT(&dmareq);
            
    do {
        dmareq.arg = pHct;
            /* set DMA callback and source */
        if (SDHC_CONTROLLER1_BASE_ADDRESS == pHct->Device.ControlRegs.Raw) {
            dmareq.callback = DmaCallBackHCD1;
            dmareq.reqSrc = DMA_REQ_SDHC1;
        } else {
            dmareq.callback = DmaCallBackHCD2;  
            dmareq.reqSrc = DMA_REQ_SDHC2;
        }
        
        dmareq.dir = 0; /* incrementing addresses */
        dmareq.repeat = 0;        
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* write DMA, destination is FIFO, source is memory*/    
            dmareq.sourceType = DMA_TYPE_LINEAR;   /* main memory */
            dmareq.destType = DMA_TYPE_FIFO;       /* SD controller buffer access FIFO */
            dmareq.srcPortSize = DMA_MEM_SIZE_32;  /* memory can be accessed in 32 bit mode */
            dmareq.destPortSize = DMA_MEM_SIZE_16; /* must access in 16 bit mode */ 
            dmareq.destAddr = (PVOID)(pHct->Device.ControlRegs.Raw + SDHC_BUF_ACCESS_REG);
            /* source will be filled below */
        } else {           
            dmareq.destType = DMA_TYPE_LINEAR;      /* main memory */
            dmareq.sourceType = DMA_TYPE_FIFO;      /* SD controller buffer access FIFO */
            dmareq.destPortSize = DMA_MEM_SIZE_32;  /* memory can be accessed in 32 bit mode */
            dmareq.srcPortSize = DMA_MEM_SIZE_16;   /* must access in 16 bit mode */            
            dmareq.sourceAddr = (PVOID)(pHct->Device.ControlRegs.Raw + SDHC_BUF_ACCESS_REG); 
            /* destination will be filled below */
        }
        
        if (pHct->SD4Bit) {
            dmareq.burstLength = 0;     /* 64 byte */
        } else {
            dmareq.burstLength = 16;    /* 1 bit mode uses 16 byte */
        }
            
        if (pHct->DmaType == SDHC_DMA_COMMON_BUFFER) {
            UINT32 dataCopy;
            
            dataCopy = min(pReq->DataRemaining,(UINT32)pHct->Device.DmaCommonBufferSize);
            dmareq.count = dataCopy;  
                    
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* write DMA */
                pReq->DataRemaining -= dataCopy;
                    /* copy to common buffer */
                memcpy(pHct->Device.pDmaCommonBuffer, pReq->pHcdContext,dataCopy);
                pReq->pHcdContext = (PUINT8)pReq->pHcdContext + dataCopy;
                    /* source is memory */
                dmareq.sourceAddr = (PVOID)pHct->Device.DmaCommonBufferPhys;  
            } else {
                    /* read DMA , just save the amount we need to copy */
                pHct->Device.LastRxCopy = dataCopy;
                    /* destination is memory */
                dmareq.destAddr = (PVOID)pHct->Device.DmaCommonBufferPhys;
            }
            
            DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("SDIO MX21 Common Buffer DMA %s bytes:%d \n",
                      IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",dataCopy));
                       
            status = SDIO_STATUS_SUCCESS;
            break;      
        }
        
            /* scatter gather version */
        DBG_ASSERT(pHct->DmaType == SDHC_DMA_SCATTER_GATHER);
        
        if (pReq->DescriptorCount > 1) {
            DBG_ASSERT(FALSE);
            break;    
        }
        
        DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("SDIO MX21 SG DMA %s Descriptors:%d, \n",
                      IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",pReq->DescriptorCount));
                      
        pSgItem = (PSDDMA_DESCRIPTOR)pReq->pDataBuffer;
       
            /* map the scatter list */
        if (!mx21_map_sg(pSgItem, 
                         1, 
                         IS_SDREQ_WRITE_DATA(pReq->Flags) ? TRUE : FALSE)) {
            break; 
        }
                            
        pHct->Device.DmaSgMapped = TRUE;
        
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* write DMA */
            dmareq.sourceAddr = (PVOID)sg_dma_address(pSgItem);
        } else {
                /* read DMA */
            dmareq.destAddr = (PVOID)sg_dma_address(pSgItem);
        }

        dmareq.count = sg_dma_len(pSgItem);
        
        status = SDIO_STATUS_SUCCESS;  
              
    } while (FALSE);
    
    if (SDIO_SUCCESS(status)) {
        
        DBG_ASSERT(dmareq.count != 0);
        DBG_ASSERT(dmareq.sourceAddr != 0);
        DBG_ASSERT(dmareq.destAddr != 0);
        DBG_PRINT(SDHC_TRACE_DMA_DEBUG, ("SDIO MX21 SG DMA Mem-Addr (%s):0x%X, Length:%d \n",
                  IS_SDREQ_WRITE_DATA(pReq->Flags) ? "src":"dest",
                  IS_SDREQ_WRITE_DATA(pReq->Flags) ? (INT)dmareq.sourceAddr : (INT)dmareq.destAddr,
                  dmareq.count));
                                  
        err = mx_dma_set_config(pHct->Device.DmaChannel, &dmareq);
               
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 DMA Config Setup Failed :%d (channel:%d)\n",
                    err,pHct->Device.DmaChannel));
            status = SDIO_STATUS_ERROR;  
        } else {     
            
            if (DBG_GET_DEBUG_LEVEL() >= SDHC_TRACE_DMA_DEBUG) {
                DumpDmaInfo(pHct);    
            }
                /* start DMA */
            err = mx_dma_start(pHct->Device.DmaChannel);
            
            if (err < 0) {
                status = SDIO_STATUS_ERROR;
                DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 DMA Start Failed :%d (channel:%d)\n",
                    err,pHct->Device.DmaChannel));
            } else {                
                status = SDIO_STATUS_PENDING;
            }
            
        }
    }
         
    return status;  
}

void CompleteRequestSyncDMA(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq, BOOL FromIsr)
{
    ULONG  flags = 0;
  
        /* sync with DMA ISR */ 
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
        /* disable DMA */
    mx_disable_dma(pHct->Device.DmaChannel);
    
        /* clean up SG mapping */
    if (pHct->Device.DmaSgMapped) {    
        pHct->Device.DmaSgMapped = FALSE; 
        mx21_unmap_sg((PSDDMA_DESCRIPTOR)pReq->pDataBuffer, 
                      pReq->DescriptorCount,
                      IS_SDREQ_WRITE_DATA(pReq->Flags) ? TRUE : FALSE);
    }  

    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
    
    QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE); 
}


/* controller interrupt routine */
static void hcd_irq_handler(int irq, void *context, struct pt_regs * r)
{
        /* call OS independent ISR */
    HcdInterrupt((PSDHCD_DRIVER_CONTEXT)context);
}


#ifdef USE_CARD_DETECT_HW
/* card detect insert interrupt request */
static irqreturn_t hcd_card_detect_insert_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(SDHC_TRACE_CARD_INSERT, ("SDIO MX21 Card Detect Insert Interrupt, queueing work item \n"));
        /* just queue the work item to debounce the pin */
    QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_CARD_DETECT);
    return IRQ_HANDLED;
}

/* card detect remove interrupt request */
static irqreturn_t hcd_card_detect_remove_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(SDHC_TRACE_CARD_INSERT, ("SDIO MX21 Card Detect Remove Interrupt, queueing work item \n"));
        /* just queue the work item to debounce the pin */
    QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_CARD_DETECT);
    return IRQ_HANDLED;
}
#endif


void SlotPowerOnOff(PSDHCD_DRIVER_CONTEXT pHct , BOOL On)
{ 
    // TODO
}

BOOL IsSlotWPSet(PSDHCD_DRIVER_CONTEXT pHct)
{                               
        /* SD slot switch in on the expansion I/O bus */
    if (_reg_EXPANDED_IO & 0x1) {
        return TRUE;    
    }
    return FALSE;
} 
