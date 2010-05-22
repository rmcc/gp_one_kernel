// Copyright (c) 2004-2006 Atheros Communications Inc.
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
@file: sample.h

@abstract: OS independent include SDIO Sample function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_SAMPLE_H___
#define __SDIO_SAMPLE_H___

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/sample_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#include "linux/sample_linux.h"
#endif /* LINUX */

#ifdef QNX
#include "nto/sample_qnx.h"
#endif /* QNX Neutrino */

#ifdef NUCLEUS_PLUS
/* Mentor Graphics Nucleus support */
#include "nucleus/sample_nucleus.h"
#endif /* NUCLEUS_PLUS */

#ifdef UNDER_CE
#include "wince/sample_wince.h"
#endif /* Windows CE*/

#define SDIO_SAMPLE_MAX_TEST_INDEX 15

typedef struct _SDIO_TEST_ARGS {
    UINT8   TestIndex;
    UINT32  Register;
    PUINT8  pBuffer;
    UINT32  BufferLength;  
}SDIO_TEST_ARGS, *PSDIO_TEST_ARGS;

typedef struct _SAMPLE_FUNCTION_INSTANCE {
    SDLIST         SDList;      /* link in the instance list */
    PSDDEVICE      pDevice;     /* bus driver's device we are supporting */
    SAMPLE_CONFIG  Config;      /* OS specific config  */
    OS_SEMAPHORE   IOComplete;  /* io complete semaphore */
    SDIO_STATUS    LastRequestStatus;  /* last request status */
}SAMPLE_FUNCTION_INSTANCE, *PSAMPLE_FUNCTION_INSTANCE;

typedef struct _SAMPLE_FUNCTION_CONTEXT {
    SDFUNCTION      Function;       /* function description for bus driver */ 
    OS_SEMAPHORE    InstanceSem;    /* instance lock */
    SDLIST          InstanceList;    /* list of instances */
    SD_BUSCLOCK_RATE ClockOverride;  /* clock rate override */
}SAMPLE_FUNCTION_CONTEXT, *PSAMPLE_FUNCTION_CONTEXT;

/* prototypes */
SDIO_STATUS InitializeInstance(PSAMPLE_FUNCTION_CONTEXT  pFuncContext,
                               PSAMPLE_FUNCTION_INSTANCE pInstance,
                               PSDDEVICE                 pDevice);
SDIO_STATUS PutByte(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs);
SDIO_STATUS GetByte(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs);
SDIO_STATUS PutArray(PSAMPLE_FUNCTION_INSTANCE pInstance,PSDIO_TEST_ARGS pArgs);
SDIO_STATUS GetArray(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs);
void DeleteSampleInstance(PSAMPLE_FUNCTION_CONTEXT   pFuncContext,
                          PSAMPLE_FUNCTION_INSTANCE  pInstance);
PSAMPLE_FUNCTION_INSTANCE FindSampleInstance(PSAMPLE_FUNCTION_CONTEXT pFuncContext,
                                             PSDDEVICE                pDevice);
PSAMPLE_FUNCTION_INSTANCE FindSampleInstanceByIndex(PSAMPLE_FUNCTION_CONTEXT pFuncContext,
                                                    UINT                     Index);
SDIO_STATUS AddSampleInstance(PSAMPLE_FUNCTION_CONTEXT   pFuncContext,
                              PSAMPLE_FUNCTION_INSTANCE  pInstance);
void CleanupFunctionContext(PSAMPLE_FUNCTION_CONTEXT pFuncContext);
SDIO_STATUS InitFunctionContext(PSAMPLE_FUNCTION_CONTEXT pFuncContext);
PSDDMA_DESCRIPTOR SampleMakeSGlist(PSAMPLE_FUNCTION_INSTANCE pDeviceInstance, UINT ByteCount, UINT Offset, PUINT pSGcount);
SDIO_STATUS SampleAllocateBuffers(PSAMPLE_FUNCTION_INSTANCE pDeviceInstance);

#endif /* __SDIO_SAMPLE_H___*/               
