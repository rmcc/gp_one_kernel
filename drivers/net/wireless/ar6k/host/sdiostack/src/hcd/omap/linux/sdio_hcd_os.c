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
@file: sdio_hcd_os.c

@abstract: Linux OMAP native SDIO Host Controller Driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#define DBG_DECLARE 4;
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/arch/mux.h>
#include <linux/dma-mapping.h>
#include <asm/arch/board.h>
#include <asm/arch/gpio.h>
#include <asm/arch/tps65010.h>
#else
#include <asm/arch/irq.h>
#endif

#define DESCRIPTION "SDIO OMAP HCD"
#define AUTHOR "Atheros Communications, Inc."

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int Probe(struct pnp_dev *pBusDevice, const struct pnp_device_id *pId);
static void Remove(struct pnp_dev *pBusDevice);
#else
static int Probe(POS_PNPDEVICE pBusDevice, const PUINT pId);
static void Remove(POS_PNPDEVICE pBusDevice);
#endif

static void RemoveDevice(POS_PNPDEVICE pBusDevice, PSDHCD_DRIVER_CONTEXT pHcdContext);
SDIO_STATUS InitOmap(PSDHCD_DEVICE pDevice, UINT deviceNumber);
void DeinitOmap(PSDHCD_DEVICE pDevice);

static void hcd_iocomplete_wqueue_handler(void *context);
static void hcd_carddetect_wqueue_handler(void *context);
static void hcd_sdioirq_wqueue_handler(void *context);

/* debug print parameter */ 
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

/* Note due to a clocking bug, the 1610 host controller cannot support
 * 4 bit SDIO card interrupts so we remove this attribute:
 *     SDHCD_ATTRIB_MULTI_BLK_IRQ
 * And set this attribute:
 *     SDHCD_ATTRIB_NO_4BIT_IRQ
 */
#define DEFAULT_ATTRIBUTES (SDHCD_ATTRIB_BUS_1BIT  |  SDHCD_ATTRIB_BUS_4BIT | \
                            SDHCD_ATTRIB_POWER_SWITCH   | \
                            0)  
                            
static UINT32 hcdattributes = DEFAULT_ATTRIBUTES;

module_param(hcdattributes, int, 0644);
MODULE_PARM_DESC(hcdattributes, "OMAP Attributes");
static UINT32 base_clock = OMAP_MODULE_CLOCK;
module_param(base_clock, int, 0444);
MODULE_PARM_DESC(base_clock, "BaseClock Hz ");
static UINT32 timeout = OMAP_DEFAULT_CMD_TIMEOUT;
module_param(timeout, int, 0644);
MODULE_PARM_DESC(timeout, "OMAP command timeout");
static UINT32 data_timeout = OMAP_DEFAULT_DATA_TIMEOUT;
module_param(data_timeout, int, 0644);
MODULE_PARM_DESC(data_timeout, "OMAP data timeout");
static UINT32 device_count = OMAP_DEFAULT_DEVICE_COUNT;
module_param(device_count, int, 0644);
MODULE_PARM_DESC(device_count, "OMAP number of devices");
static UINT32 first_device = OMAP_DEFAULT_FIRST_DEVICE;
module_param(first_device, int, 0644);
MODULE_PARM_DESC(first_device, "OMAP first device to create");
static UINT32 clock_spin_limit = HCD_COMMAND_MIN_POLLING_CLOCK;
module_param(clock_spin_limit, int, 0644);
MODULE_PARM_DESC(clock_spin_limit, "OMAP command clock spin time");

static UINT32 max_sdbus_clock = OMAP_MODULE_CLOCK;
module_param(max_sdbus_clock, int, 0644);
MODULE_PARM_DESC(max_sdbus_clock, "OMAP max SDIO bus clock");

UINT32 max_blocks = OMAP_MAX_BLOCKS;
module_param(max_blocks, int, 0644);
MODULE_PARM_DESC(max_blocks, "OMAP Max Blocks Per Transfer");
UINT32 max_bytes_per_block = OMAP_MAX_BYTES_PER_BLOCK;
module_param(max_bytes_per_block, int, 0644);
MODULE_PARM_DESC(max_bytes_per_block, "OMAP Max Blocks per transfer");

