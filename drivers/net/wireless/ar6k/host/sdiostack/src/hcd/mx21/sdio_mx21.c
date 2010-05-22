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
@file: sdio_mx21c

@abstract: iMX21 Local Bus SDIO Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "sdio_mx21.h"

#define CLOCK_ON  TRUE
#define CLOCK_OFF FALSE
#define FROM_ISR     TRUE
#define FROM_NORMAL  FALSE

#define POLL_TIMEOUT 10000000

#define WAIT_FOR_HC_STATUS(pHct,DoneMask,pError,ErrorMask,Status,Timeout)   \
{                                                                            \
     INT _timeoutCnt = (Timeout);                                            \
     while((_timeoutCnt > 0) &&                                               \
            !(READ_HC_REG((pHct), SDHC_STATUS_REG) & (DoneMask)) &&            \
            !(*(pError) = READ_HC_REG((pHct), SDHC_STATUS_REG) & (ErrorMask))){_timeoutCnt--;} \
     *(pError) = READ_HC_REG((pHct), SDHC_STATUS_REG) & (ErrorMask);            \
     if (0 == _timeoutCnt) {(Status) = SDIO_STATUS_DEVICE_ERROR; \
           DBG_PRINT(SDDBG_ERROR, \
           ("SDIO MX21 - status timeout, waiting for %s (stat=0x%X)\n",\
               #DoneMask, READ_HC_REG((pHct), SDHC_STATUS_REG))); \
                             DBG_ASSERT(FALSE);}       \
}
 
#define SD_DEFAULT_RESPONSE_BYTES 6
#define SD_R2_RESPONSE_BYTES      16

void DMATransferComplete(PVOID pContext, SDIO_STATUS Status, BOOL FromIsr);
void ResetController(PSDHCD_DRIVER_CONTEXT pHct, BOOL RestoreHcdSettings, BOOL FromIsr);

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetResponseData - get the response data 
  Input:    pHct - host context
            pReq - the request
  Output: 
  Return:
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void GetResponseData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    INT     wordCount;
    INT     byteCount;
    UINT16  readBuffer[8];
    UINT16  *pBuf;
    
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return;    
    }
    
    byteCount = SD_DEFAULT_RESPONSE_BYTES;        
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
        byteCount = SD_R2_RESPONSE_BYTES;         
    }
         
    wordCount = byteCount >> 1;      
    
        /* start the buffer at the tail and work backwards since responses are sent MSB first
            and shifted into the FIFO  */ 
    pBuf = &readBuffer[(wordCount - 1)];       
    while (wordCount) {
        *pBuf = (UINT16)READ_HC_REG(pHct, SDHC_RES_FIFO_REG);    
        pBuf--;
        wordCount--;    
    }
    
    memcpy(pReq->Response,readBuffer,byteCount);
        /* the CRC is not returned in the FIFO, just zero it out */ 
    pReq->Response[0] = 0x00;         
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            /* on R2 responses, the start token is removed, just stick it in */
        pReq->Response[SD_R2_RESPONSE_BYTES] = 0x3F; 
    }    
    if (DBG_GET_DEBUG_LEVEL() >= SDHC_TRACE_REQUESTS) { 
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            byteCount = 17; 
        }
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO MX21 - Response Dump");
    }
    
    return;  
}
 
void DumpCurrentRequestInfo(PSDHCD_DRIVER_CONTEXT pHct)
{
    PSDREQUEST pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    if (pReq != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - Current Request (0x%X) Command:%d, ARG:0x%8.8X\n",
                  (INT)pReq, pReq->Command, pReq->Argument));
        if (IS_SDREQ_DATA_TRANS(pReq->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "WRITE":"READ",
                pReq->BlockCount,
                pReq->BlockLen,
                pReq->DataRemaining));
        }
    }
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  TranslateHCError - translate error 
  Input:  ErrorStatus - error status register value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS TranslateHCError(PSDHCD_DRIVER_CONTEXT pHct,UINT32 ErrorStatus)
{
    if (ErrorStatus & SDHC_STATUS_RESP_CRC_ERROR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - RESP CRC ERROR \n"));
        return SDIO_STATUS_BUS_RESP_CRC_ERR;
    } else if (ErrorStatus & SDHC_STATUS_READ_CRC_ERROR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - READDATA CRC ERROR \n"));
        DumpCurrentRequestInfo(pHct);
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - READ ERROR (STAT:0x%X:IMASK:0x%X)\n",
                READ_HC_REG(pHct,SDHC_STATUS_REG),READ_HC_REG(pHct, SDHC_INT_MASK_REG)));        
        if (pHct->DmaType != SDHC_DMA_NONE) {
            DumpDmaInfo(pHct);
        }
        return SDIO_STATUS_BUS_READ_CRC_ERR;
    } else if (ErrorStatus & SDHC_STATUS_WRITE_CRC_ERROR) {
        DumpCurrentRequestInfo(pHct);
        
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - WRITE CRC ERROR \n"));
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - WRITE ERROR (STAT:0x%X:IMASK:0x%X)\n",
                READ_HC_REG(pHct,SDHC_STATUS_REG),READ_HC_REG(pHct, SDHC_INT_MASK_REG)));
        
        if (pHct->DmaType != SDHC_DMA_NONE) {
            DumpDmaInfo(pHct);
        }
        return SDIO_STATUS_BUS_WRITE_ERROR;
    } else if (ErrorStatus & SDHC_STATUS_RESP_TIMEOUT) {
        return SDIO_STATUS_BUS_RESP_TIMEOUT;
    } else if (ErrorStatus & SDHC_STATUS_READ_TIMEOUT) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 - READ TIMEOUT \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_READ_TIMEOUT;
    }
      
    return SDIO_STATUS_DEVICE_ERROR;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  ClockStartStop - clock control
  Input:  pHcd - HCD object
          pReq - request to issue
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL _DoClockStartStop(PSDHCD_DRIVER_CONTEXT pHct, BOOL On) 
{
    INT timeout;
    INT retry = 3;
    
    while (retry) {
        timeout = 70000;
        if (On) {
            WRITE_HC_REG(pHct, 
                         SDHC_STR_STP_CLK_REG, 
                         SDHC_STR_STP_CLK_ENABLE | SDHC_STR_STP_CLK_START);
                /* wait for clock to start */
            while (timeout) {
                if ((READ_HC_REG(pHct, SDHC_STATUS_REG) & SDHC_STATUS_CLK_RUN)) {
                    break;    
                }
                timeout--;
            }
        } else {
            WRITE_HC_REG(pHct, 
                         SDHC_STR_STP_CLK_REG, 
                         SDHC_STR_STP_CLK_ENABLE | SDHC_STR_STP_CLK_STOP); 
                /* wait for clock to stop */
            while (timeout) {
                if (!(READ_HC_REG(pHct, SDHC_STATUS_REG) & SDHC_STATUS_CLK_RUN)) {
                    break;    
                }
                timeout--;
            }
        } 
        
        if (0 == timeout) {
            retry--;    
        } else {
            break;    
        }
    } 
    
    if (0 == retry) {
        return FALSE;    
    }   
    return TRUE; 
}

