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
@file: benchmark_os.c

@abstract: Linux implementation for the SDIO Benchmark Function driver

#notes:
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 7;
#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <sdio_lib.h>
#include "../../common/benchmark.h"
#include "../benchmark_ct.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/page.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/dma-mapping.h>
#endif

#define DESCRIPTION "SDIO Benchmark Function Driver"
#define AUTHOR "Atheros Communications, Inc."

/* module param defaults */
static int sdio_manfID = 0;
static int sdio_manfcode = 0;
static int sdio_funcno = 0;
static int sdio_class = 0;

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel,"debuglevel 0-7, controls debug prints");

module_param(sdio_manfID, int, 0644);
MODULE_PARM_DESC(sdio_manfID,"SDIO manufacturer ID override");

module_param(sdio_manfcode, int, 0644);
MODULE_PARM_DESC(sdio_manfcode,"SDIO manufacturer Code overide");

module_param(sdio_funcno, int, 0644);
MODULE_PARM_DESC(sdio_funcno,"SDIO function number override");

module_param(sdio_class, int, 0644);
MODULE_PARM_DESC(sdio_class,"SDIO function class override");

BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void CleanupInstance(PBENCHMARK_FUNCTION_CONTEXT  pFunctionContext,
                            PBENCHMARK_FUNCTION_INSTANCE pInstance);
                            
/* devices we support, null terminated */
static SD_PNP_INFO Ids[] = {
    {.SDIO_ManufacturerID = 0x55AA,    /* MARS I */
     .SDIO_ManufacturerCode = 0x2211, 
     .SDIO_FunctionNo = 1,
     .SDIO_FunctionClass = 1},
    {.SDIO_ManufacturerID = 0x0000,    /* MARS II */
     .SDIO_ManufacturerCode = 0x0388, 
     .SDIO_FunctionNo = 1,
     .SDIO_FunctionClass = 1},
    {.CardFlags = CARD_SD},     /* accept any SD card */
    {.CardFlags = CARD_MMC},    /* accept any MMC card */
    {}
};


/* driver wide data */
static BENCHMARK_FUNCTION_CONTEXT FunctionContext = {
	.Function.pName    = "sdio_benchmark",
    .Function.Version  = CT_SDIO_STACK_VERSION_CODE,
    .Function.MaxDevices = SDIO_BENCHMARK_FUNCTION_MAX_DEVICES,
    .Function.NumDevices = 0,
    .Function.pIds     = Ids,
    .Function.pProbe   = Probe,
    .Function.pRemove  = Remove,
    .Function.pSuspend = NULL,
    .Function.pResume  = NULL,
    .Function.pWake    = NULL,
    .Function.pContext = &FunctionContext, 
}; 

/* static buffers for data transfers and results */
static UINT8 g_TestBuffer[BLOCK_BUFFER_BYTES][SDIO_BENCHMARK_FUNCTION_MAX_DEVICES];

/*
 * Probe - a device potentially for us
 * 
*/
BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PBENCHMARK_FUNCTION_CONTEXT pFunctionContext = 
                                (PBENCHMARK_FUNCTION_CONTEXT)pFunction->pContext;
    BOOL          accept;
    PBENCHMARK_FUNCTION_INSTANCE pNewInstance = NULL;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO BenchMark Function: Probe\n"));
    
    accept = FALSE;
    
    if (pDevice->pId[0].CardFlags & CARD_MMC) {
        accept = TRUE;    
    }      
    
    if (pDevice->pId[0].CardFlags & CARD_SD) {
        accept = TRUE;    
    }   
    
        /* make sure this is a device we can handle 
         * the card must match the manufacturer and card ID */
    if ((pDevice->pId[0].SDIO_ManufacturerID == 
         pFunctionContext->Function.pIds[0].SDIO_ManufacturerID) &&
         (pDevice->pId[0].SDIO_ManufacturerCode == 
          pFunctionContext->Function.pIds[0].SDIO_ManufacturerCode)){ 
         accept = TRUE;
    }
        /* check for class */
    if (pFunctionContext->Function.pIds[0].SDIO_FunctionClass != 0) {
        if (pDevice->pId[0].SDIO_FunctionClass == 
             pFunctionContext->Function.pIds[0].SDIO_FunctionClass) {
            accept = TRUE;            
        }
    }
    
    if (!accept) {
         DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: Probe - not our card (0x%X/0x%X/0x%X/0x%X)\n",
                            pDevice->pId[0].SDIO_ManufacturerID,
                            pDevice->pId[0].SDIO_ManufacturerCode,
                            pDevice->pId[0].SDIO_FunctionNo,                            
                            pDevice->pId[0].SDIO_FunctionClass));
        return FALSE; 
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: Probe - card matched (0x%X/0x%X/0x%X/0x%X)\n",
                            pDevice->pId[0].SDIO_ManufacturerID,
                            pDevice->pId[0].SDIO_ManufacturerCode,
                            pDevice->pId[0].SDIO_FunctionNo,
                            pDevice->pId[0].SDIO_FunctionClass));
                            
    accept = FALSE;
    
    do {
        /* create a new instance of a device and iniinitialize the device */
        pNewInstance = 
            (PBENCHMARK_FUNCTION_INSTANCE)KernelAlloc(sizeof(BENCHMARK_FUNCTION_INSTANCE));
        if (NULL == pNewInstance) {
            break;    
        }
        
        ZERO_POBJECT(pNewInstance);
                
        if (!SDIO_SUCCESS(InitializeInstance(pFunctionContext,pNewInstance,pDevice))) {            
            break; 
        }      
      
           /* add it to the list */
        if (!SDIO_SUCCESS(AddInstance(pFunctionContext, pNewInstance))) {
            break;               
        }
       
        accept = TRUE;
    } while (FALSE); 
    
    if (!accept && (pNewInstance != NULL)) {
        CleanupInstance(pFunctionContext, pNewInstance);
    }
        
    return accept;
}

