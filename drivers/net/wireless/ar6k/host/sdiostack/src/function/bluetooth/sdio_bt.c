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
@file: sdio_bt.c

@abstract: Bluetooth SDIO driver

#notes: includes OS independent portions 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <_sdio_defs.h>
#include <sdio_lib.h>
#include "sdio_bt.h"

static void BTIRQHandler(PVOID pContext);
#define BLOCK_WRITE TRUE
#define BLOCK_READ  FALSE
static SDIO_STATUS ReceiveHciPacket(PBT_HCI_INSTANCE pHci);
static void BtTxCompletion(PSDREQUEST pReq);
 
UINT32 SetRequestParam(PBT_HCI_INSTANCE pHci, 
                     UINT32           BytesToSend, 
                     PSDREQUEST       pReq, 
                     BOOL             Write);
                                                                          
/* delete an instance  */
void DeleteHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                       PBT_HCI_INSTANCE     pHci)
{
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    SDIO_STATUS  status;
    
    if (!SDIO_SUCCESS(SemaphorePendInterruptable(&pFuncContext->InstanceSem))) {
        return; 
    }
        /* pull it out of the list */
    SDListRemove(&pHci->SDList);
    SemaphorePost(&pFuncContext->InstanceSem);
   
    if (!(SDDEVICE_IS_CARD_REMOVED(pHci->pDevice))) { 
        if (pHci->Flags & FLAGS_CARD_IRQ_UNMSK) {
            SDDEVICE_SET_IRQ_HANDLER(pHci->pDevice, NULL, NULL); 
                 /* mask our IRQ */
            status = SDLIB_IssueConfig(pHci->pDevice,SDCONFIG_FUNC_MASK_IRQ,NULL,0);
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to unmask IRQ err:%d \n", status));
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: IRQ disabled \n"));
            } 
        }                     
            /* disable the card */
        if (pHci->Flags & FLAGS_CARD_ENAB) {            
            ZERO_OBJECT(fData);
            fData.EnableFlags = SDCONFIG_DISABLE_FUNC;
            fData.TimeOut = SDBT_ENABLE_DISABLE_TIMEOUT;
            status = SDLIB_IssueConfig(pHci->pDevice,
                        SDCONFIG_FUNC_ENABLE_DISABLE,
                        &fData,
                        sizeof(fData)); 
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to disable card: %d\n", status));
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Card Disabled \n"));
            }      
        }
        
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Card removed \n"));
    }
    
    SDLIB_IssueConfig(pHci->pDevice,
                      SDCONFIG_FUNC_FREE_SLOT_CURRENT,
                      NULL,
                      0);
                      
    KernelFree(pHci);
}

