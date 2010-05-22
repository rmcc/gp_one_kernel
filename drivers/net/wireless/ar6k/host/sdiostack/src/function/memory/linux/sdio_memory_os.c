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
@file: sdio_memory_os.c

@abstract: Linux implementation module for SDIO MMC and SD nenory cards driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 4;
#include "../../../include/ctsystem.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)   
#include <linux/blkpg.h>
#endif
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <asm/uaccess.h>

#include "../../../include/sdio_busdriver.h"
#include "../../../include/sdio_lib.h"
#include "../sdio_memory.h"
#include "sdio_memory_linux.h"

#define DESCRIPTION "SDIO MMC/SD memory card Driver"
#define AUTHOR "Atheros Communications, Inc."

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");


BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static SDIO_STATUS CreateDisk(PSDIO_MEMORY_CONTEXT pDriverContext, PSDIO_MEMORY_INSTANCE pInstance);
static void DiskRequest(request_queue_t *pQueue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
static void DiskRequestDma(request_queue_t *pQueue);
static char* ReplaceChar(char* pStr, char Find, char Replace);
static int CheckDeviceChange(struct gendisk *disk);
#else
/* 2.4 */
static int CheckDeviceChange(kdev_t kdev);
static int Revalidate(kdev_t i_rdev);
#endif
static int DeviceIoctl(struct inode *inode, struct file *filp,
                       unsigned int cmd, unsigned long arg);
static void DeleteDisk(PSDMEMORY_CONFIG pInstance);
static int Open(struct inode *inode, struct file *filp);
static int Release(struct inode *inode, struct file *filp);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)   
/* 2.4, only support 1 device */
#define MEM_SHIFT 3
static int temp_major;
#define MAJOR_NR        temp_major
#define DEVICE_NR(device) (MINOR(device) >> MEM_SHIFT)
#define DEVICE_NAME     SDIO_MEMORY_BASE
#define DEVICE_NO_RANDOM 
#define DEVICE_REQUEST  DiskRequest

#include <linux/blk.h>

//??static char GenDiskFlags = GENHD_FL_REMOVABLE;
static char GenDiskFlags = 0;
static char GenDiskMajorName[16];
static int GenSize[SDIO_MEMORY_MAX_PARTITIONS];
static int GenSize_size[SDIO_MEMORY_MAX_PARTITIONS];
static int GenHardsects[SDIO_MEMORY_MAX_PARTITIONS];
static int GenMaxsects[SDIO_MEMORY_MAX_PARTITIONS];
static devfs_handle_t devfs_handle;      
#endif


/* devices we support, null terminated */
static SD_PNP_INFO Ids[] = {
    //??{.SDMMC_ManfacturerID = 0x00,       /* specific MMC card  */
    //?? .SDMMC_OEMApplicationID = 0x0002}, 
    {.CardFlags = CARD_SD},           /* all SD cards */
    {.CardFlags = CARD_MMC},          /* all MMC cards */
    {}
};

/* driver wide context data */
SDIO_MEMORY_CONTEXT DriverContext = {
    .Driver.Major = SDIO_MEMORY_MAJOR,
    .Function.pName    = "sdio_mem",
    .Function.Version  = CT_SDIO_STACK_VERSION_CODE,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    .Function.MaxDevices = SDIO_MEMORY_MAX_DEVICES,
#else
    .Function.MaxDevices = 1,
#endif    
    .Function.NumDevices = 0,
    .Function.pIds     = Ids,
    .Function.pProbe   = Probe,
    .Function.pRemove  = Remove,
    .Function.pSuspend = NULL,
    .Function.pResume  = NULL,
    .Function.pWake    = NULL,
    .Function.pContext = &DriverContext, 
};

/* supported I/O operations*/
static struct block_device_operations driver_ops = {
    .owner       = THIS_MODULE,
    .open        = Open,
    .release     = Release,
    .ioctl       = DeviceIoctl,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    .media_changed = CheckDeviceChange,
#else
    /* 2.4 */
    .check_media_change = CheckDeviceChange,
    .revalidate  = Revalidate,
#endif    
};



/* per scatter-gather request */
typedef struct _REQUEST_CONTEXT {
    PSDIO_MEMORY_INSTANCE pInstance; /* this device */
    struct request * pOSRequest; /* the request we are working on */
    UINT             SectorsToTransfer; /* the number of sectors transferred */
    UINT             SGcount;    /* number of scatter-gather entries */
    SDDMA_DESCRIPTOR     SGlist[0];  /* variable length of scatter gather entries */
}REQUEST_CONTEXT, *PREQUEST_CONTEXT;

