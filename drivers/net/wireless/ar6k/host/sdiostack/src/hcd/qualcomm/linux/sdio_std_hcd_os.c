// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices. Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms. 


/******************************************************************************
 *
 * @File:       sdio_std_hcd_os.c
 *
 * @Abstract:   Standard SDIO Host Controller Driver
 *
 * @Notice:     Copyright(c), 2008 Atheros Communications, Inc.
 *
 * $ATH_LICENSE_SDIOSTACK0$
 *
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include "sdio_std_hcd_linux.h"

/* global variable */
STDHCD_DEV       StdDevices;
static UINT32    hcdattributes = DEFAULT_ATTRIBUTES;

void InitStdHostLib(void)
{
    ZERO_POBJECT(&StdDevices);
    SDLIST_INIT(&StdDevices.CoreList);
    spin_lock_init(&StdDevices.CoreListLock); 
}

void  DeinitStdHostLib(void)
{
    return;
}

PSDHCD_CORE_CONTEXT CreateStdHostCore(PVOID pBusContext)
{
    PSDHCD_CORE_CONTEXT pStdCore = NULL;
    
    do {
        pStdCore = KernelAlloc(sizeof(SDHCD_CORE_CONTEXT));  
        if (NULL == pStdCore) {
            break;    
        }
        ZERO_POBJECT(pStdCore);
        SDLIST_INIT(&pStdCore->SlotList);
        spin_lock_init(&pStdCore->SlotListLock); 
        pStdCore->pBusContext = pBusContext;
        
        /* add it */
        spin_lock_irq(&StdDevices.CoreListLock);               
        SDListInsertHead(&StdDevices.CoreList, &pStdCore->List); 
        spin_unlock_irq(&StdDevices.CoreListLock);     
    } while (FALSE);
    
    return pStdCore;
}

void DeleteStdHostCore(PSDHCD_CORE_CONTEXT pStdCore)
{   
    spin_lock_irq(&StdDevices.CoreListLock);   
    /* remove */            
    SDListRemove(&pStdCore->List);
    spin_unlock_irq(&StdDevices.CoreListLock);     
    KernelFree(pStdCore);
}

/* find the std core associated with this bus context */
PSDHCD_CORE_CONTEXT GetStdHostCore(PVOID pBusContext)
{
    PSDLIST             pListItem;
    PSDHCD_CORE_CONTEXT pStdCore = NULL;
    
    spin_lock_irq(&StdDevices.CoreListLock);    
    
    do {
        if (SDLIST_IS_EMPTY(&StdDevices.CoreList)) {
            break;    
        }        
          
        SDITERATE_OVER_LIST(&StdDevices.CoreList, pListItem) {
            pStdCore = CONTAINING_STRUCT(pListItem, SDHCD_CORE_CONTEXT, List);
            if (pStdCore->pBusContext == pBusContext) {
                /* found it */
                break;   
            } 
            pStdCore = NULL; 
        }
        
    } while (FALSE);
    
    spin_unlock_irq(&StdDevices.CoreListLock);    
    return pStdCore;
}

