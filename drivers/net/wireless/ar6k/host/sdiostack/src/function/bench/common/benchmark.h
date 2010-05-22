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

@abstract: Bench marking header file

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BENCHMARK_H___
#define __SDIO_BENCHMARK_H___

#if defined(LINUX) || defined(__linux__)
    /* timming information struct */
typedef struct _OS_TIMING_INFO {
    UINT32           JiffiesStart;     /* jiffy count if CPU does not support perf counters */ 
    UINT32           JiffiesEnd;       /* jiffy count if CPU does not support perf counters */ 
    cycles_t         CyclesStart;      /* cycle count if CPU supports perf counters */
    cycles_t         CyclesEnd;        /* cycle count if CPU supports perf counters */
}OS_TIMING_INFO, *POS_TIMING_INFO;
#endif /* LINUX */

#ifdef UNDER_CE
typedef struct _OS_TIMING_INFO {
    LARGE_INTEGER    TickStart;
    LARGE_INTEGER    TickEnd;
    DWORD            TickFreq;
}OS_TIMING_INFO, *POS_TIMING_INFO;
#endif /* Windows CE*/
      
typedef struct _BM_TEST_PARAMS {
    INT     Cmd52FixedAddress;
    PUINT8  pTestBuffer;    /* generic PIO mode test buffer */
    UINT32  BufferSize;     /* size of generic test buffer */
    PUINT8  pDMABuffer;     /* DMA buffer, if platform supports it */
    UINT32  DMABufferSize;  /* DMA buffer size, if platform supports it */
    BOOL    TETestCard;     /* the TE mars test card allows us to perform read/write validation tests*/
    INT     ErrorCount;
    INT     PassCount;
    INT     SkipCount;
    OS_TIMING_INFO OSTimingInfo;
}BM_TEST_PARAMS, *PBM_TEST_PARAMS;

#define INC_SKIP_COUNT(pParams) (pParams)->SkipCount++
#define INC_PASS_COUNT(pParams) (pParams)->PassCount++
#define INC_ERROR_COUNT(pParams) (pParams)->ErrorCount++

typedef enum _BM_BUS_MODE {
    BM_BUS_1_BIT = 0, 
    BM_BUS_4_BIT = 1, 
    BM_BUS_8_BIT,   
    BM_BUS_SPI_NO_CRC,
    BM_BUS_SPI_CRC,   
    BM_BUS_LAST
}BM_BUS_MODE, *PBM_BUS_MODE;

typedef enum _BM_CARD_TYPE {
    BM_CARD_SDIO = 0,
    BM_CARD_MMC =1,
    BM_CARD_SD,
    BM_CARD_LAST
}BM_CARD_TYPE, *PBM_CARD_TYPE;

typedef struct _BM_BUS_CHARS {
    UINT32       ClockRate;
    BM_BUS_MODE  BusMode;
    BM_CARD_TYPE CardType;
    UINT32       MaxBlocksPerTransfer;
    UINT32       MaxBytesPerBlock;
}BM_BUS_CHARS, *PBM_BUS_CHARS;

void BenchMarkTests(PVOID pContext, PBM_BUS_CHARS pBusChars); 


#define BM_FLAGS_CMD52_FIXED_ADDR    (1 << 0)
#define BM_FLAGS_CMD53_FIXED_ADDR    (1 << 1)
#define BM_FLAGS_CMD52_COMMON_SPACE  (1 << 2)
#define BM_FLAGS_WRITE               (1 << 3)
#define BM_FLAGS_ASYNC               (1 << 4)
#define BM_FLAGS_USE_SHORT_TRANSFER  (1 << 5)

/* functions implemented by the driver layer */
SDIO_STATUS Cmd52Transfer(PVOID      pContext,
                          UINT32     Address,
                          PUINT8     pBuffer,
                          UINT32     Count,
                          UINT32     Flags);
                          
SDIO_STATUS CMD53Transfer_ByteBasis(PVOID  pContext, 
                                    UINT32 Address,
                                    PUINT8 pBuffer,
                                    UINT32 Bytes,
                                    UINT32 BlockSizeLimit,
                                    UINT32 Flags);
                                    
SDIO_STATUS CMD53Transfer_BlockBasis(PVOID  pContext, 
                                     UINT32 Address,
                                     PUINT8 pBuffer,
                                     UINT32 Blocks,
                                     UINT32 BytesPerBlock,
                                     UINT32 Flags);
                                                                       
void    GetBenchMarkParameters(PVOID pContext, PBM_TEST_PARAMS pParams);

