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
@file: sdio_hcd.c

@abstract: Texas Instruments OMAP native Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME  SDOMAPHCD
#include "sdio_omap_hcd.h"

#define FROM_ISR    TRUE
#define FROM_NORMAL FALSE

void EndHCTransfer(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, BOOL FromIsr);
void ResetController(PSDHCD_DEVICE pDevice, BOOL Restore, BOOL FromIsr);

#define OMAP_REQ_PROCESSING_USE_CLOCK_CONTROL

#ifdef OMAP_REQ_PROCESSING_USE_CLOCK_CONTROL
    /* control clock during request processing */
#define ReqProcClkStartStop(p,on) ClockStartStop((p),(on))
#else 
    /* let clock run free */
#define ReqProcClkStartStop(p,on)
#endif

#define OMAP_COMMAND_DONE_POLLING         2000000 
#define OMAP_SHORT_TRANSFER_DONE_POLLING  3000000 

#define WAIT_FOR_HC_STATUS(pHct,DoneMask,Error,ErrorMask,Status,Timeout)   \
{                                                                            \
     INT _timeoutCnt = (Timeout);                                            \
     (Status) = SDIO_STATUS_SUCCESS;                                         \
     while((_timeoutCnt > 0) &&                                               \
            !(READ_HOST_REG16((pHct), OMAP_REG_MMC_MODULE_STATUS) & (DoneMask)) &&            \
            !((Error) = READ_HOST_REG16((pHct), OMAP_REG_MMC_MODULE_STATUS) & (ErrorMask))){_timeoutCnt--;} \
     (Error) = READ_HOST_REG16((pHct), OMAP_REG_MMC_MODULE_STATUS) & (ErrorMask);            \
     if (0 == _timeoutCnt) {(Status) = SDIO_STATUS_DEVICE_ERROR; \
           DBG_PRINT(SDDBG_ERROR, \
           ("SDIO OMAP - status timeout, waiting for (mask=0x%X) (stat=0x%X)\n",\
               (UINT)(DoneMask), READ_HOST_REG16((pHct), OMAP_REG_MMC_MODULE_STATUS))); \
                             DBG_ASSERT(FALSE);}       \
}

#define SetFifoAFL(pHct,Depth) \
{                              \
    UINT16 fifoSettings = (Depth)/2;  \
    if (fifoSettings > 0) {fifoSettings--;} \
    fifoSettings = ((fifoSettings) << OMAP_REG_MMC_BUFFER_CONFIG_AFL_SHIFT) & \
                                        OMAP_REG_MMC_BUFFER_CONFIG_AFL_MASK; \
    WRITE_HOST_REG16((pHct), OMAP_REG_MMC_BUFFER_CONFIG, fifoSettings);     \
}

#define SetFifoAEL(pHct,Depth) \
{                              \
    UINT16 fifoSettings = (Depth)/2;  \
    if (fifoSettings > 0) {fifoSettings--;} \
    fifoSettings = ((fifoSettings) << OMAP_REG_MMC_BUFFER_CONFIG_AEL_SHIFT) & \
                                        OMAP_REG_MMC_BUFFER_CONFIG_AEL_MASK; \
    WRITE_HOST_REG16((pHct), OMAP_REG_MMC_BUFFER_CONFIG, fifoSettings);     \
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetResponseData - get the response data 
  Input:    pDevice - device context
            pReq - the request
  Output: 
  Return:
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void GetResponseData(PSDHCD_DEVICE pDevice, PSDREQUEST pReq)
{
    INT     wordCount;
    INT     byteCount;
    UINT16  readBuffer[8];
    UINT    ii;
    
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return;  
    }
    
       
    byteCount = SD_DEFAULT_RESPONSE_BYTES;        
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
        byteCount = SD_R2_RESPONSE_BYTES - 1; 
        wordCount = (byteCount + 1) / 2;      
        /* move data into read buffer */
        for (ii = 0; ii < wordCount; ii++) {
            readBuffer[ii] = READ_HOST_REG16(pDevice, OMAP_REG_MMC_CMD_RESPONSE0+(ii*4));
        }
        memcpy(&pReq->Response[0],readBuffer,byteCount);
    } else {
        wordCount = (byteCount + 1) / 2;      
    
        /* move data into read buffer */
        for (ii = 0; ii < wordCount; ii++) {
            readBuffer[ii] = READ_HOST_REG16(pDevice, OMAP_REG_MMC_CMD_RESPONSE6+(ii*4));
        }
        memcpy(&pReq->Response[1],readBuffer,byteCount);
    }
   
    if (DBG_GET_DEBUG_LEVEL() >= OMAP_TRACE_REQUESTS) { 
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO OMAP - Response Dump");
    }
    
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  DumpCurrentRequestInfo - debug dump  
  Input:    pDevice - device context
  Output: 
  Return: 
  Notes: This function debug prints the current request  
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void DumpCurrentRequestInfo(PSDHCD_DEVICE pDevice)
{
    if (pDevice->Hcd.pCurrentRequest != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP - Current Request Command:%d, ARG:0x%8.8X\n",
                  pDevice->Hcd.pCurrentRequest->Command, pDevice->Hcd.pCurrentRequest->Argument));
        if (IS_SDREQ_DATA_TRANS(pDevice->Hcd.pCurrentRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                      IS_SDREQ_WRITE_DATA(pDevice->Hcd.pCurrentRequest->Flags) ? "WRITE":"READ",
                      pDevice->Hcd.pCurrentRequest->BlockCount,
                      pDevice->Hcd.pCurrentRequest->BlockLen,
                      pDevice->Hcd.pCurrentRequest->DataRemaining));
        }
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  TranslateSDError - check for an SD error 
  Input:    pDevice - device context
            Status -  error interrupt status register value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS TranslateSDError(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, UINT16 Status)
{
    if (Status & OMAP_REG_MMC_MODULE_STATUS_CERR) {
        DBG_PRINT(SDDBG_WARN, ("SDIO OMAP TranslateSDError : Warning command response has error bits set\n"));
        return SDIO_STATUS_SUCCESS;
    }
     
    if (Status & OMAP_REG_MMC_MODULE_STATUS_CTO) {
        if (!((pReq->Command == 5) || (pReq->Command == 55) || (pReq->Command == 1))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Command Timeout: CMD:%d\n",pReq->Command)); 
            if (IS_SDREQ_DATA_TRANS(pReq->Flags)) { 
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO OMAP (CMD:%d) Timeout, %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                    pReq->Command,
                    IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                    pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
            }
        }   
        return SDIO_STATUS_BUS_RESP_TIMEOUT;
    }
    
    DBG_PRINT(SDDBG_WARN, ("SDIO OMAP TranslateSDError : current controller status: 0x%X\n",
        READ_HOST_REG16(pDevice,OMAP_REG_MMC_MODULE_STATUS)));
                      
    if (Status & OMAP_REG_MMC_MODULE_STATUS_CCRC) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP TranslateSDError : command CRC error\n"));
        return SDIO_STATUS_BUS_RESP_CRC_ERR;
    }

    if (Status & OMAP_REG_MMC_MODULE_STATUS_DCRC) {
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP TranslateSDError : write data CRC error\n"));
            return SDIO_STATUS_BUS_WRITE_ERROR;
        } else {
            DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP TranslateSDError : read data CRC error\n"));
            return SDIO_STATUS_BUS_READ_CRC_ERR;
        }
    }
    
    if (Status & OMAP_REG_MMC_MODULE_STATUS_DTO) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP TranslateSDError : data timeout\n"));
        return SDIO_STATUS_BUS_READ_TIMEOUT;
    }
    
    DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP - untranslated error 0x%X\n", (UINT)Status));    
    return SDIO_STATUS_DEVICE_ERROR;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  ClockStartStop - SD clock control
  Input:  pDevice - device object
          On - turn on or off (TRUE/FALSE)
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void ClockStartStop(PSDHCD_DEVICE pDevice, BOOL On) 
{ 
    UINT16 state;

    DBG_PRINT(OMAP_TRACE_CLOCK, ("SDIO OMAP - ClockStartStop, %d\n", (UINT)On));
    
    state = READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG);
    if (On) {        
        state &= ~OMAP_REG_MMC_MODULE_CONFIG_CLK_MASK;
        state |= pDevice->Clock;
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, state);
    } else {
        state &= ~OMAP_REG_MMC_MODULE_CONFIG_CLK_MASK;
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, state);
    }  
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  SetBusMode - Set Bus mode
  Input:  pDevice - device object
          pMode - mode
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void SetBusMode(PSDHCD_DEVICE pDevice, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    int ii;
    int clockIndex;
    UINT16 state  = 0;
    UINT32 rate;
    
    DBG_PRINT(OMAP_TRACE_CONFIG , ("SDIO OMAP - SetBusMode\n"));

        /* set clock index to the end max. divide */
    pMode->ActualClockRate = (pDevice->BaseClock) / OMAP_MAX_CLOCK_DIVIDE;
    clockIndex = OMAP_MAX_CLOCK_DIVIDE;
    for (ii = 1; ii <= OMAP_MAX_CLOCK_DIVIDE ; ii++) {
        rate = pDevice->BaseClock / ii;
        if (pMode->ClockRate >= rate) {
            pMode->ActualClockRate = rate;
            clockIndex = ii;
            break; 
        }   
    }

    state = READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG);
                                        
    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_1_BIT:
            state &=  ~OMAP_REG_MMC_MODULE_CONFIG_4BIT;
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            state |=  OMAP_REG_MMC_MODULE_CONFIG_4BIT;
            break;
        default:
            break;
    }

    pDevice->Clock = clockIndex;
    state &= ~OMAP_REG_MMC_MODULE_CONFIG_CLK_MASK;
    state |= pDevice->Clock;
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, state);
    MicroDelay(50);
    DBG_PRINT(OMAP_TRACE_CONFIG , ("SDIO OMAP - SetBusMode Clock: %d Khz, ClockRate %d (%d) state:0x%X\n", 
                                   pMode->ActualClockRate, pMode->ClockRate, clockIndex, (UINT)state));
}