/* create a standard host memory instance */
PSDHCD_INSTANCE CreateStdHcdInstance(POS_DEVICE pOSDevice, 
                                     UINT       SlotNumber, 
                                     PTEXT      pName)
{
    PSDHCD_INSTANCE pHcInstance = NULL;
    BOOL            success = FALSE;
    
    do {
        /* allocate an instance for this new device */
        pHcInstance = (PSDHCD_INSTANCE)KernelAlloc(sizeof(SDHCD_INSTANCE));
        
        if (pHcInstance == NULL) {
            printk(" SDIO Qualcomm HCD: no memory for instance\n");
            break;
        }
        
        ZERO_POBJECT(pHcInstance);
        SET_SDIO_STACK_VERSION(&pHcInstance->Hcd);
        
        pHcInstance->OsSpecific.SlotNumber = SlotNumber;
        spin_lock_init(&pHcInstance->OsSpecific.Lock);
        spin_lock_init(&pHcInstance->OsSpecific.RegAccessLock);

        /* initialize work items */
        ATH_INIT_WORK(
            &(pHcInstance->OsSpecific.iocomplete_work),
            hcd_iocomplete_wqueue_handler,
            pHcInstance);

        ATH_INIT_WORK(
            &(pHcInstance->OsSpecific.sdioirq_work),
            hcd_sdioirq_wqueue_handler,
            pHcInstance);

        /* allocate space for the name */ 
        pHcInstance->Hcd.pName = (PTEXT)KernelAlloc(strlen(pName) + 1);
        if (NULL == pHcInstance->Hcd.pName) {
            break;    
        }        
        strcpy(pHcInstance->Hcd.pName, pName);

        /* set OS device for DMA allocations and mapping */
        pHcInstance->Hcd.pDevice = pOSDevice;
        pHcInstance->Hcd.Attributes = hcdattributes;
        pHcInstance->Hcd.MaxBlocksPerTrans = SDIO_SD_MAX_BLOCKS;
        pHcInstance->Hcd.pContext = pHcInstance;
        pHcInstance->Hcd.pRequest = HcdRequest;
        pHcInstance->Hcd.pConfigure = HcdConfig;
        pHcInstance->Hcd.pModule = THIS_MODULE;

        /* success now */
        success = TRUE;
    } while (FALSE);
    
    if (!success && (pHcInstance != NULL)) {
        DeleteStdHcdInstance(pHcInstance);
    }
    
    return pHcInstance;
}

/*
 * AddStdHcdInstance - add the std host controller instance
 */
SDIO_STATUS AddStdHcdInstance(
        PSDHCD_CORE_CONTEXT     pStdCore,
        PSDHCD_INSTANCE         pHcInstance,
        UINT                    Flags,
        PPLAT_OVERRIDE_CALLBACK pCallBack)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
        if (!SDIO_SUCCESS((status = HcdInitialize(pHcInstance)))) {
            DBG_PRINT(
                SDDBG_ERROR,
                (" SDIO Qualcomm HCD: StartStdHcdInstance - "
		 "failed to init HW, status=[%d]\n", status));  
            break;
        }    

        /* mark that the hardware was initialized */
        pHcInstance->InitStateMask |= SDHC_HW_INIT;
        pHcInstance->Hcd.pDmaDescription = NULL;
       
        if (!(Flags & START_HCD_FLAGS_FORCE_NO_DMA)) {

            DBG_PRINT(SDDBG_TRACE,
                      (" SDIO Qualcomm HCD: StartStdHcdInstance - using DMA\n"));

            /*
             * setup DMA buffers for scatter-gather
             * descriptor tables used in DMA
             */
            status = SetupDmaBuffers(pHcInstance);

            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR,
                          (" SDIO Qualcomm HCD: StartStdHcdInstance - "
                           "failed to setup DMA buffer\n"));
                break;
            }
        }
        
        if (pCallBack != NULL) {
            /* allow the platform to override any settings */
            status = pCallBack(pHcInstance);
            if (!SDIO_SUCCESS(status)) {
                break;
            }
        }

        /*
	 * add this instance to our list, we will
	 * start the HCDs later protect the devicelist
	 */
        spin_lock_irq(&pStdCore->SlotListLock);               
        SDListInsertHead(&pStdCore->SlotList, &pHcInstance->List); 
        pStdCore->SlotCount++;
        spin_unlock_irq(&pStdCore->SlotListLock);     
        
    } while (FALSE);
        
    
    if (SDIO_SUCCESS(status)) {
        printk(" SDIO Qualcomm HCD: ready!\n");
    } else {
        /* undo everything */
        DeinitializeStdHcdInstance(pHcInstance);
    }
    
    return status;  
}