/* create a Hci instance */
PBT_HCI_INSTANCE CreateHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                                   PSDDEVICE            pDevice)
{
    PBT_HCI_INSTANCE pHci = NULL;
    UINT32           nextTpl;
    UINT8            tplLength;
    SDIO_STATUS      status = SDIO_STATUS_SUCCESS;
    struct PKT_RETRY_CONTROL_TUPLE rtcTuple;
    SDCONFIG_FUNC_ENABLE_DISABLE_DATA fData;
    SDCONFIG_FUNC_SLOT_CURRENT_DATA   slotCurrent;
    
    ZERO_OBJECT(slotCurrent);
    ZERO_OBJECT(fData);
    
    do {
        pHci = (PBT_HCI_INSTANCE)KernelAlloc(sizeof(BT_HCI_INSTANCE));
        if (NULL == pHci) {
            break;   
        }
        ZERO_POBJECT(pHci);
        SDLIST_INIT(&pHci->SDList);
        pHci->pDevice = pDevice;
        
        pHci->FuncNo = SDDEVICE_GET_SDIO_FUNCNO(pHci->pDevice);
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO BT HCI Function Instance: 0x%Xn Fn:%d \n",(INT)pHci,
                                pHci->FuncNo));
            
        if (SDDEVICE_GET_SDIO_FUNC_MAXBLKSIZE(pHci->pDevice) == 0) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Function does not support Block Mode! \n"));
            status = SDIO_STATUS_ERROR;
            break;                    
        }
            /* limit block size to operational block limit or card function capability*/
        pHci->MaxBlockSize = min(SDDEVICE_GET_OPER_BLOCK_LEN(pHci->pDevice),
                                 SDDEVICE_GET_SDIO_FUNC_MAXBLKSIZE(pHci->pDevice));   
                                     
            /* check if the card support multi-block transfers */
        if (!(SDDEVICE_GET_SDIOCARD_CAPS(pHci->pDevice) & SDIO_CAPS_MULTI_BLOCK)) {
            pHci->Flags |= FLAGS_BYTE_BASIS;  
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Byte basis only \n"));
                /* limit block size to max byte basis */
            pHci->MaxBlockSize = min(pHci->MaxBlockSize, 
                                     (UINT16)SDIO_MAX_LENGTH_BYTE_BASIS);
            pHci->MaxBlocks = 1;
        } else {
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Multi-block capable \n"));
            pHci->MaxBlocks = SDDEVICE_GET_OPER_BLOCKS(pHci->pDevice);   
        }
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Bytes Per Block: %d bytes, Block Count:%d \n",
                                pHci->MaxBlockSize,
                                pHci->MaxBlocks));  
        
        status = SDLIB_GetDefaultOpCurrent(pDevice, &slotCurrent.SlotCurrent);
        
        if (!SDIO_SUCCESS(status)) { 
            break;
        }   
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Allocating Slot current: %d mA\n", slotCurrent.SlotCurrent));         
        status = SDLIB_IssueConfig(pDevice,
                                   SDCONFIG_FUNC_ALLOC_SLOT_CURRENT,
                                   &slotCurrent,
                                   sizeof(slotCurrent));
                                   
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: failed to allocate slot current %d\n",
                                    status));
            if (status == SDIO_STATUS_NO_RESOURCES) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Remaining Slot Current: %d mA\n",
                                    slotCurrent.SlotCurrent));  
            }
            break;
        }
                      
        nextTpl = SDDEVICE_GET_SDIO_FUNC_CISPTR(pHci->pDevice); 
        tplLength = sizeof(rtcTuple);   
        status = SDLIB_FindTuple(pHci->pDevice,
                                 CISTPL_VENDOR,
                                 &nextTpl,
                                 (PUINT8)&rtcTuple,
                                 &tplLength);
        if (SDIO_SUCCESS(status)) {
            pHci->Flags |= FLAGS_RTC_SUPPORTED;
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Retry protocol supported \n"));
            if (!(rtcTuple.SDIO_RetryControl & RTC_READ_ACK_NOT_REQUIRED)) {
                pHci->Flags |= FLAGS_READ_ACK; 
                DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Read Acknowledgements required \n"));
            }   
        } else {
            DBG_PRINT(SDDBG_WARN, ("SDIO BT Function: No RTC Tuple, retry not supported \n"));
            status = SDIO_STATUS_SUCCESS;   
        }
        
            /* now enable the card */
        fData.EnableFlags = SDCONFIG_ENABLE_FUNC;
        fData.TimeOut = SDBT_ENABLE_DISABLE_TIMEOUT;
        status = SDLIB_IssueConfig(pHci->pDevice,
                                   SDCONFIG_FUNC_ENABLE_DISABLE,
                                   &fData,
                                   sizeof(fData));
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: failed to enable function Err:%d\n", status));
            break;
        }
        pHci->Flags |= FLAGS_CARD_ENAB;
        
        if (!(pHci->Flags & FLAGS_BYTE_BASIS)) {
                /* for cards that support multi-block transfers, set the block size */      
            status = SDLIB_SetFunctionBlockSize(pHci->pDevice,
                                                pHci->MaxBlockSize);
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to set block size. Err:%d\n", status));
                break;   
            }      
        } 
            /* if it was Type-B card, put it in Type-A mode */
        if (SDDEVICE_GET_SDIO_FUNC_CLASS(pHci->pDevice) == 0x03) {
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Type B card detected, switch to type A mode \n"));
            status = WriteRegister(pHci,MODE_STATUS_REG,MODE_STATUS_TYPE_A);  
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to switch to type A mode \n"));
                break;   
            } 
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: TYPE A Mode switched \n"));
        }
        
        if (pHci->Flags & FLAGS_READ_ACK) {
            status = WriteRegister(pHci,RETRY_CONTROL_STATUS_REG,RETRY_CONTROL_STATUS_USE_ACKS);  
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to set retry control to use acks \n"));
                break;   
            }            
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Read acks enabled \n"));
            status = WriteRegister(pHci,RECV_PACKET_CONTROL_REG,RECV_PACKET_CONTROL_ACK);  
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to set issue read ack \n"));
                break;    
            }  
            DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: Read ack issued \n"));
        }
        
        SDDEVICE_SET_IRQ_HANDLER(pHci->pDevice,BTIRQHandler,pHci);
            /* unmask our function IRQ */
        status = SDLIB_IssueConfig(pHci->pDevice,SDCONFIG_FUNC_UNMASK_IRQ,NULL,0);  
        if (!SDIO_SUCCESS((status))) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: failed to unmask IRQ Err:%d\n", status));
            break;
        }  
        pHci->Flags |= FLAGS_CARD_IRQ_UNMSK;
          
            /* enable packet ready interrupt */
        status = WriteRegister(pHci,INTERRUPT_ENABLE_REG,RCV_PKT_PENDING_ENABLE);
        if (!SDIO_SUCCESS(status)) {
            break;    
        }
        
        DBG_PRINT(SDDBG_TRACE, ("SDIO BT Function: IRQ enabled - ready \n"));
