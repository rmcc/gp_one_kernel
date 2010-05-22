/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
@file: sample.c

@abstract: SDIO Sample Function driver

#notes: OS independent portions
 
@notice: Copyright (c), 2005-2006 Atheros Communications, Inc.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <sdio_lib.h>

#include "sample.h"



/////
#if 1
//#define ALIGN_TRANSFERS 1
//#define ALIGN_BUFFERS 1
#define TEST_SIZE_INCR
#endif
/////

       
#define DATA_BUFFER_BYTES   4096

#define RXCHECKDELAY 500

#if 1
#define DMA_MODE_TEST0  (ATH_TRANS_DMA_16 | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC)
#define DMA_MODE_TEST1  (ATH_TRANS_DMA_32 | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC)
#define DMA_MODE_TEST2  (ATH_TRANS_DMA_8  | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC)
#define DMA_MODE_TEST3  (ATH_TRANS_DMA_16 | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC | ATH_TRANS_DMA_PSEUDO)
#define DMA_MODE_TEST0_MINSIZE 6
#define DMA_MODE_TEST1_MINSIZE 7
#define DMA_MODE_TEST2_MINSIZE 7
#define DMA_MODE_TEST3_MINSIZE 6
#else 
    /* a specific DMA test case */
#define DMA_MODE_TEST0  (ATH_TRANS_DMA_32 | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC)
#define DMA_MODE_TEST1  DMA_MODE_TEST0
#define DMA_MODE_TEST2  DMA_MODE_TEST0
#define DMA_MODE_TEST3  DMA_MODE_TEST0
#define DMA_MODE_TEST0_MINSIZE 21
#define DMA_MODE_TEST1_MINSIZE DMA_MODE_TEST0_MINSIZE
#define DMA_MODE_TEST2_MINSIZE DMA_MODE_TEST0_MINSIZE
#define DMA_MODE_TEST3_MINSIZE DMA_MODE_TEST0_MINSIZE
#endif

    /* list of DMA tests to cycle through */
DMA_TEST_ENTRY DMATestTemplate[MAX_DMA_TESTS] =
{
    {DMA_MODE_TEST0,DMA_MODE_TEST0_MINSIZE,0},
    {DMA_MODE_TEST1,DMA_MODE_TEST1_MINSIZE,0},
    {DMA_MODE_TEST2,DMA_MODE_TEST2_MINSIZE,0},
    {DMA_MODE_TEST3,DMA_MODE_TEST3_MINSIZE,0},
}; 

SDIO_STATUS MboxRx(PSAMPLE_FUNCTION_INSTANCE pInstance,
                   UINT8                     RxMbox,
                   PUINT8                    pBuffer,
                   UINT16                    TotalBytes,
                   ATH_TRANS_CMD             DMAMode,
                   BOOL                      Verify);

SDIO_STATUS MboxTx(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                   UINT8                     TxMbox,
                   UINT8                     RxMbox,
                   PUINT8                    pBuffer,
                   UINT16                    NumBytes,
                   UINT8                     DataSeed,
                   ATH_TRANS_CMD             DMAMode); 

SDIO_STATUS CopyBuffer(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                       UINT16 Address,
                       ATH_TRANS_CMD Type,
                       PVOID  pBuffer,
                       UINT16 Bytes,
                       BOOL   Read);
                       
SDIO_STATUS TransferDMA(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                        UINT16 Address,
                        ATH_TRANS_CMD  Type,
                        PVOID  pBuffer,
                        UINT16 Bytes,
                        BOOL   Read);
                                                                                        
void IRQHandler(PVOID pContext);

static UINT32 gAlignedDataBuffer[DATA_BUFFER_BYTES];
 
static PUCHAR gDataBuffer = (PUCHAR)gAlignedDataBuffer;
static UINT8 VerifyBuffer[DATA_BUFFER_BYTES];
     
#define PIO_READ  TRUE
#define PIO_WRITE FALSE
#define SCRATCH_REG_ADDRESS    (0x0460)  /* external scratch register */


#define MAX_MBOXES 4

//#define START_MBOX_ADDRESS 0x0000
//#define MBOX_SIZE          0x0100

#define START_MBOX_ADDRESS 0x0800
#define MBOX_SIZE          0x0800  

#define GET_MBOX_ADDRESS(i) ((i)*MBOX_SIZE + START_MBOX_ADDRESS)

SDIO_STATUS PioReg(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                   UINT16                    Address,
                   ATH_TRANS_CMD             Type,
                   UINT32                    *pData,
                   BOOL                      Read); 
  

