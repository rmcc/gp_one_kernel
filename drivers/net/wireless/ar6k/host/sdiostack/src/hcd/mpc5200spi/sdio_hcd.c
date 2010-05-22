// Copyright (c) 2004, 2005 Atheros Communications Inc.
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

@abstract: MPC5200 SDIO SPI Host Controller Driver

#notes: OS independent code
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MODULE_NAME  MPC5200HCD
#include "sdio_mpc5200spi_hcd.h"


void Dbg_DumpBuffer(PUCHAR pBuffer, INT Length);
static SDIO_STATUS SendCommand(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                               PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress);
static SDIO_STATUS SendDataCommand(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                                   PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress);
static SDIO_STATUS HandleDataPhase(PSDHCD_DEVICE pDevice);
static void SendCommand12Completion(PVOID pContext); 
static SDIO_STATUS SendCommand12(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                               PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress);



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
        DBG_PRINT(SDDBG_ERROR, ("SDIO PCI Ellen - Current Request Command:%d, ARG:0x%8.8X\n",
                  pDevice->Hcd.pCurrentRequest->Command, pDevice->Hcd.pCurrentRequest->Argument));
        if (IS_SDREQ_DATA_TRANS(pDevice->Hcd.pCurrentRequest->Flags)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO PCI Ellen - Data %s, Blocks: %d, BlockLen:%d Remaining: %d \n",
                      IS_SDREQ_WRITE_DATA(pDevice->Hcd.pCurrentRequest->Flags) ? "WRITE":"READ",
                      pDevice->Hcd.pCurrentRequest->BlockCount,
                      pDevice->Hcd.pCurrentRequest->BlockLen,
                      pDevice->Hcd.pCurrentRequest->DataRemaining));
        }
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
    DBG_PRINT(PXA_TRACE_CONFIG , ("+SDIO MPC5200 - SetBusMode\n"));
    
        /* set clock index to the end max. divide */
    pMode->ActualClockRate = pMode->ClockRate;
    SDStartStopClock(pDevice, CLOCK_OFF);

    SDSetClock(pDevice, pMode);
    
    DBG_PRINT(PXA_TRACE_CONFIG , ("-SDIO MPC5200 - SetBusMode\n"));
}


/*
 * SendDataCompletion - send data completion routine. Can be called recursively
*/
static void SendDataCompletion(PVOID pContext)
{
    PSDHCD_DEVICE   pDevice = (PSDHCD_DEVICE)pContext;
    SDIO_STATUS     status;
    PSDREQUEST      pReq;
    
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
    if (pReq == NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 SendDataCompletion, no request, Cancelled?, pDevice: 0x%X\n",
                (UINT)pDevice));
        return;
    }

    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendDataCompletion\n"));

    status = HandleDataPhase(pDevice);
    if (status != SDIO_STATUS_PENDING) {  
        pReq->Status = status;
        
        SDStartStopClock(pDevice, CLOCK_OFF);
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendDataCompletion deferring completion to work item \n"));
            /* the HCD must do the indication in a separate context and return status pending */
        QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
    }    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendDataCompletion\n"));
}

/*
 * HandleDataPhase - handle data phase of tarnsfer
 */
static SDIO_STATUS HandleDataPhase(PSDHCD_DEVICE pDevice)
{
    SDIO_STATUS     status;
    PSDREQUEST      pReq;
    UINT            length;
    PUINT8      pDataBuffer = MPC5200_GET_DATA_BUFFER(pDevice);
    DMA_ADDRESS dataDmaAddress = MPC5200_GET_DATA_DMA_BUFFER(pDevice);
    UINT8 tempLastByte = pDataBuffer[pDevice->LastTransferLength-1];

    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
    DBG_ASSERT(pReq != NULL);

    
    if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
        SDLIB_PrintBuffer(pDataBuffer,  pDevice->LastTransferLength, "SDIO MPC5200 HandleDataPhase response buffer(0)");
    }
    if (pDevice->SpiShift) {
        SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength,
                                pDataBuffer, pDevice->LastByte);
        pDevice->LastByte = tempLastByte;
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pDataBuffer, pDevice->LastTransferLength, "SDIO MPC5200 HandleDataPhase response buffer(1)");
        }
    }
    /* go on with the data portion of transfer */
    if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
        /* handle writes */
        length = MPC5200_GET_DATA_BUFFER_SIZE(pDevice);
        status = SDSpi_ProcessDataBlock(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, 
                                        &length, &pDevice->RemainingBytes);                        