#if 0  // test command
        {        
            UCHAR data[7] = {0x07,0x00,0x00,0x01,0x03,0x10,0x00};
            PSDREQUEST  pReq = NULL;
            
            /* allocate request to send to host controller */
            pReq = SDDeviceAllocRequest(pHci->pDevice);
            if (NULL == pReq) {
                break;
            }   
                        
            SetRequestParam(pHci, 
                            7, 
                            pReq, 
                            TRUE);
                            
            pReq->Flags &= ~SDREQ_FLAGS_TRANS_ASYNC; 
            pReq->pCompletion =NULL;
            pReq->pCompleteContext = NULL;
            pReq->pDataBuffer = data;
                   
            SDLIB_PrintBuffer(data,7,
                "Sending first BT command...");                        
            status = SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);
            
            if (!SDIO_SUCCESS(status)) {
               DBG_PRINT(SDDBG_WARN, ("SDIO BT - TEST: Synch CMD53 write failed %d \n", 
                                       status));
            } 
            
            SDDeviceFreeRequest(pHci->pDevice,pReq); 
        }      
#endif
                     
    } while (FALSE);

    if (!SDIO_SUCCESS(status) && (pHci != NULL)) {
        DeleteHciInstance(pFuncContext, pHci);
        pHci = NULL;
    }
    
    return pHci;
}

/* find an instance associated with the SD device */
PBT_HCI_INSTANCE FindHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                                 PSDDEVICE            pDevice)
{
    SDIO_STATUS status;
    PSDLIST     pItem;
    PBT_HCI_INSTANCE pHci = NULL;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return NULL; 
    }
        /* walk the list and find our instance */
    SDITERATE_OVER_LIST(&pFuncContext->InstanceList, pItem) {
        pHci = CONTAINING_STRUCT(pItem, BT_HCI_INSTANCE, SDList);
        if (pHci->pDevice == pDevice) {
                /* found it */
            break;   
        }
        pHci = NULL;  
    }    
    
    SemaphorePost(&pFuncContext->InstanceSem);
    return pHci;
}

/* add an instance to our list */
SDIO_STATUS AddHciInstance(PBT_FUNCTION_CONTEXT  pFuncContext,
                           PBT_HCI_INSTANCE      pHci)
{
    SDIO_STATUS status;
    
    status = SemaphorePendInterruptable(&pFuncContext->InstanceSem);
    if (!SDIO_SUCCESS(status)) {
        return status; 
    }
  
    SDListAdd(&pFuncContext->InstanceList,&pHci->SDList);  
    SemaphorePost(&pFuncContext->InstanceSem);
    
    return SDIO_STATUS_SUCCESS;
}