INT gpiodebug = 0;
module_param(gpiodebug, int, 0444);
MODULE_PARM_DESC(gpiodebug, "Special GPIO debug");

INT noDMA = 0;
module_param(noDMA, int, 0444);
MODULE_PARM_DESC(noDMA, "Force No DMA");

UINT32 dma_buffer_size = 16*1024;
module_param(dma_buffer_size, int, 0644);
MODULE_PARM_DESC(dma_buffer_size, "OMAP common buffer DMA size");

UINT32 builtin_card = 0;
module_param(builtin_card, int, 0644);
MODULE_PARM_DESC(builtin_card, "SDIO card is built-in");

UINT32 async_irq = 0;
module_param(async_irq, int, 0644);
MODULE_PARM_DESC(async_irq, "Allow async IRQ detection in 4 bit mode");

/* the driver context data */
SDHCD_DRIVER_CONTEXT HcdContext = {
   .pDescription  = DESCRIPTION,
   .DeviceCount   = 0,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
   .Driver.HcdDevice.name = "sdio_omap_hcd",
   .Driver.HcdDriver.name = "sdio_omap_hcd",
   .Driver.HcdDriver.probe  = Probe, 
   .Driver.HcdDriver.remove = Remove,    
#endif
   .Driver.Dma.Mask = OMAP_DMA_MASK,
   .Driver.Dma.Flags = SDDMA_DESCRIPTION_FLAG_DMA,
   .Driver.Dma.MaxBytesPerDescriptor = 0xFFFFFFFF, /* the controller can DMA up to 4GB per DMA transfer*/
   .Driver.Dma.AddressAlignment = 0x01,  /* illegal address bits, buffers must be on even word bounadries */
   .Driver.Dma.LengthAlignment = 0x1,    /* illegal address bits, buffer lengths must be even */
   .Driver.Dma.MaxDescriptors = 1,       /* we don't suppport scatter-gather DMA, just a single buffer at a time */
};

/*
 * Probe - probe to setup our device, if present
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
int Probe(struct pnp_dev *pBusDevice, const struct pnp_device_id *pId)
#else
static int Probe(POS_PNPDEVICE pBusDevice, const PUINT pId)
#endif
{
    SYSTEM_STATUS err = 0;
    SDIO_STATUS   status = SDIO_STATUS_SUCCESS;
    PSDHCD_DEVICE pDeviceContext = NULL;
    int ii;
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAP HCD: Probe - probing for new device\n"));

    if (!async_irq) {
        hcdattributes |= SDHCD_ATTRIB_NO_4BIT_IRQ;
        DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: No 4-bit IRQ detection\n"));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: 4-bit IRQ detect without Clock enabled\n"));
    }
    
    if (!builtin_card) {
            /* use slot polling */
        hcdattributes |= SDHCD_ATTRIB_SLOT_POLLING;
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD: Built-in Card forcing ATTACH\n"));    
    }

    max_blocks = min(max_blocks, (UINT32)OMAP_MAX_BLOCKS);
    max_bytes_per_block = min(max_bytes_per_block, (UINT32)OMAP_MAX_BYTES_PER_BLOCK);
     
    if (device_count > OMAP_MAX_DEVICE_COUNT) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: Probe - too many devices requested: %d\n", device_count));
        return -EINVAL;
    }
 
    for (ii = first_device; ii < device_count+first_device; ii++) {
        /* create a device the slot */
        /* allocate a device context for this new device */
        pDeviceContext =  (PSDHCD_DEVICE)KernelAlloc(sizeof(SDHCD_DEVICE));
        if (pDeviceContext == NULL) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: Probe - no memory for device context\n"));
            err = -ENOMEM;
            break;
        }
        ZERO_POBJECT(pDeviceContext);
        SDLIST_INIT(&pDeviceContext->List);
        pDeviceContext->OSInfo.pBusDevice = pBusDevice;
        pDeviceContext->OSInfo.CommonBufferSize = dma_buffer_size;        
        SET_SDIO_STACK_VERSION(&pDeviceContext->Hcd);
        pDeviceContext->Hcd.pName = (PTEXT)KernelAlloc(SDHCD_MAX_DEVICE_NAME+1);
        snprintf(pDeviceContext->Hcd.pName, SDHCD_MAX_DEVICE_NAME, SDIO_BD_BASE"%i:%i",
                 pHcdContext->DeviceCount++, ii);
        pDeviceContext->Hcd.Attributes = hcdattributes;
        pDeviceContext->Hcd.pContext = pDeviceContext;
        pDeviceContext->Hcd.pRequest = HcdRequest;
        pDeviceContext->Hcd.pConfigure = HcdConfig;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        pDeviceContext->Hcd.pDevice = &pBusDevice->dev;