/*
 * Remove - our device is being removed
*/
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice) 
{
    PBENCHMARK_FUNCTION_CONTEXT pFunctionContext = 
                                (PBENCHMARK_FUNCTION_CONTEXT)pFunction->pContext;
    PBENCHMARK_FUNCTION_INSTANCE pInstance;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO BenchMark Function: Remove\n"));
   
    pInstance = FindInstance(pFunctionContext,pDevice);
    
    if (pInstance != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: Removing instance: 0x%X From Remove()\n",
                                (INT)pInstance));
        CleanupInstance(pFunctionContext, pInstance);    
    } else {
        DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: could not find matching instance!\n"));
    }    
}

static void CleanupInstance(PBENCHMARK_FUNCTION_CONTEXT  pFunctionContext,
                            PBENCHMARK_FUNCTION_INSTANCE pInstance)
{
    
    if (pInstance->Config.pDmaBuffer != NULL) { 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        dma_free_coherent(SD_GET_OS_DEVICE(pInstance->pDevice), 
                          pInstance->Config.DmaBufferSize,
                          pInstance->Config.pDmaBuffer,
                          pInstance->Config.DmaAddress);
          
#else
        consistent_free(pInstance->Config.pDmaBuffer, 
                        pInstance->Config.DmaBufferSize, 
                        pInstance->Config.DmaAddress);
#endif    
        pInstance->Config.pDmaBuffer = NULL;                     
    }
    
    DeleteInstance(pFunctionContext, pInstance);     
    KernelFree(pInstance);
}


/*
 * AllocateBenchMarkBuffers - allocate the transfer buffers
*/
SDIO_STATUS AllocateBenchMarkBuffers(PBENCHMARK_FUNCTION_INSTANCE pInstance)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
       
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        if ((SDGET_DMA_DESCRIPTION(pInstance->pDevice) != NULL)) {
                /* allocate DMAable buffers */
            pInstance->Config.DmaBufferSize = BLOCK_BUFFER_BYTES;
                        
            pInstance->Config.pDmaBuffer  = (PUINT8)dma_alloc_coherent(SD_GET_OS_DEVICE(pInstance->pDevice), 
                                                            pInstance->Config.DmaBufferSize , 
                                                            &pInstance->Config.DmaAddress, 
                                                            GFP_DMA);
            if (pInstance->Config.pDmaBuffer == NULL) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: AllocateBenchMarkBuffers - unable to get DMA buffer\n"));
               
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: AllocateBenchMarkBuffers - pDmaBuffer: 0x%X, DmaAddress: 0x%X\n",
                                (UINT)pInstance->Config.pDmaBuffer, (UINT)pInstance->Config.DmaAddress));
                pInstance->Config.pDmaBufferEnd = pInstance->Config.pDmaBuffer + 
                                              pInstance->Config.DmaBufferSize - 1;
            }      
            /* fall through and allocate PIO mode buffers */
        }   