//??        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE) && (!pDevice->SpiShift)){
        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE)){
            /* the data is in error, but could be shifted into alignment */
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, pDataBuffer, 0xFF);
            length = MPC5200_GET_DATA_BUFFER_SIZE(pDevice);
            status = SDSpi_ProcessDataBlock(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, 
                                        &length, &pDevice->RemainingBytes);                        
            pDevice->SpiShift = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 HandleDataPhase, bit shifted status: %d\n",
                            status));
        }

        if (status == SDIO_STATUS_PENDING) {
            /* send the request out the SPI port, keep even */
            pDevice->LastByte = tempLastByte;
            pDevice->LastTransferLength = length + pDevice->RemainingBytes;
            status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress,
                                       pDevice->LastTransferLength, SendDataCompletion, pDevice);
        } else {
            /* done with write */
        }
    } else {
        /* handle reads */
        status = SDSpi_ProcessDataBlock(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, 
                                        &length, &pDevice->RemainingBytes);                        
//??        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE) && (!pDevice->SpiShift)){
        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE)){
            /* the data is in error, but could be shited into alugnment */
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, pDataBuffer, 0xFF);
            status = SDSpi_ProcessDataBlock(&pDevice->Config, pDataBuffer, pDevice->LastTransferLength, 
                                        &length, &pDevice->RemainingBytes);                        
            pDevice->SpiShift = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 HandleDataPhase,(2) bit shifted status: %d\n",
                            status));
        }
        if (status == SDIO_STATUS_PENDING) {
            /* send the request out the SPI port, keep even */
            pDevice->LastTransferLength = pDevice->RemainingBytes;
            /* outgoing buffers must be FF*/
            memset(pDataBuffer, 0xFF, pDevice->LastTransferLength);
            pDevice->LastByte = tempLastByte;
            status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress, 
                                       pDevice->LastTransferLength, SendDataCompletion, pDevice);
            DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 HandleDataPhase,5 %d\n", status));
        }
        if (SDIO_SUCCESS(status) && (status != SDIO_STATUS_PENDING) && (pReq->Flags & SDREQ_FLAGS_AUTO_CMD12)) {
            /* need to send an CMD12 */
            pDevice->Cmd12Request.Argument = 0;
            pDevice->Cmd12Request.Flags = SDREQ_FLAGS_RESP_R1B;
            pDevice->Cmd12Request.Command = CMD12;
            pDevice->Cmd12Request.BlockCount = 0;
            pDevice->Cmd12Request.BlockLen = 0;
            pDevice->Cmd12Request.pDataBuffer = NULL;
            pDevice->Cmd12Request.DataRemaining = 0;
            SendCommand12(pDevice, &pDevice->Cmd12Request, &pDevice->Config,//???fix ptrs
                          pDataBuffer, dataDmaAddress);
            status = SDIO_STATUS_PENDING;                        
        }
    }
    
    if (!SDIO_SUCCESS(status)) {
            if (DBG_GET_DEBUG_LEVEL() >= SDDBG_TRACE) {
                SDLIB_PrintBuffer(pDataBuffer, pDevice->LastTransferLength, "SDIO MPC5200 HandleDataPhase - temp"); //??????
            }
        DBG_PRINT(SDDBG_WARN, ("SDIO MPC5200 HandleDataPhase - status: %d, State: %d, remaining: %d, datasize:%d length: %d\n",
              status, pDevice->Config.State, pDevice->Config.pRequest->DataRemaining, pDevice->Config.DataSize, length));
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
    SDIO_STATUS     status;
    PSDHCD_DEVICE   pDevice = (PSDHCD_DEVICE)pHcd->pContext;
    PSDREQUEST      pReq;
    PUINT8          pDataBuffer = MPC5200_GET_DATA_BUFFER(pDevice);
    DMA_ADDRESS     dataDmaAddress = MPC5200_GET_DATA_DMA_BUFFER(pDevice);
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 HcdRequest, pHcd: 0x%X HCD: 0x%X\n", (UINT)pHcd, (UINT)&pDevice->Hcd));
    
    pDevice->Config.EnableCRC = TRUE;
    pDevice->Config.ReverseResponse = TRUE;
    pDevice->Config.MaxReadTimeoutBytes = MPC5200SPI_MAX_BUSY_TIMEOUT; //??should this be based on the clock?
    pDevice->Config.HandleBitShift = TRUE;
    pReq = GET_CURRENT_REQUEST(pHcd);
    DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 HcdRequest: pReq: 0x%X\n", (UINT)pReq));
    DBG_ASSERT(pReq != NULL);

    if(pDevice->ShuttingDown) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 HcdRequest returning canceled\n"));
        return SDIO_STATUS_CANCELED;
    }
        /* start the clock */
    SDStartStopClock(pDevice, CLOCK_ON);

    if (pReq->Flags & SDREQ_FLAGS_DATA_TRANS) {
        /* handle the writes with data */
        /* use the context to hold where we are in the buffer */
        /* send out the command */
        status = SendDataCommand(pDevice, pReq, &pDevice->Config, pDataBuffer, dataDmaAddress);
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 HcdRequest: command SDSpi_ProcessResponse/SDGetBuffer error: %d\n", status));
        } else {
            if (status != SDIO_STATUS_PENDING) {
                status = HandleDataPhase(pDevice);
            }
        }                   
    } else {
        /* send out the command */
        status = SendCommand(pDevice, pReq, &pDevice->Config, pDataBuffer, dataDmaAddress);
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 HcdRequest: command SDSpi_ProcessResponse/SDGetBuffer error: %d\n", status));
        } else {
            if ((status != SDIO_STATUS_PENDING) && (pReq->Flags & SDREQ_FLAGS_DATA_TRANS)){
                    /* check with the bus driver if it is okay to continue with data */
                status = SDIO_CheckResponse(pHcd, pReq, SDHCD_CHECK_DATA_TRANS_OK);
                if (SDIO_SUCCESS(status)) {
                    status = HandleDataPhase(pDevice);
                }
            }
        }                   
    }
    if (status != SDIO_STATUS_PENDING) {  
        pReq->Status = status;
        SDStartStopClock(pDevice, CLOCK_OFF);
        
        if (IS_SDREQ_FORCE_DEFERRED_COMPLETE(pReq->Flags)) {
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 HcdRequest deferring completion to work item \n"));
            SDStartStopClock(pDevice, CLOCK_OFF);
                /* the HCD must do the indication in a separate context and return status pending */
            QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
            return SDIO_STATUS_PENDING;
        } else {        
                /* complete the request */
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 HcdRequest Command Done, status:%d \n", status));
        }
    }     

    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 HcdRequest: pReq: 0x%X, Current: 0x%X\n", 
                            (UINT)pReq, (UINT)GET_CURRENT_REQUEST(pHcd)));
    return status;
} 

