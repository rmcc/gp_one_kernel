/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_hcd_os.c

@abstract: Linux OMAP McBSP SPI SDIO Host Controller Driver

#notes: includes module load and unload functions
 
@notice: Copyright (c), 2004-2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#define DBG_DECLARE 7;
#include "../../../include/ctsystem.h"

#include "../sdio_omapmcbsp_hcd.h"
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <asm/arch/mux.h>
#include <asm/arch/gpio.h>

/* temp addition *///?????????????????????????? goes in mcbsp.h
struct omap_mcbsp * omap_mcbsp_get_config(unsigned int id);
 struct omap_mcbsp {
         u32                          io_base;
         u8                           id;
         u8                           free;
         omap_mcbsp_word_length       rx_word_length;
         omap_mcbsp_word_length       tx_word_length;
 
         /* IRQ based TX/RX */
         int                          rx_irq;
         int                          tx_irq;
 
         /* DMA stuff */
         u8                           dma_rx_sync;
         short                        dma_rx_lch;
         u8                           dma_tx_sync;
         short                        dma_tx_lch;
 
         /* Completion queues */
         struct completion            tx_irq_completion;
         struct completion            rx_irq_completion;
         struct completion            tx_dma_completion;
         struct completion            rx_dma_completion;
 
         spinlock_t                   lock;
};//??????????????????????????????????????????????

#define DESCRIPTION "SDIO OMAP McBSP SPI HCD"
#define AUTHOR "Atheros Communications, Inc."

static int Probe(struct device *pBusDevice);
static int Remove(struct device *pBusDevice);
static void RemoveDevice(struct device *pBusDevice, PSDHCD_DRIVER_CONTEXT pHcdContext);
static void hcd_release_device(struct device *pDevice);


static void hcd_iocomplete_wqueue_handler(void *context);
static void hcd_carddetect_wqueue_handler(void *context);
static void hcd_sdioirq_wqueue_handler(void *context);
static void hcd_dmairq_wqueue_handler(void *context);
static SDIO_STATUS InitializeMcBSP(PSDHCD_DEVICE pDevice);
static void DeinitializeMcBSP(PSDHCD_DEVICE pDevice);
static irqreturn_t SD_irq_handler(int, void *, struct pt_regs *);
static void RxWork(PSDHCD_DEVICE pDevice);

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");
#define DEFAULT_ATTRIBUTES (SDHCD_ATTRIB_BUS_SPI | SDHCD_ATTRIB_SLOT_POLLING | SDHCD_ATTRIB_AUTO_CMD12)
UINT32 hcdattributes = DEFAULT_ATTRIBUTES;
module_param(hcdattributes, int, 0644);
MODULE_PARM_DESC(hcdattributes, "OMAP McBSP Attributes");

UINT32 channels = 1;
module_param(channels, int, 0644);
MODULE_PARM_DESC(channels, "OMAP McBSP number of supported channels");
UINT32 channel0 = OMAPMCBSP_DEFAULT_DMA_ID;
module_param(channel0, int, 0644);
MODULE_PARM_DESC(channel0, "OMAP McBSP 0 DMA Channel");
UINT32 channel1 = 0;
module_param(channel1, int, 0644);
MODULE_PARM_DESC(channel, "OMAP McBSP 1 DMA Channel");
UINT32 channel2 = 0;
module_param(channel2, int, 0644);
MODULE_PARM_DESC(channel2, "OMAP McBSP 2 DMA Channel");
UINT32 channel3 = 0;
module_param(channel3, int, 0644);
MODULE_PARM_DESC(channel3, "OMAP McBSP 3 DMA Channel");
UINT32 base_clock = OMAPMCBSP_BASE_CLOCK;
module_param(base_clock, int, 0644);
MODULE_PARM_DESC(base_clock, "OMAP McBSP base clock");
UINT32 min_clock = OMAPMCBSP_DEFAULT_MIN_CLOCK_DIVIDE;
module_param(min_clock, int, 0644);
MODULE_PARM_DESC(min_clock, "OMAP McBSP minimum clock divisor");
UINT32 irq_pin = OMAPMCBSP_DEFAULT_IRQ_PIN;
module_param(irq_pin, int, 0644);
MODULE_PARM_DESC(irq_pin, "OMAP McBSP irq GPIO pin");
UINT32 disable_polling = 0;
module_param(disable_polling, int, 0644);
MODULE_PARM_DESC(disable_polling, "OMAP McBSP set 1 to disable card polling");
UINT32 max_blocks = OMAPMCBSP_DEFAULT_SPI_MAX_BLOCKS;
module_param(max_blocks, int, 0644);
MODULE_PARM_DESC(max_blocks, "OMAP McBSP maximum blocks per transfer");


static u64 dma_mask = OMAPMCBSP_DMA_MASK;
/* the driver context data */
/* the driver context data */
static SDHCD_DRIVER_CONTEXT HcdContext = {
   .pDescription  = DESCRIPTION,
   .DeviceCount   = 0,
   .Driver.HcdPlatformDevice.name   = "sdio_omapmcbsp_hcd",
   .Driver.HcdPlatformDevice.id     = 0,
   .Driver.HcdPlatformDevice.dev.dma_mask = &dma_mask,
   .Driver.HcdPlatformDevice.dev.coherent_dma_mask = OMAPMCBSP_DMA_MASK,
   .Driver.HcdPlatformDevice.dev.release = hcd_release_device,
   .Driver.HcdPlatformDriver.name   = "sdio_omapmcbsp_hcd",
   .Driver.HcdPlatformDriver.bus    = &platform_bus_type,
   .Driver.HcdPlatformDriver.probe  = Probe,
   .Driver.HcdPlatformDriver.remove = Remove, 
};

