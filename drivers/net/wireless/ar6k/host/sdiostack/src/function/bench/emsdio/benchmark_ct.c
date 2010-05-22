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
@file: benchmark_ct.c

@abstract: SDIO Benchmarking driver, Codetelligence Driver Model

#notes: OS independent portions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <sdio_lib.h>
#include <_sdio_defs.h>
#include "../common/benchmark.h"
#include "benchmark_ct.h"       
     
/* initialize instance */
SDIO_STATUS InitializeInstance(PBENCHMARK_FUNCTION_CONTEXT  pFuncContext,
                               PBENCHMARK_FUNCTION_INSTANCE pInstance, 
                               PSDDEVICE                    pDevice)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    SDCONFIG_FUNC_SLOT_CURRENT_DATA   slotCurrent;
    CARD_INFO_FLAGS                   cardFlags;
    BOOL                              sdioCard = FALSE;
    SDCONFIG_BUS_MODE_DATA            busSettings;
                    
    
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    ZERO_OBJECT(fData);
    ZERO_OBJECT(slotCurrent);
    ZERO_OBJECT(pInstance->BusChars);
    SDLIST_INIT(&pInstance->SDList);
    
    do {
        
        status = SignalInitialize(&pInstance->IOComplete);
        
        if (!SDIO_SUCCESS(status)) {
            break;
        }
        
        pInstance->pDevice = pDevice;    
            
        cardFlags = SDDEVICE_GET_CARD_FLAGS(pDevice);
        
        switch(GET_CARD_TYPE(cardFlags)) {
            case CARD_MMC: 
                pInstance->BusChars.CardType = BM_CARD_MMC;
                break;
            case CARD_SD:  
                pInstance->BusChars.CardType = BM_CARD_SD;
                break;
            case CARD_SDIO: 
                sdioCard = TRUE;
                pInstance->BusChars.CardType = BM_CARD_SDIO;
                break;
            default:
                DBG_ASSERT(FALSE);
                break;
        }
        
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO Benchmark Function Instance: 0x%X \n",(INT)pInstance));
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
                
        pInstance->BusChars.ClockRate = SDDEVICE_GET_OPER_CLOCK(pDevice);
        pInstance->BusChars.MaxBlocksPerTransfer = SDDEVICE_GET_MAX_BLOCKS(pDevice);
        pInstance->BusChars.MaxBytesPerBlock = SDDEVICE_GET_MAX_BLOCK_LEN(pDevice);
        
        if (SDDEVICE_GET_BUSWIDTH(pDevice) == SDCONFIG_BUS_WIDTH_1_BIT) {
            pInstance->BusChars.BusMode = BM_BUS_1_BIT;   
        } else if (SDDEVICE_GET_BUSWIDTH(pDevice) == SDCONFIG_BUS_WIDTH_4_BIT) {
            pInstance->BusChars.BusMode = BM_BUS_4_BIT;   
        } else if (SDDEVICE_GET_BUSWIDTH(pDevice) == SDCONFIG_BUS_WIDTH_MMC8_BIT) {
            pInstance->BusChars.BusMode = BM_BUS_8_BIT;
        } else if (SDDEVICE_GET_BUSWIDTH(pDevice) == SDCONFIG_BUS_WIDTH_SPI) {
            if (SDDEVICE_GET_BUSMODE_FLAGS(pDevice) &  SDCONFIG_BUS_MODE_SPI_NO_CRC) {
                pInstance->BusChars.BusMode = BM_BUS_SPI_NO_CRC; 
            } else {
                pInstance->BusChars.BusMode = BM_BUS_SPI_CRC;    
            }
        } else {
            DBG_ASSERT(FALSE);    
        }
          
        if (((SDDEVICE_GET_SDIO_MANFID(pDevice) == 0x0) &&    
             (SDDEVICE_GET_SDIO_MANFCODE(pDevice) == 0x388)) ||
             ((SDDEVICE_GET_SDIO_MANFID(pDevice) == 0x55AA) &&    
             (SDDEVICE_GET_SDIO_MANFCODE(pDevice) == 0x2211)) ) {
                pInstance->TETestCard = TRUE;
        }
        
        if ((SDDEVICE_GET_SDIO_MANFID(pDevice) == 0x0) &&    
            (SDDEVICE_GET_SDIO_MANFCODE(pDevice) == 0x388) && 
            (SDDEVICE_GET_MAX_CLOCK(pDevice) > SD_MAX_BUS_CLOCK)) {
            UCHAR hsControl;
            
            /* MARSII board has an incorrect revision code, so we have to 
             * manually set high speed mode */
             
            status = Cmd52ReadByteCommon(pDevice, 
                                         SDIO_HS_CONTROL_REG, 
                                         &hsControl); 
                                            
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_TRACE, 
                    ("SDIO Failed to read high speed control (%d) \n",status)); 
                    /* reset status and continue */  
                status = SDIO_STATUS_SUCCESS;
            } else {
                if (hsControl & SDIO_HS_CONTROL_SHS) {                    
                    DBG_PRINT(SDDBG_TRACE, ("SDIO Card Supports High Speed Mode\n"));
                    
                    ZERO_OBJECT(busSettings);
                       /* get current bus flags and keep the same bus width */
                    busSettings.BusModeFlags = SDDEVICE_GET_BUSMODE_FLAGS(pDevice);
                        /* switch to high speed */
                    busSettings.BusModeFlags |= SDCONFIG_BUS_MODE_SD_HS;
                    busSettings.ClockRate = SD_HS_MAX_BUS_CLOCK;
                    status = SDLIB_IssueConfig(pDevice,
                                               SDCONFIG_FUNC_CHANGE_BUS_MODE,
                                               &busSettings,
                                               sizeof(SDCONFIG_BUS_MODE_DATA)); 
                    
                    
                    if (!SDIO_SUCCESS(status)) {
                        DBG_PRINT(SDDBG_ERROR, ("Failed to switch test card to high speed mode (%d)\n",
                            status));
                        /* just keep the current settings and move on */
                    }  else {  
                        pInstance->BusChars.ClockRate = SDDEVICE_GET_OPER_CLOCK(pDevice);
                    }
                }   
            }         
        } 
        
       
            /* allocate buffers to perform I/O on */                
        status = AllocateBenchMarkBuffers(pInstance);
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark: Failed to allocate buffers: %d\n", status));
            break;    
        }  
            /* pick some reasonable number */
        slotCurrent.SlotCurrent = 200;   
       
        DBG_PRINT(SDDBG_TRACE, 
                  ("SDIO BenchMark: Allocating Slot current: %d mA\n", 
                  slotCurrent.SlotCurrent));
                           
        status = SDLIB_IssueConfig(pDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
                                   
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: Remaining Slot Current: %d mA\n",
                                    slotCurrent.SlotCurrent));  
            }
            break; 
        }
        
        if (sdioCard) {
                /* enable the card */
            fData.EnableFlags = SDCONFIG_ENABLE_FUNC;
            fData.TimeOut = 500;
            status = SDLIB_IssueConfig(pDevice,
                                       SDCONFIG_FUNC_ENABLE_DISABLE,
                                       &fData,
                                       sizeof(fData));
            if (!SDIO_SUCCESS((status))) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BenchMark Function: Initialize, failed to enable function %d\n",
                                        status));
                break;
            }
        }
 
        if (!pInstance->DeferTestExecution) {    
            BenchMarkTests(pInstance, 
                           &pInstance->BusChars);
        }
        
    } while (FALSE);
        
    if (!SDIO_SUCCESS(status)) {
        SignalDelete(&pInstance->IOComplete);   
    }
    
    return status;
}

