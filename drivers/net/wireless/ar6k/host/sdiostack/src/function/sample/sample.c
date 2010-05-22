// Copyright (c) 2004-2006 Atheros Communications Inc.
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
@file: sample.c

@abstract: SDIO Sample Function driver

#notes: OS independent portions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME  SDSAMPLEFD
#include "../../include/ctsystem.h"

#include "../../include/sdio_busdriver.h"
#include "../../include/_sdio_defs.h"
#include "../../include/sdio_lib.h"

#include "sample.h"
        
#define NUM_ASYNC_REQ 5


/* test parameters setup in OS dependent code */
extern int testb;
extern int testbpb;
extern int testusedma;

SDIO_STATUS ArrayBlockTest_ByteBasis(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                              UINT32 Address,
                              PVOID  pBuffer,
                              UINT32 Bytes,
                              BOOL   Read);
SDIO_STATUS ArrayBlockTest_BlockBasis(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                              UINT32 Address,
                              PVOID  pBuffer,
                              PSDDMA_DESCRIPTOR pSGList,
                              UINT   SGcount,
                              UINT32 Blocks,
                              UINT32 BytesPerBlock,
                              BOOL Read);
                              
SDIO_STATUS DoBlockTest(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                        UINT32 Address,
                        INT Blocks, 
                        INT BlocksPerByte);
                        
