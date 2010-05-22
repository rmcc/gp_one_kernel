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
@file: sdio_hcd_os_2_6.c

@abstract: Linux OMAP native SDIO Host Controller Driver, 2.6 and higher

#notes: includes initialization and DMA code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "../../../include/ctsystem.h"
#include "../sdio_omap_hcd.h"
#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/arch/dma.h>
#include <asm/arch/mux.h>
#include <linux/dma-mapping.h>
#include <asm/arch/board.h>
#include <asm/arch/gpio.h>
#include <asm/arch/tps65010.h>

extern INT gpiodebug;
extern INT noDMA;
extern SDHCD_DRIVER_CONTEXT HcdContext;

static irqreturn_t hcd_sdio_irq(int irq, void *context, struct pt_regs * r);
static void setupOmapDma(PSDHCD_DEVICE pDevice, 
                         int           Length, 
                         DMA_ADDRESS   DmaAddress,
                         BOOL          Transmit);
/*
 * unsetup the OMAP registers 
*/
void DeinitOmap(PSDHCD_DEVICE pDevice)
{
        /* deallocate DMA buffer  */
    if (pDevice->OSInfo.pDmaBuffer != NULL) {
        dma_free_coherent(&pDevice->OSInfo.pBusDevice->dev, 
                          pDevice->OSInfo.CommonBufferSize, 
                          pDevice->OSInfo.pDmaBuffer, 
                          pDevice->OSInfo.hDmaBuffer);
        pDevice->OSInfo.pDmaBuffer = NULL;
    }
    
    if (!IS_ERR(pDevice->OSInfo.pMMCClock)) {
        clk_disable(pDevice->OSInfo.pMMCClock);
        clk_put(pDevice->OSInfo.pMMCClock);
    }
    
    if (pDevice->OSInfo.InitStateMask & SDIO_IRQ_INTERRUPT_INIT) {
        disable_irq(pDevice->OSInfo.Interrupt);
        free_irq(pDevice->OSInfo.Interrupt, pDevice);
        pDevice->OSInfo.InitStateMask &= ~SDIO_IRQ_INTERRUPT_INIT;
    }

    if (pDevice->OSInfo.PowerPin >= 0) {
        omap_free_gpio(pDevice->OSInfo.PowerPin);
    }
}

