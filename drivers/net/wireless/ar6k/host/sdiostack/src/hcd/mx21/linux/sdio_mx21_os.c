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
@file: sdio_hcd_os.c

@abstract: Linux MX21 Local Bus SDIO Host Controller Driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#define DBG_DECLARE 7;
#include <ctsystem.h>
#include "../sdio_mx21.h"
#include <linux/fs.h>
#include <linux/ioport.h> 
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define DESCRIPTION "SDIO MX21 Local Bus HCD"
#define AUTHOR "Atheros Communications, Inc."

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int Probe(struct pnp_dev *pBusDevice, const struct pnp_device_id *pId);
static void Remove(struct pnp_dev *pBusDevice);
#else
static int Probe(POS_PNPDEVICE pBusDevice, const PUINT pId);
static void Remove(POS_PNPDEVICE pBusDevice);
#endif

static void hcd_iocomplete_wqueue_handler(void *context);
static void hcd_sdioirq_wqueue_handler(void *context);

#define BASE_HCD_ATTRIBUTES (SDHCD_ATTRIB_BUS_1BIT      |      \
                             SDHCD_ATTRIB_SLOT_POLLING)
               
#define DEFAULT_ATTRIBUTES (BASE_HCD_ATTRIBUTES | SDHCD_ATTRIB_BUS_4BIT)

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

static SDHCD_DRIVER_CONTEXT HcdContext = {
   .pDescription  = DESCRIPTION,
   .Hcd.pName = "sdio_mx21hcd",
   .Hcd.Version = CT_SDIO_STACK_VERSION_CODE,
   .Hcd.SlotNumber = 0,
   .Hcd.Attributes = DEFAULT_ATTRIBUTES,
   .Hcd.MaxBytesPerBlock = SDIO_SDHC_MAX_BYTES_PER_BLOCK,
   .Hcd.MaxBlocksPerTrans = SDIO_SDHC_MAX_BLOCKS,
   .Hcd.MaxSlotCurrent = 500, /* 1/2 amp */
   .Hcd.SlotVoltageCaps = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.SlotVoltagePreferred = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.MaxClockRate = 20000000, 
   .Hcd.pContext = &HcdContext,
   .Hcd.pRequest = HcdRequest,
   .Hcd.pConfigure = HcdConfig,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
   .Device.HcdDevice.name = "sdio_mx21_hcd",
   .Device.HcdDriver.name = "sdio_mx21_hcd",
   .Device.HcdDriver.probe  = Probe,
   .Device.HcdDriver.remove = Remove,    
#endif
   .Device.Dma.Mask = 0xFFFFFFFF,    /* any address */
   .Device.Dma.Flags = SDDMA_DESCRIPTION_FLAG_DMA,
   .Device.Dma.MaxBytesPerDescriptor = SDHC_MAX_BYTES_PER_DMA_DESCRIPTOR, /* max per descriptor */ 
   .Device.Dma.AddressAlignment = 0x0,  /* no illegal bits, buffers address can be on any boundary*/
   .Device.Dma.LengthAlignment = 0x0,   /* no illegal bits, buffer lengths can be any byte count */
   .Device.Dma.MaxDescriptors = 1,      /* only 1 scatter gather descriptor */
};
    
/* work queues */
static struct work_struct iocomplete_work;
static struct work_struct sdioirq_work;

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
    SDIO_STATUS   status;
    PSDHCD_DRIVER_CONTEXT pHct = &HcdContext; /* for now , only 1 instance */

    DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 Local HCD: Probe  \n"));
     
    do {        
        
        pHct->Device.pBusDevice = pBusDevice;
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) 
        pHct->Hcd.pDevice = &pBusDevice->dev;
