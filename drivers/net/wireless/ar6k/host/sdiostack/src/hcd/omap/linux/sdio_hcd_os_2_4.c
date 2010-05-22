// Copyright (c) 2004-2006 Atheros Communications Inc.
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
@file: sdio_hcd_os_2_4.c

@abstract: Linux OMAP native SDIO Host Controller Driver 2.4 implementation

#notes: includes DMA and initialization code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#include "../../../include/ctsystem.h"
#include "../sdio_omap_hcd.h"
#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <asm/mach-types.h>
#include <asm/arch/dma.h>
#include <asm/arch/irq.h>
#include <asm/arch/irq.h>

extern INT gpiodebug;
extern INT noDMA;
extern SDHCD_DRIVER_CONTEXT HcdContext;

#define READ_MODIFY_WRITE_AND(reg, value) \
{   UINT32 temp;                        \
    temp = _READ_DWORD_REG((reg));      \
    temp &=  value;                    \
    _WRITE_DWORD_REG((reg), temp);      \
}
#define READ_MODIFY_WRITE_OR(reg, value) \
{   UINT32 temp;                        \
    temp = _READ_DWORD_REG((reg));      \
    temp |=  value;                    \
    _WRITE_DWORD_REG((reg), temp);      \
}

#define SET_PIN_MUX(reg,value,shift)     \
{   UINT32 temp;                          \
    temp = _READ_DWORD_REG((reg));       \
    temp &=  ~(0x7 << (shift));          \
    temp |= (value) << (shift);          \
    _WRITE_DWORD_REG((reg), temp);       \
}

void ToggleGPIOPin(PSDHCD_DEVICE pDevice, INT PinNo)
{
    UINT32 dummy;
    UINT32 pinMask;
    INT i;
     
    if (!gpiodebug) {
        return;
    }
   
    switch (PinNo) {
        case DBG_GPIO_PIN_1:
            pinMask = (1 << 8);
            break;  
        case DBG_GPIO_PIN_2:
            pinMask = (1 << 11);
            break;  
        default:
            pinMask = 0;
            break;
    }
   
   _WRITE_DWORD_REG(GPIO_SET_DATAOUT_REG, pinMask); 
    for (i = 0; i < 2; i++) {
            /* some dummy cycles to extend the pulse */
        dummy = _READ_DWORD_REG(GPIO_REVISION_REG);
    }    
    _WRITE_DWORD_REG(GPIO_CLEAR_DATAOUT_REG, pinMask);
}

static void hcd_sdio_irq(int irq, void *context, struct pt_regs * r);


/*
 * unsetup the OMAP registers 
*/
void DeinitOmap(PSDHCD_DEVICE pDevice)
{
    if (pDevice->OSInfo.pDmaBuffer != NULL) {
        consistent_free(pDevice->OSInfo.pDmaBuffer, 
                        pDevice->OSInfo.CommonBufferSize, 
                        pDevice->OSInfo.hDmaBuffer);
        pDevice->OSInfo.pDmaBuffer = NULL;
    }
        /* disable clock */
    _WRITE_DWORD_REG(MOD_CONF_CTRL_0, _READ_DWORD_REG(MOD_CONF_CTRL_0) & ~((1 << 23) | (1 << 21)));
    
    if (pDevice->OSInfo.InitStateMask & SDIO_IRQ_INTERRUPT_INIT) {
        disable_irq(pDevice->OSInfo.Interrupt);
        free_irq(pDevice->OSInfo.Interrupt, pDevice);
    }   
     
}