#define ClockStartStop(pHct, On) \
{if (!_DoClockStartStop((pHct),(On))) DBG_ASSERT(FALSE);}

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
    int    i;
    int    clockIndex;
    UINT32 regValue;
    
    DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21 - SetMode\n"));
    
        /* set clock index to the end, the table is sorted this way */
    clockIndex = pHct->ValidClockEntries - 1;
    pMode->ActualClockRate = pHct->ClockDivisorTable[clockIndex].ClockRate;
    for (i = 0; i < pHct->ValidClockEntries; i++) {
        if (pMode->ClockRate >= pHct->ClockDivisorTable[i].ClockRate) {
            pMode->ActualClockRate = pHct->ClockDivisorTable[i].ClockRate;
            clockIndex = i;
            break; 
        }   
    }
                                        
    switch (SDCONFIG_GET_BUSWIDTH(pMode->BusModeFlags)) {
        case SDCONFIG_BUS_WIDTH_1_BIT:
            DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21 - 1-bit bus width\n"));
            pHct->SD4Bit = FALSE;            
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21 - 4-bit bus width\n"));
            pHct->SD4Bit = TRUE;
            break;
        default:
            break;
    }
        /* get the base clock divisor value and preserve */
    regValue = READ_HC_REG(pHct, SDHC_CLK_RATE_REG);
        /* set new value */
    regValue &= ~SDHC_CLK_RATE_PRESCALE_MASK;
    regValue |= pHct->ClockDivisorTable[clockIndex].RegisterValue << SDHC_CLK_RATE_PRESCALE_SHIFT;
        /* set the clock divisor */
    WRITE_HC_REG(pHct, SDHC_CLK_RATE_REG, regValue);
    
    DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21 - SD Clock: %d Hz (CLK_RATE_REG:0x%X)\n", 
            pMode->ActualClockRate,regValue));   
            
    memcpy(&pHct->SavedBusMode,pMode,sizeof(pHct->SavedBusMode));
     
}

BOOL HcdTransferTxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    INT     dataCopy;
    PUINT8  pBuf;
    UINT16  data;
    volatile UINT32 *pFifo;
    
    pFifo = (volatile UINT32 *)(GET_HC_REG_BASE(pHct) + SDHC_BUF_ACCESS_REG);
    dataCopy = min(pReq->DataRemaining,pHct->FifoDepth);
    pBuf = (PUINT8)pReq->pHcdContext;
         
        /* update remaining count */
    pReq->DataRemaining -= dataCopy;
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
        _WRITE_DWORD_REG(pFifo,(UINT32)data);       
    }
         
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
    
    DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 Pending TX Remaining: %d \n",pReq->DataRemaining));
                                  
    if (pReq->DataRemaining) { 
        return FALSE; 
    }
    
    return TRUE;
}

void HcdTransferRxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
    
    INT     dataCopy;
    PUINT8  pBuf;
    UINT16  data;
    volatile UINT32 *pFifo; 
       
    pFifo = (volatile UINT32 *)(GET_HC_REG_BASE(pHct) + SDHC_BUF_ACCESS_REG);
    
        /* read the whole FIFO or up to what is left */
    dataCopy = min(pReq->DataRemaining,pHct->FifoDepth); 
        /* get where we are */  
    pBuf = (PUINT8)pReq->pHcdContext;    
        /* update remaining count */
    pReq->DataRemaining -= dataCopy; 
        /* copy from fifo */
    while (dataCopy) { 
        data = (UINT16)_READ_DWORD_REG(pFifo);
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
    DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 Pending RX Remaining: %d \n",pReq->DataRemaining));
}

SDIO_STATUS ProcessCommandDone(PSDHCD_DRIVER_CONTEXT pHct, 
                               PSDREQUEST            pReq,
                               UINT32                HwErrors,
                               BOOL                  FromIsr)
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    
    do {
        if (HwErrors) {
            status = TranslateHCError(pHct,HwErrors); 
            DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 command failure: STAT:0x%X \n",HwErrors));  
            break;               
        } else {
              /* get the response data for the command */
            GetResponseData(pHct,pReq);
        }
                                                
        if (!IS_SDREQ_DATA_TRANS(pReq->Flags)) {
                /* all done */
            break;   
        }
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pHct->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
 
        if (!SDIO_SUCCESS(status)) {
            break;
        }
        
            /* start up DMA for data transfers */
        if (pHct->DmaType != SDHC_DMA_NONE) {  
                 
            status = SetUpHCDDMA(pHct,
                                 pReq,
                                 DMATransferComplete,
                                 pHct);
                                 
            if (!SDIO_SUCCESS(status)) {
                break;     
            }
        }
        
            /* data transfer pending */
        status = SDIO_STATUS_PENDING;
        
    } while (FALSE);
         
    if (SDIO_STATUS_PENDING == status) { 
        DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 Pending %s transfer \n",
                                   IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
    }        
                    
    return status;
}
 