SDIO_STATUS IssueCMD52ASync(PBENCHMARK_FUNCTION_INSTANCE pInstance, PSDREQUEST pReq);


/* asynch callback for CMD52 benchmarking */
void AsyncCMD52Callback(PSDREQUEST pReq)
{
    SDIO_STATUS                  status;
    PBENCHMARK_FUNCTION_INSTANCE pInstance; 
    BOOL                         done = FALSE;
    
    pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pReq->pCompleteContext;
    
    status = pReq->Status;
    
    do {
        
        if (!SDIO_SUCCESS(status)) {
            done = TRUE; 
            pInstance->LastStatus = status;
            break;    
        }
        
        if (!(pInstance->TestFlags & BM_FLAGS_WRITE)) {
            *pInstance->pCurrBuffer = 
                    SD_R5_GET_READ_DATA(pReq->Response);  
        }
        
        DBG_ASSERT(pInstance->CurrentCount > 0);   
        pInstance->CurrentCount--;
               
        if (0 == pInstance->CurrentCount) { 
            done = TRUE;   
            break;    
        }        
        
        pInstance->pCurrBuffer++;
        
        if (!(pInstance->TestFlags & BM_FLAGS_CMD52_FIXED_ADDR)) {
            pInstance->CurrentAddress++;
        }
        
            /* continue on */   
        IssueCMD52ASync(pInstance, pReq);
        
    } while (FALSE); 
    
    
    if (done) {
        SDDeviceFreeRequest(pInstance->pDevice, pReq);
        SignalSet(&pInstance->IOComplete); 
    }
}
                                          
