// Copyright (c) 2005, 2006 Atheros Communications Inc.
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

@abstract: PXA270 Local Bus SDIO Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "sdio_pxa270hcd.h"

#define CLOCK_ON  TRUE
#define CLOCK_OFF FALSE

void DMATransferComplete(PVOID pContext, SDIO_STATUS Status, BOOL FromIsr);

#define POLL_TIMEOUT 10000000

#define WAIT_FOR_MMC(pHct,pCancel,DoneMask,Error,ErrorMask,Status,Timeout)   \
{                                                                            \
     INT _timeoutCnt = (Timeout);                                            \
     while(!*(pCancel) &&  (_timeoutCnt > 0) &&                              \
            !(READ_MMC_REG((pHct), MMC_STAT_REG) & (DoneMask)) &&            \
            !(*(Error) = READ_MMC_REG((pHct), MMC_STAT_REG) & (ErrorMask))){_timeoutCnt--;} \
     *(Error) = READ_MMC_REG((pHct), MMC_STAT_REG) & (ErrorMask);            \
     if (0 == _timeoutCnt) {(Status) = SDIO_STATUS_DEVICE_ERROR; DBG_ASSERT(FALSE);}       \
}

MMC_CLOCK_TBL_ENTRY MMCClockDivisorTable[MMC_MAX_CLOCK_ENTRIES] =
{
    {19500000,0x00},  /* must be in decending order */
    {9750000,0x01},
    {4880000,0x02},
    {2440000,0x03},
    {1220000,0x04},
    {609000,0x05},
    {304000,0x06}
};
 
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetResponseData - get the response data 
  Input:    pHct - host context
            pReq - the request
  Output: 
  Return: returns status
  Notes: This function returns SDIO_STATUS_SUCCESS for SD mode.  In SPI mode, all cards return 
  response tokens regardless of whether the command is supported or not.  In SD, the response times 
  times-out and we would never reach here.  In SPI mode we query the bus driver to check the SPI
  response and return an appropriate error status to "simulate" timeouts. 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS  GetResponseData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    INT     wordCount;
    INT     byteCount;
    UINT16  readBuffer[8];
    UINT16  *pBuf;
    
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return SDIO_STATUS_SUCCESS;    
    }
    
    if (IS_HCD_BUS_MODE_SPI(&pHct->Hcd)) {
        /* handle SPI oddities */
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {
            case SDREQ_FLAGS_RESP_R2: 
            case SDREQ_FLAGS_RESP_SDIO_R5: 
                    /* this is the special SPI R2 and SPI SDIO R5 responses */
                byteCount = 2; 
                wordCount = 1;
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                    /* SD, MMC, SDIO OCR reading */
                byteCount = 5;
                wordCount = 3;
                break;
            default:
                byteCount = 1;
                wordCount = 1;
                break;    
        } 
    } else {
       
        byteCount = SD_DEFAULT_RESPONSE_BYTES;        
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            byteCount = SD_R2_RESPONSE_BYTES;         
        } 
        wordCount = byteCount / 2;      
    } 
    
        /* start the buffer at the tail and work backwards since responses are sent MSB first
            and shifted into the FIFO  */ 
    pBuf = &readBuffer[(wordCount - 1)];       
    while (wordCount) {
        *pBuf = (UINT16)READ_MMC_REG(pHct, MMC_RES_REG);    
        pBuf--;
        wordCount--;    
    }
    
    if (IS_HCD_BUS_MODE_SPI(&pHct->Hcd)) {   
        switch (byteCount) {
            case 1:
                    /* the single response byte is stuck in the MSB */
                pReq->Response[0] = readBuffer[0] >> 8;
                break;
            case 2:
                    /* extended status token , shifted in last */
                pReq->Response[0] = (UINT8)readBuffer[0];
                    /* response token shifted in first (in the high byte) */
                pReq->Response[1] = (UINT8)(readBuffer[0] >> 8);
                break;
            case 5:         
                    /* offset the read buffer by one byte since we read WORDs from fifo */
                memcpy(&pReq->Response[0],((PUINT8)readBuffer) + 1, 5); 
                break;                
        }          
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {   
            SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO PXA270 - Response Dump (SPI)");    
        } 
            /* the bus driver will determine the appropriate status based on the SPI 
             * token received, the bus driver may return a time-out status for tokens indicating an
             * illegal command */
        return SDIO_CheckResponse(&pHct->Hcd, pReq, SDHCD_CHECK_SPI_TOKEN);    
    }
    
        /* handle normal SD/MMC responses */        
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
        pReq->Response[0] = 0x00;
            /* adjust for lack of CRC */
        memcpy(&pReq->Response[1],readBuffer,byteCount);
    } else {
        memcpy(pReq->Response,readBuffer,byteCount);           
    } 
    if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) { 
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            byteCount = 17; 
        }
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO PXA270 - Response Dump");
    }
    
    return SDIO_STATUS_SUCCESS;  
}
 