SDIO_STATUS PutArraySync(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs);
SDIO_STATUS GetArraySync(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs);
void IRQHandler(PVOID pContext);
 
        
/* create a sample instance */
SDIO_STATUS InitializeInstance(PSAMPLE_FUNCTION_CONTEXT  pFuncContext,
                               PSAMPLE_FUNCTION_INSTANCE pInstance, 
                               PSDDEVICE                 pDevice)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    SDCONFIG_FUNC_SLOT_CURRENT_DATA   slotCurrent;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    struct SDIO_FUNC_EXT_FUNCTION_TPL_1_1 funcTuple;
    UINT32          nextTpl;
    UINT8           tplLength;
    ZERO_OBJECT(fData);
    ZERO_OBJECT(slotCurrent);
        
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
        DBG_PRINT(SDDBG_TRACE, (" Card Flags:   0x%X \n",SDDEVICE_GET_CARD_FLAGS(pDevice)));     
        DBG_PRINT(SDDBG_TRACE, (" Card RCA:     0x%X \n",SDDEVICE_GET_CARD_RCA(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper Clock:   %d Hz \n",SDDEVICE_GET_OPER_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max Clock:    %d Hz \n",SDDEVICE_GET_MAX_CLOCK(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlklenLim:  %d bytes \n",SDDEVICE_GET_OPER_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  BlkLen:     %d bytes\n",SDDEVICE_GET_MAX_BLOCK_LEN(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Oper BlksLim:    %d blocks per trans \n",SDDEVICE_GET_OPER_BLOCKS(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Max  Blks:       %d blocks per trans \n",SDDEVICE_GET_MAX_BLOCKS(pDevice)));
        DBG_PRINT(SDDBG_TRACE, (" Slot Voltage Mask:  0x%X \n",SDDEVICE_GET_SLOT_VOLTAGE_MASK(pDevice)));
                
                
        /* allocate buffers to perform I/O on */                
        status = SampleAllocateBuffers(pInstance);
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample: Failed to buffers: %d\n", status));
            break;    
        }        
                        
        if (pFuncContext->ClockOverride != 0) { 
                 
            SDCONFIG_BUS_MODE_DATA  busSettings;
            ZERO_OBJECT(busSettings);
            busSettings.BusModeFlags = SDDEVICE_GET_BUSMODE_FLAGS(pInstance->pDevice);
            busSettings.ClockRate = pFuncContext->ClockOverride;  // adjust clock 
            status = SDLIB_IssueConfig(pDevice,
                                       SDCONFIG_FUNC_CHANGE_BUS_MODE,
                                       &busSettings,
                                       sizeof(SDCONFIG_BUS_MODE_DATA));
            if (!SDIO_SUCCESS(status)) {
                break;   
            } 
            DBG_PRINT(SDDBG_WARN, (" New Oper Clock:   %d Hz \n",SDDEVICE_GET_OPER_CLOCK(pDevice)));
            
        }
 
        if ((SDDEVICE_GET_CARD_FLAGS(pDevice) & CARD_SDIO) &&
            (SDDEVICE_GET_SDIO_FUNCNO(pDevice) != 0)) {
            DBG_PRINT(SDDBG_TRACE, (" SDIO Func:  %d\n",SDDEVICE_GET_SDIO_FUNCNO(pDevice)));
            DBG_PRINT(SDDBG_TRACE, (" CIS PTR:    0x%X \n",SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice)));
            DBG_PRINT(SDDBG_TRACE, (" CSA PTR:    0x%X \n",SDDEVICE_GET_SDIO_FUNC_CSAPTR(pDevice)));
        }      
        
        /* allocate slot current (in mA), the value should be read from the FUNC Extension TPL */
        /* if the slot current must be adjusted based on voltage, check the Slot voltage Mask
         * using SDDEVICE_GET_SLOT_VOLTAGE_MASK().  The actual current is hardware specific.
         * For SDIO 1.10 or greater, the current value should be the MAX operational power value in the
         * FUNCE TPL since SDIO functions default to high current mode when they are enabled.  */
         
        /* NOTE: you can also use the SDLIB_GetDefaultOpCurrent() function which 
         * performs the tuple query and parsing automatically and returns the default current.
         *   EXAMPLE: 
         *          status = SDLIB_GetDefaultOpCurrent(pDevice, &slotCurrent.SlotCurrent); */
         
        nextTpl = SDDEVICE_GET_SDIO_FUNC_CISPTR(pDevice);
        tplLength = sizeof(funcTuple); 
            /* go get the function Extension tuple */
        status = SDLIB_FindTuple(pDevice,
                                 CISTPL_FUNCE,
                                 &nextTpl,
                                 (PUINT8)&funcTuple,
                                 &tplLength);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample: Failed to get FuncE Tuple: %d \n", status));
                /* pick some reason number */
            slotCurrent.SlotCurrent = 200;   
        }      
        
        if (0 == slotCurrent.SlotCurrent) {  
                /* use the operational power (8-bit) value of current in mA as default*/
            slotCurrent.SlotCurrent = funcTuple.CommonInfo.OpMaxPwr;            
            if (tplLength > sizeof(funcTuple.CommonInfo)) {
                    /* we have a 1.1 tuple */         
                DBG_PRINT(SDDBG_TRACE, ("SDIO Sample: 1.1 Tuple Found \n"));  
                     /* check for HIPWR mode */
                if (SDDEVICE_GET_CARD_FLAGS(pDevice) & CARD_HIPWR) {        
                        /* use the maximum operational power (16 bit ) from the tuple */
                    slotCurrent.SlotCurrent = CT_LE16_TO_CPU_ENDIAN(funcTuple.HiPwrMaxPwr); 
                }
            }
        }
        
        if (slotCurrent.SlotCurrent == 0) {
            DBG_PRINT(SDDBG_WARN, ("SDIO Sample: FUNCE tuple indicates greater than 200ma OpMaxPwr current! \n"));
                /* try something higher than 200ma */
            slotCurrent.SlotCurrent = 300;  
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample: Allocating Slot current: %d mA\n", slotCurrent.SlotCurrent));         
        status = SDLIB_IssueConfig(pDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Remaining Slot Current: %d mA\n",
                                    slotCurrent.SlotCurrent));  
            }
            break; 
        }
        
        /* enable the card */
        fData.EnableFlags = SDCONFIG_ENABLE_FUNC;
        fData.TimeOut = 500;
        status = SDLIB_IssueConfig(pDevice,
                                   SDCONFIG_FUNC_ENABLE_DISABLE,
                                   &fData,
                                   sizeof(fData));
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: Initialize, failed to enable function %d\n",
                                    status));
            break;
        }
        
            
        if (SDDEVICE_IS_SDIO_REV_GTEQ_1_10(pDevice)) {
            /* if the card is 1.10 we can check the SPS bit and optionally
             * set for low current mode. Uncomment out the following if
             * this is required */ 
            /* 
            UCHAR temp;
            UINT32 address = FBR_FUNC_POWER_SELECT_OFFSET(
                                CalculateFBROffset(SDDEVICE_GET_SDIO_FUNCNO(pDevice)));  
            status = Cmd52ReadByteCommon(pDevice,
                                         address,
                                         &temp);                                  
            if (!SDIO_SUCCESS(status)) {
                 DBG_PRINT(SDDBG_ERROR, ("SDIO Sample: Failed to get power select, Err:%d", status));
                 break;   
            }  
             
            if (temp & FUNC_POWER_SELECT_SPS) {
                temp |= FUNC_POWER_SELECT_EPS;
                status = Cmd52WriteByteCommon(pDevice,
                                          address,
                                          &temp);                                  
                 if (!SDIO_SUCCESS(status)) {
                     DBG_PRINT(SDDBG_ERROR, ("SDIO Sample: Failed to set power select, Err:%d", status));
                     break;   
                 }       
            } 
            */
        }
        
        /* @TODO: add specific card initialization here */
        
        
        /* @TODO: register for interrupt handler if required */
            /* set our IRQ handler, passing into it our device as the context  */
        SDDEVICE_SET_IRQ_HANDLER(pDevice, IRQHandler, pDevice);
        /*     ....unmask our interrupt on the card */
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: unmasking IRQ \n"));
        status = SDLIB_IssueConfig(pDevice, SDCONFIG_FUNC_UNMASK_IRQ, NULL, 0);  
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed to unmask IRQ %d\n",
                                    status));
        }       
       
        /* do a block test on driver load if requested */
        if ((testb > 0) && (testbpb > 0)) {       
            status  = DoBlockTest(pInstance, 
                        		  0,
                                  testb, 
                                  testbpb);
            DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function:: block test:  %d, \n",status));          
        }
                    
    } while (FALSE);
        
    return status;
}

