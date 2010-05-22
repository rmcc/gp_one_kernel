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

#include <ctsystem.h>
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
#include <asm/arch/gpio.h>

extern INT gpiodebug;
extern INT noDMA;
extern SDHCD_DRIVER_CONTEXT HcdContext;
extern SDIO_STATUS SlotEnableControl(BOOL Enable);
static void SD_DMACompleteCallback(int lch, UINT16 DMAStatus, PVOID pContext);

extern int menelaus_init(void);
extern void menelaus_exit(void);

#define OMP_DMA_RX TRUE
#define OMP_DMA_TX FALSE

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

static void hcd_sdio_irq(int irq, void *context, struct pt_regs * r);


/*
 * unsetup the OMAP registers 
*/
void DeinitOmap(PSDHCD_DEVICE pDevice)
{
    
        /* disable clock */
    //??_WRITE_DWORD_REG(MOD_CONF_CTRL_0, _READ_DWORD_REG(MOD_CONF_CTRL_0) & ~((1 << 23) | (1 << 21)));
    
    if (pDevice->OSInfo.InitStateMask & SDIO_IRQ_INTERRUPT_INIT) {
        disable_irq(pDevice->OSInfo.Interrupt);
        free_irq(pDevice->OSInfo.Interrupt, pDevice);
    }   
    
    if (pDevice->OSInfo.pDmaBuffer != NULL) {
        consistent_free(pDevice->OSInfo.pDmaBuffer, 
                        pDevice->OSInfo.CommonBufferSize, 
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

#define OMAP_CONTROL_PADCONF_BASE_ADDRESS 0x48000000
#define OMAP_CONTROL_PADCONF_SIZE 0x0400 
#define OMAP_PAD_PULLUPDWN_ENABLE (1 << 3)
#define OMAP_PAD_PULLUP_TYPE      (1 << 4)
#define OMAP_PAD_PULLDOWN_TYPE    (0 << 4)

#define CONTROL_PADCONF_Y11       0x00E8   

static UINT32 padConfig = 0;

void OmapPadConfig(UINT32 Offset, UINT8 BitPos, UINT8 PadValue)
{
    UINT32 value;
    
    if (padConfig == 0) {
        padConfig = (UINT32)ioremap(OMAP_CONTROL_PADCONF_BASE_ADDRESS,
                                    OMAP_CONTROL_PADCONF_SIZE); 
    }
    
    value = readl(padConfig+Offset);
    value &= ~((UINT32)0xff << BitPos);
    value |= (UINT32)PadValue << BitPos;
    writel(value, padConfig+Offset);
}

/*
 * setup the OMAP registers 
*/
SDIO_STATUS InitOmap(PSDHCD_DEVICE pDevice, UINT deviceNumber)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    ULONG       baseAddress;
    int         err;
   
    do {
               
        status = SlotEnableControl(TRUE);
        
        if (!SDIO_SUCCESS(status)) {
            status = SDIO_STATUS_NO_RESOURCES; 
            DBG_ASSERT(FALSE);
            break;        
        }
        
            /* CMD , CLK and DAT0 are dedicated pins */
            /* DAT1 */
        OmapPadConfig(0xF4,16,0);
            /* DAT2 */
        OmapPadConfig(0xF4,24,0);
            /* DAT3 */
        OmapPadConfig(0xF8,0,0);
            /* DAT0-DIR */
        OmapPadConfig(0xF8,8,0);
            /* DAT1-DIR */
        OmapPadConfig(0xF8,16,0);
            /* DAT2-DIR */
        OmapPadConfig(0xF8,24,0);        
            /* DAT3-DIR */
        OmapPadConfig(0xFC,0,0);      
        
        if (pDevice->OSInfo.DAT0_gpio_pin != -1) {
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO OMAP HCD: InitOmap - GPIO %d used for DAT0-busy polling (0x%X,0x%X,0x%X) \n",
                pDevice->OSInfo.DAT0_gpio_pin,
                pDevice->OSInfo.DAT0_gpio_conf_offset,
                pDevice->OSInfo.DAT0_gpio_pad_conf_byte,
                pDevice->OSInfo.DAT0_gpio_pad_mode_value));
                /* set gpio pin for polling DAT0 is input only */
            omap_set_gpio_direction(pDevice->OSInfo.DAT0_gpio_pin, OMAP2420_DIR_INPUT);
                /* configure I/O pad mux configuration provided by the user */
            OmapPadConfig((UINT32)pDevice->OSInfo.DAT0_gpio_conf_offset,
                          (UINT8)(pDevice->OSInfo.DAT0_gpio_pad_conf_byte * 8), 
                          (UINT8)pDevice->OSInfo.DAT0_gpio_pad_mode_value); 
        }
                         
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
            
                /* tell upper drivers that we support direct DMA from the request buffers */
            pDevice->Hcd.pDmaDescription = &HcdContext.Driver.Dma;
            pDevice->DmaCapable = TRUE;                   
        }
             
        if (gpiodebug) {
              /* setup GPIO 16 */        
            OmapPadConfig(CONTROL_PADCONF_Y11, 
                          0, 
                          0x3 | OMAP_PAD_PULLUPDWN_ENABLE | OMAP_PAD_PULLUP_TYPE);
            omap_set_gpio_direction(16, OMAP2420_DIR_OUTPUT);
            omap_set_gpio_dataout(16,FALSE);
        }
            
        baseAddress = OMAP_BASE_ADDRESS1;
        pDevice->OSInfo.Interrupt = INT_MMC_IRQ;
        pDevice->OSInfo.DmaRxId = OMAP_DMA_MMC_RX;
        pDevice->OSInfo.DmaTxId = OMAP_DMA_MMC_TX;
        pDevice->OSInfo.DmaRxChannel = -1;
        pDevice->OSInfo.DmaTxChannel = -1;
            
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
        //??pDevice->OSInfo.Address.pMapped = (PVOID)IO_ADDRESS(baseAddress);
        pDevice->OSInfo.Address.Raw = baseAddress;
        pDevice->OSInfo.Address.Length = OMAP_BASE_LENGTH;
        if (MapAddress(&pDevice->OSInfo.Address, "SDHC Regs") < 0) {
            status = SDIO_STATUS_NO_RESOURCES; 
            break;    
        }
        DBG_PRINT(SDDBG_TRACE, 
               ("SDIO OMAP - InitOMAP I/O Virt:0x%X Phys:0x%X\n", 
               (UINT)pDevice->OSInfo.Address.pMapped, (UINT)pDevice->OSInfo.Address.Raw));
    
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
    DBG_PRINT(SDDBG_TRACE, ("  MMC_BUFFER_CONFIG : 0x%X \n", READ_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG)));
}