void ClockEnable(PSDHCD_DEVICE pDevice, BOOL Enable)
{
        /* protect read-mod-write */
    cli();
    if (Enable) { 
        _WRITE_DWORD_REG(MOD_CONF_CTRL_0,
                     _READ_DWORD_REG(MOD_CONF_CTRL_0) | ((1 << 23) | (1 << 21)));
    } else {
        _WRITE_DWORD_REG(MOD_CONF_CTRL_0,
                     _READ_DWORD_REG(MOD_CONF_CTRL_0) & (~((1 << 23) | (1 << 21))));
    }
    sti();
}

                     
/*
 * setup the OMAP registers 
*/
SDIO_STATUS InitOmap(PSDHCD_DEVICE pDevice, UINT deviceNumber)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    ULONG       baseAddress;
    int         err;

        /* enable clock */
    _WRITE_DWORD_REG(MOD_CONF_CTRL_0,
                     _READ_DWORD_REG(MOD_CONF_CTRL_0) | ((1 << 23) | (1 << 21)));

    do {

        if (!noDMA) {
             
                /* allocate a DMA buffer large enough for the data buffers */
            pDevice->OSInfo.pDmaBuffer = consistent_alloc(GFP_KERNEL | GFP_DMA | GFP_ATOMIC,
                                      pDevice->OSInfo.CommonBufferSize, 
                                       &pDevice->OSInfo.hDmaBuffer);
                                                      
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO OMAP HCD: InitOmap - pDmaBuffer: 0x%X, hDmaBuffer: 0x%X Size:%d\n",
                                    (UINT)pDevice->OSInfo.pDmaBuffer , 
                                    (UINT)pDevice->OSInfo.hDmaBuffer,
                                    pDevice->OSInfo.CommonBufferSize));
           
            if (pDevice->OSInfo.pDmaBuffer == NULL) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: InitOmap - unable to get DMA buffer\n"));
                status = SDIO_STATUS_NO_RESOURCES;
                break;
            }
            
            pDevice->DmaCapable = TRUE;
                /* tell upper drivers that we support direct DMA from the request buffers */
            pDevice->Hcd.pDmaDescription = &HcdContext.Driver.Dma;                    
        }
    
#ifdef CONFIG_ARCH_OMAP1610 
        /* setup the clock and the mux */
         READ_MODIFY_WRITE_OR(MOD_CONF_CTRL_0, (5 << 21));
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_D, ~(7 << 12));
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_A, ~(7 << 21)); /*CLK*/
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_B, ~(7 << 0));  /*DAT0*/
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_A, ~(7 << 18)); /*DAT2*/
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_A, ~(7 << 24)); /*DAT1*/
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_A, ~(7 << 27)); /*CMD*/
         READ_MODIFY_WRITE_AND(FUNC_MUX_CTRL_10, ~(7 << 15)); /*DAT3*/
         if (gpiodebug) {
            UINT32 tempValue;
                /* enable GPIO 8*/
            SET_PIN_MUX(FUNC_MUX_CTRL_B, 0x0, 24);  
                /* enable GPIO 11*/
            SET_PIN_MUX(FUNC_MUX_CTRL_6, 0x0, 18); 
            tempValue = _READ_DWORD_REG(GPIO_DIRECTION_REG);
                /* set direction for output */
            tempValue &= ~(1 << 8);    
            tempValue &= ~(1 << 11);      
            _WRITE_DWORD_REG(GPIO_DIRECTION_REG, tempValue);            
            //ToggleGPIOPin(pDevice,DBG_GPIO_PIN_1);
            //ToggleGPIOPin(pDevice,DBG_GPIO_PIN_2);
        }