/*
 * SetDataTimeout - set timeout for data transfers
*/
static void SetDataTimeout(PSDHCD_DEVICE pDevice, UINT TimeOut)
{
    UINT sdreg;
    UINT to = TimeOut;
    
    /* Check if we need to use timeout multiplier register */
    sdreg = READ_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG);
    if (TimeOut > 0xFFFF) {
        sdreg |= OMAP_REG_MMC_SDIO_MODE_CONFIG_DPE;
        to /= 1024;
    } else {
        sdreg &= ~OMAP_REG_MMC_SDIO_MODE_CONFIG_DPE;
    }
    DBG_PRINT(OMAP_TRACE_CONFIG , ("SDIO OMAP - SetDataTimeout Timeout: %d, mode: 0x%x,  to: 0x%x\n",
                        TimeOut, sdreg, to));
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG, sdreg);
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_DATA_READ_TIMEOUT, to);
}

/* DMA completion routine */
void DMACompletion(PVOID pContext, SDIO_STATUS status, BOOL FromIsr) 
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pContext;
    PSDREQUEST pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
             
    if (!SDIO_SUCCESS(status)) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP (%s) (%s) DMA transfer failed, status: %d\n", 
            IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX", 
            (OMAP_DMA_COMMON == pDevice->DmaMode) ? "Common-Buffer" : "Direct",
            status));
    } else {
        DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP (%s) (%s) DMA transfer completed \n", 
            IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",             
            (OMAP_DMA_COMMON == pDevice->DmaMode) ? "Common-Buffer" : "Direct"));  
    }
         
    CompleteRequestSyncDMA(pDevice, pReq, status);
    return;
}

/* transfer a FIFO worth of data, returns TRUE of all data was transfered */
BOOL HcdTransferTxData(PSDHCD_DEVICE pDevice, PSDREQUEST pReq)
{
    INT     dataCopy;
    PUINT8  pBuf;
    UINT16  data;
    volatile UINT16 *pFifo;
         
    pFifo = (volatile UINT16 *)((UINT32)GET_HC_REG_BASE(pDevice) + OMAP_REG_MMC_DATA_ACCESS);
    
        /* if we get called here because of an AEL interrupt, we know we have 
         * OMAP_MMC_FIFO_SIZE - OMAP_MMC_AEL_FIFO_THRESH room in the fifo to store more data */
    dataCopy = min(pReq->DataRemaining,(UINT32)(OMAP_MMC_FIFO_SIZE - OMAP_MMC_AEL_FIFO_THRESH));
    pBuf = (PUINT8)pReq->pHcdContext;
         
        /* update remaining count */
    pReq->DataRemaining -= dataCopy;
    DBG_ASSERT((INT)pReq->DataRemaining >= 0);
    
        /* copy to fifo */
    while (dataCopy) { 
        data = *pBuf;
        dataCopy--;
        pBuf++;
        if (dataCopy) {
            data |= ((UINT16)*pBuf) << 8;    
            dataCopy--;
            pBuf++;    
        }
        *pFifo = data;       
    }
         
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
          
    DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP Pending TX Remaining: %d \n",pReq->DataRemaining));
                                  
    if (pReq->DataRemaining) { 
        return FALSE;
    }
       
    return TRUE;
}