/* sample interrupt handler */
void IRQHandler(PVOID pContext) 
{
    PSDDEVICE pDevice;
    SDIO_STATUS   status = SDIO_STATUS_DEVICE_ERROR;
   
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: ***IRQHandler\n"));
    pDevice = (PSDDEVICE)pContext; 
    status = SDLIB_IssueConfig(pDevice,SDCONFIG_FUNC_ACK_IRQ,NULL,0);  
}
/* 
 * AsyncPutByteLastCompletion - I/O completion routine for asynchronous put of last byte
*/
void AsyncPutByteLastCompletion(struct _SDREQUEST *pRequest)
{
    PSAMPLE_FUNCTION_INSTANCE pInstance = (PSAMPLE_FUNCTION_INSTANCE)pRequest->pCompleteContext;
   
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: AsyncPutByteCompletion, Last Status:%d \n",
            pRequest->Status)); 
    pInstance->LastRequestStatus = pRequest->Status; 
    SemaphorePost(&pInstance->IOComplete);              
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);
    
}

/* 
 * AsyncPutByteCompletion - I/O completion routine for asynchronous put byte
*/
void AsyncPutByteCompletion(struct _SDREQUEST *pRequest)
{
    PSAMPLE_FUNCTION_INSTANCE pInstance = (PSAMPLE_FUNCTION_INSTANCE)pRequest->pCompleteContext;
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: AsyncPutByteCompletion Status:%d\n",
           pRequest->Status));
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);
}

