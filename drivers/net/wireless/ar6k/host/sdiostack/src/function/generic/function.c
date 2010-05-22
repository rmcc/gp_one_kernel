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
@file: function.c

@abstract: Generic SDIO Function driver

#notes: includes OS independent portions - Place holder for now
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "../../include/ctsystem.h"

#include "../../include/sdio_busdriver.h"
#include "function.h"


/* find a match */
static BOOL CheckMatch(PSD_PNP_INFO pInfoIn, PSD_PNP_INFO pInfoCheck)
{
    BOOL accept = FALSE;
    
    do {
        if ((pInfoCheck->SDIO_ManufacturerID != 0) && (pInfoCheck->SDIO_ManufacturerCode != 0) &&
            (pInfoIn->SDIO_ManufacturerID == pInfoCheck->SDIO_ManufacturerID) && 
            (pInfoIn->SDIO_ManufacturerCode == pInfoCheck->SDIO_ManufacturerCode) &&
            (pInfoIn->SDIO_FunctionNo == pInfoCheck->SDIO_FunctionNo)) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Probe - card matched (0x%X/0x%X/0x%X)\n",
                                    pInfoIn->SDIO_ManufacturerID,
                                    pInfoIn->SDIO_ManufacturerCode,
                                    pInfoIn->SDIO_FunctionNo));
            accept = TRUE;
            break;
        }     
        if ((pInfoCheck->SDIO_ManufacturerID == 0) && (pInfoCheck->SDIO_FunctionClass != 0) &&
            (pInfoIn->SDIO_FunctionClass == pInfoCheck->SDIO_FunctionClass)){ 
            accept = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Probe - SDIO Class Match:%d \n",
                                    pInfoIn->SDIO_FunctionClass));
            break;
        }    
        if ((pInfoCheck->SDMMC_OEMApplicationID != 0) &&
            (pInfoIn->SDMMC_ManfacturerID == pInfoCheck->SDMMC_ManfacturerID) &&
            (pInfoIn->SDMMC_OEMApplicationID == pInfoCheck->SDMMC_OEMApplicationID)){ 
            accept = TRUE;
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO Generic Function: Probe - SD/MMC MANFID-OEMID Match: MANF:0x%X, OEMID:0x%X \n",
                 pInfoIn->SDMMC_ManfacturerID, pInfoIn->SDMMC_OEMApplicationID));
            break;
        }
            /* SD generic only */
        if ((pInfoCheck->CardFlags != 0) && (pInfoCheck->SDMMC_OEMApplicationID == 0) &&
            (pInfoIn->CardFlags & CARD_SD) && (pInfoCheck->CardFlags & CARD_SD)){ 
            accept = TRUE;
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO Generic Function: Probe - SD Card Type Match (MANF:0x%X, OEMID:0x%X) \n",
                 pInfoIn->SDMMC_ManfacturerID, pInfoIn->SDMMC_OEMApplicationID));
            break;
        }
        
          /* SD generic only */
        if ((pInfoCheck->CardFlags != 0) && (pInfoCheck->SDMMC_OEMApplicationID == 0) &&
            (pInfoIn->CardFlags & CARD_MMC) && (pInfoCheck->CardFlags & CARD_MMC)){ 
            accept = TRUE;
            DBG_PRINT(SDDBG_TRACE, 
                ("SDIO Generic Function: Probe - MMC Card Type Match (MANF:0x%X, OEMID:0x%X) \n",
                 pInfoIn->SDMMC_ManfacturerID, pInfoIn->SDMMC_OEMApplicationID));
            break;
        }
    } while (FALSE);    
    
    return accept;
}

    /* test whether the generic driver should accept this device */
BOOL TestAccept(PGENERIC_FUNCTION_CONTEXT pFuncContext, PSDDEVICE pDevice)
{
    BOOL accept;
    PSD_PNP_INFO  pInfoCheck;
    pInfoCheck = pFuncContext->Function.pIds;   
    accept = FALSE; 
    do {
        if (IS_LAST_SDPNPINFO_ENTRY(pInfoCheck)) {
            break;   
        }
        accept = CheckMatch(pDevice->pId, pInfoCheck);
        pInfoCheck++;        
    } while (!accept);
    return accept;
}
    
                                         
/* delete an instance  */
void DeleteGenericInstance(PGENERIC_FUNCTION_CONTEXT   pFuncContext,
                           PGENERIC_FUNCTION_INSTANCE  pInstance)
{
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pFuncContext->InstanceSem))) {
        return; 
    }
        /* pull it out of the list */
    SDListRemove(&pInstance->SDList);
    SemaphorePost(&pFuncContext->InstanceSem);
    
        /* TODO other cleanup */
        
    KernelFree(pInstance);
}

