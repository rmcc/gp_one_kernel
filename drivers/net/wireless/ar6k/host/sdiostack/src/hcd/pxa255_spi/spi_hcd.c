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
@file: spi_hcd.c

@abstract: PXA255 Local Bus SDIO (SPI Only) Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include "spi_pxa255hcd.h"

SPI_CLOCK_TBL_ENTRY SPIClockDivisorTable[SPI_MAX_CLOCK_ENTRIES] =
{
    {20000000,0x00},  /* must be in decending order */
};

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  UnmaskSPIIrq - Un mask an SPI interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline void UnmaskSPIIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask)
{
   
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  MaskSPIIrq - Mask SPI interrupts
  Input:    pHct - host controller
            Mask - mask value
  Output: 
  Return: 
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static inline void MaskSPIIrq(PSDHCD_DRIVER_CONTEXT pHct, UINT32 Mask)
{
   
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  GetResponseData - get the response data 
  Input:    pHct - host context
            pReq - the request
  Output: 
  Return: returns status
  Notes: 
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS  GetResponseData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
  
#if 0 
    INT     wordCount;
    INT     byteCount;
    UINT16  readBuffer[8];
    UINT16  *pBuf;
    
    if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_NO_RESP) {
        return SDIO_STATUS_SUCCESS;    
    }
    
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
   
    
  
    if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) { 
        if (GET_SDREQ_RESP_TYPE(pReq->Flags) == SDREQ_FLAGS_RESP_R2) {
            byteCount = 17; 
        }
        SDLIB_PrintBuffer(pReq->Response,byteCount,"SDIO PXA255 - Response Dump");
    }
#endif
    
    return SDIO_STATUS_SUCCESS;  
}
 
void DumpCurrentRequestInfo(PSDHCD_DRIVER_CONTEXT pHct)
{
    if (pHct->Hcd.pCurrentRequest != NULL) {
        DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - Current Request Command:%d, ARG:0x%8.8X\n",
            pHct->Hcd.pCurrentRequest->Command, pHct->Hcd.pCurrentRequest->Argument));    
        if (IS_SDREQ_DATA_TRANS(pHct->Hcd.pCurrentRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                IS_SDREQ_WRITE_DATA(pHct->Hcd.pCurrentRequest->Flags) ? "WRITE":"READ",
                pHct->Hcd.pCurrentRequest->BlockCount,
                pHct->Hcd.pCurrentRequest->BlockLen,
                pHct->Hcd.pCurrentRequest->DataRemaining));
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
    
    DBG_PRINT(PXA_TRACE_CONFIG , ("SPI PXA255 - SetMode\n")); 
    
        /* set clock index to the end, the table is sorted this way */
    clockIndex = SPI_MAX_CLOCK_ENTRIES - 1;
    pMode->ActualClockRate = SPIClockDivisorTable[clockIndex].ClockRate;
    for (i = 0; i < SPI_MAX_CLOCK_ENTRIES; i++) {
        if (pMode->ClockRate >= SPIClockDivisorTable[i].ClockRate) {
            pMode->ActualClockRate = SPIClockDivisorTable[i].ClockRate;
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
        case SDCONFIG_BUS_WIDTH_4_BIT:
            DBG_ASSERT(FALSE);
            break;
        default:
            break;
    }
         
    DBG_PRINT(PXA_TRACE_CONFIG , ("SPI PXA255 - SPI Clock: %d Khz\n", pMode->ActualClockRate));  
}

BOOL HcdTransferTxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
#if 0
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
#endif
    return TRUE;
}