/*
 * DoAsyncPutByte - perform asynchronous byte write
*/
SDIO_STATUS DoAsyncPutByte(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs) 
{
    SDIO_STATUS  status = SDIO_STATUS_SUCCESS;
    INT          i;
    PSDREQUEST   requestArray[NUM_ASYNC_REQ];
    
    memset(requestArray, 0, sizeof(requestArray));
    do {

            /* allocate and build up the requests */   
        for (i = 0; i < NUM_ASYNC_REQ; i++) {
            requestArray[i] = SDDeviceAllocRequest(pInstance->pDevice);
            if (NULL == requestArray[i]) {
                status = SDIO_STATUS_NO_RESOURCES; 
                break;
            }
                /* setup CMD52 arguments */
            SDLIB_SetupCMD52RequestAsync(SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                         pArgs->Register,
                                         TRUE,
                                         *pArgs->pBuffer,                                   
                                         requestArray[i]);
            if (i == (NUM_ASYNC_REQ - 1)) {
                    /* the last one signals the semaphore */
                requestArray[i]->pCompletion = AsyncPutByteLastCompletion;
                requestArray[i]->pCompleteContext = (PVOID)pInstance;  
            } else {  
                    /* setup completion routine */                             
                requestArray[i]->pCompletion = AsyncPutByteCompletion; 
                requestArray[i]->pCompleteContext = (PVOID)pInstance;  
            }                                       
        }
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
            /* now send the requests down rapidly */
        for (i = 0; i < NUM_ASYNC_REQ; i++) {
            SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,requestArray[i]);      
        }
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: DoAsyncPutByte : waiting on semaphore..\n"));
            /* wait for completion */
        SemaphorePend(&pInstance->IOComplete);    
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: DoAsyncPutByte : woke up! \n"));
            /* just get the completion status from the last request */    
        status = pInstance->LastRequestStatus;
    } while (FALSE);
    
    return status;
}

/* PutByte - use CMD52 to write a byte */
SDIO_STATUS PutByte(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    switch (pArgs->TestIndex) {
        case 1:
            {                
                PSDREQUEST pRequest;
                INT count = 5;  
                pRequest = SDDeviceAllocRequest(pInstance->pDevice);
                if (NULL == pRequest) {
                    status = SDIO_STATUS_NO_RESOURCES;
                    break;    
                }
                DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: PutByte - issuing requests.. \n"));                                                   
                    /* do this in a loop synchronously as fast as we can using the
                     * same request */    
                while (count && SDIO_SUCCESS(status)) { 
                    SDLIB_SetupCMD52Request(SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                            pArgs->Register,
                                            TRUE,
                                            *pArgs->pBuffer,                                   
                                            pRequest);               
                    status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pRequest);
                    count--;    
                }    
                DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: PutByte - issuing requests done! \n")); 
                SDDeviceFreeRequest(pInstance->pDevice, pRequest);    
            }    
            break;
        case 2:
            status = DoAsyncPutByte(pInstance,pArgs); 
            break;
        default:                
                /* write out 1 byte synchronously */ 
            status = SDLIB_IssueCMD52(pInstance->pDevice, SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                      pArgs->Register, pArgs->pBuffer, 1, TRUE); 
            break;
    }
    
    return status;
}

/* 
 * GetByte - use CMD52 to read a byte 
*/
SDIO_STATUS GetByte(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    /* read in 1 byte synchronously */ 
    return SDLIB_IssueCMD52(pInstance->pDevice, SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                            pArgs->Register, pArgs->pBuffer, 1, FALSE); 
}

/*
 * PutArray - write an array
*/
SDIO_STATUS PutArray(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
     
    switch (pArgs->TestIndex) {
        
        case 3:
            if (pArgs->BufferLength < 4) {
                status = SDIO_STATUS_INVALID_PARAMETER;
                break;  
            }
            { 
                INT blocks, bytesPerBlock;
                
                blocks = pArgs->pBuffer[0] | pArgs->pBuffer[1] << 8;
                bytesPerBlock = pArgs->pBuffer[2] | pArgs->pBuffer[3] << 8;
                status = DoBlockTest(pInstance, pArgs->Register, blocks, bytesPerBlock); 
            }
            break;
        case 2:
            memset(pInstance->Config.pBlockBuffer, 0xff, pInstance->Config.BufferSize);
            memset(pInstance->Config.pBlockBuffer, 0x00, 16); 
            status = ArrayBlockTest_ByteBasis(pInstance, 
                                              pArgs->Register, 
                                              pInstance->Config.pBlockBuffer,
                                              BYTE_BASIS_SIZE,
                                              FALSE);
            break;
        case 1:
            memset(pInstance->Config.pBlockBuffer, 0xff, pInstance->Config.BufferSize);
            memset(pInstance->Config.pBlockBuffer, 0x00, 16); 
            status = ArrayBlockTest_BlockBasis(pInstance, 
                                               pArgs->Register,
                                               pInstance->Config.pBlockBuffer,
                                               NULL,
                                               0,
                                               BLOCK_BASIS_COUNT,
                                               BLOCK_BASIS_SIZE,
                                               FALSE);
            break;
        default:                
                /* default synchronous test */ 
            status = PutArraySync(pInstance,pArgs);
            break;
    }
    
    return status;
}
        