/*
 * setup the OMAP registers 
*/
SDIO_STATUS InitOmap(PSDHCD_DEVICE pDevice, UINT deviceNumber)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    const struct omap_mmc_config *pConfig = omap_get_config(OMAP_TAG_MMC, struct omap_mmc_config);
    ULONG       baseAddress;
    int         err;
    
    if (pConfig == NULL) {
        DBG_PRINT(SDDBG_WARN, ("SDIO OMAP HCD: InitOmap - unable to get OMAP_TAG_MMC\n"));
        return SDIO_STATUS_NO_RESOURCES;
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: InitOmap - OMAP_TAG_MMC blocks: %d, mmc1PowerPin: %d, mmc1SwitchPin: %d, mmc2PowerPin: %d, mmc2SwitchPin: %d\n",
                            (UINT)pConfig->mmc_blocks, (UINT)pConfig->mmc1_power_pin, (UINT)pConfig->mmc1_switch_pin, (UINT)pConfig->mmc2_power_pin, (UINT)pConfig->mmc2_switch_pin));

    if (pConfig->mmc_blocks == 0) {
        DBG_PRINT(SDDBG_WARN, ("SDIO OMAP HCD: InitOmap - no host controller blocks enabled\n"));
        return SDIO_STATUS_NO_RESOURCES;
    } 
       
    if (deviceNumber == 0) {
        pDevice->OSInfo.PowerPin = pConfig->mmc1_power_pin;
        pDevice->OSInfo.SwitchPin = pConfig->mmc1_switch_pin;
    } else {
        if (pConfig->mmc_blocks & 2) {
            pDevice->OSInfo.PowerPin = pConfig->mmc2_power_pin;
            pDevice->OSInfo.SwitchPin = pConfig->mmc2_switch_pin;
        }else {
            pDevice->OSInfo.PowerPin = pConfig->mmc1_power_pin;
            pDevice->OSInfo.SwitchPin = pConfig->mmc1_switch_pin;
        }
    }
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: InitOmap - Number: %d, PowerPin: %d, SwitchPin: %d, DMAmask: 0x%X\n",
                            deviceNumber, (UINT)pDevice->OSInfo.PowerPin, (UINT)pDevice->OSInfo.SwitchPin, (UINT)*pDevice->OSInfo.pBusDevice->dev.dma_mask));
   
    do {
        if (!noDMA) {
           
                /* allocate a DMA buffer larger enough for the command buffers and the data buffers */
            pDevice->OSInfo.pDmaBuffer =  dma_alloc_coherent(&pDevice->OSInfo.pBusDevice->dev, 
                                                             pDevice->OSInfo.CommonBufferSize, 
                                                             &pDevice->OSInfo.hDmaBuffer,
                                                             GFP_DMA);
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: InitOmap - pDmaBuffer: 0x%X, hDmaBuffer: 0x%X Size:%d\n",
                (UINT)pDevice->OSInfo.pDmaBuffer , 
                (UINT)pDevice->OSInfo.hDmaBuffer,
                pDevice->OSInfo.CommonBufferSize));
            
            if (pDevice->OSInfo.pDmaBuffer == NULL) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: InitOmap - unable to get DMA buffer\n"));
                status =  SDIO_STATUS_NO_RESOURCES;
                break;
            }
            
            pDevice->DmaCapable = TRUE;
                /* tell upper drivers that we support direct DMA */
            pDevice->Hcd.pDmaDescription = &HcdContext.Driver.Dma;                    
        
        }
        
        pDevice->OSInfo.pMMCClock = clk_get(&pDevice->OSInfo.pBusDevice->dev, 
                                            (deviceNumber == 0) ? "mmc1_ck" : "mmc2_ck");
        if (IS_ERR(pDevice->OSInfo.pMMCClock)) {
            DBG_PRINT(SDDBG_ERROR, 
                ("SDIO OMAP HCD: InitOmap - unable to get clock: %s, err: %d, device: %d\n",
                     (deviceNumber) ? "mmc1_ck" : "mmc2_ck", (UINT)PTR_ERR(pDevice->OSInfo.pMMCClock), deviceNumber));
            status = SDIO_STATUS_NO_RESOURCES;
            break;
        }    
   
        clk_enable(pDevice->OSInfo.pMMCClock);
    
        /* configure the mux for the sd controller */
        if (deviceNumber == 0) {
            omap_cfg_reg(MMC_CMD);
            omap_cfg_reg(MMC_CLK);
            omap_cfg_reg(MMC_DAT0);
            omap_cfg_reg(MMC_DAT1);
            omap_cfg_reg(MMC_DAT2);
            omap_cfg_reg(MMC_DAT3);
            baseAddress = OMAP_BASE_ADDRESS1;
            pDevice->OSInfo.Interrupt = OMAP_INTERRUPT1;
            pDevice->OSInfo.DmaRxChannel = OMAP_DMA_RX1;
            pDevice->OSInfo.DmaTxChannel = OMAP_DMA_TX1;
        } else {
            omap_cfg_reg(Y8_1610_MMC2_CMD);
            omap_cfg_reg(Y10_1610_MMC2_CLK);
            omap_cfg_reg(R18_1610_MMC2_CLKIN);
            omap_cfg_reg(W8_1610_MMC2_DAT0);
            omap_cfg_reg(V8_1610_MMC2_DAT1);
            omap_cfg_reg(W15_1610_MMC2_DAT2);
            omap_cfg_reg(R10_1610_MMC2_DAT3);
            omap_cfg_reg(V9_1610_MMC2_CMDDIR);
            omap_cfg_reg(V5_1610_MMC2_DATDIR0);
            omap_cfg_reg(W19_1610_MMC2_DATDIR1);
            baseAddress = OMAP_BASE_ADDRESS2;
            pDevice->OSInfo.Interrupt = OMAP_INTERRUPT2;
            pDevice->OSInfo.DmaRxChannel = OMAP_DMA_RX2;
            pDevice->OSInfo.DmaTxChannel = OMAP_DMA_TX2;
        }
        pDevice->OSInfo.DmaChannel = -1;
            /* map the memory address for the control registers */
        pDevice->OSInfo.Address.pMapped = (PVOID)IO_ADDRESS(baseAddress);
        pDevice->OSInfo.Address.Raw = baseAddress;
        DBG_PRINT(OMAP_TRACE_CONFIG , 
               ("SDIO OMAP - InitOMAP 0x%X\n", (UINT)pDevice->OSInfo.Address.pMapped));
    
        pDevice->OSInfo.InitStateMask |= SDIO_BASE_MAPPED;
    
                /* map the controller interrupt, we map it to each device. 
                   Interrupts can be called from this point on */
        err = request_irq(pDevice->OSInfo.Interrupt, hcd_sdio_irq, 0,
                          "OMAP HCD", pDevice);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: OmapInit, unable to map interrupt \n"));
            err = -ENODEV;
            status = SDIO_STATUS_NO_RESOURCES;
            break;
        }
        
        pDevice->OSInfo.InitStateMask |= SDIO_IRQ_INTERRUPT_INIT;
    
        if (pDevice->OSInfo.PowerPin >= 0) {
            if (omap_request_gpio(pDevice->OSInfo.PowerPin) != 0) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: OmapInit, unable to get power pin GPIO, %d, (dev %d)\n",
                                        pDevice->OSInfo.PowerPin, deviceNumber));
            } else {
                omap_set_gpio_direction(pDevice->OSInfo.PowerPin, 0);
            }
        }
        
    } while (FALSE);

    if (!SDIO_SUCCESS(status)) {
        DeinitOmap(pDevice);    
    }
    
    return status;
}

