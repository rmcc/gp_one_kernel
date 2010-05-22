/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sdio_hcd_os.c

@abstract: Standard PCI SDIO Host Controller Driver

#notes: includes module load and unload functions
 
@notice: Copyright (c), 2006 Atheros Communications, Inc.


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
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
 
#include "../../../include/ctsystem.h"
#include "../../../include/sdio_busdriver.h"
#include "../../stdhost/linux/sdio_std_hcd_linux.h"
#include "../../stdhost/sdio_std_hcd.h"
#include "../../stdhost/linux/sdio_std_hcd_linux_lib.h"
#include <linux/pci.h>

#define DESCRIPTION "SDIO Standard PCI HCD"
#define AUTHOR "Atheros Communications, Inc."

static INT ForceSDMA = 0;
module_param(ForceSDMA, int, 0444);
MODULE_PARM_DESC(ForceSDMA, "Force Host controller to use simple DMA if available");

#define PCI_CLASS_SYSTEM_SDIO    0x0805

/* the config space slot number and start for SD host */
#define PCI_CONFIG_SLOT   0x40
#define GET_SLOT_COUNT(config)\
    ((((config)>>4)& 0x7) +1)
#define GET_SLOT_FIRST(config)\
    ((config) & 0x7)
#define PCI_CONFIG_CLASS  0x09
#define PCI_SD_DMA_SUPPORTED 0x01  

#define SDIO_PCI_BAR_MAPPED            0x01

typedef enum _SDHCD_TYPE {
    TYPE_CLASS,     /* standard class device */
    TYPE_PCIELLEN,  /* Tokyo Electron PCI Ellen card */
}SDHCD_TYPE, *PSDHCD_TYPE;

    /* PCI devices supported */
static const struct pci_device_id pci_ids [] = {  
     /* Ellen I */ 
  {
    .vendor = 0x1679, .device = 0x3000,
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 
  },
  {  /* Ellen II Standard Host */
    .vendor = 0x10ee, .device = 0x0300,
    .subvendor = 0x10b5, .subdevice = 0x9030,
  },  
  {  /* ENE Standard Host */
    .vendor = 0x1524, .device = 0x0750,
    .subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID,
  },
  {  /* Standard Host */
   PCI_DEVICE_CLASS(PCI_CLASS_SYSTEM_SDIO << 8, 0xFFFFFF00),
  },
 { /* end: all zeroes */ }
};

MODULE_DEVICE_TABLE (pci, pci_ids);

#define SDHCD_PCI_IRQ_HOOKED 0x01


static SYSTEM_STATUS Probe(struct pci_dev *pPCIdevice, const struct pci_device_id *pId);
static void Remove(struct pci_dev *pPCIdevice);

static irqreturn_t hcd_sdio_irq(int irq, void *context
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
, struct pt_regs * r);
#else
);
#ifndef SA_SHIRQ
#define SA_SHIRQ           IRQF_SHARED
#endif
#ifndef pci_module_init 
#define pci_module_init pci_register_driver
#endif

#endif

static SDIO_STATUS InitEllen(struct pci_dev *pPCIdevice);

/* tell PCI bus driver about us */
static struct pci_driver sdio_pci_driver = {
    .name =     "sdio_pcistd_hcd",
    .id_table = pci_ids,

    .probe =    Probe,
    .remove =   Remove,

#ifdef CONFIG_PM
    .suspend =  NULL,     
    .resume =  NULL,      
#endif
};


    /* Advanced DMA description */
SDDMA_DESCRIPTION HcdADMADefaults = {
    .Flags = SDDMA_DESCRIPTION_FLAG_SGDMA,
    .MaxDescriptors = SDHCD_MAX_ADMA_DESCRIPTOR,
    .MaxBytesPerDescriptor = SDHCD_MAX_ADMA_LENGTH,
    .Mask = SDHCD_ADMA_ADDRESS_MASK, 
    .AddressAlignment = SDHCD_ADMA_ALIGNMENT,
    .LengthAlignment = SDHCD_ADMA_LENGTH_ALIGNMENT,
};

    /* simple DMA descriptions */