void EndHCTransfer(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq, BOOL FromIsr)
{
    if (!SDIO_SUCCESS(pReq->Status)) { 
            /* bus responses are normal errors */
        if (pReq->Status != SDIO_STATUS_BUS_RESP_TIMEOUT) {
            ResetController(pHct,TRUE,FromIsr);   
        } else {
                /* a bus response timeout occured, find it what command it was, some commands
                 * will normally time out */
            if (!((pReq->Command == 5) || (pReq->Command == 55) || (pReq->Command == 1))) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Bus Timeout: CMD:%d\n",pReq->Command));  
            }   
        }
    } else {
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) { 
            /* on data transfers, reset the controller, on occasion we 
             * see Write CRC errors  */
            ResetController(pHct,TRUE,FromIsr);
        }  
    }
        /* turn off interrupts (except SDIO IRQs) and clock */
    MaskHcdIrq(pHct,(SDHC_INT_MASK_ALL & (~SDHC_INT_SDIO_MASK)),FromIsr); 
        /* stop the clock, this apparently clears statuses */
    ClockStartStop(pHct, CLOCK_OFF);   
        /* restart the clock if we need interrupt detection */   
    if (pHct->KeepClockOn) {
        ClockStartStop(pHct, CLOCK_ON);      
    }    
    
    if (pHct->SDIOIrqDetectArmed) {
            /* re-arm interrupt detection */
        UnmaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,FromIsr);  
    }
}

void DMATransferComplete(PVOID pContext, SDIO_STATUS Status, BOOL FromIsr)
{
    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)pContext;
    PSDREQUEST            pReq;
    
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    DBG_ASSERT(pReq != NULL);
    
    DBG_PRINT(SDHC_TRACE_DATA, 
            ("+SDIO MX21 %s DMATransferComplete, Status:%d Req:0x%X \n",
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",Status,(UINT32)pReq));
 
    if (!SDIO_SUCCESS(Status)) {
            /* if DMA failed, we need to complete the request here 
             * the SDIO controller ISR will not fire in this case */
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 %s DMATransferComplete failed with status:%d \n",
                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",Status)); 
        pReq->Status = Status; 
            /* turn off interrupts and clock */
        EndHCTransfer(pHct,pReq,FromIsr);
            /* complete the request */
        CompleteRequestSyncDMA(pHct,pReq,FromIsr); 
    } else {
        
        DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 From DMA, StatReg:0x%X IMASK:0x%X \n", 
                        READ_HC_REG(pHct,SDHC_STATUS_REG),
                        READ_HC_REG(pHct, SDHC_INT_MASK_REG)));    
    }
        
    DBG_PRINT(SDHC_TRACE_DATA, ("-SDIO MX21 DMATransferComplete\n"));  
}

