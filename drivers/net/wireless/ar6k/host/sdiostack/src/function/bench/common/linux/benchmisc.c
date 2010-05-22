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
@file: benchmisc.c

@abstract: Benchmarking miscellaneous functions.

#notes: OS dependent
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include "../benchmark.h"

#ifdef USE_64_BIT_DO_DIV
#include <asm/div64.h>
#endif

#ifdef USE_HW_CYCLES_COUNT
#define COUNTER_TICK_RATE CLOCK_TICK_RATE
#else
#define COUNTER_TICK_RATE HZ
#endif   

void SetUpOSTimming(PBM_TEST_PARAMS pParams)
{
     REL_PRINT(SDDBG_TRACE, 
        ("SDIO BenchMark - OS Timming TickRate: %d Hz \n",COUNTER_TICK_RATE));
}


void BenchMarkStart(PBM_TEST_PARAMS pParams)
{
#ifdef USE_HW_CYCLES_COUNT
    pParams->OSTimingInfo.CyclesStart = get_cycles();
#else
    pParams->OSTimingInfo.JiffiesStart = jiffies;
#endif 
    
}

void BenchMarkEnd(PBM_TEST_PARAMS pParams)
{
#ifdef USE_HW_CYCLES_COUNT
    pParams->OSTimingInfo.CyclesEnd = get_cycles();
#else
    pParams->OSTimingInfo.JiffiesEnd = jiffies;
#endif   
}

UINT32 GetBenchMarkDeltaTicks(PBM_TEST_PARAMS pParams)
{
 
#ifdef USE_HW_CYCLES_COUNT
     if (pParams->OSTimingInfo.CyclesEnd >= pParams->OSTimingInfo.CyclesStart) {
        return (UINT32)(pParams->OSTimingInfo.CyclesEnd - pParams->OSTimingInfo.CyclesStart);
    }
#else  
    if (pParams->OSTimingInfo.JiffiesEnd >= pParams->OSTimingInfo.JiffiesStart) {
        return pParams->OSTimingInfo.JiffiesEnd - pParams->OSTimingInfo.JiffiesStart;
    }
#endif    
    REL_PRINT(SDDBG_ERROR, ("SDIO BenchMark , Tick Overflow!!! Timming may not be accurate!\n"));
    return 0;
}

void GetTimeStats(PBM_TEST_PARAMS pParams,
                  PTEXT  pDescription, 
                  PTEXT  pOpsDescription, 
                  UINT32 Ops)
{
    unsigned long long temp;
    unsigned long long totalOps;
    UINT32 freq =  COUNTER_TICK_RATE;            
    ZERO_OBJECT(temp);
    ZERO_OBJECT(totalOps);
    
    totalOps = Ops;
    temp = GetBenchMarkDeltaTicks(pParams);

    temp *= 1000;
         
#ifdef USE_64_BIT_DO_DIV
    do_div(temp,freq);
#else
    temp /= COUNTER_TICK_RATE;
#endif
    
    REL_PRINT(SDDBG_TRACE, 
       ("----> SDIO Test Stats (%s) : Total %s : %d, time:%d MS \n",
               pDescription, 
               pOpsDescription, 
               (UINT32)totalOps, 
               (UINT32)temp));
    
    if (0 == temp) {
        REL_PRINT(SDDBG_TRACE, ("      Calculated time delta was zero! \n"));  
        return;   
    }
    
    totalOps *= 1000;
#ifdef USE_64_BIT_DO_DIV
    do_div(totalOps,(UINT32)temp);
#else
    totalOps /= temp;
#endif   
    REL_PRINT(SDDBG_TRACE, ("      (%d %s/sec) \n",(UINT32)totalOps,pOpsDescription));             
}

void FillBuffer(PUINT8 pBuffer, INT Length)
{
    INT   ii;
    UINT8 pattern = (UINT8)jiffies;
    
    for (ii = 0; ii < Length; ii++,pBuffer++) {
        *pBuffer = pattern;
        pattern++;
    }
}