SDDMA_DESCRIPTION HcdSDMADefaults = {
    .Flags = SDDMA_DESCRIPTION_FLAG_DMA,
    .MaxDescriptors = 1,
    .MaxBytesPerDescriptor = SDHCD_MAX_SDMA_LENGTH,
    .Mask = SDHCD_SDMA_ADDRESS_MASK, 
    .AddressAlignment = SDHCD_SDMA_ALIGNMENT,
    .LengthAlignment = SDHCD_SDMA_LENGTH_ALIGNMENT,
};

/*
 * MapAddress - sets up the address for a given BAR
*/
static int MapAddress(struct pci_dev *pPCIdevice, char *pName, UINT8 bar, PSDHCD_MEMORY pAddress)
{
    if (pci_resource_flags(pPCIdevice, bar) & PCI_BASE_ADDRESS_SPACE  ) {
        DBG_PRINT(SDDBG_WARN, ("SDIO STDPCI HCD: MapAddress, port I/O not supported\n"));
        return -ENOMEM;
    } 
    pAddress->Raw = pci_resource_start(pPCIdevice, bar);
    pAddress->Length = pci_resource_len(pPCIdevice, bar);
    if (!request_mem_region (pAddress->Raw, pAddress->Length, pName)) {
        DBG_PRINT(SDDBG_WARN, ("SDIO STDPCI HCD: MapAddress - memory in use: 0x%X(0x%X)\n",
                               (UINT)pAddress->Raw, (UINT)pAddress->Length));
        return -EBUSY;
    }
    pAddress->pMapped = ioremap_nocache(pAddress->Raw, pAddress->Length);
    if (pAddress->pMapped == NULL) {
        DBG_PRINT(SDDBG_WARN, ("SDIO STDPCI HCD: MapAddress - unable to map memory\n"));
        /* cleanup region */
        release_mem_region (pAddress->Raw, pAddress->Length);
        return -EFAULT;
    }
    DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: MapAddress - mapped memory: 0x%X(0x%X) to 0x%X\n",
                            (UINT)pAddress->Raw, (UINT)pAddress->Length, (UINT)pAddress->pMapped));
    return 0;
}


/*
 * UnmapAddress - unmaps the address 
*/
static void UnmapAddress(PSDHCD_MEMORY pAddress) {
    iounmap(pAddress->pMapped);
    release_mem_region(pAddress->Raw, pAddress->Length);
    pAddress->pMapped = NULL;
}
   
/*
 * CleanupPCIResources - cleanup PCI resources
*/
static void CleanupPCIResources(struct pci_dev *pPCIdevice, 
                                PSDHCD_INSTANCE pHcInstance)
{    
    if (pHcInstance->OsSpecific.InitMask & SDIO_PCI_BAR_MAPPED) {
        UnmapAddress(&pHcInstance->OsSpecific.Address);
        pHcInstance->OsSpecific.InitMask &= ~SDIO_PCI_BAR_MAPPED;
    }
}

SDIO_STATUS SetUpOneSlotController(PSDHCD_CORE_CONTEXT pStdCore,
                                   struct pci_dev *pPCIdevice,
                                   const struct pci_device_id *pId,
                                   UINT       SlotNumber,
                                   int        BAR,
                                   BOOL       AllowDMA,
                                   SDHCD_TYPE Type)
{
    SDIO_STATUS status = SDIO_STATUS_ERROR;
    TEXT nameBuffer[SDHCD_MAX_DEVICE_NAME];
    PSDHCD_INSTANCE pHcInstance = NULL;
    UINT startFlags = 0;
    
    do {   
            /* setup the name */
        snprintf(nameBuffer, SDHCD_MAX_DEVICE_NAME, "pcistd_%X:%i",
                 (UINT)pPCIdevice,SlotNumber);
         
            /* create the instance */        
        pHcInstance = CreateStdHcdInstance(&pPCIdevice->dev, 
                                           SlotNumber, 
                                           nameBuffer);
      
        if (NULL == pHcInstance) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;     
        }
        
        if ((pId->vendor == 0x1524) && (pId->device == 0x0750)) {            
            /* ENE card has a card IRQ detect issue */                                 
            pHcInstance->Idle1BitIRQ = TRUE;  
            DBG_PRINT(SDDBG_WARN, 
                ("SDIO STDPCI HCD: ENE Host controller detected, using 1bit IRQ mode workaround \n"));
        }
               
            /* map the memory BAR */
        status = MapAddress(pPCIdevice, 
                            pHcInstance->Hcd.pName, 
                            (UINT8)BAR, 
                            &pHcInstance->OsSpecific.Address);
                            
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, 
               ("SDIO STDPCI HCD: Probe - failed to map device memory address %s 0x%X, status %d\n",
                pHcInstance->Hcd.pName, (UINT)pci_resource_start(pPCIdevice, BAR),
                status)); 
            break;                  
        }
        pHcInstance->OsSpecific.InitMask |= SDIO_PCI_BAR_MAPPED;
    
        pHcInstance->pRegs = pHcInstance->OsSpecific.Address.pMapped;       
    
        if (!AllowDMA) {
            startFlags |= START_HCD_FLAGS_FORCE_NO_DMA;
        }
        
        if (ForceSDMA) {
            startFlags |= START_HCD_FLAGS_FORCE_SDMA;
        }
        
            /* startup this instance */
        status = AddStdHcdInstance(pStdCore,
                                   pHcInstance,
                                   startFlags,
                                   NULL,
                                   &HcdSDMADefaults,
                                   &HcdADMADefaults); 
        
    } while (FALSE);     
    
    if (!SDIO_SUCCESS(status)) {
        if (pHcInstance != NULL) {
            CleanupPCIResources(pPCIdevice,pHcInstance);
            DeleteStdHcdInstance(pHcInstance);
        }    
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI Probe - HCD:0x%X ready! \n",(UINT)pHcInstance));    
    }  
    
    return status;
}