#endif        
        pDeviceContext->Hcd.pModule = THIS_MODULE;
        pDeviceContext->BaseClock = base_clock;
        pDeviceContext->Hcd.MaxSlotCurrent = OMAP_DEFAULT_CURRENT; 
        pDeviceContext->Hcd.SlotVoltageCaps = OMAP_DEFAULT_VOLTAGE; 
        pDeviceContext->Hcd.SlotVoltagePreferred = OMAP_DEFAULT_VOLTAGE; 
        pDeviceContext->Hcd.MaxClockRate = min(max_sdbus_clock,base_clock);
        pDeviceContext->TimeOut = timeout; 
        pDeviceContext->DataTimeOut = data_timeout; 
        
        pDeviceContext->Hcd.pConfigure = HcdConfig;
       
        /* add device to our list of devices */
            /* protect the devicelist */
        if (!SDIO_SUCCESS(status = SemaphorePendInterruptable(&pHcdContext->DeviceListSem))) {
            break;;   /* wait interrupted */
        }
        SDListInsertTail(&pHcdContext->DeviceList, &pDeviceContext->List); 
        SemaphorePost(&pHcdContext->DeviceListSem);
        
        /* initialize work items */
        INIT_WORK(&(pDeviceContext->OSInfo.iocomplete_work), hcd_iocomplete_wqueue_handler, pDeviceContext);
        INIT_WORK(&(pDeviceContext->OSInfo.carddetect_work), hcd_carddetect_wqueue_handler, pDeviceContext);
        INIT_WORK(&(pDeviceContext->OSInfo.sdioirq_work), hcd_sdioirq_wqueue_handler, pDeviceContext);
    
        if (!SDIO_SUCCESS((status = InitOmap(pDeviceContext, ii - first_device)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Probe - failed to init OMAP HW, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
            /* InitOmap may back off these values because of DMA common buffer restrictions */
        pDeviceContext->Hcd.MaxBytesPerBlock = max_bytes_per_block;
        pDeviceContext->Hcd.MaxBlocksPerTrans = max_blocks;
       
        if (!SDIO_SUCCESS((status = HcdInitialize(pDeviceContext)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Probe - failed to init HW, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
               
        pDeviceContext->OSInfo.InitStateMask |= SDHC_HW_INIT;
        
           /* register with the SDIO bus driver */
        if (!SDIO_SUCCESS((status = SDIO_RegisterHostController(&pDeviceContext->Hcd)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP HCD: Probe - failed to register with host, status =%d\n",status));
            err = SDIOErrorToOSError(status);
            break;
        }      
        pDeviceContext->OSInfo.InitStateMask |= SDHC_REGISTERED;
        
        if (builtin_card) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HCD Forcing ATTACH on built-in card \n"));
            SDIO_HandleHcdEvent(&pDeviceContext->Hcd, EVENT_HCD_ATTACH);    
        }
    }    
    if (err < 0) {
        Remove(pBusDevice); /* TODO: the cleanup should not really be done in the Remove function */
    } else {
           /* TODO: check and see if there is a card inserted at powerup */
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Probe - HCD ready! \n"));
        /* queue a work item to check for a card present at start up
           this call will unmask the insert/remove interrupts */
        //QueueEventResponse(pDeviceContext, WORK_ITEM_CARD_DETECT);
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HCD: Probe - err:%d\n", err));
    return err;  
}    

/* Remove - remove  device
 * perform the undo of the Probe
*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static void Remove(struct pnp_dev *pBusDevice)
#else
static void Remove(POS_PNPDEVICE pBusDevice)
#endif
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAP HCD: Remove - removing device\n"));
    RemoveDevice(pBusDevice, pHcdContext);
    pHcdContext->DeviceCount--;
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HCD: Remove\n"));
}

/*
 * RemoveDevice - remove all devices associated with bus device
*/
static void RemoveDevice(POS_PNPDEVICE pBusDevice, PSDHCD_DRIVER_CONTEXT pHcdContext)
{
    PSDHCD_DEVICE pDeviceContext; 
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAP HCD: RemoveDevice\n"));
    
    /* protect the devicelist */
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pHcdContext->DeviceListSem))) {
        return;   /* wait interrupted */
    }
    
    SDITERATE_OVER_LIST_ALLOW_REMOVE(&pHcdContext->DeviceList, pDeviceContext, SDHCD_DEVICE, List)
        if (pDeviceContext->OSInfo.pBusDevice == pBusDevice) {
            if (pDeviceContext->OSInfo.InitStateMask & SDHC_HW_INIT) {
                HcdDeinitialize(pDeviceContext);
            }

            if (pDeviceContext->OSInfo.InitStateMask & SDHC_REGISTERED) {
                SDIO_UnregisterHostController(&pDeviceContext->Hcd);
            }
            
            /* wait for any of our work items to run */
            flush_scheduled_work();
            
            DeinitOmap(pDeviceContext);
            
            if (pDeviceContext->Hcd.pName != NULL) {
                KernelFree(pDeviceContext->Hcd.pName);
                pDeviceContext->Hcd.pName = NULL;
            }
            KernelFree(pDeviceContext);
        }
    SDITERATE_END;
    SemaphorePost(&pHcdContext->DeviceListSem);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HCD: RemoveDevice\n"));
}

/*
 * QueueEventResponse - queues an event in a process context back to the bus driver
 * 
*/
SDIO_STATUS QueueEventResponse(PSDHCD_DEVICE pDeviceContext, INT WorkItemID)
{
    struct work_struct *work;
    
    DBG_PRINT(OMAP_TRACE_WORK, ("+SDIO OMAP QueueEventResponse\n"));
    if (pDeviceContext->OSInfo.ShuttingDown) {
        return SDIO_STATUS_CANCELED;
    }

    switch (WorkItemID) { 
        case WORK_ITEM_IO_COMPLETE:
            DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP QueueEventResponse - WORK_ITEM_IO_COMPLETE \n"));
            work = &pDeviceContext->OSInfo.iocomplete_work;
            break;
        case WORK_ITEM_CARD_DETECT:
            DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP QueueEventResponse - WORK_ITEM_CARD_DETECT \n"));           
            work = &pDeviceContext->OSInfo.carddetect_work;
            break;
        case WORK_ITEM_SDIO_IRQ:
            DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP QueueEventResponse - WORK_ITEM_SDIO_IRQ \n"));           
            work = &pDeviceContext->OSInfo.sdioirq_work;  
            break;
        default:
            DBG_ASSERT(FALSE);
            return SDIO_STATUS_ERROR;
            break;  
    }

    if (schedule_work(work) > 0) {
        DBG_PRINT(OMAP_TRACE_WORK, ("-SDIO OMAP QueueEventResponse - Success \n"));
        return SDIO_STATUS_SUCCESS;
    } else {
        DBG_PRINT(SDDBG_ERROR, ("-SDIO OMAP QueueEventResponse - Error scheduling work\n"));
        return SDIO_STATUS_PENDING;
    }
}
/*
 * CompleteRequestSyncDMA - handle a synchronized request completion between the ISR and the DMA complete
*/
void CompleteRequestSyncDMA(PSDHCD_DEVICE pDeviceContext, PSDREQUEST pRequest, SDIO_STATUS Status)
{
    unsigned long flags;
    DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP HcdCompleteRequest - enter, status: %d, count: %d\n", 
              Status, pDeviceContext->CompletionCount));

    /* disable the DMA and the EOC interrupts */
    local_irq_save(flags);
    pDeviceContext->CompletionCount++;
    DBG_ASSERT_WITH_MSG(pDeviceContext->CompletionCount < 3, "SDIO OMAP: HcdCompleteRequest completion count bad!")
    DBG_ASSERT_WITH_MSG(Status != SDIO_STATUS_PENDING, "SDIO OMAP: HcdCompleteRequest completion still pending status!")
    DBG_ASSERT_WITH_MSG(pRequest != NULL, "SDIO OMAP: HcdCompleteRequest completion NULL pRequest!")
    if (((pDeviceContext->CompletionCount == 2) && (IS_SDREQ_DATA_TRANS(pRequest->Flags)))  ||
        ((pDeviceContext->CompletionCount >= 1) && (!IS_SDREQ_DATA_TRANS(pRequest->Flags))) ||
        !SDIO_SUCCESS(Status)) {
        
        if(!SDIO_SUCCESS(Status)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
            if (pDeviceContext->OSInfo.DmaChannel != -1) {
                DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP HcdCompleteRequest - freeing DMA\n"));
                omap_free_dma(pDeviceContext->OSInfo.DmaChannel);
                pDeviceContext->OSInfo.DmaChannel = -1;
            }
#else
    /* 2.4 */
            if (pDeviceContext->OSInfo.DmaChannel != NULL) {
                DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP HcdCompleteRequest - freeing DMA\n"));
                omap_free_dma(pDeviceContext->OSInfo.DmaChannel);
                pDeviceContext->OSInfo.DmaChannel = NULL;
            }
#endif
        }
        local_irq_restore(flags);
       
        if (pRequest != NULL) {
                /* set the status */
            pRequest->Status = Status;
            DBG_PRINT(OMAP_TRACE_WORK, ("SDIO OMAP HcdCompleteRequest - queueing work from IRQ , status: %d\n", Status));
                /* queue work item to notify bus driver of I/O completion */
            QueueEventResponse(pDeviceContext, WORK_ITEM_IO_COMPLETE);
        } else {
            DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP HcdCompleteRequest - no request to report: status %d \n",
                                           Status));
        }
    } else {
        local_irq_restore(flags);
    } 
}

