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
#include <ctsystem.h>
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
#include <asm/arch/menelaus.h>
#include <asm/arch/clock.h>

extern INT gpiodebug;
extern INT noDMA;
extern SDHCD_DRIVER_CONTEXT HcdContext;
extern int clk_safe(struct clk *clk);

static irqreturn_t hcd_sdio_irq(int irq, void *context, struct pt_regs * r);
static void setupOmapDma(PSDHCD_DEVICE pDevice, 
                         int           Length, 
                         DMA_ADDRESS   DmaAddress,
                         BOOL          RX);
static void SD_DMACompleteCallback(int Channel, u16 DMAStatus, PVOID pContext);

void ToggleGPIOPin(PSDHCD_DEVICE pDevice, INT PinNo)
{  
    if (!gpiodebug) {
        return;
    }
   
    switch (PinNo) {
        case DBG_GPIO_PIN_1:   
            omap_set_gpio_dataout(16,TRUE);
            omap_set_gpio_dataout(16,FALSE);
            break;  
        case DBG_GPIO_PIN_2:
            break;  
        default:
            break;
    }
}


#define OMAP_CONTROL_PADCONF_BASE_ADDRESS 0x48000000
#define OMAP_CONTROL_PADCONF_SIZE 0x0400 
#define OMAP_PAD_PULLUPDWN_ENABLE (1 << 3)
#define OMAP_PAD_PULLUP_TYPE      (1 << 4)
#define OMAP_PAD_PULLDOWN_TYPE    (0 << 4)

void OmapPadConfig(UINT32 Offset, UINT8 BitPos, UINT8 PadValue)
{
    UINT32 value;
    UINT32 padConfig = (UINT32)ioremap(OMAP_CONTROL_PADCONF_BASE_ADDRESS,
                               OMAP_CONTROL_PADCONF_SIZE); 
    
    value = readl(padConfig+Offset);
    value &= ~((UINT32)0xff << BitPos);
    value |= (UINT32)PadValue << BitPos;
    writel(value,padConfig+Offset);
}