static void BTIRQHandler(PVOID pContext)
{
    PBT_HCI_INSTANCE pHci;
    SDIO_STATUS      status = SDIO_STATUS_SUCCESS;
    UINT8            temp;
    
    DBG_PRINT(SDBT_DBG_RECEIVE, ("SDIO BT IRQ \n"));
    
    pHci = (PBT_HCI_INSTANCE)pContext;
       
    while (1) { 
       
        status = ReadRegister(pHci,INTERRUPT_STATUS_CLEAR_REG,&temp); 
        if (!SDIO_SUCCESS(status)) {
            /* can't read it for some reason */
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to read int status, err:%d\n", status));
            break;
        }  
        
        if (!(temp & RCV_PKT_PENDING)) {
            break;    
        }
            /* clear the status */
        status = WriteRegister(pHci,INTERRUPT_STATUS_CLEAR_REG,RCV_PKT_PENDING);        
        
        if (!SDIO_SUCCESS(status)) {
                /* can't clear it for some reason */
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Failed to clear int status, err:%d\n", status));
            break;   
        }
        
        status = ReceiveHciPacket(pHci); 
        if (!SDIO_SUCCESS(status)) {
            break;   
        }  
    } 
    
    if (!SDIO_SUCCESS(status)) {
            /* mask the interrupt if we can't handle them */
        DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: FATAL Error detected in IRQ processing , err:%d\n", status));   
        WriteRegister(pHci,INTERRUPT_ENABLE_REG,0);   
    } 
        /* ack the interrupt */
    SDLIB_IssueConfig(pHci->pDevice,SDCONFIG_FUNC_ACK_IRQ,NULL,0);    
}

/* set the request parameters in a request
 * This does not set the pDataBuffer field */
UINT32 SetRequestParam(PBT_HCI_INSTANCE pHci, 
                       UINT32           BytesToSend, 
                       PSDREQUEST       pReq, 
                       BOOL             Write)
{
    UINT32 bytesToTransfer;
    
    pReq->Command = CMD53;
    pReq->Flags = SDREQ_FLAGS_RESP_SDIO_R5 | SDREQ_FLAGS_DATA_TRANS;
    if (Write) {
            /* do write in non-blocking fashion */
            /* note, that read-operations are performed in our normal IRQ handler which
             * allows synchronous operations */
        pReq->Flags |= (SDREQ_FLAGS_DATA_WRITE | SDREQ_FLAGS_TRANS_ASYNC);   
        pReq->pCompletion = BtTxCompletion;
        pReq->pCompleteContext = (PVOID)pHci;
    }   
            
    if ((pHci->Flags & FLAGS_BYTE_BASIS) || (BytesToSend < pHci->MaxBlockSize)) {
            /* byte basis */
        bytesToTransfer = min((UINT32)pHci->MaxBlockSize, BytesToSend);
        SDIO_SET_CMD53_ARG(pReq->Argument,
                           (Write ? CMD53_WRITE : CMD53_READ),
                           pHci->FuncNo,
                           CMD53_BYTE_BASIS,
                           CMD53_FIXED_ADDRESS,
                           (Write ? XMIT_DATA_REG : RECV_DATA_REG),
                           CMD53_CONVERT_BYTE_BASIS_BLK_LENGTH_PARAM(bytesToTransfer)); 
        pReq->BlockCount = 1;   
        pReq->BlockLen = bytesToTransfer; 
        if (pReq->BlockLen < 8) {
            pReq->Flags |= SDREQ_FLAGS_DATA_SHORT_TRANSFER;    
        }
    } else {
            /* block mode */
        pReq->BlockLen = pHci->MaxBlockSize;
            /* get block counts (whole blocks, no partials allowed) */
        pReq->BlockCount = min(BytesToSend / (UINT32)pHci->MaxBlockSize, (UINT32)pHci->MaxBlocks);
        DBG_ASSERT(pReq->BlockCount != 0);    
            /* calculate total transfer to return */
        bytesToTransfer = pReq->BlockCount * pReq->BlockLen;
            /* set argument */
        SDIO_SET_CMD53_ARG(pReq->Argument,
                           (Write ? CMD53_WRITE : CMD53_READ),
                           pHci->FuncNo,
                           CMD53_BLOCK_BASIS,
                           CMD53_FIXED_ADDRESS,
                           (Write ? XMIT_DATA_REG : RECV_DATA_REG),
                           CMD53_CONVERT_BLOCK_BASIS_BLK_COUNT_PARAM(pReq->BlockCount)); 
    } 
  
    return bytesToTransfer;
} 