/*
 * Probe - a device potentially for us
*/
BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) 
{
    PSDIO_MEMORY_CONTEXT pDriverContext = 
                                (PSDIO_MEMORY_CONTEXT)pFunction->pContext;

    BOOL          accept = FALSE;
    PSDIO_MEMORY_INSTANCE pNewInstance = NULL;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - enter\n"));
    
    /* make sure we can handle this device type */
    if ((pDevice->pId[0].CardFlags & CARD_SD) && (pDevice->pId[0].SDIO_FunctionNo == 0)) {
            /* we check against a zero SDIO function number to make sure this is not an SDIO function
             * on a combo card which will also have CARD_SD set as well.  SDIO functions start 
             * with 1*/
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - SD Card Type Match (MANF:0x%X, OEMID:0x%X) \n",
                  pDevice->pId[0].SDMMC_ManfacturerID, pDevice->pId[0].SDMMC_OEMApplicationID));
    } else if (pDevice->pId[0].CardFlags & CARD_MMC) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - MMC Card Type Match (MANF:0x%X, OEMID:0x%X) \n",
                  pDevice->pId[0].SDMMC_ManfacturerID, pDevice->pId[0].SDMMC_OEMApplicationID));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - not ours \n"));
        return FALSE;
    }

    do {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - creating instance \n"));
        pNewInstance = CreateDeviceInstance(pDriverContext, pDevice);
        if (NULL == pNewInstance) {
            break;    
        }
        atomic_set(&pNewInstance->Config.OpenCount, 0);
        
           /* add it to the list */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - adding instance  \n"));
        AddDeviceInstance(pDriverContext, pNewInstance);
        
            /* get the card info */
        GetCardCSD(pDevice, pNewInstance, (pDevice->pId[0].CardFlags & CARD_MMC));
        
            /* create an OS disk for this device */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - creating disk \n"));
        if (!SDIO_SUCCESS(CreateDisk(pDriverContext, pNewInstance))) {
            /* failed */
            break;
        }    
        accept = TRUE;
        break;
    } while (FALSE); 
    
    if (!accept && (pNewInstance != NULL)) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Probe - deleting instance \n"));
        DeleteInstance(pDriverContext, pNewInstance);
    }
    
    return accept;
}

/*
 * Remove - our device is being removed
*/
void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice) 
{
    PSDIO_MEMORY_CONTEXT pDriverContext = 
                             (PSDIO_MEMORY_CONTEXT)pFunction->pContext;
    PSDIO_MEMORY_INSTANCE pInstance;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: Remove\n"));
   
   
    pInstance = FindInstance(pDriverContext, pDevice);
    
    if (pInstance != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function:: Removing instance: 0x%X From Remove() \n",
                                (INT)pInstance));
        DeleteDisk(&pInstance->Config);
        DeleteInstance(pDriverContext, pInstance);
    } else {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function:: could not find matching instance! \n"));
    }    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: Remove\n"));
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
/* 2.4 */
static struct gendisk* alloc_disk(int minor)
{
    struct gendisk* pDisk;
    int len; 
    
