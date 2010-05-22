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
@file: benchsdio.c

@abstract: SDIO BenchMarking functions

#notes: OS independent portions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include "benchmark.h"

/* data write/read verify test table to run a gamut of block sizes and modes */
CMD53_VERIFY_TEST_ENTRY g_CMD53VerifyTestTable[] =
{
        /* the most common ones */
    {PERF_TEST_BLOCK_MODE, 8  },
    {PERF_TEST_BLOCK_MODE, 16  },
    {PERF_TEST_BLOCK_MODE, 32  },
    {PERF_TEST_BLOCK_MODE, 64  },
    {PERF_TEST_BLOCK_MODE, 128 },
    {PERF_TEST_BLOCK_MODE, 256 },
    {PERF_TEST_BLOCK_MODE, 512 },
    {PERF_TEST_BLOCK_MODE, 1024 },
    {PERF_TEST_BLOCK_MODE, 2048 },
        /* some byte mode tests */
    {PERF_TEST_BYTE_MODE, 1  },
    {PERF_TEST_BYTE_MODE, 4  },
    {PERF_TEST_BYTE_MODE | PERF_TEST_SHORT_TRANSFER, 4  },
    {PERF_TEST_BYTE_MODE | PERF_TEST_SHORT_TRANSFER, 7  },
    {PERF_TEST_BYTE_MODE, 16 },
    {PERF_TEST_BYTE_MODE, 33 },
    {PERF_TEST_BYTE_MODE, 88 },
    {PERF_TEST_BYTE_MODE, 95 },
    {PERF_TEST_BYTE_MODE, 233},
    {PERF_TEST_BYTE_MODE, 300},
    {PERF_TEST_BYTE_MODE, 512},
        /* some DMA tests */
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 8    },  
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 16   },  
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 32   },
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 64   },
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 128  },
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 512  },
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 1024 },
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 2048 },
};

#define NUM_CMD53_VERIFY_TESTS (sizeof(g_CMD53VerifyTestTable)) / (sizeof(CMD53_VERIFY_TEST_ENTRY))

    /* performance tests on some common data transfer modes */ 
CMD53_PERF_TEST_ENTRY g_CMD53PerfTestTable[] = 
{
    {PERF_TEST_BLOCK_MODE, 
     0,   
     0,          
     "BLOCK Basis, PIO Mode, READ-SYNC" },
     
    {PERF_TEST_BLOCK_MODE, 
     BM_FLAGS_WRITE,  
     0,
     "BLOCK Basis, PIO Mode, WRITE-SYNC"},
     
    {PERF_TEST_BYTE_MODE , 
     0,    
     0,
     "BYTE Basis, PIO Mode, READ-SYNC"},
     
    {PERF_TEST_BYTE_MODE , 
     BM_FLAGS_WRITE, 
     0, 
     "BYTE Basis, PIO Mode, WRITE-SYNC"},
     
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 
     0,             
     0,
     "BLOCK Basis, DMA Mode, READ-SYNC" },
     
    {PERF_TEST_BLOCK_MODE | PERF_TEST_USE_DMA, 
     BM_FLAGS_WRITE, 
     0, 
     "BLOCK Basis, DMA Mode, WRITE-SYNC"},
 
    {PERF_TEST_BYTE_MODE | PERF_TEST_BYTE_LIMIT_SHORT, 
     0,
     0, 
     "BYTE Basis, Short Transfer non-optimized, READ-SYNC"},
    
    {PERF_TEST_BYTE_MODE | PERF_TEST_BYTE_LIMIT_SHORT, 
     BM_FLAGS_WRITE,
     0,
     "BYTE Basis, Short Transfer non-optimized, WRITE-SYNC"},
    
    {PERF_TEST_BYTE_MODE | PERF_TEST_BYTE_LIMIT_SHORT, 
     BM_FLAGS_USE_SHORT_TRANSFER, 
     0,
     "BYTE Basis, Short Transfer optimized, READ-SYNC"},
    
    {PERF_TEST_BYTE_MODE | PERF_TEST_BYTE_LIMIT_SHORT, 
     BM_FLAGS_USE_SHORT_TRANSFER | BM_FLAGS_WRITE,
     0,
     "BYTE Basis, Short Transfer optimized, WRITE-SYNC"},

    {PERF_TEST_MIXED_MODE, 
     0,
     1518,
     "Mixed, Ethernet Frame, PIO ,READ-SYNC"},
    {PERF_TEST_MIXED_MODE, 
     BM_FLAGS_WRITE,
     1518,
     "Mixed, Ethernet Frame, PIO, WRITE-SYNC"},
    {PERF_TEST_MIXED_MODE | PERF_TEST_USE_DMA, 
     0,
     1518,
     "Mixed, Ethernet Frame, DMA ,READ-SYNC"},
    {PERF_TEST_MIXED_MODE | PERF_TEST_USE_DMA, 
     BM_FLAGS_WRITE,
     1518,
     "Mixed, Ethernet Frame, DMA, WRITE-SYNC"}, 
};
                                          