#define OMAP_MCBSP_NO_CONTEXT (void*)0x55AAAA55
/*
 * Probe - probe to setup our device, if present
*/
static int Probe(struct device *pBusDevice)
{
    SYSTEM_STATUS err = 0;
    SDIO_STATUS   status = SDIO_STATUS_SUCCESS;
    PSDHCD_DEVICE pDeviceContext = NULL;
    PSDHCD_DEVICE pLastDeviceContext; 
    int ii;
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP HCD: Probe - probing for new device\n"));
 
    
    /* create a device for each slot that we have */
    for(ii = 0; ii < channels; ii++) {
        pLastDeviceContext = pDeviceContext;
        /* allocate a device context for this new device */
        pDeviceContext =  (PSDHCD_DEVICE)KernelAlloc(sizeof(SDHCD_DEVICE));
        if (pDeviceContext == NULL) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP HCD: Probe - no memory for device context\n"));
            err = -ENOMEM;
            break;
        }
        ZERO_POBJECT(pDeviceContext);
        SDLIST_INIT(&pDeviceContext->List);
        pDeviceContext->pBusDevice = pBusDevice;
        
        SET_SDIO_STACK_VERSION(&pDeviceContext->Hcd);
        pDeviceContext->Hcd.pName = (PTEXT)KernelAlloc(SDHCD_MAX_DEVICE_NAME+1);
        snprintf(pDeviceContext->Hcd.pName, SDHCD_MAX_DEVICE_NAME, SDIO_BD_BASE"%i:%i",
                 pHcdContext->DeviceCount++, ii);
        if (disable_polling != 0) {
            hcdattributes &= ~SDHCD_ATTRIB_SLOT_POLLING;
        }                 
        pDeviceContext->Hcd.Attributes = hcdattributes;
        pDeviceContext->Hcd.pContext = pDeviceContext;
        pDeviceContext->Hcd.pRequest = HcdRequest;
        pDeviceContext->Hcd.pConfigure = HcdConfig;
        pDeviceContext->Hcd.pDevice = pBusDevice;
        pDeviceContext->Hcd.pModule = THIS_MODULE;
        pDeviceContext->BaseClock = base_clock;
        pDeviceContext->Hcd.MaxBytesPerBlock = OMAPMCBSP_MAX_BYTES_PER_BLOCK;
        pDeviceContext->Hcd.MaxBlocksPerTrans = max_blocks;
        pDeviceContext->Hcd.MaxSlotCurrent = OMAPMCBSP_DEFAULT_CURRENT; 
        pDeviceContext->Hcd.SlotVoltageCaps = OMAPMCBSP_DEFAULT_VOLTAGE;
        pDeviceContext->Hcd.SlotVoltagePreferred = OMAPMCBSP_DEFAULT_VOLTAGE;
        pDeviceContext->Hcd.MaxClockRate = 25000000; /* 25 Mhz */ ; 
        pDeviceContext->MinClockDiv = min_clock;
        pDeviceContext->ShuttingDown = FALSE;
        spin_lock_init(&pDeviceContext->IrqLock);
        
        pDeviceContext->Hcd.pConfigure = HcdConfig;
        switch(ii) {
            case 0:
                pDeviceContext->Channel = channel0;
                break;
            case 1:
                pDeviceContext->Channel = channel1;
                break;
            case 2:
                pDeviceContext->Channel = channel2;
                break;
            case 3:
                pDeviceContext->Channel = channel3;
                break;
                    
        }
        pDeviceContext->pHcdPlatformDriver = &pHcdContext->Driver.HcdPlatformDriver;
        
        /* add device to our list of devices */
            /* protect the devicelist */
        if (!SDIO_SUCCESS(status = SemaphorePendInterruptable(&pHcdContext->DeviceListSem))) {
            break;;   /* wait interrupted */
        }
        SDListInsertTail(&pHcdContext->DeviceList, &pDeviceContext->List); 
        SemaphorePost(&pHcdContext->DeviceListSem);
        
        /* initialize work items */
        INIT_WORK(&(pDeviceContext->iocomplete_work), hcd_iocomplete_wqueue_handler, pDeviceContext);
        INIT_WORK(&(pDeviceContext->carddetect_work), hcd_carddetect_wqueue_handler, pDeviceContext);
        INIT_WORK(&(pDeviceContext->sdioirq_work), hcd_sdioirq_wqueue_handler, pDeviceContext);
        INIT_WORK(&(pDeviceContext->dmairq_work), hcd_dmairq_wqueue_handler, pDeviceContext);

        /* map the controller interrupt, we map it to each device. 
           Interrupts can be called from this point on */
//??        err = request_irq(pPCIdevice->irq, hcd_sdio_irq, SA_SHIRQ,
//??                          pDeviceContext->DeviceName, pDeviceContext);
//??        if (err < 0) {
//??              DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP HCD: probe, unable to map interrupt \n"));
//??              err = -ENODEV;
//??              break;
//??        }
//??        pDeviceContext->InitStateMask |= SDIO_IRQ_INTERRUPT_INIT;

        pDeviceContext->InitStateMask |= SDHC_HW_INIT;

        if (!SDIO_SUCCESS((status = InitializeMcBSP(pDeviceContext)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP Probe - failed to init DMA, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
        pDeviceContext->InitStateMask |= SDHC_DMA_INIT;
        if (!SDIO_SUCCESS((status = HcdInitialize(pDeviceContext)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP Probe - failed to init HW, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
               
        pDeviceContext->InitStateMask |= SDHC_HW_INIT;
        
           /* register with the SDIO bus driver */
        if (!SDIO_SUCCESS((status = SDIO_RegisterHostController(&pDeviceContext->Hcd)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP HCD: Probe - failed to register with host, status =%d\n",status));
            err = SDIOErrorToOSError(status);
            break;
        }      
        pDeviceContext->InitStateMask |= SDHC_REGISTERED;
        
       DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP McBSP HCD: Probe - name: %s, shutdown: %d, pDC: 0x%X, pDC->Hcd.pC: 0x%X HCDL 0x%X work data: 0x%X\n",
            pDeviceContext->Hcd.pName, (UINT)pDeviceContext->ShuttingDown, 
            (UINT)pDeviceContext, (UINT)pDeviceContext->Hcd.pContext, (UINT)&pDeviceContext->Hcd, 
            (UINT)pDeviceContext->iocomplete_work.data));

    }
    
    if (err < 0) {
        Remove(pBusDevice); /* TODO: the cleanup should not really be done in the Remove function */
    } else {
           /* TODO: check and see if there is a card inserted at powerup */
            /* queue a work item to check for a card present at start up
               this call will unmask the insert/remove interrupts */
        if (!(pDeviceContext->Hcd.Attributes & SDHCD_ATTRIB_SLOT_POLLING))                {
            QueueEventResponse(pDeviceContext, WORK_ITEM_CARD_DETECT);//??
        }
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAPMcBSP HCD: Probe - err:%d\n", err));
    return err;  
}

/* Remove - remove  device
 * perform the undo of the Probe
*/
static int Remove(struct device *pBusDevice) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP HCD: Remove - removing device\n"));
    
    RemoveDevice(pBusDevice, pHcdContext);
    pHcdContext->DeviceCount--;
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAPMcBSP HCD: Remove\n"));
    return 0;
}

/*
 * RemoveDevice - remove all devices associated with bus device
*/
static void RemoveDevice(struct device *pBusDevice, PSDHCD_DRIVER_CONTEXT pHcdContext)
{
    PSDHCD_DEVICE pDeviceContext; 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP HCD: RemoveDevice\n"));
    
    /* protect the devicelist */
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pHcdContext->DeviceListSem))) {
        return;   /* wait interrupted */
    }
    
    SDITERATE_OVER_LIST_ALLOW_REMOVE(&pHcdContext->DeviceList, pDeviceContext, SDHCD_DEVICE, List)
        if (pDeviceContext->pBusDevice == pBusDevice) {
            if (pDeviceContext->InitStateMask & SDHC_HW_INIT) {
                HcdDeinitialize(pDeviceContext);
            }
            if (pDeviceContext->InitStateMask & SDHC_REGISTERED) {
                    /* unregister from the bus driver */
                SDIO_UnregisterHostController(&pDeviceContext->Hcd);
            }

            /* wait for any of our work items to run */
            flush_scheduled_work();
            
            if (pDeviceContext->InitStateMask & SDHC_DMA_INIT) {
                DeinitializeMcBSP(pDeviceContext);
            }

            pDeviceContext->InitStateMask = 0;

            if (pDeviceContext->Hcd.pName != NULL) {
                KernelFree(pDeviceContext->Hcd.pName);
                pDeviceContext->Hcd.pName = NULL;
            }
            KernelFree(pDeviceContext);
        }
    SDITERATE_END;
    SemaphorePost(&pHcdContext->DeviceListSem);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAPMcBSP HCD: RemoveDevice\n"));
}

/*
 * QueueEventResponse - queues an event in a process context back to the bus driver
 * 
*/
SDIO_STATUS QueueEventResponse(PSDHCD_DEVICE pDeviceContext, INT WorkItemID)
{
    struct work_struct *work;
    
    switch (WorkItemID) {
        case WORK_ITEM_IO_COMPLETE:
            work = &pDeviceContext->iocomplete_work;
            
            break;
        case WORK_ITEM_CARD_DETECT:
            work = &pDeviceContext->carddetect_work;
            break;
        case WORK_ITEM_SDIO_IRQ:
            work = &pDeviceContext->sdioirq_work;
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
    
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)context;
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP HCD: hcd_iocomplete_wqueue_handler, pDevice: 0x%X pReq: 0x%X  HCD: 0x%X\n",
                            (UINT)pDevice, (UINT)GET_CURRENT_REQUEST(&pDevice->Hcd),  (UINT)&pDevice->Hcd));
    
    SDIO_HandleHcdEvent(&pDevice->Hcd, EVENT_HCD_TRANSFER_DONE);
}