#define FIFO_SYNC_BLOCK_SIZE 32
/* 2.4 */
static void setupOmapDma(PSDHCD_DEVICE pDevice, int Length, DMA_ADDRESS SystemAddress, BOOL RX)
{
    INT  fifoLen;
    INT  cen,cfn;
    int channel = RX ? pDevice->OSInfo.DmaRxChannel : pDevice->OSInfo.DmaTxChannel;
    UINT32 csdp;
    BOOL   burstEnable = FALSE;
    UINT32 address = pDevice->OSInfo.Address.Raw + OMAP_REG_MMC_DATA_ACCESS; 

    DBG_PRINT(OMAP_TRACE_DATA, ("OMAP setupOmapDma (%s) Length:%d  channel sync ID: %d \n", 
             (RX) ? "RX":"TX", Length, (RX) ? pDevice->OSInfo.DmaRxId : pDevice->OSInfo.DmaTxId));
             
    if (RX) {
        if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
                /* multiple of fifo size */
            cen = FIFO_SYNC_BLOCK_SIZE>>1;
            cfn = (Length/FIFO_SYNC_BLOCK_SIZE);
            fifoLen = 0xF;
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
    } else {
        if (Length == (FIFO_SYNC_BLOCK_SIZE * (Length/FIFO_SYNC_BLOCK_SIZE))) {
                /* multiple of fifo size */
            cen = FIFO_SYNC_BLOCK_SIZE>>1;
            cfn = (Length/FIFO_SYNC_BLOCK_SIZE);
            fifoLen = 0xF;
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
    }
    
    omap_set_dma_transfer_params(channel,
                                 OMAP_DMA_DATA_TYPE_S16,
                                 cen,
                                 cfn,
                                 OMAP_DMA_SYNC_BLOCK,
                                 //??OMAP_DMA_SYNC_FRAME,
                                 RX ? pDevice->OSInfo.DmaRxId : pDevice->OSInfo.DmaTxId,
                                 RX ? TRUE:FALSE);
    
     
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
        
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_RXDE |
                        ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK));        
             
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
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, OMAP_REG_MMC_BUFFER_CONFIG_TXDE |
                        ((fifoLen << OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT) & OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK));
    }                     
    
    csdp = readl(OMAP_DMA4_CSDP_REG(channel));
        /* clear previous burst settings */
    csdp &= ~((0x3 << 7) | (0x3 << 14) | (1 << 13) | (1 << 6)); 
    
    if (burstEnable) {
        csdp |= (0x2 << 7) | (0x2 << 14) | (1 << 13) | (1 << 6); /* 32 byte burst enable */
    } 
  
    writel(csdp, OMAP_DMA4_CSDP_REG(channel));
  
    if (DBG_GET_DEBUG_LEVEL() >= OMAP_TRACE_DMA_DUMP) {
        DumpDMASettings(pDevice, RX ? FALSE:TRUE);
    }           
    
        /* start */
    omap_start_dma(channel);
}