/*
 * hcd_iocomplete_wqueue_handler - the work queue for io completion
*/
static void hcd_iocomplete_wqueue_handler(void *context) 
{
    PSDHCD_DEVICE pDeviceContext = (PSDHCD_DEVICE)context;
    DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP hcd_iocomplete_wqueue_handler \n"));
    if (!pDeviceContext->OSInfo.ShuttingDown) {
        SDIO_HandleHcdEvent(&pDeviceContext->Hcd, EVENT_HCD_TRANSFER_DONE);
    }
}

/*
 * hcd_carddetect_handler - the work queue for card detect debouncing
*/
static void hcd_carddetect_wqueue_handler(void *context) 
{

#if 0 /* TODO */    
    PSDHCD_DEVICE pDeviceContext = (PSDHCD_DEVICE)context;
    HCD_EVENT event;

    event = EVENT_HCD_ATTACH;
    pDeviceContext->OSInfo.CardInserted = TRUE; 
    SDIO_HandleHcdEvent(&pDeviceContext->Hcd, event);
#endif
}

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
*/
static void hcd_sdioirq_wqueue_handler(void *context) 
{
    PSDHCD_DEVICE pDeviceContext = (PSDHCD_DEVICE)context;
    DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP: hcd_sdioirq_wqueue_handler \n"));
    if (!pDeviceContext->OSInfo.ShuttingDown) {
        SDIO_HandleHcdEvent(&pDeviceContext->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
    }
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  UnmaskIrq - Unmask SD interrupts
  Input:    pDevice - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
UINT16 UnmaskIrq(PSDHCD_DEVICE pDevice, UINT16 Mask, BOOL FromIsr)
{
    UINT16 ints;
    UINT16 ints2;    
    /* protected read-modify-write */
    if (!FromIsr) {
        spin_lock_irq(&pDevice->OSInfo.AddressSpinlock);
    }
    ints = READ_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE);
    ints2 = ints;    
    ints |= Mask;
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE, ints);
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP: UnmaskIrq ints: 0x%x, Mask: 0x%X, ints2: 0x%x, rge: 0x%X\n",
        ints, Mask, ints2, READ_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE)));
    if (!FromIsr) {
        spin_unlock_irq(&pDevice->OSInfo.AddressSpinlock);
    }
    return ints;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MaskIrq - Mask SD interrupts
  Input:    pDevice - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