void DoDataReadWriteTest(PSAMPLE_FUNCTION_INSTANCE pInstance, UINT16 Address, ATH_TRANS_CMD Type, UINT32 DataPattern) 
{
    UINT32 testData;
    UINT32 mask;
    SDIO_STATUS status;
    PTEXT pWidth;
    
    switch (ATH_GET_TRANS_DS(Type)) {
        
        case ATH_TRANS_DS_8:
            pWidth = "8-bit";
            mask = 0xFF;
            break;
        case ATH_TRANS_DS_16:
            pWidth = "16-bit";
            mask = 0xFFFF;
            break;
        case ATH_TRANS_DS_32:
            pWidth = "32-bit";
            mask = 0xFFFFFFFF;
            break;
        default:
            DBG_ASSERT(FALSE);
            return;    
    } 
     
    do {
             
        testData = DataPattern;
        status = PioReg(pInstance, 
                        Address,
                        Type,
                        &testData,
                        PIO_WRITE); 
                    
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
        testData = 0;
        status = PioReg(pInstance, 
                    Address,
                    Type,
                    &testData,
                    PIO_READ); 
                    
        if (!SDIO_SUCCESS(status)) {
            break;
        } 
        
        testData &= mask;
        
        if (testData != DataPattern) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO ATHRAW: (%s) Address:0x%X  r/w error!  Expected:0x%X Got:0x%X \n",
             pWidth,Address,DataPattern,testData));  
            break;   
        } else {
            DBG_PRINT(SDDBG_TRACE, ("SDIO ATHRAW: (%s) Address:0x%X  r/w Success! Got:0x%X \n",
                pWidth,Address,testData));  
        }
    
    } while (FALSE);
     
}         
      
SDIO_STATUS SetInterfaceClock(PSAMPLE_FUNCTION_INSTANCE pInstance, UINT32 Clock)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST pReq = NULL;
    
    do {
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        
        if (NULL == pReq) {
            status = SDIO_STATUS_NO_RESOURCES;
            break;    
        }
        
        ATH_SET_CLOCK_CONTROL(pReq,Clock);
        pReq->Flags = SDREQ_FLAGS_RAW;  /* note we could do this async with a completion routine as well */
        
            /* submit synchronously */
        status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_TRACE, ("Failed to set interface clock %d \n",status)); 
            break;   
        }
            /* print the actual clock value */
        DBG_PRINT(SDDBG_TRACE, ("New Oper Clock:   %d Hz \n",ATH_GET_CLOCK_CONTROL_VALUE(pReq)));
    } while (FALSE);
  
    if (pReq != NULL) {
        SDDeviceFreeRequest(pInstance->pDevice, pReq);                
    }
    
    return status;
}

   
/* create a sample instance */
SDIO_STATUS InitializeInstance(PSAMPLE_FUNCTION_CONTEXT  pFuncContext,
                               PSAMPLE_FUNCTION_INSTANCE pInstance, 
                               PSDDEVICE                 pDevice)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    INT debugLevel;
    
    do {
       
        SDLIST_INIT(&pInstance->SDList);        
        status = SemaphoreInitialize(&pInstance->IOComplete, 0);
        if (!SDIO_SUCCESS(status)) {
            break;
        }
        
        pInstance->pDevice = pDevice;        
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function Instance: 0x%X \n",(INT)pInstance));
        DBG_PRINT(SDDBG_TRACE, (" Stack Version: %d.%d \n",
            SDDEVICE_GET_VERSION_MAJOR(pDevice),
            SDDEVICE_GET_VERSION_MINOR(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper Clock:   %d Hz \n",SDDEVICE_GET_OPER_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max Clock:    %d Hz \n",SDDEVICE_GET_MAX_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlklenLim:  %d bytes \n",SDDEVICE_GET_OPER_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  BlkLen:     %d bytes\n",SDDEVICE_GET_MAX_BLOCK_LEN(pDevice)));
        
            /* save off max bytes per DMA, this will be limited by the HCD */
        pInstance->MaxBytesPerDMA = SDDEVICE_GET_OPER_BLOCK_LEN(pDevice);
             
            /* set our IRQ handler, passing into it our device as the context  */
        SDDEVICE_SET_IRQ_HANDLER(pDevice, IRQHandler, pInstance);
        /*     ....unmask our interrupt on the card */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: unmasking IRQ \n"));
        status = SDLIB_IssueConfig(pDevice, SDCONFIG_FUNC_UNMASK_IRQ, NULL, 0);  
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to unmask IRQ %d\n",
                                    status));
        }  
        
        
        status = SDLIB_IssueConfig(pDevice, ATH_CUSTOM_CONFIG_GET_DEBUG, &debugLevel, sizeof(INT));  
        
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to get debug level %d\n",
                                    status));
        } else {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: current debug level %d\n",
                                    debugLevel));  
                                    
            status = SDLIB_IssueConfig(pDevice, ATH_CUSTOM_CONFIG_SET_DEBUG, &debugLevel, sizeof(INT));  
        
            if (!SDIO_SUCCESS((status))) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to set debug level %d\n",
                                        status));
            }  
        }  
        
        
        
        status = SetInterfaceClock(pInstance, 12000000);
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
        pInstance->DataSeed = 0;
        memcpy(pInstance->DMATests,DMATestTemplate, sizeof(DMATestTemplate));
