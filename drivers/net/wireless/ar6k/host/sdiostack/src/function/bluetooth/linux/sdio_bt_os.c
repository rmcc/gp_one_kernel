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
@file: sdio_bt_os.c

@abstract: Linux implementation module for SDIO Bluetooth Function driver

#notes: includes module load and unload functions
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* debug level for this module*/
#define DBG_DECLARE 3;
#include <ctsystem.h>
#include <sdio_busdriver.h>
#include <_sdio_defs.h>
#include <sdio_lib.h>
#include "../sdio_bt.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#define DESCRIPTION "SDIO Bluetooth Function Driver"
#define AUTHOR "Atheros Communications, Inc."

/* debug print parameter */
module_param(debuglevel, int, 0644);
MODULE_PARM_DESC(debuglevel, "debuglevel 0-7, controls debug prints");

static INT blockfix = 0;
module_param(blockfix, int, 0644);
MODULE_PARM_DESC(blockfix, "HCI packet block fix");

static INT sdrequests = 8;
module_param(sdrequests, int, 0644);
MODULE_PARM_DESC(sdrequests, "HCI SDRequest list size");

static int bt_open(struct hci_dev *hdev);
static int bt_close(struct hci_dev *hdev);
static int bt_send_frame(struct sk_buff *skb);
static int bt_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);
static int bt_flush(struct hci_dev *hdev);
static void bt_destruct(struct hci_dev *hdev);
static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice);
static void CleanupInstance(PBT_FUNCTION_CONTEXT  pFunctionContext,
                            PBT_HCI_INSTANCE      pHci);

/* devices we support, null terminated */
static SD_PNP_INFO Ids[] = {
   {.SDIO_FunctionClass = 0x02}, /* SDIO-BLUETOOTH SDIO standard interface code */
   {.SDIO_FunctionClass = 0x03},
    {}
};

/* driver data */
static BT_FUNCTION_CONTEXT FunctionContext = {
    .Function.Version = CT_SDIO_STACK_VERSION_CODE,
    .Function.pName    = "sdio_bluetooth",
    .Function.MaxDevices = 1,
    .Function.NumDevices = 0,
    .Function.pIds     = Ids,
    .Function.pProbe   = Probe,
    .Function.pRemove  = Remove,
    .Function.pSuspend = NULL,
    .Function.pResume  = NULL,
    .Function.pWake    = NULL,
    .Function.pContext = &FunctionContext, 
}; 

void OSFreeSDRequest(PBT_HCI_INSTANCE pHci, PSDREQUEST pReq)
{
    spin_lock(&pHci->Config.RequestListLock);   
    SDListAdd(&pHci->Config.RequestList, &pReq->SDList); 
    spin_unlock(&pHci->Config.RequestListLock); 
}

PSDREQUEST OSAllocSDRequest(PBT_HCI_INSTANCE pHci)
{
    PSDREQUEST pReq = NULL;
    PSDLIST    pItem;
    
    spin_lock(&pHci->Config.RequestListLock);   
    do {
            /* check the list */
        pItem = SDListRemoveItemFromHead(&pHci->Config.RequestList);
        if (NULL == pItem) {
            break;   
        }
        pReq = CONTAINING_STRUCT(pItem, SDREQUEST, SDList);
    } while (FALSE); 
    
    spin_unlock(&pHci->Config.RequestListLock);  
  
    return pReq;
}

/* allocate a receive packet  */
PSDBT_HCI_PACKET OSAllocHCIRcvPacket(PBT_HCI_INSTANCE pHci,
                                     UINT32           HCIPacketLength,
                                     UINT8            Type)
{
    PSDBT_HCI_PACKET pPacket;
    
        /* allocate a buffer */
    pPacket = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC);
    if (pPacket != NULL) {
        pPacket->dev = (PVOID)pHci->Config.pHciDev; 
    }
    return pPacket;
}

