// Copyright (c) 2004 Atheros Communications Inc.
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
@file: spi_hcd_os.c

@abstract: Linux PXA255 Local Bus SDIO (SPI) Host Controller Driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/

#define DBG_DECLARE 7;
#include "../../../include/ctsystem.h"

#include "../spi_pxa255hcd.h"
#include <linux/fs.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#define DESCRIPTION "SPI PXA255 Local Bus HCD"
#define AUTHOR "Atheros Communications, Inc."

static int Probe(struct device *pBusDevice);
static int Remove(struct device *pBusDevice);
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription);
static void UnmapAddress(PSDHCD_MEMORY pMap);
static void hcd_release_device(struct device *pDevice);

static irqreturn_t hcd_spi_irq(int irq, void *context, struct pt_regs * r);
static irqreturn_t hcd_sdio_irq(int irq, void *context, struct pt_regs * r);
static irqreturn_t hcd_card_detect_irq(int irq, void *context, struct pt_regs * r);

static void hcd_iocomplete_wqueue_handler(void *context);
static void hcd_carddetect_wqueue_handler(void *context);
static void hcd_sdioirq_wqueue_handler(void *context);

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");
#define DEFAULT_ATTRIBUTES (SDHCD_ATTRIB_BUS_SPI)
UINT32 hcdattributes = DEFAULT_ATTRIBUTES;
module_param(hcdattributes, int, 0644);
MODULE_PARM_DESC(hcdattributes, "PXA255 SPI Attributes");



/* the driver context data */
static SDHCD_DRIVER_CONTEXT HcdContext = {
   .pDescription  = DESCRIPTION,
   .Hcd.Version = CT_SDIO_STACK_VERSION_CODE,
   .Hcd.pName = "spi_pxa255hcd",
   .Hcd.SlotNumber = 0,
   .Hcd.Attributes = DEFAULT_ATTRIBUTES,
   .Hcd.MaxBytesPerBlock = SPI_PXA_MAX_BYTES_PER_BLOCK,
   .Hcd.MaxBlocksPerTrans = SPI_PXA_MAX_BLOCKS,
   .Hcd.MaxSlotCurrent = 500, /* 1/2 amp */
   .Hcd.SlotVoltageCaps = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.SlotVoltagePreferred = SLOT_POWER_3_3V, /* 3.3V */
   .Hcd.MaxClockRate = 1840000, /* 1.84  Mhz */
   .Hcd.pContext = &HcdContext,
   .Hcd.pRequest = HcdRequest,
   .Hcd.pConfigure = HcdConfig,
   .Device.HcdPlatformDevice.name   = "spi_pxa255hcd",
   .Device.HcdPlatformDevice.id     = 0,
   .Device.HcdPlatformDevice.dev.release = hcd_release_device,
   .Device.HcdPlatformDriver.name   = "spi_pxa255hcd",
   .Device.HcdPlatformDriver.bus    = &platform_bus_type,
   .Device.HcdPlatformDriver.probe  = Probe,
   .Device.HcdPlatformDriver.remove = Remove, 
};

    
/* work queues */
static DECLARE_WORK(iocomplete_work, hcd_iocomplete_wqueue_handler, &HcdContext);
static DECLARE_WORK(carddetect_work, hcd_carddetect_wqueue_handler, &HcdContext);
static DECLARE_WORK(sdioirq_work, hcd_sdioirq_wqueue_handler, &HcdContext);

