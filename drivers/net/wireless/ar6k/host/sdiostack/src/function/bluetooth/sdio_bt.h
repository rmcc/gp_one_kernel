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
@file: sdio_bt.h

@abstract: OS independent include Bluetooth function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BT_H___
#define __SDIO_BT_H___

#ifdef VXWORKS
/* Wind River VxWorks support */
#include "vxworks/sdio_bt_vxworks.h"
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#include "linux/sdio_bt_linux.h"
#endif /* LINUX */

#ifdef UNDER_CE
#include "wince/sdio_bt_wince.h"
#endif /* WINCE */

#define SDBT_DBG_TRANSMIT (SDDBG_TRACE + 1)
#define SDBT_DBG_RECEIVE  (SDDBG_TRACE + 2)

#define FLAGS_RTC_SUPPORTED  0x01   /* re-try control protocol supported */
#define FLAGS_BYTE_BASIS     0x02   /* byte basis transfer of packets */
#define FLAGS_READ_ACK       0x04   /* requires read acknowledgements */
#define FLAGS_CARD_ENAB      0x08
#define FLAGS_CARD_IRQ_UNMSK 0x10

#define SDBT_ENABLE_DISABLE_TIMEOUT 2000 
#define SDBT_PKT_RETRIES            3

typedef enum _BTHCI_TX_STATES {
    TX_BLOCK_PROCESSING = 0,
    TX_PACKET_RETRY = 1
}BTHCI_TX_STATES, *PBTHCI_TX_STATES;
 
typedef struct _BT_HCI_INSTANCE {
    SDLIST      SDList;          /* hcd list entry */
    PSDDEVICE   pDevice;         /* bus driver's device we are supporting */
    UINT8       FuncNo;          /* function number we are on */
    SDBT_CONFIG Config;          /* devices local data  */
    UINT8       Flags;           /* operational flags */
    UINT16      MaxBlockSize;    /* max block size for transfers */
    UINT16      MaxBlocks;       /* max blocks for transfers (multi-block) */
    PSDBT_HCI_PACKET pCurrentTxPacket;
    PUINT8           pTxBufferPosition;
    UINT32           TxRemaining;
    UINT32           RxRemaining;
    UINT32           TxBytesToTransfer;
    UINT8            TxRetry;
    BTHCI_TX_STATES  TxState;
    UINT8            RxRetry;
    BOOL             BlockTransferFix;
}BT_HCI_INSTANCE, *PBT_HCI_INSTANCE;

typedef struct _BT_FUNCTION_CONTEXT {
    SDFUNCTION      Function;       /* function description for bus driver */ 
    OS_SEMAPHORE    InstanceSem;    /* instance lock */
    SDLIST          InstanceList;   /* list of instances */
}BT_FUNCTION_CONTEXT, *PBT_FUNCTION_CONTEXT;

#define SDIO_BT_TYPE_A_HCI_CMD 0x01
#define SDIO_BT_TYPE_A_HCI_ACL 0x02
#define SDIO_BT_TYPE_A_HCI_SCO 0x03
#define SDIO_BT_TYPE_A_HCI_EVT 0x04
#define MAX_RX_RETRY 3
#define SDIO_BT_TRANSPORT_HEADER_LENGTH 4
#define SDIO_BT_SET_HEADER(pBuf,ServiceID,Length)    \
{                                                    \
   (pBuf)[0] = (UINT8)(Length);                      \
   (pBuf)[1] = (UINT8)((Length) >> 8);               \
   (pBuf)[2] = (UINT8)((Length) >> 16);              \
   (pBuf)[3] = (UINT8)(ServiceID);                   \
}

#define SDIO_BT_GET_LENGTH(pHdr) \
    ((UINT32)((pHdr)[0]) | ((UINT32)((pHdr)[1]) << 8) | ((UINT32)((pHdr)[2]) << 16))

#define SDIO_BT_GET_SERVICEID(pHdr) (pHdr)[3]
        
/* register offsets */
#define RECV_DATA_REG               0x00
#define XMIT_DATA_REG               0x00
#define RECV_PACKET_CONTROL_REG     0x10
#define XMIT_PACKET_CONTROL_REG     0x11
#define RETRY_CONTROL_STATUS_REG    0x12
#define INTERRUPT_STATUS_CLEAR_REG  0x13
#define INTERRUPT_ENABLE_REG        0x14
#define MODE_STATUS_REG             0x20
/* bit masks */
#define RCV_PKT_PENDING                 (1 << 0)
#define RETRY_STATUS_BIT_MASK           (1 << 0)
#define RCV_PKT_PENDING_ENABLE          (1 << 0)
#define RCV_PKT_CONTROL_RETRY           (1 << 0)
#define XMIT_PKT_CONTROL_RETRY          (1 << 0)
#define MODE_STATUS_TYPE_A            0x00
#define RECV_PACKET_CONTROL_ACK       0x00
#define RETRY_CONTROL_STATUS_USE_ACKS 0x00

#include "ctstartpack.h"  

struct PKT_RETRY_CONTROL_TUPLE {
    UINT8   SDIO_Interface;      /* interface code */
    UINT8   Class;               /* standard function class */
#define RTC_READ_ACK_REQUIRED     0x00
#define RTC_READ_ACK_NOT_REQUIRED 0x01
    UINT8   SDIO_RetryControl;   /* retry control */
}CT_PACK_STRUCT;

#include "ctendpack.h" 

#define ReadRegister(pHci,reg,pData) \
        SDLIB_IssueCMD52((pHci)->pDevice,(pHci)->FuncNo,\
                          (reg),(pData),1,CMD52_DO_READ)
static INLINE SDIO_STATUS WriteRegister(PBT_HCI_INSTANCE pHci, UINT reg, UINT8 Data)
{
  return SDLIB_IssueCMD52((pHci)->pDevice,(pHci)->FuncNo,
                           (reg),&Data,1,CMD52_DO_WRITE);
}

PBT_HCI_INSTANCE CreateHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                                   PSDDEVICE            pDevice);
void DeleteHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                       PBT_HCI_INSTANCE     pHci); 
                           
SDIO_STATUS AddHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                           PBT_HCI_INSTANCE     pHci);       

PBT_HCI_INSTANCE FindHciInstance(PBT_FUNCTION_CONTEXT pFuncContext,
                                 PSDDEVICE            pDevice);

SDIO_STATUS SendHciPacket(PBT_HCI_INSTANCE pHci);
 
PSDBT_HCI_PACKET OSAllocHCIRcvPacket(PBT_HCI_INSTANCE pHci,
                                     UINT32           HCIPacketLength,
                                     UINT8            Type);

void OSFreeHciRcvPacket(PBT_HCI_INSTANCE pHci, PSDBT_HCI_PACKET pPkt);

SDIO_STATUS OSIndicateHCIPacketReceived(PBT_HCI_INSTANCE pHci,
                                        PSDBT_HCI_PACKET pPacket,
                                        UINT32           HCIPacketLength,
                                        UINT8            ServiceID);
void OSIndicateHCIPacketTransmitDone(PBT_HCI_INSTANCE pHci,
                                     SDIO_STATUS      status);     
                                     
void OSFreeSDRequest(PBT_HCI_INSTANCE pHci, PSDREQUEST pReq); 

PSDREQUEST OSAllocSDRequest(PBT_HCI_INSTANCE pHci);
                                   
#endif /* __SDIO_BT_H___*/