/* issue CMD52 asynchronously to move the data from the buffer */
SDIO_STATUS IssueCMD52ASync(PBENCHMARK_FUNCTION_INSTANCE pInstance, PSDREQUEST pReq)
{
    
    SDLIB_SetupCMD52RequestAsync((pInstance->TestFlags & BM_FLAGS_CMD52_COMMON_SPACE) ? 0 :
                                            SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                 pInstance->CurrentAddress,
                                 (pInstance->TestFlags & BM_FLAGS_WRITE) ? CMD52_DO_WRITE : CMD52_DO_READ,
                                 (pInstance->TestFlags & BM_FLAGS_WRITE) ? *pInstance->pCurrBuffer  : 0,
                                 pReq);
                                                     
    pReq->pCompleteContext = pInstance;
    pReq->pCompletion = AsyncCMD52Callback;   
                                    
    return SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice, pReq);
}

SDIO_STATUS Cmd52Transfer(PVOID    pContext,
                          UINT32   Address,
                          PUINT8   pBuffer,
                          UINT32   Count,
                          UINT32   Flags)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    PSDREQUEST pReq = NULL;
    
    do { 
    
        if (Flags & BM_FLAGS_ASYNC) {
            pInstance->LastStatus = SDIO_STATUS_SUCCESS;
            pInstance->pCurrBuffer = pBuffer;
            pInstance->TestFlags = Flags;
            pInstance->CurrentCount = Count;
            pInstance->CurrentAddress = Address;
            
            pReq = SDDeviceAllocRequest(pInstance->pDevice);
        
            if (NULL == pReq) {
                status = SDIO_STATUS_NO_RESOURCES; 
                break;
            }
        
            status = IssueCMD52ASync(pInstance, pReq);
            
            if (!SDIO_SUCCESS(status)) {
                break; 
            }
            
                /* null it out , it will get freed in completion routine */
            pReq = NULL;
                        
            SignalWait(&pInstance->IOComplete);
            
            status = pInstance->LastStatus;         
            
            break;
        }
        
        if (!(Flags & BM_FLAGS_CMD52_FIXED_ADDR)) {
                /* let the library handle it */
            status = SDLIB_IssueCMD52(pInstance->pDevice,
                                      (Flags & BM_FLAGS_CMD52_COMMON_SPACE) ? 0 : 
                                                SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                      Address,
                                      pBuffer,
                                      (INT)Count,
                                      (Flags & BM_FLAGS_WRITE) ? TRUE : FALSE);  
            break;    
        }    
        
        pReq = SDDeviceAllocRequest(pInstance->pDevice);
        
        if (NULL == pReq) {
            status = SDIO_STATUS_NO_RESOURCES;    
            break;
        }
        
            /* FIFO mode */
        while (Count) {
            
            SDLIB_SetupCMD52Request((Flags & BM_FLAGS_CMD52_COMMON_SPACE) ? 0 : 
                                     SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice),
                                    Address,
                                    (Flags & BM_FLAGS_WRITE) ? TRUE : FALSE,
                                    (Flags & BM_FLAGS_WRITE) ? *pBuffer : 0,                                    
                                    pReq);
                                    
            status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
            
            if (!SDIO_SUCCESS(status)) {
                break;    
            }   
            
            if (!(Flags & BM_FLAGS_WRITE)) {
                *pBuffer =  SD_R5_GET_READ_DATA(pReq->Response);        
            }
            
            pBuffer++;
            Count--;
            
        }   
                             
    } while (FALSE);                            
             
    if (pReq != NULL) {
        SDDeviceFreeRequest(pInstance->pDevice, pReq);    
    }       
    
    return status;        
}