/*
 * SendCommandCompletion - send command completion routine. Can be called recursively
 */
static void SendCommandCompletion(PVOID pContext)
{
    PSDHCD_DEVICE   pDevice = (PSDHCD_DEVICE)pContext;
    PSDREQUEST      pReq;
    
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PUINT8          pDataBuffer = MPC5200_GET_DATA_BUFFER(pDevice);
    DMA_ADDRESS     dataDmaAddress = MPC5200_GET_DATA_DMA_BUFFER(pDevice);
    UINT8 tempLastByte = pDataBuffer[pDevice->LastTransferLength-1];
    
    pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
    if (pReq == NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 SendCommandCompletion, no request, Cancelled?, pDevice: 0x%X\n",
                (UINT)pDevice));
        return;
    }
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendCommandCompletion, pReq: 0x%X pDevice: 0x%X\n",
                            (UINT)pReq, (UINT)pDevice));
    if (pDevice->RemainingBytes > 0) {
        /* handle the response */
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pDataBuffer, 16, "SDIO MPC5200 SendCommandCompletion pDataBuffer");
        }
        if (pDevice->SpiShift) {
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes,
                                    pDataBuffer, pDevice->LastByte);
            pDevice->LastByte = tempLastByte;                                   
            if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
                SDLIB_PrintBuffer(pReq->pDataBuffer, pDevice->RemainingBytes, "SDIO MPC5200 SendCommandCompletion corrected");
            }
        }
        status = SDSpi_ProcessResponse(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, &pDevice->RemainingBytes);
        /* handle the response */
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pReq->Response, MAX_CARD_RESPONSE_BYTES, "SDIO MPC5200 SendCommandCompletion response buffer");
        }
        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE) && (!pDevice->SpiShift)){
            /* the data is in error, but could be shited into alignment */
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, pDataBuffer, 0xFF);
            if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
                SDLIB_PrintBuffer(pDataBuffer, pDevice->RemainingBytes, "SDIO MPC5200 SendCommandCompletion corrected(2)");
            }
            status = SDSpi_ProcessResponse(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, &pDevice->RemainingBytes);
            pDevice->SpiShift = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 SendCommandCompletion, bit shifted status: %d\n",
                            status));
        }
        if (status == SDIO_STATUS_PENDING)  {
            if ((pReq->Flags & SDREQ_FLAGS_DATA_TRANS) && (!IS_SDREQ_WRITE_DATA(pReq->Flags))) {
                /* read data, handle with HandleDataPhase */
                status = SDIO_STATUS_SUCCESS;
            } else {
                /* more data to receive */
                pDevice->LastByte = tempLastByte;
                pDevice->LastTransferLength = pDevice->RemainingBytes;
                memset(pDataBuffer, 0xFF, pDevice->RemainingBytes);
                status = SDTransferBuffers(pDevice,  
                                           pDataBuffer, dataDmaAddress, pDevice->RemainingBytes,
                                           SendCommandCompletion, pDevice);
            }
        }
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommandCompletion: command SDSpi_ProcessResponse/SDGetBuffer error: %d\n", status));
        }
    }
    if (SDIO_SUCCESS(status) && (status != SDIO_STATUS_PENDING)){
                /* check with the bus driver if it is okay to continue with data */
            status = SDIO_CheckResponse(&pDevice->Hcd, pReq, SDHCD_CHECK_SPI_TOKEN);
            if (SDIO_SUCCESS(status) && (pReq->Flags & SDREQ_FLAGS_DATA_TRANS)) {
                status = HandleDataPhase(pDevice);
            }
    }
    if (status != SDIO_STATUS_PENDING) {  
        /* complete the request */
        pReq->Status = status;
        
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pReq->Response, MAX_CARD_RESPONSE_BYTES, "SDIO MPC5200 SendCommandCompletion response buffer(1)");
        }

        SDStartStopClock(pDevice, CLOCK_OFF);
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommandCompletion deferring completion to work item, pReq: 0x%X, status: %d \n",
                 (UINT)pReq, pReq->Status));
        QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendCommandCompletion\n"));
}