#if 0
      /*
        {
            UINT8 temp[5]; 
            
            TransferDMA(pInstance, 
                    SCRATCH_REG_ADDRESS,
                    (ATH_TRANS_DMA_32 | ATH_TRANS_EXT_ADDR | ATH_TRANS_DMA_ADDR_INC),
                    &temp[1],  
                    5,
                    FALSE); 
        }
        break; */
        
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,0x1234);
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_32 | ATH_TRANS_EXT_ADDR,0xDEADBEEF);
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,0x5678);
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_8 | ATH_TRANS_EXT_ADDR,0x55);
              /* some basic read/write tests */        
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,0xAAAA); 
        
        {
            UINT32 temp = 0x0;
                /* local bus register test */
            PioReg(pInstance, 
                   ATH_LOCAL_BUS_REG,
                   ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,
                   &temp,
                   PIO_READ); 
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: LOCAL_BUS: 0x%X\n", 
                    temp & 0xFF));
                           
            PioReg(pInstance, 
                   ATH_LOCAL_BUS_REG,
                   ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,
                   &temp,
                   PIO_WRITE);
            temp = 0;
            PioReg(pInstance, 
                   ATH_LOCAL_BUS_REG,
                   ATH_TRANS_DS_16 | ATH_TRANS_EXT_ADDR,
                   &temp,
                   PIO_READ); 
             DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: LOCAL_BUS2x: 0x%X\n", 
                    temp & 0xFF));
        }
                                           
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_8 | ATH_TRANS_EXT_ADDR,0x33);
        DoDataReadWriteTest(pInstance, SCRATCH_REG_ADDRESS, ATH_TRANS_DS_32 | ATH_TRANS_EXT_ADDR,0xDEADBEEF);
       
#endif

        {  
            UINT32 temp;
                      
            temp = ATH_SPI_CONFIG_INTR;
            
            status = PioReg(pInstance, 
                            ATH_SPI_CONFIG_REG,
                            ATH_TRANS_DS_8 | ATH_TRANS_INT_ADDR,
                            &temp,
                            PIO_WRITE);  
            
            if (!SDIO_SUCCESS(status)) {
                break;    
            } 
            
           
        }                   
             /* ping test
             * note: the data size cannot be too big, the SD11 outputs each byte it receives
             * out a 9600 baud serial port.  The Dragon core issues an interrupt even though
             * the SD11 has not moved the data to the RX Mbox (because of the prints). 
             * we put a delay in the interrupt handler to hold off reading until the SD11
             * has finished it's serial port debug output and copied the data to the RX Mailbox  */
        pInstance->PendingRxMBox = 2;  
        pInstance->TxMBox = 1;
        pInstance->PendingDMAModeIndex = 0;   
            /* set IRQ state , the handler will expect a TX DMA interrupt */
        pInstance->IrqProcState = ATH_IRQ_TXDMA_PENDING;   
        pInstance->pUseBuffer = gDataBuffer;  
        
        status = MboxTx(pInstance, 
                        pInstance->TxMBox,     /* TX MBOX */
                        pInstance->PendingRxMBox,     /* RX MBOX */
                        pInstance->pUseBuffer,
                        pInstance->DMATests[pInstance->PendingDMAModeIndex].MinDataSize,        /* bytes */
                        pInstance->DataSeed,    /* seed */
                        pInstance->DMATests[pInstance->PendingDMAModeIndex].DmaMode);  
        if (!SDIO_SUCCESS(status)){
            break;    
        }
    } while (FALSE);
        
    return status;
}

/* sample interrupt handler */
void IRQHandler(PVOID pContext)  
{
    PSAMPLE_FUNCTION_INSTANCE pInstance;
    SDIO_STATUS status;
    UINT32      statusErrs = 0;
    UINT16      nextBytes;
    UINT32      offset;
    BOOL        statestay;
    PDMA_TEST_ENTRY pDmaTest;
     
    DBG_PRINT(ATH_IRQ, ("+SDIO Sample Function: ***IRQHandler\n"));
    
    pInstance = (PSAMPLE_FUNCTION_INSTANCE)pContext;
   
    pDmaTest = &pInstance->DMATests[pInstance->PendingDMAModeIndex];
   
    do {
   
        if (pInstance->ShutDown) {
            status = SDIO_STATUS_ERROR;
            break; 
        }    
#if 0
       
    // TODO , these registers are largely undocumented and they do not
    // give correct results,  
    BOOL        spiIrq = FALSE;
            /* read host interrupt status */   
        status = PioReg(pInstance, 
                        ATH_HOST_INT_STATUS_REG,
                        ATH_TRANS_PIO_16 | ATH_TRANS_EXT_ADDR,
                        &statusErrs,
                        PIO_READ); 
                    
        if (!SDIO_SUCCESS(status)) {
            break;   
        }    
                       
        DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: HOST_INT_STATUS: 0x%X\n", 
                    statusErrs));
       
        if (statusErrs & ATH_HOST_INT_STATUS_ERROR) {
                /* get the error status register */
            status = PioReg(pInstance, 
                            ATH_HOST_INT_ERR_STATUS_REG,
                            ATH_TRANS_PIO_16 | ATH_TRANS_EXT_ADDR,
                            &statusErrs,
                            PIO_READ); 
                            
            if (!SDIO_SUCCESS(status)) {
                break;   
            }  
            
            DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: ERROR_INT_STATUS: 0x%X\n", 
                    statusErrs));  
                /* is this a SPI attention interrupt? */        
            if (statusErrs & ATH_HOST_INT_ERR_STATUS_SPI) {
                spiIrq = TRUE;   
                DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: ATTN IRQ PENDING..\n"));       
            }         
        }
 
      
