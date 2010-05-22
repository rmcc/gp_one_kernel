// Copyright (c) 2004, 2005 Atheros Communications Inc.
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
@file: sample_os.c

@abstract: Linux implementation module for the SDIO Sample Function driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 7;
#include "../../../include/ctsystem.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/dma.h>
#include <asm/page.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/dma-mapping.h>
#endif

#include "../../../include/sdio_busdriver.h"
#include "../sample.h"
#include "sample_app.h"

#define DESCRIPTION "SDIO Sample Function Driver"
#define AUTHOR "Atheros Communications, Inc."

/* module param defaults */
static int sdio_manfID = 0;
static int sdio_manfcode = 0;
static int sdio_funcno = 0;
static int sdio_class = 0;
static int clockoverride = 0;
static int sdio_major = SDIO_SAMPLE_FUNCTION_MAJOR;
int testb = 0;
int testbpb = 0;
int testusedma = 0;

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

module_param(clockoverride, int, 0644);
MODULE_PARM_DESC(clockoverride,"SDIO card clock override");

module_param(sdio_major, int, 0644);
MODULE_PARM_DESC(sdio_major,"The major number for driver.");

module_param(testb, int, 0644);
MODULE_PARM_DESC(testb,"Test block count");

module_param(testbpb, int, 0644);
MODULE_PARM_DESC(testbpb,"Test bytes per block");

module_param(testusedma, int, 0644);
MODULE_PARM_DESC(testusedma,"Use DMA when testing");

#define SETUP_TEST_ARGS(a,s) {               \
    (s).Register = (a).reg;                  \
    (s).pBuffer = &((a).argument);           \
    (s).BufferLength = sizeof((a).argument); \
    (s).TestIndex = (a).testindex;           \
}
    
#define SETUP_TEST_ARGS_2(a,s) {                \
    (s).Register = (a).reg;                     \
    (s).pBuffer = (a).argument;                 \
    (s).BufferLength = sizeof((a).argument);    \
    (s).TestIndex = (a).testindex;              \
}

/* device base name */
#define SDIO_SAMPLE_FUNCTION_BASE "sdiosam"
#define SDIO_SAMPLE_FUNCTION_MAX_DEVICES 2

BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void CleanupInstance(PSAMPLE_FUNCTION_CONTEXT  pFunctionContext,
                            PSAMPLE_FUNCTION_INSTANCE pInstance);
int sdio_function_open(struct inode *inode, struct file *filp);
int sdio_function_release(struct inode *inode, struct file *filp);
int sdio_function_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