#else 
        if ((SDGET_DMA_DESCRIPTION(pInstance->pDevice) != NULL)) {
                /* allocate DMAable buffers */
            pInstance->Config.DmaBufferSize = BLOCK_BUFFER_BYTES;
                        
            pInstance->Config.pDmaBuffer  = (PUINT8)consistent_alloc(GFP_KERNEL | GFP_DMA | GFP_ATOMIC,
                                                                     pInstance->Config.DmaBufferSize, 
                                                                     &pInstance->Config.DmaAddress); 
                                         
            if (pInstance->Config.pDmaBuffer == NULL) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: AllocateBenchMarkBuffers - unable to get DMA buffer\n"));
               
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: AllocateBenchMarkBuffers - pDmaBuffer: 0x%X, DmaAddress: 0x%X\n",
                                (UINT)pInstance->Config.pDmaBuffer, (UINT)pInstance->Config.DmaAddress));
                pInstance->Config.pDmaBufferEnd = pInstance->Config.pDmaBuffer + 
                                              pInstance->Config.DmaBufferSize - 1;
            }      
            /* fall through and allocate PIO mode buffers */
        }   
  
#endif   
       
            /* allocate nomal PIO buffers */
        pInstance->Config.BufferSize = BLOCK_BUFFER_BYTES;
        
        if (SDDEVICE_GET_SLOT_NUMBER(pInstance->pDevice) >= SDIO_BENCHMARK_FUNCTION_MAX_DEVICES) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: AllocateBuffers - can't allocate verify/data buffers\n"));
            status = SDIO_STATUS_NO_RESOURCES;
            break;    
        }
        
            /* we use the unique slot number as the device index, get a buffer for verifies */ 
        pInstance->Config.pTestBuffer = &g_TestBuffer[0][SDDEVICE_GET_SLOT_NUMBER(pInstance->pDevice)];
        DBG_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function: AllocateBuffers - non-DMA pBuffer: 0x%X \n",
                      (UINT)pInstance->Config.pTestBuffer));

    } while (FALSE);
   
    return status;
}

SDIO_STATUS CheckDMABuildSGlist(PBENCHMARK_FUNCTION_INSTANCE pInstance, 
                                PUINT8                       pBuffer,
                                UINT                         ByteCount, 
                                PUINT                        pSGcount,
                                PSDDMA_DESCRIPTOR            *ppDescrip)
{
    PSDDMA_DESCRIPTION pDmaDescrip = SDGET_DMA_DESCRIPTION(pInstance->pDevice);
    DMA_ADDRESS        address;
    UINT32             offset;
    INT                index;
    SDIO_STATUS        status = SDIO_STATUS_SUCCESS;
    
    *pSGcount = 0;
    *ppDescrip = NULL;
    
    do {
        
        if (NULL == pDmaDescrip) {
                /* DMA not supported by this device */
            break;
        }
        
        if (((UINT32)pBuffer < (UINT32)pInstance->Config.pDmaBuffer) || 
            ((UINT32)pBuffer > (UINT32)pInstance->Config.pDmaBufferEnd)) {
                /* not using our DMA buffer */
            break;        
        }
        
            /* fail the check so that the bench mark can terminate early if
             * the buffer alignment doesn't work out */
        status = SDIO_STATUS_UNSUPPORTED;
        
        offset = pBuffer - pInstance->Config.pDmaBuffer;
        address = pInstance->Config.DmaAddress + offset;
        
        
        /* now verify that the buffer meets the HCD's DMA requirements */
        /* since we are only using a single contiguous buffer, we support HCDs that support DMA or Scatter-Gather DMA,
           DMA is just scatter-gather DMA with only single scatter-gather entry */
        if (!(pDmaDescrip->Flags & (SDDMA_DESCRIPTION_FLAG_DMA | SDDMA_DESCRIPTION_FLAG_SGDMA))) {
            DBG_ASSERT(FALSE);
            break;
        }
        
        if (pDmaDescrip->MaxDescriptors == 0) {
            DBG_ASSERT(FALSE);
            break;    
        }
        
         /* check that the length of our descriptor is not too big,
           if it was and scatter-gather was supported by the HCD, we could break this transfer into more SG entries  */
        if (ByteCount > (pDmaDescrip->MaxBytesPerDescriptor * NUM_SCATTER_GATHER_ENTRIES)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: BytesCount %d exceeds per descriptor length %d , count %d\n",
                      ByteCount,pDmaDescrip->MaxBytesPerDescriptor,NUM_SCATTER_GATHER_ENTRIES));    
            break;
        }
        
            /* check that the start address is properly aligned for the HCD */
        if (address & pDmaDescrip->AddressAlignment) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: DMA Phys Address: 0x%X is not aligned (requires 0x%X)\n",
                        (UINT)address,pDmaDescrip->AddressAlignment));        
          
            break;
        }   
        
        /* no unwanted address bits, DMA address is okay */    
        
        /* check that the length is properly aligned */
        if ((ByteCount & pDmaDescrip->LengthAlignment)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: Bytecount %d is not aligned (requires 0x%X)\n",
                   ByteCount,pDmaDescrip->LengthAlignment));   
            break;   
        }
        
        
        /* no unwanted length bits, number of bytes is okay */
         
            /* assemble SG list */   
        for (index = 0 ; (index < NUM_SCATTER_GATHER_ENTRIES) && (ByteCount > 0); index++) {
             /* we have one scatter-gather list pre allocated, and just need to fill the data for our contiguous data buffer*/ 
            pInstance->Config.SGList[index].page = 
                            virt_to_page(pInstance->Config.pDmaBuffer + offset);
            pInstance->Config.SGList[index].offset = 
                            address - page_to_phys(pInstance->Config.SGList[index].page);
            pInstance->Config.SGList[index].length =  
                            min(ByteCount,pDmaDescrip->MaxBytesPerDescriptor);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,20)
                /* under linux 2.4.20, the address field must be filled in for the HCD
                 * in order to call consistent_sync() to flush the caches, this API
                 * requires a virtual address range, in 2.6 the .address field was
                 * removed */
            pInstance->Config.SGList[index].address = pInstance->Config.pDmaBuffer + offset;