void DumpCurrentRequestInfo(PSDHCD_DRIVER_CONTEXT pHct)
{
    if (pHct->Hcd.pCurrentRequest != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - Current Request Command:%d, ARG:0x%8.8X\n",
                  pHct->Hcd.pCurrentRequest->Command, pHct->Hcd.pCurrentRequest->Argument));
        if (IS_SDREQ_DATA_TRANS(pHct->Hcd.pCurrentRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                IS_SDREQ_WRITE_DATA(pHct->Hcd.pCurrentRequest->Flags) ? "WRITE":"READ",
                pHct->Hcd.pCurrentRequest->BlockCount,
                pHct->Hcd.pCurrentRequest->BlockLen,
                pHct->Hcd.pCurrentRequest->DataRemaining));
        }
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  TranslateMMCError - check for an MMC error 
  Input:  MMCStatus - MMC status register value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS TranslateMMCError(PSDHCD_DRIVER_CONTEXT pHct,UINT32 MMCStatus)
{

    if (MMCStatus & MMC_STAT_RESP_CRC_ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - RESP CRC ERROR \n"));
        return SDIO_STATUS_BUS_RESP_CRC_ERR;
    } else if (MMCStatus & MMC_STAT_SPI_RDTKN_ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - SPI RDTKN ERROR \n"));
        return SDIO_STATUS_BUS_READ_TIMEOUT;
    } else if (MMCStatus & MMC_STAT_RDDAT_CRC_ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - READDATA CRC ERROR \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_READ_CRC_ERR;
    } else if (MMCStatus & MMC_STAT_WR_ERROR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - WRITE ERROR \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_WRITE_ERROR;
    } else if (MMCStatus & MMC_STAT_RESP_TIMEOUT) {
        if (pHct->CardInserted) {
                /* hide error if we are polling an empty slot */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - RESPONSE TIMEOUT \n"));
        }
        return SDIO_STATUS_BUS_RESP_TIMEOUT;
    } else if (MMCStatus & MMC_STAT_READ_TIMEOUT) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 - READ TIMEOUT \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_READ_TIMEOUT;
    }
    
    return SDIO_STATUS_DEVICE_ERROR;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  ClockStartStop - MMC clock control
  Input:  pHcd - HCD object
          pReq - request to issue
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void ClockStartStop(PSDHCD_DRIVER_CONTEXT pHct, BOOL On) 
{

    if (On) {
        WRITE_MMC_REG(pHct, MMC_STRPCL_REG, MMC_CLOCK_START);
    } else {
        if (READ_MMC_REG(pHct, MMC_STAT_REG) & MMC_STAT_CLK_ON) {
            WRITE_MMC_REG(pHct, MMC_STRPCL_REG, MMC_CLOCK_STOP); 
                /* wait for clock to stop */
            while (READ_MMC_REG(pHct, MMC_STAT_REG) & MMC_STAT_CLK_ON);
        }  
    }  
          
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  SetBusMode - Set Bus mode
  Input:  pHcd - HCD object
          pMode - mode
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void SetBusMode(PSDHCD_DRIVER_CONTEXT pHct, PSDCONFIG_BUS_MODE_DATA pMode) 
{
    int i;
    int clockIndex;
    
    DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA270 - SetMode\n"));
    
        /* set clock index to the end, the table is sorted this way */
    clockIndex = MMC_MAX_CLOCK_ENTRIES - 1;
    pMode->ActualClockRate = MMCClockDivisorTable[clockIndex].ClockRate;
    for (i = 0; i < MMC_MAX_CLOCK_ENTRIES; i++) {
        if (pMode->ClockRate >= MMCClockDivisorTable[i].ClockRate) {
            pMode->ActualClockRate = MMCClockDivisorTable[i].ClockRate;
            clockIndex = i;
            break; 
        }   
    }
                                        
    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_SPI:
            /* nothing to really do here */
            if (pMode->BusModeFlags & SDCONFIG_BUS_MODE_SPI_NO_CRC) {
                /* caller wants all SPI transactions without CRC */  
            } else {
                /* caller wants all SPI transaction to use CRC */
            }                                          
            break;
        case SDCONFIG_BUS_WIDTH_1_BIT:
            DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA270 - 1-bit bus width\n"));
            pHct->SD4Bit = FALSE;            
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA270 - 4-bit bus width\n"));
            pHct->SD4Bit = TRUE;
            break;
        default:
            break;
    }
   
        /* set the clock divisor */
    WRITE_MMC_REG(pHct, MMC_CLKRT_REG, MMCClockDivisorTable[clockIndex].Divisor);
    
    DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA270 - MMCClock: %d Khz\n", pMode->ActualClockRate));
    
}