/* devices we support, null terminated */
/* @TODO: change to your devices identification */
static SD_PNP_INFO Ids[] = {
    {.SDIO_ManufacturerID = 0x55AA,    /* specific SDIO card */
     .SDIO_ManufacturerCode = 0x2211, 
     .SDIO_FunctionNo = 1,
     .SDIO_FunctionClass = 1},
    /*  {.SDIO_FunctionClass = 4}, or by specific class */
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

/* char drivers functions */
static struct file_operations sdio_function_fops_dev = {
    .ioctl      = sdio_function_ioctl,
    .open       = sdio_function_open,
    .release    = sdio_function_release,
};

/* static buffers for data transfers and results */
static UINT8 verifyBuffer[BLOCK_BUFFER_BYTES * BLOCK_BUFFERS][SDIO_SAMPLE_FUNCTION_MAX_DEVICES];
static UINT8 dataBuffer[BLOCK_BUFFER_BYTES * BLOCK_BUFFERS][SDIO_SAMPLE_FUNCTION_MAX_DEVICES];


/*
 * Probe - a device potentially for us
 * 
 * notes: probe is called when the bus driver has located a card for us to support.
 *        We accept the new device by returning TRUE.
*/
BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PSAMPLE_FUNCTION_CONTEXT pFunctionContext = 
                                (PSAMPLE_FUNCTION_CONTEXT)pFunction->pContext;
	SYSTEM_STATUS err = 0;
	BOOL          accept;
    PSAMPLE_FUNCTION_INSTANCE pNewInstance = NULL;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Sample Function: Probe\n"));
    
    accept = FALSE;
            
        /* make sure this is a device we can handle 
         * the card must match the manufacturer and card ID */
    if ((pDevice->pId[0].SDIO_ManufacturerID == 
         pFunctionContext->Function.pIds[0].SDIO_ManufacturerID) &&
         (pDevice->pId[0].SDIO_ManufacturerCode == 
          pFunctionContext->Function.pIds[0].SDIO_ManufacturerCode)){         
         /* optionally check the function number if the card is SDIO 1.0 or 
          * the card does not have a CISTPL_MANF in it's function CIS
         if (pDevice->pId[0].SDIO_FunctionNo == 
                pFunctionContext->Function.pIds[0].SDIO_FunctionNo)) {
                 
         }
          */
          
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
         DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Probe - not our card (0x%X/0x%X/0x%X/0x%X)\n",
                            pDevice->pId[0].SDIO_ManufacturerID,
                            pDevice->pId[0].SDIO_ManufacturerCode,
                            pDevice->pId[0].SDIO_FunctionNo,                            
                            pDevice->pId[0].SDIO_FunctionClass));
        return FALSE; 
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Probe - card matched (0x%X/0x%X/0x%X/0x%X)\n",
                            pDevice->pId[0].SDIO_ManufacturerID,
                            pDevice->pId[0].SDIO_ManufacturerCode,
                            pDevice->pId[0].SDIO_FunctionNo,
                            pDevice->pId[0].SDIO_FunctionClass));
    accept = FALSE;
    do {
        /* create a new instance of a device and iniinitialize the device */
        pNewInstance = (PSAMPLE_FUNCTION_INSTANCE)KernelAlloc(sizeof(SAMPLE_FUNCTION_INSTANCE));
        if (NULL == pNewInstance) {
            break;    
        }
        ZERO_POBJECT(pNewInstance);
        
        if (!SDIO_SUCCESS(InitializeInstance(pFunctionContext,pNewInstance,pDevice))) {            
            break; 
        }      
            /* create a character device name */  
        snprintf(pNewInstance->Config.DeviceName, sizeof(pNewInstance->Config.DeviceName), 
                 SDIO_SAMPLE_FUNCTION_BASE"%d", 
                 pDevice->pHcd->SlotNumber);
        pNewInstance->Config.Major = sdio_major;    
            /* create a character device for the user mode to access this function */  
        err = register_chrdev(pNewInstance->Config.Major, 
                              pNewInstance->Config.DeviceName, 
                              &sdio_function_fops_dev);
        if (err < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Probe - could not register device, %d\n", err));
            pNewInstance->Config.CharRegistered = FALSE;
            break;            
        }
        pNewInstance->Config.CharRegistered = TRUE;     
        
        if (pNewInstance->Config.Major == 0) {
            pNewInstance->Config.Major =  err; /* save dynamically assigned major number for cleanup*/
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
    
    if (pInstance->Config.CharRegistered) {  
            /* unregister char driver */ 
        unregister_chrdev(pInstance->Config.Major, pInstance->Config.DeviceName);
        
    
    }
    
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    if (pInstance->Config.hBlockBuffer != 0) {
        dma_free_coherent(SD_GET_OS_DEVICE(pInstance->pDevice), 
                          pInstance->Config.BufferSize,
                          pInstance->Config.pBlockBuffer,
                          pInstance->Config.hBlockBuffer);
        pInstance->Config.hBlockBuffer = 0;
    }
#endif    
        
    DeleteSampleInstance(pFunctionContext, pInstance);     
    KernelFree(pInstance);
}


/* 
 * sdio_function_open - handle the open request
*/
int sdio_function_open(struct inode *inode, struct file *filp)
{
    unsigned int minor = MINOR(inode->i_rdev);
    //unsigned int major = MAJOR(inode->i_rdev);
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: sdio_function_open\n"));
        
    if (minor >= SDIO_SAMPLE_FUNCTION_MAX_DEVICES) {
        DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: sdio_function_open - minor type too large\n"));
        return -ENODEV;
    }
    
    /* find our device based on the minor number */
    filp->private_data = FindSampleInstanceByIndex(&FunctionContext, minor);
    if (filp->private_data == NULL) {
        return -ENODEV;
    }
    
    /* reference this module */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    try_module_get(THIS_MODULE);
#else
    /* 2.4 */
    try_inc_mod_count(THIS_MODULE);
#endif
    return 0;          /* success */
}

/* sdio_function_release - handle the close request
 * undo what we did in open()
*/
int sdio_function_release(struct inode *inode, struct file *filp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    module_put(THIS_MODULE);
#else
    /* 2.4 */
    __MOD_DEC_USE_COUNT(THIS_MODULE);
#endif
    return 0;
}
 
/*
    handle any ioctls
*/
int sdio_function_ioctl(struct inode *inode, struct file *filp,
                        unsigned int cmd, unsigned long arg) {
    PSAMPLE_FUNCTION_INSTANCE pDeviceInstance = (PSAMPLE_FUNCTION_INSTANCE)filp->private_data;
    SDIO_STATUS   status;
    SYSTEM_STATUS osStatus;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: sdio_function_ioctl\n"));
    
    osStatus = 0;
    
    if (pDeviceInstance->Config.Removing) { 
        return -EFAULT;  
    }
    
    /* example commands */
    switch(cmd) {
      /* CMD52 byte write*/
      case SDIO_IOCTL_SAMPLE_PUT_CMD: {
        struct sdio_sample_args args; 
        SDIO_TEST_ARGS sdioArgs;       
        /* get the user argument */
        if (copy_from_user(&args, (void *)arg, sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_PUT_CMD - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }

        SETUP_TEST_ARGS(args,sdioArgs);
        status = PutByte(pDeviceInstance, &sdioArgs);
        osStatus = SDIOErrorToOSError(status);
        break;
      }  
      /* CMD52 byte read */
      case SDIO_IOCTL_SAMPLE_GET_CMD: {
        struct sdio_sample_args args;        
        SDIO_TEST_ARGS sdioArgs;       
        /* get the user argument */
        if (copy_from_user(&args, (void *)arg, sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_CMD - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_CMD \n"));
        SETUP_TEST_ARGS(args,sdioArgs);
        status = GetByte(pDeviceInstance, &sdioArgs);
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_CMD register: %d, data %d, status: %d\n",
                                args.reg, args.argument, status));
        /* set response to user */
        if (copy_to_user((void *)arg, &args,  sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_CMD - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }
        osStatus = SDIOErrorToOSError(status);
        break;
      }
      /* CMD53 array write*/
      case SDIO_IOCTL_SAMPLE_PUT_BUFFER: { 
        struct sdio_sample_buffer args;               
        SDIO_TEST_ARGS sdioArgs;       
        /* get the user argument */
        if (copy_from_user(&args, (void *)arg, sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_PUT_BUFFER - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }
        SETUP_TEST_ARGS_2(args,sdioArgs);
        status = PutArray(pDeviceInstance, &sdioArgs);
        osStatus = SDIOErrorToOSError(status);
        break;
      }
      /* CMD53 array read */
      case SDIO_IOCTL_SAMPLE_GET_BUFFER: {
        struct sdio_sample_buffer args;
        SDIO_TEST_ARGS sdioArgs;   
        /* get the user argument */
        if (copy_from_user(&args, (void *)arg, sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_BUFFER - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }
        SETUP_TEST_ARGS_2(args,sdioArgs);
        status = GetArray(pDeviceInstance, &sdioArgs);
        /* set response to user */
        if (copy_to_user((void *)arg, &args,  sizeof(args)) != 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: SDIO_IOCTL_SAMPLE_GET_BUFFER - bad ioctl buffer\n"));
            osStatus = -EFAULT;
            break;
        }
        osStatus = SDIOErrorToOSError(status);
        break;
      }
    }
    
	return osStatus;
}
/*
 * SampleAllocateBuffers - allocate the transfer buffers
*/
SDIO_STATUS SampleAllocateBuffers(PSAMPLE_FUNCTION_INSTANCE pDeviceInstance)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
       
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        if (testusedma && (SDGET_DMA_DESCRIPTION(pDeviceInstance->pDevice) != NULL)) {
                /* allocate DMAable buffers */
            pDeviceInstance->Config.BufferSize = BLOCK_BUFFER_BYTES * BLOCK_BUFFERS;
                        
            pDeviceInstance->Config.pBlockBuffer  =  dma_alloc_coherent(SD_GET_OS_DEVICE(pDeviceInstance->pDevice), 
                                                            pDeviceInstance->Config.BufferSize , 
                                                            &pDeviceInstance->Config.hBlockBuffer, 
                                                            GFP_DMA);
            if (pDeviceInstance->Config.pBlockBuffer == NULL) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: SampleAllocateBuffers - unable to get DMA buffers\n"));
                status = SDIO_STATUS_NO_RESOURCES;
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: SampleAllocateBuffers - pDmaBuffer: 0x%X, hDmaBuffer: 0x%X\n",
                                (UINT)pDeviceInstance->Config.pBlockBuffer, (UINT)pDeviceInstance->Config.hBlockBuffer));
               
            }         
            break;
        }    
#endif   
       
        /* allocate buffers, we will not be using DMA */
        pDeviceInstance->Config.BufferSize = BLOCK_BUFFER_BYTES * BLOCK_BUFFERS;
        
        if (SDDEVICE_GET_SLOT_NUMBER(pDeviceInstance->pDevice) >= SDIO_SAMPLE_FUNCTION_MAX_DEVICES) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: SampleAllocateBuffers - can't allocate verify/data buffers\n"));
            status = SDIO_STATUS_NO_RESOURCES;
            break;    
        }
        /* we use the unique slot number as the device index, get a buffer for verifies */ 
        pDeviceInstance->Config.pBlockBuffer = &dataBuffer[0][SDDEVICE_GET_SLOT_NUMBER(pDeviceInstance->pDevice)];
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: SampleAllocateBuffers - non-DMA pBuffer: 0x%X \n",
                      (UINT)pDeviceInstance->Config.pBlockBuffer));

    } while (FALSE);
    
    if (SDIO_SUCCESS(status)) {   
        /* we use the unique slot number as the device index, get a buffer for verifies */
        if (SDDEVICE_GET_SLOT_NUMBER(pDeviceInstance->pDevice) >= SDIO_SAMPLE_FUNCTION_MAX_DEVICES) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: SampleAllocateBuffers - can't allocate verify buffer\n"));
            status = SDIO_STATUS_NO_RESOURCES;
        } else {
            pDeviceInstance->Config.pVerifyBuffer = &verifyBuffer[0][SDDEVICE_GET_SLOT_NUMBER(pDeviceInstance->pDevice)];
        }
    }    
    return status;
}