/*
 * hcd_carddetect_handler - the work queue for card detect debouncing
*/
static void hcd_carddetect_wqueue_handler(void *context) 
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)context;
    HCD_EVENT event;
    pDevice->CardInserted = TRUE;
    event = EVENT_HCD_ATTACH;
    SDIO_HandleHcdEvent(&pDevice->Hcd, event);
#if 0
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    HCD_EVENT event;
    
    event = EVENT_HCD_NOP;
    
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("+ SDIO PXA255 Card Detect Work Item \n"));
    
    if (!pHcdContext->CardInserted) { 
        DBG_PRINT(PXA_TRACE_CARD_INSERT, ("Delaying to debounce card... \n"));
            /* sleep for slot debounce if there is no card */
        msleep(PXA_SLOT_DEBOUNCE_MS);
    }
                
    if (GetGpioPinLevel(pHcdContext,SDIO_CARD_INSERT_GPIO) == CARD_INSERT_POLARITY) {
    /* doesn't work on gumstix , we can only detect removal */
        if (!pHcdContext->CardInserted) {  
            pHcdContext->CardInserted = TRUE;
            event = EVENT_HCD_ATTACH;
            DBG_PRINT(PXA_TRACE_CARD_INSERT, (" Card Inserted! \n"));
        } else {
            DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already inserted card! \n"));
        }
    } else {
        if (pHcdContext->CardInserted) {  
            event = EVENT_HCD_DETACH;
            pHcdContext->CardInserted = FALSE; 
            DBG_PRINT(PXA_TRACE_CARD_INSERT, (" Card Removed! \n"));
                /* requeue the CD timer to start polling again*/
            QueueCDTimer(pHcdContext);
        } else {
            DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already removed card! \n"));
        }
    }
    
    if (event != EVENT_HCD_NOP) {
        SDIO_HandleHcdEvent(&pHcdContext->Hcd, event);
    }
    
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("- SDIO PXA255 Card Detect Work Item \n"));
#endif
}

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
*/
static void hcd_sdioirq_wqueue_handler(void *context) 
{
    PSDHCD_DEVICE pDeviceContext = (PSDHCD_DEVICE)context;
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAPMcBSP: hcd_sdioirq_wqueue_handler \n"));
    SDIO_HandleHcdEvent(&pDeviceContext->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
}


/* SDIO interrupt request */
static irqreturn_t SD_irq_handler(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP SD_irq_handler\n"));
    
    /* check gpio pin for assertion */
        /* disable IRQ */
    disable_irq(OMAP_GPIO_IRQ(irq_pin));
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP SDIO IRQ Asserted.. Queueing Work Item \n"));
        /* queue the work item to notify the bus driver*/
    if (!SDIO_SUCCESS(QueueEventResponse((PSDHCD_DEVICE)context, WORK_ITEM_SDIO_IRQ))) {
            /* failed */
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP McBSP: HcdInterrupt - queue event failed\n"));
    }
    return IRQ_HANDLED;
}

/*
 * AckSDIOIrq - acknowledge SDIO interrupt
*/
SDIO_STATUS AckSDIOIrq(PSDHCD_DEVICE pDevice)
{
    ULONG flags;
   
        /* block the SDIO-IRQ handler from running */ 
    spin_lock_irqsave(&pDevice->IrqLock, flags);   
        /* re-enable edge detection */
    EnableDisableSDIOIrq(pDevice, TRUE);    

        /* delay enough so that we can sample a level interrupt if it's already
         * asserted */
    udelay(2);
    
    if (omap_get_gpio_datain(irq_pin) == 0) { 
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP: pending int during Ack, issuing event \n"));
            /* disable SDIO irq detection */   
        EnableDisableSDIOIrq(pDevice, FALSE);    
        spin_unlock_irqrestore(&pDevice->IrqLock, flags); 
             /* queue work item to process another interrupt */
        return QueueEventResponse(pDevice, WORK_ITEM_SDIO_IRQ);
    }
        /* let the normal GPIO edge detect take over */
    spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
    return SDIO_STATUS_SUCCESS;
}

SDIO_STATUS EnableDisableSDIOIrq(PSDHCD_DEVICE pDevice, BOOL Enable)
{
    if (Enable) {    
        enable_irq(OMAP_GPIO_IRQ(irq_pin));
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP - SDIO IRQ Detection Enabled!\n"));
    } else {
            /* to mask the GPIO PXA interrupt we clear the falling edge bit */
        disable_irq(OMAP_GPIO_IRQ(irq_pin));
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP - SDIO IRQ Detection Disabled!\n"));
    }
    
    return SDIO_STATUS_SUCCESS;  
}

#ifndef omap_get_dma_dst_pos
#define omap_get_dma_dst_pos(ch)\
    (dma_addr_t) (OMAP_DMA_CDSA_L(ch) | (OMAP_DMA_CDSA_U(ch) << 16))
#endif
void SDDmaTimeOut(unsigned long context)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)context;
    ULONG   flags;
    
    spin_lock_irqsave(&pDevice->IrqLock, flags);   
    if ((volatile short)pDevice->pMcbspData->dma_rx_lch == -1) {
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);
        DBG_PRINT(SDDBG_WARN, ("SDIO OMAP McBSP - SDDmaTimeOut: DMA timeout without timeout??\n"));
        return;
    }
    pDevice->LastDstAddr = (ULONG)omap_get_dma_dst_pos(pDevice->pMcbspData->dma_rx_lch);
    pDevice->DmaTimeoutCount++;
    DBG_PRINT(SDDBG_WARN, ("SDIO OMAP McBSP - SDDmaTimeOut****: count: %d, dst address: 0x%X, start addres: 0x%X/0x%X\n",
              (UINT)pDevice->DmaTimeoutCount, (UINT)pDevice->LastDstAddr, (UINT)pDevice->hDmaDataBuffer, (UINT)pDevice->hDmaCommandBuffer));
    omap_stop_dma(pDevice->pMcbspData->dma_rx_lch);
    omap_free_dma(pDevice->pMcbspData->dma_rx_lch);
    RxWork(pDevice);
    pDevice->pMcbspData->dma_rx_lch = -1;
    spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
    schedule_work(&pDevice->dmairq_work);
}