void OSFreeHciRcvPacket(PBT_HCI_INSTANCE pHci, PSDBT_HCI_PACKET pPkt)
{
    kfree_skb(pPkt);  
}

/*
 * Indicate that a HCI packet was received 
*/
SDIO_STATUS OSIndicateHCIPacketReceived(PBT_HCI_INSTANCE pHci,
                                        PSDBT_HCI_PACKET pPacket,
                                        UINT32           HCIPacketLength,
                                        UINT8            ServiceID)
{   
    UINT8   btType;
    
    switch (ServiceID) {
        case SDIO_BT_TYPE_A_HCI_ACL:
            btType = HCI_ACLDATA_PKT;
            break;
        case SDIO_BT_TYPE_A_HCI_SCO:
            btType = HCI_SCODATA_PKT;
            break;
        case SDIO_BT_TYPE_A_HCI_EVT:  
            btType = HCI_EVENT_PKT;  
            break;
        default:
            DBG_ASSERT(FALSE);
            return SDIO_STATUS_ERROR;
    } 
        /* set the final type */
    pPacket->pkt_type = btType;
        /* adjust packet length to what the caller copied to the buffer */
    skb_put(pPacket,HCIPacketLength); 
        /* pass receive packet up the stack */    
    if (hci_recv_frame(pPacket) != 0) {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth Function: hci_recv_frame failed \n"));
    } else {
        DBG_PRINT(SDBT_DBG_RECEIVE, ("SDIO Bluetooth Function: Indicated RCV of type:%d, Length:%d \n",
                                     btType,HCIPacketLength));
    }
    return SDIO_STATUS_SUCCESS;
}

/*
 * bt_open - open a handle to the device
*/
static int bt_open(struct hci_dev *hdev)
{
 
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_open - enter - x\n"));
 
    set_bit(HCI_RUNNING, &hdev->flags);
    set_bit(HCI_UP, &hdev->flags);
    set_bit(HCI_INIT, &hdev->flags); 
    ((PBT_HCI_INSTANCE)hdev->driver_data)->Config.PktFlush = FALSE;
         
    return 0;
}

/*
 * bt_close - close handle to the device
*/
static int bt_close(struct hci_dev *hdev)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_close - enter\n"));

    clear_bit(HCI_RUNNING, &hdev->flags);
    return 0;
}


void OSIndicateHCIPacketTransmitDone(PBT_HCI_INSTANCE pHci,
                                     SDIO_STATUS      status)
{
    PSDBT_HCI_PACKET pPacket;
      
    pPacket = pHci->pCurrentTxPacket;
    DBG_ASSERT(pPacket != NULL);
    
    spin_lock(&pHci->Config.TxListLock);   
    pHci->pCurrentTxPacket = NULL;
    if (!pHci->Config.PktFlush) {           
            /* dequeue HCI packet */
        pHci->pCurrentTxPacket = __skb_dequeue(&pHci->Config.TxList);
        spin_unlock(&pHci->Config.TxListLock); 
        if (pHci->pCurrentTxPacket != NULL) {
                /* start next */
            SendHciPacket(pHci);       
        }
    } else {       
        spin_unlock(&pHci->Config.TxListLock); 
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth, cleanup in progress \n"));
    }
        /* free the one that completed */    
    kfree_skb(pPacket); 
}