/*
 * Probe - probe to setup our device, if present
*/
static int Probe(struct device *pBusDevice)
{
    SYSTEM_STATUS err = 0;
    SDIO_STATUS   status;
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;

    DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Local HCD: Probe - probing for new device\n"));    
 
    pHcdContext->Device.pBusDevice = pBusDevice;
    pHcdContext->Hcd.pDevice = pBusDevice;
    pHcdContext->Device.SPIInterrupt = SDIO_PXA_CONTROLLER_IRQ;
    pHcdContext->Device.CardInsertInterrupt = SDIO_PXA_CARD_INSERT_IRQ;  
    pHcdContext->Device.SDIOInterrupt = SDIO_PXA_SDIO_IRQ;        
    pHcdContext->Device.ControlRegs.Raw = PXA_SPI_CONTROLLER_BASE_ADDRESS;
    pHcdContext->Device.ControlRegs.Length = PXA_SPI_CONTROLLER_ADDRESS_LENGTH;  
    pHcdContext->Device.GpioRegs.Raw = PXA_GPIO_PIN_LVL_REGS_BASE;
    pHcdContext->Device.GpioRegs.Length = PXA_GPIO_PIN_LVL_REGS_LENGTH;
    spin_lock_init(&pHcdContext->Device.Lock);
    pHcdContext->Hcd.Attributes = hcdattributes; 
    
    do {
          
            /* map the devices memory addresses */
        err = MapAddress(&pHcdContext->Device.ControlRegs, "spi_pxahcd");
        if (err < 0) {
            /* couldn't map the address */
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: Probe - unable to map memory\n"));
            break; 
        }
        
        err = MapAddress(&pHcdContext->Device.GpioRegs, "spi_pxahcd");
        if (err < 0) {
            /* couldn't map the address */
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: Probe - unable to map memory\n"));
            break;
        }
        
        DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Switching GPIO lines \n"));
            /* switch GPIOs to SSP port */    
        pxa_gpio_mode(GPIO23_SCLK_md);
        pxa_gpio_mode(GPIO24_SFRM_MD);   
        pxa_gpio_mode(GPIO25_STXD_MD); 
        pxa_gpio_mode(GPIO26_SRXD_MD);  
            /* set SPI CS to output */
        pxa_gpio_mode(SDIO_CARD_SPI_CS | GPIO_OUT);
            /* route SSP clock */
        pxa_set_cken(CKEN3_SSP , 1);
        
        DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 - SPI IRQ:%d \n",
            pHcdContext->Device.SPIInterrupt));
            
            /* map the controller interrupt */
        err = request_irq (pHcdContext->Device.SPIInterrupt, hcd_spi_irq, 0,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - unable to map SPI interrupt \n"));
            err = -ENODEV;
            break;
        }
        
        pHcdContext->Device.InitStateMask |= SPI_INTERRUPT_INIT;
        
        DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 - card insert IRQ:%d \n",
            pHcdContext->Device.CardInsertInterrupt));

        err = request_irq (pHcdContext->Device.CardInsertInterrupt, hcd_card_detect_irq, SA_SHIRQ,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - unable to map CardInsert interrupt \n"));
            err = -ENODEV;
            break; 
        } 
        
        err = set_irq_type(pHcdContext->Device.CardInsertInterrupt, IRQT_BOTHEDGE);
         
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - failed to set card insert IRQ edge \n"));
            err = -ENODEV;
            break;
        } 
        
        pHcdContext->Device.InitStateMask |= CARD_DETECT_INTERRUPT_INIT;
        
        DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 - SDIO IRQ:%d \n",pHcdContext->Device.SDIOInterrupt));
           /* map the SDIO IRQ interrupt */
        err = request_irq (pHcdContext->Device.SDIOInterrupt, hcd_sdio_irq, SA_SHIRQ,
                           pHcdContext->pDescription, pHcdContext);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - unable to map SDIO-IRQ interrupt\n"));
            err = -ENODEV;
            break;
        } 
        pHcdContext->Device.InitStateMask |= SDIO_IRQ_INTERRUPT_INIT;       
        
        if (!SDIO_SUCCESS((status = HcdInitialize(pHcdContext)))) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Probe - failed to init HW, status =%d\n",status));
            err = SDIOErrorToOSError(status);
            break;
        } 
               
        pHcdContext->Device.InitStateMask |= SDHC_HW_INIT;
        
         /* register with the SDIO bus driver */
     if (!SDIO_SUCCESS((status = SDIO_RegisterHostController(&pHcdContext->Hcd)))) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Probe - failed to register with host, status =%d\n",
                status));
            err = SDIOErrorToOSError(status);
            break;
      } 
             
        pHcdContext->Device.InitStateMask |= SDHC_REGISTERED;
       
        DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Probe - GRER0 0x%X \n",
                READ_GPIO_REG(pHcdContext, GPIO_GRER0)));
        DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Probe - GFER0 0x%X \n",
                READ_GPIO_REG(pHcdContext, GPIO_GFER0)));
    } while (FALSE);
    
    if (err < 0) {
        Remove(pBusDevice); 
    } else {
           /* check and see if there is a card inserted at powerup */
        if (GetGpioPinLevel(pHcdContext,SDIO_CARD_INSERT_GPIO) == CARD_INSERT_POLARITY) {
            DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Probe - Card Detected \n"));  
                /* queue the work item to notify the bus driver */
            if (!SDIO_SUCCESS(QueueEventResponse(pHcdContext, WORK_ITEM_SDIO_IRQ))) {
                    /* failed */
                DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Probe - queue event failed\n"));
            }
        }
               
        DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Probe - HCD ready! \n"));       
    }
    return err;  
}