BOOL HcdTransferTxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    INT     dataCopy;
    PUINT8  pBuf;
    volatile UINT32 *pFifo;
    BOOL    partial = FALSE;
    
    pFifo = (volatile UINT32 *)((UINT32)GET_MMC_BASE(pHct) + MMC_TXFIFO_REG);
    dataCopy = min(pReq->DataRemaining,(UINT32)MMC_MAX_TXFIFO);   
    pBuf = (PUINT8)pReq->pHcdContext;
   
        /* clear partial flag */
    WRITE_MMC_REG(pHct,MMC_PRTBUF_REG,0); 
         
    if (dataCopy < MMC_MAX_TXFIFO) {
            /* need to set partial flag after we load the fifos */
        partial = TRUE;       
    }
        /* update remaining count */
    pReq->DataRemaining -= dataCopy;
        /* copy to fifo */
    while(dataCopy) { 
        _WRITE_BYTE_REG(pFifo,*pBuf);
        dataCopy--;
        pBuf++;
    }
      
    if (partial) {
            /* partial buffer */
        WRITE_MMC_REG(pHct,MMC_PRTBUF_REG,MMC_PRTBUF_PARTIAL);    
    }
    
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
    if (pReq->DataRemaining) {
        return FALSE; 
    }
    return TRUE;
}

void HcdTransferRxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    INT     dataCopy, thisCopy;
    PUINT8  pBuf;
    volatile UINT32 *pFifo;    
    pFifo = (volatile UINT32 *)((UINT32)GET_MMC_BASE(pHct) + MMC_RXFIFO_REG);
    dataCopy = min(pReq->DataRemaining,(UINT32)MMC_MAX_RXFIFO);   
    pBuf = (PUINT8)pReq->pHcdContext;
    
        /* update remaining count */
    pReq->DataRemaining -= dataCopy; 
    thisCopy = dataCopy;
           /* copy from fifo */
    while(dataCopy) { 
        (*pBuf) = _READ_BYTE_REG(pFifo);
        dataCopy--;
        pBuf++;    
    }  
    
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
}

SDIO_STATUS ProcessCommandDone(PSDHCD_DRIVER_CONTEXT pHct, 
                               PSDREQUEST            pReq,
                               UINT32                HwErrors,
                               BOOL                  FromIsr)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    UINT32      statValue = 0;
    UINT32      errorMask = 0;
    
    do {
        if (HwErrors) {
            status = TranslateMMCError(pHct,HwErrors);               
            if ((HwErrors & MMC_STAT_RESP_CRC_ERR) && 
                (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2)) { 
                if (SDIO_SUCCESS(GetResponseData(pHct,pReq))) {
                    if (pReq->Response[15] & 0x80) {                        
                        DBG_PRINT(SDDBG_WARN, ("SDIO PXA270 Bypass CRC error due to CRC hardware bug on R2 response..\n"));
                        //SDLIB_PrintBuffer(pReq->Response,SD_R2_RESPONSE_BYTES,"SDIO PXA270 - R2 Response Dump"); 
                            /* 270 controller has a bug where bit 127 of an R2 response is not
                             * taken into account when the CRC is calculated */
                        status = SDIO_STATUS_SUCCESS;
                    }       
                } 
            }
            
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 command failure: STAT:0x%X \n",HwErrors));  
                break;
            }
                        
        } else if (pHct->Cancel) {
            status = SDIO_STATUS_CANCELED;   
            break;
        } else {
              /* get the response data for the command */
            status = GetResponseData(pHct,pReq);
            if (!SDIO_SUCCESS(status)) {
                break;   
            }   
        }
        
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 command success:  STAT:0x%X \n",
                                        READ_MMC_REG((pHct), MMC_STAT_REG)));
                                        
        if (!IS_SDREQ_DATA_TRANS(pReq->Flags)) {
                /* all done */
            break;   
        }
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pHct->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
 
        if (!SDIO_SUCCESS(status)) {
            break;
        }
        
        if (pHct->DmaType != PXA_DMA_NONE) {
                /* start DMA */                 
            status = SetUpPXADMA(pHct,
                                 pReq,
                                 DMATransferComplete,
                                 pHct);                                
            break;   
        }
        
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* for writes, we need to pre-load the TX FIFO */
            if (HcdTransferTxData(pHct, pReq)) {
                    /* entire transfer fits inside the fifos */
                if (pReq->Flags & SDREQ_FLAGS_DATA_SHORT_TRANSFER) {
                        /* the requestor has provided us with a hint, we can poll for
                         * completion if it fits in the fifo */   
                    statValue = MMC_STAT_PRG_DONE;
                    errorMask = MMC_STAT_WR_ERROR;                    
                } else {
                    UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS,FromIsr);  
                }   
            } else {
                    /* expecting a TX empty interrupt */  
                UnmaskMMCIrq(pHct, MMC_MASK_TXFIFO_WR,FromIsr);    
            }  
        } else {                
            if (pReq->DataRemaining <= MMC_MAX_RXFIFO) {
                if (pReq->Flags & SDREQ_FLAGS_DATA_SHORT_TRANSFER) {
                     /* the requestor has provided us with a hint, we can poll for
                         * completion since this is less than a FIFOs worth */  
                    statValue = MMC_STAT_DATA_DONE;
                    errorMask = MMC_STAT_RD_ERRORS;
                } else {
                        /* just wait for data transfer done,  we won't get fifo full interrupts  */
                    UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS,FromIsr);   
                }                     
            } else {
                    /* turn on fifo full interrupts */
                UnmaskMMCIrq(pHct, MMC_MASK_RXFIFO_RD,FromIsr); 
            }       
        } 
                  
        if (0 == statValue) {
                /* return pending, if this is not a short transfer */  
            status = SDIO_STATUS_PENDING; 
        } else {
                /* this will be polled in-line */
            status = SDIO_STATUS_SUCCESS;   
        }
    } while (FALSE);
  
    if (statValue != 0) {
        UINT32 temp = 0;
        
        DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Short %s transfer \n",
                                       IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
        {
            volatile BOOL *pCancel;
            pCancel = (volatile BOOL *)&pHct->Cancel;
            
            WAIT_FOR_MMC(pHct,pCancel,statValue,&temp,errorMask, status, POLL_TIMEOUT); 
        }
        
        if (SDIO_SUCCESS(status)) {
            if (temp) { 
                DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 Short Transfer Failure: STAT:0x%X \n",temp));
                status = TranslateMMCError(pHct,temp);  
            } else {
                if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                        /* drain the FIFO on reads */               
                    HcdTransferRxData(pHct,pReq); 
                    DBG_ASSERT(pReq->DataRemaining == 0);    
                }  
            }  
        }    
    }
     
    if (SDIO_STATUS_PENDING == status) { 
        DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Pending %s transfer \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
    }                        
    return status;
}
 