#else
#error "Pin MUX undefined for this architecture!"
#endif     
    
        baseAddress = OMAP_BASE_ADDRESS1;
        pDevice->OSInfo.Interrupt = INT_MMC_SDIO1;
        pDevice->OSInfo.DmaRxChannel = eMMCRx;
        pDevice->OSInfo.DmaTxChannel = eMMCTx;
        pDevice->OSInfo.DmaChannel = NULL;
            /* map the memory address for the control registers */
        pDevice->OSInfo.Address.pMapped = (PVOID)IO_ADDRESS(baseAddress);
        pDevice->OSInfo.Address.Raw = baseAddress;
        DBG_PRINT(OMAP_TRACE_CONFIG , 
             ("SDIO OMAP - InitOMAP 0x%X\n", (UINT)pDevice->OSInfo.Address.pMapped));
    
        pDevice->OSInfo.InitStateMask |= SDIO_BASE_MAPPED;
    
                /* map the controller interrupt, we map it to each device. 
                   Interrupts can be called from this point on */
        err = request_irq(pDevice->OSInfo.Interrupt, hcd_sdio_irq, 0,
                          "omap_sdio", pDevice);
                          
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: OmapInit, unable to map interrupt \n"));
            err = -ENODEV;
            status = SDIO_STATUS_NO_RESOURCES;
            break;
        }
        pDevice->OSInfo.InitStateMask |= SDIO_IRQ_INTERRUPT_INIT;

    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) {
        DeinitOmap(pDevice);    
    }
    return status;
}

#define FIFO_SYNC_BLOCK_SIZE 32
#define DMAREG_CSDP ((1 << 10) | (1 << 9) | (2 << 7) | (1 << 6) | 1)
#define DMAREG_CCR  ((1 << 12) | (1 << 10) | (1 << 5) | (1 << 6))
#define DMAREG_CICR ((1 << 5) | (1 << 1) | 1)
/* 2.4 */
static void setupOmapDmaTx(PSDHCD_DEVICE pDevice, int Length, DMA_ADDRESS TxDmaAddress)
{
   INT  fifoLen;
   
   UINT32 address = virt_to_phys((void *)pDevice->OSInfo.Address.pMapped) + OMAP_REG_MMC_DATA_ACCESS; 
   
   pDevice->OSInfo.DmaChannel->cdsa_l = address & 0xFFFF;
   pDevice->OSInfo.DmaChannel->cdsa_u = address >> 16;
   pDevice->OSInfo.DmaChannel->cssa_l =  TxDmaAddress & 0xFFFF;
   pDevice->OSInfo.DmaChannel->cssa_u =  TxDmaAddress >> 16;
   pDevice->OSInfo.DmaChannel->csdp = DMAREG_CSDP;
   pDevice->OSInfo.DmaChannel->ccr |= DMAREG_CCR;

   pDevice->OSInfo.DmaChannel->cicr = DMAREG_CICR;


   pDevice->OSInfo.DmaChannel->ccr2 = 0;
   pDevice->OSInfo.DmaChannel->lch_ctrl = 2;
    
   if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
            /* multiple of fifo size */
        pDevice->OSInfo.DmaChannel->cen = FIFO_SYNC_BLOCK_SIZE>>1;
        pDevice->OSInfo.DmaChannel->cfn = (Length/FIFO_SYNC_BLOCK_SIZE);
         fifoLen = 0xF;
     } else {
        if (Length < FIFO_SYNC_BLOCK_SIZE) {
            pDevice->OSInfo.DmaChannel->cen = Length>>1;
            pDevice->OSInfo.DmaChannel->cfn = 1;
            fifoLen = (Length>>1)-1;   
            fifoLen = (fifoLen < 0) ? 0 : fifoLen;                                       
        } else {
            if (Length == (8 * (Length/8))) {
                pDevice->OSInfo.DmaChannel->cen = 1;
                pDevice->OSInfo.DmaChannel->cfn = (Length+1)>>1;
                fifoLen = 0;                                       
            } else {
                pDevice->OSInfo.DmaChannel->cen = 1;
                pDevice->OSInfo.DmaChannel->cfn = (Length+1)>>1;
                fifoLen = 0;                                          
            }
        }
     }
     
     WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_TXDE |
                    ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK));

     /* start */
     pDevice->OSInfo.DmaChannel->ccr |= (1 << 7); 
}