#endif  
         
            /* get the status so we can figure out which events to clear */
        status = PioReg(pInstance, 
                        ATH_SPI_STATUS_REG,
                        ATH_TRANS_PIO_16 | ATH_TRANS_INT_ADDR,
                        &statusErrs,
                        PIO_READ); 
                                   
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
        statusErrs &= (ATH_SPI_STATUS_DMA_OVER | ATH_SPI_STATUS_ERRORS);
        
        DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: SPI_STATUS_REG: 0x%X\n", 
                    statusErrs));   
         
        if (statusErrs & ATH_SPI_STATUS_DMA_OVER) {
            DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: DMA COMPLETE..\n"));  
        }
               
        if (statusErrs & ATH_SPI_STATUS_ERRORS) {            
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function, Error Status : 0x%X \n",
                    statusErrs));    
        }  
        
        if (statusErrs) {
                /* clear status */
            DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: Clearing status: 0x%X \n",statusErrs)); 
            status = PioReg(pInstance, 
                            ATH_SPI_STATUS_REG,
                            ATH_TRANS_PIO_16 | ATH_TRANS_INT_ADDR,
                            &statusErrs,
                            PIO_WRITE);            
            if (!SDIO_SUCCESS(status)) {
                DBG_ASSERT(FALSE);
                return;   
            } else {
                DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: Cleared Status\n"));   
            }
        }   
        
    } while (FALSE);
    
    statestay = TRUE;
    
    while (statestay) {
        statestay = FALSE;
        switch (pInstance->IrqProcState) {
         
            case ATH_IRQ_TXDMA_PENDING:
                DBG_ASSERT(pInstance->PendingRxBytes != 0);
                if (!(statusErrs & ATH_SPI_STATUS_DMA_OVER) && 
                    !(pDmaTest->DmaMode & ATH_TRANS_DMA_PSEUDO)) {
                    DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO Sample Function, ATH_IRQ_TXDMA_PENDING, Unknown status from IRQ: 0x%X \n",
                            statusErrs)); 
                    status = SDIO_STATUS_DEVICE_ERROR; 
                    break;
                }
                DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: ATH_IRQ_TXDMA_PENDING Delaying..\n"));  
                /* delay a little because the SD11 ping application may not have copied
                 * the data to the mail box yet, its probably outputing through the
                 * serial port right now */          
                OSSleep(RXCHECKDELAY); 
                DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: Issuing RX...\n")); 
                status = MboxRx(pInstance,
                            pInstance->PendingRxMBox,
                            pInstance->pUseBuffer,
                            pInstance->PendingRxBytes,
                            pDmaTest->DmaMode, 
                            TRUE);  
                            
                if (!SDIO_SUCCESS(status)) {
                    break;   
                }
                    /* this receive operation will generate a DMA complete interrupt */                
                pInstance->IrqProcState = ATH_IRQ_RXDMA_PENDING;
                if (pDmaTest->DmaMode & ATH_TRANS_DMA_PSEUDO) {
                    statestay = TRUE;    
                }
                break;
            case ATH_IRQ_RXDMA_PENDING:
                if (!(statusErrs & ATH_SPI_STATUS_DMA_OVER) &&
                    !(pDmaTest->DmaMode & ATH_TRANS_DMA_PSEUDO)) {
                    DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO Sample Function, ATH_IRQ_RXDMA_PENDING, Unknown status from IRQ: 0x%X \n",
                            statusErrs)); 
                    status = SDIO_STATUS_DEVICE_ERROR;  
                    break;
                }
                    /* set up next transfer */
                pInstance->DataSeed++;  
                pInstance->PendingDMAModeIndex = pInstance->DataSeed % 4;
                pDmaTest = &pInstance->DMATests[pInstance->PendingDMAModeIndex];              
                pDmaTest->TestCount++;
                pDmaTest->TestCount &= 0x1F;
                offset = pDmaTest->TestCount;
#ifdef TEST_SIZE_INCR
                nextBytes = pDmaTest->MinDataSize + pDmaTest->TestCount;
#else
                nextBytes = pDmaTest->MinDataSize;
#endif                      
#ifdef ALIGN_BUFFERS 
                pInstance->pUseBuffer = gDataBuffer; 
#else             
                pInstance->pUseBuffer = gDataBuffer + offset;  
#endif            
                DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: Issuing new TX...\n")); 
                status = MboxTx(pInstance,  
                                pInstance->TxMBox,     
                                pInstance->PendingRxMBox,  
                                pInstance->pUseBuffer,   
                                nextBytes,    
                                pInstance->DataSeed,    /* seed */
                                pDmaTest->DmaMode);  
               
                if (!SDIO_SUCCESS(status)) {
                    break;   
                }                 
                pInstance->IrqProcState = ATH_IRQ_TXDMA_PENDING;
                break;
            default:
                status = SDIO_STATUS_DEVICE_ERROR;  
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO Sample Function, ATH_IRQ_RXDMA_PENDING, Unknown status from IRQ: 0x%X \n",
                            statusErrs)); 
                break; 
        }
    }
    
    if (!SDIO_SUCCESS(status)) {
         pInstance->IrqProcState = ATH_IRQ_NONE;    
    } else {
        UINT32 temp;
        
        DBG_PRINT(ATH_IRQ, ("SDIO Sample Function: Next State: %d \n",pInstance->IrqProcState)); 
        SDLIB_IssueConfig(pInstance->pDevice,SDCONFIG_FUNC_ACK_IRQ,NULL,0);
            /* TODO, this should be done in the HCD driver since the ACK could be asynchronous!
             * It was temporarily removed for debugging.
             * the OMAP GPIO is edge detect only so we need to mask/unmask the IRQ so we can detect
             * interrupts properly */
        temp = 0;
        
        status = PioReg(pInstance, 
                        ATH_SPI_CONFIG_REG,
                        ATH_TRANS_DS_8 | ATH_TRANS_INT_ADDR,
                        &temp,
                        PIO_WRITE); 
        
        if (!SDIO_SUCCESS(status)) {
            return;  
        } 
        
        temp = ATH_SPI_CONFIG_INTR;
        
        status = PioReg(pInstance, 
                        ATH_SPI_CONFIG_REG,
                        ATH_TRANS_DS_8 | ATH_TRANS_INT_ADDR,
                        &temp,
                        PIO_WRITE);
             
    }
    DBG_PRINT(ATH_IRQ, ("-SDIO Sample Function: ***IRQHandler\n")); 
}