/* Remove - remove  device
 * perform the undo of the Probe
*/
static int Remove(struct device *pBusDevice) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = &HcdContext;
    
    DBG_PRINT(SDDBG_TRACE, ("+SPI PXA255 Local HCD: Remove - removing device\n"));    
    
    if (pHcdContext->Device.InitStateMask & SPI_INTERRUPT_INIT) {
        /* TODO mask all controller interrupts */
    }
 
    if (pHcdContext->Device.InitStateMask & SDHC_REGISTERED) {
            /* unregister from the bus driver */
        SDIO_UnregisterHostController(&pHcdContext->Hcd);
    }
    
        /* free irqs */
    if (pHcdContext->Device.InitStateMask & SPI_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.SPIInterrupt, pHcdContext);
    }
    if (pHcdContext->Device.InitStateMask & CARD_DETECT_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.CardInsertInterrupt, pHcdContext);
    }
    if (pHcdContext->Device.InitStateMask & SDIO_IRQ_INTERRUPT_INIT) {
        free_irq(pHcdContext->Device.SDIOInterrupt, pHcdContext);
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
    
    DBG_PRINT(SDDBG_TRACE, ("-SPI PXA255 Local HCD: Remove\n"));  
    return 0;
}
 
/*
 * MapAddress - maps I/O address
*/
static SYSTEM_STATUS MapAddress(PSDHCD_MEMORY pMap, PTEXT pDescription) {
     
    if (request_mem_region(pMap->Raw, pMap->Length, pDescription) == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 Local HCD: MapAddress - memory in use\n"));
        return -EBUSY;
    }
    pMap->pMapped = ioremap_nocache(pMap->Raw, pMap->Length);
    if (pMap->pMapped == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 Local HCD: MapAddress - unable to map memory\n"));
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

/* MMC controller interrupt routine */
static irqreturn_t hcd_spi_irq(int irq, void *context, struct pt_regs * r)
{
        /* call OS independent ISR */
    if (HcdSPIInterrupt((PSDHCD_DRIVER_CONTEXT)context)) {
        return IRQ_HANDLED;
    } else {
        return IRQ_NONE;
    }    
}

/* card detect interrupt request */
static irqreturn_t hcd_card_detect_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("SPI PXA255 Card Detect Interrupt, queueing work item \n"));
        /* just queue the work item to debounce the pin */
    QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_CARD_DETECT);
    return IRQ_HANDLED;
}

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
        case WORK_ITEM_CARD_DETECT:
            work = &carddetect_work;
            break;
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