static SDIO_STATUS InitializeMcBSP(PSDHCD_DEVICE pDevice) 
{
    SYSTEM_STATUS err;
    UINT temp;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP InitializeMcBSP\n"));
    
    /* allocate a DMA buffer larger enough for the command buffers and the data buffers */
    pDevice->pDmaCommandBuffer =  dma_alloc_coherent(pDevice->pBusDevice, 
                                    OMAPMCBSP_DMA_BUFFER_SIZE+3, 
                                    &pDevice->hDma, 
                                    GFP_DMA);
    if (pDevice->pDmaCommandBuffer == NULL) {
        return SDIO_STATUS_NO_RESOURCES;
    }
    /* set the offsets for the buffers */
    pDevice->hDmaCommandBuffer = pDevice->hDma;
    temp = (OMAPMCBSP_COMMAND_RESPONSE_BUFFER_SIZE + 3) & 0xFFFFFFF0;
    pDevice->pDmaDataBuffer = pDevice->pDmaCommandBuffer + temp;
    pDevice->hDmaDataBuffer = (dma_addr_t)((PUINT8)pDevice->hDma + temp);

    init_timer(&pDevice->DmaTimeOut);
    pDevice->DmaTimeOut.function = SDDmaTimeOut;
    pDevice->DmaTimeOut.data = (unsigned long)pDevice;
    

    /* enable the dma clock */
    pDevice->pDmaClock = clk_get( pDevice->Hcd.pDevice, "dma_ck");
    if (IS_ERR(pDevice->pDmaClock)) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP InitializeMcBSP: can't get dma clock, value: %d\n",
                  (UINT)pDevice->pDmaClock));
        return SDIO_STATUS_NO_RESOURCES;
    }
    
    omap_cfg_reg(MCBSP2_CLKX);
    omap_cfg_reg(MCBSP2_DR);
    omap_cfg_reg(MCBSP2_DX);