/* delete an instance  */
void DeleteSampleInstance(PSAMPLE_FUNCTION_CONTEXT   pFuncContext,
                          PSAMPLE_FUNCTION_INSTANCE  pInstance)
{
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Sample Function: DeleteSampleInstance\n")); 
    pInstance->ShutDown = TRUE; 
    SDLIB_IssueConfig(pInstance->pDevice, SDCONFIG_FUNC_MASK_IRQ, NULL, 0);    
                               
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pFuncContext->InstanceSem))) {
        return; 
    }
        /* pull it out of the list */
    SDListRemove(&pInstance->SDList);
    SemaphorePost(&pFuncContext->InstanceSem);                   
    SemaphoreDelete(&pInstance->IOComplete);
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Sample Function: DeleteSampleInstance\n")); 
}

/* find an instance associated with the SD device */
PSAMPLE_FUNCTION_INSTANCE FindSampleInstance(PSAMPLE_FUNCTION_CONTEXT pFuncContext,
                                             PSDDEVICE                pDevice)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PSAMPLE_FUNCTION_INSTANCE pInstance = NULL;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, SAMPLE_FUNCTION_INSTANCE, SDList);
        if (pInstance->pDevice == pDevice) {
                /* found it */
            break;   
        }
        pInstance = NULL;  
    }    
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pInstance;
}

/* find an instance associated with an index */
PSAMPLE_FUNCTION_INSTANCE FindSampleInstanceByIndex(PSAMPLE_FUNCTION_CONTEXT pFuncContext,
                                                    UINT                     Index)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PSAMPLE_FUNCTION_INSTANCE pInstance = NULL;
    UINT ii = 0;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, SAMPLE_FUNCTION_INSTANCE, SDList);
        if (ii == Index) {
                /* found it */
            break;   
        }
        pInstance = NULL;
        ii++;  
    }    
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pInstance;
}

/* add and instance to our list */
SDIO_STATUS AddSampleInstance(PSAMPLE_FUNCTION_CONTEXT   pFuncContext,
                              PSAMPLE_FUNCTION_INSTANCE  pInstance)
{
    SDIO_STATUS status;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return status; 
    }
  
    SDListAdd(&pFuncContext->InstanceList,&pInstance->SDList);  
    SemaphorePost(&pFuncContext->InstanceSem);
    
    return SDIO_STATUS_SUCCESS;
}

/* cleanup the function context */
void CleanupFunctionContext(PSAMPLE_FUNCTION_CONTEXT pFuncContext)
{
    SemaphoreDelete(&pFuncContext->InstanceSem);  
}

/* initialize the function context */
SDIO_STATUS InitFunctionContext(PSAMPLE_FUNCTION_CONTEXT pFuncContext)
{
    SDIO_STATUS status;
    SDLIST_INIT(&pFuncContext->InstanceList); 
   
    status = SemaphoreInitialize(&pFuncContext->InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return status;
    }
    
    return SDIO_STATUS_SUCCESS;
}

void AsyncComplete(struct _SDREQUEST *pRequest)
{
    PSAMPLE_FUNCTION_INSTANCE pInstance = (PSAMPLE_FUNCTION_INSTANCE)pRequest->pCompleteContext;
   
    DBG_PRINT(ATH_BUS_REQ, ("SDIO Sample Function: AsyncComplete Status:%d \n",
            pRequest->Status)); 
    if (!SDIO_SUCCESS(pRequest->Status)) {
         DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: AsyncComplete Transfer Failed:%d \n",
            pRequest->Status));  
    } 
    pInstance->LastRequestStatus = pRequest->Status;
    SemaphorePost(&pInstance->IOComplete);                 
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);             
}

PTEXT GetTypeString(ATH_TRANS_CMD Type)
{
    if (ATH_SPI_ADDRESS_EXTERNAL(Type)) {
        switch (ATH_GET_TRANS_DS(Type)) {
            case ATH_TRANS_DS_8:
                return "8 bit External";
            case ATH_TRANS_DS_16:
                return "16 bit External";
            case ATH_TRANS_DS_32:
                return "32 bit External";
            default:
                DBG_ASSERT(FALSE);
                return "unknown";
        } 
    }
    
    switch (ATH_GET_TRANS_DS(Type)) {
        case ATH_TRANS_DS_8:
            return "8 bit Internal";
        case ATH_TRANS_DS_16:
            return "16 bit Internal";
        case ATH_TRANS_DS_32:
            return "32 bit Internal";
        default:
            DBG_ASSERT(FALSE);
            return "unknown";
    } 
}