void HcdTransferRxData(PSDHCD_DRIVER_CONTEXT pHct, PSDREQUEST pReq)
{
  
#if 0
    INT     dataCopy;
    PUINT8  pBuf;
    volatile UINT32 *pFifo;
    
    pFifo = (volatile UINT32 *)((UINT32)GET_MMC_BASE(pHct) + MMC_RXFIFO_REG);
    dataCopy = min(pReq->DataRemaining,(UINT32)MMC_MAX_RXFIFO);   
    pBuf = (PUINT8)pReq->pHcdContext;
    
        /* update remaining count */
    pReq->DataRemaining -= dataCopy;
       /* copy from fifo */
    while(dataCopy) { 
        (*pBuf) = (UINT8)_READ_DWORD_REG(pFifo);
        dataCopy--;
        pBuf++;
    }
        /* update pointer position */
    pReq->pHcdContext = (PVOID)pBuf;
#endif
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
#if 0
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;

    PSDHCD_DRIVER_CONTEXT pHct = (PSDHCD_DRIVER_CONTEXT)pHcd->pContext;
    UINT32                temp = 0;
    PSDREQUEST            pReq;
    
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_ASSERT(pReq != NULL);
    
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
        DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 %s Data Transfer, Blocks:%d, BlockLen:%d, Total:%d \n",
                    IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX",
                    pReq->BlockCount, pReq->BlockLen, pReq->DataRemaining);
            /* use the context to hold where we are in the buffer */
        pReq->pHcdContext = pReq->pDataBuffer;
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            temp |= MMC_CMDDAT_DATA_WR; 
        }
    }  
   
    DBG_PRINT(PXA_TRACE_REQUESTS, "SDIO PXA255 CMDDAT:0x%X (RespType:%d, Command:0x%X , Arg:0x%X) \n",
        temp, GET_SDREQ_RESP_TYPE(pReq->Flags), pReq->Command, pReq->Argument);

 
    /*issue command here..... */
   
    
    DBG_PRINT(PXA_TRACE_REQUESTS, "SDIO PXA255 waiting for command done.. \n");
    {
        volatile BOOL *pCancel;
        pCancel = (volatile BOOL *)&pHct->Cancel;
        WAIT_FOR_MMC(pHct,pCancel,MMC_STAT_END_CMD,&temp,MMC_RESP_ERRORS); 
    }
        
    if (temp) {
        status = TranslateMMCError(pHct,temp);
    } else if (pHct->Cancel) {
        status = SDIO_STATUS_CANCELED;   
    } else {
        DBG_PRINT(PXA_TRACE_REQUESTS, "SDIO PXA255 command poll done STAT:0x%X \n",
                READ_MMC_REG((pHct), MMC_STAT_REG));
            /* get the response data for the command */
        status = GetResponseData(pHct,pReq);
    }
        /* check for data */
    if (SDIO_SUCCESS(status) && (pReq->Flags & SDREQ_FLAGS_DATA_TRANS)){
       
            /* check with the bus driver if it is okay to continue with data */
        status = SDIO_CheckResponse(pHcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
        if (SDIO_SUCCESS(status)) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                if ((pReq->Command == 53) && IS_SDREQ_WRITE_DATA(pReq->Flags) && 
                    IS_HCD_BUS_MODE_SPI(&pHct->Hcd)){              
                    DBG_PRINT(SDDBG_TRACE, "SDIO PXA255 - Test Trig on \n");
                    SET_TEST_PIN(pHct);
                }
                    /* for writes, we need to pre-load the TX FIFO */
                if (HcdTransferTxData(pHct, pReq)) {
                        /* entire transfer fits inside the fifos */
                    UnmaskSPIIrq(pHct, MMC_MASK_DATA_TRANS);     
                } else {
                        /* expecting a TX empty interrupt */  
                    UnmaskSPIIrq(pHct, MMC_MASK_TXFIFO_WR);    
                }  
            } else {
                if (pReq->DataRemaining <= MMC_MAX_RXFIFO) {
                        /* just wait for data transfer done,  we won't get fifo full interrupts  */
                    UnmaskSPIIrq(pHct, MMC_MASK_DATA_TRANS);    
                } else {
                        /* turn on fifo full interrupts */
                    UnmaskSPIIrq(pHct, MMC_MASK_RXFIFO_RD); 
                }       
            }
            DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 Pending %s transfer \n",
                    IS_SDREQ_WRITE_DATA(pReq->Flags) ? "TX":"RX");
                /* return pending */  
            status = SDIO_STATUS_PENDING; 
        }   
    }

    if (status != SDIO_STATUS_PENDING) {      
        ClockStartStop(pHct, CLOCK_OFF);
        pHct->Hcd.pCurrentRequest->Status = status;
            /* complete the request */
        DBG_PRINT(PXA_TRACE_REQUESTS, "SDIO PXA255 Command Done, status:%d \n", status);
        pHct->Cancel = FALSE;
            /* queue a work item, this is potentially a recursive call */
        QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE);           
    }

#endif 
         
    return SDIO_STATUS_PENDING;
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
#if 0
            ClockStartStop(pHct,CLOCK_ON);
                /* should be at least 80 clocks at our lowest clock setting */
            status = OSSleep(100);
            ClockStartStop(pHct,CLOCK_OFF);    