/*
 * SampleMakeSGlist - build scatter gather list
 *  input:
 *   pDeviceInstance - this device
 *   ByteCount - length of transfer in bytes
 *   Offset - byte offset into pBlockBuffer array
 *  output:
 *   pSGcount - number of SG entries
 *   return the scatter-gather list pointer
*/
PSDDMA_DESCRIPTOR SampleMakeSGlist(PSAMPLE_FUNCTION_INSTANCE pDeviceInstance, UINT ByteCount, UINT Offset, PUINT pSGcount)
{
    PSDDMA_DESCRIPTION pDmaDescrip = SDGET_DMA_DESCRIPTION(pDeviceInstance->pDevice);
    
    *pSGcount = 0;
    if (pDmaDescrip == NULL) {
        /* DMA not supported by this device */
        return NULL;
    }
    /* we have one scatter-gather list pre allocated, and just need to fill the data for our contiguous data buffer*/ 
    pDeviceInstance->Config.SGList.page = virt_to_page(pDeviceInstance->Config.pBlockBuffer + Offset);
    pDeviceInstance->Config.SGList.offset = pDeviceInstance->Config.hBlockBuffer + Offset - page_to_phys(pDeviceInstance->Config.SGList.page);
    pDeviceInstance->Config.SGList.length = ByteCount;
    
    /* now verify that the buffer meets the HCD's DMA requirements */
    /* since we are only using a single contiguous buffer, we support HCDs that support DMA or Scatter-Gather DMA,
       DMA is just scatter-gather DMA with only single scatter-gather entry */
    if (pDmaDescrip->Flags & (SDDMA_DESCRIPTION_FLAG_DMA | SDDMA_DESCRIPTION_FLAG_SGDMA)) {
        /* check we aren't exceeding the number of scatter-gather entries supported (not a needed check in our case of 1 sg entry */
        if (pDmaDescrip->MaxDescriptors >= 1) {
            /* check that the length of our descriptor is not too big,
               if it was and scatter-gather was supported by the HCD, we could break this transfer into more SG entries  */
            if (ByteCount <= pDmaDescrip->MaxBytesPerDescriptor) {
                /* check that the start address is properly aligned for the HCD */
                if (!((pDeviceInstance->Config.hBlockBuffer+Offset) & pDmaDescrip->AddressAlignment)) {
                    /* no unwanted address bits, DMA address is okay */    
                    /* check that the length is properly aligned */
                    if (!(ByteCount & pDmaDescrip->LengthAlignment)) {
                        /* no unwanted length bits, number of bytes is okay */
                        /* we have a supportable address. These checks could be done in other drivers at start up in some cases or 
                           partially the block file system */
                        *pSGcount = 1;                              
                        return &pDeviceInstance->Config.SGList;
                   } else {
                      DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Bytecount %d is not aligned (requires 0x%X)\n",
                               ByteCount,pDmaDescrip->LengthAlignment));  
                   }
                } else {
                    DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Address 0x%X is not aligned (requires 0x%X)\n",
                                pDeviceInstance->Config.hBlockBuffer+Offset,pDmaDescrip->AddressAlignment));        
                }
            } else {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: BytesCount %d exceeds per descriptor length %d \n",
                          ByteCount,pDmaDescrip->MaxBytesPerDescriptor));       
                
            }
        } else {
            DBG_ASSERT(FALSE);   
        }
    } 
    /* DMA not supported by this device */
   return NULL;
}
 