//??    omap_cfg_reg(MCBSP2_FSX);
//??    omap_cfg_reg(MCBSP2_FSR);
//??    omap_cfg_reg(V7_GPIO_11);
    omap_cfg_reg(W7_GPIO_21);
    omap_set_gpio_direction(OMAPMCBSP_CHIPSELECT_PIN, 0);
    omap_set_gpio_dataout(OMAPMCBSP_CHIPSELECT_PIN, 1);
    
    omap_cfg_reg(W8_1610_GPIO9);
    if (omap_request_gpio(irq_pin) < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP InitializeMcBSP: can't get GPIO IRQ pin %d\n",
                  irq_pin));
        return SDIO_STATUS_NO_RESOURCES;
    }
    omap_set_gpio_direction(irq_pin, 1);
    omap_set_gpio_edge_ctrl(irq_pin, OMAP_GPIO_FALLING_EDGE);
    err = request_irq(OMAP_GPIO_IRQ(irq_pin), SD_irq_handler, 0, pDevice->Hcd.pName, pDevice);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP InitializeMcBSP: can't allocate GPIO IRQ pin %d, err: %d\n",
                  irq_pin, err));
        return SDIO_STATUS_NO_RESOURCES;
    }
DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP InitializeMcBSP: 1\n"));
    
#define OMAP_DMA_GDD_BASE   0x30001000
#define OMAP_DMA_GDD_GCR    OMAP_DMA_GDD_BASE+0x100
#define OMAP_DMA_GDD_GCR_AUTOGATING_ON (1 << 3)
    {
        UINT32 gcr;
        PUINT32 pMapped;
        pMapped = ioremap_nocache(OMAP_DMA_GDD_GCR, sizeof(UINT32));
        if (pMapped == NULL) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP InitializeMcBSP: can't map\n"));
        } else {
            gcr = (volatile UINT32)(*pMapped);
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP InitializeMcBSP: GCR: 0x%X\n", gcr));
//??            *pMapped = (gcr & ~OMAP_DMA_GDD_GCR_AUTOGATING_ON);
            *pMapped = (gcr | OMAP_DMA_GDD_GCR_AUTOGATING_ON);
            gcr = (volatile UINT32)(*pMapped);
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP InitializeMcBSP: GCR: 0x%X\n", gcr));
            iounmap(pMapped);
        }
    }
DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP InitializeMcBSP: 2\n"));
    pDevice->InitStateMask = SDHC_SDIRQ_INIT;
    EnableDisableSDIOIrq(pDevice, FALSE);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAPMcBSP InitializeMcBSP\n"));
    return SDIO_STATUS_SUCCESS;
}

static void DeinitializeMcBSP(PSDHCD_DEVICE pDevice) 
{
    if (pDevice->InitStateMask & SDHC_SDIRQ_INIT) {
        free_irq(OMAP_GPIO_IRQ(irq_pin), pDevice);
    }

    if (pDevice->hDma != (DMA_ADDRESS)NULL) {
        dma_free_coherent(pDevice->pBusDevice, 
                          OMAPMCBSP_DMA_BUFFER_SIZE,
                          pDevice->pDmaCommandBuffer,
                          pDevice->hDma);
        pDevice->hDma = (DMA_ADDRESS)NULL;
        pDevice->hDmaCommandBuffer = (DMA_ADDRESS)NULL;
        pDevice->hDmaDataBuffer = (DMA_ADDRESS)NULL;
        pDevice->pDmaDataBuffer = NULL;
        pDevice->pDmaCommandBuffer = NULL;
    }
    if (!IS_ERR(pDevice->pDmaClock)) {
        clk_put(pDevice->pDmaClock);
    } 
    omap_free_gpio(irq_pin);
}