/* 2.4 */
static void setupOmapDmaRx(PSDHCD_DEVICE pDevice, int Length, DMA_ADDRESS RxDmaAddress)
{
   INT  fifoLen;
   
   UINT32 address = virt_to_phys((void *)pDevice->OSInfo.Address.pMapped) + OMAP_REG_MMC_DATA_ACCESS; 
   
   pDevice->OSInfo.DmaChannel->csdp = (1 << 3) | (1 << 2) | 1; 
   pDevice->OSInfo.DmaChannel->ccr |= (1 << 14) | (1 << 10) | (1 << 5) | (1 << 6);

   pDevice->OSInfo.DmaChannel->cicr = (1 << 5) | (1 << 1) | 1 ;
   pDevice->OSInfo.DmaChannel->cssa_l = address & 0xffff;
   pDevice->OSInfo.DmaChannel->cssa_u = address >> 16;
   pDevice->OSInfo.DmaChannel->cdsa_l =  RxDmaAddress & 0xffff;
   pDevice->OSInfo.DmaChannel->cdsa_u =  RxDmaAddress >> 16;


   pDevice->OSInfo.DmaChannel->ccr2 = 0;
   pDevice->OSInfo.DmaChannel->lch_ctrl = 2;
    
   if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
            /* multiple of fifo size */
        pDevice->OSInfo.DmaChannel->cen = FIFO_SYNC_BLOCK_SIZE>>1;
        pDevice->OSInfo.DmaChannel->cfn = (Length/FIFO_SYNC_BLOCK_SIZE);
         fifoLen = 0xF;
     } else {
        if (Length < FIFO_SYNC_BLOCK_SIZE) {
            pDevice->OSInfo.DmaChannel->cen = Length>>1;
            pDevice->OSInfo.DmaChannel->cfn = 1;
            fifoLen = (Length>>1)-1;  
            fifoLen = (fifoLen < 0) ? 0 : fifoLen;                                       
        } else {
            if (Length == (8 * (Length/8))) {
                pDevice->OSInfo.DmaChannel->cen = 1;
                pDevice->OSInfo.DmaChannel->cfn = (Length+1)>>1;
                fifoLen = 0;                                 
            } else {
                pDevice->OSInfo.DmaChannel->cen = 1;
                pDevice->OSInfo.DmaChannel->cfn = (Length+1)>>1;
                fifoLen = 0;                                          
            }
        }
     }
     
     WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_RXDE |
                    ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK));

     /* start */
     pDevice->OSInfo.DmaChannel->ccr |= (1 << 7); 
}

#define OMAP_DMA_BLOCK_IRQ             0x20

BOOL PrepareSG(struct scatterlist *pSg, int Entries, BOOL ToDevice)
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
            /* virt_to_bus is broken on 2.4.20, 
             * pSg->dma_address = virt_to_bus(pSg->address); */
            pSg->dma_address = page_to_phys(pSg->page) + pSg->offset;
            pSg->dma_length =  pSg->length;
        } else {
            DBG_ASSERT(FALSE);
            return FALSE;
        }
    }
    
    return TRUE;
}

void SetupTXCommonBufferDMATransfer(PSDHCD_DEVICE pDevice, PSDREQUEST pReq)
{   
    UINT32 length;
        /* adjust length */
    length = min(pDevice->OSInfo.CommonBufferSize,
                 pReq->DataRemaining);
        /* copy to common buffer */
    memcpy(pDevice->OSInfo.pDmaBuffer, pReq->pHcdContext, length);
        /* adjust where we are */        
    pReq->pHcdContext = (PUCHAR)pReq->pHcdContext + length;
    pReq->DataRemaining -= length;
        /* setup this chunk */
    setupOmapDmaTx(pDevice, length, pDevice->OSInfo.hDmaBuffer);
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP TX Common Buffer DMA,  This Transfer: %d, Remaining:%d\n", 
        length,pReq->DataRemaining));
}