static SDIO_STATUS IssueDeviceRequest(PSDDEVICE        pDevice,
                                      UINT8            Cmd,
                                      UINT32           Argument,
                                      SDREQUEST_FLAGS  Flags)
{ 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDREQUEST  pReq;
    
        /* caller doesn't care about the response data, allocate locally */
    pReq = SDDeviceAllocRequest(pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    pReq->Argument = Argument;          
    pReq->Flags = Flags;              
    pReq->Command = Cmd; 
        
    status = SDDEVICE_CALL_REQUEST_FUNC(pDevice, pReq);

    SDDeviceFreeRequest(pDevice, pReq);   
        
    return status;
}



SDIO_STATUS SetCardBlockLength(PVOID        pContext, 
                               BM_CARD_TYPE CardType, 
                               UINT16       Length)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    
    switch (CardType) {
        case  BM_CARD_SDIO:
            status = SDLIB_SetFunctionBlockSize(pInstance->pDevice,
                                                Length);
        break;
        case BM_CARD_MMC:
        case BM_CARD_SD:
            status = IssueDeviceRequest(pInstance->pDevice, 
                                        CMD16, 
                                        Length,
                                        SDREQ_FLAGS_RESP_R1);
        break;
        default:
            status = SDIO_STATUS_INVALID_PARAMETER;
            break;
    }
   
    return status;
}

#define MMC_CMD_SET_BLOCK_LENGTH     CMD16
#define MMC_CMD_READ_SINGLE_BLOCK    CMD17
#define MMC_CMD_READ_MULTIPLE_BLOCK  CMD18
#define MMC_CMD_WRITE_SINGLE_BLOCK   CMD24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK CMD25

SDIO_STATUS MemCardBlockTransfer(PVOID          pContext,
                                 UINT32         BlockAddress,
                                 UINT16         BlockCount,
                                 UINT16         BytesPerBlock,
                                 PUINT8         pBuffer,
                                 UINT32         Flags)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    PSDREQUEST pReq = NULL;
    UINT       sgCount = 0;
    UINT32     thisTransfer;
    PSDDMA_DESCRIPTOR pDesc = NULL;
    
    pReq = SDDeviceAllocRequest(pInstance->pDevice);
    
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    while (BlockCount) {
        thisTransfer = min((UINT32)SDDEVICE_GET_MAX_BLOCKS(pInstance->pDevice),
                           (UINT32)BlockCount);    
        
        pReq->Flags = SDREQ_FLAGS_DATA_TRANS | SDREQ_FLAGS_RESP_R1;  
        
        if (Flags & BM_FLAGS_WRITE) {
            pReq->Command = (thisTransfer > 1) ? 
                             MMC_CMD_WRITE_MULTIPLE_BLOCK : MMC_CMD_WRITE_SINGLE_BLOCK;  
                             
            pReq->Flags |= SDREQ_FLAGS_DATA_WRITE | SDREQ_FLAGS_AUTO_TRANSFER_STATUS;
                             
        } else {
            pReq->Command = (thisTransfer > 1) ? 
                             MMC_CMD_READ_MULTIPLE_BLOCK : MMC_CMD_READ_SINGLE_BLOCK;  
        } 
        
        pReq->Argument = BlockAddress;
                                          
        if (thisTransfer > 1) {   
                /* bus driver issues auto stop on multi-blocks */ 
            pReq->Flags |= SDREQ_FLAGS_AUTO_CMD12; 
        }    
        
            /* see if this buffer is DMA-able */
        status = CheckDMABuildSGlist(pInstance, 
                                     pBuffer,
                                     BytesPerBlock * thisTransfer, 
                                     &sgCount,
                                     &pDesc);
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
        if (NULL == pDesc) {                             
                /* use normal PIO mode */    
            pReq->pDataBuffer = pBuffer;
        } else {
                /* using DMA */
            DBG_ASSERT(sgCount != 0);
            pReq->pDataBuffer = pDesc;
            pReq->DescriptorCount = sgCount;
            pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
        }
        
        pReq->BlockCount = thisTransfer;
        
        pReq->BlockLen = BytesPerBlock;            
            
        status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice, pReq);
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
           
        pBuffer += BytesPerBlock * thisTransfer;   
        BlockAddress += BytesPerBlock * thisTransfer;
        BlockCount -= thisTransfer;
    }
  
    SDDeviceFreeRequest(pInstance->pDevice, pReq);   
  
    return status;   
}
                           