/* 
 * PutArray - use CMD53 to write an array 
*/
SDIO_STATUS PutArraySync(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status;
    
    /* allocate request to send to host controller */
    pReq = SDDeviceAllocRequest(pInstance->pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    if (pArgs->BufferLength > SDIO_MAX_LENGTH_BYTE_BASIS) {
        SDDeviceFreeRequest(pInstance->pDevice,pReq);
        return SDIO_STATUS_INVALID_PARAMETER; 
    }
    
    /* initialize the command argument bits, see CMD53 SDIO spec. */
    SDIO_SET_CMD53_ARG(pReq->Argument,
                       CMD53_WRITE,             /* write */ 
                       SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                       CMD53_BYTE_BASIS,       /* set to byte mode */
                       CMD53_FIXED_ADDRESS,    /*  fixed address */
                       pArgs->Register,/* 17-bit register address */
                       CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(pArgs->BufferLength)  /* bytes */
                       );
    pReq->pDataBuffer = pArgs->pBuffer;
    pReq->Command = CMD53;
    pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                  SDREQ_FLAGS_DATA_WRITE;
    pReq->BlockCount = 1;    /* byte mode is always 1 block */
    pReq->BlockLen = pArgs->BufferLength;
    
    /* send the CMD53 out synchronously */                       
    status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
    if (!SDIO_SUCCESS(status)) {
       DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Synch CMD53 write failed %d \n", 
                               status));
    } 
        // free the request
    SDDeviceFreeRequest(pInstance->pDevice,pReq);
    return status;
}

/* 
 * GetArray - use CMD53 to read an array 
*/
SDIO_STATUS GetArray(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
     
    switch (pArgs->TestIndex) {        
        case 2:
            status = ArrayBlockTest_ByteBasis(pInstance, 
                                              pArgs->Register, 
                                              pInstance->Config.pBlockBuffer,
                                              BYTE_BASIS_SIZE,
                                              TRUE);
            break;
        case 1:
            status = ArrayBlockTest_BlockBasis(pInstance, 
                                               pArgs->Register,
                                               pInstance->Config.pBlockBuffer,
                                               NULL,
                                               0,
                                               BLOCK_BASIS_COUNT,
                                               BLOCK_BASIS_SIZE,
                                               TRUE);
            break;
        default:               
                /* default synchronous test */ 
            status = GetArraySync(pInstance,pArgs);
            break;
    }
    
    return status;
    
}

/* 
 * GetArray - use CMD53 to read an array 
*/
SDIO_STATUS GetArraySync(PSAMPLE_FUNCTION_INSTANCE pInstance, PSDIO_TEST_ARGS pArgs)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status;

    /* allocate request to send to host controller */
    pReq = SDDeviceAllocRequest(pInstance->pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }

    if (pArgs->BufferLength > SDIO_MAX_LENGTH_BYTE_BASIS) {
        SDDeviceFreeRequest(pInstance->pDevice,pReq);
        return SDIO_STATUS_INVALID_PARAMETER; 
    }
    
    /* initialize the command bits, see CMD53 SDIO spec. */
    SDIO_SET_CMD53_ARG(pReq->Argument,
                       CMD53_READ,             /* read */ 
                       SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                       CMD53_BYTE_BASIS,       /* set to byte mode */
                       CMD53_FIXED_ADDRESS,    /* fixed address, or use CMD53_INCR_ADDRESS */
                       pArgs->Register,/* 17-bit register address */
                       CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(pArgs->BufferLength)  /* bytes */
                       );
    pReq->pDataBuffer = pArgs->pBuffer;
    pReq->Command = CMD53;
    pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS;
    pReq->BlockCount = 1;    
    pReq->BlockLen = pArgs->BufferLength;

    /* send the CMD53 out synchronously */                       
    status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
    if (!SDIO_SUCCESS(status)) {
       DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: Synch CMD53 read failed %d \n", 
                               status));
    } 
        // free the request
    SDDeviceFreeRequest(pInstance->pDevice,pReq);
    return status;
}