#define NUM_CMD53_PERF_TESTS (sizeof(g_CMD53PerfTestTable)) / (sizeof(CMD53_PERF_TEST_ENTRY))

SDIO_STATUS SDIOCmd52Transfer(PVOID           pContext, 
                              PBM_BUS_CHARS   pBusChars,
                              PBM_TEST_PARAMS pParams,
                              UINT32          Address,
                              PUINT8          pBuffer,
                              UINT32          Count,
                              UINT32          Flags,
                              PTEXT           pDescription)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
  
    REL_PRINT(SDDBG_TRACE, ("SDIO CMD52 Test (%s) Addr:0x%X, Count:%d\n",
                        pDescription, Address, Count));
    
    PrintBusMode(pContext,pBusChars);
    
    do {            
        
        BenchMarkStart(pParams);
        
        status = Cmd52Transfer(pContext,
                               Address,
                               pBuffer,
                               Count,
                               Flags);
                                
        BenchMarkEnd(pParams);
        
        if (!SDIO_SUCCESS(status)) {
            REL_PRINT(SDDBG_TRACE, 
                ("SDIO CMD52 Test (%s):  Failed: %d \n",
                  pDescription, status));
            
            break;    
        }
                  
        GetTimeStats(pParams,
                     pDescription,
                     "Ops",
                     Count);
    
    } while (FALSE);
  
    if (!SDIO_SUCCESS(status)) {
        INC_ERROR_COUNT(pParams);    
    } else {
        INC_PASS_COUNT(pParams);    
    }
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    return status;
    
}

SDIO_STATUS Cmd53PerformanceTest(PVOID           pContext, 
                                 PBM_BUS_CHARS   pBusChars,
                                 PBM_TEST_PARAMS pParams,
                                 UINT32          Address,
                                 PUINT8          pBuffer,
                                 UINT32          Count,
                                 UINT32          PerfTestFlags,
                                 UINT32          Flags,
                                 PTEXT           pDescription)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    INT iterations = 100;
    INT iterationCount;
    UINT32 byteLimit = 0;
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    REL_PRINT(SDDBG_TRACE, ("SDIO CMD53 Performance Test (%s) Addr:0x%X, Count:%d\n",
                        pDescription, Address, Count));
    
    PrintBusMode(pContext,pBusChars);
    
    do { 
                   
        if (PerfTestFlags & PERF_TEST_BLOCK_MODE) {
            status = SetCardBlockLength(pContext, 
                                        pBusChars->CardType, 
                                        pBusChars->MaxBytesPerBlock);
            
            if (!SDIO_SUCCESS(status)) {
                break;    
            }
            REL_PRINT(SDDBG_TRACE, ("      Blocks:%d, BytesPerBlock:%d\n",
                        Count / pBusChars->MaxBytesPerBlock, pBusChars->MaxBytesPerBlock));
        } else {
            if (PerfTestFlags & PERF_TEST_BYTE_LIMIT_SHORT) {
                byteLimit = 4;   
                iterations = 10; /* shorten up iterations so the test finishes in a reasonable time */ 
            } else {
                byteLimit = 512;    
            }
            
            byteLimit = min(byteLimit, pBusChars->MaxBytesPerBlock);
            
            REL_PRINT(SDDBG_TRACE, ("      Blocks:%d, BytesPerBlock:%d\n",
                        Count / byteLimit, byteLimit)); 
        }
        
        BenchMarkStart(pParams);
        
        for (iterationCount = 0; iterationCount < iterations; iterationCount++) {                       
                
            if (PerfTestFlags & PERF_TEST_BLOCK_MODE) {
                status = CMD53Transfer_BlockBasis(pContext, 
                                                  Address,
                                                  pBuffer,
                                                  Count / pBusChars->MaxBytesPerBlock,
                                                  pBusChars->MaxBytesPerBlock,
                                                  Flags);
                
            } else {
                
                status = CMD53Transfer_ByteBasis(pContext, 
                                                 Address,
                                                 pBuffer,
                                                 Count,
                                                 byteLimit,
                                                 Flags);
            }
                                   
            
            if (!SDIO_SUCCESS(status)) {
                REL_PRINT(SDDBG_TRACE, 
                    ("SDIO CMD53 Test (%s):  Failed: %d (current iterations:%d)\n",
                      pDescription, status,iterationCount));
                break;    
            }    
             
        }     
        
        BenchMarkEnd(pParams);
        
        if (SDIO_SUCCESS(status)) {
            GetTimeStats(pParams,
                         pDescription,
                         "Bytes",
                         Count * 100);
        }
    
    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) { 
        if (SDIO_STATUS_UNSUPPORTED == status) {
                /* the test might have hit a DMA alignment issue, continue with other
                 * tests but print that the test was skipped */
            REL_PRINT(SDDBG_TRACE, 
                ("SDIO CMD53 Performance Test (%s) Addr:0x%X, Count:%d was SKIPPED\n",
                            pDescription, Address, Count));
            INC_SKIP_COUNT(pParams);
            status = SDIO_STATUS_SUCCESS;  
        } else {
            INC_ERROR_COUNT(pParams); 
        }
    } else {
        INC_PASS_COUNT(pParams);     
    }
         
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
   
    return status;
    
}

    /*** SDIO CMD53 read/write verify ****/