/* transfer a FIFO worth of data */
BOOL HcdTransferRxData(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, BOOL Flush)
{
    
    INT     dataCopy;
    PUINT8  pBuf;
    UINT16  data;
    volatile UINT16 *pFifo; 
       
    pFifo = (volatile UINT16 *)((UINT32)GET_HC_REG_BASE(pDevice) + OMAP_REG_MMC_DATA_ACCESS);
    
    if (Flush) {
        dataCopy = min(pReq->DataRemaining,(UINT32)OMAP_MMC_FIFO_SIZE);
    } else {
            /* each time we are called, we know we have at least a threshold's worth of data */
        dataCopy = min(pReq->DataRemaining,(UINT32)OMAP_MMC_AFL_FIFO_THRESH); 
    }
        /* get where we are */  
    pBuf = (PUINT8)pReq->pHcdContext;    
        /* update remaining count */
    pReq->DataRemaining -= dataCopy; 
    
    DBG_ASSERT((INT)pReq->DataRemaining >= 0);
    
        /* copy from fifo */
    while (dataCopy) { 
        data = *pFifo;
        *pBuf = (UINT8)data;
        dataCopy--;
        pBuf++;  
        if (dataCopy) {
            *pBuf = (UINT8)(data >> 8);    
            pBuf++;
            dataCopy--;
        }          
    }  
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
    
    DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP Pending RX Remaining: %d \n",pReq->DataRemaining));
     
    if (pReq->DataRemaining < OMAP_MMC_AFL_FIFO_THRESH) {
        return TRUE;
    }
    
    return FALSE;
}