/* 
 * DeleteSampleInstance - delete an instance  
*/
void DeleteSampleInstance(PSAMPLE_FUNCTION_CONTEXT   pFuncContext,
                          PSAMPLE_FUNCTION_INSTANCE  pInstance)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
                                   
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pFuncContext->InstanceSem))) {
        return; 
    }
        /* pull it out of the list */
    SDListRemove(&pInstance->SDList);
    SemaphorePost(&pFuncContext->InstanceSem);
        
    ZERO_OBJECT(fData);    
        /* try to disable the function */
    fData.EnableFlags = SDCONFIG_DISABLE_FUNC;
    fData.TimeOut = 500;
    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_FUNC_ENABLE_DISABLE,
                      &fData,
                      sizeof(fData));
                                    
        /* free slot current if we allocated any */    
    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_FUNC_FREE_SLOT_CURRENT,
                      NULL,
                      0);
                      
    SemaphoreDelete(&pInstance->IOComplete); 
    
         /* @TODO: handle other device cleanup */

}

/* 
 * FindSampleInstance - find an instance associated with the SD device 
*/
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

/* 
 * FindSampleInstanceByIndex - find an instance associated with an index 
*/
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

/* 
 * AddSampleInstance - add and instance to our list 
*/
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

/* 
 * CleanupFunctionContext - cleanup the function context 
*/
void CleanupFunctionContext(PSAMPLE_FUNCTION_CONTEXT pFuncContext)
{
    SemaphoreDelete(&pFuncContext->InstanceSem);  
}

/* 
 * InitFunctionContext - initialize the function context 
*/
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

/*
 * AsyncBlockTestComplete - completion routine for asynchronous block test
*/
void AsyncBlockTestComplete(struct _SDREQUEST *pRequest)
{
    PSAMPLE_FUNCTION_INSTANCE pInstance = (PSAMPLE_FUNCTION_INSTANCE)pRequest->pCompleteContext;
   
    DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: AsyncBlockTestComplete Status:%d \n",
            pRequest->Status));  
    pInstance->LastRequestStatus = pRequest->Status;
    SemaphorePost(&pInstance->IOComplete);                 
    SDDeviceFreeRequest(pInstance->pDevice, pRequest);              
}

/* 
 * ArrayBlockTest_ByteBasis - ArrayBlockTest byte basis mode 
*/
SDIO_STATUS ArrayBlockTest_ByteBasis(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                              UINT32 Address,
                              PVOID  pBuffer,
                              UINT32 Bytes,
                              BOOL   Read)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
        /* allocate request to send to host controller */
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        if (NULL == pReq) {
            return SDIO_STATUS_NO_RESOURCES;    
        }
    
        /* initialize the command argument bits, see CMD53 SDIO spec. */
        SDIO_SET_CMD53_ARG(pReq->Argument,
                           (Read ? CMD53_READ : CMD53_WRITE),
                           SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                           CMD53_BYTE_BASIS,       /* set to byte mode */
                           CMD53_INCR_ADDRESS,    
                           Address, /* 17-bit register address */
                           CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(Bytes)  /* bytes */
                           );
        pReq->pDataBuffer = pBuffer;
        pReq->Command = CMD53;
        pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                      (Read ? 0 : SDREQ_FLAGS_DATA_WRITE)
                      | SDREQ_FLAGS_TRANS_ASYNC;
        pReq->BlockCount = 1;    /* byte mode is always 1 block */
        pReq->BlockLen = Bytes;
        pReq->pCompletion = AsyncBlockTestComplete;
        pReq->pCompleteContext = (PVOID)pInstance;
                
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Synch CMD53 Start byte basis (%d)- %s \n",
            Bytes, Read ? "Read" : "Write"));
        SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
            /* wait for completion */
        SemaphorePend(&pInstance->IOComplete);    
        
        status = pInstance->LastRequestStatus;
    } while (FALSE);
    
    return status;
}