    pDisk = kmalloc(sizeof(struct gendisk), GFP_KERNEL);
    if (pDisk == NULL) {
        return NULL;
    }
    memset(pDisk, 0, sizeof(struct gendisk));
    len = (minor-1) * sizeof(struct hd_struct *);
    pDisk->part = kmalloc(len, GFP_KERNEL);
    if (pDisk->part == NULL) {
        kfree(pDisk);
        return NULL;
    }
    memset(pDisk->part, 0, len);
    pDisk->max_p = minor;
    pDisk->minor_shift = MEM_SHIFT;
    //?????partition inits?
    return pDisk;
}
static int add_disk(PSDIO_MEMORY_INSTANCE pInstance, struct gendisk *pDisk)
{
    struct hd_struct* pPartitions;
    /* create the partitions */
    pPartitions = kmalloc(SDIO_MEMORY_MAX_PARTITIONS * sizeof(struct hd_struct), GFP_KERNEL);
    if (pPartitions == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: add_disk, no memory\n"));
        return -ENOMEM;;
    }
    memset(pPartitions , 0, SDIO_MEMORY_MAX_PARTITIONS * sizeof(struct hd_struct));
    pPartitions[0].nr_sects = (GenSize[0] * 1024) / pInstance->FileSysBlockSize;
    pDisk->part = pPartitions;
    pDisk->nr_real = 1;  
      
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: register_disk, nr_sects: %d, GenSize[0]: %d \n", 
                            (UINT)pPartitions[0].nr_sects, (UINT)GenSize[0]));
    register_disk(pDisk, 0, SDIO_MEMORY_MAX_PARTITIONS,   
              pDisk->fops, pPartitions[0].nr_sects);
    return 0;
}
#endif
/*
 * CreateDisk - create the disk that represenst this device 
*/
static SDIO_STATUS CreateDisk(PSDIO_MEMORY_CONTEXT pDriverContext, PSDIO_MEMORY_INSTANCE pInstance) 
{
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: CreateDisk\n"));
    pInstance->Config.pGenDisk = alloc_disk(SDIO_MEMORY_MAX_PARTITIONS);
    if (pInstance->Config.pGenDisk == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: CreateDisk - cannot allocate disk\n"));
        return SDIO_STATUS_NO_RESOURCES;
    }
    spin_lock_init(&pInstance->Config.RequestLock);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    if ((SDGET_DMA_DESCRIPTION(pInstance->pDevice) != NULL) &&
        ((SDGET_DMA_DESCRIPTION(pInstance->pDevice)->Flags & SDDMA_DESCRIPTION_FLAG_DMA) || 
         (SDGET_DMA_DESCRIPTION(pInstance->pDevice)->Flags & SDDMA_DESCRIPTION_FLAG_SGDMA))){
        pInstance->Config.pRequestQueue = blk_init_queue(DiskRequestDma, &pInstance->Config.RequestLock);
    } else {
        pInstance->Config.pRequestQueue = blk_init_queue(DiskRequest, &pInstance->Config.RequestLock);
    }
#else
    /* 2.4 */
    pInstance->Config.pRequestQueue = BLK_DEFAULT_QUEUE(pDriverContext->Driver.Major);
    blk_init_queue(pInstance->Config.pRequestQueue, DiskRequest);
#endif    
    if (pInstance->Config.pRequestQueue == NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: CreateDisk - cannot allocate queue\n"));
        /* get rid of our reference from alloc_disk() */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
        put_disk(pInstance->Config.pGenDisk);
#endif        
        return SDIO_STATUS_NO_RESOURCES;
    }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    /* not sure if we need to set this, limit the number of blocks per transfer */
    blk_queue_max_sectors(pInstance->Config.pRequestQueue, 
                          pInstance->pDevice->pHcd->CardProperties.OperBlockCountLimit); 
#endif        
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: block size %d\n", pInstance->BlockSize));
    /* force the file system to use at least a 512 block, seems to break when smaller */
    pInstance->FileSysBlockSize = (pInstance->BlockSize < 512) ? 512 : pInstance->BlockSize;
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: setting block size %d\n", 
                            pInstance->FileSysBlockSize));
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    blk_queue_hardsect_size(pInstance->Config.pRequestQueue, pInstance->FileSysBlockSize);
    
    if (SDGET_DMA_DESCRIPTION(pInstance->pDevice) != NULL) {
        PSDDMA_DESCRIPTION pDmaDescrp = SDGET_DMA_DESCRIPTION(pInstance->pDevice);
        if ((pDmaDescrp->Flags & SDDMA_DESCRIPTION_FLAG_DMA) || 
            (pDmaDescrp->Flags & SDDMA_DESCRIPTION_FLAG_SGDMA)){
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: setting up DMA\n"));
            /* setup the DMA parameters */
            blk_queue_dma_alignment(pInstance->Config.pRequestQueue,
                            pDmaDescrp->AddressAlignment);
            blk_queue_bounce_limit(pInstance->Config.pRequestQueue,
                            pDmaDescrp->Mask);
            blk_queue_max_segment_size(pInstance->Config.pRequestQueue,
                            pDmaDescrp->MaxBytesPerDescriptor);
            /* setup the DMA scatter gather parameters */
            blk_queue_max_phys_segments(pInstance->Config.pRequestQueue,
                            pDmaDescrp->MaxDescriptors);
            blk_queue_max_hw_segments(pInstance->Config.pRequestQueue,
                            pDmaDescrp->MaxDescriptors);
        }
    }
    
    pInstance->Config.pGenDisk->major = pDriverContext->Driver.Major;
    /* minor number is incremented for each slot plus the max number of partitions on a disk */
    pInstance->Config.pGenDisk->first_minor = 
        SDIO_MEMORY_MAX_PARTITIONS * pInstance->pDevice->pHcd->SlotNumber;
    pInstance->Config.pGenDisk->fops = &driver_ops;
    pInstance->Config.pGenDisk->private_data = pInstance;
    pInstance->Config.pGenDisk->flags= GENHD_FL_REMOVABLE;
    sprintf(pInstance->Config.pGenDisk->disk_name, SDIO_MEMORY_BASE "%d", 
            pDriverContext->Function.NumDevices);
    snprintf(pInstance->Config.pGenDisk->devfs_name,sizeof(pInstance->Config.pGenDisk->devfs_name),
             "sdmem_%s",
            (SD_GET_OS_DEVICE(pInstance->pDevice))->bus_id);
    /* remove any colons */
    ReplaceChar(pInstance->Config.pGenDisk->devfs_name, ':', '_'); 
                
    pInstance->Config.pGenDisk->driverfs_dev = SD_GET_OS_DEVICE(pInstance->pDevice);
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: CreateDisk: devfs_name: %s, 0x%X\n",
                            pInstance->Config.pGenDisk->devfs_name,
                            (INT)pInstance->Config.pGenDisk->driverfs_dev));

    set_capacity(pInstance->Config.pGenDisk, pInstance->Size/pInstance->FileSysBlockSize * 1024);
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: CreateDisk: size %d (Size %d, FileSysBlockSize %d)\n",
                            pInstance->Size/pInstance->FileSysBlockSize * 1024,
                            (INT)pInstance->Size, (INT)pInstance->FileSysBlockSize));
    pInstance->Config.pGenDisk->queue = pInstance->Config.pRequestQueue ;
    /* is it read only ? */
    set_disk_ro(pInstance->Config.pGenDisk, pInstance->WriteProtected);
    add_disk(pInstance->Config.pGenDisk); 
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: CreateDisk major: %d, minors: %d, first_minor: %d\n",
                            pInstance->Config.pGenDisk->major, pInstance->Config.pGenDisk->minors,
                            pInstance->Config.pGenDisk->first_minor));