PTEXT GetRespString(PSDREQUEST pReq)
{
     switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {    
        case SDREQ_FLAGS_NO_RESP:
            return "NONE";       
        case SDREQ_FLAGS_RESP_SDIO_R5:
            return "SDIO R5";
        case SDREQ_FLAGS_RESP_R1:
            return "R1";
        case SDREQ_FLAGS_RESP_R1B:
            return "R1B";
        case SDREQ_FLAGS_RESP_R2:
            return "R2";
        case SDREQ_FLAGS_RESP_R3:
            return "R3";
        case SDREQ_FLAGS_RESP_SDIO_R4:
            return "SDIO R4";
        case SDREQ_FLAGS_RESP_R6:       
            return "R6";
        default:
            return "Unknown";
    } 
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
    UINT32                irqsToUnMask = 0;
    UINT32                shortTransferStatMask = 0;
    UINT32                shortTransferErrorsMask = 0;
    BOOL                  localIrqMasked = FALSE;
        
        /* make sure clock is off before we do anything */
    ClockStartStop(pHct, CLOCK_OFF);
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    
        /* reset current DMA type flag */
    pHct->DmaType = SDHC_DMA_NONE;
    pHct->CmdProcessed = FALSE;
    
    if (pHct->SDIOIrqDetectArmed) {
            /* mask SDIO interrupt detection if it was armed, bus activity seems to 
             * cause false triggering of SDIO interrupts which the bus driver has to
             * process */
        MaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,FALSE); 
            /* it will be re-enabled on request completion */
    }
        
    do {
        
        if (pHct->SD4Bit) {
                /* only set 4 bit mode for data */
            temp = SDHCD_CMD_DAT_BUS_4BIT;
            pHct->FifoDepth = SDHC_MAX_FIFO_4BIT;
        } else {
            temp = SDHCD_CMD_DAT_BUS_1BIT;    
            pHct->FifoDepth  = SDHC_MAX_FIFO_1BIT;
        }
   
        if (pReq->BlockCount > 1) {
                /* the MX21 by design is broken, for mult-block transfers the block size must
                 * be a multiple of the FIFO depth! */
            if (pReq->BlockLen & (pHct->FifoDepth- 1)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 %s Multi-block transfer has BlockLen:%d, must be a multiple of:%d \n",
                        IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                        pReq->BlockLen, pHct->FifoDepth));
                status = SDIO_STATUS_UNSUPPORTED;
                break;
            }            
        }
        
        if (pHct->IssueInitClocks) {
            pHct->IssueInitClocks = FALSE;
            temp |= SDHCD_CMD_DAT_INIT_CLKS;    
        }
        
        switch (GET_SDREQ_RESP_TYPE(pReq->Flags)) {    
            case SDREQ_FLAGS_NO_RESP:
                break;                      
            case SDREQ_FLAGS_RESP_SDIO_R5:
                    /* an SDIO R5 response is the same as an R1 */
            case SDREQ_FLAGS_RESP_R1:
                temp |= SDHCD_CMD_DAT_RESP_R1R5R6;
                break;
            case SDREQ_FLAGS_RESP_R1B:
                temp |= SDHCD_CMD_DAT_RESP_R1R5R6;
                break;
            case SDREQ_FLAGS_RESP_R2:
                temp |= SDHCD_CMD_DAT_RESP_R2;
                break;
            case SDREQ_FLAGS_RESP_R3:
            case SDREQ_FLAGS_RESP_SDIO_R4:
                temp |= SDHCD_CMD_DAT_RESP_R3R4;
                break;
            case SDREQ_FLAGS_RESP_R6:       
                temp |= SDHCD_CMD_DAT_RESP_R1R5R6;
                break;
            default:
                DBG_ASSERT(FALSE);
                status = SDIO_STATUS_INVALID_PARAMETER;
                break;
        }   
    
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
         
        if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) {
            temp |= SDHCD_CMD_DAT_DATA_ENABLE; 
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {                
                temp |= SDHCD_CMD_DAT_DATA_WRITE;   
            }
                /* set block length */
            WRITE_HC_REG(pHct, SDHC_BLK_LEN_REG, pReq->BlockLen);
            WRITE_HC_REG(pHct, SDHC_NOB_REG, pReq->BlockCount);
            pReq->DataRemaining = pReq->BlockLen * pReq->BlockCount;
            DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                        IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                        pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
                /* check scatter gather DMA */
            if (pReq->Flags & SDREQ_FLAGS_DATA_DMA) {
                DBG_ASSERT(pHcd->pDmaDescription != NULL);
                DBG_PRINT(SDHC_TRACE_DATA, ("               : Data Transfer using Scatter Gather DMA: %d Descriptors\n",
                        pReq->DescriptorCount));  
                pHct->DmaType = SDHC_DMA_SCATTER_GATHER; 
                pReq->pHcdContext = NULL; 
                if (!IsDMAAllowed(pHct, pReq)) {
                    DBG_PRINT(SDDBG_ERROR, 
                      ("SDIO MX21 DMA HCLK Errata, cannot support the current bus width with multi-block transfer \n"));
                        /* see DMA HCLK issues with SD controller 
                         * need to punt this operation to PIO mode to work around
                         * chip errata */
                    status = SDIO_STATUS_UNSUPPORTED;
                    break;
                }
            } else {
                    /* non-scatter gather, could be common buffer or PIO */
                if (IsDMAAllowed(pHct, pReq)) {
                        /* the FIFOs are a bit puny, so we use common buffer DMA (if available) 
                         * to transfer the buffer if the data is larger than a FIFO's worth */
                    if (pReq->DataRemaining > pHct->FifoDepth) {
                        pHct->DmaType = SDHC_DMA_COMMON_BUFFER; 
                    }
                }
                                
                DBG_PRINT(SDHC_TRACE_DATA, ("               : Data Transfer will use %s \n",
                    (pHct->DmaType == SDHC_DMA_COMMON_BUFFER) ? "common buffer DMA" : "PIO Mode"));  
                
                    /* use the context to hold where we are in the buffer */
                pReq->pHcdContext = pReq->pDataBuffer;
                
                    /* check for short transfer optimization */
                if ((pReq->Flags & SDREQ_FLAGS_DATA_SHORT_TRANSFER) &&
                    (pHct->DmaType == SDHC_DMA_NONE) && 
                    (pReq->DataRemaining <= pHct->FifoDepth)) {
                        /* the data will fit in the FIFO and the caller indicates this
                         * transfer will be short */
                    if (IS_SDREQ_WRITE_DATA(pReq->Flags)) { 
                        shortTransferStatMask = SDHC_STATUS_WRITE_DONE;
                        shortTransferErrorsMask = SDHC_STATUS_WRITE_CRC_ERROR;   
                    } else {
                        shortTransferStatMask = SDHC_STATUS_READ_DONE;
                        shortTransferErrorsMask = SDHC_STATUS_READ_TIMEOUT | 
                                                   SDHC_STATUS_READ_CRC_ERROR;   
                    }
                } 
            }
        }  
        
        if (0 == shortTransferStatMask) { 
                /* normal PIO or DMA mode */         
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) { 
                    /* interrupts will be used for data transfer completion */
                irqsToUnMask |= SDHC_INT_WRITE_DONE_MASK;
            } else { 
                    /* interrupts will be used for data transfer completion */       
                irqsToUnMask |= SDHC_INT_DATA_TRANS_DONE_MASK;
            }
            
            if (pHct->DmaType == SDHC_DMA_NONE) {
                    /* data requires multiple FIFO fills, the spec is not clear if
                     * we can pre-load for TX, so we'll wait for a FIFO EMPTY interrupt */
                    /* Enable FIFO full and EMPTY interrupts */
                irqsToUnMask |= SDHC_INT_BUFF_RDY_MASK;  
            }
        } else {
            DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 Short %s transfer (stat:0x%X, errs:0x%X)\n",
                                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX", 
                                shortTransferStatMask,shortTransferErrorsMask ));    
        }
        
        if (SDHCD_GET_OPER_CLOCK(pHcd) < HCD_COMMAND_MIN_POLLING_CLOCK) {
                /* clock rate is very low, need to use interrupts here */
            irqsToUnMask |= SDHC_INT_END_CMD_MASK;
        }
        
        DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 CMD_DAT:0x%X (RespType:%s, Command:0x%X , Arg:0x%X Irqs:0x%X) \n",
                  temp, GetRespString(pReq), pReq->Command, pReq->Argument,irqsToUnMask));
        
        WRITE_HC_REG(pHct, SDHC_CMD_REG, pReq->Command);
        WRITE_HC_REG(pHct, SDHC_ARGH_REG, (pReq->Argument >> 16));
        WRITE_HC_REG(pHct, SDHC_ARGL_REG, (pReq->Argument & 0xFFFF));        
        WRITE_HC_REG(pHct, SDHC_CMD_DAT_REG, temp);
        
        if (irqsToUnMask & SDHC_INT_END_CMD_MASK) {
                /* command/resp uses interrupts */
                /* unmask required interrupts */
            UnmaskHcdIrq(pHct, irqsToUnMask, FALSE);
                /* start the clock */
            ClockStartStop(pHct, CLOCK_ON);
            status = SDIO_STATUS_PENDING;
            DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 using interrupt for command done.. \n"));
            break;            
        }
        
            /* if we get here, we are optimizing the transfer using command done polling */        
        if (irqsToUnMask) {
                /* we can only write to the MASK register once during a transaction, so while
                 * we poll and check processing, we mask the interrupt at the CPU */
            DisableHcdInterrupt(pHct, FROM_NORMAL);
            localIrqMasked = TRUE;
            UnmaskHcdIrq(pHct, irqsToUnMask, FALSE);
        }
            /* start the clock */
        ClockStartStop(pHct, CLOCK_ON);        
        DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 polling for command done.. \n"));
        temp = 0;
        
        if ((pReq->Flags & SDREQ_FLAGS_DATA_TRANS) && 
            (!IS_SDREQ_WRITE_DATA(pReq->Flags))) {
            /* on READ operations, the MX21 has a hardware bug:
             * if the read data arrives early while the controller is
             * reading in the response to the command, the READ_OP_DONE or FIFO_FULL
             * bits will be set and the END_CMD will NEVER be set.  For read operations
             * we must poll for either of END_CMD or READ_OP_DONE or FIFO_FULL */
            WAIT_FOR_HC_STATUS(pHct,
                               (SDHC_STATUS_END_CMD | SDHC_STATUS_READ_DONE | SDHC_STATUS_FIFO_FULL),
                               &temp,
                               SDHC_STATUS_RESP_ERRORS | SDHC_STATUS_RD_WR_ERRORS, 
                               status, 
                               POLL_TIMEOUT);       
        } else {
               /* wait for command done */ 
            WAIT_FOR_HC_STATUS(pHct,
                               SDHC_STATUS_END_CMD,
                               &temp,
                               SDHC_STATUS_RESP_ERRORS, 
                               status, 
                               POLL_TIMEOUT);
        }
        
        if (SDIO_SUCCESS(status)) {
                /* process the command completion */
            status = ProcessCommandDone(pHct,pReq,temp,FALSE);
        }          
        
        if (!SDIO_SUCCESS(status)) {
            break;    
        } 
        
        if (0 == shortTransferStatMask) {
                /* no short data transfer necessary, we're done */
            break;    
        }
        
        /* if we get here, we need to deal with the short transfer DATA phase */
        
            /* reset status for polling again */
        status = SDIO_STATUS_SUCCESS;
        temp = 0;
            /* polling status here to accelerate processing */
        DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 Short %s transfer \n",
                                IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
                                       
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                /* load the buffer */
            HcdTransferTxData(pHct, pReq);
        }  
        
        WAIT_FOR_HC_STATUS(pHct,
                           shortTransferStatMask,
                           &temp,
                           shortTransferErrorsMask, 
                           status,
                           POLL_TIMEOUT); 
        
        if (!SDIO_SUCCESS(status)) {
                /* command timed out */
            break;
        }
        
        if (temp) { 
                /* some error bits were set */
            DBG_PRINT(SDHC_TRACE_REQUESTS, 
                        ("SDIO MX21 Short Transfer Failure: STAT:0x%X \n",temp));
            status = TranslateHCError(pHct,temp); 
            break; 
        }                
            /* no errors */
        if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* drain the FIFO on reads */               
            HcdTransferRxData(pHct,pReq); 
            DBG_ASSERT(pReq->DataRemaining == 0);    
        } 
        
    } while (FALSE);
    
    if (status != SDIO_STATUS_PENDING) { 
        pReq->Status = status;
        EndHCTransfer(pHct,pReq,FROM_NORMAL);
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags) || (pHct->DmaType != SDHC_DMA_NONE)) {
            DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 deferring completion to work item \n"));
                /* the HCD must do the indication in a separate context and return status pending */
            if (SDHC_DMA_NONE == pHct->DmaType) {
                    /* normal deferred completion */
                QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE); 
            } else {
                    /* deferred completion that synchronizes with the DMA controller 
                     * in case it got started */
                CompleteRequestSyncDMA(pHct,pReq,FALSE);  
            }
            status = SDIO_STATUS_PENDING;
        } else {
                /* complete the request */
            DBG_PRINT(SDHC_TRACE_REQUESTS, ("SDIO MX21 Command Done - inline, status:%d \n", status));        
            /* fall through and return the non-pending status */
        }
    }
    
    if (localIrqMasked) {
            /* re-enable if we turned them off */
        EnableHcdInterrupt(pHct, FROM_NORMAL);    
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
        
    switch (command){
        case SDCONFIG_SDIO_REARM_INT:
            pHct->SDIOIrqDetectArmed = TRUE;
                /* re-enable IRQ detection */
            UnmaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,FALSE);  
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {                          
                SDIO_IRQ_MODE_FLAGS irqModeFlags;
                    /* get detect mode */
                irqModeFlags = GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->IRQDetectMode;
                if (irqModeFlags & IRQ_DETECT_4_BIT) {
                    DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21: 4 Bit IRQ mode \r\n")); 
                        /* in 4 bit mode, the clock needs to be left on */
                    pHct->KeepClockOn = TRUE;
                } else {
                    DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21: 1 Bit IRQ mode \r\n")); 
                        /* in 1 bit mode, the clock can be left off */
                    pHct->KeepClockOn = FALSE;
                } 
                    /* enable */
                UnmaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,FALSE);    
                pHct->SDIOIrqDetectArmed = TRUE;    
                pHct->SDIOCardIrqDetectRequested = TRUE;       
            } else {
                    /* disable */
                MaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,FALSE);    
                pHct->KeepClockOn = FALSE;  
                pHct->SDIOIrqDetectArmed = FALSE;
                pHct->SDIOCardIrqDetectRequested = FALSE;
            }
            break;
        case SDCONFIG_GET_WP:     
            if (IsSlotWPSet(pHct)) {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 1;
            } else {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 0;  
            }  
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
                /* the first command will have the 80 clocks */
            pHct->IssueInitClocks = TRUE;      
            break;
        case SDCONFIG_BUS_MODE_CTRL:
                /* reset the controller, there appears to be a FIFO problem when you switch
                 * between 4 and 1 bit modes, some residual data remains in the FIFO */
            ResetController(pHct,TRUE,FROM_NORMAL);
            SetBusMode(pHct, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
        case SDCONFIG_POWER_CTRL: 
            /* the slot just connects VCC straight to the slot nothing to adjust here */
            DBG_PRINT(SDHC_TRACE_CONFIG, ("SDIO MX21 PwrControl: En:%d, VCC:0x%X \n",
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
            DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Local HCD: HcdConfig - bad command: 0x%X\n",command));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 

void ResetController(PSDHCD_DRIVER_CONTEXT pHct, BOOL RestoreHcdSettings, BOOL FromIsr)
{
    INT i;
   
    if (RestoreHcdSettings) {
            /* disable interrupts, when a reset is applied some interrupts
             * are unmasked */
        DisableHcdInterrupt(pHct,FromIsr);    
    }
        /* reset as per spec */    
    WRITE_HC_REG(pHct,SDHC_STR_STP_CLK_REG, SDHC_STR_STP_CLK_RESET);
    WRITE_HC_REG(pHct,SDHC_STR_STP_CLK_REG, SDHC_STR_STP_CLK_RESET | SDHC_STR_STP_CLK_STOP);
    
        /* eight register writes to finish reset cycle */
    for (i = 0; i < 8; i++) {
        WRITE_HC_REG(pHct,SDHC_STR_STP_CLK_REG, SDHC_STR_STP_CLK_STOP);    
    }
        /* set base clock divisor */
    WRITE_HC_REG(pHct,SDHC_CLK_RATE_REG, pHct->BaseClkDivisorReg);      
        /* set response and data timeouts */ 
    WRITE_HC_REG(pHct, SDHC_CMD_RES_TO_REG, SDMMC_RESP_TIMEOUT_CLOCKS);
    WRITE_HC_REG(pHct, SDHC_CMD_READ_TO_REG, SDMMC_DATA_TIMEOUT_CLOCKS);
    
    MaskHcdIrq(pHct,SDHC_INT_MASK_ALL, FromIsr); 
    
    if (RestoreHcdSettings) {
        EnableHcdInterrupt(pHct,FromIsr); 
    }
    
    if (!RestoreHcdSettings) {
        return;    
    }
   
        /* restore the bus mode */
    SetBusMode(pHct,&pHct->SavedBusMode);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInitialize - Initialize host controller
  Input:  pHct - HCD context
  Output: 
  Return: 
  Notes: I/O resources must be mapped before calling this function
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdInitialize(PSDHCD_DRIVER_CONTEXT pHct) 
{
    int     i;
    int     clkdivisor;
    UINT32  actualBaseClk;
    
    if (0 == pHct->Device.PeripheralClockRate) {
        DBG_ASSERT(pHct->Device.PeripheralClockRate != 0);
        return SDIO_STATUS_ERROR;  
    }    
    
    pHct->SDIOIrqDetectArmed = FALSE;
    pHct->SDIOIrqMasked = FALSE;
    pHct->SDIOCardIrqDetectRequested = FALSE;
    pHct->KeepClockOn = FALSE;
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 SDIO Module Revision :0x%X \n",
            READ_HC_REG(pHct, SDHC_REVISION_REG)));
                
    clkdivisor = 1;
    while (1) {
            /* figure out the divisor to set module to <= 20Mhz */
        actualBaseClk = pHct->Device.PeripheralClockRate/clkdivisor;
        if (actualBaseClk <= SDHC_MODULE_MAX_CLK) {
            break;
        }   
        clkdivisor++;
    }
        /* clock divisor is 1 less */
    clkdivisor--;
    
    if (clkdivisor > 16) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 Local HCD: HcdConfig - bad clk divisor: %d for base:%d\n",
            clkdivisor,pHct->Device.PeripheralClockRate));
        return SDIO_STATUS_ERROR;
    }
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 Clock Base:%d Hz, Using Divisor:%d\n",
        pHct->Device.PeripheralClockRate,clkdivisor));
        
        /* save this for resets */
    pHct->BaseClkDivisorReg = clkdivisor;
    
    pHct->Hcd.MaxClockRate = actualBaseClk;
        /* build the clock table */              
        /* the first entry is unity clock */
    pHct->ValidClockEntries = 1;
    pHct->ClockDivisorTable[0].ClockRate = actualBaseClk;
    pHct->ClockDivisorTable[0].RegisterValue = 0; 
        /* the remaining entries are a divisor that is a power of 2 */    
    clkdivisor = 2;
    for (i = 1; i < HCD_MAX_CLOCK_ENTRIES; i++) {
        pHct->ClockDivisorTable[i].ClockRate = actualBaseClk/clkdivisor;
        if (0 == pHct->ClockDivisorTable[i].ClockRate) {
            break;    
        }
        pHct->ClockDivisorTable[i].RegisterValue = 1 << (i - 1);
        clkdivisor <<= 1;        
        pHct->ValidClockEntries++;    
    }
     
    for (i = 0; i <  pHct->ValidClockEntries; i++) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO MX21 Clock Index:%d, Rate:%d Hz, CLKDIV:0x%X\n",
            i,pHct->ClockDivisorTable[i].ClockRate,pHct->ClockDivisorTable[i].RegisterValue));    
    }
    
        /* reset the controller */
    ResetController(pHct,FALSE,FALSE);
    
    MaskHcdIrq(pHct,SDHC_INT_MASK_ALL, FALSE);
    
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
    MaskHcdIrq(pHct,SDHC_INT_MASK_ALL, FALSE);
    ClockStartStop(pHct, CLOCK_OFF);
}