/*
 * SendCommand - output a command
 */
static SDIO_STATUS SendCommand(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                               PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress)
{
    SDIO_STATUS status;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendCommand, pReq: 0x%X\n", (UINT)pReq));
    pDevice->SpiShift = FALSE;
    pDevice->LastByte = 0xFF;
    status = SDSpi_PrepCommand(pConfig, pReq, MPC5200_GET_COMMAND_BUFFER_SIZE(pDevice), pDataBuffer, &pDevice->RemainingBytes);
    if (!SDIO_SUCCESS(status)) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommand: Prep command error: %d\n", status));
    } else {
        /* send the request out the SPI port */
        pDevice->LastTransferLength = SPILIB_COMMAND_BUFFER_SIZE + pDevice->RemainingBytes;
        status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress,
                                   pDevice->LastTransferLength, SendCommandCompletion, pDevice);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 SendCommand: command SDTransferBuffers error: %d\n", status));
        } 
        /* the rest of the work is done in the completion routine */
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendCommand- status:0x%X\n", status));
    
    return status;
}

/*
 * SendCommand12Completion - completion routine for cmd 12
 */
static void SendCommand12Completion(PVOID pContext)
{
    PSDHCD_DEVICE   pDevice = (PSDHCD_DEVICE)pContext;
    PSDREQUEST      pReq;
    
    SDIO_STATUS     status = SDIO_STATUS_SUCCESS;
    PUINT8          pDataBuffer = MPC5200_GET_DATA_BUFFER(pDevice);
    DMA_ADDRESS     dataDmaAddress = MPC5200_GET_DATA_DMA_BUFFER(pDevice);
    
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendCommand12Completion, pReq: 0x%X pDevice: 0x%X\n",
                            (UINT)&pDevice->Cmd12Request, (UINT)pDevice));
    if (pDevice->RemainingBytes > 0) {
        /* handle the response */
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pDataBuffer, 16, "SDIO MPC5200 SendCommand12Completion pDataBuffer");
        }
        if (pDevice->SpiShift) {
            UINT8 tempLastByte = pDataBuffer[pDevice->LastTransferLength-1];
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes,
                                    pDataBuffer, pDevice->LastByte);
            pDevice->LastByte = tempLastByte;                                   
            if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
                SDLIB_PrintBuffer(pDataBuffer, pDevice->RemainingBytes, "SDIO MPC5200 SendCommand12Completion corrected");
            }
        }
        status = SDSpi_ProcessResponse(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, &pDevice->RemainingBytes);
        /* handle the response */
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pDevice->Cmd12Request.Response, MAX_CARD_RESPONSE_BYTES, "SDIO MPC5200 SendCommand12Completion response buffer");
        }
        if ((status == SDIO_STATUS_BUS_RESP_TIMEOUT_SHIFTABLE) && (!pDevice->SpiShift)){
            /* the data is in error, but could be shited into alugnment */
            SDSpi_CorrectErrorShift(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, pDataBuffer, 0xFF);
            if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
                SDLIB_PrintBuffer(pDataBuffer, pDevice->RemainingBytes, "SDIO MPC5200 SendCommand12Completion corrected(2)");
            }
            status = SDSpi_ProcessResponse(&pDevice->Config, pDataBuffer, pDevice->RemainingBytes, &pDevice->RemainingBytes);
            pDevice->SpiShift = TRUE;
            DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 SendCommand12Completion, bit shifted status: %d\n",
                            status));
        }
        if (status == SDIO_STATUS_PENDING)  {
            /* more data to receive */
            pDevice->LastTransferLength = pDevice->RemainingBytes;
            status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress, 
                                       pDevice->RemainingBytes,
                                       SendCommand12Completion, pDevice);
        }
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommand12Completion: command SDSpi_ProcessResponse/SDGetBuffer error: %d\n", status));
        }
    }
    if (SDIO_SUCCESS(status) && (status != SDIO_STATUS_PENDING)){
                /* check with the bus driver if it is okay to continue with data */
            status = SDIO_CheckResponse(&pDevice->Hcd, &pDevice->Cmd12Request, SDHCD_CHECK_SPI_TOKEN);
    }
    if (status != SDIO_STATUS_PENDING) {  
        /* complete the request */
        pReq = GET_CURRENT_REQUEST(&pDevice->Hcd);
        if (pReq == NULL) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 SendCommand12Completion, no request, Cancelled?, pDevice: 0x%X\n",
                (UINT)pDevice));
            return;
        }
        
        pReq->Status = status;
        
        if (DBG_GET_DEBUG_LEVEL() >= PXA_TRACE_REQUESTS) {
            SDLIB_PrintBuffer(pReq->Response, MAX_CARD_RESPONSE_BYTES, "SDIO MPC5200 SendCommand12Completion response buffer(1)");
        }
        SDStartStopClock(pDevice, CLOCK_OFF);
        
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommand12Completion deferring completion to work item, pReq: 0x%X, status: %d \n",
                 (UINT)pReq, pReq->Status));
        QueueEventResponse(pDevice, WORK_ITEM_IO_COMPLETE); 
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendCommand12Completion\n"));
}