UINT16 MaskIrq(PSDHCD_DEVICE pDevice, UINT16 Mask, BOOL FromIsr)
{
    UINT16 ints;
    UINT16 ints2;
    /* protected read-modify-write */
    if (!FromIsr) {
        spin_lock_irq(&pDevice->OSInfo.AddressSpinlock);
    }
    ints = READ_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE);
    ints2 = ints;    
    ints &= ~Mask;
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE, ints);
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP: MaskIrq ints: 0x%x, Mask: 0x%X, ints2: 0x%x, rge: 0x%X\n",
        ints, Mask, ints2, READ_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE)));
    if (!FromIsr) {
        spin_unlock_irq(&pDevice->OSInfo.AddressSpinlock);
    }
    return ints;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetDefaults - get the user modifiable data items
  Input:    pDevice - host controller instance
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void GetDefaults(PSDHCD_DEVICE pDevice)
{
    //can't change this dynanmically: pDeviceContext->OSInfo.BaseClock = BaseClock;
    pDevice->TimeOut = timeout;
    pDevice->DataTimeOut = data_timeout;
    pDevice->ClockSpinLimit = clock_spin_limit;
}
    
/*
 * SetPowerLevel - OS dependent set power
*/ 
SDIO_STATUS SetPowerLevel(PSDHCD_DEVICE pDevice, BOOL On, SLOT_VOLTAGE_MASK Level) 
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (On) {
        if (machine_is_omap_h2()) {
            DBG_PRINT(OMAP_TRACE_CONFIG, ("SDIO OMAP: _SetPower tps65010 on\n"));
            tps65010_set_gpio_out_value(GPIO3, HIGH);
        } else if (pDevice->OSInfo.PowerPin >= 0) {
            DBG_PRINT(OMAP_TRACE_CONFIG, ("SDIO OMAP: _SetPower on\n"));
            omap_set_gpio_dataout(pDevice->OSInfo.PowerPin, 1);
        }
    } else {
        if (machine_is_omap_h2()) {
            DBG_PRINT(OMAP_TRACE_CONFIG, ("SDIO OMAP: _SetPower tps65010 off\n"));
            tps65010_set_gpio_out_value(GPIO3, LOW);
        } else if (pDevice->OSInfo.PowerPin >= 0) {
            DBG_PRINT(OMAP_TRACE_CONFIG, ("SDIO OMAP: _SetPower off\n"));
            omap_set_gpio_dataout(pDevice->OSInfo.PowerPin, 0);
        }
    }