/*
 * MapAddress - maps I/O address
*/
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription) {
     
    if (request_mem_region(pMap->Raw, pMap->Length, pDescription) == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Local HCD: MapAddress - memory in use\n"));
        return -EBUSY;
    }
    pMap->pMapped = ioremap_nocache(pMap->Raw, pMap->Length);
    if (pMap->pMapped == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Local HCD: MapAddress - unable to map memory\n"));
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

SDIO_STATUS SlotEnableControl(BOOL Enable)
{
    int value;
    SDIO_STATUS status = SDIO_STATUS_ERROR;
    
    do {
        value = menelaus_read(MENELAUS_LDO_CTRL7);
        if (value == -1) {
            DBG_ASSERT(FALSE);
            break;    
        }
        if (Enable) {
            value |= 0x03;   
        } else {
            value &= ~0x03;     
        }
        value = menelaus_write(value, MENELAUS_LDO_CTRL7);
        if (value == -1) {
            DBG_ASSERT(FALSE);
            break;    
        }
        
        value = menelaus_read(MENELAUS_MCT_CTRL3);
        if (value == -1) {
            DBG_ASSERT(FALSE);
            break;    
        }
        if (Enable) {
            value |= 0x01;   
        } else {
            value &= ~0x01;     
        }
        value = menelaus_write(value, MENELAUS_MCT_CTRL3);
        if (value == -1) {
            DBG_ASSERT(FALSE);
            break;    
        }
        
        
        status = SDIO_STATUS_SUCCESS;
    } while (FALSE);
     
    return status;
}
/*
 * unsetup the OMAP registers 
*/
void DeinitOmap(PSDHCD_DEVICE pDevice)
{
    
    if (pDevice->OSInfo.InitStateMask & SDIO_IRQ_INTERRUPT_INIT) {
        disable_irq(pDevice->OSInfo.Interrupt);
        free_irq(pDevice->OSInfo.Interrupt, pDevice);
        pDevice->OSInfo.InitStateMask &= ~SDIO_IRQ_INTERRUPT_INIT;
    }
    
        /* deallocate DMA buffer  */
    if (pDevice->OSInfo.pDmaBuffer != NULL) {
        dma_free_coherent(&pDevice->OSInfo.pBusDevice->dev, 
                          pDevice->OSInfo.CommonBufferSize, 
                          pDevice->OSInfo.pDmaBuffer, 
                          pDevice->OSInfo.hDmaBuffer);
        pDevice->OSInfo.pDmaBuffer = NULL;
    }

    if (pDevice->OSInfo.DmaRxChannel != -1) {
        omap_free_dma(pDevice->OSInfo.DmaRxChannel);
        pDevice->OSInfo.DmaRxChannel = -1;
    }
    
    if (pDevice->OSInfo.DmaTxChannel != -1) {
        omap_free_dma(pDevice->OSInfo.DmaTxChannel);
        pDevice->OSInfo.DmaTxChannel = -1;    
    }
    
    if (pDevice->OSInfo.InitStateMask & SDIO_BASE_MAPPED) {
        pDevice->OSInfo.InitStateMask &= ~SDIO_BASE_MAPPED;
        UnmapAddress(&pDevice->OSInfo.Address);    
    }
    
    SlotEnableControl(FALSE);
}

void FifoTxTest(PSDHCD_DEVICE pDevice)
{
    INT     dataCopy = 1000;
    volatile UINT16 *pFifo;
    
    pFifo = (volatile UINT16 *)((UINT32)GET_HC_REG_BASE(pDevice) + OMAP_REG_MMC_DATA_ACCESS);

    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: FifoTest (0x%X) \n", (UINT)pFifo));
              
              
    while (dataCopy) { 
        *pFifo = (UINT16)dataCopy;
        dataCopy--;      
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: FifoTest Done\n"));  
}

/*
 * setup the OMAP registers 
*/
SDIO_STATUS InitOmap(PSDHCD_DEVICE pDevice, UINT deviceNumber)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    ULONG       baseAddress;
    int         err;
    struct      clk *clksrc;
    
    do {
                
        pDevice->OSInfo.Interrupt = INT_MMC_IRQ;
        baseAddress = OMAP_BASE_ADDRESS1;
        pDevice->OSInfo.DmaRxId = OMAP_DMA_MMC1_RX;
        pDevice->OSInfo.DmaTxId = OMAP_DMA_MMC1_TX;
        pDevice->OSInfo.DmaRxChannel = -1;
        pDevice->OSInfo.DmaTxChannel = -1;
        
        SlotEnableControl(TRUE);
        
        clksrc = clk_get(NULL,"mmc_ick");
        if (NULL == clksrc) {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_NO_RESOURCES;
            break;    
        }
             
        clk_use(clksrc);
        
        clksrc = clk_get(NULL,"mmc_fck");
        if (NULL == clksrc) {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_NO_RESOURCES;
            break;    
        }
        clk_use(clksrc);
        clk_safe(clksrc);
       
          
        if (!noDMA) {
            UINT32 gcrVal;
            
            gcrVal = readl(OMAP_DMA4_GCR_REG);
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: DMA4_GCR : 0x%X \n",gcrVal));
            gcrVal &= ~0xff;
            gcrVal |= 64;
            writel(gcrVal, OMAP_DMA4_GCR_REG);
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: DMA4_GCR reread: 0x%X \n",readl(OMAP_DMA4_GCR_REG)));
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

        if (gpiodebug) {
              /* setup GPIO 16 */
#define CONTROL_PADCONF_Y11       0x00E8           
            OmapPadConfig(CONTROL_PADCONF_Y11, 
                          0, 
                          0x3 | OMAP_PAD_PULLUPDWN_ENABLE | OMAP_PAD_PULLUP_TYPE);
            omap_set_gpio_direction(16, OMAP24XX_DIR_OUTPUT);
            omap_set_gpio_dataout(16,FALSE);
        }
               
        err = omap_request_dma(pDevice->OSInfo.DmaRxId, 
                               "SDIO TX", 
                               SD_DMACompleteCallback, 
                               pDevice, 
                               &pDevice->OSInfo.DmaRxChannel);
        
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: OmapInit, unable to get RX DMA channel, %d\n",err));
            status = SDIO_STATUS_NO_RESOURCES; 
            break;
        }
                               
        err = omap_request_dma(pDevice->OSInfo.DmaTxId, 
                               "SDIO TX", 
                               SD_DMACompleteCallback, 
                               pDevice, 
                               &pDevice->OSInfo.DmaTxChannel);
       
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: OmapInit, unable to get TX DMA channel, %d\n",err));
            status = SDIO_STATUS_NO_RESOURCES; 
            break;
        }

            /* map the memory address for the control registers */
        pDevice->OSInfo.Address.pMapped = (PVOID)IO_ADDRESS(baseAddress);
        pDevice->OSInfo.Address.Raw = baseAddress;
        pDevice->OSInfo.Address.Length = OMAP_BASE_LENGTH;
        if (MapAddress(&pDevice->OSInfo.Address, "SDHC Regs") < 0) {
            status = SDIO_STATUS_NO_RESOURCES; 
            break;    
        }
        DBG_PRINT(SDDBG_TRACE, 
               ("SDIO OMAP - InitOMAP I/O Virt:0x%X Phys:0x%X\n", 
               (UINT)pDevice->OSInfo.Address.pMapped, (UINT)pDevice->OSInfo.Address.Raw));
       
        //FifoTxTest(pDevice);
               
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
            
    } while (FALSE);

    if (!SDIO_SUCCESS(status)) {
        DeinitOmap(pDevice);    
    }
    
    return status;
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
    setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,FALSE);
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
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,TRUE);
                pDevice->OSInfo.LastTransfer = length;
                status = SDIO_STATUS_PENDING;
                break;
            }
        }        
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) {
            /* call callback */
        pDevice->OSInfo.pTransferCompletion(pDevice->OSInfo.TransferContext, status, TRUE);
    }
    
}


