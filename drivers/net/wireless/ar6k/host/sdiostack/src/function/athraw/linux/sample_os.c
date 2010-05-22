/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample_os.c

@abstract: Linux implementation module for the SDIO Sample Function driver

#notes: includes module load and unload functions
 
@notice: Copyright (c), 2005-2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 7;
#include <ctsystem.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <sdio_busdriver.h>
#include "../sample.h"

#define DESCRIPTION "Atheros Raw SPI Protocol Function Driver"
#define AUTHOR "Atheros Communications, Inc."

/* module param defaults */

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel,"debuglevel 0-7, controls debug prints");


/* device base name */
#define SDIO_SAMPLE_FUNCTION_BASE "sdiosam"
#define SDIO_SAMPLE_FUNCTION_MAX_DEVICES 4

BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void CleanupInstance(PSAMPLE_FUNCTION_CONTEXT  pFunctionContext,
                            PSAMPLE_FUNCTION_INSTANCE pInstance);


static SD_PNP_INFO Ids[] = {

    {.CardFlags = CARD_RAW},    /* register for raw cards */
    {}
};


/* driver wide data */
static SAMPLE_FUNCTION_CONTEXT FunctionContext = {
	.Function.pName    = "sdio_sample",
    .Function.Version  = CT_SDIO_STACK_VERSION_CODE,
    .Function.MaxDevices = SDIO_SAMPLE_FUNCTION_MAX_DEVICES,
    .Function.NumDevices = 0,
    .Function.pIds     = Ids,
    .Function.pProbe   = Probe,
    .Function.pRemove  = Remove,
    .Function.pSuspend = NULL,
    .Function.pResume  = NULL,
    .Function.pWake    = NULL,
    .Function.pContext = &FunctionContext, 
}; 

/*
 * Probe - a device potentially for us
 * 
 * notes: probe is called when the bus driver has located a card for us to support.
 *        We accept the new device by returning TRUE.
*/
BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PSAMPLE_FUNCTION_CONTEXT pFunctionContext = 
                                (PSAMPLE_FUNCTION_CONTEXT)pFunction->pContext;
	BOOL          accept;
    PSAMPLE_FUNCTION_INSTANCE pNewInstance = NULL;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Sample Function: Probe\n"));
    
    accept = FALSE;
            
    if (!(pDevice->pId[0].CardFlags & CARD_RAW)) {
        return FALSE;   
    }
    
        /* there can be other "raw" HCD drivers, make sure this is ours */
    if (strstr(SDDEVICE_GET_HCDNAME(pDevice), SDIO_RAW_BD_BASE) == NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Probe - Not our HCD :%s\n",
                            SDDEVICE_GET_HCDNAME(pDevice)));
        return FALSE;         
    }
  
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Probe - card matched on HCD :%s\n",
                            SDDEVICE_GET_HCDNAME(pDevice)));
                            
    accept = FALSE;
    do {
        /* create a new instance of a device and ininitialize the device */
        pNewInstance = (PSAMPLE_FUNCTION_INSTANCE)KernelAlloc(sizeof(SAMPLE_FUNCTION_INSTANCE));
        if (NULL == pNewInstance) {
            break;    
        }
        ZERO_POBJECT(pNewInstance);
        
        if (!SDIO_SUCCESS(SemaphoreInitialize(&pNewInstance->Config.RemoveSem,1))) {
            break;    
        } 
        
        if (!SDIO_SUCCESS(InitializeInstance(pFunctionContext,pNewInstance,pDevice))) {            
            break; 
        }      
           
           /* add it to the list */
        if (!SDIO_SUCCESS(AddSampleInstance(pFunctionContext, pNewInstance))) {
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
    PSAMPLE_FUNCTION_CONTEXT pFunctionContext = 
                                (PSAMPLE_FUNCTION_CONTEXT)pFunction->pContext;
    PSAMPLE_FUNCTION_INSTANCE pInstance;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Sample Function: Remove\n"));
   
    pInstance = FindSampleInstance(pFunctionContext,pDevice);
    
    if (pInstance != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Removing instance: 0x%X From Remove()\n",
                                (INT)pInstance));
        CleanupInstance(pFunctionContext, pInstance);    
    } else {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: could not find matching instance!\n"));
    }    
}

static void CleanupInstance(PSAMPLE_FUNCTION_CONTEXT  pFunctionContext,
                            PSAMPLE_FUNCTION_INSTANCE pInstance)
{
    pInstance->Config.Removing = TRUE;
    
    if (!SDIO_SUCCESS(SemaphorePend(&pInstance->Config.RemoveSem))) {
         DBG_ASSERT(FALSE);
         return;
    }
    DeleteSampleInstance(pFunctionContext, pInstance);     
    SemaphoreDelete(&pInstance->Config.RemoveSem); 
    KernelFree(pInstance);
}



/*
 * module init
*/
static int __init sdio_function_init(void) {
    SDIO_STATUS status;
    REL_PRINT(SDDBG_TRACE, ("+SDIO Sample Function - load\n"));
   
    
    status = InitFunctionContext(&FunctionContext);
   
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);       
    }
   
    REL_PRINT(SDDBG_TRACE, ("-SDIO Sample Function - load\n"));
    /* register with bus driver core */
    return SDIOErrorToOSError(SDIO_RegisterFunction(&FunctionContext.Function));
}

/*
 * module cleanup
*/
static void __exit sdio_function_cleanup(void) {
    
    REL_PRINT(SDDBG_TRACE, ("+SDIO Sample Function - unload\n"));
        /* unregister, this will call Remove() for each device */
    SDIO_UnregisterFunction(&FunctionContext.Function);
    CleanupFunctionContext(&FunctionContext);
    REL_PRINT(SDDBG_TRACE, ("-SDIO Sample Function - unload\n"));
}


MODULE_LICENSE("Proprietary");
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_function_init);
module_exit(sdio_function_cleanup);

