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
@file: benchmem.c

@abstract: SD Bus BenchMarking functions for Memory Cards

#notes: OS independent portions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include "benchmark.h"


#define BLOCK_OP_READ  TRUE
#define BLOCK_OP_WRITE FALSE

#define MEM_TEST_USE_DMA (1 << 0)

typedef struct _MEMCARD_VERIFY_TEST_ENTRY {
    UINT32  TestFlags;
    UINT32  Blocks;
}MEMCARD_VERIFY_TEST_ENTRY, *PMEMCARD_VERIFY_TEST_ENTRY;

/* test table to run a gamut of block sizes */
MEMCARD_VERIFY_TEST_ENTRY g_MemCardVerifyTestTable[] =
{
    {0, 5   },
    {0, 16  },
    {0, 37  },
    {0, 44  },
    {0, 50  },
    {0 | MEM_TEST_USE_DMA, 8 },
    {0 | MEM_TEST_USE_DMA, 16 },
    {0 | MEM_TEST_USE_DMA, 32 },
    {0 | MEM_TEST_USE_DMA, 64 },
};

#define NUM_MEMCARD_VERIFY_TESTS (sizeof(g_MemCardVerifyTestTable)) / (sizeof(MEMCARD_VERIFY_TEST_ENTRY))

BOOL VerifyBuffers(PUINT8 pDataBuffer, PUINT8 pVerifyBuffer, INT Length)
{
    INT ii;
    
    for (ii = 0; ii < Length; ii++) {
        if (pDataBuffer[ii] != pVerifyBuffer[ii]) {
            REL_PRINT(SDDBG_TRACE, 
              ("!!!!VerifyBuffers: buffer read error at offset %d, Got:0x%X, Expecting:0x%X\n",
                 ii, pDataBuffer[ii], pVerifyBuffer[ii]));
            break; 
        }
    }
    
    if (ii >= Length) {
        return TRUE;
    }    
    
    return FALSE;
}



SDIO_STATUS SDMemPerfTest(PVOID           pContext,
                          PBM_TEST_PARAMS pParams,
                          UINT32          BlockAddress,
                          UINT16          Blocks,
                          UINT16          BlockSize,
                          PUINT8          pBuffer,
                          INT             Iterations,
                          PTEXT           pStatsDescrip,
                          BOOL            Read)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT32      totalBytes;
    INT         iterationsCount;
    
    REL_PRINT(SDDBG_TRACE,
     ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
  
    REL_PRINT(SDDBG_TRACE, 
        ("Memory Card Performance Test (%s) : Iterations:%d , Blocks: %d, Bytes Per Block: %d\n",
              (pStatsDescrip != NULL) ? pStatsDescrip : "GENERIC",
              Iterations, Blocks, BlockSize));
    
    totalBytes = Blocks * Iterations * BlockSize;         
    BenchMarkStart(pParams);
                      
    for (iterationsCount = 0; iterationsCount < Iterations; iterationsCount++) {
        status = MemCardBlockTransfer(pContext,
                                      BlockAddress,
                                      Blocks,
                                      BlockSize,
                                      pBuffer,
                                      Read ? 0 : BM_FLAGS_WRITE);
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }  
        
    }                         
    
    BenchMarkEnd(pParams);
    
    if (SDIO_SUCCESS(status)) {
        INC_PASS_COUNT(pParams);
        if (pParams != NULL) {
            GetTimeStats(pParams,
                         pStatsDescrip, 
                         "Bytes",
                         totalBytes);
        }
    } else {        
        REL_PRINT(SDDBG_TRACE, ("*** SDMemPerfTest (%s:%s): Failed (%d) (its:%d) \n",
                (pStatsDescrip != NULL) ? pStatsDescrip : "GENERIC", 
                Read ? "Read" : "Write",
                status, iterationsCount)); 
        INC_ERROR_COUNT(pParams);
    }  
    
    REL_PRINT(SDDBG_TRACE,
      ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
  
    return status; 
    
}


    /*** block write and read verify ****/