/*
 * hcd_carddetect_handler - the work queue for card detect debouncing
*/
static void hcd_carddetect_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    HCD_EVENT event;
    
    event = EVENT_HCD_NOP;
    
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("+SPI PXA255 Card Detect Work Item \n"));
    
    if (!pHcdContext->CardInserted) { 
        DBG_PRINT(PXA_TRACE_CARD_INSERT, ("Delaying to debounce card... \n"));
            /* sleep for slot debounce if there is no card */
        OSSleep(PXA_SLOT_DEBOUNCE_MS);
    }
                
    if (GetGpioPinLevel(pHcdContext,SDIO_CARD_INSERT_GPIO) == CARD_INSERT_POLARITY) {
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
        } else {
            DBG_PRINT(SDDBG_ERROR, ("Card detect interrupt , already removed card! \n"));
        }
    }
    
    if (event != EVENT_HCD_NOP) {
        SDIO_HandleHcdEvent(&pHcdContext->Hcd, event);
    }
    
    DBG_PRINT(PXA_TRACE_CARD_INSERT, ("- SPI PXA255 Card Detect Work Item \n"));
}

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
*/
static void hcd_sdioirq_wqueue_handler(void *context) 
{
    PSDHCD_DRIVER_CONTEXT pHcdContext = (PSDHCD_DRIVER_CONTEXT)context;
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255: hcd_sdioirq_wqueue_handler \n"));
    SDIO_HandleHcdEvent(&pHcdContext->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
}


/* SDIO interrupt request */
static irqreturn_t hcd_sdio_irq(int irq, void *context, struct pt_regs * r)
{
    DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255 SDIO IRQ \n"));
    
        /* check gpio pin for assertion */
    if (GetGpioPinLevel((PSDHCD_DRIVER_CONTEXT)context,SDIO_IRQ_GPIO) == SDIO_IRQ_POLARITY) {
            /* disable IRQ */
        EnableDisableSDIOIrq((PSDHCD_DRIVER_CONTEXT)context, FALSE);
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255 SDIO IRQ Asserted.. Queueing Work Item \n"));
            /* queue the work item to notify the bus driver*/
        if (!SDIO_SUCCESS(QueueEventResponse((PSDHCD_DRIVER_CONTEXT)context, WORK_ITEM_SDIO_IRQ))) {
                /* failed */
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: HcdInterrupt - queue event failed\n"));
        }
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255 SDIO IRQ Asserted.. Queued Work Item \n"));
        return IRQ_HANDLED;
    }
    
    return IRQ_NONE;        
}


/*
 * AckSDIOIrq - acknowledge SDIO interrupt
*/
SDIO_STATUS AckSDIOIrq(PSDHCD_DRIVER_CONTEXT pHcdContext)
{
    ULONG flags;
   
        /* block the SDIO-IRQ handler from running */ 
    spin_lock_irqsave(&pHcdContext->Device.Lock,flags);   
        /* re-enable edge detection */
    EnableDisableSDIOIrq(pHcdContext, TRUE);
        /* delay enough so that we can sample a level interrupt if it's already
         * asserted */
    udelay(2);
    
    if (GetGpioPinLevel(pHcdContext,SDIO_IRQ_GPIO) == SDIO_IRQ_POLARITY) {
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255: pending int during Ack, issuing event \n"));
            /* disable SDIO irq detection */   
        EnableDisableSDIOIrq(pHcdContext, FALSE);
        spin_unlock_irqrestore(&pHcdContext->Device.Lock,flags); 
             /* queue work item to process another interrupt */
        return QueueEventResponse(pHcdContext, WORK_ITEM_SDIO_IRQ);
    }
        /* let the normal GPIO edge detect take over */
    spin_unlock_irqrestore(&pHcdContext->Device.Lock,flags);     
    return SDIO_STATUS_SUCCESS;
}

void ModifyCSForSPIIntDetection(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable)
{
    // todo?
}

/*
 * EnableDisableSDIOIrq - enable or disable the interrupt
*/
SDIO_STATUS EnableDisableSDIOIrq(PSDHCD_DRIVER_CONTEXT pHcdContext, BOOL Enable)
{
    if (Enable) {    
        if (set_irq_type(pHcdContext->Device.SDIOInterrupt, IRQT_FALLING) < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - failed to enable SDIO IRQ edge detect \n"));
            return SDIO_STATUS_DEVICE_ERROR;
        }          
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255 - SDIO IRQ Detection Enabled!\n"));       
    } else {
            /* to mask the GPIO PXA interrupt we clear the falling edge bit */
        if (set_irq_type(pHcdContext->Device.SDIOInterrupt, 0) < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - failed clear SDIO IRQ edge detect \n"));
            return SDIO_STATUS_DEVICE_ERROR;
        } 
        DBG_PRINT(PXA_TRACE_SDIO_INT, ("SPI PXA255 - SDIO IRQ Detection Disabled!\n"));   
      
    }
    
    return SDIO_STATUS_SUCCESS;  
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
static int __init spi_local_hcd_init(void) {
    SYSTEM_STATUS err;
    
    REL_PRINT(SDDBG_TRACE, ("SPI PXA255 Local HCD: loaded\n"));
    /* register with the local bus driver */
    err = driver_register(&HcdContext.Device.HcdPlatformDriver);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: failed to register with system bus driver, %d\n", err));
    }
    /* tell bus driver we have a device */
    err = platform_device_register(&HcdContext.Device.HcdPlatformDevice);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: failed to register device with system bus driver, %d\n", err));
    }
    DBG_PRINT(SDDBG_TRACE, ("SPI PXA255 Local HCD: sdio_local_hcd_init exit\n"));
    return err;
}

/*
 * module cleanup
*/
static void __exit spi_local_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SPI PXA255 Local HCD: unloaded\n"));
    platform_device_unregister(&HcdContext.Device.HcdPlatformDevice);
    driver_unregister(&HcdContext.Device.HcdPlatformDriver);
    DBG_PRINT(SDDBG_TRACE, ("-SPI PXA255 Local HCD: leave sdio_local_hcd_cleanup\n"));
}

MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(spi_local_hcd_init);
module_exit(spi_local_hcd_cleanup);

