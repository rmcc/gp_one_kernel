/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: benchmark_ms.h

@abstract: Microsoft SDIO stack-specifc info

#notes: 
 
@notice: Copyright (c), 2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BENCHMARK_MSSTACK_H___
#define __SDIO_BENCHMARK_MSSTACK_H___

#define BLOCK_BUFFER_BYTES   BENCH_MARK_SUGGESTED_BUFFER_SIZE
#define SDIO_BENCHMARK_FUNCTION_MAX_DEVICES 4

typedef struct _BENCH_MARK_INSTANCE {
    HANDLE           hTestThread;
    BM_BUS_CHARS     BusChars;
    PWCHAR           pRegPath;
    SD_DEVICE_HANDLE hDevice;      
    UCHAR            FuncNo;     
    PUINT8           pTestBuffer;       /* I/O output buffer */
    UINT             BufferSize;        /* size of pBlockBuffer and pVerifyBuffer */    
    PUINT8           pCurrBuffer;
    UINT32           TestFlags;
    UINT32           CurrentCount;
    UINT32           CurrentAddress;
    HANDLE           hIOComplete;
    SD_API_STATUS    LastStatus;
    PSD_BUS_REQUEST  pCurrentRequest;
}BENCH_MARK_INSTANCE, *PBENCH_MARK_INSTANCE;

#endif