void ToggleGPIOPin(PSDHCD_DEVICE pDevice, INT PinNo)
{
    /* not implemented */   
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
    setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,TRUE);
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP TX Common Buffer DMA,  This Transfer: %d, Remaining:%d\n", 
        length,pReq->DataRemaining));
}
/*
 *  DMA transmit complete callback
*/
static void SD_DMACompleteCallback(int Channel, u16 DMAStatus, PVOID pContext)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    SDIO_STATUS   status = SDIO_STATUS_PENDING;
    PSDREQUEST    pReq;
    
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
    
    DBG_PRINT(OMAP_TRACE_DATA, 
            ("SDIO OMAP SD_DMACompleteCallback (%s)- DMAStatus: 0x%X, lch: %d \n", 
               IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",(UINT)status, Channel));
    do {
        
        if (-1 == pDevice->OSInfo.DmaChannel) {
            DBG_PRINT(SDDBG_WARN, 
                  ("SDIO OMAP SD_DMACompleteCallback unexpected callback - DMAStatus: 0x%X, lch: %d\n", 
                        (UINT)DMAStatus, Channel));
            break;    
        }
        
        if (DMAStatus == OMAP_DMA_SYNC_IRQ) {
                /* only a synch int, ignore it */
            break;
        }
        
        if (OMAP_DMA_SG == pDevice->DmaMode) {
            DBG_ASSERT(pDevice->OSInfo.pDmaList != NULL);   
            DBG_ASSERT(pDevice->OSInfo.SGcount != 0);
                /* unmap scatter gather */
            dma_unmap_sg(pDevice->Hcd.pDevice,
                         pDevice->OSInfo.pDmaList,
                         pDevice->OSInfo.SGcount,
                         IS_SDREQ_WRITE_DATA(pReq->Flags) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
            pDevice->OSInfo.pDmaList = NULL;
            pDevice->OSInfo.SGcount = 0;
        }
        
            /* handle errors */
        if (DMAStatus & (OMAP_DMA_TOUT_IRQ | OMAP_DMA_DROP_IRQ)) {
            status = SDIO_STATUS_DEVICE_ERROR;
            break;
        }
        
        if (!(DMAStatus & OMAP_DMA_BLOCK_IRQ)) {
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
            UINT32 length;
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
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,FALSE);
                pDevice->OSInfo.LastTransfer = length;
                status = SDIO_STATUS_PENDING;
                break;
            }
        }        
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) {
        omap_free_dma(pDevice->OSInfo.DmaChannel);
        pDevice->OSInfo.DmaChannel = -1;
            /* call callback */
        pDevice->OSInfo.pTransferCompletion(pDevice->OSInfo.TransferContext, status, TRUE);
    }
    
}

#define FIFO_SYNC_BLOCK_SIZE 32