#endif

        pHct->Hcd.pModule = THIS_MODULE;
        pHct->Device.ControlRegs.Raw = SDHC_CONTROLLER1_BASE_ADDRESS;
        pHct->Device.ControlRegs.Length = SDHC_CONTROLLER_ADDRESS_LENGTH;
          
        spin_lock_init(&pHct->Device.Lock);
    
        status = InitMX21(pHct); 
        
        if (!SDIO_SUCCESS(status)) {
            err = SDIOErrorToOSError(status);
            break; 
        }
        
        status = HcdInitialize(pHct);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Probe - failed to init HW, status =%d\n", status));
            err = SDIOErrorToOSError(status);
            break;
        } 
               
        pHct->Device.InitStateMask |= SDHC_HW_INIT;
        
    	   /* register with the SDIO bus driver */
    	if (!SDIO_SUCCESS((status = SDIO_RegisterHostController(&pHct->Hcd)))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Probe - failed to register with host, status =%d\n",
                                    status));
            err = SDIOErrorToOSError(status);
            break;
    	} 
             
        pHct->Device.InitStateMask |= SDHC_REGISTERED;
       
    } while (FALSE);
    
    if (err < 0) {
        Remove(pBusDevice); /* TODO: the cleanup should not really be done in the Remove function */
    } else {
#ifdef USE_CARD_DETECT_HW
        pHct->Device.StartUpCheck = TRUE;
            /* queue the work item test the slot */
        if (!SDIO_SUCCESS(QueueEventResponse(pHct, WORK_ITEM_CARD_DETECT))) {
                /* failed */
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Probe - queue event failed\n"));
        }
#endif
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Probe - HCD ready! \n"));
    }
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
    PSDHCD_DRIVER_CONTEXT pHct = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MX21 Local HCD: Remove - removing device\n"));
     
    if (pHct->Device.InitStateMask & SDHC_REGISTERED) {
            /* unregister from the bus driver */
        SDIO_UnregisterHostController(&pHct->Hcd);
        pHct->Device.InitStateMask &= ~SDHC_REGISTERED;
    }
    
    if (pHct->Device.InitStateMask & SDHC_HW_INIT) {
        HcdDeinitialize(pHct);
        pHct->Device.InitStateMask &= ~SDHC_HW_INIT;
    }
    
    DeinitMX21(pHct);
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MX21 Local HCD: Remove\n"));
}

/*
 * QueueEventResponse - queues an event in a process context back to the bus driver
 * 
*/
SDIO_STATUS QueueEventResponse(PSDHCD_DRIVER_CONTEXT pHct, INT WorkItemID)
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
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)context;
    
    SDIO_HandleHcdEvent(&pHct->Hcd, EVENT_HCD_TRANSFER_DONE);
}

#ifdef USE_CARD_DETECT_HW
/*
 * hcd_carddetect_handler - the work queue for card detect debouncing
*/
static void hcd_carddetect_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)context;
    HCD_EVENT event;
    
    event = EVENT_HCD_NOP;
    //SDHC_TRACE_CARD_INSERT
    
    DBG_PRINT(SDDBG_TRACE, ("+ SDIO MX21 Card Detect Work Item \n"));
    
    if (!pHct->CardInserted) { 
        DBG_PRINT(SDDBG_TRACE, ("Delaying to debounce card... \n"));
            /* sleep for slot debounce if there is no card */
        msleep(SDHC_SLOT_DEBOUNCE_MS);
    }
                
        /* check board status pin */
    if (IsCardInserted(pHct)) {
        if (!pHct->CardInserted) {  
            pHct->CardInserted = TRUE;
            event = EVENT_HCD_ATTACH;
            DBG_PRINT(SDDBG_TRACE, (" Card Inserted! \n"));
                /* disable insert */
            disable_irq(pHct->Device.CanomralrdInsertInterrupt); 
                /* enable remove */
            enable_irq(pHct->Device.CardRemoveInterrupt);
        } else {
            DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already inserted card! \n"));
        }
    } else { 
        if (pHct->CardInserted) {  
            event = EVENT_HCD_DETACH;
            pHct->CardInserted = FALSE; 
            DBG_PRINT(SDDBG_TRACE, (" Card Removed! \n"));
                /* disable remove */
            disable_irq(pHct->Device.CardRemoveInterrupt);
                /* enable insert */
            enable_irq(pHct->Device.CardInsertInterrupt);
        } else {
            if (pHct->Device.StartUpCheck) {
                pHct->Device.StartUpCheck = FALSE;    
                DBG_PRINT(SDDBG_TRACE, ("No card at power up. \n"));
            } else {
                DBG_PRINT(SDDBG_ERROR, nomral("Card detect interrupt , already removed card! \n"));
            }
        }
    }
    
    if (event != EVENT_HCD_NOP) {
        SDIO_HandleHcdEvent(&pHct->Hcd, event);
    }
    
    DBG_PRINT(SDDBG_TRACE, ("- SDIO MX21 Card Detect Work Item \n"));
}