SDIO_STATUS ProcessCommandDone(PSDHCD_DEVICE         pDevice,
                               PSDREQUEST            pReq,
                               BOOL                  FromIsr)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16      irqUnmask = 0;
    
    do {
      
            /* get the response data for the command */
        GetResponseData(pDevice, pReq);
              
            /* check for data */
        if (!IS_SDREQ_DATA_TRANS(pReq->Flags)) {
            break;    
        }
        
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pDevice->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, 
                ("SDIO OMAP Check Response failed (CMD:%d), %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                pReq->Command,
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
            break;    
        }
        
        if (pDevice->ShortTransfer) {
            UINT16 hwErrors;
            UINT16 waitMask;
            
            DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP Short %s data transfer (%d bytes) \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                                   pReq->DataRemaining));
           
                /* wait for block sent/receive or error */   
            waitMask = OMAP_REG_MMC_MODULE_STATUS_BRS;
                                   
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* load FIFO */
                HcdTransferTxData(pDevice, pReq);
                waitMask |= OMAP_REG_MMC_MODULE_STATUS_EOFB;  
            }
              
            WAIT_FOR_HC_STATUS(pDevice,
                               waitMask,
                               hwErrors,
                               OMAP_STATUS_DATA_PROCESSING_ERRORS,
                               status,
                               OMAP_SHORT_TRANSFER_DONE_POLLING); 
               
            if (!SDIO_SUCCESS(status)) {
                ResetController(pDevice,TRUE,FromIsr);
                break;    
            }
            
            if (hwErrors) {
                status = TranslateSDError(pDevice, pReq, hwErrors);
                if (!SDIO_SUCCESS(status)) {
                    break;    
                }   
            }
            
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* check for busy */
                MicroDelay(1);
                    /* check if card entered busy */
                if (!(READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS) & 
                      OMAP_REG_MMC_MODULE_STATUS_CB)) {
                        /* we are done */
                    break;    
                }
                    /* card entered busy */
                WRITE_HOST_REG16(pDevice, 
                                 OMAP_REG_MMC_MODULE_STATUS,
                                 OMAP_REG_MMC_MODULE_STATUS_CB);
                                 
                     /* wait end of busy */        
                WAIT_FOR_HC_STATUS(pDevice,
                                   OMAP_REG_MMC_MODULE_STATUS_EOFB,
                                   hwErrors,
                                   0, /* no need to check for errors */
                                   status,
                                   OMAP_SHORT_TRANSFER_DONE_POLLING) 
                   
                if (!SDIO_SUCCESS(status)) {
                    ResetController(pDevice,TRUE,FromIsr);
                }
                               
            } else {
                    /* unload FIFO */
                HcdTransferRxData(pDevice, pReq, TRUE);       
            } 
                /* done */            
            break;    
        }
  
            /* enable error interrupts, data transfer will require interrupts */
        irqUnmask = OMAP_REG_MMC_INTERRUPT_ERRORS;       
        status = SDIO_STATUS_PENDING;
        
        if (pDevice->DmaMode != OMAP_DMA_NONE) {
                /* for DMA let the DMA hardware run , we only want the interrupt
                 * for block sent/received in addition to the errors */
            irqUnmask |= OMAP_REG_MMC_INTERRUPT_ENABLE_BRS;
            
            break;    
        }
                    
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* set threshold for FIFO empty level */
            SetFifoAEL(pDevice,OMAP_MMC_AEL_FIFO_THRESH);
                /* wait for AEL interrupts */
            irqUnmask |= OMAP_REG_MMC_INTERRUPT_ENABLE_AE;
        } else {
            if (pReq->DataRemaining < OMAP_MMC_AFL_FIFO_THRESH) {
                    /* don't need AFL, wait for last block received interrupt instead */
                irqUnmask |= OMAP_REG_MMC_INTERRUPT_ENABLE_BRS;            
            } else {
                    /* set trigger level for FIFO full level */
                SetFifoAFL(pDevice,OMAP_MMC_AFL_FIFO_THRESH);
                    /* more data is expected */
                irqUnmask |= OMAP_REG_MMC_INTERRUPT_ENABLE_AF;
            }
        }
        
    } while (FALSE);
        
    if (SDIO_STATUS_PENDING == status) { 
        if (irqUnmask != 0) {
            UnmaskIrq(pDevice, irqUnmask, FromIsr);
        }
        DBG_PRINT(OMAP_TRACE_DATA, ("SDIO OMAP HcdRequest Pending %s data transfer \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
    }
    return status;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdRequest - SD request handler
  Input:  pHcd - HCD object
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdRequest(PSDHCD pHcd) 
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pHcd->pContext;
    UINT16                temp;
    PSDREQUEST            pReq;
    
    pDevice->CompletionCount = 0;
    pDevice->DmaMode = OMAP_DMA_NONE;
    pDevice->ShortTransfer = FALSE;
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
 
    do {
        if (pDevice->ShuttingDown) {
            DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdRequest returning canceled\n"));
            status = SDIO_STATUS_CANCELED;
            break;
        }
                  
        ReqProcClkStartStop(pDevice, CLOCK_OFF);
         
            /* make sure error ints and EOC is masked*/
        MaskIrq(pDevice, 
                OMAP_REG_MMC_INTERRUPT_ERRORS | OMAP_REG_MMC_INTERRUPT_ENABLE_EOC, 
                FROM_NORMAL);
        
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BUFFER_CONFIG, 0);
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD_TIMEOUT, pDevice->TimeOut);
                
            /* clear all status bits (including error bits) that deals with request processing */
        WRITE_HOST_REG16(pDevice, 
                         OMAP_REG_MMC_MODULE_STATUS, 
                         OMAP_REG_MMC_MODULE_STATUS_REQ_PROCESS);    
       
        if (READ_HOST_REG16(pDevice,OMAP_REG_MMC_MODULE_STATUS) & 
            OMAP_REG_MMC_MODULE_STATUS_REQ_PROCESS) {
            DBG_PRINT(SDDBG_WARN, ("SDIO OMAP ERROR!!! status did not clear: 0x%X\n",
                READ_HOST_REG16(pDevice,OMAP_REG_MMC_MODULE_STATUS)));
        }
                        
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {  
            default:
            case SDREQ_FLAGS_NO_RESP:
                temp = OMAP_REG_MMC_CMD_NORESPONSE;
                break;
            case SDREQ_FLAGS_RESP_R1:
                temp = OMAP_REG_MMC_CMD_R1;
                break;
            case SDREQ_FLAGS_RESP_R1B:   
                temp = OMAP_REG_MMC_CMD_R1 | OMAP_REG_MMC_CMD_R1BUSY;
                break;
            case SDREQ_FLAGS_RESP_R2:
                temp = OMAP_REG_MMC_CMD_R2;
                break;
            case SDREQ_FLAGS_RESP_R3:
                temp = OMAP_REG_MMC_CMD_R3;
                break;
            case SDREQ_FLAGS_RESP_SDIO_R4:
                    /* SDIO R4s are just OCR responses equivalent to an R3*/
                 temp = OMAP_REG_MMC_CMD_R3; 
                break;
            case SDREQ_FLAGS_RESP_SDIO_R5:
                    /* R5s are just R1 responses, do not use the R5 type in this controller
                     * because it will disable response timeout detection unless you set
                     * the C5E,C14E..bits */
                temp = OMAP_REG_MMC_CMD_R1;
                break;
            case SDREQ_FLAGS_RESP_R6:
                temp = OMAP_REG_MMC_CMD_R6;
                break;
        }   

            /* get the command type */
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {  
            case SDREQ_FLAGS_NO_RESP:
                    /* broadcast no-response */
                temp |= OMAP_REG_MMC_CMD_TYPE_BC; 
                break;
                
            case SDREQ_FLAGS_RESP_R2:
                if ((pReq->Command == CMD9) || (pReq->Command == CMD10)) {
                    temp |= OMAP_REG_MMC_CMD_TYPE_AC;    
                } else if (pReq->Command == CMD2) {
                    temp |= OMAP_REG_MMC_CMD_TYPE_BCR;     
                } else {
                    DBG_ASSERT(FALSE);   
                }
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_R6:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                    /* responses that are broadcast */
                temp |= OMAP_REG_MMC_CMD_TYPE_BCR; 
                break;
            default:
                /* all other commands are addressed responses */
                if (IS_SDREQ_DATA_TRANS(pReq->Flags)) {
                        /* commands with data */
                    temp |= OMAP_REG_MMC_CMD_TYPE_ADTC;    
                } else {
                        /* all commands without data */
                    temp |= OMAP_REG_MMC_CMD_TYPE_AC;
                }    
                break;
        }   
        
        GetDefaults(pDevice);
        
        ReqProcClkStartStop(pDevice, CLOCK_ON);
              
        if (IS_SDREQ_DATA_TRANS(pReq->Flags)){
            /* set the block size register */
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_LENGTH, pReq->BlockLen-1);
            /* set block count register */
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT, pReq->BlockCount-1);
            pReq->DataRemaining = pReq->BlockLen * pReq->BlockCount;
            DBG_PRINT(OMAP_TRACE_DATA, 
                     ("SDIO OMAP HcdRequest: %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                                       IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                                       pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
        	DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdRequest: blen: 0x%X, nblk: 0x%X\n",
                                 READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_LENGTH), 
                                 READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT)));
                /* use the context to hold where we are in the buffer */
            pReq->pHcdContext = pReq->pDataBuffer;
            temp |= IS_SDREQ_WRITE_DATA(pReq->Flags) ? 
                    OMAP_REG_MMC_CMD_DDIR_WRITE : OMAP_REG_MMC_CMD_DDIR_READ; 
            
            SetDataTimeout(pDevice, pDevice->DataTimeOut);
            
            if ((pReq->Flags & SDREQ_FLAGS_DATA_SHORT_TRANSFER) && 
                (pReq->DataRemaining < OMAP_MAX_SHORT_TRANSFER_SIZE)) {
                    /* flag current request as a short transfer */
                pDevice->ShortTransfer = TRUE;
            }
            
            if (!pDevice->ShortTransfer) {
                    /* setup dma transfer */         
                if (pDevice->DmaCapable) { 
                    if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
                            /* caller passed a scatter gather list */
                        pDevice->DmaMode = OMAP_DMA_SG; 
                    } else {
                            /* try common buffer */
                        pDevice->DmaMode = OMAP_DMA_COMMON; 
                    }    
                } else {
                    if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
                        DBG_ASSERT(FALSE);
                        status = SDIO_STATUS_INVALID_PARAMETER;
                        break; 
                    }   
                }
            }
            
            if (pDevice->DmaMode != OMAP_DMA_NONE) {
                    /* check DMA */
                status = CheckDMA(pDevice, pReq);
                                                 
                if (!SDIO_SUCCESS(status)) {
                    if ((SDIO_STATUS_UNSUPPORTED == status) && 
                        (OMAP_DMA_COMMON == pDevice->DmaMode)){
                            /* if we tried common buffer, the length may be unaligned, 
                             * punt it to PIO mode */    
                        pDevice->DmaMode = OMAP_DMA_NONE;
                        status = SDIO_STATUS_SUCCESS;
                            /* fall through */
                    } else {
                            /* fail the request */
                        break;    
                    }
                } else {
                        /* we are doing DMA */
                    status = SetUpHCDDMA(pDevice, 
                                         pReq, 
                                         DMACompletion,
                                         pDevice);
                    if (!SDIO_SUCCESS(status)) {
                        break;    
                    } 
                }    
            }
 
        	DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdRequest:(1) blen: %d, nblk: %d\n",
                   READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_LENGTH), 
                   READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT)));
        }  
       
            /* set the argument register */
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_ARG_LOW, (UINT16)(pReq->Argument & 0xFFFF));
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_ARG_HI,  (UINT16)((pReq->Argument & 0xFFFF0000) >> 16));
            /* set the command */
        temp |= (pReq->Command & OMAP_REG_MMC_CMD_MASK);
        DBG_PRINT(OMAP_TRACE_REQUESTS, 
                  ("SDIO OMAP HcdRequest CMDDAT:0x%X (RespType:%d, Command:0x%X , Arg:0x%X) \n",
                  temp, GET_SDREQ_RESP_TYPE(pReq->Flags), pReq->Command, pReq->Argument));
        
            /* set command timeout */
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD_TIMEOUT, pDevice->TimeOut);
                                                 
        if ((SDHCD_GET_OPER_CLOCK(pHcd) < pDevice->ClockSpinLimit) && 
            (pReq->Command != CMD3)) {
                /* clock rate is very low, need to use interrupts here */
            UnmaskIrq(pDevice, 
                      OMAP_REG_MMC_INTERRUPT_ERRORS | OMAP_REG_MMC_INTERRUPT_ENABLE_EOC, 
                      FROM_NORMAL);
                      
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD, temp);
    
            status = SDIO_STATUS_PENDING;
            
            if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) {
                DBG_PRINT(OMAP_TRACE_REQUESTS, 
                    ("SDIO OMAP HcdRequest using interrupt for command done.*** with data. (clock:%d, ref:%d)\n",
                    SDHCD_GET_OPER_CLOCK(pHcd),pDevice->ClockSpinLimit));
            } else {
                DBG_PRINT(OMAP_TRACE_REQUESTS, 
                    ("SDIO OMAP HcdRequest using interrupt for command done. (clock:%d, ref:%d) \n",
                    SDHCD_GET_OPER_CLOCK(pHcd),pDevice->ClockSpinLimit));
            }
            
            break;
        }
                
            /* if we get here we are polling, interrupt errors and EOC should be masked */                    
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD, temp);
        
        WAIT_FOR_HC_STATUS(pDevice,
                           OMAP_REG_MMC_MODULE_STATUS_EOC,
                           temp,
                           OMAP_STATUS_CMD_PROCESSING_ERRORS,
                           status,
                           OMAP_COMMAND_DONE_POLLING);

        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO OMAP HCD (cmd-inline) polling failed (sd command:%d,status:%d)\n",
                    pReq->Command,status));
            ResetController(pDevice,TRUE,FROM_NORMAL);
            if (pReq->Command == CMD3) {
                status = SDIO_STATUS_SUCCESS;  
            } else {
                break;  
            }  
        }

        DBG_PRINT(OMAP_TRACE_REQUESTS, 
                    ("SDIO OMAP HCD (cmd-inline) statreg: 0x%X config:0x%X\n",
                   READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS), 
                   READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG)));
                        
        if (temp & OMAP_STATUS_CMD_PROCESSING_ERRORS) {
            status = TranslateSDError(pDevice, pReq, temp);
            if (!SDIO_SUCCESS(status)) {
                break;                
            }
        }
        
        status = ProcessCommandDone(pDevice,pReq,FALSE);
        
    } while (FALSE);   
    
    if (status != SDIO_STATUS_PENDING) {
        pReq->Status = status;          
        EndHCTransfer(pDevice, pReq, FROM_NORMAL);
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags)) {
            DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdRequest deferring completion to work item \n"));
                /* the HCD must do the indication in a separate context and return status pending */
            QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
            return SDIO_STATUS_PENDING;
        } else {        
                /* complete the request */
            DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdRequest Command Done, status:%d \n", status));
        }
        pDevice->Cancel = FALSE;  
    }     

    return status;
} 

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdConfig - HCD configuration handler
  Input:  pHcd - HCD object
          pConfig - configuration setting
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdConfig(PSDHCD pHcd, PSDCONFIG pConfig) 
{
    PSDHCD_DEVICE pDevice = (PSDHCD_DEVICE)pHcd->pContext; 
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16 configSave;
    
    if(pDevice->ShuttingDown) {
        DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdConfig returning canceled\n"));
        return SDIO_STATUS_CANCELED;
    }
        
    switch (GET_SDCONFIG_CMD(pConfig)){
        case SDCONFIG_GET_WP:
            if (WriteProtectSwitchOn(pDevice)) {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 1;
            } else {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 0;    
            }
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
            DBG_PRINT(OMAP_TRACE_REQUESTS, ("SDIO OMAP HcdConfig sending init clocks\n"));
            MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ALL_INT,FROM_NORMAL);
            ReqProcClkStartStop(pDevice, CLOCK_ON);
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD, OMAP_REG_MMC_CMD_INAB);
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS, OMAP_REG_MMC_MODULE_STATUS_ALL);
            while(!(READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS) & OMAP_REG_MMC_MODULE_STATUS_EOC)) 
                ;
            WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS, OMAP_REG_MMC_MODULE_STATUS_EOC);        
            ReqProcClkStartStop(pDevice, CLOCK_OFF);
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                {
                    SDIO_IRQ_MODE_FLAGS irqModeFlags;
                    
                    irqModeFlags = 
                        GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->IRQDetectMode;
                    if (irqModeFlags & IRQ_DETECT_4_BIT) {
                        DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP HcdConfig: 4 Bit IRQ mode \n")); 
                            /* in 4 bit mode, the clock needs to be left on */
                        pDevice->KeepClockOn = TRUE;
                    } else {
                            /* in 1 bit mode, the clock can be left off */
                        pDevice->KeepClockOn = FALSE;   
                    }                   
                }
                pDevice->IrqDetectArmed = TRUE;
                            
                    /* enable SDIO mode IRQ detection */
                configSave = READ_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG);
                configSave |= OMAP_REG_MMC_SDIO_MODE_CONFIG_IRQE; 
                WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG, configSave);
                    /* enable detection IRQ */
                DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP HcdConfig: enable SDIO IRQ\n")); 
                UnmaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ, FROM_NORMAL);
            } else {
                pDevice->KeepClockOn = FALSE; 
                pDevice->IrqDetectArmed = FALSE;
                DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP HcdConfig: disable SDIO IRQ\n")); 
                MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ, FROM_NORMAL);
                configSave = READ_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG);
                configSave &= ~OMAP_REG_MMC_SDIO_MODE_CONFIG_IRQE; 
                WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG, configSave);
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
                /* re-enable IRQ detection */
            DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP HcdConfig - SDIO IRQ re-armed\n"));
                /* make sure status is cleared */
            WRITE_HOST_REG16(pDevice, 
                             OMAP_REG_MMC_MODULE_STATUS, 
                             OMAP_REG_MMC_MODULE_STATUS_CIRQ);
            pDevice->IrqDetectArmed = TRUE;
            UnmaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ, FROM_NORMAL);
            break;
        case SDCONFIG_BUS_MODE_CTRL:
            SetBusMode(pDevice, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
                /* save it in case we have to restore it later */
            memcpy(&pDevice->SavedBusMode,pConfig->pData,sizeof(SDCONFIG_BUS_MODE_DATA));
            break;
        case SDCONFIG_POWER_CTRL:
            DBG_PRINT(OMAP_TRACE_CONFIG, ("SDIO OMAP HcdConfig PwrControl: En:%d, VCC:0x%X \n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            status = SetPowerLevel(pDevice, 
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                     GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask);
            break;
        case SDCONFIG_GET_HCD_DEBUG:
            *((CT_DEBUG_LEVEL *)pConfig->pData) = DBG_GET_DEBUG_LEVEL();
            break;
        case SDCONFIG_SET_HCD_DEBUG:
            DBG_SET_DEBUG_LEVEL(*((CT_DEBUG_LEVEL *)pConfig->pData));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_WARN, ("SDIO OMAP HCD: HcdConfig - unsupported command: 0x%X\n",
                                    GET_SDCONFIG_CMD(pConfig)));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 

void ResetController(PSDHCD_DEVICE pDevice, BOOL Restore, BOOL FromIsr) 
{
    INT ii;
    
    ClockStartStop(pDevice, CLOCK_OFF);
    
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SYSTEM_CONTROL, OMAP_REG_MMC_SYSTEM_CONTROL_SW_RESET);
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SYSTEM_CONTROL, 0);

        /* wait for done */
    for(ii = 0;
        (!(READ_HOST_REG16(pDevice, OMAP_REG_MMC_SYSTEM_STATUS) &  OMAP_REG_MMC_SYSTEM_STATUS_RESET_DONE)) 
        && (ii < 1000);
        ii++);
 
    if (ii >= 1000) {
            /* reset on 1610 is broken, see errata, use alternate approach */
            /* cycle power */
         WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, 0);
         WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, 
                                         OMAP_REG_MMC_MODULE_CONFIG_PWRON | 1);
    }
         
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, 
                     OMAP_REG_MMC_MODULE_CONFIG_MODE_MMCSD | OMAP_REG_MMC_MODULE_CONFIG_PWRON);
                    
         /* configure the SDIO mode */         
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG, 
                     OMAP_REG_MMC_SDIO_MODE_CONFIG_DCR4);
                     
    SetDataTimeout(pDevice, OMAP_DEFAULT_DATA_TIMEOUT);
    
        /* set the default timeouts */
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_CMD_TIMEOUT, pDevice->TimeOut);
        /* clear all status bits, from chip erratta, the status may not clear on a reset */
    WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS, OMAP_REG_MMC_MODULE_STATUS_ALL);

    if (!Restore) {
        return;    
    }    
    
        /* restore bus clock and bus mode */
    SetBusMode(pDevice,&pDevice->SavedBusMode);
    
        /* restore interrupt state */
    if (pDevice->IrqDetectArmed) {
        UnmaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ, FromIsr);
    }
    
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInitialize - Initialize controller
  Input:  pDeviceContext - device context
  Output: 
  Return: 
  Notes: I/O resources must be mapped before calling this function
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdInitialize(PSDHCD_DEVICE pDeviceContext) 
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT16 version;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAP HcdInitialize\n"));
    
        /* reset the controller */
    ResetController(pDeviceContext, FALSE,FROM_NORMAL);

        /* display version info */
    version = READ_HOST_REG16(pDeviceContext, OMAP_REG_MMC_MODULE_REV);
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HcdInitialize: Module Spec verison: %d.%d\n",
              ((version & OMAP_REG_MMC_MODULE_REV_MAJOR_MASK) >> OMAP_REG_MMC_MODULE_REV_MAJOR_SHIFT),
              ((version & OMAP_REG_MMC_MODULE_REV_MINOR_MASK) >> OMAP_REG_MMC_MODULE_REV_MINOR_SHIFT)));
            
    if (pDeviceContext->BaseClock == 0) {
         DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP invalid base clock setting\n"));
         status = SDIO_STATUS_DEVICE_ERROR;
         return status;
    }
            
    DBG_PRINT(SDDBG_TRACE, 
    ("SDIO OMAP Using base clock: %dHz, max bus clock: %dHz, max blocks: %d max bytes per block: %d\n",
                            pDeviceContext->BaseClock, 
                            pDeviceContext->Hcd.MaxClockRate,
                            pDeviceContext->Hcd.MaxBlocksPerTrans,
                            pDeviceContext->Hcd.MaxBytesPerBlock));
                                
    DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP HcdInitialize: SlotVoltageCaps: 0x%X, MaxSlotCurrent: 0x%X\n",
                        (UINT)pDeviceContext->Hcd.SlotVoltageCaps, (UINT)pDeviceContext->Hcd.MaxSlotCurrent));
              
    /* interrupts will get enabled by the caller after all of the OS dependent work is done */
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HcdInitialize\n"));
    return status;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdDeinitialize - deactivate controller
  Input:  pDeviceContext - context
  Output: 
  Return: 
  Notes:
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void HcdDeinitialize(PSDHCD_DEVICE pDeviceContext)
{
    PSDREQUEST pReq;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO OMAP HcdDeinitialize\n"));
 
    
    pReq = GET_CURRENT_REQUEST(&pDeviceContext->Hcd);    
    
    if (pReq != NULL) {
        pReq->Status = SDIO_STATUS_CANCELED;
        DBG_PRINT(SDDBG_TRACE, 
        ("SDIO OMAP HcdDeinitialize - cancelling request. (command:%d) mod status:0x%X, IRQ Enables:0x%X\n",
        pReq->Command,  (UINT)READ_HOST_REG16(pDeviceContext, OMAP_REG_MMC_MODULE_STATUS),
        (UINT)READ_HOST_REG16(pDeviceContext, OMAP_REG_MMC_INTERRUPT_ENABLE)));
    }     
    
    pDeviceContext->KeepClockOn = FALSE;
    MaskIrq(pDeviceContext, OMAP_REG_MMC_INTERRUPT_ALL_INT, FROM_NORMAL);
    pDeviceContext->ShuttingDown = TRUE;
    ClockStartStop(pDeviceContext, CLOCK_OFF);
    
    if (pReq != NULL) {
        SDIO_HandleHcdEvent(&pDeviceContext->Hcd, EVENT_HCD_TRANSFER_DONE);    
    }
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO OMAP HcdDeinitialize\n"));
}

