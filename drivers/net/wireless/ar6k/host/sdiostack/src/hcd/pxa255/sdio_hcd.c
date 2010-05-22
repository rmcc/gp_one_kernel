// Copyright (c) 2004 Atheros Communications Inc.
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

@abstract: PXA255 Local Bus SDIO Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "sdio_pxa255hcd.h"

#define CLOCK_ON  TRUE
#define CLOCK_OFF FALSE

void Dbg_DumpBuffer(PUCHAR pBuffer, INT Length);

#define WAIT_FOR_MMC(pHct,pCancel,DoneMask,Error,ErrorMask) \
{                                                                            \
     while(!*(pCancel) &&                                                    \
            !(READ_MMC_REG((pHct), MMC_STAT_REG) & (DoneMask)) &&            \
            !(*(Error) = READ_MMC_REG((pHct), MMC_STAT_REG) & (ErrorMask))); \
     *(Error) = READ_MMC_REG((pHct), MMC_STAT_REG) & (ErrorMask);            \
}

MMC_CLOCK_TBL_ENTRY MMCClockDivisorTable[MMC_MAX_CLOCK_ENTRIES] =
{
    {20000000,0x00},  /* must be in decending order */
    {10000000,0x01},
    {5000000,0x02},
    {2500000,0x03},
    {1250000,0x04},
    {625000,0x05},
    {312000,0x06}
};

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  UnmaskMMCIrq - Un mask an MMC interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline void UnmaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask)
{
    UINT32 ints;
    ints = READ_MMC_REG(pHct, MMC_I_MASK_REG);
    ints &= ~Mask;
    WRITE_MMC_REG(pHct, MMC_I_MASK_REG, ints);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MaskMMCIrq - Mask MMC interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline void MaskMMCIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask)
{
    UINT32 ints;
    ints = READ_MMC_REG(pHct, MMC_I_MASK_REG);
    ints |= Mask;
    WRITE_MMC_REG(pHct, MMC_I_MASK_REG, ints); 
}

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
            SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO PXA255 - Response Dump (SPI)");    
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
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO PXA255 - Response Dump");
    }
    
    return SDIO_STATUS_SUCCESS;  
}
 
void DumpCurrentRequestInfo(PSDHCD_DRIVER_CONTEXT pHct)
{
    if (pHct->Hcd.pCurrentRequest != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - Current Request Command:%d, ARG:0x%8.8X\n",
                  pHct->Hcd.pCurrentRequest->Command, pHct->Hcd.pCurrentRequest->Argument));
        if (IS_SDREQ_DATA_TRANS(pHct->Hcd.pCurrentRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
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
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - RESP CRC ERROR \n"));
        return SDIO_STATUS_BUS_RESP_CRC_ERR;
    } else if (MMCStatus & MMC_STAT_SPI_RDTKN_ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - SPI RDTKN ERROR \n"));
        return SDIO_STATUS_BUS_READ_TIMEOUT;
    } else if (MMCStatus & MMC_STAT_RDDAT_CRC_ERR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - READDATA CRC ERROR \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_READ_CRC_ERR;
    } else if (MMCStatus & MMC_STAT_WR_ERROR) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - WRITE ERROR \n"));
        DumpCurrentRequestInfo(pHct);
        return SDIO_STATUS_BUS_WRITE_ERROR;
    } else if (MMCStatus & MMC_STAT_RESP_TIMEOUT) {
        if (pHct->CardInserted) {
                /* hide error if we are polling an empty slot */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - RESPONSE TIMEOUT \n"));
        }
        return SDIO_STATUS_BUS_RESP_TIMEOUT;
    } else if (MMCStatus & MMC_STAT_READ_TIMEOUT) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 - READ TIMEOUT \n"));
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
        CLEAR_TEST_PIN(pHct);  
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
    
    DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA255 - SetMode\n"));
    
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
            break;        
        case SDCONFIG_BUS_WIDTH_4_BIT:
            DBG_ASSERT(FALSE);
            break;
        default:
            break;
    }
   
        /* set the clock divisor */
    WRITE_MMC_REG(pHct, MMC_CLKRT_REG, MMCClockDivisorTable[clockIndex].Divisor);
    
    DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA255 - MMCClock: %d Khz\n", pMode->ActualClockRate));
    
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
        _WRITE_DWORD_REG(pFifo,(UINT32)(*pBuf));
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
        (*pBuf) = (UINT8)_READ_DWORD_REG(pFifo);
        dataCopy--;
        pBuf++;
    }  
    if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_DATA) {     
        SDLIB_PrintBuffer(pReq->pHcdContext,thisCopy,"SDIO PXA255 - RX FIFO Dump");    
    }  
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
}