/* 2.4
 *  DMA transmit complete callback
*/
static void SD_DMACompleteCallback(PVOID pContext)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    SDIO_STATUS   status = SDIO_STATUS_PENDING;
    PSDREQUEST    pReq;
    int           dmaStatus;
    UINT32        length;
    
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
    
    do {
        
        if (pDevice->OSInfo.DmaChannel == NULL) {
             DBG_PRINT(SDDBG_WARN, ("SDIO OMAP unexpected callback\n"));
             break;     
        }
        
        dmaStatus = omap_dma_get_status(pDevice->OSInfo.DmaChannel);
        DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP DMA (%s) Complete Callback - status: %d, channelId: %d channel: %d\n", 
             IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
             (UINT)dmaStatus, 
             IS_SDREQ_WRITE_DATA(pReq->Flags) ? pDevice->OSInfo.DmaTxChannel :
             pDevice->OSInfo.DmaRxChannel, 
             (UINT)pDevice->OSInfo.DmaChannel));
        
        if (dmaStatus == DCSR_SYNC_SET) {
            /* only a synch int, ignore it */
            break;
        }
        
        if (NULL == pDevice->OSInfo.pTransferCompletion) {
            DBG_ASSERT(FALSE);
            break;    
        }
            /* handle errors */
        if (dmaStatus & DCSR_ERROR) {
            status = SDIO_STATUS_DEVICE_ERROR;
            break;
        }
        if (!(dmaStatus & OMAP_DMA_BLOCK_IRQ)) {
            status = SDIO_STATUS_DEVICE_ERROR;
            break;
        }
            
            /* no DMA errors */
        status = SDIO_STATUS_SUCCESS;
             
        if (OMAP_DMA_SG == pDevice->DmaMode) {
                /* nothing more to do */
            break;    
        }
        
            /* handle common buffer DMA */      
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            if (pReq->DataRemaining) {
                    /* send the next chunk */
                SetupTXCommonBufferDMATransfer(pDevice,pReq);
                status = SDIO_STATUS_PENDING;
                break;    
            }
        } else {
                /* copy RX Data from common buffer */
            memcpy(pReq->pHcdContext, pDevice->OSInfo.pDmaBuffer, pDevice->OSInfo.LastTransfer);
                /* adjust where we are */
            pReq->pHcdContext = (PUCHAR)pReq->pHcdContext + pDevice->OSInfo.LastTransfer;
            pReq->DataRemaining -= pDevice->OSInfo.LastTransfer;
                /* set up next transfer */
            length = min(pDevice->OSInfo.CommonBufferSize,
                         pReq->DataRemaining);
            if (length) {
                DBG_PRINT(OMAP_TRACE_DATA, 
                    ("SDIO OMAP RX Common Buffer DMA,  Pending Transfer: %d, Remaining:%d\n", 
                            length, pReq->DataRemaining));
                setupOmapDmaRx(pDevice, length, pDevice->OSInfo.hDmaBuffer);
                pDevice->OSInfo.LastTransfer = length;
                status = SDIO_STATUS_PENDING;
                break;
            }
        }
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) {
        omap_free_dma(pDevice->OSInfo.DmaChannel);
        pDevice->OSInfo.DmaChannel = NULL;
        pDevice->OSInfo.pTransferCompletion(pDevice->OSInfo.TransferContext, status, TRUE);
    }
    
}