void EndHCTransfer(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, BOOL FromIsr)
{
      
    if (!SDIO_SUCCESS(pReq->Status) && (pDevice->DmaMode != OMAP_DMA_NONE)) {
            /* DMA may be running cancel the DMA transfer */
        SDCancelDMATransfer(pDevice);    
    }
         
    MaskIrq(pDevice, 
            (OMAP_REG_MMC_INTERRUPT_ALL_INT & ~OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ),FromIsr);
    
    if (!pDevice->KeepClockOn) {
        ReqProcClkStartStop(pDevice, CLOCK_OFF);    
    }
    
    if (!SDIO_SUCCESS(pReq->Status)) {
          switch (pReq->Status) {
            case SDIO_STATUS_BUS_READ_TIMEOUT:
            case SDIO_STATUS_BUS_READ_CRC_ERR: 
            case SDIO_STATUS_BUS_WRITE_ERROR:
            case SDIO_STATUS_BUS_RESP_CRC_ERR: 
                DBG_PRINT(SDDBG_TRACE, ("SDIO OMAP - resetting controller on bus errors (CMD:%d) \n",
                        pReq->Command));
                    /* controller gets stuck on some errors */  
                ResetController(pDevice,TRUE,FromIsr);
                break;
            default:
                break;
        }   
    }
    
    if ((DBG_GET_DEBUG_LEVEL() >= OMAP_TRACE_DATA) && SDIO_SUCCESS(pReq->Status) &&
        IS_SDREQ_DATA_TRANS(pReq->Flags) && (pDevice->DmaMode != OMAP_DMA_SG)) {
        if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {     
            SDLIB_PrintBuffer(pReq->pDataBuffer,(pReq->BlockLen*pReq->BlockCount),"SDIO OMAP - RX DataDump"); 
        } else {
            SDLIB_PrintBuffer(pReq->pDataBuffer,(pReq->BlockLen*pReq->BlockCount),"SDIO OMAP - TX DataDump");     
        }   
    }
        
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdSDInterrupt - process controller interrupt
  Input:  pDeviceContext - context
  Output: 
  Return: TRUE if interrupt was handled
  Notes:
               
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdSDInterrupt(PSDHCD_DEVICE pDevice) 
{
    UINT16      statusErrs,errorMask,statusMask;
    PSDREQUEST  pReq = NULL;
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("+SDIO OMAP ISR handler \n"));
    
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
      
    while (1) {
        
            /* get status */ 
        statusErrs = READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS);
        
        DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR, status: 0x%X \n", 
                  (UINT)statusErrs));
                        
            /* for ISR processing, only deal with interrupts that are actually enabled */
        statusMask = READ_HOST_REG16(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE);
        statusErrs &= statusMask;
            /* ack the status bits we care about */
        WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS, statusErrs);
                     
        DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR, valid status: 0x%X, IRQ Enables:0x%X\n", 
                  (UINT)statusErrs, statusMask));
                  
            /* deal with SDIO interrupts */
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_CIRQ) {
            if (READ_HOST_REG16(pDevice, OMAP_REG_MMC_SDIO_MODE_CONFIG) 
                    & OMAP_REG_MMC_SDIO_MODE_CONFIG_IRQE) {
                        /* this interrupt is level triggered and will remain set until the card interrupt
                       source is cleared. */
                    DBG_PRINT(OMAP_TRACE_SDIO_INT, ("SDIO OMAP ISR - SDIO_IRQ detected\n"));
                        /* ack*/
                    WRITE_HOST_REG16(pDevice, 
                                     OMAP_REG_MMC_MODULE_STATUS, 
                                     OMAP_REG_MMC_MODULE_STATUS_CIRQ);
                    MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_CIRQ, FROM_ISR); 
                    QueueEventResponse(pDevice, WORK_ITEM_SDIO_IRQ);
            } else { 
                DBG_ASSERT_WITH_MSG(FALSE,
                        "SDIO OMAP ISR - unexpected card interrupt!\n");
            }
        }
         
        if (0 == statusErrs) {
                /* nothing to process */
            break; 
        }

        if (NULL == pReq) {
                /* nothing more to do */
            break;    
        } 
        
        errorMask = OMAP_REG_MMC_MODULE_STATUS_CTO  | 
                    OMAP_REG_MMC_MODULE_STATUS_CCRC;
       
        if (IS_SDREQ_DATA_TRANS(pReq->Flags)){
            errorMask |= OMAP_REG_MMC_MODULE_STATUS_DTO | OMAP_REG_MMC_MODULE_STATUS_DCRC; 
        }
                   
        if (statusErrs & errorMask) {
            status = TranslateSDError(pDevice, pReq, (statusErrs & errorMask));
            if (!SDIO_SUCCESS(status)) {
                break;                
            }
        }
        
        
        /* if we reach here, there were no command processing errors */
                    
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_EOC) {
            MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_EOC, FROM_ISR);
            status = ProcessCommandDone(pDevice,
                                        pReq,
                                        TRUE);
            if (!SDIO_SUCCESS(status)) {
                break;    
            }
        }
        
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_AE) {
            DBG_ASSERT(IS_SDREQ_DATA_TRANS(pReq->Flags));
            DBG_ASSERT(IS_SDREQ_WRITE_DATA(pReq->Flags));
            DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR TX Transfer AE\n"));
            
                /* refill the FIFO */
            if (HcdTransferTxData(pDevice, pReq)) {
                    /* fifo contains final data, disable almost empty interrupts */
                MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_AE, FROM_ISR);
                    /* get ready for BRS or EOFB, it has been observed that EOFB can come early 
                     * and mask out the BRS bit, this looks like a controller bug */
                UnmaskIrq(pDevice, 
                          OMAP_REG_MMC_INTERRUPT_ENABLE_BRS | OMAP_REG_MMC_INTERRUPT_ENABLE_EOFB,
                          FROM_ISR);
                DBG_PRINT(OMAP_TRACE_BUSY,
                    ("SDIO OMAP ISR, TX near complete, waiting for BRS or EOFB (bcnt:%d,blen:%d)\n",
                   (UINT)READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT),
                   (UINT)READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_LENGTH))); 
            } else {
                    /* more data to go, if this is a multi-block transfer we want to make sure
                     * the EOFB is cleared for all blocks except the last one, we will
                     * actually wait for EOFB on the last block */
                if (READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT) > 2) {
                    WRITE_HOST_REG16(pDevice, 
                                     OMAP_REG_MMC_MODULE_STATUS, 
                                     OMAP_REG_MMC_MODULE_STATUS_EOFB);
                }    
            }
        }
        
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_AF) {
            DBG_ASSERT(IS_SDREQ_DATA_TRANS(pReq->Flags));
            DBG_ASSERT(!IS_SDREQ_WRITE_DATA(pReq->Flags));
            DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR RX Transfer AF\n"));               
                /* drain the FIFO */ 
            if (HcdTransferRxData(pDevice, pReq, FALSE)) {
                    /* last bit of data remaining, we can wait for BRS */
                MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_AF, FROM_ISR);
                    /* get ready for BRS */
                UnmaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_BRS,FROM_ISR);    
                DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR, RX near complete, waiting for BRS \n"));             
            }
        }
        
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_BRS) {
            DBG_ASSERT(IS_SDREQ_DATA_TRANS(pReq->Flags));
            MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_BRS, FROM_ISR);
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* check for busy on write operations */               
                MicroDelay(10);
                    /* check card enter busy */
                if (!(READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_STATUS) & 
                      OMAP_REG_MMC_MODULE_STATUS_CB)) {
                    DBG_PRINT(OMAP_TRACE_BUSY, ("SDIO OMAP ISR TX Transfer Done - not busy \n"));
                    status = SDIO_STATUS_SUCCESS;
                        /* we are done */
                    break;    
                }
                
                    /* clear status */
                WRITE_HOST_REG16(pDevice, 
                                 OMAP_REG_MMC_MODULE_STATUS,
                                 OMAP_REG_MMC_MODULE_STATUS_CB);
                DBG_PRINT(OMAP_TRACE_BUSY, ("SDIO OMAP ISR TX Transfer Done - waiting on busy release \n"));
                statusErrs &= ~OMAP_REG_MMC_MODULE_STATUS_CB;
                UnmaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_EOFB,FROM_ISR);
                
            } else {
                 DBG_PRINT(OMAP_TRACE_MMC_INT, ("SDIO OMAP ISR RX Transfer Done \n"));                     
                 if (pDevice->DmaMode == OMAP_DMA_NONE) {
                        /* In PIO mode, the FIFO may contain some residue data */
                     HcdTransferRxData(pDevice, pReq, TRUE);
                     DBG_ASSERT(pReq->DataRemaining == 0);
                 }
                 status = SDIO_STATUS_SUCCESS;
                 break;
            }
        }
     
        if (statusErrs & OMAP_REG_MMC_MODULE_STATUS_EOFB) {
            DBG_ASSERT(IS_SDREQ_DATA_TRANS(pReq->Flags));
            DBG_ASSERT(IS_SDREQ_WRITE_DATA(pReq->Flags));
            MaskIrq(pDevice, OMAP_REG_MMC_INTERRUPT_ENABLE_EOFB,FROM_ISR);  
            DBG_PRINT(OMAP_TRACE_BUSY,("SDIO OMAP ISR Card Busy Done (bcnt:%d,blen:%d)\n",
                   (UINT)READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_COUNT),
                   (UINT)READ_HOST_REG16(pDevice, OMAP_REG_MMC_BLOCK_LENGTH)));
                /* the write operation is finally done */             
            status = SDIO_STATUS_SUCCESS;
            break;                 
        }
        
    }
 
    if (status != SDIO_STATUS_PENDING) {
        
        if (SDIO_SUCCESS(status)) {
        
            if (IS_SDREQ_WRITE_DATA(pReq->Flags) && (pReq->Command == CMD53)) {
#if 0

                /* NOTE: the following code was one plausible way to work around the DAT0-busy
                 * issue by placing the controller in TEST_MODE and sampling the DAT0 pin.  However
                 * it was discovered that restoring the controller back to MMCSD mode was not reliable
                 * The controller would stop indicating data transfer completion.
                 * 
                 */
                 
                UINT16 val;
                int    count = 200000;
                
                    /* switch to SYSTEST mode to read back DAT1 pin */
                val = READ_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG);
                val &= ~OMAP_REG_MMC_MODULE_CONFIG_MODE_MASK;
                val |= OMAP_REG_MMC_MODULE_CONFIG_MODE_TEST;
                WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, val);
                
                while (count) {
                    if (READ_HOST_REG16(pDevice, OMAP_REG_MMC_SYSTEM_TEST) & OMAP_REG_MMC_SYSTEM_TEST_D0D) {
                        /* DAT0 went high*/
                        break;    
                    }    
                    MicroDelay(1);
                    count--;
                }  
                
                /* restore */
                val &= ~OMAP_REG_MMC_MODULE_CONFIG_MODE_MASK;
                val |= OMAP_REG_MMC_MODULE_CONFIG_MODE_MMCSD;
                WRITE_HOST_REG16(pDevice, OMAP_REG_MMC_MODULE_CONFIG, val);
      
                if (!count) {
                    status = SDIO_STATUS_BUS_WRITE_ERROR;    
                }          
#else               
                    /* call OS-specific layer to poll on a GPIO pin 
                     * configuring and controlling the GPIO pin is done in the OS_specific layer */
                status = WaitDat0Busy(pDevice);
#endif         

                if (!SDIO_SUCCESS(status)) {
                    DBG_PRINT(SDDBG_ERROR, ("SDIO OMAP Write Busy Timeout ! \n"));
                }
            }   
        }
      
        pReq->Status = status;
        
        EndHCTransfer(pDevice,pReq,FROM_ISR);
        if (OMAP_DMA_NONE == pDevice->DmaMode) {
                /* queue work item to notify bus driver of I/O completion */
            QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE);
        } else {
                /* using some form of DMA */
            if (!SDIO_SUCCESS(status)) {
                    /* EndHCTransfer will cancel DMA, no need to synch with DMA */
                QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
            } else {
                    /* sync request completion with DMA */
                CompleteRequestSyncDMA(pDevice,pReq,status);
            }
        }
    }
    
    DBG_PRINT(OMAP_TRACE_MMC_INT, ("-SDIO OMAP ISR handler \n"));
        
    return TRUE;
}