INT GetCurrentHcdInstanceCount(PSDHCD_CORE_CONTEXT pStdCore)
{
    return pStdCore->SlotCount;   
}

static void DeinitializeStdHcdInstance(PSDHCD_INSTANCE pHcInstance)
{
    /* wait for any of our work items to run */
    flush_scheduled_work();

    if (pHcInstance->InitStateMask & SDHC_REGISTERED) {
        SDIO_UnregisterHostController(&pHcInstance->Hcd);
        pHcInstance->InitStateMask &= ~SDHC_REGISTERED;
    }

    if (pHcInstance->InitStateMask & SDHC_HW_INIT) {
        HcdDeinitialize(pHcInstance);
        pHcInstance->InitStateMask &= ~SDHC_HW_INIT;
    }

#ifdef DMA_SUPPORT
    /* free any DMA resources */
    if (pHcInstance->OsSpecific.dma.nc_busaddr != (DMA_ADDRESS)NULL) {
        dma_free_coherent(pHcInstance->Hcd.pDevice, 
                          SDHCD_ADMA_DESCRIPTOR_SIZE,
                          pHcInstance->OsSpecific.dma.nc,
                          pHcInstance->OsSpecific.dma.nc_busaddr);
        pHcInstance->OsSpecific.dma.nc = (DMA_ADDRESS)NULL;
        pHcInstance->OsSpecific.dma.nc_busaddr = NULL;
    }     
#endif
    
}

void DeleteStdHcdInstance(PSDHCD_INSTANCE pHcInstance)
{
    if (pHcInstance->Hcd.pName != NULL) {
        KernelFree(pHcInstance->Hcd.pName);
        pHcInstance->Hcd.pName = NULL;
    }

    KernelFree(pHcInstance);
}

/*
 * RemoveStdHcdInstance - remove the hcd instance
 */
PSDHCD_INSTANCE RemoveStdHcdInstance(PSDHCD_CORE_CONTEXT pStdCore)
{
    PSDHCD_INSTANCE pHcInstanceToRemove = NULL;
    PSDLIST         pListItem;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Qualcomm HCD: RemoveStdHcdInstance\n"));

    /* protect the devicelist */
    spin_lock_irq(&pStdCore->SlotListLock);
    
    do {        
        pListItem = SDListRemoveItemFromHead(&pStdCore->SlotList);

        if (NULL == pListItem) {
            break;
        }
    
        pHcInstanceToRemove = CONTAINING_STRUCT(pListItem, SDHCD_INSTANCE, List);
        pStdCore->SlotCount--;
    } while (FALSE);
    
    spin_unlock_irq(&pStdCore->SlotListLock);

    if (pHcInstanceToRemove != NULL) {
        DBG_PRINT(
            SDDBG_TRACE,
            (" SDIO Qualcomm HCD: RemoveStdHcdInstance - Deinitializing [0x%X]\n",
            (UINT)pHcInstanceToRemove));
        DeinitializeStdHcdInstance(pHcInstanceToRemove);   
    }           

    DBG_PRINT(SDDBG_TRACE, ("-SDIO Qualcomm HCD: RemoveStdHcdInstance\n"));

    /* return the instance we found */
    return pHcInstanceToRemove;
}

/*
 * SetupDmaBuffers - allocate required DMA buffers
 */