#else
    /* 2.4 */
    pInstance->Config.pGenDisk->major = pDriverContext->Driver.Major;
    pInstance->Config.pGenDisk->fops = &driver_ops;
    pInstance->Config.pGenDisk->flags= &GenDiskFlags;
    sprintf(GenDiskMajorName, SDIO_MEMORY_BASE "%d", 
            pDriverContext->Function.NumDevices);
    pInstance->Config.pGenDisk->major_name = GenDiskMajorName;                

    {
        int ii;
        for (ii = 0; ii < SDIO_MEMORY_MAX_PARTITIONS; ii++) {
//??            GenSize[ii] = pInstance->Size; /* * 1024, already in kbytes*/
            GenSize_size[ii] = pInstance->FileSysBlockSize;
            GenHardsects[ii] = pInstance->FileSysBlockSize;
            GenMaxsects[ii] = pInstance->pDevice->pHcd->CardProperties.OperBlockCountLimit;
        }
        memset(GenSize, 0, sizeof(GenSize));
        GenSize[0] = pInstance->Size; /* * 1024, already in kbytes*/
        pInstance->Config.pGenDisk->sizes = GenSize;
        blk_size[pDriverContext->Driver.Major] = pInstance->Config.pGenDisk->sizes;
        blksize_size[pDriverContext->Driver.Major] = GenSize_size;
        hardsect_size[pDriverContext->Driver.Major] = GenHardsects;
        max_sectors[pDriverContext->Driver.Major] = GenMaxsects;
    }
    read_ahead[pDriverContext->Driver.Major] = 2;
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: CreateDisk: size %d (Size %d, FileSysBlockSize %d) GenSize[0]: %d\n",
                            pInstance->Size/pInstance->FileSysBlockSize * 1024,
                            (INT)pInstance->Size, (INT)pInstance->FileSysBlockSize, GenSize[0]));
    /* is it read only ? */
//??    set_disk_ro(pInstance->Config.pGenDisk, pInstance->WriteProtected);
    add_disk(pInstance, pInstance->Config.pGenDisk); 
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: CreateDisk major: %d, max_p: %d\n",
                            pInstance->Config.pGenDisk->major, pInstance->Config.pGenDisk->max_p));
#endif        
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: CreateDisk\n"));
    return SDIO_STATUS_SUCCESS;
}

/*
 * DeleteDisk - remove the disk
*/
static void DeleteDisk(PSDMEMORY_CONFIG pConfig) 
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DeleteDisk\n"));
    del_gendisk(pConfig->pGenDisk);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
    pConfig->pGenDisk->queue = NULL; //???
    put_disk(pConfig->pGenDisk);
    blk_cleanup_queue(pConfig->pRequestQueue);
#else
    /* 2.4 */
    {
        int ii;
        int major = DriverContext.Driver.Major;
        /* flush the devices */
        for (ii = 0; ii < SDIO_MEMORY_MAX_PARTITIONS; ii++) {
            fsync_dev(MKDEV(major, ii)); 
        }

        devfs_register_partitions(pConfig->pGenDisk, 0, 1);
        if (devfs_handle) {
            devfs_unregister(devfs_handle);
        }
        devfs_handle = NULL;

        invalidate_device (MKDEV(DriverContext.Driver.Major,0), 1);

        blk_cleanup_queue(BLK_DEFAULT_QUEUE(major));
        read_ahead[major] = 0;
        blk_size[major] = NULL;
        if (pConfig->pGenDisk->part != NULL) {
            kfree(pConfig->pGenDisk->part);
        }
        blksize_size[major] = NULL;
        del_gendisk(pConfig->pGenDisk);    
    } 
#endif    
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)   
/* 2.4 */
static int Revalidate(kdev_t i_rdev)
{
    int index;
    int max_p;
    int ii;
    int start; 
    PSDIO_MEMORY_INSTANCE pInstance = GetFirstInstance(&DriverContext);

    index = DEVICE_NR(i_rdev);

    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: Revalidate: %d \n", index));

    max_p = pInstance->Config.pGenDisk->max_p;
    start = index << MEM_SHIFT;

    for (ii = max_p - 1 ; ii >= 0 ; ii--) {
        int item = start + ii;
        invalidate_device(MKDEV(DriverContext.Driver.Major, item),1);
        pInstance->Config.pGenDisk->part[item].start_sect = 0;
        pInstance->Config.pGenDisk->part[item].nr_sects   = 0;
    }

    register_disk(pInstance->Config.pGenDisk, i_rdev, 1 << MEM_SHIFT, pInstance->Config.pGenDisk->fops,
                 (pInstance->Size * 1024) / pInstance->FileSysBlockSize);
    return 0;
}
#endif

/*
 * Open - open the device
*/
static int Open(struct inode *inode, struct file *filp)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    PSDIO_MEMORY_INSTANCE pInstance = (PSDIO_MEMORY_INSTANCE)inode->i_bdev->bd_disk->private_data;
#else
    /* 2.4 */
    PSDIO_MEMORY_INSTANCE pInstance = GetFirstInstance(&DriverContext);