SDIO_STATUS SetCardBusMode(PVOID pContext,BM_BUS_MODE BusMode,PUINT32 pClock);
SDIO_STATUS SetCardBlockLength(PVOID pContext, BM_CARD_TYPE CardType, UINT16 Length);
SDIO_STATUS MemCardBlockTransfer(PVOID          pContext,
                                 UINT32         BlockAddress,
                                 UINT16         BlockCount,
                                 UINT16         BytesPerBlock,
                                 PUINT8         pBuffer,
                                 UINT32         Flags);
 
#define MEMCARD_TEST_BLOCK_ADDRESS   0x00
#define MEMCARD_TEST_BLOCK_SIZE      512
#define MEMCARD_TEST_ITERATIONS      16 

#define BENCH_MARK_SUGGESTED_BUFFER_SIZE 16*1024 

/***************** End of driver layer APIs ******************************/


/* helper APIs */

#define PERF_TEST_BYTE_MODE         0
#define PERF_TEST_BLOCK_MODE        (1 << 0)
#define PERF_TEST_USE_DMA           (1 << 1)
#define PERF_TEST_BYTE_LIMIT_SHORT  (1 << 2)
#define PERF_TEST_MIXED_MODE        (1 << 3)
#define PERF_TEST_SHORT_TRANSFER    (1 << 4)

typedef struct _CMD53_PERF_TEST_ENTRY {
    UINT32  TestFlags;
    UINT32  IOFlags;
    UINT32  TestArg;
    TEXT    *pDescription;
}CMD53_PERF_TEST_ENTRY, *PCMD53_PERF_TEST_ENTRY;

typedef struct _CMD53_VERIFY_TEST_ENTRY {
    UINT32  TestFlags;
    UINT32  TestArg;
}CMD53_VERIFY_TEST_ENTRY, *PCMD53_VERIFY_TEST_ENTRY;

void    BenchMarkStart(PBM_TEST_PARAMS pParams);
void    BenchMarkEnd(PBM_TEST_PARAMS pParams);
void    SetUpOSTimming(PBM_TEST_PARAMS pParams);

SDIO_STATUS CMD53ReadWriteVerify(PVOID  pContext, 
                                 PBM_BUS_CHARS   pBusChars,
                                 PBM_TEST_PARAMS pParams,
                                 UINT32 Address, 
                                 UINT16 BlockSize,
                                 PUINT8 pBuffer, 
                                 UINT32 Length,
                                 UINT32 TestFlags,
                                 PTEXT  pDesc);
                                 
SDIO_STATUS CMD53VerifyMixed(PVOID           pContext,
                             PBM_BUS_CHARS   pBusChars,
                             PBM_TEST_PARAMS pParams,
                             UINT32          Address,
                             PUINT8          pBuffer, 
                             UINT32          BufferSize,
                             UINT32          PacketLength,
                             UINT32          BlockModeBlockSize);

SDIO_STATUS Cmd53PerformanceTest(PVOID           pContext, 
                                 PBM_BUS_CHARS   pBusChars,
                                 PBM_TEST_PARAMS pParams,
                                 UINT32          Address,
                                 PUINT8          pBuffer,
                                 UINT32          Count,
                                 UINT32          PerfTestFlags,
                                 UINT32          Flags,
                                 PTEXT           pDescription);
                                                                                               
void PrintBusMode(PVOID           pContext, 
                  PBM_BUS_CHARS   pBusChars);
                                                   
void GetTimeStats(PBM_TEST_PARAMS pParams,
                  PTEXT  pDescription, 
                  PTEXT  pOpsDescription, 
                  UINT32 Ops);

void FillBuffer(PUINT8 pBuffer, INT Length);
BOOL VerifyBuffers(PUINT8 pDataBuffer, PUINT8 pVerifyBuffer, INT Length);
SDIO_STATUS SDMemPerfTest(PVOID           pContext,
                          PBM_TEST_PARAMS pParams,
                          UINT32          BlockAddress,
                          UINT16          Blocks,
                          UINT16          BlockSize,
                          PUINT8          pBuffer,
                          INT             Iterations,
                          PTEXT           pStatsDescrip,
                          BOOL            Read);  
                               
SDIO_STATUS BlockReadWriteVerify(PVOID  pContext, 
                                 UINT32 BlockAddress, 
                                 UINT32 Blocks,
                                 UINT16 BlockSize,
                                 PUINT8 pBuffer,
                                 PTEXT  pDesc);     
#ifdef UNDER_CE
#define TEST_THREAD_DEF_PRIORITY 200
#endif
#endif /* __BENCH_MARK_H___*/               