SDIO_STATUS PioReg(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                   UINT16                    Address,
                   ATH_TRANS_CMD             Type,
                   UINT32                    *pData,
                   BOOL                      Read) 
{
    SDIO_STATUS status;
    PSDREQUEST  pReq = NULL;
    
    do {
            /* allocate request to send to host controller */
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        
        if (NULL == pReq) {
            status = SDIO_STATUS_NO_RESOURCES;    
            break;
        }
         
        if (Read) {
            ATH_SET_PIO_READ_OPERATION(pReq,Type,Address);
        } else {
            ATH_SET_PIO_WRITE_OPERATION(pReq,Type,Address,*pData);
        }
        
          /* set flags for raw mode, this is a SYNC request */
        pReq->Flags = SDREQ_FLAGS_RAW;
        /* TODO, for ASYNC, set SDREQ_FLAGS_TRANS_ASYNC, and provide completion routine:
         *  pReq->pCompletion = AsyncComplete; 
         * pReq->pCompleteContext = (PVOID)pInstance; */
         
        DBG_PRINT(ATH_BUS_REQ, ("SDIO Sample Function:PIO Type:%s, Dir:%s \n",
              GetTypeString(Type), Read ? "Read" : "Write"));
            /* submit */
        status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
        
        if (SDIO_SUCCESS(status)) {
            if (Read) {
                *pData = ATH_GET_PIO_READ_RESULT32(pReq); 
            }   
        } else {
            if (status == SDIO_STATUS_DEVICE_ERROR) {
                /* the device returned an error status during the status phase */
                 DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Type:%s  dir:%s, Address:0x%X, Failed : Device Status:0x%X \n",
                  GetTypeString(Type), Read ? "Read" : "Write",Address, ATH_GET_STATUS_RESULT(pReq))); 
            } else {
                 DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Type:%s  dir:%s, Address:0x%X, Failed : %d\n",
                  GetTypeString(Type), Read ? "Read" : "Write", Address, status));    
            }
        }
    } while (FALSE); 
    
    if (pReq != NULL) {
        SDDeviceFreeRequest(pInstance->pDevice, pReq);       
    }
    
    return status;
}
 
SDIO_STATUS CopyBuffer(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                       UINT16 Address,
                       ATH_TRANS_CMD  Type,
                       PVOID  pBuffer,
                       UINT16 Bytes,
                       BOOL   Read)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;   
    PUINT8 pData = (PUINT8)pBuffer;
    UINT32 data; 
    
    while (Bytes) {
        if (!Read) {
            switch ATH_GET_TRANS_DS(Type) {
                case ATH_TRANS_DS_8:
                    data = *pData; 
                    break; 
                case ATH_TRANS_DS_16:
                    data = *((PUINT16)pData);
                    break;
                case ATH_TRANS_DS_32:
                    data = *((PUINT32)pData); 
                    break; 
                default:
                    DBG_ASSERT(FALSE);
                    return SDIO_STATUS_INVALID_PARAMETER;
            }
        }
        
        status = PioReg(pInstance, 
                        Address,
                        Type,
                        &data,
                        Read ? PIO_READ : PIO_WRITE);  
                                
        if (!SDIO_SUCCESS(status)) { 
            break;   
        } 
        
        switch ATH_GET_TRANS_DS(Type) {
            case ATH_TRANS_DS_8:
                if (Read) {
                    *pData = (UINT8)data; 
                }
                Address++;
                Bytes--;   
                pData++;  
                break; 
            case ATH_TRANS_DS_16:
                if (Read) {
                    *((PUINT16)pData) = (UINT16)data; 
                }
                Address += 2;
                Bytes -= 2;   
                pData += 2; 
                break;
            case ATH_TRANS_DS_32:
                if (Read) {
                    *((PUINT32)pData) = data; 
                }
                Address += 4;
                Bytes -= 4;     
                pData += 4;
                break; 
            default:
                DBG_ASSERT(FALSE);
                return SDIO_STATUS_INVALID_PARAMETER;
        }
    }
    
    return status;
}