SDIO_STATUS ProcessCommandDone(PSDHCD_DRIVER_CONTEXT pHct, 
                               PSDREQUEST            pReq,
                               UINT32                HwErrors)
{
    SDIO_STATUS status;
    UINT32      statValue = 0;
    UINT32      errorMask = 0;
    
    if (HwErrors) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 command failure: STAT:0x%X \n",HwErrors));
        status = TranslateMMCError(pHct,HwErrors);
    } else if (pHct->Cancel) {
        status = SDIO_STATUS_CANCELED;   
    } else {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 command success:  STAT:0x%X \n",
                                        READ_MMC_REG((pHct), MMC_STAT_REG)));
            /* get the response data for the command */
        status = GetResponseData(pHct,pReq);
    }
        /* check for data */
    if (SDIO_SUCCESS(status) && IS_SDREQ_DATA_TRANS(pReq->Flags)){
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(&pHct->Hcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
        if (SDIO_SUCCESS(status)) {
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
                        UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS);  
                    }   
                } else {
                        /* expecting a TX empty interrupt */  
                    UnmaskMMCIrq(pHct, MMC_MASK_TXFIFO_WR);    
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
                        UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS);   
                    }                     
                } else {
                        /* turn on fifo full interrupts */
                    UnmaskMMCIrq(pHct, MMC_MASK_RXFIFO_RD); 
                }       
            }
            DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 Pending %s transfer \n",
                                       IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX"));
                                       
            if (0 == statValue) {
                    /* return pending */  
                status = SDIO_STATUS_PENDING; 
            } else {
                    /* this will be polled in-line */
                status = SDIO_STATUS_SUCCESS;   
            }
        }   
    }  
  
    if (statValue != 0) {
        UINT32 temp = 0;
        {
            volatile BOOL *pCancel;
            pCancel = (volatile BOOL *)&pHct->Cancel;
            
            WAIT_FOR_MMC(pHct,pCancel,statValue,&temp,errorMask); 
        }
        
        if (temp) { 
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 Short Transfer Failure: STAT:0x%X \n",temp));
            status = TranslateMMCError(pHct,temp);  
        } else {
            if (!IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* drain the FIFO on reads */               
                HcdTransferRxData(pHct,pReq); 
                DBG_ASSERT(pReq->DataRemaining == 0);    
            }  
        }      
    }
     
    return status;
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
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    
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
        DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                    IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                    pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining));
            /* use the context to hold where we are in the buffer */
        pReq->pHcdContext = pReq->pDataBuffer;
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            temp |= MMC_CMDDAT_DATA_WR; 
        }
    }  
   
    DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 CMDDAT:0x%X (RespType:%d, Command:0x%X , Arg:0x%X) \n",
              temp, GET_SDREQ_RESP_TYPE(pReq->Flags), pReq->Command, pReq->Argument));
    
    
    
    WRITE_MMC_REG(pHct, MMC_CMD_REG, pReq->Command);
    WRITE_MMC_REG(pHct, MMC_ARGH_REG, (pReq->Argument >> 16));
    WRITE_MMC_REG(pHct, MMC_ARGL_REG, (pReq->Argument & 0xFFFF));
    WRITE_MMC_REG(pHct, MMC_CMDAT_REG, temp);
    
    if (SDHCD_GET_OPER_CLOCK(pHcd) < HCD_COMMAND_MIN_POLLING_CLOCK) {
            /* clock rate is very low, need to use interrupts here */
        UnmaskMMCIrq(pHct, MMC_MASK_END_CMD);
            /* start the clock */
        ClockStartStop(pHct, CLOCK_ON);
        status = SDIO_STATUS_PENDING;
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 using interrupt for command done.. \n"));
    } else {
            /* start the clock */
        ClockStartStop(pHct, CLOCK_ON);        
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 waiting for command done.. \n"));
        temp = 0;
        {
            volatile BOOL *pCancel;
            pCancel = (volatile BOOL *)&pHct->Cancel;
               /* this macro polls */ 
            WAIT_FOR_MMC(pHct,pCancel,MMC_STAT_END_CMD,&temp,MMC_RESP_ERRORS); 
        }
            /* process the command completion */
        status = ProcessCommandDone(pHct,pReq,temp);
    }
    
    if (status != SDIO_STATUS_PENDING) {      
        ClockStartStop(pHct, CLOCK_OFF);  
        pReq->Status = status;
        pHct->Cancel = FALSE;
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags)) {
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 deferring completion to work item \n"));
                /* the HCD must do the indication in a separate context and return status pending */
            QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE); 
            return SDIO_STATUS_PENDING;
        }    
            /* complete the request */
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO PXA255 Command Done - inline, status:%d \n", status));      
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
        
    switch (command){
        case SDCONFIG_GET_WP:
            if (GetGpioPinLevel(pHct,SDIO_CARD_WP_GPIO) == WP_POLARITY) {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 1;
            } else {
                *((SDCONFIG_WP_VALUE *)pConfig->pData) = 0;  
            }            
            break;
        case SDCONFIG_SEND_INIT_CLOCKS:
            ClockStartStop(pHct,CLOCK_ON);
                /* should be at least 80 clocks at our lowest clock setting */
            status = OSSleep(100);
            ClockStartStop(pHct,CLOCK_OFF);          
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                status = EnableDisableSDIOIrq(pHct, TRUE); 
                if (SDIO_SUCCESS(status) && IS_HCD_BUS_MODE_SPI(pHcd)) {
                        /* turn on chip select */
                    ModifyCSForSPIIntDetection(pHct, TRUE); 
                }
            } else {
                status = EnableDisableSDIOIrq(pHct, FALSE);  
                if (IS_HCD_BUS_MODE_SPI(pHcd)) {
                        /* switch CS */
                   ModifyCSForSPIIntDetection(pHct, FALSE); 
                } 
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
                /* re-enable IRQ detection */
            AckSDIOIrq(pHct);
            break;
        case SDCONFIG_BUS_MODE_CTRL:
            SetBusMode(pHct, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
        case SDCONFIG_POWER_CTRL:
            /* TODO, the slot just connects VCC straight to the slot nothing to adjust here */
            DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO PXA255 PwrControl: En:%d, VCC:0x%X \n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR, ("SDIO PXA255 Local HCD: HcdConfig - bad command: 0x%X\n",command));
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
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA255 Using SPI Mode\n"));
        /* each HCD request will set up SPI mode with or without CRC protection */
    }else if (pHct->Hcd.Attributes & SDHCD_ATTRIB_BUS_1BIT) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO PXA255 Using 1-bit MMC Mode\n"));
        WRITE_MMC_REG(pHct, 
                      MMC_SPI_REG, 
                      0x00);        
    }
   
    WRITE_MMC_REG(pHct, MMC_RESTO_REG, SDMMC_RESP_TIMEOUT_CLOCKS);
    WRITE_MMC_REG(pHct, MMC_RDTO_REG, SDMMC_DATA_TIMEOUT_CLOCKS);
    MaskMMCIrq(pHct,MMC_MASK_ALL_INTS);
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
    EnableDisableSDIOIrq(pHct, FALSE);
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
    
    DBG_PRINT(PXA_TRACE_MMC_INT, ("+SDIO PXA255 IMMC Int handler \n"));
    
    ints = READ_MMC_REG(pHct, MMC_I_REG_REG);
    
    if (!ints) {
        DBG_PRINT(SDDBG_ERROR, ("-SDIO PXA255 False Interrupt! \n"));
        return FALSE;   
    }
    
    errors = 0;
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    
    while ((ints = READ_MMC_REG(pHct, MMC_I_REG_REG))){
        DBG_PRINT(PXA_TRACE_MMC_INT, ("SDIO PXA255 Ints:0x%X \n", ints));
       
            /* read status */
        errors = READ_MMC_REG(pHct, MMC_STAT_REG);   
                     
        if (ints & MMC_INT_END_CMD) { 
                /* mask off end cmd */
            MaskMMCIrq(pHct, MMC_MASK_END_CMD); 
                /* only care about response errors */
            errors &= MMC_RESP_ERRORS;
            status = ProcessCommandDone(pHct,pReq,errors);
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
                /* transfer data */ 
            if (HcdTransferTxData(pHct, pReq)) {
                MaskMMCIrq(pHct, MMC_MASK_TXFIFO_WR); 
                    /* transfer is complete, wait for done */
                UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS); 
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 TX Fifo writes done. Waiting for TRANS_DONE \n"));
            } 
            continue;  
        }
        
        if (ints & MMC_INT_RXFIFO_RD) {
                /* unload fifo */
            HcdTransferRxData(pHct,pReq);
            if (pReq->DataRemaining < MMC_MAX_RXFIFO) {
                    /* we're done or there is a partial FIFO left */
                MaskMMCIrq(pHct, MMC_MASK_RXFIFO_RD); 
                    /* transfer is complete wait for CRC check*/
                UnmaskMMCIrq(pHct, MMC_MASK_DATA_TRANS);    
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 RX Waiting for TRANS_DONE \n"));
            }
            continue; 
        }
        
        if (ints & MMC_INT_DATA_TRANS) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* data transfer done */
                MaskMMCIrq(pHct, MMC_MASK_DATA_TRANS); 
                    /* now wait for program done signalling */
                UnmaskMMCIrq(pHct, MMC_MASK_PRG_DONE); 
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 Transfer done, Waiting for PRG_DONE \n"));
                continue;          
            } else {
                if (pReq->DataRemaining) {
                        /* there was partial FIFO, we need to drain it */    
                    HcdTransferRxData(pHct,pReq); 
                        /* this should drain it */
                    DBG_ASSERT(pReq->DataRemaining == 0);
                }
                    /* if we get here without an error, we are done with the read
                     * data operation */
                status = SDIO_STATUS_SUCCESS;
                DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 RX Transfer done \n"));
                break;   
            }   
        }
        
        if (ints & MMC_INT_PRG_DONE) {
                /* if we get here without errors, we are done with the
                 * write data operation */
            status = SDIO_STATUS_SUCCESS; 
            DBG_PRINT(PXA_TRACE_DATA, ("SDIO PXA255 Got TX PRG_DONE. \n"));
            break;
        }
     
    }
   
    if (status != SDIO_STATUS_PENDING) {
            /* set the status */
        pReq->Status = status;
            /* turn off interrupts and clock */
        MaskMMCIrq(pHct, MMC_MASK_ALL_INTS); 
        ClockStartStop(pHct, CLOCK_OFF);
            /* queue work item to notify bus driver of I/O completion */
        QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE);
    }
    
    DBG_PRINT(PXA_TRACE_MMC_INT, ("-SDIO PXA255 IMMC Int handler \n"));
    return TRUE;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetGpioPinLevel - get gpio pin level
  Input:  pHct - HCD context
          Pin - gpio pin number
  Output: 
  Return: TRUE if  pin level is high, low otherwise
  Notes:
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL GetGpioPinLevel(PSDHCD_DRIVER_CONTEXT pHct, INT Pin)
{
    UINT32  gpio_pinlvl_offset = 0;
    UINT32  gpio_state;
    UINT32  mask;

    mask =  (1 << (Pin % 32));
    gpio_pinlvl_offset = (Pin / 32);
    gpio_pinlvl_offset = GPIO_GPLR0 + gpio_pinlvl_offset*4;
    
    gpio_state = READ_GPIO_REG(pHct, gpio_pinlvl_offset);
//??    DBG_PRINT(SDDBG_TRACE, ("+-gpio pin:%d, mask:0x%X, offset:0x%X, read:0x%X \n",
//??       Pin,mask,gpio_pinlvl_offset,gpio_state));
    return ((gpio_state & mask) ? TRUE : FALSE);
}