#endif    
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: Open\n"));
    atomic_inc(&pInstance->Config.OpenCount);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    check_disk_change(inode->i_bdev);
#else
    check_disk_change(inode->i_rdev);
    /* 2.4 */
#endif    
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: Open\n"));
    return 0;
}

/*
 * Release - handle close 
*/
static int Release(struct inode *inode, struct file *filp)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    PSDIO_MEMORY_INSTANCE pInstance = (PSDIO_MEMORY_INSTANCE)inode->i_bdev->bd_disk->private_data;
#else
    /* 2.4 */
    PSDIO_MEMORY_INSTANCE pInstance = GetFirstInstance(&DriverContext);
#endif    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: Release\n"));
    atomic_dec(&pInstance->Config.OpenCount);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: Release\n"));
    return 0;
}

/*
 * DiskRequest - process a user request
*/
static void DiskRequest(request_queue_t *pQueue)
{
    struct request *pRequest;
    PSDIO_MEMORY_INSTANCE pInstance; 

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: DiskRequest\n"));
    
    /* for each request in queue */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
    while ((pRequest = elv_next_request(pQueue)) != NULL) {
         if (!blk_fs_request(pRequest)) {
            /* not a command we care about */
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequest - unsupported command: flags 0x%X\n",
                                    (UINT)pRequest->flags));
            end_request(pRequest, 0);
            continue;
        }
        pInstance = (PSDIO_MEMORY_INSTANCE)pRequest->rq_disk->private_data;
        /* we don't need the queue spinlock while processing the head of the queue */
        spin_unlock_irq(&pInstance->Config.RequestLock);
        if (SDIO_SUCCESS(MemoryTransfer(pInstance, 
                                        pRequest->sector, pRequest->current_nr_sectors, 
                                        pRequest->buffer, rq_data_dir(pRequest)))) {
#else
    /* 2.4 */
#define rq_data_dir(r) ((r)->cmd == WRITE)
    spin_unlock_irq(&io_request_lock);
    pInstance = GetFirstInstance(&DriverContext);
    spin_lock_irq(&pInstance->Config.RequestLock);
    while(TRUE) {
        INIT_REQUEST;
        pRequest = CURRENT;
        /* we don't need the queue spinlock while processing the head of the queue */
        spin_unlock_irq(&io_request_lock);
        DBG_PRINT(SDDBG_TRACE, 
            ("SDIO Memory Function: DiskRequest -sector:%d, nr_sectors: %d, hard_sector: %d, hard_nr_sectors: %d, \n nr_segments: %d, nr_hw_segments: %d, current_nr_sectors: %d, hard_cur_sectors: %d, start_sect: %d, minor: %d\n",
            (UINT)pRequest->sector, (UINT)pRequest->nr_sectors, (UINT)pRequest->hard_sector,
            (UINT)pRequest->hard_nr_sectors, (UINT)pRequest->nr_segments, (UINT)pRequest->nr_hw_segments, (UINT)pRequest->current_nr_sectors, (UINT)pRequest->hard_cur_sectors,
            (UINT)pInstance->Config.pGenDisk->part[MINOR(pRequest->rq_dev)].start_sect, MINOR(pRequest->rq_dev)));
        if (SDIO_SUCCESS(MemoryTransfer(pInstance, 
                                        pRequest->sector+pInstance->Config.pGenDisk->part[MINOR(pRequest->rq_dev)].start_sect,
                                        pRequest->current_nr_sectors, 
                                        pRequest->buffer, rq_data_dir(pRequest)))) {
#endif    
        
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
            spin_lock_irq(&pInstance->Config.RequestLock);
            end_request(pRequest, 1);
#else
    /* 2.4 */
            spin_lock_irq(&io_request_lock);
            end_request(1);
#endif            
        } else {
            DBG_PRINT(SDDBG_WARN, ("SDIO Memory Function: DiskRequest - failing request\n"));
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)   
            spin_lock_irq(&pInstance->Config.RequestLock);
            end_request(pRequest, 0);
#else
    /* 2.4 */
            spin_lock_irq(&io_request_lock);
            end_request(0);
#endif            
        }
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: DiskRequest\n"));
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
/*
 * AsyncCompletion - asynch I/O completion routine
*/
void AsyncCompletion(struct _SDREQUEST *pRequest)
{
    PREQUEST_CONTEXT pRegContext = (PREQUEST_CONTEXT)pRequest->pCompleteContext;
    PSDIO_MEMORY_INSTANCE pInstance = pRegContext->pInstance;
    struct request * pOSRequest = pRegContext->pOSRequest;
    UINT    sectorsTransferred = pRegContext->SectorsToTransfer;
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: AsyncCompletion: status: %d\n",
                            pRequest->Status));
    if (DBG_GET_DEBUG_LEVEL() >= SDDBG_DUMP) {
        SDLIB_PrintBuffer(pOSRequest->buffer, 
             sectorsTransferred* pInstance->BlockSize,
            "SDIO Memory Function: AsyncCompletion");
    }
    KernelFree(pRegContext);
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);
    
       /* now deal with the disk request */    
    spin_lock_irq(&pInstance->Config.RequestLock);  
    
    do {
        if (SDIO_SUCCESS(pRequest->Status)) {
            if (end_that_request_first(pOSRequest, 1, sectorsTransferred)) {
                    /* more to do */
                DiskRequestDma(pInstance->Config.pRequestQueue);
                break;        
            } 
        } else {
            if (end_that_request_first(pOSRequest, 0, sectorsTransferred)) {
                    /* more to do */
                DiskRequestDma(pInstance->Config.pRequestQueue);
                break;        
            }    
        }
        blkdev_dequeue_request(pOSRequest);
        end_that_request_last(pOSRequest);
    } while (FALSE);
    
    spin_unlock_irq(&pInstance->Config.RequestLock); 
}