BOOL IsCardInserted(PSDHCD_DRIVER_CONTEXT pHct)
{
        // TODO
    return TRUE; 
}

#endif

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
*/
static void hcd_sdioirq_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)context;
    DBG_PRINT(SDHC_TRACE_SDIO_INT, ("SDIO MX21: hcd_sdioirq_wqueue_handler \n"));
    SDIO_HandleHcdEvent(&pHct->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  UnmaskHcdIrq - Un mask a HCD interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes:  
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void UnmaskHcdIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr)
{
    ULONG  flags = 0;
    UINT32 ints;
   
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    if (Mask & SDHC_INT_SDIO_MASK) {
        pHct->SDIOIrqMasked = FALSE;
    }
    
    ints = READ_HC_REG(pHct, SDHC_INT_MASK_REG);
    ints &= ~Mask;
    WRITE_HC_REG(pHct, SDHC_INT_MASK_REG, ints);
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MaskHcdIrq - Mask Hcd interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void MaskHcdIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask, BOOL FromIsr)
{
    UINT32 ints;
    ULONG  flags = 0;
    
    if (!FromIsr) {
        spin_lock_irqsave(&pHct->Device.Lock,flags);   
    }
    if (Mask & SDHC_INT_SDIO_MASK) {
        pHct->SDIOIrqMasked = TRUE;
    }
    ints = READ_HC_REG(pHct, SDHC_INT_MASK_REG);
    ints |= Mask;
    WRITE_HC_REG(pHct, SDHC_INT_MASK_REG, ints); 
    if (!FromIsr) {
        spin_unlock_irqrestore(&pHct->Device.Lock,flags);  
    } 
}


/*
 * module init
*/
static int __init sdio_local_hcd_init(void) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    SDIO_STATUS status;
#endif
    INIT_WORK(&iocomplete_work, hcd_iocomplete_wqueue_handler, &HcdContext);
    INIT_WORK(&sdioirq_work, hcd_sdioirq_wqueue_handler, &HcdContext);

    REL_PRINT(SDDBG_TRACE, ("SDIO MX21 Local HCD: loaded\n"));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    status = SDIO_BusAddOSDevice(&HcdContext.Driver.Dma, &HcdContext.Driver.HcdDriver, &HcdContext.Driver.HcdDevice);
    return SDIOErrorToOSError(status);
#else
    DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 Local HCD: sdio_local_hcd_init exit\n"));
    /* 2.4 */
    return Probe(NULL, NULL);
#endif
    
}

/*
 * module cleanup
*/
static void __exit sdio_local_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SDIO MX21 Local HCD: unloaded\n"));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    SDIO_BusRemoveOSDevice(&HcdContext.Driver.HcdDriver, &HcdContext.Driver.HcdDevice);
#else 
    /* 2.4 */
    Remove(NULL); 
#endif
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MX21 Local HCD: leave sdio_local_hcd_cleanup\n"));
}

// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_local_hcd_init);
module_exit(sdio_local_hcd_cleanup);