/*
 * bt_send_frame - send data frames
*/
static int bt_send_frame(struct sk_buff *skb)
{
    SDIO_STATUS status;
    struct hci_dev *hdev = (struct hci_dev *) skb->dev;
    PBT_HCI_INSTANCE pHci;
    UINT8            serviceID;
    UINT8            *pTemp;

    if (!hdev) {
        DBG_PRINT(SDDBG_WARN, ("SDIO Bluetooth Function: bt_send_frame - no device\n"));
        return -ENODEV;
    }
 
    if (!test_bit(HCI_RUNNING, &hdev->flags)) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_send_frame - not open\n"));
        return -EBUSY;
    }
    
    pHci = (PBT_HCI_INSTANCE)hdev->driver_data;
    
    if (pHci->Config.PktFlush) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_send_frame - flush in progress..\n"));
        return -EBUSY;  
    }
    
    switch (skb->pkt_type) {
        case HCI_COMMAND_PKT:
            serviceID = SDIO_BT_TYPE_A_HCI_CMD;
            hdev->stat.cmd_tx++;
            break;
 
        case HCI_ACLDATA_PKT:
            serviceID = SDIO_BT_TYPE_A_HCI_ACL;
            hdev->stat.acl_tx++;
            break;

        case HCI_SCODATA_PKT:
            serviceID = SDIO_BT_TYPE_A_HCI_SCO;
            hdev->stat.sco_tx++;
            break; 
        default:
            kfree_skb(skb);
            return 0;
    } 
    if (DBG_GET_DEBUG_LEVEL() >= SDBT_DBG_TRANSMIT) {
        SDLIB_PrintBuffer(SDBTHCI_GET_PKT_BUFFER(skb),SDBTHCI_GET_PKT_LENGTH(skb),
                "Linux BT HCI Packet Dump");
        if (skb->pkt_type == HCI_COMMAND_PKT) {
            pTemp = SDBTHCI_GET_PKT_BUFFER(skb);  
            DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO BT HCI Command: OCF:0x%4.4X, OGF:0x%2.2X \n",
                    (INT)pTemp[0] | (((INT)(pTemp[1] & 0x03)) << 8), pTemp[1] >> 2 ));
        } 
    }
    
        /* BT HCI packets have 8 bytes of header space, push on 4 bytes for the header
         * this bumps up the "len" field */
    pTemp = (PUINT8)skb_push(skb, SDIO_BT_TRANSPORT_HEADER_LENGTH);
        /* set the header */ 
    SDIO_BT_SET_HEADER(pTemp,
                       serviceID,
                       SDBTHCI_GET_PKT_LENGTH(skb));
    
    
    DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO Bluetooth Function: bt_send_frame (hci:0x%X) Packet:0x%X \n",
                                  (INT)pHci, (INT)skb));
    DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO BT Send ServiceID:%d, Total Length:%d Bytes \n",
                                  serviceID,SDBTHCI_GET_PKT_LENGTH(skb)));
   
    spin_lock(&pHci->Config.TxListLock);   
    status = SDIO_STATUS_PENDING;
    if (pHci->pCurrentTxPacket != NULL) {
            /* queue HCI packet */
        __skb_queue_tail(&pHci->Config.TxList,skb);
        spin_unlock(&pHci->Config.TxListLock);     
        DBG_PRINT(SDBT_DBG_TRANSMIT, ("SDIO BT Send , Packet Queued \n"));
    } else {
        pHci->pCurrentTxPacket = skb;
        spin_unlock(&pHci->Config.TxListLock); 
        status = SendHciPacket(pHci);   
    }
    
    if (!SDIO_SUCCESS(status)) {
        return  SDIOErrorToOSError(status);      
    }
    return 0;
}

/*
 * bt_ioctl - ioctl processing
*/
static int bt_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_ioctl - enter\n"));
    return -ENOIOCTLCMD;
}

/*
 * bt_flush - flush outstandingbpackets
*/
static int bt_flush(struct hci_dev *hdev)
{
    PSDBT_HCI_PACKET pPkt;
    PBT_HCI_INSTANCE pHci; 
    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_flush - enter\n"));
    
    pHci = (PBT_HCI_INSTANCE)hdev->driver_data;
    pHci->Config.PktFlush = TRUE;
    
    spin_lock(&pHci->Config.TxListLock);   
        /* cleanup the queue */
    while (1) {
        pPkt = __skb_dequeue(&pHci->Config.TxList);
        if (pPkt != NULL) {   
            kfree_skb(pPkt); 
        } else {
            break;   
        }
            
    }
    if (pHci->pCurrentTxPacket != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Outstanding HCI packet:0x%X \n",
        (INT)pHci->pCurrentTxPacket));
    }
    spin_unlock(&pHci->Config.TxListLock);
    
    return 0;
}


