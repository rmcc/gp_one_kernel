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
@file: function.h

@abstract: OS independent include memory card function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_MEMORY_H___
#define __SDIO_MEMORY_H___

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/sdio_memory_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_memory_linux.h"
#endif /* LINUX */

#ifdef QNX
#include "nto/sdio_memory_qnx.h"
#endif /* QNX */

#ifdef INTEGRITY
#include "integrity/sdio_memory_integrity.h"
#endif /* INTEGRITY */

#ifdef NUCLEUS_PLUS
/* Mentor Graphics Nucleus support */
#include "nucleus/sdio_memory_nucleus.h"
#endif /* NUCLEUS_PLUS */

#define EXTENDED_DATA_SIZE 512
typedef struct _SDIO_MEMORY_INSTANCE {
    SDLIST         SDList;      /* link in the instance list */
    PSDDEVICE      pDevice;     /* bus driver's device we are supporting */
    SDMEMORY_CONFIG Config;     /* OS specific config  */
    UINT32         Size;        /* size of this card, in 1024 byte units*/
    UINT           FileSysBlockSize; /* block size used by file system */
    BOOL           WriteProtected; /* true if write protected */
    UINT           WriteBlockLength; /* length of writes */
    BOOL           PartialWritesAllowed; /* writes < WriteBlockLength OK */
    UINT           ReadBlockLength; /* length of reads */
    BOOL           PartialReadsAllowed; /* reads < ReadBlockLength OK */
    UINT           BlockSize;   /* read/write size based on above 4 parameters */
    UINT           MaxBlocksPerTransfer; /* maximum number of blocks per individual transfer */
    UINT8          ExtendedData[EXTENDED_DATA_SIZE]; /* store extended CSD and/or SD status block */
}SDIO_MEMORY_INSTANCE, *PSDIO_MEMORY_INSTANCE;

typedef struct _SDIO_MEMORY_CONTEXT {
    SDFUNCTION      Function;       /* function description for bus driver */ 
    SDMEMORY_DRIVER_CONFIG  Driver; /* OS specific driver wide configuration */
    OS_SEMAPHORE    InstanceSem;    /* instance lock */
    SDLIST          InstanceList;   /* list of instances */
}SDIO_MEMORY_CONTEXT, *PSDIO_MEMORY_CONTEXT;

#define SDDBG_DUMP (SDDBG_TRACE + 1)

void DeleteInstance(PSDIO_MEMORY_CONTEXT   pFuncContext,
                    PSDIO_MEMORY_INSTANCE  pInstance);
PSDIO_MEMORY_INSTANCE CreateDeviceInstance(PSDIO_MEMORY_CONTEXT pFuncContext,
                                           PSDDEVICE            pDevice);
PSDIO_MEMORY_INSTANCE GetFirstInstance(PSDIO_MEMORY_CONTEXT pFuncContext);
PSDIO_MEMORY_INSTANCE FindInstance(PSDIO_MEMORY_CONTEXT pFuncContext,
                                   PSDDEVICE            pDevice);
SDIO_STATUS AddDeviceInstance(PSDIO_MEMORY_CONTEXT  pFuncContext,
                              PSDIO_MEMORY_INSTANCE pInstance);
void CleanupFunctionContext(PSDIO_MEMORY_CONTEXT pFuncContext);
SDIO_STATUS InitFunctionContext(PSDIO_MEMORY_CONTEXT pFuncContext);
SDIO_STATUS MemoryTransfer(PSDIO_MEMORY_INSTANCE pInstance, SDSECTOR_SIZE sectorNumber, 
                           ULONG sectorCount, PUCHAR pBuffer, BOOL WriteDirection);
SDIO_STATUS GetCardCSD(PSDDEVICE pDevice, PSDIO_MEMORY_INSTANCE pInstance, BOOL IsMmcCardType);
SDIO_STATUS IssueAsyncTransfer(PSDDEVICE        pDevice,
                            PSDIO_MEMORY_INSTANCE pInstance,
                            UINT32           Address,
                            UINT32           Length,
                            BOOL             Write,
                            PVOID            pBufferOrSGList,
                            UINT             SGcount,
                            void (*pCompletion)(struct _SDREQUEST *pRequest),
                            PVOID            pContext);

#endif /* __SDIO_MEMORY_H___*/               