static INLINE UINT32 AdjustBytesForHC(PBT_HCI_INSTANCE pHci, UINT32 RemainingBytes) {

    if (pHci->BlockTransferFix) {
        /* some host controllers (like the PXA25x) do not support 1 and 3 byte transfers 
         * we are trying to avert a 1 or 3 byte block transfer 
         * unfortunately not all SDIO BT cards allow CMD52 operations on the data port
         * so we have to "look ahead" and prevent a small block transfer by dividing the
         * current chunk */
        if (((RemainingBytes % pHci->MaxBlockSize) == 1) ||
            ((RemainingBytes % pHci->MaxBlockSize) == 3)) {
                /* divide by two, this should split the remaining bytes into something
                 * manageable */
            DBG_PRINT(SDDBG_WARN, ("SDIO BT - adjusting %d bytes (max blocks: %d) \n",
                                   RemainingBytes,pHci->MaxBlockSize));
            return (RemainingBytes >> 1);
        } 
        return RemainingBytes; 
    }
        /* no adjustment required */
    return RemainingBytes;  
}

static INLINE void SetUpNextTxBlocTransfer(PBT_HCI_INSTANCE pHci,
                                           PSDREQUEST       pReq) {
    pHci->pTxBufferPosition += pHci->TxBytesToTransfer;
    pHci->TxRemaining -= pHci->TxBytesToTransfer;
    if (pHci->TxRemaining) {
             /* set where we are */
        pReq->pDataBuffer = pHci->pTxBufferPosition;
            /* set up parameters for the request */   
        pHci->TxBytesToTransfer = SetRequestParam(pHci,
                                                  AdjustBytesForHC(pHci,pHci->TxRemaining),
                                                  pReq,
                                                  BLOCK_WRITE);                                     
    }
}

static INLINE void ResetCurrentTxPacketTransfer(PBT_HCI_INSTANCE pHci) {
        pHci->pTxBufferPosition = SDBTHCI_GET_PKT_BUFFER(pHci->pCurrentTxPacket);
        pHci->TxRemaining = SDBTHCI_GET_PKT_LENGTH(pHci->pCurrentTxPacket);
        pHci->TxBytesToTransfer = 0;
}

static void BtTxCompletion(PSDREQUEST pReq) 
{
    PBT_HCI_INSTANCE pHci;
    SDIO_STATUS      status;
    BOOL             done = FALSE;
     
    pHci = (PBT_HCI_INSTANCE)pReq->pCompleteContext;
    DBG_ASSERT(pHci != NULL);
    status = pReq->Status;
    
    switch (pHci->TxState) {       
        case TX_BLOCK_PROCESSING:
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: XMIT Failed, Err:%d \n", status));
                    /* check for retry */
                if (pHci->Flags & FLAGS_RTC_SUPPORTED) {  
                    pHci->TxRetry--; 
                    if (pHci->TxRetry) {
                        pHci->TxState = TX_PACKET_RETRY; 
                            /* setup request for async CMD52 */                        
                        SDLIB_SetupCMD52RequestAsync((pHci)->FuncNo,
                                                     XMIT_PACKET_CONTROL_REG,
                                                     CMD52_DO_WRITE,
                                                     XMIT_PKT_CONTROL_RETRY,
                                                     pReq);
                            /* set completion routine */
                        pReq->pCompletion = BtTxCompletion;
                        pReq->pCompleteContext = (PVOID)pHci; 
                            /* submit */
                        SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);                             
                    } else {
                        DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: XMIT retries exceeded \n"));
                        done = TRUE;
                        break;   
                    }
                } else {
                    DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Card does not support retries! \n"));
                    done = TRUE;
                }
            } else {
                    /* set it up for the next transfer */
                SetUpNextTxBlocTransfer(pHci,pReq);
                    /* are we done? */
                if (pHci->TxRemaining) {
                        /* submit request asynchronously */ 
                    SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);   
                } else {
                    DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO BT Function: TX Packet 0x%X sent \n", 
                                                  (INT)pHci->pCurrentTxPacket));
                    done = TRUE;  
                }  
            }
            break;            
        case TX_PACKET_RETRY:
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO BT Function: Failed to set TX packet retry control, Err:%d \n", status));
                done = TRUE;
                break;   
            }
            if (SD_R5_GET_RESP_FLAGS(pReq->Response) & SD_R5_ERRORS) {
                DBG_PRINT(SDDBG_ERROR, 
                    ("SDIO BT Function: CMD52 failed in TX retry control \n"));
                status = SDIO_STATUS_DEVICE_ERROR;
                done = TRUE;
                break;
            }
            ResetCurrentTxPacketTransfer(pHci);
            pHci->TxState = TX_BLOCK_PROCESSING;
                /* setup block transfer */
            SetUpNextTxBlocTransfer(pHci,pReq);  
                /* submit request asynchronously */ 
            SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);
            break;
        default:
            DBG_ASSERT(FALSE);
            break;  
    } 
   
    if (done) {
        OSIndicateHCIPacketTransmitDone(pHci,status);
        OSFreeSDRequest(pHci, pReq);    
    }  
}
    
    
    