/*
 * bt_destruct - 
*/
static void bt_destruct(struct hci_dev *hdev)
{
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: bt_destruct - enter\n"));
    /* currently only supporting a single statically assigned device, nothing to do here */
}

/*
 * Probe - a device potentially for us
*/
static BOOL Probe(PSDFUNCTION pFunction, PSDDEVICE pDevice) {
    PBT_FUNCTION_CONTEXT pFunctionContext = 
                                (PBT_FUNCTION_CONTEXT)pFunction->pContext;
    SYSTEM_STATUS err = 0;
    BOOL          okay = FALSE;
    struct hci_dev *pHciDev = NULL;
    PBT_HCI_INSTANCE pNewHci = NULL;
    INT i;
    PSDREQUEST pReq;
      
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Probe - enter\n"));

    /* make sure this is a device we can handle */
    if ((pDevice->pId[0].SDIO_FunctionClass == 0x02) || 
        (pDevice->pId[0].SDIO_FunctionClass == 0x03)) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Probe - card matched (0x%X/0x%X/0x%X)\n",
                                pDevice->pId[0].SDIO_ManufacturerID,
                                pDevice->pId[0].SDIO_ManufacturerCode,
                                pDevice->pId[0].SDIO_FunctionNo));
    } else {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Probe - not our card (0x%X/0x%X/0x%X)\n",
                                pDevice->pId[0].SDIO_ManufacturerID,
                                pDevice->pId[0].SDIO_ManufacturerCode,
                                pDevice->pId[0].SDIO_FunctionNo));
        return FALSE;
    }
    
    do {
    
        pNewHci = CreateHciInstance(pFunctionContext, pDevice);
        if (NULL == pNewHci) {
            break; 
        } 
        
        if (blockfix) {
            pNewHci->BlockTransferFix = TRUE;
        } else {
            pNewHci->BlockTransferFix = FALSE;
        }
        
        skb_queue_head_init(&pNewHci->Config.TxList);
        spin_lock_init(&pNewHci->Config.TxListLock);
        spin_lock_init(&pNewHci->Config.RequestListLock);
        SDLIST_INIT(&pNewHci->Config.RequestList);
      
            /* allocate bus requests for block transfers */
        for (i = 0; i < sdrequests; i++) {
            pReq = SDDeviceAllocRequest(pDevice);
            if (NULL == pReq) {
                break;    
            } 
                /* add it to our list */
            OSFreeSDRequest(pNewHci, pReq);
        }                   
        
            /* allocate a BT HCI struct for this device */
        pHciDev = hci_alloc_dev();
        if (NULL == pHciDev) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth Function: Probe - failed to allocate bt struct.\n"));
            break;
        }
        SET_HCIDEV_DEV(pHciDev, SD_GET_OS_DEVICE(pDevice));
        
        pNewHci->Config.pHciDev = pHciDev;
             /* add this instance to our list */
        if (!SDIO_SUCCESS(AddHciInstance(pFunctionContext,pNewHci))) {  
            DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth : failed to add instance to list \n"));
            break;   
        }
                  
        pHciDev->type = HCI_VHCI; /* we don't really have a type assigned ????*/
        pHciDev->driver_data = pNewHci;
        pHciDev->open     = bt_open;
        pHciDev->close    = bt_close;
        pHciDev->send     = bt_send_frame;
        pHciDev->ioctl    = bt_ioctl;
        pHciDev->flush    = bt_flush;
        pHciDev->destruct = bt_destruct;
        pHciDev->owner = THIS_MODULE; 
            
            /* mark that we are registered */
        pNewHci->Config.HciRegistered = TRUE;
        if ((err = hci_register_dev(pHciDev)) < 0) {
            DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth Function: Probe - can't register with bluetooth %d\n",
                                    err));
            pNewHci->Config.HciRegistered = FALSE;
            break;
        }
                              
        okay = TRUE;
    } while (FALSE);
        
    if (!okay) {
        if (pNewHci != NULL) {
            CleanupInstance(pFunctionContext, pNewHci);
        }   
    }
    
    return okay;
}