/* setup DMA for transfer */
static void setupOmapDma(PSDHCD_DEVICE pDevice, 
                         int           Length, 
                         DMA_ADDRESS   DmaAddress,
                         BOOL          Transmit)
{
    INT  fifoLen;
 
    if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
            /* multiple of fifo size */
         omap_set_dma_transfer_params(pDevice->OSInfo.DmaChannel,
                                      OMAP_DMA_DATA_TYPE_S16,
                                      FIFO_SYNC_BLOCK_SIZE>>1, (Length/FIFO_SYNC_BLOCK_SIZE), 
                                      OMAP_DMA_SYNC_FRAME);
        fifoLen = 0xF;
     } else {
        if (Length < FIFO_SYNC_BLOCK_SIZE) {
             omap_set_dma_transfer_params(pDevice->OSInfo.DmaChannel,
                                          OMAP_DMA_DATA_TYPE_S16,
                                          Length>>1, 1, 
                                          OMAP_DMA_SYNC_FRAME);
            fifoLen = (Length>>1)-1;   
            fifoLen = (fifoLen < 0) ? 0 : fifoLen;                                       
        } else {
            if (Length == (8 * (Length/8))) {
                 omap_set_dma_transfer_params(pDevice->OSInfo.DmaChannel,
                                              OMAP_DMA_DATA_TYPE_S16,
                                              1, (Length+1)>>1, 
                                              OMAP_DMA_SYNC_FRAME);
                fifoLen = 0;                                             
            } else {
                 omap_set_dma_transfer_params(pDevice->OSInfo.DmaChannel,
                                              OMAP_DMA_DATA_TYPE_S16,
                                              1, (Length+1)>>1, 
                                              OMAP_DMA_SYNC_FRAME);
                fifoLen = 0;                                          
            }
        }
     }
     
     if (Transmit) {
        omap_set_dma_src_params(pDevice->OSInfo.DmaChannel,
                                OMAP_DMA_PORT_EMIFF,
                                OMAP_DMA_AMODE_POST_INC,
                                DmaAddress);
 
        omap_set_dma_dest_params(pDevice->OSInfo.DmaChannel,
                                OMAP_DMA_PORT_TIPB,
                                OMAP_DMA_AMODE_CONSTANT,
                                (virt_to_phys((void *)pDevice->OSInfo.Address.pMapped) + 
                                        OMAP_REG_MMC_DATA_ACCESS));
     
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_TXDE |
                    ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK));

        
        
     } else {
         omap_set_dma_src_params(pDevice->OSInfo.DmaChannel,
                                 OMAP_DMA_PORT_TIPB,
                                 OMAP_DMA_AMODE_CONSTANT,
                                 (virt_to_phys((void *)pDevice->OSInfo.Address.pMapped) + 
                                        OMAP_REG_MMC_DATA_ACCESS));
         omap_set_dma_dest_params(pDevice->OSInfo.DmaChannel,
                                  OMAP_DMA_PORT_EMIFF,
                                  OMAP_DMA_AMODE_POST_INC,
                                  DmaAddress);                                
         WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_RXDE |
                        ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK));
     }
     
     DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP SDReadBuffer: return pending, transfer size: %d, fifo: %d\n",
                    Length, fifoLen));
     omap_start_dma(pDevice->OSInfo.DmaChannel);
    
}

SDIO_STATUS SetUpHCDDMA(PSDHCD_DEVICE            pDevice, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext)
{
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    SYSTEM_STATUS err;
    UINT32 length = pReq->BlockCount * pReq->BlockLen;
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
        }
        
        pDevice->OSInfo.pTransferCompletion = pCompletion;
        pDevice->OSInfo.TransferContext = pContext;
    
        if (pDevice->OSInfo.DmaChannel != -1) {
            DBG_PRINT(SDDBG_WARN, ("SDIO OMAP SetUpHCDDMA: **DMA still in use,  channel: %d\n",
                      (UINT)pDevice->OSInfo.DmaChannel));
            omap_free_dma(pDevice->OSInfo.DmaChannel);
            pDevice->OSInfo.DmaChannel = -1;
        }
  
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {      
            err =  omap_request_dma(pDevice->OSInfo.DmaTxChannel, 
                                   "SDIO TX", 
                                    SD_DMACompleteCallback, 
                                    pDevice, &pDevice->OSInfo.DmaChannel);
            
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
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,FALSE); 
                pDevice->OSInfo.LastTransfer = length;
            }
            break;
        }
       
            /* setup scatter gather */
        DBG_ASSERT(pDesc != NULL);
            /* map DMA */
        dma_map_sg(pDevice->Hcd.pDevice, 
                   pDesc, 
                   pReq->DescriptorCount, 
                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
                         
        pDevice->OSInfo.pDmaList = pDesc;
        pDevice->OSInfo.SGcount = pReq->DescriptorCount;
        DBG_PRINT(OMAP_TRACE_DATA, 
          ("SDIO OMAP SetUpHCDDMA, Direct DMA  dma_address:0x%X\n", (UINT32)sg_dma_address(pDesc)));
          
        setupOmapDma(pDevice, 
                     length, 
                     sg_dma_address(pDesc),
                     IS_SDREQ_WRITE_DATA(pReq->Flags) ? TRUE : FALSE); 
      
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
    if (pDevice->OSInfo.DmaChannel != -1) {
        omap_stop_dma(pDevice->OSInfo.DmaChannel);
        omap_free_dma(pDevice->OSInfo.DmaChannel);
        pDevice->OSInfo.DmaChannel  = -1;
    }
}

/* SDIO interrupt request */
static irqreturn_t hcd_sdio_irq(int irq, void *context, struct pt_regs * r)
{
    irqreturn_t retStat;
    
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP SDIO IRQ \n"));
    
        /* call OS independent ISR */
    if (HcdSDInterrupt((PSDHCD_DEVICE)context)) {
        retStat = IRQ_HANDLED;
    } else {
        retStat = IRQ_NONE;
    }    
    return retStat;
}