SDIO_STATUS CMD53Transfer_ByteBasis(PVOID  pContext, 
                                    UINT32 Address,
                                    PUINT8 pBuffer,
                                    UINT32 Bytes,
                                    UINT32 BlockSizeLimit,
                                    UINT32 Flags)
{ 
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT        sgCount = 0;
    UINT32      thisTransfer;
    PSDDMA_DESCRIPTOR pDesc = NULL;
    
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    
        /* allocate request to send to host controller */
    pReq = SDDeviceAllocRequest(pInstance->pDevice); 
    
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    
    while (Bytes) {

        thisTransfer = min(BlockSizeLimit, Bytes); 
        
        thisTransfer = min(thisTransfer, (UINT32)SDIO_MAX_LENGTH_BYTE_BASIS);
        
        thisTransfer = min(thisTransfer, pInstance->BusChars.MaxBytesPerBlock);
        
            /* initialize the command argument bits */
        SDIO_SET_CMD53_ARG(pReq->Argument,
                           (Flags & BM_FLAGS_WRITE) ? CMD53_WRITE : CMD53_READ,
                           SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                           CMD53_BYTE_BASIS,       /* set to byte mode */
                           (Flags & BM_FLAGS_CMD53_FIXED_ADDR) ? CMD53_FIXED_ADDRESS : CMD53_INCR_ADDRESS,    
                           Address, /* 17-bit register address */
                           CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(thisTransfer)  /* bytes */
                           );
                           
        pReq->Command = CMD53;
            /* synch */
        pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                      ((Flags & BM_FLAGS_WRITE) ? SDREQ_FLAGS_DATA_WRITE : 0);
                      
        if (Flags & BM_FLAGS_USE_SHORT_TRANSFER) {
            pReq->Flags |= SDREQ_FLAGS_DATA_SHORT_TRANSFER;   
        }
        
        pReq->BlockCount = 1;    /* byte mode is always 1 block */
        pReq->BlockLen = thisTransfer;
                
            /* see if this buffer is DMA-able */
        status = CheckDMABuildSGlist(pInstance, 
                                     pBuffer,
                                     thisTransfer, 
                                     &sgCount,
                                     &pDesc);
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
        
        if (NULL == pDesc) {                             
                /* use normal PIO mode */    
            pReq->pDataBuffer = pBuffer;
        } else {
                /* using DMA */
            DBG_ASSERT(sgCount != 0);
            pReq->pDataBuffer = pDesc;
            pReq->DescriptorCount = sgCount;
            pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
        }
        
        status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq); 
        
        if (!SDIO_SUCCESS(status)) {
            REL_PRINT(SDDBG_ERROR,("*** CMD53Transfer_ByteBasis: (%s,remaining=%d) Failed:%d\n",
                      (Flags & BM_FLAGS_WRITE) ? "Write":"Read", Bytes, status));
            SDLIB_IssueIOAbort(pInstance->pDevice);
            break;    
        }
         
        pBuffer += thisTransfer;
        Bytes -= thisTransfer;
        if (!(Flags & BM_FLAGS_CMD53_FIXED_ADDR)) {
            Address += thisTransfer;    
        }   
        
    }
    
    SDDeviceFreeRequest(pInstance->pDevice, pReq);   
     
    return status;
}