/* 
 * ArrayBlockTest_BlockBasis - array block test : block basis mode 
*/
SDIO_STATUS ArrayBlockTest_BlockBasis(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                              UINT32 Address,
                              PVOID  pBuffer,
                              PSDDMA_DESCRIPTOR pSGList,
                              UINT   SGcount,
                              UINT32 Blocks,
                              UINT32 BytesPerBlock,
                              BOOL Read)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status;
    
    do {
        /* allocate request to send to host controller */
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        if (NULL == pReq) {
            return SDIO_STATUS_NO_RESOURCES;    
        }
    
        status =  SDLIB_SetFunctionBlockSize(pInstance->pDevice, BytesPerBlock);
        
        if (!SDIO_SUCCESS(status)) {
           DBG_PRINT(SDDBG_WARN, ("SDIO Sample Function: block length set failed %d \n", 
                                   status));
           break;
        }

        /* send the CMD53 out asynchronously */                       
        SDIO_SET_CMD53_ARG(pReq->Argument,
                       (Read ? CMD53_READ : CMD53_WRITE),             /* oepration */ 
                       SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                       CMD53_BLOCK_BASIS,       /* set to block mode */
                       CMD53_INCR_ADDRESS,
                       Address, /* 17-bit register address */
                       Blocks  /* blocks */
                       );
        pReq->pDataBuffer = pBuffer;
        pReq->Command = CMD53;
        pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                      (Read ? 0 : SDREQ_FLAGS_DATA_WRITE) | 
                      SDREQ_FLAGS_TRANS_ASYNC;
        if (SGcount > 0) {
            /* we have a DMAable buffer, request a DMA transfer */
            pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
            pReq->pDataBuffer  = (PVOID)pSGList;
            pReq->DescriptorCount = SGcount;
        }              
        pReq->BlockCount = Blocks;    
        pReq->BlockLen = BytesPerBlock;
        pReq->pCompletion = AsyncBlockTestComplete; 
        pReq->pCompleteContext = (PVOID)pInstance;
         
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: Synch CMD53 Block Basis (%d,%d) Start  - %s \n",
            Blocks,BytesPerBlock, Read ? "Read" : "Write"));
            
        SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
            /* wait for completion */
        SemaphorePend(&pInstance->IOComplete);          
        status = pInstance->LastRequestStatus;              
    } while (FALSE);
    
    return status;
}