/* create a generic instance */
PGENERIC_FUNCTION_INSTANCE CreateGenericInstance(PGENERIC_FUNCTION_CONTEXT pFuncContext,
                                                 PSDDEVICE                 pDevice)
{
    PGENERIC_FUNCTION_INSTANCE pInstance = NULL;
    
    do {
        pInstance = (PGENERIC_FUNCTION_INSTANCE)KernelAlloc(sizeof(GENERIC_FUNCTION_INSTANCE));
        if (NULL == pInstance) {
            break;   
        }
        ZERO_POBJECT(pInstance);
        SDLIST_INIT(&pInstance->SDList);
        pInstance->pDevice = pDevice;
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function Instance: 0x%X \n",(INT)pInstance));
        DBG_PRINT(SDDBG_TRACE, (" Card Flags:   0x%X \n",SDDEVICE_GET_CARD_FLAGS(pDevice)));     
        DBG_PRINT(SDDBG_TRACE, (" Card RCA:     0x%X \n",SDDEVICE_GET_CARD_RCA(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper Clock:   %d Hz \n",SDDEVICE_GET_OPER_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max Clock:    %d Hz \n",SDDEVICE_GET_MAX_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlklenLim:  %d bytes \n",SDDEVICE_GET_OPER_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  BlkLen:     %d bytes\n",SDDEVICE_GET_MAX_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlksLim:    %d blocks per trans \n",SDDEVICE_GET_OPER_BLOCKS(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  Blks:       %d blocks per trans \n",SDDEVICE_GET_MAX_BLOCKS(pDevice)));
        
        if ((SDDEVICE_GET_CARD_FLAGS(pDevice) & CARD_SDIO) &&
            (SDDEVICE_GET_SDIO_FUNCNO(pDevice) != 0)) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Func:  %d\n",SDDEVICE_GET_SDIO_FUNCNO(pDevice)));
            DBG_PRINT(SDDBG_TRACE, (" CIS PTR:    0x%X \n",SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice)));
            DBG_PRINT(SDDBG_TRACE, (" CSA PTR:    0x%X \n",SDDEVICE_GET_SDIO_FUNC_CSAPTR(pDevice)));
        }      
        /* TODO other init stuff */
    } while (FALSE);

    
    return pInstance;
}

/* find an instance associated with the SD device */
PGENERIC_FUNCTION_INSTANCE FindGenericInstance(PGENERIC_FUNCTION_CONTEXT pFuncContext,
                                               PSDDEVICE                 pDevice)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PGENERIC_FUNCTION_INSTANCE pInstance = NULL;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, GENERIC_FUNCTION_INSTANCE, SDList);
        if (pInstance->pDevice == pDevice) {
                /* found it */
            break;   
        }
        pInstance = NULL;  
    }    
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pInstance;
}

/* add and instance to our list */
SDIO_STATUS AddGenericInstance(PGENERIC_FUNCTION_CONTEXT    pFuncContext,
                               PGENERIC_FUNCTION_INSTANCE   pInstance)
{
    SDIO_STATUS status;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return status; 
    }
  
    SDListAdd(&pFuncContext->InstanceList,&pInstance->SDList);  
    SemaphorePost(&pFuncContext->InstanceSem);
    
    return SDIO_STATUS_SUCCESS;
}

/* cleanup the function context */
void CleanupFunctionContext(PGENERIC_FUNCTION_CONTEXT pFuncContext)
{
    SemaphoreDelete(&pFuncContext->InstanceSem);  
}

/* initialize the function context */
SDIO_STATUS InitFunctionContext(PGENERIC_FUNCTION_CONTEXT    pFuncContext)
{
    SDIO_STATUS status;
    SDLIST_INIT(&pFuncContext->InstanceList); 
   
    status = SemaphoreInitialize(&pFuncContext->InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return status;
    }
    
    return SDIO_STATUS_SUCCESS;
}