SDIO_STATUS CMD53ReadWriteVerify(PVOID           pContext,
                                 PBM_BUS_CHARS   pBusChars,
                                 PBM_TEST_PARAMS pParams,
                                 UINT32 Address, 
                                 UINT16 BlockSize,
                                 PUINT8 pBuffer, 
                                 UINT32 Length,
                                 UINT32 TestFlags,
                                 PTEXT  pDesc)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PUINT8      pDataBuffer;
    PUINT8      pVerifyBuffer;
    UINT32      verifySize;
    UINT32      verifyBlocks = 0;
    UINT32      flags;
    
    pDataBuffer = pBuffer;
        /* divide the buffer in two */
    verifySize = Length / 2;
    pVerifyBuffer = &pBuffer[verifySize];
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
 
    if (TestFlags & PERF_TEST_BLOCK_MODE) {
            /* round down verify size to whole blocks */
        verifyBlocks = verifySize / BlockSize;
        verifySize = verifyBlocks * BlockSize;
    }
                          
    if (TestFlags & PERF_TEST_BLOCK_MODE) {
        REL_PRINT(SDDBG_TRACE,("SDIO CMD53 Data Verify (%s) Block Mode, Block Size:%d, Blocks:%d Total:%d bytes\n",
                       pDesc, BlockSize,verifyBlocks,verifySize));
    } else {
        REL_PRINT(SDDBG_TRACE,
            ("SDIO CMD53 Data Verify (%s) Byte Mode (%s), Total:%d bytes, BlockSizeLimit:%d (blocks:%d,residue:%d)\n",
                       pDesc, (TestFlags & PERF_TEST_SHORT_TRANSFER) ? "Short Transfer":"default", 
                       verifySize,BlockSize,verifySize/BlockSize,verifySize%BlockSize));
    }
    
    PrintBusMode(pContext,pBusChars);
                                             
    do {  
        
        if (TestFlags & PERF_TEST_BLOCK_MODE) {
            status = SetCardBlockLength(pContext, 
                                        BM_CARD_SDIO, 
                                        BlockSize);
            
            if (!SDIO_SUCCESS(status)) {
                break;   
            }
        }
        
        FillBuffer(pDataBuffer,verifySize);
        
            /* keep a copy in the second half buffer */
        memcpy(pVerifyBuffer,pDataBuffer,verifySize);
        
        flags = BM_FLAGS_WRITE;
        
        BenchMarkStart(pParams);
        
            /* write the pattern */            
        if (TestFlags & PERF_TEST_BLOCK_MODE) {
            status = CMD53Transfer_BlockBasis(pContext, 
                                              Address,
                                              pDataBuffer,
                                              verifyBlocks,
                                              BlockSize,
                                              flags);
        } else {
            if (TestFlags & PERF_TEST_SHORT_TRANSFER) {
                flags |= BM_FLAGS_USE_SHORT_TRANSFER;        
            }
            status =  CMD53Transfer_ByteBasis(pContext, 
                                              Address,
                                              pDataBuffer,
                                              verifySize,
                                              BlockSize, // block size hint
                                              flags); 
             
        }
          
        if (!SDIO_SUCCESS(status)) {
            break;        
        } 
        
        BenchMarkEnd(pParams);
        
        GetTimeStats(pParams,
                     "Write",
                     "Bytes",
                     verifySize);
        
                             
        memset(pDataBuffer,0,verifySize);
        
        flags = 0;
        
        BenchMarkStart(pParams);
        
        if (TestFlags & PERF_TEST_BLOCK_MODE) {
            status = CMD53Transfer_BlockBasis(pContext, 
                                              Address,
                                              pDataBuffer,
                                              verifyBlocks,
                                              BlockSize,
                                              flags);
        } else {
            if (TestFlags & PERF_TEST_SHORT_TRANSFER) {
                flags |= BM_FLAGS_USE_SHORT_TRANSFER;        
            }
            status =  CMD53Transfer_ByteBasis(pContext, 
                                              Address,
                                              pDataBuffer,
                                              verifySize,
                                              BlockSize, // block size hint
                                              flags); 
             
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        BenchMarkEnd(pParams);
        
        GetTimeStats(pParams,
                     "Read",
                     "Bytes",
                     verifySize);
        
        if (!VerifyBuffers(pDataBuffer,pVerifyBuffer,verifySize)) {
            status = SDIO_STATUS_ERROR;    
            break;
        }     
    
        REL_PRINT(SDDBG_TRACE,("SDIO CMD53 Data Mode:(%s) DATA VERIFIED SUCCESSFULLY\n",
                      (TestFlags & PERF_TEST_BLOCK_MODE) ? "Block Mode" : "Byte Mode"));    
                      
    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) {
        if (SDIO_STATUS_UNSUPPORTED == status) {
                /* the test might have hit a DMA alignment issue, continue with other
                 * tests but print that the test was skipped */
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 Data Verify Mode:(%s) was SKIPPED\n",
                      (TestFlags & PERF_TEST_BLOCK_MODE) ? "Block Mode" : "Byte Mode")); 
            INC_SKIP_COUNT(pParams);
            status = SDIO_STATUS_SUCCESS;   
        } else {
            INC_ERROR_COUNT(pParams);
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 Data Verify Mode:(%s) Failed:%d\n",
                      (TestFlags & PERF_TEST_BLOCK_MODE) ? "Block Mode" : "Byte Mode", status));    
        }
    } else {
        INC_PASS_COUNT(pParams);
    }
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    
    return status; 
    
}


   /*** SDIO CMD53 read/write verify mixed mode ****/