/* 
 * block test : block basis mode 
*/
SDIO_STATUS CMD53Transfer_BlockBasis(PVOID  pContext, 
                                     UINT32 Address,
                                     PUINT8 pBuffer,
                                     UINT32 Blocks,
                                     UINT32 BytesPerBlock,
                                     UINT32 Flags)
{
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT32      thisTransfer;
    UINT        sgCount = 0;
    PSDDMA_DESCRIPTOR pDesc = NULL;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    
        /* allocate request to send to host controller */
    pReq = SDDeviceAllocRequest(pInstance->pDevice);
    if (NULL == pReq) {
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    while (Blocks) {
        thisTransfer = min((UINT32)SDDEVICE_GET_MAX_BLOCKS(pInstance->pDevice),
                           Blocks);    
        
        thisTransfer = min(thisTransfer, (UINT32)SDIO_MAX_BLOCKS_BLOCK_BASIS);
                          
            /* send the CMD53 out synchronously */                       
        SDIO_SET_CMD53_ARG(pReq->Argument,
                       (Flags & BM_FLAGS_WRITE) ? CMD53_WRITE : CMD53_READ,  /* operation */ 
                       SDDEVICE_GET_SDIO_FUNCNO(pInstance->pDevice), /* function number */
                       CMD53_BLOCK_BASIS,       /* set to block mode */
                       (Flags & BM_FLAGS_CMD53_FIXED_ADDR) ? CMD53_FIXED_ADDRESS : CMD53_INCR_ADDRESS,
                       Address, /* 17-bit register address */
                       thisTransfer  /* blocks */
                       );
                       
        pReq->pDataBuffer = pBuffer;
        pReq->Command = CMD53;
        pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS | 
                      ((Flags & BM_FLAGS_WRITE) ? SDREQ_FLAGS_DATA_WRITE : 0);
 
            /* see if this buffer is DMA-able */
        status = CheckDMABuildSGlist(pInstance, 
                                     pBuffer,
                                     thisTransfer * BytesPerBlock, 
                                     &sgCount,
                                     &pDesc);
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        if (NULL == pDesc) {                             
                /* use normal PIO mode */    
            pReq->pDataBuffer = pBuffer;
        } else {
                /* using DMA */
            DBG_ASSERT(sgCount != 0);
            pReq->pDataBuffer = pDesc;
            pReq->DescriptorCount = sgCount;
            pReq->Flags |= SDREQ_FLAGS_DATA_DMA;
        }
                        
        pReq->BlockCount = thisTransfer;    
        pReq->BlockLen = BytesPerBlock;
        
        status = SDDEVICE_CALL_REQUEST_FUNC(pInstance->pDevice,pReq);
    
        if (!SDIO_SUCCESS(status)) {
            REL_PRINT(SDDBG_ERROR,("*** CMD53Transfer_BlockBasis: (blocks remaining=%d) Failed:%d\n",
                      Blocks, status));
            SDLIB_IssueIOAbort(pInstance->pDevice);
            break;    
        }
        
        pBuffer += BytesPerBlock * thisTransfer; 
        if (!(Flags & BM_FLAGS_CMD53_FIXED_ADDR)) {
            Address += BytesPerBlock * thisTransfer;    
        }   
        Blocks -= thisTransfer;
    }
    
    SDDeviceFreeRequest(pInstance->pDevice, pReq);
    
    return status;
}

/* 
 * DeleteSampleInstance - delete an instance  
*/
void DeleteInstance(PBENCHMARK_FUNCTION_CONTEXT   pFuncContext,
                          PBENCHMARK_FUNCTION_INSTANCE  pInstance)
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
                      

}