void DMATransferComplete(PVOID pContext, SDIO_STATUS Status, BOOL FromIsr)
{
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)pContext;
    PSDREQUEST            pReq;
    
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    DBG_ASSERT(pReq != NULL);
    
    DBG_PRINT(PXA_TRACE_DATA, 
            ("+SDIO PXA270 %s DMATransferComplete, Status:%d Req:0x%X \n",
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",Status,(UINT32)pReq));
 
    if (!SDIO_SUCCESS(Status)) {
            /* if DMA failed, we need to complete the request here 
             * the SDIO controller ISR will not fire in this case */
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 %s DMATransferComplete failed with status:%d \n",
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",Status)); 
        pReq->Status = Status; 
            /* turn off interrupts and clock */
        MaskMMCIrq(pHct,(MMC_MASK_ALL_INTS & (~MMC_MASK_SDIO_IRQ)),FromIsr); 
        if (!pHct->KeepClockOn) {
            ClockStartStop(pHct, CLOCK_OFF);
        }
            /* complete the request */
        CompleteRequestSyncDMA(pHct,pReq,FromIsr); 
    } else {
        if (pHct->PartialTxFIFO) { 
                /* the DMA transfer completed, but the TX fifo was not completely
                 * filled, we need to set the partial flag so that the controller can
                 * flip the buffer filled by DMA and send it out */ 
            WRITE_MMC_REG(pHct,MMC_PRTBUF_REG,MMC_PRTBUF_PARTIAL);  
        }  
            /* now wait for data trans complete */
        UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS,FromIsr);       
    }
    
    DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Waiting for TRANS_DONE : (IMASK:0x%X) (IREG:0x%X)\n",
       READ_MMC_REG(pHct, MMC_I_MASK_REG) , READ_MMC_REG((pHct), MMC_I_REG_REG)));     
    DBG_PRINT(PXA_TRACE_DATA, ("-SDIO PXA270 DMATransferComplete\n"));  
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdRequest - SD request handler
  Input:  pHcd - HCD object
          pReq - request to issue
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdRequest(PSDHCD pHcd) 
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)pHcd->pContext;
    UINT32                temp = 0;
    PSDREQUEST            pReq;
    
        /* make sure clock is off before we do anything */
    ClockStartStop(pHct, CLOCK_OFF);
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    
        /* reset current DMA type flag */
    pHct->DmaType = PXA_DMA_NONE;
    
    do {
        
        if (IS_SDHCD_SHUTTING_DOWN(pHct)) {
            status = SDIO_STATUS_CANCELED;
            break;    
        }
        
        if (IS_HCD_BUS_MODE_SPI(pHcd)) {
                /* check for CMD0 */
            if (pReq->Command == 0x00) {
                    /* this command must always have a CRC */
                WRITE_MMC_REG(pHct, 
                              MMC_SPI_REG, 
                              SPI_ENABLE_WITH_CRC);          
            } else {
                    /* for all other SPI-mode commands, check bus mode */
                if (IS_HCD_BUS_MODE_SPI_NO_CRC(pHcd)) {
                        /* not running with CRC */
                    WRITE_MMC_REG(pHct, 
                                  MMC_SPI_REG, 
                                  SPI_ENABLE_NO_CRC);      
                } else {
                        /* running with CRC */
                    WRITE_MMC_REG(pHct, 
                                  MMC_SPI_REG, 
                                  SPI_ENABLE_WITH_CRC);     
                }
            }       
        }
     
        if (pHct->SD4Bit) {
            temp |= MMC_CMDAT_SD_4DAT;
        }
        
        if (pHct->SDIrqData) {
            temp |= MMC_CMDAT_SDIO_IRQ_DETECT; 
        }
        
        if (pHct->IssueInitClocks) {
            pHct->IssueInitClocks = FALSE;
            temp |= MMC_CMDAT_80_CLOCKS;    
        }
        
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {    
            case SDREQ_FLAGS_NO_RESP:
                break;
            case SDREQ_FLAGS_RESP_R1:
            case SDREQ_FLAGS_RESP_MMC_R4:        
            case SDREQ_FLAGS_RESP_MMC_R5:
            case SDREQ_FLAGS_RESP_R6:       
                temp |= MMC_CMDDAT_RES_R1_R4_R5;
                break;
            case SDREQ_FLAGS_RESP_R1B:
                temp |= (MMC_CMDDAT_RES_R1_R4_R5 | MMC_CMDAT_RES_BUSY);
                break;
            case SDREQ_FLAGS_RESP_R2:
                temp |= MMC_CMDDAT_RES_R2;
                break;
            case SDREQ_FLAGS_RESP_SDIO_R5:
                if (IS_HCD_BUS_MODE_SPI(pHcd)) {
                        /* sdio R5s in SPI mode is really an R2 in SPI mode */
                    temp |= MMC_CMDDAT_RES_R2;
                } else {
                        /* in SD mode, its an R1 */
                    temp |= MMC_CMDDAT_RES_R1_R4_R5;   
                }
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                temp |= MMC_CMDDAT_RES_R3;
                break;
        }   
    
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS){
            temp |= MMC_CMDDAT_DATA_EN; 
                /* set block length */
            WRITE_MMC_REG(pHct, MMC_BLKLEN_REG, pReq->BlockLen);
            WRITE_MMC_REG(pHct, MMC_NOB_REG_REG, pReq->BlockCount);
            pReq->DataRemaining = pReq->BlockLen * pReq->BlockCount;
            DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                        IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                        pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
                /* check scatter gather DMA */
            if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
                DBG_ASSERT(pHcd->pDmaDescription != NULL);
                DBG_PRINT(PXA_TRACE_DATA, ("               : Data Transfer using Scatter Gather DMA: %d Descriptors\n",
                        pReq->DescriptorCount));  
                pHct->DmaType = PXA_DMA_SCATTER_GATHER; 
                pReq->pHcdContext = NULL; 
            } else {
                    /* non-scatter gather */
                if (pHct->DmaCapable) {
                        /* the PXA270 FIFOs are a bit puny, so we use common buffer DMA (if available) 
                         * to transfer the buffer */
                    if (pReq->DataRemaining > PXA_DMA_THRESHOLD) {
                        pHct->DmaType = PXA_DMA_COMMON_BUFFER;  
                        DBG_PRINT(PXA_TRACE_DATA, ("               : Data Transfer will use common buffer DMA\n"));  
                    }
                } else {
                     DBG_PRINT(PXA_TRACE_DATA, ("               : Data Transfer will use PIO Mode \n"));     
                }
                    /* use the context to hold where we are in the buffer */
                pReq->pHcdContext = pReq->pDataBuffer;
            }
            
            pHct->PartialTxFIFO = FALSE;
            
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                temp |= MMC_CMDDAT_DATA_WR;
                if (pReq->DataRemaining & PXA_TX_PARTIAL_FIFO_MASK) {
                        /* let DMA completion know that this will result in a partial FIFO
                         * fill */
                    pHct->PartialTxFIFO = TRUE;  
                }
            }
            
            if (pHct->DmaType != PXA_DMA_NONE) {
                    /* enable DMA */
                temp |= MMC_CMDAT_DMA_ENABLE; 
            }
        }  
       
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 CMDDAT:0x%X (RespType:%d, Command:0x%X , Arg:0x%X) \n",
                  temp, GET_SDREQ_RESP_TYPE(pReq->Flags), pReq->Command, pReq->Argument));
        
        WRITE_MMC_REG(pHct, MMC_CMD_REG, pReq->Command);
        WRITE_MMC_REG(pHct, MMC_ARGH_REG, (pReq->Argument >> 16));
        WRITE_MMC_REG(pHct, MMC_ARGL_REG, (pReq->Argument & 0xFFFF));
        WRITE_MMC_REG(pHct, MMC_CMDAT_REG, temp);
        
        if (SDHCD_GET_OPER_CLOCK(pHcd) < HCD_COMMAND_MIN_POLLING_CLOCK) {
                /* clock rate is very low, need to use interrupts here */
            UnmaskMMCIrq(pHct, MMC_MASK_END_CMD, FALSE);
                /* start the clock */
            ClockStartStop(pHct, CLOCK_ON);
            status = SDIO_STATUS_PENDING;
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 using interrupt for command done.. \n"));
        } else {
                /* start the clock */
            ClockStartStop(pHct, CLOCK_ON);        
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 waiting for command done.. \n"));
            temp = 0;
            {
                volatile BOOL *pCancel;
                pCancel = (volatile BOOL *)&pHct->Cancel;
                   /* this macro polls */ 
                WAIT_FOR_MMC(pHct,pCancel,MMC_STAT_END_CMD,&temp,MMC_RESP_ERRORS, status, POLL_TIMEOUT);
                 
            }
            if (SDIO_SUCCESS(status)) {
                    /* process the command completion */
                status = ProcessCommandDone(pHct,pReq,temp,FALSE);
            }
        }
        
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) { 
      
        if (!pHct->KeepClockOn) {     
            ClockStartStop(pHct, CLOCK_OFF);  
        }
        
        pReq->Status = status;
        pHct->Cancel = FALSE;
        
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags) || (pHct->DmaType != PXA_DMA_NONE)) {
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 deferring completion to work item \n"));
                /* the HCD must do the indication in a separate context and return status pending */
            if (PXA_DMA_NONE == pHct->DmaType) {
                QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE); 
            } else {
                CompleteRequestSyncDMA(pHct,pReq,FALSE);  
            }
            return SDIO_STATUS_PENDING;
        } 
            /* complete the request */
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA270 Command Done - inline, status:%d \n", status));      
        /* fall through and return the non-pending status */
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
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)pHcd->pContext;
    UINT16      command;
    
    command = GET_SDCONFIG_CMD(pConfig);
    
    if (IS_SDHCD_SHUTTING_DOWN(pHct)) {
        return SDIO_STATUS_CANCELED;
    }    
    
    switch (command){
        case SDCONFIG_GET_WP:     
            if (IsSlotWPSet(pHct)) {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 1;
            } else {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 0;  
            }  
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
            pHct->IssueInitClocks = TRUE;  
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                    /* enable */
                UnmaskMMCIrq(pHct,MMC_MASK_SDIO_IRQ,FALSE);                
                if (IS_HCD_BUS_MODE_SPI(pHcd)) {
                        /* turn on chip select */
                    ModifyCSForSPIIntDetection(pHct, TRUE); 
                } else {
                    SDIO_IRQ_MODE_FLAGS irqModeFlags;
                        /* get detect mode */
                    irqModeFlags = GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->IRQDetectMode;
                    pHct->SDIrqData = TRUE;
                    if (irqModeFlags & IRQ_DETECT_4_BIT) {
                        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270: 4 Bit IRQ mode \r\n")); 
                            /* in 4 bit mode, the clock needs to be left on */
                        pHct->KeepClockOn = TRUE;
                        if (irqModeFlags & IRQ_DETECT_MULTI_BLK) {
                            // interrupt between blocks 
                            DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270: IRQ between blocks, detect enabled \r\n")); 
                            
                        } else {
                            // no interrupts between blocks
                        }                         
                    } else {
                        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270: 4 Bit IRQ mode \r\n")); 
                            /* in 1 bit mode, the clock can be left off */
                        pHct->KeepClockOn = FALSE;
                    }   
                }             
            } else {
                    /* disable */
                MaskMMCIrq(pHct,MMC_MASK_SDIO_IRQ,FALSE);    
                if (IS_HCD_BUS_MODE_SPI(pHcd)) {
                        /* switch CS */
                   ModifyCSForSPIIntDetection(pHct, FALSE); 
                } 
                pHct->KeepClockOn = FALSE;  
                pHct->SDIrqData = FALSE;   
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
                /* re-enable IRQ detection */
            UnmaskMMCIrq(pHct,MMC_MASK_SDIO_IRQ,FALSE); 
            break;
        case SDCONFIG_BUS_MODE_CTRL:
            SetBusMode(pHct, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
        case SDCONFIG_POWER_CTRL: 
            /* TODO, the slot just connects VCC straight to the slot nothing to adjust here */
            DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA270 PwrControl: En:%d, VCC:0x%X \n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            break;
        case SDCONFIG_GET_HCD_DEBUG:
            *((CT_DEBUG_LEVEL *)pConfig->pData) = DBG_GET_DEBUG_LEVEL();
            break;
        case SDCONFIG_SET_HCD_DEBUG:
            DBG_SET_DEBUG_LEVEL(*((CT_DEBUG_LEVEL *)pConfig->pData));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA270 Local HCD: HcdConfig - bad command: 0x%X\n",command));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInitialize - Initialize MMC controller
  Input:  pHct - HCD context
  Output: 
  Return: 
  Notes: I/O resources must be mapped before calling this function
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdInitialize(PSDHCD_DRIVER_CONTEXT pHct) 
{
        /* turn off clock */
    ClockStartStop(pHct, CLOCK_OFF);
        /* init controller */
    if (pHct->Hcd.Attributes & SDHCD_ATTRIB_BUS_SPI) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Using SPI Mode\n"));
        /* each HCD request will set up SPI mode with or without CRC protection */
    }else if (pHct->Hcd.Attributes & SDHCD_ATTRIB_BUS_1BIT) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA270 Using Normal SD,SDIO Mode\n"));
        WRITE_MMC_REG(pHct, 
                      MMC_SPI_REG, 
                      0x00);         
    }
   
    WRITE_MMC_REG(pHct, MMC_RESTO_REG, SDMMC_RESP_TIMEOUT_CLOCKS);
    WRITE_MMC_REG(pHct, MMC_RDTO_REG, SDMMC_DATA_TIMEOUT_CLOCKS);
    MaskMMCIrq(pHct,MMC_MASK_ALL_INTS, FALSE);
    return SDIO_STATUS_SUCCESS;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdDeinitialize - deactivate MMC controller
  Input:  pHct - HCD context
  Output: 
  Return: 
  Notes:
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void HcdDeinitialize(PSDHCD_DRIVER_CONTEXT pHct)
{
    WRITE_MMC_REG(pHct, MMC_I_MASK_REG, MMC_MASK_ALL_INTS);
    ClockStartStop(pHct, CLOCK_OFF);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdMMCInterrupt - process MMC controller interrupt
  Input:  pHct - HCD context
  Output: 
  Return: TRUE if interrupt was handled
  Notes:
               
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdMMCInterrupt(PSDHCD_DRIVER_CONTEXT pHct) 
{
    UINT32      ints,errors;
    PSDREQUEST  pReq;
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    
    DBG_PRINT(PXA_TRACE_MMC_INT, ("+SDIO PXA270 IMMC Int handler \n"));
    
    ints = READ_MMC_REG(pHct, MMC_I_REG_REG);
    
    if (!ints) {
        DBG_PRINT(SDDBG_ERROR, ("-SDIO PXA270 False Interrupt! \n"));
        return FALSE;   
    }
    
    errors = 0;
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    
    while (1) {
        ints = READ_MMC_REG(pHct, MMC_I_REG_REG);
           /* mask out ones we don't care about */  
        ints &= ~(READ_MMC_REG(pHct, MMC_I_MASK_REG));
        
        if (0 == ints) {
            break;   
        }
        DBG_PRINT(PXA_TRACE_MMC_INT, ("SDIO PXA270 Ints:0x%X \n", ints));
            /* read status */
        errors = READ_MMC_REG(pHct, MMC_STAT_REG); 
         
        if (ints & MMC_INT_SDIO_IRQ) {
             DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO PXA270 SDIO IRQ \n")); 
                /* mask off */
             MaskMMCIrq(pHct,MMC_MASK_SDIO_IRQ,TRUE);          
             QueueEventResponse(pHct, WORK_ITEM_SDIO_IRQ); 
        }             
        
        if (NULL == pReq) {
                /* might just be an SDIO irq */
            break;    
        }
        
        if (ints & MMC_INT_END_CMD) { 
                /* mask off end cmd */
            MaskMMCIrq(pHct, MMC_MASK_END_CMD, TRUE);
                /* only care about response errors */
            errors &= MMC_RESP_ERRORS;
            status = ProcessCommandDone(pHct,pReq,errors,TRUE);
            if (status != SDIO_STATUS_PENDING) {
                    /* no data phase or the command failed, get out */
                break;    
            }            
               /* ProcessCommandDone will turn on interrupts for data transfers */
            continue;
        }
        
            /* if we get here, its a data transfer interrupt */
            
            /* filter data errors */
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            errors &= MMC_STAT_WR_ERROR;    
        } else {
            if (IS_HCD_BUS_MODE_SPI(&pHct->Hcd)) {
                errors &= (MMC_STAT_RD_ERRORS | MMC_STAT_SPI_RDTKN_ERR);    
            } else {
                errors &= MMC_STAT_RD_ERRORS;      
            }  
        }
        
        if (errors) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA- Data Transfer has errors: Stat:0x%X \n", errors));
                /* set status based on error */
            status = TranslateMMCError(pHct,errors);   
            break;  
        }           
                    
        if (ints & MMC_INT_TXFIFO_WR) {  
            DBG_ASSERT(PXA_DMA_NONE == pHct->DmaType);           
                /* transfer data */ 
            if (HcdTransferTxData(pHct, pReq)) {
                MaskMMCIrq(pHct, MMC_MASK_TXFIFO_WR, TRUE); 
                    /* transfer is complete, wait for done */
                UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS, TRUE); 
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 TX Fifo writes done. Waiting for TRANS_DONE \n"));
            } 
            continue;  
        }
        
        if (ints & MMC_INT_RXFIFO_RD) {
            DBG_ASSERT(PXA_DMA_NONE == pHct->DmaType); 
                /* unload fifo */
            HcdTransferRxData(pHct,pReq); 
            if (pReq->DataRemaining < MMC_MAX_RXFIFO) {
                    /* we're done or there is a partial FIFO left */
                MaskMMCIrq(pHct, MMC_MASK_RXFIFO_RD, TRUE); 
                    /* transfer is complete wait for CRC check*/
                UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS, TRUE);    
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 RX Waiting for TRANS_DONE \n"));
            }
            continue; 
        }
        
        if (ints & MMC_INT_DATA_TRANS) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* data transfer done */
                MaskMMCIrq(pHct, MMC_MASK_DATA_TRANS, TRUE); 
                    /* now wait for program done signalling */
                UnmaskMMCIrq(pHct, MMC_MASK_PRG_DONE, TRUE); 
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Transfer done, Waiting for PRG_DONE \n"));
                continue;          
            } else {
                if (PXA_DMA_NONE == pHct->DmaType) {
                    if (pReq->DataRemaining) {
                            /* there was a partial FIFO, we need to drain it */    
                        HcdTransferRxData(pHct,pReq); 
                            /* this should drain it */
                        DBG_ASSERT(pReq->DataRemaining == 0);
                    }
                }
                    /* if we get here without an error, we are done with the read
                     * data operation */
                status = SDIO_STATUS_SUCCESS;
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 RX Transfer done \n"));
                break;   
            }   
        }
        
        if (ints & MMC_INT_PRG_DONE) {
                /* if we get here without errors, we are done with the
                 * write data operation */
            status = SDIO_STATUS_SUCCESS; 
            DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA270 Got TX PRG_DONE. \n"));
            break;
        }
     
    }
   
    if (status != SDIO_STATUS_PENDING) {
            /* set the status */
        pReq->Status = status;
            /* turn off interrupts and clock */
        MaskMMCIrq(pHct,(MMC_MASK_ALL_INTS & (~MMC_MASK_SDIO_IRQ)),TRUE); 
        if (!pHct->KeepClockOn) {
            ClockStartStop(pHct, CLOCK_OFF);
        }
        
        if ((DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_DATA_DUMP) && SDIO_SUCCESS(status) &&
            IS_SDREQ_DATA_TRANS(pReq->Flags) && !IS_SDREQ_WRITE_DATA(pReq->Flags) &&
            (pHct->DmaType != PXA_DMA_SCATTER_GATHER)) {     
            //SDLIB_PrintBuffer(pReq->pDataBuffer,(pReq->BlockLen*pReq->BlockCount),"SDIO PXA270 - RX DataDump");    
        }
        
        if (PXA_DMA_NONE == pHct->DmaType) {
                /* queue work item to notify bus driver of I/O completion */
            QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE);
        } else {
            if (!SDIO_SUCCESS(status)) {
                SDCancelDMATransfer(pHct);
            }
                /* the request used DMA, we need to let the OS-specific code deal with DMA */
            CompleteRequestSyncDMA(pHct,pReq,TRUE);
            
        }
    } 
    
    DBG_PRINT(PXA_TRACE_MMC_INT, ("-SDIO PXA270 IMMC Int handler \n"));
    return TRUE;
}




