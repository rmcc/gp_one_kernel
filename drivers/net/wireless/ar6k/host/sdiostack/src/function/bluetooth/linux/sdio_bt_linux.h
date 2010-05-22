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
@file: sdio_bt_linux.h

@abstract: OS dependent include Bluetooth function driver

#notes: 
 
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef __SDIO_BT_LINUX_H___
#define __SDIO_BT_LINUX_H___


#include <linux/skbuff.h>
#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#define SDIO_FUNCTION_BASE "sdiofn"
#define SDIO_FUNCTION_MAX_DEVICES 1
#define SDIO_FUNCTION_MAJOR 0

typedef struct _SDBT_CONFIG {
    struct hci_dev *pHciDev;        /* the HCI device */
    BOOL            HciRegistered;
    struct sk_buff_head TxList;
    spinlock_t      TxListLock;
    BOOL            PktFlush;
    spinlock_t      RequestListLock;
    SDLIST          RequestList;
}SDBT_CONFIG, *PSBT_CONFIG;

typedef struct sk_buff *PSDBT_HCI_PACKET;
#define SDBTHCI_GET_PKT_BUFFER(p)   (p)->data
#define SDBTHCI_GET_PKT_LENGTH(p)   (p)->len

#endif /*__SDIO_BT_LINUX_H___*/