/*
 * SendCommand12 - output a cmd 12
 */
static SDIO_STATUS SendCommand12(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                               PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress)
{
    SDIO_STATUS status;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendCommand12,** pReq: 0x%X\n", (UINT)pReq));
    pDevice->SpiShift = FALSE;
    pDevice->LastByte = 0xFF;
    
    status = SDSpi_PrepCommand(pConfig, pReq, MPC5200_GET_COMMAND_BUFFER_SIZE(pDevice), pDataBuffer, &pDevice->RemainingBytes);
    if (!SDIO_SUCCESS(status)) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendCommand12: Prep command error: %d\n", status));
    } else {
        /* send the request out the SPI port */
        pDevice->LastTransferLength = SPILIB_COMMAND_BUFFER_SIZE + pDevice->RemainingBytes;
        status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress,
                                   pDevice->LastTransferLength, SendCommand12Completion, pDevice);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 SendCommand12: command SDTransferBuffers error: %d\n", status));
        } 
        /* the rest of the work is done in the completion routine */
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendCommand12- status:0x%X\n", status));
    
    return status;
}

/*
 * SendDataCommand - output a command that has data
 */
static SDIO_STATUS SendDataCommand(PSDHCD_DEVICE pDevice, PSDREQUEST pReq, PSDSPI_CONFIG pConfig,
                                   PUINT8 pDataBuffer, DMA_ADDRESS dataDmaAddress)
{
    SDIO_STATUS status;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200 SendDataCommand, pReq: 0x%X\n", (UINT)pReq));
    
    pDevice->SpiShift = FALSE;
    pDevice->LastByte = 0xFF;
    if (IS_SDREQ_WRITE_DATA(pReq->Flags)) {
        status = SDIO_STATUS_SUCCESS; //??debugging
    }
    status = SDSpi_PrepCommand(pConfig, pReq, MPC5200_GET_DATA_BUFFER_SIZE(pDevice), pDataBuffer, &pDevice->RemainingBytes);
    if (!SDIO_SUCCESS(status)) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 SendDataCommand: Prep command error: %d\n", status));
    } else {
        /* send the request out the SPI port, keep even */
        pDevice->LastTransferLength = SPILIB_COMMAND_BUFFER_SIZE + pDevice->RemainingBytes;
        status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress, 
                                   pDevice->LastTransferLength, SendDataCompletion, pDevice);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 SendDataCommand: command SDTransferBuffers error: %d\n", status));
        } 
        /* the rest of the work is done in the completion routine */
    }
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200 SendDataCommand- status:0x%X\n", status));
    
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
    UINT16      command;

    DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200 HcdConfig: name: %s, shutdown: %d, pD: 0x%X \n",
        pDevice->Hcd.pName, (UINT)pDevice->ShuttingDown, (UINT)pDevice));
    
    if(pDevice->ShuttingDown) {
        DBG_PRINT(PXA_TRACE_REQUESTS, ("SDIO MPC5200 HcdConfig returning canceled\n"));
        return SDIO_STATUS_CANCELED;
    }

    command = GET_SDCONFIG_CMD(pConfig);
        
    switch (command){
        case SDCONFIG_GET_WP: 
            /* get write protect */
            /* if write enabled, set WP value to zero */
            *((SDCONFIG_WP_VALUE *)pConfig->pData) = 
                    (pDevice->WriteProt)? 1 : 0;
            break;
        case SDCONFIG_SEND_INIT_CLOCKS: {
                /* send 50 FF's */
            PUINT8      pDataBuffer = MPC5200_GET_DATA_BUFFER(pDevice);
            DMA_ADDRESS dataDmaAddress = MPC5200_GET_DATA_DMA_BUFFER(pDevice);
            memset(pDataBuffer, 0xFF, MPC5200_INIT_CLOCK_BYTES);
            /* send the request out the SPI port */
            DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO MPC5200 HcdConfig - sending clocks\n"));
            SDStartStopClock(pDevice, CLOCK_ON);
            status = SDTransferBuffers(pDevice, pDataBuffer, dataDmaAddress, 
                                       MPC5200_INIT_CLOCK_BYTES, NULL, NULL);
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 HcdConfig - failed: %d\n", status));
            }
            SDStartStopClock(pDevice, CLOCK_OFF);
            DBG_PRINT(PXA_TRACE_SDIO_INT, ("SDIO MPC5200 HcdConfig - sent clocks\n"));
            break;
        }
        case SDCONFIG_SDIO_INT_CTRL: //enable disable SDIO irq
            if (GET_SDCONFIG_CMD_DATA(PSDCONFIG_SDIO_INT_CTRL_DATA,pConfig)->SlotIRQEnable) {
                    /* enable detection */
               status = EnableDisableSDIOIrq(pDevice,TRUE); 
            } else {
               status = EnableDisableSDIOIrq(pDevice,FALSE);
            }
            break;
        case SDCONFIG_SDIO_REARM_INT:
                /* re-enable IRQ detection */
            status = AckSDIOIrq(pDevice);
            break;
        case SDCONFIG_BUS_MODE_CTRL: //return success
            DBG_ASSERT_WITH_MSG(((PSDCONFIG_BUS_MODE_DATA)(pConfig->pData))->BusModeFlags & SDCONFIG_BUS_WIDTH_SPI, 
                "SDIO MPC5200 HcdConfig: non-SPI mode set\n");
            SetBusMode(pDevice, (PSDCONFIG_BUS_MODE_DATA)(pConfig->pData));
            break;
                
        case SDCONFIG_POWER_CTRL:
            /* we are always 3.0v */
            DBG_PRINT(PXA_TRACE_CONFIG, ("SDIO MPC5200 HcdConfig: En:%d, VCC:0x%X \n",
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerEnable,
                      GET_SDCONFIG_CMD_DATA(PSDCONFIG_POWER_CTRL_DATA,pConfig)->SlotPowerVoltageMask));
            break;
        default:
            /* invalid request */
            DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200 HcdConfig:  bad command: 0x%X\n",
                                    command));
            status = SDIO_STATUS_INVALID_PARAMETER;
    }
    
    return status;
} 



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdInitialize - Initialize MMC controller
  Input:  pDeviceContext - device context
  Output: 
  Return: 
  Notes: I/O resources must be mapped before calling this function
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
SDIO_STATUS HcdInitialize(PSDHCD_DEVICE pDeviceContext) 
{
    SDIO_STATUS status = SDIO_STATUS_SUCCESS;
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200: HcdInitialize\n"));
    
        
    if (pDeviceContext->BaseClock == 0) {
         DBG_PRINT(SDDBG_ERROR, ("SDIO MPC5200: HcdInitialize invalid base clock setting\n"));
         status = SDIO_STATUS_DEVICE_ERROR;
         return status;
    }
            
    pDeviceContext->Hcd.MaxClockRate =  pDeviceContext->BaseClock;
    DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200: HcdInitialize Using clock %dHz, max. block %d\n",
                            pDeviceContext->BaseClock, pDeviceContext->Hcd.MaxBytesPerBlock));
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO MPC5200: HcdInitialize SlotVoltageCaps: 0x%X, MaxSlotCurrent: 0x%X\n",
                        (UINT)pDeviceContext->Hcd.SlotVoltageCaps, (UINT)pDeviceContext->Hcd.MaxSlotCurrent));
              
    pDeviceContext->Clock = 254;
    SDStartStopClock(pDeviceContext, CLOCK_OFF);
    
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200: HcdInitialize\n"));
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
    DBG_PRINT(SDDBG_TRACE, ("+SDIO MPC5200: HcdDeinitialize\n"));
    pDeviceContext->ShuttingDown = TRUE;
    SDStartStopClock(pDeviceContext, CLOCK_OFF);
    
    DBG_PRINT(SDDBG_TRACE, ("-SDIO MPC5200: HcdDeinitialize\n"));
}