static void CleanUpHcdCore(struct pci_dev *pPCIdevice, PSDHCD_CORE_CONTEXT pStdCore)
{   
    PSDHCD_INSTANCE pHcInstance;
    
        /* make sure interrupts are disabled */
    if (pStdCore->CoreReserved1 & SDHCD_PCI_IRQ_HOOKED) {
        pStdCore->CoreReserved1 &= ~SDHCD_PCI_IRQ_HOOKED; 
        free_irq(pPCIdevice->irq, pStdCore);
    }
    
        /* remove all hcd instances associated with this PCI device  */
    while (1) {
        pHcInstance = RemoveStdHcdInstance(pStdCore);
        if (NULL == pHcInstance) {
                /* no more instances */
            break;    
        }
        DBG_PRINT(SDDBG_TRACE, (" SDIO STDPCI HCD: Remove - removed HC Instance:0x%X, HCD:0x%X\n",
            (UINT)pHcInstance, (UINT)&pHcInstance->Hcd));
            /* hcd is now removed, we can clean it up */            
        CleanupPCIResources(pPCIdevice,pHcInstance); 
        DeleteStdHcdInstance(pHcInstance);    
    }
    
    DeleteStdHostCore(pStdCore);     
}

/*
 * Probe - probe to setup our device, if present
*/
static SYSTEM_STATUS Probe(struct pci_dev *pPCIdevice, const struct pci_device_id *pId)
{
    SDIO_STATUS   status = SDIO_STATUS_SUCCESS;
    int ii;
    int count;
    int firstBar;
    int controllers = 0;
    UINT8 config;
    SDHCD_TYPE type = TYPE_CLASS;
    BOOL dmaSupported = FALSE;
    PSDHCD_CORE_CONTEXT pStdCore = NULL;
    SYSTEM_STATUS err = 0;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: Probe - probing for new device\n"));
    if (NULL == pId) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO STDPCI HCD: Probe - no device\n"));
        return -EINVAL;
    }
    
    if (pci_enable_device(pPCIdevice) < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO STDPCI HCD: Probe  - failed to enable device\n"));
        return -ENODEV;
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: setting up card with vendor:0x%X, dev:0x%X\n",
        pId->vendor, pId->device));
    
    if ((pId->vendor == pci_ids[0].vendor) && (pId->device == pci_ids[0].device)) {
        type = TYPE_PCIELLEN;
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: Probe  - found PCI Ellen type\n"));
    }
    
    do {
        
        pStdCore = CreateStdHostCore(pPCIdevice);
        
        if (NULL == pStdCore) {
            err = -ENOMEM; 
            break;  
        }        
            /* get the number of slots supported and the initial BAR for it */
        pci_read_config_byte(pPCIdevice, PCI_CONFIG_SLOT, &config);
        count = GET_SLOT_COUNT(config);
        firstBar = GET_SLOT_FIRST(config);
    
        if (type == TYPE_PCIELLEN) {
            /* move the first bar to the right start place, the original ellen card used a PLX
             * bridge chip which uses the first BAR for control registers */
            firstBar = 2;
            status = InitEllen(pPCIdevice);
            if (!SDIO_SUCCESS(status)) {
                err = -ENODEV;    
                break;
            }
        }
        
        if (count > 0) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI: Probe - slot count: %d, first BAR: %d\n", count, firstBar));
        } else {
            DBG_PRINT(SDDBG_ERROR, ("SDIO STDPCI: Probe - no slots defined, first BAR: %d\n", firstBar));
            //pci_disable_device(pPCIdevice);
            err = -ENODEV;
            break;
        }
        /* see if bus mastering DMA is supported */
        pci_read_config_byte(pPCIdevice, PCI_CONFIG_CLASS, &config);
        if (config & PCI_SD_DMA_SUPPORTED) {
            dmaSupported = TRUE;
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI: Probe - DMA %s enabled in config space: %d\n", 
                                (dmaSupported)? "is": "not", dmaSupported));
        
        
            /* setup an hcd instance for each bar that we have */
        for(ii = 0; ii < count; ii++, firstBar++) {
            status = SetUpOneSlotController(pStdCore,
                                            pPCIdevice,    /* pci device instance */
                                            pId,           /* pci IDs for this instance */
                                            ii,            /* std host slot number */
                                            firstBar,      /* pci BAR for the registers */
                                            dmaSupported,  /* PCI enabled DMA */
                                            type           /* specific PCI card type */
                                            );       
            if (SDIO_SUCCESS(status)) {
                controllers++;    
            }
        }      
        
        if (0 == controllers) {
                /* if none were created, error */
            err = -ENODEV;    
            break;
        }
                
            /* enable the single PCI controller interrupt 
               Interrupts can be called from this point on */
        err = request_irq(pPCIdevice->irq, hcd_sdio_irq, SA_SHIRQ,
                          "stdhcdpci", pStdCore);
                          
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO STDPCI - probe, unable to map interrupt \n"));
            break;
        } 
        
        pStdCore->CoreReserved1 |= SDHCD_PCI_IRQ_HOOKED; 
        
            /* startup the hosts..., this will enable interrupts for card detect */
        status = StartStdHostCore(pStdCore);
        
        if (!SDIO_SUCCESS(status)) {
            err = -ENODEV;  
            break;
        }
               
    } while (FALSE);
    
    if (err < 0) {
        if (pStdCore != NULL) {
            CleanUpHcdCore(pPCIdevice,pStdCore);    
        }
    }
    return err;  
}