#endif
                /* advance address */
            address += pInstance->Config.SGList[index].length;
            ByteCount -= pInstance->Config.SGList[index].length ;
            *pSGcount += 1;
        }
        
        if (*pSGcount > pDmaDescrip->MaxDescriptors) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: %d descriptors required, max:%d\n",
                   *pSGcount,pDmaDescrip->MaxDescriptors));   
            break;    
        }
        
        status = SDIO_STATUS_SUCCESS;
                 
        *ppDescrip = pInstance->Config.SGList;

    } while (FALSE);
        
    return status;
}

/* fetch benchmark parameters */

void GetBenchMarkParameters(PVOID pContext, PBM_TEST_PARAMS pParams)
{
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
 
        /* return PIO buffers */   
    pParams->pTestBuffer = pInstance->Config.pTestBuffer;
    pParams->BufferSize = pInstance->Config.BufferSize;
    
    if (pInstance->Config.pDmaBuffer != NULL) {
        REL_PRINT(SDDBG_TRACE, ("SDIO BenchMark Function - Direct DMA Capable...\n"));
        pParams->pDMABuffer = pInstance->Config.pDmaBuffer;
        pParams->DMABufferSize = pInstance->Config.DmaBufferSize;
    }
    
    pParams->TETestCard = pInstance->TETestCard;
}
 
/*
 * module init
*/
static int __init sdio_function_init(void) {
    SDIO_STATUS status;
    REL_PRINT(SDDBG_TRACE, ("+SDIO BenchMark Function - load\n"));
   
    if (sdio_manfID != 0) {
        Ids[0].SDIO_ManufacturerID = (UINT16)sdio_manfID;  
        DBG_PRINT(SDDBG_WARN, ("SDIO BenchMark Function: Override MANFID: 0x%x \n",sdio_manfID)); 
    }
    if (sdio_manfcode != 0) {
        Ids[0].SDIO_ManufacturerCode = (UINT16)sdio_manfcode; 
         DBG_PRINT(SDDBG_WARN, ("SDIO BenchMark Function: Override MANFCODE: 0x%x \n",sdio_manfcode)); 
    }
    if (sdio_funcno != 0) {
        Ids[0].SDIO_FunctionNo =  (UINT8)sdio_funcno;
        DBG_PRINT(SDDBG_WARN, ("SDIO BenchMark Function: Override FuncNo: 0x%x \n",sdio_funcno)); 
    }
    if (sdio_class != 0) {
        Ids[0].SDIO_FunctionClass = (UINT8)sdio_class;
        DBG_PRINT(SDDBG_WARN, ("SDIO BenchMark Function: Override Class: 0x%x \n",sdio_class)); 
    }
    
    status = InitFunctionContext(&FunctionContext);
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);       
    }
   
    REL_PRINT(SDDBG_TRACE, ("-SDIO BenchMark Function - load\n"));
    /* register with bus driver core */
    return SDIOErrorToOSError(SDIO_RegisterFunction(&FunctionContext.Function));
}

/*
 * module cleanup
*/
static void __exit sdio_function_cleanup(void) {
    
    REL_PRINT(SDDBG_TRACE, ("+SDIO BenchMark Function - unload\n"));
        /* unregister, this will call Remove() for each device */
    SDIO_UnregisterFunction(&FunctionContext.Function);
    CleanupFunctionContext(&FunctionContext);
    REL_PRINT(SDDBG_TRACE, ("-SDIO BenchMark Function - unload\n"));
}


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_function_init);
module_exit(sdio_function_cleanup);