UINT32 GetValidStatusBitsForIRQ(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pCurrentReq) 
{
    UINT32 statusBits = 0;    
    UINT32 ints;
    
    ints = READ_HC_REG(pHct, SDHC_INT_MASK_REG);

    if (ints & SDHC_INT_CARD_DETECT) {
        statusBits |= SDHC_STATUS_CARD_PRESENT;    
    }
    
    if (!(ints & SDHC_INT_SDIO_MASK)) {
        statusBits |= SDHC_STATUS_SDIO_INT;   
    }
    
    if (pCurrentReq != NULL) {
            /* these require a current request */        
            /* command done */
        if (!(ints & SDHC_INT_END_CMD_MASK)) {
            statusBits |= SDHC_STATUS_END_CMD | SDHC_STATUS_RESP_ERRORS;   
        }    
            /* READ/WRITE processing */        
        if (IS_SDREQ_WRITE_DATA(pCurrentReq->Flags)) {
                /* write data */
            if (!(ints & SDHC_INT_BUFF_RDY_MASK)) {
                statusBits |= SDHC_STATUS_FIFO_EMPTY;
            }     
            if (!(ints & SDHC_INT_WRITE_DONE_MASK)) {
                statusBits |= SDHC_STATUS_WRITE_DONE;   
            }   
            statusBits |= SDHC_STATUS_WRITE_CRC_ERROR;
        } else {
            if (!(ints & SDHC_INT_BUFF_RDY_MASK)) {
                statusBits |= SDHC_STATUS_FIFO_FULL;
            }            
            if (!(ints & SDHC_INT_DATA_TRANS_DONE_MASK)) {
                statusBits |= SDHC_STATUS_READ_DONE;   
            }      
            statusBits |= SDHC_STATUS_READ_CRC_ERROR | SDHC_STATUS_READ_TIMEOUT; 
        }  
    }
    DBG_PRINT(SDHC_TRACE_HC_INT, ("SDIO MX21 Valid Stats in IRQ:0x%X \n", statusBits));
    return statusBits;
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInterrupt - process controller interrupt
  Input:  pHct - HCD context
  Output: 
  Return: TRUE if interrupt was handled
  Notes:
               
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdInterrupt(PSDHCD_DRIVER_CONTEXT pHct) 
{
    UINT32      statReg,errors,validStatBits;
    PSDREQUEST  pReq;
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    
    DBG_PRINT(SDHC_TRACE_HC_INT, ("+SDIO MX21 HCD Int handler \n"));
        
    errors = 0;
    
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    
    
    while (1) {
            /* get the status we care about */
        statReg = READ_HC_REG(pHct,SDHC_STATUS_REG);        
        validStatBits = GetValidStatusBitsForIRQ(pHct,pReq);
        DBG_PRINT(SDHC_TRACE_HC_INT, ("SDIO MX21 StatReg:0x%X IMASK:0x%X ValidBits:0x%X\n", 
                        statReg,  READ_HC_REG(pHct, SDHC_INT_MASK_REG), validStatBits));
                        
            /* keep only relevent status bits */
        statReg &= validStatBits;
        
        if (pHct->CmdProcessed) {
                /* mask out command bit, this will stay set until the interrupt mask is written
                 * to or until the clock is stopped */
            statReg &= ~SDHC_STATUS_END_CMD;    
        }
        
        if (0 == statReg) {
            break;   
        }
        
        DBG_PRINT(SDHC_TRACE_HC_INT, ("SDIO MX21 After Mask: StatReg:0x%X  \n", statReg));
               
        if (statReg & SDHC_STATUS_SDIO_INT) {
            DBG_PRINT(SDHC_TRACE_SDIO_INT, ("SDIO MX21 SDIO IRQ (STAT:0x%X:IMASK:0x%X)\n",
                READ_HC_REG(pHct,SDHC_STATUS_REG),READ_HC_REG(pHct, SDHC_INT_MASK_REG)));          
            if (pHct->SDIOIrqMasked) {
                DBG_ASSERT(FALSE);    
                DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 SDIO IRQ (STAT:0x%X:IMASK:0x%X)\n",
                    READ_HC_REG(pHct,SDHC_STATUS_REG),READ_HC_REG(pHct, SDHC_INT_MASK_REG)));          
            }            
            pHct->SDIOIrqDetectArmed = FALSE;
                /* mask off */
            MaskHcdIrq(pHct,SDHC_INT_SDIO_MASK,TRUE);      
            QueueEventResponse(pHct, WORK_ITEM_SDIO_IRQ); 
        }             
        
        if (NULL == pReq) {
                /* might just be an SDIO irq */
            break;    
        }
        
        if (statReg & SDHC_STATUS_END_CMD) { 
                /* set flag to mask END_CMD status bit */
            pHct->CmdProcessed = TRUE;
                /* only care about response errors */
            status = ProcessCommandDone(pHct,
                                        pReq,
                                        (statReg & SDHC_STATUS_RESP_ERRORS),
                                        TRUE);            
            if (status != SDIO_STATUS_PENDING) {
                    /* no data phase or the command failed, get out */
                break;    
            }            
        }
        
        /* if we get here we are processing interrupts associated with DATA */
        
            /* check for any read/write errors */
        if (statReg & SDHC_STATUS_RD_WR_ERRORS) {
            status = TranslateHCError(pHct,(statReg & SDHC_STATUS_RD_WR_ERRORS)); 
            break;    
        }
       
        /* at this point, there are no status errors */       
                   
        if (statReg & SDHC_STATUS_FIFO_EMPTY) {  
                /* write use Fifo EMPTY signal */
            DBG_ASSERT(SDHC_DMA_NONE == pHct->DmaType);  
            DBG_ASSERT(IS_SDREQ_WRITE_DATA(pReq->Flags));         
                /* transfer data */ 
            if (HcdTransferTxData(pHct, pReq)) {
                MaskHcdIrq(pHct,SDHC_INT_BUFF_RDY_MASK, FROM_ISR);
                DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 TX Fifo writes done. Waiting for WRITE_DONE \n"));
            } 
        }
        
        if (statReg & SDHC_STATUS_FIFO_FULL) {
                /* READs use Fifo FULL signal */
            DBG_ASSERT(SDHC_DMA_NONE == pHct->DmaType);
            DBG_ASSERT(!IS_SDREQ_WRITE_DATA(pReq->Flags));  
                /* unload fifo */
            HcdTransferRxData(pHct,pReq); 
            if (pReq->DataRemaining < pHct->FifoDepth) {
                UINT32 temp = 0;
                MaskHcdIrq(pHct,SDHC_INT_BUFF_RDY_MASK, FROM_ISR);
                DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 RX Fifo reads done. waiting for READ_DONE \n"));               
                
                if (pReq->DataRemaining & 0x1) {
                    DBG_PRINT(SDHC_TRACE_DATA, 
                            ("SDIO MX21 RX - Non-WORD aligned remaining bytes:%d \n",pReq->DataRemaining));                            
                    WAIT_FOR_HC_STATUS(pHct,
                                       SDHC_STATUS_READ_DONE | SDHC_STATUS_FIFO_FULL,
                                       &temp,
                                       SDHC_STATUS_RESP_ERRORS | SDHC_STATUS_RD_WR_ERRORS, 
                                       status, 
                                       POLL_TIMEOUT);       
                    if (!SDIO_SUCCESS(status)) {
                        DBG_PRINT(SDDBG_ERROR, ("SDIO MX21 RX Fifo READ DONE Timeout! -- \n"));                                      
                    }
                    if (temp & SDHC_STATUS_RD_WR_ERRORS) {
                        status = TranslateHCError(pHct,(temp & SDHC_STATUS_RD_WR_ERRORS));   
                        break;
                    }
                    status = SDIO_STATUS_SUCCESS; 
                    statReg |= SDHC_STATUS_READ_DONE;
                }
                    /* fall through and let READ done processing continue */   
            }
        }
        
        if (statReg & SDHC_STATUS_READ_DONE) {            
            DBG_ASSERT(!IS_SDREQ_WRITE_DATA(pReq->Flags));
                /* read operation is done */
            if (SDHC_DMA_NONE == pHct->DmaType) {
                if (pReq->DataRemaining) {
                        /* there was a partial FIFO, we need to drain it */    
                    HcdTransferRxData(pHct,pReq); 
                        /* this should drain it */
                    DBG_ASSERT(pReq->DataRemaining == 0);
                }
            }
            status = SDIO_STATUS_SUCCESS;
            DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 READ Transfer done. \n"));
            break;   
        }   
        
        if (statReg & SDHC_STATUS_WRITE_DONE) {            
            DBG_ASSERT(IS_SDREQ_WRITE_DATA(pReq->Flags)); 
                /* write operation is done */
            status = SDIO_STATUS_SUCCESS; 
            DBG_PRINT(SDHC_TRACE_DATA, ("SDIO MX21 WRITE Transfer done. \n"));
            break;
        }
    }
   
    if (status != SDIO_STATUS_PENDING) {
            /* set the status */
        pReq->Status = status;
        EndHCTransfer(pHct,pReq,FROM_ISR);        
        if ((DBG_GET_DEBUG_LEVEL() >= SDHC_TRACE_DATA_DUMP) && SDIO_SUCCESS(status) &&
            IS_SDREQ_DATA_TRANS(pReq->Flags) && !IS_SDREQ_WRITE_DATA(pReq->Flags) &&
            (pHct->DmaType != SDHC_DMA_SCATTER_GATHER)) {     
            SDLIB_PrintBuffer(pReq->pDataBuffer,(pReq->BlockLen*pReq->BlockCount),"SDIO MX21 - RX DataDump");    
        }        
        if (SDHC_DMA_NONE == pHct->DmaType) {
                /* queue work item to notify bus driver of I/O completion */
            QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE);
        } else {
                /* the request used DMA, we need to let the OS-specific code deal with DMA */
            CompleteRequestSyncDMA(pHct,pReq,TRUE);   
        }
    } 
    
    DBG_PRINT(SDHC_TRACE_HC_INT, ("-SDIO MX21 HCD Int handler \n"));
    return TRUE;
}




