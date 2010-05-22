/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample.h

@abstract: OS independent include SDIO sample function driver

#notes: 
 
@notice: Copyright (c), 2005-2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_SAMPLE_H___
#define __SDIO_SAMPLE_H___

#if defined(LINUX) || defined(__linux__)
#include "linux/sample_linux.h"
#endif /* LINUX */


#include "../../hcd/omap_raw_spi/raw_spi_hcd_if.h"


enum ATH_SAMPLE_TRACE_ENUM {
    ATH_IRQ = (SDDBG_TRACE + 1),
    ATH_BUS_REQ = (SDDBG_TRACE + 2),
};

typedef enum _ATH_IRQ_PROC_STATE {
  ATH_IRQ_NONE,
  ATH_IRQ_TXDMA_PENDING,
  ATH_IRQ_RXDMA_PENDING,
}ATH_IRQ_PROC_STATE, PATH_IRQ_PROC_STATE; 

#define MAX_DMA_TESTS 6

typedef struct DMA_TEST_ENTRY {
    ATH_TRANS_CMD DmaMode;    
    INT           MinDataSize;
    INT           TestCount;
}DMA_TEST_ENTRY, *PDMA_TEST_ENTRY;

typedef struct _SAMPLE_FUNCTION_INSTANCE {
    SDLIST         SDList;      /* link in the instance list */
    PSDDEVICE      pDevice;     /* bus driver's device we are supporting */
    SAMPLE_CONFIG  Config;      /* OS specific config  */
    OS_SEMAPHORE   IOComplete;  /* io complete semaphore */
    SDIO_STATUS    LastRequestStatus;  /* last request status */
    UINT16         MaxBytesPerDMA;     /* max bytes per DMA transfer request */
    UINT8          TxMBox;
    UINT8          PendingRxMBox;
    UINT16         PendingRxBytes;
    INT            PendingDMAModeIndex; 
    DMA_TEST_ENTRY DMATests[MAX_DMA_TESTS];
    UINT8          DataSeed;
    PUINT8         pUseBuffer;
    ATH_IRQ_PROC_STATE IrqProcState;
    BOOL           ShutDown;
}SAMPLE_FUNCTION_INSTANCE, *PSAMPLE_FUNCTION_INSTANCE;

typedef struct _SAMPLE_FUNCTION_CONTEXT {
    SDFUNCTION      Function;       /* function description for bus driver */ 
    OS_SEMAPHORE    InstanceSem;    /* instance lock */
    SDLIST          InstanceList;    /* list of instances */
}SAMPLE_FUNCTION_CONTEXT, *PSAMPLE_FUNCTION_CONTEXT;

/* prototypes */
SDIO_STATUS InitializeInstance(PSAMPLE_FUNCTION_CONTEXT  pFuncContext,
                               PSAMPLE_FUNCTION_INSTANCE pInstance,
                               PSDDEVICE                 pDevice);

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
#endif /* __SDIO_SAMPLE_H___*/               