SDIO_STATUS SetUpHCDDMA(PSDHCD_DEVICE            pDevice, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext)
{
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    SYSTEM_STATUS err;
    UINT32 length = pReq->DataRemaining;
    PSDDMA_DESCRIPTOR pDesc = NULL;
   
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("+SDIO OMAP SetUpHCDDMA: length: %d\n",length));
    
    do {
        
        if ((OMAP_DMA_COMMON == pDevice->DmaMode) &&
            (length & 0x1)) {
                /* DMA requires WORD alignment, tell caller to punt it to PIO mode */
            status = SDIO_STATUS_UNSUPPORTED;
            break;    
        }
        
        if (OMAP_DMA_SG == pDevice->DmaMode) {
                /* doing direct DMA */
            if  (pReq->DescriptorCount > 1) {
                DBG_ASSERT(FALSE);
                status = SDIO_STATUS_INVALID_PARAMETER;
                break;    
            }  
            
            pDesc = (PSDDMA_DESCRIPTOR)pReq->pDataBuffer;
            DBG_ASSERT(pDesc != NULL);
                /* map descriptor */
            if (!PrepareSG(pDesc, 
                           1, 
                           IS_SDREQ_WRITE_DATA(pReq->Flags) ? TRUE : FALSE)) {
                status = SDIO_STATUS_INVALID_PARAMETER;
                break;         
            }
        }
        
        pDevice->OSInfo.pTransferCompletion = pCompletion;
        pDevice->OSInfo.TransferContext = pContext;
    
        if (pDevice->OSInfo.DmaChannel != NULL) {
            DBG_PRINT(SDDBG_WARN, ("SDIO OMAP SetUpHCDDMA: **DMA still in use,  channel: %d\n",
                      (UINT)pDevice->OSInfo.DmaChannel));
            omap_free_dma(pDevice->OSInfo.DmaChannel);
        }
  
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {      
            err = omap_request_dma(pDevice->OSInfo.DmaTxChannel, 
                                   "SDIO TX", 
                                   SD_DMACompleteCallback, 
                                   pDevice, 
                                   &pDevice->OSInfo.DmaChannel);
        } else {
            err = omap_request_dma(pDevice->OSInfo.DmaRxChannel, 
                                   "SDIO RX", 
                                   SD_DMACompleteCallback, 
                                   pDevice, 
                                   &pDevice->OSInfo.DmaChannel);
        }
        
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP SetUpHCDDMA: can't get DMA channel: %d, err:%d\n",
                      IS_SDREQ_WRITE_DATA(pReq->Flags) ? 
                            pDevice->OSInfo.DmaTxChannel : pDevice->OSInfo.DmaRxChannel, 
                      err));
            status = SDIO_STATUS_NO_RESOURCES;
            break;
        }
    
        if (OMAP_DMA_COMMON == pDevice->DmaMode) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) { 
                SetupTXCommonBufferDMATransfer(pDevice,pReq);
            } else {  
                length = min(pDevice->OSInfo.CommonBufferSize,
                             pReq->DataRemaining);
                setupOmapDmaRx(pDevice, length, pDevice->OSInfo.hDmaBuffer);
                pDevice->OSInfo.LastTransfer = length;
            }
            
            break;
        }
       
            /* setup scatter gather */
        DBG_ASSERT(pDesc != NULL);
        
        DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP SetUpHCDDMA, Direct DMA x, address:0x%X, dma_address:0x%X\n", 
            (UINT32)pDesc->address, (UINT32)pDesc->dma_address));
          
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            setupOmapDmaTx(pDevice, length, pDesc->dma_address); 
        } else {  
            setupOmapDmaRx(pDevice, length,pDesc->dma_address);
        } 
        
    } while (FALSE);
    
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("-SDIO OMAP SetUpHCDDMA: status %d\n",status));
        
    return status;
}

/*
 * SDCancelTransfer - stop DMA transfer
*/
void SDCancelDMATransfer(PSDHCD_DEVICE pDevice)
{
    DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP SDCancelDMATransfer\n"));
    if (pDevice->OSInfo.DmaChannel != NULL) {
        omap_stop_dma(pDevice->OSInfo.DmaChannel);
        omap_free_dma(pDevice->OSInfo.DmaChannel);
        pDevice->OSInfo.DmaChannel  = NULL;
    }       
}

/* SDIO interrupt request */
static void hcd_sdio_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP SDIO IRQ \n"));
        /* call OS independent ISR */
    HcdSDInterrupt((PSDHCD_DEVICE)context);
      
}



