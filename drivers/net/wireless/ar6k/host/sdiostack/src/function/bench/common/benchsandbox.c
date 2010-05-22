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
@file: benchsandbox.c

@abstract: SD Bus Bench Mark sandbox testing

#notes: This file can be used to perform HCD debugging without having to check
        the changes back into the source tree
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include "benchmark.h"


void SetHcdDebugLevel(PVOID pContext,CT_DEBUG_LEVEL Level);
void RestoreHcdDebugLevel(PVOID pContext);
                                 
SDIO_STATUS RunSandboxTests(PVOID           pContext, 
                            PBM_BUS_CHARS   pBusChars,
                            PBM_TEST_PARAMS pParams)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    CT_DEBUG_LEVEL dlevel = 0; 
                       
    if (dlevel != 0) {                          
        SetHcdDebugLevel(pContext,dlevel);
    }
       
    do {     
            /* add sandbox tests here.... */
        
        /* for example:
        SetCardBusMode(pContext,BM_BUS_1_BIT,&pBusChars->ClockRate);
                                
        CMD53ReadWriteVerify(pContext, 
                             pBusChars,
                             pParams,
                             0, 
                             33,
                             pParams->pTestBuffer, 
                             pParams->BufferSize,
                             PERF_TEST_BYTE_MODE,
                             "test"); 
        */
        
                                                                               
        /* set return status to an error if the test should stop after
         * the sandbox tests */
        /* status = SDIO_STATUS_ERROR; */
        
               
    } while (FALSE); 
    
    if (dlevel != 0) { 
        RestoreHcdDebugLevel(pContext);
    }
    
    return status;
}