//??#if 0 //???????????????????????????????????????????????Place in mcbsp.c with delays passed in.
void SDomap_mcbsp_set_spi_mode(unsigned int id, const struct omap_mcbsp_spi_cfg * spi_cfg, unsigned int tx_delay, unsigned int rx_delay)
{
         struct omap_mcbsp_reg_cfg mcbsp_cfg;
 
//??         if (omap_mcbsp_check(id) < 0)
//??                 return;
 
         memset(&mcbsp_cfg, 0, sizeof(struct omap_mcbsp_reg_cfg));
 
         /* SPI has only one frame */
         mcbsp_cfg.rcr1 |= (RWDLEN1(spi_cfg->word_length) | RFRLEN1(0));
         mcbsp_cfg.xcr1 |= (XWDLEN1(spi_cfg->word_length) | XFRLEN1(0));
 
         /* Clock stop mode */
         if (spi_cfg->clk_stp_mode == OMAP_MCBSP_CLK_STP_MODE_NO_DELAY)
                 mcbsp_cfg.spcr1 |= (1 << 12);
         else
                 mcbsp_cfg.spcr1 |= (3 << 11);
 
         /* Set clock parities */
         if (spi_cfg->rx_clock_polarity == OMAP_MCBSP_CLK_RISING)
                 mcbsp_cfg.pcr0 |= CLKRP;
         else
                 mcbsp_cfg.pcr0 &= ~CLKRP;
 
         if (spi_cfg->tx_clock_polarity == OMAP_MCBSP_CLK_RISING)
                 mcbsp_cfg.pcr0 &= ~CLKXP;
         else
                 mcbsp_cfg.pcr0 |= CLKXP;
 
         /* Set SCLKME to 0 and CLKSM to 1 */
         mcbsp_cfg.pcr0 &= ~SCLKME;
         mcbsp_cfg.srgr2 |= CLKSM;
 
         /* Set FSXP */
         if (spi_cfg->fsx_polarity == OMAP_MCBSP_FS_ACTIVE_HIGH)
                 mcbsp_cfg.pcr0 &= ~FSXP;
         else
                 mcbsp_cfg.pcr0 |= FSXP;
 
         if (spi_cfg->spi_mode == OMAP_MCBSP_SPI_MASTER) {
                 mcbsp_cfg.pcr0 |= CLKXM;
                 mcbsp_cfg.srgr1 |= CLKGDV(spi_cfg->clk_div -1);
                 mcbsp_cfg.pcr0 |= FSXM;
                 mcbsp_cfg.srgr2 &= ~FSGM;
//??                 mcbsp_cfg.xcr2 |= XDATDLY(1);
//??                 mcbsp_cfg.rcr2 |= RDATDLY(1);
                 mcbsp_cfg.xcr2 |= XDATDLY(tx_delay);
                 mcbsp_cfg.rcr2 |= RDATDLY(rx_delay);
//??                 mcbsp_cfg.rcr2 |= RFIG;
         }
         else {
                 mcbsp_cfg.pcr0 &= ~CLKXM;
                 mcbsp_cfg.srgr1 |= CLKGDV(1);
                 mcbsp_cfg.pcr0 &= ~FSXM;
                 mcbsp_cfg.xcr2 &= ~XDATDLY(1);
                 mcbsp_cfg.rcr2 &= ~RDATDLY(1);
         }
 
         mcbsp_cfg.xcr2 &= ~XPHASE;
         mcbsp_cfg.rcr2 &= ~RPHASE;
 
         omap_mcbsp_config(id, &mcbsp_cfg);
 }
//??#endif //????????????????????????

SDIO_STATUS SDSetClock(PSDHCD_DEVICE pDevice)
{
    struct omap_mcbsp_spi_cfg config = {
        .spi_mode           = OMAP_MCBSP_SPI_MASTER,
        .rx_clock_polarity  = OMAP_MCBSP_CLK_RISING,
        .tx_clock_polarity  = OMAP_MCBSP_CLK_FALLING,
        .fsx_polarity       = OMAP_MCBSP_FS_ACTIVE_LOW,
        .clk_stp_mode       = OMAP_MCBSP_CLK_STP_MODE_NO_DELAY,
        .word_length        = OMAP_MCBSP_WORD_16,
    };
    DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO OMAPMcBSP SDSetClock clock: %d, channel: %d\n", 
            pDevice->Clock, pDevice->Channel));
    config.clk_div = pDevice->Clock+1;
    SDomap_mcbsp_set_spi_mode(pDevice->Channel, &config, 0, 0);
    
    DBG_PRINT(PXA_TRACE_REQUESTS, ("-SDIO OMAPMcBSP SDSetClock\n"));
    return SDIO_STATUS_SUCCESS;
}

SDIO_STATUS SDStartStopClock(PSDHCD_DEVICE pDevice, BOOL Start)
{
    DBG_PRINT(PXA_TRACE_REQUESTS, ("+SDIO OMAPMcBSP SDStartStopClock: %s, Clock div: %d\n", 
              (Start)?"start":"stop", pDevice->Clock));
    if (Start) {
        omap_mcbsp_start(pDevice->Channel);
    } else {
        omap_mcbsp_stop(pDevice->Channel);
    }
    DBG_PRINT(PXA_TRACE_REQUESTS, ("-SDIO OMAPMcBSP SDStartStopClock: %s\n", 
              (Start)?"start":"stop"));
    return SDIO_STATUS_SUCCESS;
}

static void hcd_dmairq_wqueue_handler(void *context)
{
    ULONG         flags;
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)context;
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAPMcBSP: hcd_dmairq_wqueue_handler\n"));

    spin_lock_irqsave(&pDevice->IrqLock, flags);
    if ((volatile short)pDevice->pMcbspData->dma_rx_lch != -1) {
        spin_unlock_irqrestore(&pDevice->IrqLock, flags); 
        /* wait for it too be done */
        while((volatile short)pDevice->pMcbspData->dma_rx_lch != -1) {
            ;
        }
    } else {
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
    }
    spin_lock_irqsave(&pDevice->IrqLock, flags);
    if ((volatile short)pDevice->pMcbspData->dma_tx_lch != -1) {
        spin_unlock_irqrestore(&pDevice->IrqLock, flags); 
        /* wait for it too be done */
        while((volatile short)pDevice->pMcbspData->dma_tx_lch != -1) {
            ;
        }
    } else {
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
    }

    omap_free_dma(pDevice->TxDmaSave);
    omap_set_gpio_dataout(OMAPMCBSP_CHIPSELECT_PIN, 1);
    SDStartStopClock(pDevice, CLOCK_OFF);  

    spin_lock_irqsave(&pDevice->IrqLock, flags);
    if (pDevice->pTransferCompletion != NULL) {
        void (*pTempCompletion)(PVOID pContext) = pDevice->pTransferCompletion;
        pDevice->pTransferCompletion = NULL;
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP hcd_dmairq_wqueue_handler - calling complete\n"));
        pTempCompletion(pDevice->TransferContext);
        return;
    } else if (pDevice->TransferContext == OMAP_MCBSP_NO_CONTEXT) {
        pDevice->TransferContext = 0;
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO OMAP McBSP hcd_dmairq_wqueue_handler - completing\n"));
        complete(&pDevice->TransferComplete);
        return;
    }
    spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
}