/*
 * AsyncCompletionLast - asynch I/O completion routine, handles last of series of requests
*/
void AsyncCompletionLast(struct _SDREQUEST *pRequest)
{
    PREQUEST_CONTEXT pRegContext = (PREQUEST_CONTEXT)pRequest->pCompleteContext;
    PSDIO_MEMORY_INSTANCE pInstance = pRegContext->pInstance;
    struct request * pOSRequest = pRegContext->pOSRequest;
    UINT    sectorsTransferred = pRegContext->SectorsToTransfer;
     
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: AsyncCompletionLast: status: %d\n",
                            pRequest->Status));
    if (DBG_GET_DEBUG_LEVEL() >= SDDBG_DUMP) {
        SDLIB_PrintBuffer(pOSRequest->buffer, 
            sectorsTransferred * pInstance->BlockSize,
            "SDIO Memory Function: AsyncCompletion");
    }
    KernelFree(pRegContext);
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);
    
        /* now deal with the disk request */    
    spin_lock_irq(&pInstance->Config.RequestLock);  
        
    do {      
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function:  completed sectors :%d nr_sectors: %d \n",
                            (UINT)sectorsTransferred, (UINT)pOSRequest->nr_sectors)); 
        
        if (SDIO_SUCCESS(pRequest->Status)) {
            if (end_that_request_first(pOSRequest, 1, sectorsTransferred)) {
                DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: end_that_request_first not done!\n"));
                    /* more to do */
                DiskRequestDma(pInstance->Config.pRequestQueue);
                break;      
            } 
        } else {
            if (end_that_request_first(pOSRequest, 0, sectorsTransferred)) {
                    /* more to do */
                DiskRequestDma(pInstance->Config.pRequestQueue);
                break;             
            }    
        } 
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: current disk request done!\n"));
              
        blkdev_dequeue_request(pOSRequest);
        end_that_request_last(pOSRequest);
        
            /* look for more requests */
        DiskRequestDma(pInstance->Config.pRequestQueue);
         
    } while (FALSE);
    
    spin_unlock_irq(&pInstance->Config.RequestLock); 
        
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: AsyncCompletionLast\n"));
}