SDIO_STATUS TransferDMA(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                        UINT16 Address,
                        ATH_TRANS_CMD  Type,
                        PVOID  pBuffer,
                        UINT16 Bytes,
                        BOOL   Read)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16      bytesRemaining;
    
    bytesRemaining = Bytes; 
    
    if (!(ATH_GET_TRANS_TYPE(Type) == ATH_TRANS_DMA_16) && 
        !(ATH_GET_TRANS_TYPE(Type) == ATH_TRANS_DMA_32) && 
        !(ATH_GET_TRANS_TYPE(Type) == ATH_TRANS_DMA_8)) {
        DBG_ASSERT(FALSE);
        return SDIO_STATUS_INVALID_PARAMETER;    
    } 
    
    while (bytesRemaining) {
            /* allocate request to send to host controller */
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        
        if (NULL == pReq) {
            status = SDIO_STATUS_NO_RESOURCES;    
            break;
        }
            /* split up transaction based on maximum transfer per request */
        Bytes = min(bytesRemaining,pInstance->MaxBytesPerDMA);
        
            /* build up custom request */
        ATH_SET_DMA_OPERATION(pReq,
                              Type,
                              Read ? ATH_TRANS_READ:  ATH_TRANS_WRITE,
                              Address, 
                              Bytes);
       
            /* set flags for raw mode and ASYNC (this could be SYNC as well) */
        pReq->Flags = SDREQ_FLAGS_RAW  | SDREQ_FLAGS_TRANS_ASYNC;
            /* set buffer to send */
        pReq->pDataBuffer = pBuffer;
            /* set completion routine */       
        pReq->pCompletion = AsyncComplete; 
        pReq->pCompleteContext = (PVOID)pInstance;
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: %d bytes , mode:%s  dir:%s \n",
            Bytes,GetTypeString(Type), Read ? "Read" : "Write"));
            /* submit */
        SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
            /* wait for completion */
        SemaphorePend(&pInstance->IOComplete);          
        status = pInstance->LastRequestStatus;  
        if (!SDIO_SUCCESS(status)){
            break;    
        } 
        bytesRemaining -= Bytes;
        pBuffer += Bytes; 
        Address += Bytes;         
    }
    
    return status;
}

#define MBOX_HEADER_BYTES   5
#define MBOX_PNG_HDR_BYTES  3
#define MBOX_HTC_HDR_BYTES  (MBOX_HEADER_BYTES - MBOX_PNG_HDR_BYTES)

SDIO_STATUS MboxTx(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                   UINT8                     TxMbox,
                   UINT8                     RxMbox,
                   PUINT8                    pBuffer,
                   UINT16                    NumBytes,
                   UINT8                     DataSeed,
                   ATH_TRANS_CMD             DMAMode)
{
    SDIO_STATUS status = SDIO_STATUS_INVALID_PARAMETER;
    INT i;
    UINT16 totalBytes,address;
            
    do {
       
        if (0 == NumBytes) {
            DBG_ASSERT(FALSE);
            break;     
        } 
       
        if ((TxMbox >= MAX_MBOXES) || (RxMbox >= MAX_MBOXES)) {
            DBG_ASSERT(FALSE);
            break;    
        }          
          
        if (TxMbox == RxMbox) {
            DBG_ASSERT(FALSE);
            break;        
        }
        
        totalBytes = NumBytes + MBOX_HEADER_BYTES;

#ifdef ALIGN_TRANSFERS        
            /* must align total bytes */
        if (ATH_GET_TRANS_DS(DMAMode) == ATH_TRANS_DS_16) {
            if (totalBytes & 1) {
                    /* round up to nearest WORD */
                totalBytes++;    
            }  
              
        } else if (ATH_GET_TRANS_DS(DMAMode) == ATH_TRANS_DS_32) {    
            if (totalBytes & 3) {
                    /* round up to nearest DWORD */
                totalBytes += 4;
                totalBytes &= ~0x3;    
            }   
        } 
#endif
        
            /* make sure we don't exceed our buffer */
        if (totalBytes > DATA_BUFFER_BYTES) {
            DBG_ASSERT(FALSE);
            break; 
        }
        
        if (totalBytes > MBOX_SIZE) {
            DBG_ASSERT(FALSE);
            break;   
        }
        
            /* get the new payload byte count */        
        NumBytes = totalBytes - MBOX_HEADER_BYTES;
                   
            /* create data pattern in payload */     
        for (i = 0; i < NumBytes; i++) {
           pBuffer[MBOX_HEADER_BYTES + i] = (UCHAR)i + DataSeed;   
           VerifyBuffer[MBOX_HEADER_BYTES + i] = (UCHAR)i + DataSeed; 
        }
        
        totalBytes = (NumBytes + MBOX_PNG_HDR_BYTES);
        pBuffer[0] =  (UINT8)totalBytes;
        pBuffer[1] =  (UINT8)(totalBytes >> 8); 
        pBuffer[2] = RxMbox;  
        pBuffer[3] = 0;  /* filled in by target */
        pBuffer[4] = 0;  /* filled in by target */   
               
            /* add in HTC header */
        totalBytes += MBOX_HTC_HDR_BYTES;
        
            /* set up address so that last byte falls on the EOM address */
        address = GET_MBOX_ADDRESS(TxMbox) + (MBOX_SIZE - totalBytes);
        
        DBG_PRINT(SDDBG_TRACE,
            ("SDIO Sample Function: MboxTx  TxMbox:%d, RxMbox:%d, Mbox Address:0x%X (ending:0x%X), Payload Bytes: %d, Total Bytes: %d Buffer:0x%X\n",
            TxMbox, RxMbox, address, (address + totalBytes - 1), NumBytes, totalBytes, (UINT32)pBuffer));
       
        if (DBG_GET_DEBUG_LEVEL() >= ATH_BUS_REQ) { 
            SDLIB_PrintBuffer(pBuffer, 5, "Sample Function MBOX Ping TX Header");
        }
    
            /* set expected bytes for IRQ */
        pInstance->PendingRxBytes = totalBytes;
        
        status =  TransferDMA(pInstance, 
                              address,
                              DMAMode,
                              pBuffer,
                              (UINT16)totalBytes,
                              FALSE);
      
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to write to mailbox: %d, \n",status));
            break;   
        }
         
        
    } while (FALSE);
 
    return status;
}