static void RxWork(PSDHCD_DEVICE pDevice) 
{
    UINT          ii;
    /* swap the bytes in the receive buffer to deal with the 16-bit dma transfers */
    for(ii = 0; ii < pDevice->RxLength/2; ii++) {
        ((PUINT16)pDevice->pRxData)[ii] = swab16(((PUINT16)pDevice->pRxData)[ii]);
    }
    if (debuglevel >= PXA_TRACE_REQUESTS) {
        SDLIB_PrintBuffer(pDevice->pRxData, pDevice->RxLength, "SDIO OMAPMcBSP SD_RxCallback RX buffer");
    }
}

static void SD_RxCallback(int lch, u16 status, PVOID pContext)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    ULONG         flags;

    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SD_RxCallback - status: %d, tx: %d, lch: %d, 0x%X, 0x%X\n", 
             (UINT)status, pDevice->pMcbspData->dma_rx_lch, lch,
             (UINT)omap_get_dma_dst_pos(pDevice->pMcbspData->dma_rx_lch),
             (UINT)omap_get_dma_pos(pDevice->pMcbspData->dma_rx_lch)));
    spin_lock_irqsave(&pDevice->IrqLock, flags);   
    del_timer(&pDevice->DmaTimeOut);
    /* swap the bytes in the receive buffer to deal with the 16-bit dma transfers */
    RxWork(pDevice);
    omap_free_dma(pDevice->pMcbspData->dma_rx_lch);
    pDevice->pMcbspData->dma_rx_lch = -1;
    spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
    schedule_work(&pDevice->dmairq_work);
    return;    
}

static void SD_TxCallback(int lch, u16 status, PVOID pContext)
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    ULONG         flags;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SD_TxCallback - status: %d, tx: %d, lch: %d\n", 
             (UINT)status, pDevice->pMcbspData->dma_tx_lch, lch));
    spin_lock_irqsave(&pDevice->IrqLock, flags);   
    pDevice->TxDmaSave = pDevice->pMcbspData->dma_tx_lch;
    pDevice->pMcbspData->dma_tx_lch = -1;
    spin_unlock_irqrestore(&pDevice->IrqLock, flags);
    return;
}


SDIO_STATUS SDTransferBuffers(PSDHCD_DEVICE pDevice, 
                              PUINT8 pTxData, DMA_ADDRESS TxDmaAddress,
                              PUINT8 pRxData, DMA_ADDRESS RxDmaAddress,
                              UINT Length,
                              void (*pCompletion)(PVOID pContext), PVOID pContext)
{
    SYSTEM_STATUS err;
    ULONG         flags;
    
    struct omap_mcbsp * pMcbspData = omap_mcbsp_get_config(pDevice->Channel);
    int dmaChannelTx;
    int dmaChannelRx;
    int ii;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP SDTransferBuffers: length: %d, TO: %d\n",
              Length, pDevice->DmaTimeoutCount));
    DBG_PRINT(SDDBG_TRACE, ("TxDmaAddress: 0x%X, RxDmaAddress: 0x%X\n",
              (UINT)TxDmaAddress, (UINT)RxDmaAddress));
    
    /* must have an even length */
    Length = (Length & 0x1) ? Length + 1 : Length;
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SDTransferBuffers: updated length: %d\n", Length));
    
    if (IS_ERR(pMcbspData)) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP SDTransferBuffers: can't get McBSP data: %d\n",
                  (UINT)pMcbspData));
        return SDIO_STATUS_NO_RESOURCES;
    }
    if (pCompletion == NULL) {
        /* no completion, make synchronous */
        pDevice->pTransferCompletion  = NULL;
        pDevice->TransferContext = OMAP_MCBSP_NO_CONTEXT;
        init_completion(&pDevice->TransferComplete);
    } else {
        pDevice->pTransferCompletion  = pCompletion;
        pDevice->TransferContext = pContext;
    }
    pDevice->pMcbspData = pMcbspData;
    pDevice->pRxData = pRxData;
    pDevice->RxLength = Length;
    /* swap the bytes in the input buffer to deal with the 16-bit dma transfers */
    if (debuglevel >= PXA_TRACE_REQUESTS) {
        SDLIB_PrintBuffer(pTxData, Length, "SDIO OMAPMcBSP SDTransferBuffers TX buffer 0 ");
    }
    for(ii = 0; ii < Length/2; ii++) {
       ((PUINT16)pTxData)[ii] = swab16(((PUINT16)pTxData)[ii]);
    }
    spin_lock_irqsave(&pDevice->IrqLock, flags);   
    if ((volatile short)pMcbspData->dma_tx_lch == -1) {
         spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
         err = omap_request_dma(pMcbspData->dma_tx_sync, "SDIO TX", SD_TxCallback, pDevice, &dmaChannelTx);
         if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP SDTransferBuffers: can't get TX DMA channel: %d\n",
                      err));
            return SDIO_STATUS_NO_RESOURCES;
         }
         pMcbspData->dma_tx_lch = dmaChannelTx;
     } else {
        /* wait for any previous dma to finish */
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP SDTransferBuffers: TX dma channel busy\n"));
        return SDIO_STATUS_NO_RESOURCES;         
     }
     omap_set_dma_transfer_params(pMcbspData->dma_tx_lch,
                                  OMAP_DMA_DATA_TYPE_S16,
                                   1, Length>>1,
                                  OMAP_DMA_SYNC_ELEMENT);
 
     omap_set_dma_dest_params(pMcbspData->dma_tx_lch,
                              OMAP_DMA_PORT_TIPB,
                              OMAP_DMA_AMODE_CONSTANT,
                              pMcbspData->io_base + OMAP_MCBSP_REG_DXR1);
 
     omap_set_dma_src_params(pMcbspData->dma_tx_lch,
                             OMAP_DMA_PORT_EMIFF,
                             OMAP_DMA_AMODE_POST_INC,
                             TxDmaAddress);

     spin_lock_irqsave(&pDevice->IrqLock, flags);   
     if ((volatile short)pMcbspData->dma_rx_lch == -1) {
         spin_unlock_irqrestore(&pDevice->IrqLock, flags);     
         err = omap_request_dma(pMcbspData->dma_rx_sync, "SDIO RX", SD_RxCallback, pDevice, &dmaChannelRx);
         if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP SDTransferBuffers: can't get RX DMA channel: %d\n",
                      err));
            omap_free_dma(pDevice->pMcbspData->dma_tx_lch);
            return SDIO_STATUS_NO_RESOURCES;
         }
             
         pMcbspData->dma_rx_lch = dmaChannelRx;
     } else {
         /* wait for any previous dma to finish */
        spin_unlock_irqrestore(&pDevice->IrqLock, flags);
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP SDTransferBuffers: RX dma channel busy\n"));
        return SDIO_STATUS_NO_RESOURCES;         
     }
     DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SDTransferBuffers: DMA channels TX: %d, RX: %d\n",
               pMcbspData->dma_tx_lch, pMcbspData->dma_rx_lch));
 
     omap_set_dma_transfer_params(pMcbspData->dma_rx_lch,
                                  OMAP_DMA_DATA_TYPE_S16,
                                  1, Length>>1,
                                  OMAP_DMA_SYNC_ELEMENT);
 
     omap_set_dma_src_params(pMcbspData->dma_rx_lch,
                             OMAP_DMA_PORT_TIPB,
                             OMAP_DMA_AMODE_CONSTANT,
                             pMcbspData->io_base + OMAP_MCBSP_REG_DRR1);
 
     omap_set_dma_dest_params(pMcbspData->dma_rx_lch,
                              OMAP_DMA_PORT_EMIFF,
                              OMAP_DMA_AMODE_POST_INC,
                              RxDmaAddress);
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SDTransferBuffers - addr: 0x%X, 0x%X\n", 
             (UINT)omap_get_dma_dst_pos(pDevice->pMcbspData->dma_rx_lch),
             (UINT)omap_get_dma_pos(pDevice->pMcbspData->dma_rx_lch)));
                              
     spin_lock_irqsave(&pDevice->IrqLock, flags);   

        /* start receive DMA first */
     omap_start_dma(pMcbspData->dma_rx_lch);
        /* then start TX DMA */