/*
 * DiskRequest - process a user request via DMA
*/
static void DiskRequestDma(request_queue_t *pQueue)
{
    struct request *pRequest;
    PSDIO_MEMORY_INSTANCE pInstance; 
    UINT MaxDescriptors;
    SDIO_STATUS status;
    INT  outstandingReq = 1; /* for now, just queue one at a time  */
    UINT32 checkLength;
    PREQUEST_CONTEXT pContext;

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: DiskRequestDma\n"));
    
    /* for each request in queue */
    while ((outstandingReq-- > 0) && ((pRequest = elv_next_request(pQueue)) != NULL)) {
         DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma : processing block request :0x%X\n",(UINT32)pRequest));
        if (!blk_fs_request(pRequest)) {
            /* not a command we care about */
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - unsupported command: flags 0x%X\n",
                                    (UINT)pRequest->flags));
            end_request(pRequest, 0);
                /* reset */
            outstandingReq = 1;
            continue;
        }          
        
        pInstance = (PSDIO_MEMORY_INSTANCE)pRequest->rq_disk->private_data;
            /* we don't need the queue spinlock while processing the head of the queue */
        spin_unlock_irq(&pInstance->Config.RequestLock);
        
            /* allocate a context for ASYNC requests */
        pContext = KernelAlloc(sizeof(REQUEST_CONTEXT) + ((sizeof(SDDMA_DESCRIPTOR))*pRequest->nr_phys_segments));
        
        if (pContext == NULL) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: DiskRequestDma - no memory, failing request\n"));
            spin_lock_irq(&pInstance->Config.RequestLock);
            end_request(pRequest, 0);
                /* exit */
            break;
        }
        
        pContext->pOSRequest = pRequest;
        pContext->pInstance = pInstance;
                
        DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - Phys Segments %d, HW Segments:%d, current_nr_sectors:%d, nr_sectors:%d , hard_nr_sectors:%d \n",
                        pRequest->nr_phys_segments, pRequest->nr_hw_segments, pRequest->current_nr_sectors, (UINT32)pRequest->nr_sectors, (UINT32)pRequest->hard_nr_sectors));      
        
        if (pInstance->FileSysBlockSize != pInstance->BlockSize) {
            /* incompatible block size, use PIO mode, asynchronously */
DO_DIO:               
            pContext->SectorsToTransfer = pRequest->current_nr_sectors;
            status = IssueAsyncTransfer(pInstance->pDevice,
                                        pInstance,
                                        pRequest->sector * pInstance->FileSysBlockSize,                                             
                                        pContext->SectorsToTransfer*pInstance->FileSysBlockSize, 
                                        rq_data_dir(pRequest),
                                        pRequest->buffer,
                                        0,
                                        (outstandingReq > 0) ? AsyncCompletion : AsyncCompletionLast, 
                                        pContext);

        } else {
                /* try DMA */
            int ii;
            UINT32 align = SDGET_DMA_DESCRIPTION(pInstance->pDevice)->AddressAlignment;
            UINT32 lenAlign = SDGET_DMA_DESCRIPTION(pInstance->pDevice)->LengthAlignment;
                                          
                /* process as a DMA */            
            if (!((SDGET_DMA_DESCRIPTION(pInstance->pDevice)->Flags) & SDDMA_DESCRIPTION_FLAG_SGDMA)) {
                /* single block DMA */
                DBG_ASSERT_WITH_MSG((pRequest->nr_phys_segments == 1), "SDIO Memory Function: DiskRequestDma, invalid SG size")
            }
                /* transfer all sectors */
            pContext->SectorsToTransfer = pRequest->nr_sectors;
                        
                /* get the scatter gather mapping */
            MaxDescriptors =  blk_rq_map_sg(pQueue, pRequest, pContext->SGlist);
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - SG entries: %d, sector start: %d, seccnt: %d\n",
                                   MaxDescriptors, (UINT)pRequest->sector, pContext->SectorsToTransfer));
         
            DBG_ASSERT_WITH_MSG((MaxDescriptors > 0), "SDIO Memory Function: DiskRequestDma, zero descriptors in request")
      
            checkLength = 0;    
                /* check DMA restrictions */
            for (ii = 0; ii < MaxDescriptors; ii++) {
                DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - SG Index:%d, page: 0x%X, length: %d, offset: 0x%X\n",
                                   ii, (UINT)pContext->SGlist[ii].page, pContext->SGlist[ii].length, pContext->SGlist[ii].offset));
                
                    /* check address alignment */
                if (pContext->SGlist[ii].offset & align) {
                    /* we have some illegal bits here, not a supportable address boundary, go PIO*/
                    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - punting to direct IO, offset: 0x%X, alignment: 0x%X\n",
                                   pContext->SGlist[ii].offset, align));
                    goto DO_DIO;
                }
                    /* check length alignement */
                if (pContext->SGlist[ii].length & lenAlign) {
                        /* we have some illegal bits here, not a supportable length go PIO*/
                    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - punting to direct IO, Length: 0x%X (%d bytes), Length Alignment: 0x%X\n",
                                   pContext->SGlist[ii].length, pContext->SGlist[ii].length, lenAlign));
                    goto DO_DIO;
                }
                    /* we are all good here, add the length */
                checkLength += pContext->SGlist[ii].length;
            }
                     
            if (checkLength != pContext->SectorsToTransfer*pInstance->FileSysBlockSize) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: DiskRequestDma - SG Data length and Sector Mismatch! SG Total Length %d, Sectors:%d, bytespersector:%d\n",
                                   checkLength, pContext->SectorsToTransfer, pInstance->FileSysBlockSize));     
                goto DO_DIO;
            }       
            
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DiskRequestDma - submitting %s using SG entries: %d,len: %d\n",
                                   rq_data_dir(pRequest) ? "Write":"Read", MaxDescriptors, (UINT)(pContext->SectorsToTransfer * pInstance->BlockSize)));
    
                /* issue async transfer */
            status = IssueAsyncTransfer(pInstance->pDevice,
                                        pInstance,
                                        pRequest->sector * pInstance->FileSysBlockSize,                                             
                                        pContext->SectorsToTransfer*pInstance->FileSysBlockSize, 
                                        rq_data_dir(pRequest),
                                        pContext->SGlist,
                                        MaxDescriptors,
                                        (outstandingReq > 0)?AsyncCompletion : AsyncCompletionLast, pContext);        
        }
            /* reacquire the lock, the lock is held on entry of this function */
        spin_lock_irq(&pInstance->Config.RequestLock);
    }
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: DiskRequestDma\n"));
}

/*
  ReplaceChar - replace all Find with Replace
*/
static char* ReplaceChar(char* pStr, char Find, char Replace) 
{
    int ii;
    for(ii = 0; pStr[ii] != 0; ii++) {
        if (pStr[ii] == Find) {
            pStr[ii] = Replace;
        }
    }
    return pStr;
}
#endif            

/*
 * DeviceIoctl - handle IOCTL requests
 */