static SDIO_STATUS SetupDmaBuffers(PSDHCD_INSTANCE pHcInstance)
{
#ifdef DMA_SUPPORT
	memset(&pHcInstance->OsSpecific.dma, 0, sizeof(struct dma_data));
	pHcInstance->OsSpecific.dma.pHost = pHcInstance;
	pHcInstance->OsSpecific.dma.channel = -1;

	pHcInstance->OsSpecific.dma.nc = dma_alloc_coherent(
						pHcInstance->Hcd.pDevice,
						sizeof(struct nc_dmadata),
  						&pHcInstance->OsSpecific.dma.nc_busaddr,
  						GFP_KERNEL);
	if (pHcInstance->OsSpecific.dma.nc == NULL) {
		printk(KERN_ERR "Unable to allocate DMA buffer\n");
		return SDIO_STATUS_NO_RESOURCES;
	}
	memset(pHcInstance->OsSpecific.dma.nc, 0x00, sizeof(struct nc_dmadata));
	pHcInstance->OsSpecific.dma.cmd_busaddr = pHcInstance->OsSpecific.dma.nc_busaddr;
	pHcInstance->OsSpecific.dma.cmdptr_busaddr =
			pHcInstance->OsSpecific.dma.nc_busaddr +
			offsetof(struct nc_dmadata, cmdptr);
	pHcInstance->OsSpecific.dma.channel = SDIO_WLAN_SLOT_DMA_CHAN;
	return SDIO_STATUS_SUCCESS;
#else
	return SDIO_STATUS_ERROR;
#endif
}

/*
 * QueueEventResponse - queues an event in a process context back to the bus driver
 * 
 */
SDIO_STATUS QueueEventResponse(PSDHCD_INSTANCE pHcInstance, INT WorkItemID)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
     struct work_struct *work;
#else
    struct delayed_work *work;
#endif

    if (pHcInstance->ShuttingDown) {
        return SDIO_STATUS_CANCELED;
    }

    switch (WorkItemID) {
        case WORK_ITEM_IO_COMPLETE:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: IO complete INT\n"));
            work = &pHcInstance->OsSpecific.iocomplete_work;
            break;
        case WORK_ITEM_CARD_DETECT:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: card detect INT\n"));
            work = &pHcInstance->OsSpecific.carddetect_work;
            break;
        case WORK_ITEM_SDIO_IRQ:
            DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: SDIO INT\n"));
            work = &pHcInstance->OsSpecific.sdioirq_work;
            break;
        default:
            printk(" SDIO Qualcomm HCD: unknown INT\n");
            DBG_ASSERT(FALSE);
            return SDIO_STATUS_ERROR;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
    if (schedule_work(work) > 0)
#else
    if (schedule_delayed_work(work,0) > 0) 
#endif
    {
        if (WORK_ITEM_IO_COMPLETE == WorkItemID) {
            pHcInstance->RequestCompleteQueued = TRUE;
        }
        return SDIO_STATUS_SUCCESS;
    } else {
        printk(" SDIO Qualcomm HCD: schedule_delayed_work pending\n");
        return SDIO_STATUS_PENDING;
    }
}

/*
 * hcd_iocomplete_wqueue_handler - the work queue for io completion
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void hcd_iocomplete_wqueue_handler(void *context)
{
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)context;

    pHcInstance->RequestCompleteQueued = FALSE;
    if (!pHcInstance->ShuttingDown) {
        SDIO_HandleHcdEvent(&pHcInstance->Hcd, EVENT_HCD_TRANSFER_DONE);
    }
}
#else
static void hcd_iocomplete_wqueue_handler(struct work_struct *work)
{
    PSDHCD_INSTANCE pHcInstance =
        container_of(work, SDHCD_INSTANCE, OsSpecific.iocomplete_work.work);
 
    pHcInstance->RequestCompleteQueued = FALSE;
    if (!pHcInstance->ShuttingDown) {
        SDIO_HandleHcdEvent(&pHcInstance->Hcd, EVENT_HCD_TRANSFER_DONE);
    }
}
#endif

/*
 * hcd_sdioirq_handler - the work queue for handling SDIO IRQ
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static void hcd_sdioirq_wqueue_handler(void *context)
 {
    PSDHCD_INSTANCE pHcInstance = (PSDHCD_INSTANCE)context;

    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: hcd_sdioirq_wqueue_handler\n"));
    if (!pHcInstance->ShuttingDown) {
        SDIO_HandleHcdEvent(&pHcInstance->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
    }
}
#else
static void hcd_sdioirq_wqueue_handler(struct work_struct *work)
{
    PSDHCD_INSTANCE pHcInstance  =
        container_of( work, SDHCD_INSTANCE, OsSpecific.sdioirq_work.work );

    DBG_PRINT(SDDBG_TRACE, (" SDIO Qualcomm HCD: hcd_sdioirq_wqueue_handler\n"));
    if (!pHcInstance->ShuttingDown) {
        SDIO_HandleHcdEvent(&pHcInstance->Hcd, EVENT_HCD_SDIO_IRQ_PENDING);
    }
}
#endif

/*
 * HcdTransferDataDMAEnd - cleanup bus master scatter-gather DMA read/write
 */