/* send the hci packet asynchronously the caller handles the header and packet queues and sets the
 * pCurrentPacket field for the packet ready to go out */
SDIO_STATUS SendHciPacket(PBT_HCI_INSTANCE pHci)
{
    SDIO_STATUS status = SDIO_STATUS_PENDING; 
    PSDREQUEST  pReq;
     
    do {
        DBG_ASSERT(pHci->pCurrentTxPacket != NULL);
        pReq = OSAllocSDRequest(pHci);
        
        if (NULL == pReq) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: No SD requests remaining \n"));
            status =  SDIO_STATUS_NO_RESOURCES;    
            break;
        }
    
        pHci->TxRetry = SDBT_PKT_RETRIES;
        ResetCurrentTxPacketTransfer(pHci);
        pHci->TxState = TX_BLOCK_PROCESSING;
            /* setup block transfer */
        SetUpNextTxBlocTransfer(pHci,pReq); 
          
        DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO BT Function: BlockLen:%d,BlockCount:%d, Arg:0x%X \n",
                                      pReq->BlockLen, pReq->BlockCount,pReq->Argument));           
            /* submit request asynchronously */ 
        SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);
    
    } while (FALSE);    
  
    if (!SDIO_SUCCESS(status)) {
       OSIndicateHCIPacketTransmitDone(pHci,status);
    }    
                             
    return status;  
}