static int DeviceIoctl(struct inode *inode, struct file *filp,
                       unsigned int cmd, unsigned long arg)
{
    struct hd_geometry geometry;
 

    DBG_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: DeviceIoctl - cmd: %d\n", cmd));
    switch(cmd) {
        case HDIO_GETGEO: {
            /* get device geometry request. Return something reasonable,
               our device doesn't care */
            geometry.heads = 4;
            geometry.sectors = 16;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
            geometry.start = get_start_sect(inode->i_bdev); 
            geometry.cylinders = get_capacity(inode->i_bdev->bd_disk) / (4 * 16);
            DBG_PRINT(SDDBG_TRACE, 
             ("SDIO Memory Function: DeviceIoctl - HDIO_GETGEO, size: %d heads: %d sectors: %d start: %d cylinders: %d\n",
                    (INT)get_capacity(inode->i_bdev->bd_disk),
                    (INT)geometry.heads,
                    (INT)geometry.sectors,
                    (INT)geometry.start,
                    (INT)geometry.cylinders));
#else
/* 2.4 */   {
                PSDIO_MEMORY_INSTANCE pInstance = GetFirstInstance(&DriverContext);
                geometry.start = 0; 
                geometry.cylinders = (pInstance->Size/pInstance->FileSysBlockSize * 1024) / (4 * 16);
                DBG_PRINT(SDDBG_TRACE, 
                 ("SDIO Memory Function: DeviceIoctl - HDIO_GETGEO, size: %d heads: %d sectors: %d start: %d cylinders: %d\n",
                        (INT)(pInstance->Size/pInstance->FileSysBlockSize * 1024),
                        (INT)geometry.heads,
                        (INT)geometry.sectors,
                        (INT)geometry.start,
                        (INT)geometry.cylinders));
            }
#endif
            if (copy_to_user((void *) arg, &geometry, sizeof(geometry))) {
                return -EFAULT;
            }
            return 0;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)   
/* 2.4 */
        case BLKGETSIZE: {
            PSDIO_MEMORY_INSTANCE pInstance = GetFirstInstance(&DriverContext);
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DeviceIoctl BLKGETSIZE\n"));
            if (!access_ok(VERIFY_WRITE, arg, sizeof(long))) {
                return -EFAULT;
            }
            return put_user(pInstance->Config.pGenDisk->part[MINOR(inode->i_rdev)].nr_sects, (long *)arg);
        }
        case BLKRRPART: /* re-read partition table */
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DeviceIoctl BLKRRPART\n"));
            if (!capable(CAP_SYS_ADMIN)) 
                return -EACCES;
            return Revalidate(inode->i_rdev);
        
        default:
            DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: DeviceIoctl default\n"));
            return blk_ioctl(inode->i_rdev, cmd, arg);
        
#endif        
    }

    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: DeviceIoctl - cmd: %d\n", cmd));
    return -ENOTTY; /* unknown command */
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int CheckDeviceChange(struct gendisk *disk)
#else
/* 2.4 */
static int CheckDeviceChange(kdev_t kdev)
#endif
{
    return 0;
}

/*
 * module init
*/
static int __init sdio_memory_init(void) {
    SDIO_STATUS status; 
    SYSTEM_STATUS err;
    
    REL_PRINT(SDDBG_TRACE, ("+SDIO Memory Function: enter sdio_memory_init\n"));

    SDLIST_INIT(&DriverContext.InstanceList); 
   
    status = SemaphoreInitialize(&DriverContext.InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);
    }
    /* register with the block core */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
    err = register_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE);
#else
    /* 2.4 */
    err = devfs_register_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE, &driver_ops);
    devfs_handle = devfs_mk_dir(NULL, SDIO_MEMORY_BASE, NULL);    
#endif    
    if (err <= 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: unable to register with block driver, %d\n",
                                (INT)err));
        return err;         
    } 
    if (DriverContext.Driver.Major == 0) {
        /* save the assigned major number if it was a dynanmic assignment */
        DriverContext.Driver.Major = err;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)   
        /* 2.4 */
        temp_major = DriverContext.Driver.Major;
#endif
    }  

    /* register with bus driver core */
    if (!SDIO_SUCCESS((status = SDIO_RegisterFunction(&DriverContext.Function)))) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Memory Function: failed to register with bus driver, %d\n",
                                status));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
        unregister_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE);
#else
        devfs_unregister_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE);
#endif        
        return SDIOErrorToOSError(status);
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Memory Function: sdio_memory_init, major: %d\n", 
                            DriverContext.Driver.Major));

    return 0;
}

/*
 * module cleanup
*/
static void __exit sdio_memory_cleanup(void) {
    
    REL_PRINT(SDDBG_TRACE, ("SDIO Memory Function: enter sdio_memory_cleanup\n"));
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: sdio_memory_cleanup unregistering sdio device\n"));
    SDIO_UnregisterFunction(&DriverContext.Function);
    /* unregister with the block driver core */
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: sdio_memory_cleanup unregistering block device, major: %d\n",
                            DriverContext.Driver.Major));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)   
        unregister_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE);
#else
        devfs_unregister_blkdev(DriverContext.Driver.Major, SDIO_MEMORY_BASE);
#endif        
    DBG_PRINT(SDDBG_TRACE, ("SDIO Memory Function: sdio_memory_cleanup unregistering bus device\n"));
}


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_memory_init);
module_exit(sdio_memory_cleanup);