//??if (Length == 50) omap_set_gpio_dataout(9, 1);
//??if (pTxData[1] == 0x40) omap_set_gpio_dataout(9, 1);
     omap_start_dma(pMcbspData->dma_tx_lch);
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SDTransferBuffers - addr: 0x%X, 0x%X\n", 
             (UINT)omap_get_dma_dst_pos(pDevice->pMcbspData->dma_rx_lch),
             (UINT)omap_get_dma_pos(pDevice->pMcbspData->dma_rx_lch)));
//??if (pTxData[1] == 0x40) omap_set_gpio_dataout(9, 0);
//??if (Length == 50) omap_set_gpio_dataout(9, 0);
     /* add time out timer for lost RX DMA interrupts */
     pDevice->DmaTimeOut.expires = OMAPMCBSP_DMA_TIMEOUT + jiffies;
     add_timer(&pDevice->DmaTimeOut);
     
     spin_unlock_irqrestore(&pDevice->IrqLock, flags);
     omap_set_gpio_dataout(OMAPMCBSP_CHIPSELECT_PIN, 0);
     udelay(1);
     SDStartStopClock(pDevice, CLOCK_ON);

     
     if (pCompletion == NULL) {
        /* synchcronous call, wait for completion */
        wait_for_completion(&pDevice->TransferComplete);
        return SDIO_STATUS_SUCCESS;
     } else {
         DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP SDTransferBuffers: return pending, transfer size: %d\n",
                   Length));
         return SDIO_STATUS_PENDING;
     }
}

/*
 * hcd_release_device - device release call
*/
static void hcd_release_device(struct device *pDevice)
{
    /* nothing to do, but a required function */
}

/*
 * module init
*/
static int __init sdio_local_hcd_init(void) {
    SYSTEM_STATUS err;
    SDIO_STATUS status; 
    
    REL_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP Local HCD: loaded\n"));

    SDLIST_INIT(&HcdContext.DeviceList);
    status = SemaphoreInitialize(&HcdContext.DeviceListSem, 1);
    if (!SDIO_SUCCESS(status)) {
       return SDIOErrorToOSError(status);
    }       
    
    /* register with the local bus driver */
    err = driver_register(&HcdContext.Driver.HcdPlatformDriver);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP Local HCD: failed to register with system bus driver, %d\n",
                                err));
    }
    /* tell bus driver we have a device */
    err = platform_device_register(&HcdContext.Driver.HcdPlatformDevice);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAPMcBSP Local HCD: failed to register device with system bus driver, %d\n",
                                err));
    }
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAPMcBSP Local HCD: sdio_local_hcd_init exit\n"));
    return err;
}

/*
 * module cleanup
*/
static void __exit sdio_local_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SDIO OMAPMcBSP Local HCD: unloaded\n"));
    platform_device_unregister(&HcdContext.Driver.HcdPlatformDevice);
    driver_unregister(&HcdContext.Driver.HcdPlatformDriver);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAPMcBSP Local HCD: leave sdio_local_hcd_cleanup\n"));
}

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_local_hcd_init);
module_exit(sdio_local_hcd_cleanup);

