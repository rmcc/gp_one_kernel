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
@file: sdio_memory_linux.h

@abstract: OS dependent include memory card function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_MEMORY_LINUX_H___
#define __SDIO_MEMORY_LINUX_H___



#define SDIO_MEMORY_BASE "sdmem"
#define SDIO_MEMORY_MAX_DEVICES 4
#define SDIO_MEMORY_MAJOR 0
//??#define SDIO_MEMORY_MAX_MINOR 4
#define SDIO_MEMORY_MAX_PARTITIONS 8
/* sector size is aribtary and matchs the Linux requests */
//??#define SDIO_MEMORY_SECTOR_SIZE 512

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
typedef sector_t SDSECTOR_SIZE;
#else
/* 2.4 */
typedef unsigned long SDSECTOR_SIZE;
#endif
/* driver configuration */
typedef struct _SDMEMORY_DRIVER_CONFIG {
    int     Major;              /* device's major number */
    struct device *pDevice;     /* the parent device we are on*/
}SDMEMORY_DRIVER_CONFIG, *PSDMEMORY_DRIVER_CONFIG;

/* per device configuration */
typedef struct _SDMEMORY_CONFIG {
    struct hci_dev *pHciDev;    /* the HCI device */
    struct gendisk *pGenDisk;   /* the disk definition */
    struct request_queue *pRequestQueue; /* request queue for disk requests*/
    spinlock_t RequestLock;     /* lock for the RequestQueue */
    spinlock_t DeviceLock;      /* lock for this device */
    atomic_t   OpenCount;       /* how many opens are current */
}SDMEMORY_CONFIG, *PSDMEMORY_CONFIG;


#endif /*__SDIO_MEMORY_LINUX_H___*/