/*
 * DoBlockTest - block I/O test
*/
SDIO_STATUS DoBlockTest(PSAMPLE_FUNCTION_INSTANCE pInstance, 
                        UINT32 Address,
                        INT Blocks, 
                        INT BytesPerBlock)
{
    SDIO_STATUS status = SDIO_STATUS_INVALID_PARAMETER;
    int ii;
    int totalBytes;
    int cnt;
        
    do {
        /* make sure the block size and count fit in our allocated buffers */
        totalBytes = Blocks * BytesPerBlock;
        if (totalBytes > (int)pInstance->Config.BufferSize) {
            status = SDIO_STATUS_BUFFER_TOO_SMALL;
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: DoBlockTest - transfer size too large: Blocks:%d, BytesPerBlocks:%d \n",
                      Blocks, BytesPerBlock)); 
            break;
        }
        /* make sure the block size and the count don't exceed what the device can do */
        if (BytesPerBlock > SDDEVICE_GET_OPER_BLOCK_LEN(pInstance->pDevice)) {
            status = SDIO_STATUS_BUFFER_TOO_SMALL;
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: DoBlockTest - block size to big: size:%d, operational size:%d \n",
                      BytesPerBlock, SDDEVICE_GET_OPER_BLOCK_LEN(pInstance->pDevice))); 
            break;
        }
        if (Blocks > SDDEVICE_GET_OPER_BLOCKS(pInstance->pDevice)) {
            status = SDIO_STATUS_BUFFER_TOO_SMALL;
            DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: DoBlockTest - block count to big: count:%d, operational count:%d \n",
                      Blocks, SDDEVICE_GET_OPER_BLOCKS(pInstance->pDevice))); 
            break;
        }
        
        if (0 == totalBytes) {
            break; 
        }           
          
        DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: block read/write test: blocks:%d, bytesPerBlocks:%d \n",
          Blocks, BytesPerBlock)); 
             
        /* initialize the buffers with some test data */
        cnt = 0;
        for (ii = 0; ii < totalBytes; ii++) {
           pInstance->Config.pBlockBuffer[ii]  = (UCHAR)cnt;
           pInstance->Config.pVerifyBuffer[ii] = pInstance->Config.pBlockBuffer[ii];
           if (ii < 256) {
               cnt++;
           } else {
               cnt--;
           }
        }
        pInstance->Config.pBlockBuffer[0] = 0x5A;
        pInstance->Config.pVerifyBuffer[0] = pInstance->Config.pBlockBuffer[0];
        pInstance->Config.pBlockBuffer[totalBytes-2] = 0xDE;
        pInstance->Config.pVerifyBuffer[totalBytes-2] = pInstance->Config.pBlockBuffer[totalBytes-2];
        pInstance->Config.pBlockBuffer[totalBytes-1] = 0xAD;
        pInstance->Config.pVerifyBuffer[totalBytes-1] = pInstance->Config.pBlockBuffer[totalBytes-1];
        if (testusedma == 0) {
            /* no DMA */
NON_DMA:
            /* write buffer */
            status = ArrayBlockTest_BlockBasis(pInstance, 
                              Address,
                              pInstance->Config.pBlockBuffer,
                              NULL,
                              0,
                              Blocks,
                              BytesPerBlock,
                              FALSE); 
          
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed test write: %d, \n",status));
                break;   
            }
            
            memset(pInstance->Config.pBlockBuffer, 0, totalBytes);                 
            /* read buffer */              
            status = ArrayBlockTest_BlockBasis(pInstance, 
                              Address,
                              pInstance->Config.pBlockBuffer,
                              NULL,
                              0,
                              Blocks,
                              BytesPerBlock,
                              TRUE);    
                             
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed test read: %d, \n",status));          
                break;   
            }    
        } else {
            PSDDMA_DESCRIPTOR pSGList; 
            UINT   SGcount;
            
            /* check that we can support this DMA and get the scatter gather entry for it */
            pSGList = SampleMakeSGlist(pInstance, totalBytes, 0, &SGcount);
            if (pSGList == NULL) {
                /* punt to non-DMA */
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: punting to non-DMA\n"));
                goto NON_DMA;
            }            
            /* setup the test using DMA */
            /* write buffer */
            status = ArrayBlockTest_BlockBasis(pInstance, 
                              Address,
                              pInstance->Config.pBlockBuffer,
                              pSGList,
                              SGcount,
                              Blocks,
                              BytesPerBlock,
                              FALSE); 
          
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed test write: %d, \n",status));
                break;   
            }
            
            memset(pInstance->Config.pBlockBuffer, 0, totalBytes);                 
            /* read buffer */              
            status = ArrayBlockTest_BlockBasis(pInstance, 
                              Address,
                              pInstance->Config.pBlockBuffer,
                              pSGList,
                              SGcount,
                              Blocks,
                              BytesPerBlock,
                              TRUE);    
                             
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: failed test read: %d, \n",status));          
                break;   
            }    
        }
        
		status = SDIO_STATUS_SUCCESS;
                        /* check returned data */
        for (ii = 0; ii < totalBytes; ii++) {
            if (pInstance->Config.pVerifyBuffer[ii] != pInstance->Config.pBlockBuffer[ii]) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Sample Function: buffer read error at %d, Got:0x%X, Expecting:0x%X\n",
                    ii, pInstance->Config.pBlockBuffer[ii], pInstance->Config.pVerifyBuffer[ii]));
                SDLIB_PrintBuffer(pInstance->Config.pBlockBuffer,totalBytes,"BlockBuffer");
                SDLIB_PrintBuffer(pInstance->Config.pVerifyBuffer,totalBytes,"VerifyBuffer");
                status = SDIO_STATUS_ERROR;
                break; 
            }
        }
        
        if (ii >= totalBytes) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO Sample Function: buffer read-back success! \n")); 
        }
      
    } while (FALSE);
 
    return status;
}