#define OMAP_DMA_ERRORS  (OMAP_DMA_TOUT_IRQ | OMAP_DMA_DROP_IRQ)

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
    setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer, OMP_DMA_TX);
    DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP TX Common Buffer DMA,  This Transfer: %d, Remaining:%d\n", 
        length,pReq->DataRemaining));
}

/* 2.4
 *  DMA transmit complete callback
*/
static void SD_DMACompleteCallback(int lch, UINT16 DMAStatus, PVOID pContext)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    SDIO_STATUS   status = SDIO_STATUS_PENDING;
    PSDREQUEST    pReq;
    UINT32        length;
   
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
              
    do {
       
        if (NULL == pReq) {
            DBG_ASSERT(FALSE);
            break;    
        } 
        
        if (NULL == pDevice->OSInfo.pTransferCompletion) {
            DBG_ASSERT(FALSE);
            break;    
        }
                            
        if (DMAStatus == OMAP_DMA_SYNC_IRQ) {
            /* only a synch int, ignore it */
            break;
        }
        
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            omap_stop_dma(pDevice->OSInfo.DmaTxChannel);
        } else {
            omap_stop_dma(pDevice->OSInfo.DmaRxChannel);    
        }
            /* handle errors */
        if (DMAStatus & OMAP_DMA_ERRORS) {
            status = SDIO_STATUS_DEVICE_ERROR;
        }
        
        if (!(DMAStatus & OMAP_DMA_BLOCK_IRQ)) {
            /* TODO, the dma interrupt handler code in the kernel does not
             * return the correct status 
             * 
             * status = SDIO_STATUS_DEVICE_ERROR; */
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
            DBG_ASSERT((INT)pReq->DataRemaining >= 0);
                /* set up next transfer */
            length = min(pDevice->OSInfo.CommonBufferSize,
                         pReq->DataRemaining);
            if (length) {
                DBG_PRINT(OMAP_TRACE_DATA, 
                    ("SDIO OMAP RX Common Buffer DMA,  Pending Transfer: %d, Remaining:%d\n", 
                            length, pReq->DataRemaining));
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,OMP_DMA_RX);
                pDevice->OSInfo.LastTransfer = length;
                status = SDIO_STATUS_PENDING;
                break;
            }
        }
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) {
        pDevice->OSInfo.pTransferCompletion(pDevice->OSInfo.TransferContext, status, TRUE);
    }
                                
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
                setupOmapDma(pDevice, length, pDevice->OSInfo.hDmaBuffer,OMP_DMA_RX);
                pDevice->OSInfo.LastTransfer = length;
                DBG_PRINT(OMAP_TRACE_DATA, 
                    ("SDIO OMAP RX Common Buffer DMA,  Pending Transfer: %d, Remaining:%d\n", 
                            length, pReq->DataRemaining));
            }
            
            break;
        }
       
            /* setup scatter gather */
        DBG_ASSERT(pDesc != NULL);
        
        DBG_PRINT(OMAP_TRACE_DATA, 
        ("SDIO OMAP SetUpHCDDMA, Direct DMA x, address:0x%X, dma_address:0x%X\n", 
            (UINT32)pDesc->address, (UINT32)pDesc->dma_address));
          
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            setupOmapDma(pDevice, length, pDesc->dma_address,OMP_DMA_TX); 
        } else {  
            setupOmapDma(pDevice, length,pDesc->dma_address,OMP_DMA_RX);
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
    if (pDevice->OSInfo.DmaRxChannel != -1) {
        omap_stop_dma(pDevice->OSInfo.DmaRxChannel);
    }       
    if (pDevice->OSInfo.DmaTxChannel != -1) {
        omap_stop_dma(pDevice->OSInfo.DmaTxChannel);
    }
}

/* SDIO interrupt request */
static void hcd_sdio_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP SDIO IRQ \n"));
        /* call OS independent ISR */
    HcdSDInterrupt((PSDHCD_DEVICE)context);
      
}


/* workaround for controller bug that fails to detect card busy assertion properly.
 * This workaround requires a gpio pin to sample the DAT0 polling */
SDIO_STATUS WaitDat0Busy(PSDHCD_DEVICE pDevice)
{  
    int timeleft = 200000;  /* 200 MS */
        
    if (pDevice->OSInfo.DAT0_gpio_pin == -1) {
        /* gpio pin is not available */
        return SDIO_STATUS_SUCCESS;        
    }
    
    while (timeleft) {
        
        if (omap_get_gpio_datain(pDevice->OSInfo.DAT0_gpio_pin)) {
                /* pin is reading back high , busy signal deasserted...*/
            break;    
        }
        
        udelay(1);
        timeleft--;
    }    
    
    udelay(1);
    
    if (!timeleft) {
        return SDIO_STATUS_BUS_WRITE_ERROR;    
    }
    
    return SDIO_STATUS_SUCCESS;
}