SDIO_STATUS CMD53TransferMixed(PVOID           pContext,
                               UINT32          Address,
                               PUINT8          pBuffer, 
                               UINT32          TransferLength,
                               UINT32          BlockModeBlockSize,
                               UINT32          IOFlags)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT32      fullBlocks;
    UINT32      bytesFullBlocks;
    UINT32      residue;
    
    fullBlocks = TransferLength / BlockModeBlockSize;
    bytesFullBlocks = fullBlocks * BlockModeBlockSize;
    residue = TransferLength - bytesFullBlocks;
                                      
    do {  
        
        /* note the caller must set the block size register first */
        
            /* do full blocks first */  
        if (fullBlocks) {
            status = CMD53Transfer_BlockBasis(pContext, 
                                              Address,
                                              pBuffer,
                                              fullBlocks,
                                              BlockModeBlockSize,
                                              IOFlags);
        }
            
        if (!SDIO_SUCCESS(status)) {
            break;        
        } 
        
            /* do residue */
        if (residue) {
            status =  CMD53Transfer_ByteBasis(pContext, 
                                              Address + bytesFullBlocks,
                                              &pBuffer[bytesFullBlocks],
                                              residue,
                                              512,
                                              IOFlags); 
             
        }
          
        if (!SDIO_SUCCESS(status)) {
            break;        
        }
        
    } while (FALSE);
    
    return status;   
}

