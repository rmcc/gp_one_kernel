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
@file: benchmark.h

@abstract: Bench marking driver header file

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BENCHMARK_CT_H___
#define __SDIO_BENCHMARK_CT_H___

#if defined(LINUX) || defined(__linux__)
#include "linux/benchmark_linux.h"
#endif /* LINUX */

#ifdef UNDER_CE
#include "wince/benchmark_wince.h"
#endif /* Windows CE*/

typedef struct _BENCHMARK_FUNCTION_CONTEXT {
    SDFUNCTION      Function;
    OS_SEMAPHORE    InstanceSem;     /* instance lock */
    SDLIST          InstanceList;    /* list of instances */
}BENCHMARK_FUNCTION_CONTEXT, *PBENCHMARK_FUNCTION_CONTEXT;

typedef struct _BENCHMARK_FUNCTION_INSTANCE {
    SDLIST          SDList;
    PSDDEVICE       pDevice;
    BM_BUS_CHARS    BusChars;
    PUINT8          pCurrBuffer;
    UINT32          TestFlags;
    INT             CurrentCount;
    UINT32          CurrentAddress;
    OS_SIGNAL       IOComplete;
    SDIO_STATUS     LastStatus;
    BOOL            TETestCard;
    BOOL            DeferTestExecution;
    OS_FUNC_CONFIG  Config;      /* OS specific config  */
}BENCHMARK_FUNCTION_INSTANCE, *PBENCHMARK_FUNCTION_INSTANCE;

void CleanupFunctionContext(PBENCHMARK_FUNCTION_CONTEXT pFuncContext);
SDIO_STATUS InitFunctionContext(PBENCHMARK_FUNCTION_CONTEXT pFuncContext);
void DeleteInstance(PBENCHMARK_FUNCTION_CONTEXT   pFuncContext,
                          PBENCHMARK_FUNCTION_INSTANCE  pInstance);
  
SDIO_STATUS InitializeInstance(PBENCHMARK_FUNCTION_CONTEXT  pFuncContext,
                               PBENCHMARK_FUNCTION_INSTANCE pInstance,
                               PSDDEVICE                    pDevice);
                                                       
PBENCHMARK_FUNCTION_INSTANCE FindInstance(PBENCHMARK_FUNCTION_CONTEXT pFuncContext,
                                                PSDDEVICE                pDevice);
PBENCHMARK_FUNCTION_INSTANCE FindInstanceByIndex(PBENCHMARK_FUNCTION_CONTEXT pFuncContext,
                                                    UINT                     Index);
SDIO_STATUS AddInstance(PBENCHMARK_FUNCTION_CONTEXT   pFuncContext,
                              PBENCHMARK_FUNCTION_INSTANCE  pInstance);
                              
SDIO_STATUS AllocateBenchMarkBuffers(PBENCHMARK_FUNCTION_INSTANCE pDeviceInstance);

SDIO_STATUS CheckDMABuildSGlist(PBENCHMARK_FUNCTION_INSTANCE pInstance, 
                                PUINT8                       pBuffer,
                                UINT                         ByteCount, 
                                PUINT                        pSGcount,
                                PSDDMA_DESCRIPTOR            *ppDescrip);
                              
#endif /* __BENCH_MARK_H___*/               