/* Remove - remove  device
 * perform the undo of the Probe
*/
static void Remove(struct pci_dev *pPCIdevice) 
{
    PSDHCD_CORE_CONTEXT  pStdCore;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO STDPCI HCD: Remove - removing device\n"));

    pStdCore = GetStdHostCore(pPCIdevice);
    
    if (NULL == pStdCore) {
        DBG_ASSERT(FALSE);
        return;    
    }

    CleanUpHcdCore(pPCIdevice, pStdCore);
    
    /* DO NOT CALL : pci_disable_device(pPCIdevice); this destroys the bus mastering settings
     * on the PCI device, on re-load DMA transfers will fail ***/
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO STDPCI HCD: Remove\n"));
    return;
}

/* SDIO interrupt request */
static irqreturn_t hcd_sdio_irq(int irq, void *context
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
, struct pt_regs * r
#endif
)
{
    irqreturn_t retStat;
     
        /* call shared handling ISR in case this is a mult-slot controller using 1 PCI IRQ.
         * if this was not a mult-slot controller or each controller has it's own system
         * interrupt, we could call HcdSDInterrupt((PSDHCD_INSTANCE)context)) instead */
    if (HandleSharedStdHostInterrupt((PSDHCD_CORE_CONTEXT)context)) {
        retStat = IRQ_HANDLED;
    } else {
        retStat = IRQ_NONE;
    }    
    
    return retStat;
}

/*
 * module init
*/
static int __init sdio_pci_hcd_init(void) {
    SYSTEM_STATUS err;
    
    REL_PRINT(SDDBG_TRACE, ("+SDIO STDPCI HCD: loading....\n"));
    InitStdHostLib();
    
    /* register with the PCI bus driver */
    err = pci_module_init(&sdio_pci_driver);
    if (err < 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO STDPCI HCD: failed to register with system PCI bus driver, %d\n",
                                err));
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO STDPCI HCD \n"));
    return err;
}