static void CleanupInstance(PBT_FUNCTION_CONTEXT  pFunctionContext,
                            PBT_HCI_INSTANCE      pHci)
{
    int err;
    PSDREQUEST pReq;
    
    DBG_PRINT(SDDBG_TRACE, ("+SDIO Bluetooth CleanupInstance \n"));
     
    if (pHci->Config.pHciDev != NULL) {
        if (pHci->Config.HciRegistered) {
                /* first unregister */
            if ((err = hci_unregister_dev(pHci->Config.pHciDev)) < 0) {
                DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth Function: Remove - can't unregister with bluetooth %d\n",
                                        err));
            } else {
                DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Remove - HCI Instance:0x%X, unregistered\n",
                                       (INT)pHci));
            }
            
            if (pHci->pCurrentTxPacket != NULL) {
                /* TODO fix this with polling or an event */
                OSSleep(2000);   
            }
            DBG_ASSERT(pHci->pCurrentTxPacket == NULL);             
            KernelFree(pHci->Config.pHciDev);
        }
    }
    
        /* cleanup list */
    while (1) {
        pReq = OSAllocSDRequest(pHci);
        if (NULL == pReq) {
            break;    
        } 
        SDDeviceFreeRequest(pHci->pDevice,pReq);
    }
        /* remove this instance */
    DeleteHciInstance(pFunctionContext, pHci);  
    DBG_PRINT(SDDBG_TRACE, ("-SDIO Bluetooth CleanupInstance \n"));
}

/*
 * Remove - our device is being removed
*/
static void Remove(PSDFUNCTION pFunction, PSDDEVICE pDevice) 
{
    PBT_HCI_INSTANCE    pHci;
    PBT_FUNCTION_CONTEXT pFunctionContext = 
                          (PBT_FUNCTION_CONTEXT)pFunction->pContext;
                                    
    DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Remove - enter\n"));

    pHci =  FindHciInstance(pFunctionContext,pDevice);
    if (pHci != NULL) {
        DBG_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: Removing instance: 0x%X From Remove() \n",
                                (INT)pHci)); 
        CleanupInstance(pFunctionContext, pHci);    
    } else {
        DBG_PRINT(SDDBG_ERROR, ("SDIO Bluetooth Function: could not find matching instance! \n"));
    }      
}


/*
 * module init
*/
static int __init sdio_bt_init(void) {
    SDIO_STATUS status;
    
    REL_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: enter\n"));
    
    SDLIST_INIT(&FunctionContext.InstanceList); 
    status = SemaphoreInitialize(&FunctionContext.InstanceSem, 1);    
    if (!SDIO_SUCCESS(status)) {
        return SDIOErrorToOSError(status);
    }
    /* register with bus driver core */
    return SDIOErrorToOSError(SDIO_RegisterFunction(&FunctionContext.Function));
}

/*
 * module cleanup
*/
static void __exit sdio_bt_cleanup(void) {
    REL_PRINT(SDDBG_TRACE, ("SDIO Bluetooth Function: exit\n"));
    SDIO_UnregisterFunction(&FunctionContext.Function);
    SemaphoreDelete(&FunctionContext.InstanceSem);  
}


// 
//
//
MODULE_DESCRIPTION(DESCRIPTION);
MODULE_AUTHOR(AUTHOR);
module_init(sdio_bt_init);
module_exit(sdio_bt_cleanup);