void HcdTransferDataDMAEnd(PSDHCD_INSTANCE pHcInstance, PSDREQUEST pReq)
{
    if (pHcInstance->OsSpecific.SGcount > 0) {
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            dma_unmap_sg(pHcInstance->Hcd.pDevice,
                         pHcInstance->OsSpecific.pDmaList,
                         pHcInstance->OsSpecific.SGcount,
                         DMA_TO_DEVICE);
        } else {
            dma_unmap_sg(pHcInstance->Hcd.pDevice,
                         pHcInstance->OsSpecific.pDmaList,
                         pHcInstance->OsSpecific.SGcount,
                         DMA_FROM_DEVICE);
        }
        pHcInstance->OsSpecific.SGcount = 0;
    }
}

void DumpDMADescriptorsInfo(PSDHCD_INSTANCE pHcInstance)
{
    if (IS_HCD_ADMA(pHcInstance)) {
        DBG_PRINT(SDDBG_ERROR,
                  (" SDIO Qualcomm HCD: ADMA Descriptor Start (PHYS):[0x%X]\n",
                  (UINT32)pHcInstance->OsSpecific.hDmaBuffer));

        SDLIB_PrintBuffer((PUCHAR)pHcInstance->OsSpecific.pDmaBuffer,
                          SDHCD_ADMA_DESCRIPTOR_SIZE,
                          " SDIO Qualcomm HCD: ALL DMA Descriptors");                           
    }
}

/*
 * start the standard host instances 
 * this function registers the standard host drivers
 * and queues an event to check the slots
 */
SDIO_STATUS StartStdHostCore(PSDHCD_CORE_CONTEXT pStdCore)
{
    SDIO_STATUS         status = SDIO_STATUS_SUCCESS;
    PSDHCD_INSTANCE     pHcInstance;
    PSDLIST             pListItem;
    INT                 coreStarts = 0;
    
    spin_lock_irq(&pStdCore->SlotListLock);
     
    do {
        if (SDLIST_IS_EMPTY(&pStdCore->SlotList)) {
            break;    
        }
         
        SDITERATE_OVER_LIST(&pStdCore->SlotList, pListItem) {
            
            pHcInstance = CONTAINING_STRUCT(pListItem, SDHCD_INSTANCE, List);
            
            spin_unlock_irq(&pStdCore->SlotListLock);
            
            /* register with the SDIO bus driver */
            status = SDIO_RegisterHostController(&pHcInstance->Hcd);
            
            spin_lock_irq(&pStdCore->SlotListLock);
            
            if (!SDIO_SUCCESS(status)) {
                printk(" SDIO Qualcomm HCD: failed to register with host, status=[%d]\n",status);
                break;
            }
            
            coreStarts++;
            
            /* mark that it has been registered */
            pHcInstance->InitStateMask |= SDHC_REGISTERED; 
        }
    } while (FALSE);

    spin_unlock_irq(&pStdCore->SlotListLock);

    if (0 == coreStarts) {
        return SDIO_STATUS_ERROR;
    }

    return SDIO_STATUS_SUCCESS;
}

/* module parameters */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");
module_param(hcdattributes, int, 0644);
MODULE_PARM_DESC(hcdattributes, "Qualcomm Host Attributes");

