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
@file: benchmark.c

@abstract: SD Bus BenchMarking functions

#notes: OS independent portions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include "benchmark.h"
     
PTEXT g_BusModeString[BM_BUS_LAST] = {
    "1-Bit",    
    "4-Bit", 
    "8-Bit",
    "SPI no CRC",
    "SPI with CRC",     
};

PTEXT g_CardTypeString[BM_CARD_LAST] = {
    "SDIO",
    "MMC",
    "SDCARD",
};

SDIO_STATUS SDMEMTests(PVOID           pContext, 
                       PBM_BUS_CHARS   pBusChars,
                       PBM_TEST_PARAMS pParams);


SDIO_STATUS SDIOTests(PVOID           pContext, 
                      PBM_BUS_CHARS   pBusChars,
                      PBM_TEST_PARAMS pParams);

SDIO_STATUS RunSandboxTests(PVOID           pContext, 
                            PBM_BUS_CHARS   pBusChars,
                            PBM_TEST_PARAMS pParams);
    
SDIO_STATUS RunCardTests(PVOID pContext, PBM_BUS_CHARS pBusChars, PBM_TEST_PARAMS pTestParams) 
{
    SDIO_STATUS status = SDIO_STATUS_ERROR;
    REL_PRINT(SDDBG_TRACE,
    ("***********************************Test Run Start*********************************************\n"));
    
    REL_PRINT(SDDBG_TRACE, ("SD Bus BenchMark (%s): Bus Clock rate: %d Hz Bus Mode: %s \n",
        (pBusChars->CardType < BM_CARD_LAST) ? g_CardTypeString[pBusChars->CardType]: "UNKNOWN",
        pBusChars->ClockRate, 
        (pBusChars->BusMode < BM_BUS_LAST) ? g_BusModeString[pBusChars->BusMode]: "UNKNOWN" ));

    REL_PRINT(SDDBG_TRACE, ("SD Bus BenchMark : Max Blocks: %d , Max Bytes Per Block:%d \n",
        pBusChars->MaxBlocksPerTransfer, pBusChars->MaxBytesPerBlock));
    
    do {
        status = RunSandboxTests(pContext,pBusChars,pTestParams);
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
                 
         /* start the tests */                
           
        switch (pBusChars->CardType) {
        
            case BM_CARD_SDIO:
                status = SDIOTests(pContext,pBusChars,pTestParams);
                break;
            case BM_CARD_SD:
            case BM_CARD_MMC:
                status = SDMEMTests(pContext,pBusChars,pTestParams);
                break;
            default:
                DBG_ASSERT(FALSE);
                break;
        }
        
    } while (FALSE);
    
    REL_PRINT(SDDBG_TRACE,
    ("***********************************Test Run End***********************************************\n"));
    
        
    return status;
}                 
                                                 
/* benchmark test entry point */
void BenchMarkTests(PVOID pContext, PBM_BUS_CHARS pBusChars)
{
    BM_TEST_PARAMS testParams;
    SDIO_STATUS    status;
    
    REL_PRINT(SDDBG_TRACE, ("+SD Bus BenchMark Starting....\n"));
    
    ZERO_OBJECT(testParams);
    
    testParams.Cmd52FixedAddress  = -1;
    
    GetBenchMarkParameters(pContext, &testParams);
  
    do {
         
        if ((NULL == testParams.pTestBuffer) || (0== testParams.BufferSize)) {
            REL_PRINT(SDDBG_TRACE, ("*** No Buffers! \n"));
            break;    
        }
        
        SetUpOSTimming(&testParams);
        
        REL_PRINT(SDDBG_TRACE, ("SD Bus BenchMark : Buffer Size: %d bytes \n",testParams.BufferSize));
               
        status = RunCardTests(pContext,pBusChars,&testParams);
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        if (pBusChars->BusMode != BM_BUS_4_BIT) {
            break;    
        }
        
            /* drop to 1 bit and run the tests again */
        status = SetCardBusMode(pContext,BM_BUS_1_BIT, &pBusChars->ClockRate);        
        
        if (!SDIO_SUCCESS(status)) {
            REL_PRINT(SDDBG_TRACE, 
               ("SD Bus BenchMark : Failed to switch to 1 bit mode %d  \n",status));
            break;   
        }
        
        pBusChars->BusMode = BM_BUS_1_BIT;
        
        status = RunCardTests(pContext,pBusChars,&testParams);
        
            /* restore bus mode */
        SetCardBusMode(pContext,BM_BUS_4_BIT,&pBusChars->ClockRate); 
                    
    } while (FALSE);  
    
    if (0 == testParams.ErrorCount) {
        REL_PRINT(SDDBG_TRACE, ("+++++++ SD Bus BenchMark TEST SUCCESS (passed:%d, skipped:%d, total:%d)+++++++++\n",
            testParams.PassCount,testParams.SkipCount,(testParams.PassCount + testParams.SkipCount)));        
    } else {
        REL_PRINT(SDDBG_TRACE,
            ("******* SD Bus BenchMark some TEST FAILURES (passed:%d, skipped:%d, failed:%d) *********\n",
            testParams.PassCount,testParams.SkipCount, testParams.ErrorCount));     
    }
    
    REL_PRINT(SDDBG_TRACE, ("-SD Bus BenchMark Done \n"));
}

void PrintBusMode(PVOID           pContext, 
                  PBM_BUS_CHARS   pBusChars)
{
    REL_PRINT(SDDBG_TRACE, ("    BusClk: %d Hz BusMode: %s \n",
        pBusChars->ClockRate, 
        (pBusChars->BusMode < BM_BUS_LAST) ? g_BusModeString[pBusChars->BusMode]: "UNKNOWN" ));                                
}
