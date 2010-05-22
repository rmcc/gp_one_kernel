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
@file: sdio_function_os.c

@abstract: Linux implementation module for SDIO Generic Function driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 7;
#include "../../../include/ctsystem.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include "../../../include/sdio_busdriver.h"
#include "../function.h"

#define DESCRIPTION "SDIO Generic Function Driver"
#define AUTHOR "Atheros Communications, Inc."

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

/* device base name */
#define SDIO_FUNCTION_BASE "sdiofn"
#define SDIO_FUNCTION_MAX_DEVICES 1

BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
int sdio_function_open(struct inode *inode, struct file *filp);
int sdio_function_release(struct inode *inode, struct file *filp);
int sdio_function_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

/* devices we support, null terminated */
static SD_PNP_INFO Ids[] = {
    {.SDIO_ManufacturerID = 0xaa55,    /* specific SDIO card */
     .SDIO_ManufacturerCode = 0x5555, 
     .SDIO_FunctionNo = 1},
    {.SDIO_FunctionClass = 0xa},        /* SDIO class test */
    {.SDMMC_ManfacturerID = 55,       /* specific SD card  */
     .SDMMC_OEMApplicationID = 55}, 
    {.SDMMC_ManfacturerID = 0x00,       /* specific MMC card  */
     .SDMMC_OEMApplicationID = 0x0002}, 
    {.CardFlags = CARD_SD},           /* all SD cards */
    {.CardFlags = CARD_MMC},          /* all MMC cards */
    {}
};


/* driver data */
static GENERIC_FUNCTION_CONTEXT FunctionContext = {
    .Function.Version = CT_SDIO_STACK_VERSION_CODE,
	.Function.pName    = "sdio_generic",
    .Function.MaxDevices = 1,
    .Function.NumDevices = 0,
    .Function.pIds     = Ids,
    .Function.pProbe   = Probe,
    .Function.pRemove  = Remove,
    .Function.pSuspend = NULL,
    .Function.pResume  = NULL,
    .Function.pWake    = NULL,
    .Function.pContext = &FunctionContext, 
}; 


static struct file_operations sdio_function_fops_dev = {
    .ioctl      = sdio_function_ioctl,
    .open       = sdio_function_open,
    .release    = sdio_function_release,
};


static void CleanupInstance(PGENERIC_FUNCTION_CONTEXT  pFunctionContext,
                            PGENERIC_FUNCTION_INSTANCE pInstance)
{
    
    if (pInstance->Config.CharRegistered) {  
            /* unregister char driver */ 
        unregister_chrdev(pInstance->Config.Major, SDIO_FUNCTION_BASE);
    }
    DeleteGenericInstance(pFunctionContext,
                          pInstance);     
}


/*
 * Probe - a device potentially for us
*/
BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PGENERIC_FUNCTION_CONTEXT pFunctionContext = 
                                (PGENERIC_FUNCTION_CONTEXT)pFunction->pContext;
	SYSTEM_STATUS err = 0;
	BOOL          accept;
    PGENERIC_FUNCTION_INSTANCE pNewInstance = NULL;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Probe - enter\n"));
    if (!TestAccept(pFunctionContext,pDevice)) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Probe - not ours \n"));
        return FALSE;
    }    
    accept = FALSE;
    do {
        pNewInstance = CreateGenericInstance(pFunctionContext,pDevice);
        if (NULL == pNewInstance) {
            break;    
        }
        pNewInstance->Config.Major = SDIO_FUNCTION_MAJOR; 
        pNewInstance->Config.CharRegistered = TRUE;     
            /* create a character device for the user mode to access this function */  
            /* TODO : how do we associate and instance with a char instance ? */    
        err = register_chrdev(pNewInstance->Config.Major, 
                              SDIO_FUNCTION_BASE, 
                              &sdio_function_fops_dev);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Generic Function: Probe - could not register device, %d\n", err));
            pNewInstance->Config.CharRegistered = FALSE;
            break;            
        }
        
        if (pNewInstance->Config.Major == 0) {
            pNewInstance->Config.Major =  err; /* save dynamically assigned major number for cleanup*/
        }
           /* add it to the list */
        if (!SDIO_SUCCESS(AddGenericInstance(pFunctionContext, pNewInstance))) {
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
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PGENERIC_FUNCTION_CONTEXT pFunctionContext = 
                                (PGENERIC_FUNCTION_CONTEXT)pFunction->pContext;
    PGENERIC_FUNCTION_INSTANCE pInstance;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Remove - enter\n"));
   
    pInstance = FindGenericInstance(pFunctionContext,pDevice);
    
    if (pInstance != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: Removing instance: 0x%X From Remove() \n",
                                (INT)pInstance));
        CleanupInstance(pFunctionContext, pInstance);    
    } else {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Generic Function: could not find matching instance! \n"));
    }    
}

/* 
 * sdio_function_open - handle the open request
*/
int sdio_function_open(struct inode *inode, struct file *filp)
{
    unsigned int minor = MINOR(inode->i_rdev);
    //unsigned int major = MAJOR(inode->i_rdev);
    DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: sdio_function_open - enter\n"));
        
    if (minor >= SDIO_FUNCTION_MAX_DEVICES) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Generic Function: sdio_function_open - minor type too large\n"));
        return -ENODEV;
    }
    
    /* reference this module */
    try_module_get(THIS_MODULE);

    return 0;          /* success */
}

/* sdio_function_release - handle the close request
 * undo what we did in open()
*/
int sdio_function_release(struct inode *inode, struct file *filp)
{
    module_put(THIS_MODULE);
    return 0;
}
 
/*
    handle any ioctls
*/
int sdio_function_ioctl(struct inode *inode, struct file *filp,
                        unsigned int cmd, unsigned long arg) {
    //struct sdio_bd_device_context *device_context = (struct sdio_bd_device_context *)filp->private_data;
    
    /* call internal_ioctl with slot 0 */
//??    return sdio_bd_internal_ioctl(&device_context->slot_context[0], cmd, arg);
	return -ENODEV;
}

/*
 * module init
*/
static int __init sdio_function_init(void) {
    SDIO_STATUS status;
    REL_PRINT(SDDBG_TRACE, ("SDIO Generic Function: enter sdio_function_init\n"));
   
    status = InitFunctionContext(&FunctionContext);
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);       
    }
    
    /* register with bus driver core */
    return SDIOErrorToOSError(SDIO_RegisterFunction(&FunctionContext.Function));
}

/*
 * module cleanup
*/
static void __exit sdio_function_cleanup(void) {
    
    REL_PRINT(SDDBG_TRACE, ("SDIO Generic Function: exit sdio_function_cleanup\n"));
        /* unregister, this will call Remove() for each device */
    SDIO_UnregisterFunction(&FunctionContext.Function);
    CleanupFunctionContext(&FunctionContext);
}


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_function_init);
module_exit(sdio_function_cleanup);