SDIO_STATUS BlockReadWriteVerify(PVOID  pContext, 
                                 UINT32 BlockAddress, 
                                 UINT32 Blocks,
                                 UINT16 BlockSize,
                                 PUINT8 pBuffer,
                                 PTEXT  pDesc)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PUINT8      pDataBuffer;
    PUINT8      pVerifyBuffer;
    UINT32      verifySize;
    
    REL_PRINT(SDDBG_TRACE,
       ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
    
    verifySize = Blocks * BlockSize;
    pDataBuffer = pBuffer;    
    pVerifyBuffer = &pBuffer[verifySize];
    
        
    REL_PRINT(SDDBG_TRACE, 
        ("Memory Card Read/Write Verify Test (%s), Blocks: %d, BytesPerBlock: %d, Total: %d bytes\n", 
        pDesc, Blocks, BlockSize, verifySize));
           
    do {  
        
        FillBuffer(pDataBuffer,verifySize);
        
            /* keep a copy in the second half buffer */
        memcpy(pVerifyBuffer,pDataBuffer,verifySize);
        
            /* write out the pattern */            
        status = MemCardBlockTransfer(pContext,
                                      BlockAddress,
                                      Blocks,
                                      BlockSize,
                                      pDataBuffer,
                                      BM_FLAGS_WRITE);

        if (!SDIO_SUCCESS(status)) {
            break;    
        }
                
        memset(pDataBuffer,0,verifySize);
        
            /* read back the pattern */            
        status = MemCardBlockTransfer(pContext,
                                      BlockAddress,
                                      Blocks,
                                      BlockSize,
                                      pDataBuffer,
                                      0);
                                             
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        if (!VerifyBuffers(pDataBuffer,pVerifyBuffer,verifySize)) {
            status = SDIO_STATUS_ERROR;    
            break;
        }     
    
        REL_PRINT(SDDBG_TRACE, ("Memory Card Read/Verify : VERIFY SUCCESS! \n"));    
    
    } while (FALSE);
    
    if (!SDIO_SUCCESS(status)) {
        REL_PRINT(SDDBG_TRACE, ("Memory Card Read/Verify : Error: %d \n",status));           
    }
    REL_PRINT(SDDBG_TRACE,
      ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"));
  
    return status; 
    
}

                        
SDIO_STATUS SDMEMTests(PVOID           pContext, 
                       PBM_BUS_CHARS   pBusChars,
                       PBM_TEST_PARAMS pParams)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16      blocks;
    UINT32      blockAddress;   
    INT         i;             
    PUINT8      pBuffer;
    UINT32      bufferSize;
    PTEXT       pDesc;
          
    do {
        
        status = SetCardBlockLength(pContext,
                                    pBusChars->CardType,
                                    MEMCARD_TEST_BLOCK_SIZE);
                                    
        if (!SDIO_SUCCESS(status)) {
            REL_PRINT(SDDBG_TRACE, ("*** Memory Card Failed to set Block Length : %d \n",
                MEMCARD_TEST_BLOCK_SIZE));
            INC_ERROR_COUNT(pParams);
            break;    
        }                                       
        
        blockAddress = MEMCARD_TEST_BLOCK_ADDRESS * MEMCARD_TEST_BLOCK_SIZE;
        blocks = pParams->BufferSize / MEMCARD_TEST_BLOCK_SIZE;
               
            /* PIO mode read performance tests */
        status = SDMemPerfTest(pContext,
                               pParams,
                               blockAddress,
                               blocks,
                               MEMCARD_TEST_BLOCK_SIZE,
                               pParams->pTestBuffer,
                               MEMCARD_TEST_ITERATIONS,
                               "PIO Buffer Read, SYNC",
                               BLOCK_OP_READ);
                                     
        if (!SDIO_SUCCESS(status)) {
            break;    
        }                         
         
            /* PIO mode write performance tests .. using the same data we read */
        status = SDMemPerfTest(pContext,
                               pParams,
                               blockAddress,
                               blocks,
                               MEMCARD_TEST_BLOCK_SIZE,
                               pParams->pTestBuffer,
                               MEMCARD_TEST_ITERATIONS,
                               "PIO Buffer Write, SYNC",
                               BLOCK_OP_WRITE);
                                     
        if (!SDIO_SUCCESS(status)) { 
            break;    
        }  
        
        if (pParams->pDMABuffer != NULL) {
            
            blocks = pParams->DMABufferSize / MEMCARD_TEST_BLOCK_SIZE;
            
                /* DMA mode, read performance test */
            status = SDMemPerfTest(pContext,
                                   pParams,
                                   blockAddress,
                                   blocks,
                                   MEMCARD_TEST_BLOCK_SIZE,
                                   pParams->pDMABuffer,
                                   MEMCARD_TEST_ITERATIONS,
                                   "DMA Buffer Read, SYNC",
                                   BLOCK_OP_READ);
            
            if (!SDIO_SUCCESS(status)) { 
                break;    
            }  
        
                /* DMA mode, write performance test..using same data */
            status = SDMemPerfTest(pContext,
                                   pParams,
                                   blockAddress,
                                   blocks,
                                   MEMCARD_TEST_BLOCK_SIZE,
                                   pParams->pDMABuffer,
                                   MEMCARD_TEST_ITERATIONS,
                                   "DMA Buffer Write, SYNC",
                                   BLOCK_OP_WRITE);
                                         
            if (!SDIO_SUCCESS(status)) { 
                break;    
            } 
                                         
        } else {
            INC_SKIP_COUNT(pParams);
            INC_SKIP_COUNT(pParams);
            REL_PRINT(SDDBG_TRACE, ("*** Memory Card Test: DMA Benchmark test SKIPPED, DMA not supported in HCD\n"));   
        
        }
        
        /*** block write and read verify ****/
        
        for (i = 0; i < NUM_MEMCARD_VERIFY_TESTS; i++) {
            
            if (g_MemCardVerifyTestTable[i].TestFlags & MEM_TEST_USE_DMA) {
                
                if (NULL == pParams->pDMABuffer) {
                    REL_PRINT(SDDBG_ERROR,
                      ("*** Memcard read/write verify (blocks:%d,bpb:%d) using DMA test: SKIPPED, no DMA support in HCD\n",
                       g_MemCardVerifyTestTable[i].Blocks,MEMCARD_TEST_BLOCK_SIZE));         
                    INC_SKIP_COUNT(pParams);
                    continue;   
                }
                
                pBuffer = pParams->pDMABuffer, 
                bufferSize = pParams->DMABufferSize;     
                pDesc = "DMA Buffer";    
                
            } else {
                
                pBuffer = pParams->pTestBuffer;
                bufferSize = pParams->BufferSize;
                pDesc = "Normal Buffer";
            
            }
           
                /* figure out how many total blocks in the buffer for both write and verify
                 * portions */ 
            blocks = (bufferSize / 2) / MEMCARD_TEST_BLOCK_SIZE; 

            if (blocks < g_MemCardVerifyTestTable[i].Blocks) {
                REL_PRINT(SDDBG_ERROR,
                 ("*** Memcard read/write verify test: SKIPPED, %d blocks available in %s, is less than desired blocks :%d\n",
                        blocks,pDesc,g_MemCardVerifyTestTable[i].Blocks));  
                INC_SKIP_COUNT(pParams);
                continue;        
            }
                
            blocks = g_MemCardVerifyTestTable[i].Blocks;
               
            status = BlockReadWriteVerify(pContext, 
                                          blockAddress, 
                                          blocks,
                                          MEMCARD_TEST_BLOCK_SIZE,
                                          pBuffer,
                                          pDesc);

            if (!SDIO_SUCCESS(status)) {
                INC_ERROR_COUNT(pParams);
                break;    
            } else {
                INC_PASS_COUNT(pParams);   
            }
        
        }
        
    } while (FALSE);   
 
    return status;
}