/* 
 * FindSampleInstance - find an instance associated with the SD device 
*/
PBENCHMARK_FUNCTION_INSTANCE FindInstance(PBENCHMARK_FUNCTION_CONTEXT pFuncContext,
                                          PSDDEVICE                pDevice)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = NULL;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, BENCHMARK_FUNCTION_INSTANCE, SDList);
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
PBENCHMARK_FUNCTION_INSTANCE FindInstanceByIndex(PBENCHMARK_FUNCTION_CONTEXT pFuncContext,
                                                    UINT                     Index)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = NULL;
    UINT ii = 0;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pInstance = CONTAINING_STRUCT(pItem, BENCHMARK_FUNCTION_INSTANCE, SDList);
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
SDIO_STATUS AddInstance(PBENCHMARK_FUNCTION_CONTEXT   pFuncContext,
                              PBENCHMARK_FUNCTION_INSTANCE  pInstance)
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
void CleanupFunctionContext(PBENCHMARK_FUNCTION_CONTEXT pFuncContext)
{
    SemaphoreDelete(&pFuncContext->InstanceSem);  
}

/* 
 * InitFunctionContext - initialize the function context 
*/
SDIO_STATUS InitFunctionContext(PBENCHMARK_FUNCTION_CONTEXT pFuncContext)
{
    SDIO_STATUS status;
    SDLIST_INIT(&pFuncContext->InstanceList); 
   
    status = SemaphoreInitialize(&pFuncContext->InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return status;
    }
    
    return SDIO_STATUS_SUCCESS;
}

SDIO_STATUS SetCardBusMode(PVOID pContext,BM_BUS_MODE BusMode,PUINT32 pClock)
{
    SDIO_STATUS                  status = SDIO_STATUS_SUCCESS;
    SDCONFIG_BUS_MODE_DATA       busSettings;
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
 
    do {
        ZERO_OBJECT(busSettings);
        busSettings.BusModeFlags = SDDEVICE_GET_BUSMODE_FLAGS(pInstance->pDevice);
        if (BM_BUS_1_BIT == BusMode) {
            SDCONFIG_SET_BUS_WIDTH(busSettings.BusModeFlags,SDCONFIG_BUS_WIDTH_1_BIT);    
        } else if (BM_BUS_4_BIT == BusMode) {
            SDCONFIG_SET_BUS_WIDTH(busSettings.BusModeFlags,SDCONFIG_BUS_WIDTH_4_BIT);  
        } else {
            DBG_ASSERT(FALSE);
            status = SDIO_STATUS_ERROR;
            break;   
        }
            
        busSettings.ClockRate = *pClock;
        status = SDLIB_IssueConfig(pInstance->pDevice,
                                   SDCONFIG_FUNC_CHANGE_BUS_MODE,
                                   &busSettings,
                                   sizeof(SDCONFIG_BUS_MODE_DATA)); 
                                   
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        *pClock = busSettings.ActualClockRate;
    
    } while (FALSE);
                                               
    return status;
}

static CT_DEBUG_LEVEL g_SavedDebug;

void SetHcdDebugLevel(PVOID pContext,CT_DEBUG_LEVEL Level) 
{
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;
    
    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_GET_HCD_DEBUG,
                      &g_SavedDebug,
                      sizeof(g_SavedDebug));
                      
    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_SET_HCD_DEBUG,
                      &Level,
                      sizeof(Level));
}

void RestoreHcdDebugLevel(PVOID pContext) 
{
    PBENCHMARK_FUNCTION_INSTANCE pInstance = (PBENCHMARK_FUNCTION_INSTANCE)pContext;

    SDLIB_IssueConfig(pInstance->pDevice,
                      SDCONFIG_SET_HCD_DEBUG,
                      &g_SavedDebug,
                      sizeof(g_SavedDebug));

}