#endif  
    return SDIO_STATUS_SUCCESS;      
}

/* platform-specific write protect switch test */
BOOL WriteProtectSwitchOn(PSDHCD_DEVICE pDevice)
{
        /* TODO if write protect is implemented and is set, return TRUE */
    return FALSE;   
}

/* micro second delay */
void MicroDelay(PSDHCD_DEVICE pDevice, INT Microseconds)
{
    udelay(Microseconds);
}


/*
 * module init
*/
static int __init sdio_local_hcd_init(void) {
    SDIO_STATUS status;
    
    REL_PRINT(SDDBG_TRACE, ("+SDIO OMAP HCD: loaded\n"));

    SDLIST_INIT(&HcdContext.DeviceList);
    status = SemaphoreInitialize(&HcdContext.DeviceListSem, 1);
    if (!SDIO_SUCCESS(status)) {
       return SDIOErrorToOSError(status);
    }       
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    status = SDIO_BusAddOSDevice(&HcdContext.Driver.Dma, &HcdContext.Driver.HcdDriver, &HcdContext.Driver.HcdDevice);
    return SDIOErrorToOSError(status);
#else
    /* 2.4 */
    return Probe(NULL, NULL);
#endif
}

/*
 * module cleanup
*/
static void __exit sdio_local_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SDIO OMAP HCD: unloaded\n"));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    SDIO_BusRemoveOSDevice(&HcdContext.Driver.HcdDriver, &HcdContext.Driver.HcdDevice);
#else 
    /* 2.4 */
    Remove(NULL);
#endif
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HCD: leave sdio_local_hcd_cleanup\n"));
}

// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_local_hcd_init);
module_exit(sdio_local_hcd_cleanup);