SDIO_STATUS CMD53VerifyMixed(PVOID           pContext,
                             PBM_BUS_CHARS   pBusChars,
                             PBM_TEST_PARAMS pParams,
                             UINT32          Address,
                             PUINT8          pBuffer, 
                             UINT32          BufferSize,
                             UINT32          PacketLength,
                             UINT32          BlockModeBlockSize)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PUINT8      pDataBuffer;
    PUINT8      pVerifyBuffer;
    UINT32      fullBlocks;
    UINT32      residue;
    UINT32      blockSize;
    
    if (0 == PacketLength) {
        DBG_ASSERT(FALSE);
        return SDIO_STATUS_ERROR;    
    }
    
    if (0 == BlockModeBlockSize) {
        DBG_ASSERT(FALSE);
        return SDIO_STATUS_ERROR;    
    }
    
    pDataBuffer = pBuffer;
    pVerifyBuffer = &pBuffer[BufferSize/2];
    blockSize = min(BlockModeBlockSize,pBusChars->MaxBytesPerBlock); 
    
    fullBlocks = PacketLength / blockSize;
    residue = PacketLength - (fullBlocks*blockSize);
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    
    REL_PRINT(SDDBG_TRACE,
            ("SDIO CMD53 MIXED Verify, Block Length:%d Full Blocks:%d, Residue:%d, Total:%d bytes\n",
            blockSize, fullBlocks, residue, PacketLength));
    
    PrintBusMode(pContext,pBusChars);
           
    do {  
        
        if (PacketLength > (BufferSize/2)) {
            status = SDIO_STATUS_ERROR;
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 MIXED Verify Mode: insufficient test buffer\n")); 
            break;  
        }
   
        FillBuffer(pDataBuffer,PacketLength);
        
            /* keep a copy in the second half buffer */
        memcpy(pVerifyBuffer,pDataBuffer,PacketLength); 
     
        if (fullBlocks) {
            status = SetCardBlockLength(pContext, 
                                        BM_CARD_SDIO, 
                                        blockSize);
            
            if (!SDIO_SUCCESS(status)) {
                break;   
            }
        }
                
        BenchMarkStart(pParams);
            
        status = CMD53TransferMixed(pContext,
                                    Address,
                                    pDataBuffer, 
                                    PacketLength,
                                    blockSize,
                                    BM_FLAGS_WRITE);
                                    
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        BenchMarkEnd(pParams);
        
        GetTimeStats(pParams,
                     "Write-Verify",
                     "Bytes",
                     PacketLength);
        
            /* nuke buffer */
        memset(pDataBuffer, 0, PacketLength);
                      
        BenchMarkStart(pParams);
       
        status = CMD53TransferMixed(pContext,
                                    Address,
                                    pDataBuffer, 
                                    PacketLength,
                                    blockSize,
                                    0);
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        BenchMarkEnd(pParams);
        
        GetTimeStats(pParams,
                     "Read-Verify",
                     "Bytes",
                     PacketLength);             
       
        if (!VerifyBuffers(pDataBuffer,pVerifyBuffer,PacketLength)) {
            status = SDIO_STATUS_ERROR;    
            break;
        }   
        
    } while (FALSE);
        
    if (!SDIO_SUCCESS(status)) {
        if (SDIO_STATUS_UNSUPPORTED == status) {
                /* the test might have hit a DMA alignment issue, continue with other
                 * tests but print that the test was skipped */
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 MIXED Mode Verify: was SKIPPED\n")); 
            INC_SKIP_COUNT(pParams);
            status = SDIO_STATUS_SUCCESS;   
        } else {
            INC_ERROR_COUNT(pParams);
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 MIXED Mode Verify Failed:%d\n", status));    
        }
    } else {
        REL_PRINT(SDDBG_ERROR,("    SDIO CMD53 MIXED Mode Verify: SUCCESS \n")); 
        INC_PASS_COUNT(pParams);
    }

    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
 
    return status;   
}

