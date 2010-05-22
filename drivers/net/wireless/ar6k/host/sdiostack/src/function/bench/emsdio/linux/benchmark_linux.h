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
@file: benchmark_linux.h

@abstract: OS dependent include for SDIO benchmarking function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BENCHMARK_LINUX_H___
#define __SDIO_BENCHMARK_LINUX_H___

#include <asm/timex.h>

#define BLOCK_BUFFER_BYTES   64*1024 /* largest you can do on linux for allocating DMA buffers */
#define SDIO_BENCHMARK_FUNCTION_MAX_DEVICES 2
#define NUM_SCATTER_GATHER_ENTRIES   256


typedef struct _OS_FUNC_CONFIG {
    PUINT8           pTestBuffer;      /* I/O output buffer */
    UINT32           BufferSize;       /* size of pBlockBuffer and pVerifyBuffer */
    PUINT8           pDmaBuffer;       /* dma-able buffer */
    PUINT8           pDmaBufferEnd;    /* end of dma-able buffer */
    UINT32           DmaBufferSize;    /* dma buffer size */
    DMA_ADDRESS      DmaAddress;       /* buffer physical address */
    SDDMA_DESCRIPTOR SGList[NUM_SCATTER_GATHER_ENTRIES]; /* scatter-gather list */
}OS_FUNC_CONFIG, *POS_FUNC_CONFIG;

#endif /*__SDIO_BENCHMARK_LINUX_H___*/