SDIO_STATUS MboxRx(PSAMPLE_FUNCTION_INSTANCE pInstance,
                   UINT8                     RxMbox,
                   PUINT8                    pBuffer,
                   UINT16                    TotalBytes,
                   ATH_TRANS_CMD             DMAMode,
                   BOOL                      Verify)
{
    SDIO_STATUS status = SDIO_STATUS_INVALID_PARAMETER;
    INT i;
    UINT32 getBytes,rxBytes,payloadBytes,address;
        
    do {
        if (RxMbox >= MAX_MBOXES) {
            DBG_ASSERT(FALSE);
            break;    
        }             
        
        if (0 == TotalBytes) {        
                /* caller wants us to figure it out by reading the header */
                /* we need at least a header's worth */
            getBytes = MBOX_HEADER_BYTES;
        } else {
            getBytes = TotalBytes;   
        }
        
        memset(pBuffer, 0, getBytes);
        
#ifdef ALIGN_TRANSFERS          
         if (ATH_GET_TRANS_DS(DMAMode) == ATH_TRANS_DS_16) {
            if (getBytes & 1) {
                    /* round up to nearest WORD */
                getBytes++;    
            }    
        } else if (ATH_GET_TRANS_DS(DMAMode) == ATH_TRANS_DS_32) {  
            if (getBytes & 3) {
                    /* round up to nearest DWORD */
                getBytes += 4;
                getBytes &= ~0x3;    
            }   
        }
#endif
        
        address = GET_MBOX_ADDRESS(RxMbox);
          
            /* get the header plus whatever we can read in */               
        status =  TransferDMA(pInstance, 
                              address,
                              DMAMode,
                              pBuffer,
                              getBytes,
                              TRUE);
                         
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, 
                ("SDIO Sample Function: failed to get header from mailbox:%d, address:0x%X  %d, \n",
                    RxMbox,address,status));          
            break;   
        } 
        
        if (DBG_GET_DEBUG_LEVEL() >= ATH_BUS_REQ) {
            SDLIB_PrintBuffer(pBuffer, MBOX_HEADER_BYTES, "Sample Function MBOX Ping RX Header");
        }
      
        rxBytes = (pBuffer[1] << 8) | pBuffer[0];  
      
        if (TotalBytes != 0) { 
                /* if the caller supplied the total packet length, it better add up */ 
            if (TotalBytes != (rxBytes + 2)) {
                DBG_ASSERT(FALSE);
                status = SDIO_STATUS_INVALID_PARAMETER;
                SDLIB_PrintBuffer(pBuffer, getBytes, "Error: Sample Function MBOX Ping RX Header");
                break; 
            }
        }
            /* the payload is this value minus the PNG header */
        payloadBytes = rxBytes - MBOX_PNG_HDR_BYTES;
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Rx Packet: RxMbox:%d, Address:0x%X, Pkt Bytes: %d payload bytes:%d (recv buffer:0x%X)\n",
            RxMbox, address, rxBytes,payloadBytes,(UINT32)pBuffer));  
            
        if (rxBytes == 0) { 
            SDLIB_PrintBuffer(pBuffer, getBytes, "Error: Sample Function MBOX Ping RX Header");
            status = SDIO_STATUS_INVALID_PARAMETER;
            DBG_ASSERT(FALSE); 
            break; 
        } 
        
        if (rxBytes > MBOX_SIZE) {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_INVALID_PARAMETER;
            SDLIB_PrintBuffer(pBuffer, getBytes, "Error: Sample Function MBOX Ping RX Header");
            break;  
        }
        
            /* take off what we already retrieved , note: the 2 byte length is not
             * calculated as part of the payload + PNG_HDR length */
        rxBytes -= (getBytes - 2);  
        
        if (rxBytes) {
                /* get the payload */     
            status =  TransferDMA(pInstance, 
                                 (address + getBytes),
                                 DMAMode,
                                 (pBuffer + getBytes),
                                 rxBytes, 
                                 TRUE);
                             
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO Sample Function: failed to get rest of payload from mailbox:%d, address:0x%X  %d, \n",
                        RxMbox,address,status));          
                break;   
            } 
        }
        
        if (!Verify) {
            break; 
        }
        
        if (DBG_GET_DEBUG_LEVEL() >= ATH_BUS_REQ) {
            SDLIB_PrintBuffer(pBuffer,payloadBytes + MBOX_HEADER_BYTES,"pBuffer RX Received");
        }
            /* verify payload */ 
        for (i = MBOX_HEADER_BYTES; i < (MBOX_HEADER_BYTES + payloadBytes); i++) {
            if (VerifyBuffer[i] != pBuffer[i]) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: RX payload error at %d, Got:0x%X, Expecting:0x%X\n",
                i, pBuffer[i],VerifyBuffer[i]));
                SDLIB_PrintBuffer(pBuffer,payloadBytes + MBOX_HEADER_BYTES,"Data Buffer");
                SDLIB_PrintBuffer(VerifyBuffer,payloadBytes + MBOX_HEADER_BYTES,"VerifyBuffer");
                status = SDIO_STATUS_ERROR;
                break;  
            }  
        } 
        
        if (i >= (MBOX_HEADER_BYTES + payloadBytes)) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: RX ping read-back success! \n")); 
        }
      
    } while (FALSE);
 
    return status;
}