SDIO_STATUS CMD53PerfTestMixed(PVOID           pContext,
                               PBM_BUS_CHARS   pBusChars,
                               PBM_TEST_PARAMS pParams,
                               UINT32          Address,
                               PUINT8          pBuffer, 
                               UINT32          TransferLength,
                               UINT32          BlockModeBlockSize,
                               UINT32          IOFlags,
                               PTEXT           pDesc,
                               INT             Iterations)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    INT         iterationCount;
    UINT32      fullBlocks;
    UINT32      residue;
    UINT32      blockSize;
   
    blockSize = min(BlockModeBlockSize,pBusChars->MaxBytesPerBlock); 
    fullBlocks = TransferLength / blockSize;
    residue = TransferLength - (fullBlocks*blockSize);
    
    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    
    REL_PRINT(SDDBG_TRACE,
            ("SDIO CMD53 MIXED (%s) Block Length:%d Full Blocks:%d, Residue:%d, Total:%d bytes, Iterations:%d\n",
            pDesc, blockSize, fullBlocks, residue, TransferLength, Iterations));
    
    PrintBusMode(pContext,pBusChars);
    
     do {  
        
        if (fullBlocks) { 
            status = SetCardBlockLength(pContext, 
                                        BM_CARD_SDIO, 
                                        blockSize);
            
            if (!SDIO_SUCCESS(status)) {
                break;   
            }
        }
                
        BenchMarkStart(pParams);
            
        for (iterationCount = 0; iterationCount < Iterations; iterationCount++) {
            status = CMD53TransferMixed(pContext,
                                        Address,
                                        pBuffer, 
                                        TransferLength,
                                        blockSize,
                                        IOFlags);
            if (!SDIO_SUCCESS(status)) {
                break;    
            }
        }
        
        BenchMarkEnd(pParams);
        
        if (SDIO_SUCCESS(status)) {
            GetTimeStats(pParams,
                         "Mixed",
                         "Bytes",
                         TransferLength*Iterations);
        }
    } while (FALSE);
        
    if (!SDIO_SUCCESS(status)) {
        if (SDIO_STATUS_UNSUPPORTED == status) {
                /* the test might have hit a DMA alignment issue, continue with other
                 * tests but print that the test was skipped */
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 MIXED Mode: was SKIPPED\n")); 
            INC_SKIP_COUNT(pParams);
            status = SDIO_STATUS_SUCCESS;   
        } else {
            INC_ERROR_COUNT(pParams);
            REL_PRINT(SDDBG_ERROR,("*** SDIO CMD53 MIXED Mode) Failed:%d\n", status));    
        }
    } else {
        INC_PASS_COUNT(pParams);
    }

    REL_PRINT(SDDBG_TRACE,
    ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
 
    return status;   
}

    /* SDIO tests */