static SDIO_STATUS ReceiveHciPacket(PBT_HCI_INSTANCE pHci)
{
    SDIO_STATUS      status;
    PSDBT_HCI_PACKET pPacket = NULL;
    UINT8            header[SDIO_BT_TRANSPORT_HEADER_LENGTH];
    PSDREQUEST       pReq; 
    UINT32           bytesToTransfer;
    UINT32           remaining;
    PUINT8           pBufferLoc;
    
    pReq = OSAllocSDRequest(pHci);
    
    if (NULL == pReq) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: No SD requests remaining \n"));
        return SDIO_STATUS_NO_RESOURCES;    
    }
    
    do{
        pReq->pDataBuffer = header;
        SetRequestParam(pHci,AdjustBytesForHC(pHci,sizeof(header)),pReq,BLOCK_READ);          
            /* go get the header */ 
        status = SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);
        
        if (!SDIO_SUCCESS(status)) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: failed to get header, Err:%d\n", status));
            break;   
        }
        
        remaining = SDIO_BT_GET_LENGTH(header);
        if (remaining <= SDIO_BT_TRANSPORT_HEADER_LENGTH) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Invalid Length:%d \n", remaining));
            break;   
        }
        
        if (DBG_GET_DEBUG_LEVEL() >= SDBT_DBG_RECEIVE) {
            SDLIB_PrintBuffer(header,sizeof(header),"SDIO BT, header dump \n");
        }
        switch (SDIO_BT_GET_SERVICEID(header)) {
            case SDIO_BT_TYPE_A_HCI_EVT:
            case SDIO_BT_TYPE_A_HCI_ACL:
            case SDIO_BT_TYPE_A_HCI_SCO:
                break;    
            default:
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Invalid packet type:%d \n",
                                        SDIO_BT_GET_SERVICEID(header)));
                status = SDIO_STATUS_ERROR;
                break;
        }
        
        if (!SDIO_SUCCESS(status)) {
            break;   
        }
            /* subtract off header */
        remaining -= SDIO_BT_TRANSPORT_HEADER_LENGTH;
            /* get a buffer for this HCI packet */
        pPacket = OSAllocHCIRcvPacket(pHci,
                                      remaining,
                                      SDIO_BT_GET_SERVICEID(header));                                       
        if (NULL == pPacket) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: failed to allocate packet \n"));
            status = SDIO_STATUS_NO_RESOURCES; 
            break;
        } 
        
        DBG_PRINT(SDBT_DBG_RECEIVE, ("SDIO BT Function: Getting HCI Packet (type:%d, Length:%d) \n",
                                     SDIO_BT_GET_SERVICEID(header),remaining));
                  
        pBufferLoc = SDBTHCI_GET_PKT_BUFFER(pPacket);
        
        while (remaining) {
                /* set where we are */
            pReq->pDataBuffer = pBufferLoc;
                /* set up parameters for the request */   
            bytesToTransfer = SetRequestParam(pHci,AdjustBytesForHC(pHci,remaining),pReq,BLOCK_READ);                                     
                /* submit request synchronously */ 
            status = SDDEVICE_CALL_REQUEST_FUNC(pHci->pDevice,pReq);
            if (!SDIO_SUCCESS(status)) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Rcv Failed, Err:%d \n", status));
                break;    
            }
            
            pBufferLoc += bytesToTransfer;   
            remaining -= bytesToTransfer;
        }  
        
        if (!SDIO_SUCCESS(status)) {
                /* free this packet */
            OSFreeHciRcvPacket(pHci, pPacket); 
            if (pHci->Flags & FLAGS_RTC_SUPPORTED) { 
                pHci->RxRetry++;
                if (pHci->RxRetry > MAX_RX_RETRY) {
                    DBG_PRINT(SDDBG_ERROR, 
                            ("SDIO BT Function: RX Retry Exceeded \n"));
                    break;        
                }               
                    /* set bit to discard this packet, we need to start over */
                status = WriteRegister(pHci,RECV_PACKET_CONTROL_REG,RCV_PKT_CONTROL_RETRY);
                if (!SDIO_SUCCESS(status)) {
                    DBG_PRINT(SDDBG_ERROR, 
                            ("SDIO BT Function: failed to set RCV PKT RETRY, err:%d \n", status));
                }   
            } else {
                DBG_PRINT(SDDBG_ERROR, ("SDIO BT Function: Card does not support retries! \n"));
            }           
            break;
        } else {  
                /* reset retry on good packets */
            pHci->RxRetry = 0;            
            DBG_PRINT(SDBT_DBG_RECEIVE, ("SDIO BT Function: Packet received --\n"));
            if (DBG_GET_DEBUG_LEVEL() >= SDBT_DBG_RECEIVE) {
                SDLIB_PrintBuffer(SDBTHCI_GET_PKT_BUFFER(pPacket),
                                  (SDIO_BT_GET_LENGTH(header) - SDIO_BT_TRANSPORT_HEADER_LENGTH),
                                  "Received HCI Packet Dump");
            }
                /* indicate the packet */                  
            OSIndicateHCIPacketReceived(pHci,
                                        pPacket,
                                       (SDIO_BT_GET_LENGTH(header) - SDIO_BT_TRANSPORT_HEADER_LENGTH),
                                        SDIO_BT_GET_SERVICEID(header));
                /* we no longer own this packet */
            pPacket = NULL;
            if (pHci->Flags & FLAGS_READ_ACK) {                
                    /* ack the hardware indicating we pulled the packet out */
                status = WriteRegister(pHci,RECV_PACKET_CONTROL_REG,RECV_PACKET_CONTROL_ACK);
                if (!SDIO_SUCCESS(status)) {
                    DBG_PRINT(SDDBG_ERROR, 
                            ("SDIO BT Function: failed to set ACK Packet, err:%d \n", status));
                    break;
                } else {
                    DBG_PRINT(SDBT_DBG_RECEIVE, ("SDIO BT Function: RCV Packet Ack'd \n"));
                }    
            }
        }
    }while(FALSE);
  
   
    OSFreeSDRequest(pHci, pReq);    

    return status;
}