/*
 * module cleanup
*/
static void __exit sdio_pci_hcd_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("+SDIO STDPCI HCD: unloaded\n"));
    pci_unregister_driver(&sdio_pci_driver);
    DeinitStdHostLib();
    DBG_PRINT(SDDBG_TRACE, ("-SDIO STDPCI HCD: leave sdio_pci_hcd_cleanup\n"));
}

/* PLX 9030 control registers */
#define INTCSR 0x4C
#define INTCSR_LINTi1ENABLE         (1 << 0)
#define INTCSR_LINTi1STATUS         (1 << 2)
#define INTCSR_LINTi2ENABLE         (1 << 3)
#define INTCSR_LINTi2STATUS         (1 << 5)
#define INTCSR_PCIINTENABLE         (1 << 6)

#define GPIOCTRL 0x54
#define GPIO8_PIN_DIRECTION     (1 << 25)
#define GPIO8_DATA_MASK         (1 << 26)
#define GPIO3_PIN_SELECT        (1 << 9)
#define GPIO3_PIN_DIRECTION     (1 << 10)
#define GPIO3_DATA_MASK         (1 << 11)
#define GPIO2_PIN_SELECT        (1 << 6)
#define GPIO2_PIN_DIRECTION     (1 << 7)
#define GPIO2_DATA_MASK         (1 << 8)
#define GPIO4_PIN_SELECT        (1 << 12)
#define GPIO4_PIN_DIRECTION     (1 << 13)
#define GPIO4_DATA_MASK         (1 << 14)

#define GPIO_CONTROL(pDevice, on,  GpioMask)   \
{                                   \
     UINT32 temp;                    \
     temp = READ_CONTROL_REG32((pDevice),GPIOCTRL);   \
     if (on) temp |= (GpioMask); else temp &= ~(GpioMask);   \
     WRITE_CONTROL_REG32((pDevice),GPIOCTRL, temp);   \
}

static SDIO_STATUS InitEllen(struct pci_dev *pPCIdevice)
{
    SDIO_STATUS  status = SDIO_STATUS_SUCCESS;
    SDHCD_MEMORY controlRegs;    
    UINT32       temp;
    BOOL         mapped = FALSE;
    
    do {      
            /* map the slots control register BAR */
        status = MapAddress(pPCIdevice, 
                            "EllenPCI", 
                            (UINT8)0, 
                            &controlRegs);
    
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        mapped = TRUE;
        
        temp = _READ_WORD_REG((((UINT32)controlRegs.pMapped) + INTCSR));
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: InitEllen Init:INTCSR - 0x%X\n", (UINT)temp));
        
        temp |= INTCSR_LINTi1ENABLE | INTCSR_LINTi2ENABLE | INTCSR_PCIINTENABLE;
        
            /* enable local to PCI interrupts */  
        _WRITE_WORD_REG((((UINT32)controlRegs.pMapped) + INTCSR), (UINT16)temp);
    
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: InitEllen Wrote:INTCSR - 0x%X\n", (UINT)temp));
        
        /*
        temp = READ_CONTROL_REG32((pHcInstance),GPIOCTRL);
        temp &= ~(GPIO3_PIN_SELECT | GPIO2_PIN_SELECT | GPIO4_PIN_SELECT);  
        temp |= (GPIO8_PIN_DIRECTION | GPIO3_PIN_DIRECTION | GPIO2_PIN_DIRECTION | GPIO4_PIN_DIRECTION);               
        WRITE_CONTROL_REG32((pHcInstance),GPIOCTRL, temp);
        DBG_PRINT(SDDBG_TRACE, ("SDIO STDPCI HCD: InitEllen GPIOCTRL - 0x%X\n", (UINT)temp));
        */
        
        TRACE_SIGNAL_DATA_WRITE(pHcInstance, FALSE);
        TRACE_SIGNAL_DATA_READ(pHcInstance, FALSE);
        TRACE_SIGNAL_DATA_ISR(pHcInstance, FALSE);
        TRACE_SIGNAL_DATA_IOCOMP(pHcInstance, FALSE);
        
    } while (FALSE);
    
    if (mapped) {
        UnmapAddress(&controlRegs);     
    }
    return status;    
}


MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);

module_init(sdio_pci_hcd_init);
module_exit(sdio_pci_hcd_cleanup);