void DumpDMASettings(PSDHCD_DEVICE pDevice, BOOL TX)
{
    int channel = TX ? pDevice->OSInfo.DmaTxChannel : pDevice->OSInfo.DmaRxChannel;
    
    DBG_PRINT(SDDBG_TRACE, ("OMAP DMA Reg Dump (%s) Channel:0x%X, DMAREQ:%d \n", 
             TX ? "Transmit":"Receive", channel,
             TX ? pDevice->OSInfo.DmaTxId:pDevice->OSInfo.DmaRxId));
    DBG_PRINT(SDDBG_TRACE, ("  CCR       : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CCR_REG(channel))));           
    DBG_PRINT(SDDBG_TRACE, ("  CLNK_CTRL : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CLNK_CTRL_REG(channel))));   
    DBG_PRINT(SDDBG_TRACE, ("  CICR      : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CICR_REG(channel))));    
    DBG_PRINT(SDDBG_TRACE, ("  CSR       : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CSR_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CSDP      : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CSDP_REG(channel))));   
    DBG_PRINT(SDDBG_TRACE, ("  CEN       : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CEN_REG(channel)))); 
    DBG_PRINT(SDDBG_TRACE, ("  CFN       : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CFN_REG(channel))));     
    DBG_PRINT(SDDBG_TRACE, ("  CSSA      : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CSSA_REG(channel))));    
    DBG_PRINT(SDDBG_TRACE, ("  CDSA      : 0x%X \n",_READ_DWORD_REG(OMAP_DMA4_CDSA_REG(channel))));    
    DBG_PRINT(SDDBG_TRACE, ("  CSEI      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CSEI_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CSFI      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CSFI_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CDEI      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CDEI_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CDFI      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CDFI_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CSAC      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CSAC_REG(channel)))); 
    DBG_PRINT(SDDBG_TRACE, ("  CDAC      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CDAC_REG(channel))));
    DBG_PRINT(SDDBG_TRACE, ("  CCEN     : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CCEN_REG(channel)))); 
    DBG_PRINT(SDDBG_TRACE, ("  CCFN      : 0x%X \n", _READ_DWORD_REG(OMAP_DMA4_CCFN_REG(channel))));
    
}

#define FIFO_SYNC_BLOCK_SIZE (OMAP_MMC_FIFO_SIZE/2)  /* sync set to 1/2 full/empty*/

/* setup DMA for transfer */
static void setupOmapDma(PSDHCD_DEVICE pDevice, 
                         int           Length, 
                         DMA_ADDRESS   SystemAddress,
                         BOOL          RX)
{   
    INT  fifoLen;
    INT  cen,cfn;
    int channel = RX ? pDevice->OSInfo.DmaRxChannel : pDevice->OSInfo.DmaTxChannel;
    BOOL burstEnable = FALSE;
    UINT32 csdp; 
       
    UINT32 address = pDevice->OSInfo.Address.Raw + OMAP_REG_MMC_DATA_ACCESS; 

    if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
            /* multiple of fifo synch size */
        cen = FIFO_SYNC_BLOCK_SIZE>>1;
        cfn = (Length/FIFO_SYNC_BLOCK_SIZE);
        fifoLen = 0xF; /* threshold set to 32 bytes which is half way on the FIFOs */
            /* enable bursting since we are nicely divisible by a FIFO size */
        burstEnable = TRUE; 
    } else {
        if (Length < FIFO_SYNC_BLOCK_SIZE) {
            cen = Length>>1;
            cfn = 1;
            fifoLen = (Length>>1)-1;   
            fifoLen = (fifoLen < 0) ? 0 : fifoLen;                                       
        } else {
            if (Length == (8 * (Length/8))) {
                cen = 1;
                cfn = (Length+1)>>1;
                fifoLen = 0;                                       
            } else {
                cen = 1;
                cfn = (Length+1)>>1;
                fifoLen = 0;                                          
            }
        }
    }
    
    omap_set_dma_transfer_params(channel,
                                 OMAP_DMA_DATA_TYPE_S16,
                                 cen,
                                 cfn,
                                 OMAP_DMA_SYNC_BLOCK,
                                 RX ? pDevice->OSInfo.DmaRxId : pDevice->OSInfo.DmaTxId,
                                 RX ? TRUE : FALSE);
    
     
    if (RX) {
        omap_set_dma_src_params(channel,
                                OMAP_DMA_AMODE_CONSTANT,
                                (int)address,
                                0,
                                0); 
      
        omap_set_dma_dest_params(channel,
                                 OMAP_DMA_AMODE_POST_INC,
                                 SystemAddress,
                                 0, 
                                 0);      
    } else {
        
            /* source is system memory */
        omap_set_dma_src_params(channel,
                                OMAP_DMA_AMODE_POST_INC,
                                (int)SystemAddress,
                                0, 
                                0);
       
        omap_set_dma_dest_params(channel,
                                OMAP_DMA_AMODE_CONSTANT,
                                (int)address,
                                0,
                                0); 
    }

    DBG_PRINT(OMAP_TRACE_DATA, ("OMAP DMA channel sync ID: %d \n", 
             (RX) ? pDevice->OSInfo.DmaRxId : pDevice->OSInfo.DmaTxId));
   
       
    if (DBG_GET_DEBUG_LEVEL() >= OMAP_TRACE_DMA_DUMP) {
        DumpDMASettings(pDevice, RX ? FALSE:TRUE);
    }                                

    csdp = readl(OMAP_DMA4_CSDP_REG(channel));
        /* clear previous burst settings */
    csdp &= ~((0x3 << 7) | (0x3 << 14) | (1 << 13) | (1 << 6)); 
    
    if (burstEnable) {
        csdp |= (0x2 << 7) | (0x2 << 14) | (1 << 13) | (1 << 6); /* 32 byte burst enable */
    } 
  
    writel(csdp, OMAP_DMA4_CSDP_REG(channel));
    
    if (RX) {
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_RXDE |
                        ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK));        
    } else {
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_TXDE |
                        ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK));
    }
        /* start */ 
    omap_start_dma(channel);
}

SDIO_STATUS CheckDMA(PSDHCD_DEVICE pDevice, 
                     PSDREQUEST    pReq)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
        
        if ((OMAP_DMA_COMMON == pDevice->DmaMode) &&
            (pReq->DataRemaining & 0x1)) {
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
        }
    } while (FALSE);
    
    return status;
}

SDIO_STATUS SetUpHCDDMA(PSDHCD_DEVICE            pDevice, 
                        PSDREQUEST               pReq, 
                        PDMA_TRANSFER_COMPLETION pCompletion,
                        PVOID                    pContext)
{
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    UINT32 length = pReq->DataRemaining;
    PSDDMA_DESCRIPTOR pDesc = NULL;
    
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("+SDIO OMAP SetUpHCDDMA: length: %d\n",length));
    
    do {
                        
        pDevice->OSInfo.pTransferCompletion = pCompletion;
        pDevice->OSInfo.TransferContext = pContext;
          
        if (OMAP_DMA_COMMON == pDevice->DmaMode) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) { 
                SetupTXCommonBufferDMATransfer(pDevice,pReq);
            } else {  
                length = min(pDevice->OSInfo.CommonBufferSize,
                             pReq->DataRemaining);
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,TRUE); 
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
                     IS_SDREQ_WRITE_DATA(pReq->Flags) ? FALSE : TRUE); 
      
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
    if (pDevice->OSInfo.DmaRxChannel != -1) {
        omap_stop_dma(pDevice->OSInfo.DmaRxChannel);
    }       
    if (pDevice->OSInfo.DmaTxChannel != -1) {
        omap_stop_dma(pDevice->OSInfo.DmaTxChannel);
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