SDIO_STATUS SDIOTests(PVOID           pContext, 
                      PBM_BUS_CHARS   pBusChars,
                      PBM_TEST_PARAMS pParams)
{
     SDIO_STATUS    status = SDIO_STATUS_SUCCESS;
     UINT32         address;
     UINT32         flags = BM_FLAGS_CMD52_FIXED_ADDR;
     INT            i;
     PUINT8         pBuffer;
     UINT32         bufferSize;
     PTEXT          pVerifyDesc;
     UINT32         blockSize;
     
     do {
        if (pParams->Cmd52FixedAddress != -1) {
            address = (UINT32)pParams->Cmd52FixedAddress;                    
        } else {
            address = 0; /* read the revision register in common space */
            flags |= BM_FLAGS_CMD52_COMMON_SPACE;
        }
         
        status = SDIOCmd52Transfer(pContext, 
                                   pBusChars,
                                   pParams,
                                   address,
                                   pParams->pTestBuffer,
                                   pParams->BufferSize,
                                   flags,
                                   "Fixed Address, READ-SYNC");
        
        if (!SDIO_SUCCESS(status)) {
            break;        
        }        
                
        status = SDIOCmd52Transfer(pContext, 
                                   pBusChars,
                                   pParams,
                                   address,
                                   pParams->pTestBuffer,
                                   pParams->BufferSize,
                                   flags | BM_FLAGS_ASYNC,
                                   "Fixed Address, READ-ASYNC");
        
        if (!SDIO_SUCCESS(status)) {
            break;        
        }              
                
        if (!pParams->TETestCard) {
            break;    
        }
        
        /* the following tests require the Test card */
           
           /* RAM starts at offset 0 */
        address = 0;
        
        for (i = 0; i < NUM_CMD53_PERF_TESTS; i++) {
            
            if (g_CMD53PerfTestTable[i].TestFlags & PERF_TEST_USE_DMA) {
                if (pParams->pDMABuffer != NULL) {
                    pBuffer = pParams->pDMABuffer;
                    bufferSize = pParams->DMABufferSize;
                } else {
                    REL_PRINT(SDDBG_ERROR,("*** CMD53 performance test:(%s) was SKIPPED, no DMA support in HCD\n",
                        g_CMD53PerfTestTable[i].pDescription));   
                    INC_SKIP_COUNT(pParams);      
                    continue;
                }
            } else {
                pBuffer = pParams->pTestBuffer;
                bufferSize = pParams->BufferSize;
            }
            
            if (g_CMD53PerfTestTable[i].TestFlags & PERF_TEST_MIXED_MODE) {
                if (g_CMD53PerfTestTable[i].TestArg > bufferSize) {
                    REL_PRINT(SDDBG_ERROR,("*** CMD53 Perf Test mixed mode was SKIPPED, insufficient buffer %d\n",
                        bufferSize));         
                    INC_SKIP_COUNT(pParams);
                    continue;    
                }
                
                status = CMD53PerfTestMixed(pContext,
                                            pBusChars,
                                            pParams,
                                            address,
                                            pBuffer,
                                            g_CMD53PerfTestTable[i].TestArg,
                                            min((UINT32)1024,pBusChars->MaxBytesPerBlock),
                                            g_CMD53PerfTestTable[i].IOFlags,
                                            g_CMD53PerfTestTable[i].pDescription,
                                            100);  
            } else {
            
                status = Cmd53PerformanceTest(pContext, 
                                              pBusChars,
                                              pParams,
                                              address,
                                              pBuffer,
                                              bufferSize,
                                              g_CMD53PerfTestTable[i].TestFlags,
                                              g_CMD53PerfTestTable[i].IOFlags,
                                              g_CMD53PerfTestTable[i].pDescription);
            }
         
            if (!SDIO_SUCCESS(status)) {
                break;        
            }  
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;        
        }   
             
        for (i = 0; i < NUM_CMD53_VERIFY_TESTS; i++) {
            if (g_CMD53VerifyTestTable[i].TestFlags & PERF_TEST_USE_DMA) {
                if (pParams->pDMABuffer != NULL) {
                    pBuffer = pParams->pDMABuffer;
                    bufferSize = pParams->DMABufferSize;
                    pVerifyDesc = "DMA Buffer, SYNC";
                } else {
                    REL_PRINT(SDDBG_ERROR,("*** CMD53 read verify test: was SKIPPED, no DMA support in HCD\n"));         
                    INC_SKIP_COUNT(pParams);
                    continue;
                }
            } else {
                pBuffer = pParams->pTestBuffer;
                bufferSize = pParams->BufferSize;
                pVerifyDesc = "Normal Buffer, SYNC";
            }
                       
            blockSize = g_CMD53VerifyTestTable[i].TestArg;
            
            if (g_CMD53VerifyTestTable[i].TestFlags & PERF_TEST_BLOCK_MODE) {
                if (blockSize > pBusChars->MaxBytesPerBlock) {
                     REL_PRINT(SDDBG_ERROR,
                        ("SDIO CMD53 Data Verify, Block Mode Test Blocksize:%d is > HCD max (%d) SKIPPING \n",
                        blockSize,pBusChars->MaxBytesPerBlock)); 
                    INC_SKIP_COUNT(pParams);
                    continue;  
                }   
            } else {
                 if (blockSize > 512) {
                    REL_PRINT(SDDBG_ERROR,
                        ("SDIO CMD53 Data Verify, Byte Mode Test Blocksize:%d is > 512 \n",
                        blockSize));   
                        /* force it to 512 */        
                    blockSize = 512;    
                }   
                
                if (blockSize > pBusChars->MaxBytesPerBlock) {
                     REL_PRINT(SDDBG_ERROR,
                        ("SDIO CMD53 Data Verify, Byte Mode Test Blocksize:%d is > HCD max (%d) SKIPPING \n",
                        blockSize,pBusChars->MaxBytesPerBlock)); 
                    INC_SKIP_COUNT(pParams);
                    continue;   
                }
            }
            
             
            status = CMD53ReadWriteVerify(pContext,
                                          pBusChars,
                                          pParams,
                                          address, 
                                          blockSize,
                                          pBuffer, 
                                          bufferSize,
                                          g_CMD53VerifyTestTable[i].TestFlags,
                                          pVerifyDesc);
                                          
            if (!SDIO_SUCCESS(status)) {
                if (status == SDIO_STATUS_ERROR) {
                    continue;    
                }
                break;    
            }
            
            
        }    
                                 
     } while (FALSE);
        
     return status;
}