#endif      
            break;
        case SDCONFIG_SDIO_INT_CTRL:
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                status = EnableDisableSDIOIrq(pHct, TRUE); 
                    /* turn on chip select */
                ModifyCSForSPIIntDetection(pHct, TRUE); 
            } else {
                status = EnableDisableSDIOIrq(pHct, FALSE); 
                   /* switch CS */
                ModifyCSForSPIIntDetection(pHct, FALSE); 
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
            DBG_PRINT(PXA_TRACE_CONFIG , ("SPI PXA255 PwrControl: En:%d, VCC:0x%X \n",
            GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
            GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR, ("SPI PXA255 Local HCD: HcdConfig - bad command: 0x%X\n",command)); 
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

   
   // MaskSPIIrq(pHct,MMC_MASK_ALL_INTS);
  
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
    // MaskSPIIrq(pHct,MMC_MASK_ALL_INTS);
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdSPIInterrupt - process SPI controller interrupt
  Input:  pHct - HCD context
  Output: 
  Return: TRUE if interrupt was handled
  Notes:
               
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
BOOL HcdSPIInterrupt(PSDHCD_DRIVER_CONTEXT pHct) 
{
#if 0
    UINT32      ints,errors;
    PSDREQUEST  pReq;
    SDIO_STATUS status = SDIO_STATUS_PENDING;
    
    DBG_PRINT(PXA_TRACE_SPI_INT, "+SPI PXA255 IMMC Int handler \n"); 
    
    ints = READ_MMC_REG(pHct, MMC_I_REG_REG);
    
    if (!ints) {
        DBG_PRINT(SDDBG_ERROR, "-SPI PXA255 False Interrupt! \n"); 
        return FALSE;   
    }
    
    errors = 0;
    pReq = GET_CURRENT_REQUEST(&pHct->Hcd);
    
    while ((ints = READ_MMC_REG(pHct, MMC_I_REG_REG))){
            /* read status */
        errors = READ_MMC_REG(pHct, MMC_STAT_REG);
            /* filter errors */
        if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
            errors &= MMC_STAT_WR_ERROR;    
        } else {
            errors &= MMC_STAT_RD_ERRORS;   
        }
        
        if (errors) {
            DBG_PRINT(SDDBG_ERROR,"SDIO PXA- Transfer has errors: Stat:0x%X \n",errors);
            break;  
        }           
        
        DBG_PRINT(PXA_TRACE_MMC_INT, "SDIO PXA255 Ints:0x%X \n", ints); 
            
        if (ints & MMC_INT_TXFIFO_WR) {             
                /* transfer data */ 
            if (HcdTransferTxData(pHct, pReq)) {
                MaskSPIIrq(pHct, MMC_MASK_TXFIFO_WR); 
                    /* transfer is complete, wait for done */
                UnmaskSPIIrq(pHct, MMC_MASK_DATA_TRANS); 
                DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 TX Fifo writes done. Waiting for TRANS_DONE \n");     
            } 
            continue;  
        }
        
        if (ints & MMC_INT_RXFIFO_RD) {
                /* unload fifo */
            HcdTransferRxData(pHct,pReq);
            if (pReq->DataRemaining < MMC_MAX_RXFIFO) {
                    /* we're done or there is a partial FIFO left */
                MaskSPIIrq(pHct, MMC_MASK_RXFIFO_RD); 
                    /* transfer is complete wait for CRC check*/
                UnmaskSPIIrq(pHct, MMC_MASK_DATA_TRANS);    
                DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 RX Waiting for TRANS_DONE \n");    
            }
            continue; 
        }
        
        if (ints & MMC_INT_DATA_TRANS) {
            if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
                    /* data transfer done */
                MaskSPIIrq(pHct, MMC_MASK_DATA_TRANS); 
                    /* now wait for program done signalling */
                UnmaskSPIIrq(pHct, MMC_MASK_PRG_DONE); 
                DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 Transfer done, Waiting for PRG_DONE \n");
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
                DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 RX Transfer done \n");
                break;   
            }   
        }
        
        if (ints & MMC_INT_PRG_DONE) {
                /* if we get here without errors, we are done with the
                 * write data operation */
            status = SDIO_STATUS_SUCCESS; 
            DBG_PRINT(PXA_TRACE_DATA, "SDIO PXA255 Got TX PRG_DONE. \n");
            break;
        }
     
    }
   
    if (errors) {
            /* alter status based on error */
        status = TranslateMMCError(pHct,errors);   
    } 
    
    if (status != SDIO_STATUS_PENDING) {
            /* set the status */
        pReq->Status = status;
            /* turn off interrupts and clock */
        MaskSPIIrq(pHct, MMC_MASK_ALL_INTS); 
        ClockStartStop(pHct, CLOCK_OFF);
            /* queue work item to notify bus driver of I/O completion */
        QueueEventResponse(pHct, WORK_ITEM_IO_COMPLETE);
    }
    
    DBG_PRINT(PXA_TRACE_SPI_INT, "-SPI PXA255 IMMC Int handler \n"); 
#endif
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
    return ((gpio_state & mask) ? TRUE : FALSE);
}