/*
 * module init
*/
static int __init sdio_function_init(void) {
    SDIO_STATUS status;
    REL_PRINT(SDDBG_TRACE, ("+SDIO Sample Function - load\n"));
   
    if (sdio_manfID != 0) {
        Ids[0].SDIO_ManufacturerID = (UINT16)sdio_manfID;  
        DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Override MANFID: 0x%x \n",sdio_manfID)); 
    }
    if (sdio_manfcode != 0) {
        Ids[0].SDIO_ManufacturerCode = (UINT16)sdio_manfcode; 
         DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Override MANFCODE: 0x%x \n",sdio_manfcode)); 
    }
    if (sdio_funcno != 0) {
        Ids[0].SDIO_FunctionNo =  (UINT8)sdio_funcno;
        DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Override FuncNo: 0x%x \n",sdio_funcno)); 
    }
    if (sdio_class != 0) {
        Ids[0].SDIO_FunctionClass = (UINT8)sdio_class;
        DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Override Class: 0x%x \n",sdio_class)); 
    }
    
    status = InitFunctionContext(&FunctionContext);
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);       
    }
    
    if (clockoverride != 0) {
        FunctionContext.ClockOverride = (SD_BUSCLOCK_RATE)clockoverride;
        DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Override Clock: %d \n",clockoverride)); 
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


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_function_init);
module_exit(sdio_function_cleanup);

